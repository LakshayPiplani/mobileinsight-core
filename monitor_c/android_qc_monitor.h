/**
 * @file android_qc_monitor.h
 * @brief Android monitor that reads Qualcomm DIAG logs relayed over a FIFO
 *        by the `diag_revealer` helper (root-only; reads /dev/diag on-device).
 *        Replaces mobile_insight/monitor/android_dev_diag_monitor.py.
 *
 * Unlike the desktop monitor there is no live SET_MASK handshake: the
 * enable-mask is written to a Diag.cfg file *before* diag_revealer is
 * spawned, and diag_revealer plays it into /dev/diag itself. setup() mirrors
 * the Python run() ordering:
 *   1. generate Diag.cfg from config_.type_names
 *   2. mkfifo the FIFO node (su fallback on EPERM)
 *   3. spawn diag_revealer via `su -c "<exe> <Diag.cfg> <fifo> <logdir> <cutsize>"`
 *   4. open the FIFO (blocks until diag_revealer opens the write end)
 * After that MonitorBase::run()'s read/feed/decode loop works unchanged --
 * the FIFO payload bytes are genuine HDLC frames (see diag_fifo_source.h).
 *
 * Deliberately deferred (see CLAUDE_CPP_MIGRATION.md step 4b): liveness
 * watchdog / auto-restart (a dead diag_revealer surfaces as FIFO EOF, which
 * stops run() cleanly), new_diag_log rotation events, and kill-on-exit
 * teardown (the FIFO itself is closed by DiagFifoSource's destructor).
 */

#pragma once
#include "monitor_base.h"
#include "../bytes_channel/diag_fifo_source.h"
#include "../decoder/qualcomm_decoder/qualcomm_decoder.h"
#include "../bytes_proto/qualcomm/log_config.h"
#include "../bytes_proto/qualcomm/consts.h"
#include <cstdio>
#include <memory>
#include <string>
#include <sys/types.h>

// Process-lifecycle parameters for the diag_revealer helper. These are the
// monitor's concern, not the channel's (mirrors how send_command /
// await_response live on QualcommDesktopMonitor rather than SerialPort);
// DiagFifoSource only ever sees the fifo path.
struct DiagRevealerParams {
    std::string exe_path;       // diag_revealer executable
    std::string diag_cfg_path;  // where setup() writes Diag.cfg
    std::string fifo_path;      // FIFO node setup() creates; must equal the
                                // path the injected DiagFifoSource was built with
    std::string log_dir;        // diag_revealer's own on-disk .mi2log output dir
    double      log_cut_size = 0.5;  // MB per on-disk log file before rotation
                                     // (Python default; "DO NOT CHANGE" per ref)
    std::string shell_path = "/system/bin/sh";  // ANDROID_SHELL in Python
    bool        use_su = true;  // wrap the spawn in `su -c` (needed on-device
                                // for /dev/diag; disable for local mock tests)
};

class AndroidQcMonitor : public MonitorBase {
public:
    AndroidQcMonitor(std::unique_ptr<DiagFifoSource> src,
                     std::unique_ptr<QualcommDecoder> decoder,
                     const MonitorConfig& config,
                     const DiagRevealerParams& params);
    ~AndroidQcMonitor() override;

    std::vector<std::string> available_log_types() const override;

    // pid of the spawned shell wrapping diag_revealer (the `su`/sh process,
    // not diag_revealer itself -- same limitation Python has, which is why
    // its watchdog resorted to `ps | grep`). -1 if not spawned.
    pid_t revealer_pid() const { return revealer_pid_; }

protected:
    bool setup() override;

private:
    // Port of dm_collector_c_generate_diag_cfg(): 12 DIAG_BEGIN_* headers,
    // DISABLE_DEBUG + DISABLE, DEBUG_WCDMA_L1 (iff Modem_debug_message is
    // requested) + SET_MASK batches, DIAG_END_6000 -- each HDLC-encoded and
    // written to params_.diag_cfg_path.
    bool generate_diag_cfg(const std::vector<std::string>& type_names) const;
    // encode_log_config(op, ids) -> HDLC-frame -> fwrite. False on encode or
    // write failure.
    bool write_config_msg(FILE* fp, LogConfigOp op, const IdVector& ids) const;

    // Python _mkfifo(): unlink-if-exists then mkfifo(0666); on EPERM fall
    // back to `su -c "mknod <path> p"`.
    bool make_fifo() const;

    // Prepare log_dir (mkdir + chmod 777, via shell like Python) and launch
    // diag_revealer. Fire-and-forget: stores the shell pid, does not wait.
    bool spawn_diag_revealer();

    // Run `cmd` through the shell (wrapped in `su -c` when params_.use_su),
    // optionally waiting for completion. Returns false on spawn failure or,
    // when waiting, nonzero exit status.
    bool run_shell_cmd(const std::string& cmd, bool wait) const;

    DiagRevealerParams params_;
    pid_t revealer_pid_ = -1;
};
