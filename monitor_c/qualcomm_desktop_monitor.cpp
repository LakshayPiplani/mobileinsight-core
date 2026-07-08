/**
 * @file qualcomm_desktop_monitor.cpp
 * @author Lakshay Piplani (CSE, Penn State)
 * @date 2026-07-05
 * @brief Implementation of concrete Monitor class for reading over Serial USB and using a Qualcomm-based decoder
 */

#include "qualcomm_desktop_monitor.h"

#include <cerrno>
#include <cstdio>
#include <sys/select.h>
#include <sys/time.h>

QualcommDesktopMonitor::QualcommDesktopMonitor(
    std::unique_ptr<SerialPort> port,
    std::unique_ptr<QualcommDecoder> decoder,
    const MonitorConfig &config)
    : MonitorBase(std::move(port), std::move(decoder), config)
{
    // The ctor signature guarantees source_ is a SerialPort (or subclass),
    // so keep a typed view for get_fd() in await_response().
    serial_ = static_cast<SerialPort*>(source_.get());
}

std::vector<std::string> QualcommDesktopMonitor::available_log_types() const {
    std::vector<std::string> result;
    for (int i = 0; i < LogPacketTypeID_To_Name_n; i++)
        if (LogPacketTypeID_To_Name[i].b_public)
            result.push_back(LogPacketTypeID_To_Name[i].name);
    return result;
}


bool QualcommDesktopMonitor::setup()
{
    if (!source_->open())
        return false;
    if (!disable_log_all())
        return false;
    if (config_.type_names[0] == "all") {
        return enable_log_all();
    }
    return enable_log(config_.type_names);
}

bool QualcommDesktopMonitor::send_command(const char *b, int length) {
    std::string frame = encode_hdlc_frame(b, length);
    if (!source_->write(frame.c_str(), frame.size()))
        return false;
    // DIAG is command-response: wait for this command's reply frame before
    // the caller issues the next command (see await_response comment in .h).
    return await_response(1000);
}

bool QualcommDesktopMonitor::await_response(int timeout_ms) {
    int fd = serial_ ? serial_->get_fd() : -1;
    if (fd < 0)
        return true;   // mock/file source: no modem to wait for

    char buf[256];
    while (true) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        struct timeval tv;
        tv.tv_sec  = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;

        int r = select(fd + 1, &rfds, NULL, NULL, &tv);
        if (r < 0) {
            if (errno == EINTR) continue;
            perror("[setup] await_response select");
            return false;
        }
        if (r == 0) {
            // No reply within the window. Don't fail setup — some commands
            // may legitimately go unanswered — but make it visible.
            fprintf(stderr, "[setup] WARNING: no response frame within %d ms "
                            "after command\n", timeout_ms);
            return true;
        }

        ssize_t got = source_->read(buf, sizeof(buf));
        if (got <= 0)
            return false;
        // Nothing is thrown away: the decoder buffers these bytes and the
        // run() loop later classifies the response frames (and drops them
        // as non-log DIAG traffic).
        decoder_->feed(buf, (int) got);

        for (ssize_t i = 0; i < got; i++) {
            if (buf[i] == '\x7e') {   // end of the response frame
                if (config_.verbose)
                    fprintf(stderr, "[setup]   response frame received\n");
                return true;
            }
        }
    }
}

bool QualcommDesktopMonitor::enable_log(const std::vector<std::string>& type_names) {

    return generate_log_config_msgs_serial(type_names);
}

bool QualcommDesktopMonitor::enable_log_all() {
    if (config_.verbose) fprintf(stderr, "[setup] enabling all public log types\n");
    return enable_log(available_log_types());
}

bool QualcommDesktopMonitor::disable_log_all()
{
    IdVector empty;
    BinaryBuffer buf = encode_log_config(LogConfigOp::DISABLE, empty);
    if (buf.first == NULL || buf.second == 0)
        return false;
    bool ok = send_command(buf.first, buf.second);
    if (config_.verbose)
        fprintf(stderr, "[setup] disable_log_all: write %s (%d bytes on the wire)\n",
                ok ? "OK" : "FAILED", buf.second);

    delete[] buf.first;
    return ok;
}


bool QualcommDesktopMonitor::generate_log_config_msgs_serial(const std::vector<std::string>& type_names) {
    IdVector type_ids;
    bool success = map_typenames_to_ids(type_names, type_ids);
    if (!success) {
        return false;
    }
    BinaryBuffer buf;
    IdVector::iterator debug_ind = type_ids.begin();
    for (; debug_ind != type_ids.end(); debug_ind++) {
        if (*debug_ind == LogPacketType::Modem_debug_message)
            break;
    }
    if (debug_ind != type_ids.end()) {
        type_ids.erase(debug_ind);
        buf = encode_log_config(DEBUG_WCDMA_L1, type_ids);
        if (buf.first != NULL && buf.second != 0) {
            send_command(buf.first, buf.second);
            delete[] buf.first;
        } else {
            // PyErr_SetString(PyExc_RuntimeError, "Log config msg failed to encode.");
            return false;
        }
    }

    std::vector<IdVector> type_id_vectors;
    sort_type_ids(type_ids, type_id_vectors);
    for (size_t i = 0; i < type_id_vectors.size(); i++) {
        const IdVector &v = type_id_vectors[i];
        buf = encode_log_config(SET_MASK, v);
        if (buf.first != NULL && buf.second != 0) {
            bool ok = send_command(buf.first, buf.second);
            if (config_.verbose) {
                fprintf(stderr, "[setup] SET_MASK batch %zu/%zu: %s (%zu type_ids:",
                        i + 1, type_id_vectors.size(), ok ? "OK" : "FAILED", v.size());
                for (const int j : v) fprintf(stderr, " %d", j);
                fprintf(stderr, ")\n");
            } else if (!ok) {
                fprintf(stderr, "[setup] SET_MASK batch %zu/%zu failed to send\n",
                        i + 1, type_id_vectors.size());
            }
            delete[] buf.first;
        } else {
            // PyErr_SetString(PyExc_RuntimeError, "Log config msg failed to encode.");
            return false;
        }
    }
    return true;
}
