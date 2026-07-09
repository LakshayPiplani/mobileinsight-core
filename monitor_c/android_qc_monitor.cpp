/**
 * @file android_qc_monitor.cpp
 * @brief Implementation of AndroidQcMonitor. Ports the setup path of
 *        android_dev_diag_monitor.py (Diag.cfg generation via
 *        dm_collector_c_generate_diag_cfg, _mkfifo, _start_diag_revealer);
 *        the read loop itself is MonitorBase::run() + DiagFifoSource.
 */

#include "android_qc_monitor.h"
#include "../bytes_proto/qualcomm/hdlc.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

AndroidQcMonitor::AndroidQcMonitor(
    std::unique_ptr<DiagFifoSource> src,
    std::unique_ptr<QualcommDecoder> decoder,
    const MonitorConfig &config,
    const DiagRevealerParams &params)
    : MonitorBase(std::move(src), std::move(decoder), config),
      params_(params)
{
}

AndroidQcMonitor::~AndroidQcMonitor() {
    // Killing diag_revealer on exit is deferred (see header); just reap the
    // wrapper shell if it has already exited so we don't leave a zombie.
    if (revealer_pid_ > 0)
        waitpid(revealer_pid_, NULL, WNOHANG);
}

std::vector<std::string> AndroidQcMonitor::available_log_types() const {
    std::vector<std::string> result;
    for (int i = 0; i < LogPacketTypeID_To_Name_n; i++)
        if (LogPacketTypeID_To_Name[i].b_public)
            result.push_back(LogPacketTypeID_To_Name[i].name);
    return result;
}

bool AndroidQcMonitor::setup() {
    // Resolve what goes into Diag.cfg. Python parity: with no requested
    // types, fall back to an existing Diag.cfg on disk if there is one
    // ("existing Diag.cfg file will be used"), else fail.
    std::vector<std::string> type_names = config_.type_names;
    bool generate = true;
    if (type_names.empty()) {
        if (access(params_.diag_cfg_path.c_str(), F_OK) == 0) {
            generate = false;
            fprintf(stderr, "[setup] no log types requested; existing %s will be used\n",
                    params_.diag_cfg_path.c_str());
        } else {
            fprintf(stderr, "[setup] log types not specified and no existing Diag.cfg\n");
            return false;
        }
    } else if (type_names[0] == "all") {
        if (config_.verbose)
            fprintf(stderr, "[setup] enabling all public log types\n");
        type_names = available_log_types();
    }

    if (generate && !generate_diag_cfg(type_names))
        return false;
    if (!make_fifo())
        return false;
    if (!spawn_diag_revealer())
        return false;
    // Blocks until diag_revealer opens the write end of the fifo.
    return source_->open();
}

bool AndroidQcMonitor::write_config_msg(FILE* fp, LogConfigOp op,
                                        const IdVector& ids) const {
    BinaryBuffer buf = encode_log_config(op, ids);
    if (buf.first == NULL || buf.second == 0) {
        fprintf(stderr, "[setup] Diag.cfg: log config msg (op %d) failed to encode\n",
                (int) op);
        return false;
    }
    // Same on-disk encoding as send_msg_to_pyobj(): each config message is
    // stored as a complete HDLC frame -- diag_revealer plays the file into
    // /dev/diag verbatim.
    std::string frame = encode_hdlc_frame(buf.first, buf.second);
    delete[] buf.first;
    if (fwrite(frame.data(), 1, frame.size(), fp) != frame.size()) {
        perror("[setup] Diag.cfg fwrite");
        return false;
    }
    return true;
}

bool AndroidQcMonitor::generate_diag_cfg(
        const std::vector<std::string>& type_names) const {
    FILE* fp = fopen(params_.diag_cfg_path.c_str(), "wb");  // overwrite, not append
    if (fp == NULL) {
        perror(("[setup] cannot open Diag.cfg for writing: "
                + params_.diag_cfg_path).c_str());
        return false;
    }

    bool ok = true;
    IdVector empty;

    // Config headers (generate_log_config_headers(), same order).
    static const LogConfigOp kHeaders[] = {
        DIAG_BEGIN_1D, DIAG_BEGIN_00, DIAG_BEGIN_7C, DIAG_BEGIN_1C,
        DIAG_BEGIN_0C, DIAG_BEGIN_63, DIAG_BEGIN_4B0F0000, DIAG_BEGIN_4B090000,
        DIAG_BEGIN_4B080000, DIAG_BEGIN_4B080100, DIAG_BEGIN_4B040000,
        DIAG_BEGIN_4B040F00,
    };
    for (size_t i = 0; ok && i < sizeof(kHeaders) / sizeof(kHeaders[0]); i++)
        ok = write_config_msg(fp, kHeaders[i], empty);

    // Disable previous logs (reference order: debug msgs first).
    ok = ok && write_config_msg(fp, DISABLE_DEBUG, empty);
    ok = ok && write_config_msg(fp, DISABLE, empty);

    // Enable requested types (generate_log_config_msgs()). Same
    // Modem_debug_message special case as the desktop monitor's
    // generate_log_config_msgs_serial().
    IdVector type_ids;
    ok = ok && map_typenames_to_ids(type_names, type_ids);
    if (ok) {
        IdVector::iterator debug_ind = std::find(
            type_ids.begin(), type_ids.end(),
            (int) LogPacketType::Modem_debug_message);
        if (debug_ind != type_ids.end()) {
            type_ids.erase(debug_ind);
            ok = write_config_msg(fp, DEBUG_WCDMA_L1, type_ids);
        }
        std::vector<IdVector> type_id_vectors;
        sort_type_ids(type_ids, type_id_vectors);
        for (size_t i = 0; ok && i < type_id_vectors.size(); i++) {
            ok = write_config_msg(fp, SET_MASK, type_id_vectors[i]);
            if (ok && config_.verbose)
                fprintf(stderr, "[setup] Diag.cfg SET_MASK batch %zu/%zu (%zu type_ids)\n",
                        i + 1, type_id_vectors.size(), type_id_vectors[i].size());
        }
    }

    // Config end.
    ok = ok && write_config_msg(fp, DIAG_END_6000, empty);

    if (fclose(fp) != 0) {
        perror("[setup] Diag.cfg fclose");
        return false;
    }
    if (ok && config_.verbose)
        fprintf(stderr, "[setup] wrote %s (%zu log types)\n",
                params_.diag_cfg_path.c_str(), type_names.size());
    return ok;
}

bool AndroidQcMonitor::make_fifo() const {
    const char* path = params_.fifo_path.c_str();
    // Python _mkfifo(): remove a stale node first so we never inherit a
    // regular file (or a fifo with a forgotten reader) at that path.
    if (unlink(path) < 0 && errno != ENOENT)
        perror(("[setup] cannot remove stale fifo " + params_.fifo_path).c_str());

    if (mkfifo(path, 0666) == 0)
        return true;
    if (errno == EEXIST)   // someone else won the race; usable as-is
        return true;
    if (errno == EPERM) {
        // Not permitted (e.g. sdcard fs); retry as root like Python does.
        fprintf(stderr, "[setup] mkfifo not permitted, retrying via su\n");
        return run_shell_cmd("mknod " + params_.fifo_path + " p", true);
    }
    perror(("[setup] mkfifo " + params_.fifo_path).c_str());
    return false;
}

bool AndroidQcMonitor::spawn_diag_revealer() {
    // Directory prep, same shell commands (and same tolerance of failure --
    // Python never checks these) and order as _start_diag_revealer().
    if (access(params_.log_dir.c_str(), F_OK) == 0)
        run_shell_cmd("chmod -R 777 \"" + params_.log_dir + "\"", true);
    run_shell_cmd("mkdir \"" + params_.log_dir + "\"", true);
    run_shell_cmd("chmod -R 777 \"" + params_.log_dir + "\"", true);

    char args[64];
    snprintf(args, sizeof(args), " %.6f", params_.log_cut_size);
    std::string cmd = params_.exe_path + " " + params_.diag_cfg_path + " "
                    + params_.fifo_path + " " + params_.log_dir + args;

    fprintf(stderr, "[setup] starting diag_revealer: %s\n", cmd.c_str());
    if (params_.use_su)
        cmd = "su -c " + cmd;   // exact Python form: `Popen("su -c " + cmd, shell=True)`

    pid_t pid = fork();
    if (pid < 0) {
        perror("[setup] fork for diag_revealer");
        return false;
    }
    if (pid == 0) {
        execl(params_.shell_path.c_str(), "sh", "-c", cmd.c_str(), (char*) NULL);
        // exec failed; nothing sensible to do in the child but bail
        perror("[setup] execl diag_revealer");
        _exit(127);
    }
    // Note: this is the pid of the wrapper shell (`su`), not diag_revealer
    // itself -- same limitation as Python. Don't wait: it runs until the
    // capture is torn down; a crash surfaces as FIFO EOF in run().
    revealer_pid_ = pid;
    return true;
}

bool AndroidQcMonitor::run_shell_cmd(const std::string& cmd, bool wait) const {
    std::string full = params_.use_su ? ("su -c " + cmd) : cmd;
    if (config_.verbose)
        fprintf(stderr, "[setup] shell: %s\n", full.c_str());

    pid_t pid = fork();
    if (pid < 0) {
        perror("[setup] fork for shell cmd");
        return false;
    }
    if (pid == 0) {
        execl(params_.shell_path.c_str(), "sh", "-c", full.c_str(), (char*) NULL);
        perror("[setup] execl shell cmd");
        _exit(127);
    }
    if (!wait)
        return true;
    int status = 0;
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR) { }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}
