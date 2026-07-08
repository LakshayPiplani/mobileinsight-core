#include "qualcomm_decoder.h"
#include "hdlc.h"
#include "log_packet.h"
#include "log_config.h"
#include "../../export_manager/export_manager.h"
#include "utils.h"
#include "consts.h"
#include <algorithm>
#include <cstring>

QualcommDecoder::DMCollector() {
    manager_init_state(&emanager_);
}

void QualcommDecoder::configure(const MonitorConfig& config) {
    IdVector type_ids;
    for (const auto& name : config.type_names)
        find_ids(LogPacketTypeID_To_Name,
                 ARRAY_SIZE(LogPacketTypeID_To_Name, ValueName),
                 name.c_str(), type_ids);

    const char* path = config.log_output_path.empty() ? NULL
                                                       : config.log_output_path.c_str();
    manager_change_config(&emanager_, path, type_ids);
}

void QualcommDecoder::feed(const char* buf, int n) {
    feed_binary(buf, n);
}

PyObject* QualcommDecoder::get_next_packet(bool skip_decoding, bool include_timestamp) {
    std::string frame;
    bool crc_correct = false;
    double posix_timestamp = include_timestamp ? get_posix_timestamp() : -1.0;
    bool success = true;

    while (success) {
        success = get_next_frame(frame, crc_correct);
        if (!success) Py_RETURN_NONE;

        if (success && crc_correct) {
            check_frame_format(frame);

            if (is_custom_packet(frame.c_str(), frame.size())) {
                if (skip_decoding) Py_RETURN_NONE;
                const char* s = frame.c_str();
                PyObject* decoded = decode_custom_packet(s + 2, frame.size() - 2);
                if (include_timestamp) {
                    PyObject* ret = Py_BuildValue("(Od)", decoded, posix_timestamp);
                    Py_DECREF(decoded);
                    return ret;
                }
                return decoded;
            }

            // Classify once; the export manager receives the type_id instead
            // of re-parsing the frame.
            bool b_log   = is_log_packet(frame.c_str(), frame.size());
            bool b_debug = !b_log && is_debug_packet(frame.c_str(), frame.size());
            int  type_id = -1;
            if (b_log && frame.size() >= 8) {
                // 8 = 2 (0x1000) + 2 (len1) + 2 (log_msg_len) + 2 (type_id)
                type_id = ((int) *((const unsigned short*) (frame.c_str() + 6))) & 0xFFFF;
            } else if (b_debug) {
                type_id = Modem_debug_message;
            }

            if (!manager_export_binary(&emanager_, type_id, frame.c_str(), frame.size()))
                continue;

            if (b_log) {
                const char* s = frame.c_str();
                PyObject* decoded = decode_log_packet(s + 2, frame.size() - 2, skip_decoding);
                if (include_timestamp) {
                    PyObject* ret = Py_BuildValue("(Od)", decoded, posix_timestamp);
                    if (decoded != Py_None) Py_DECREF(decoded);
                    return ret;
                }
                return decoded;

            } else if (b_debug) {
                unsigned short n_size = frame.size() + sizeof(char) * 14;
                unsigned char tmp[14] = {
                    0xFF, 0xFF, 0x00, 0x00, 0xeb, 0x1f,
                    0x00, 0x00, 0x73, 0xB7, 0xB8, 0x65, 0xDD, 0x00
                };
                *(tmp + 2) = n_size;
                *(tmp)     = n_size;
                char* s = new char[n_size];
                memmove(s, tmp, sizeof(char) * 14);
                memmove(s + sizeof(char) * 14, frame.c_str(), frame.size());
                PyObject* decoded = decode_log_packet_modem(s, n_size, skip_decoding);
                if (include_timestamp) {
                    PyObject* ret = Py_BuildValue("(Od)", decoded, posix_timestamp);
                    Py_DECREF(decoded);
                    return ret;
                }
                return decoded;
            } else {
                continue;
            }
        } else {
            if (success && !crc_correct) {
                fprintf(stderr, "MI(PACKET FAIL) dropped frame of %zu bytes", frame.size());
                for (size_t i = 0; i < frame.size(); i++)
                    fprintf(stderr, "%02x ", (unsigned char)frame[i]);
                fprintf(stderr, "\n");
            }
            continue;
        }
    }
    Py_RETURN_NONE;
}
