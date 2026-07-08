#include "qualcomm_decoder.h"
#include "../../bytes_proto/qualcomm/hdlc.h"
#include "../../bytes_proto/qualcomm/utils.h"
#include "../../bytes_proto/qualcomm/consts.h"
#include "log_packet.h"

#include <cstdio>
#include <cstring>
#include <sys/time.h>

static double get_posix_timestamp() {
    struct timeval tv;
    (void) gettimeofday(&tv, NULL);
    return (double) tv.tv_sec + (double) tv.tv_usec / 1.0e6;
}

QualcommDecoder::QualcommDecoder() {
    manager_init_state(&emanager_);
}

QualcommDecoder::~QualcommDecoder() {
    manager_close(&emanager_);   // flush + close the .mi2log export file
}

void QualcommDecoder::configure(const MonitorConfig& config) {
    skip_decoding_ = config.skip_decoding;

    IdVector type_ids;
    for (const auto& name : config.type_names)
        find_ids(LogPacketTypeID_To_Name, LogPacketTypeID_To_Name_n,
                 name.c_str(), type_ids);

    const char* path = config.log_output_path.empty() ? NULL
                                                      : config.log_output_path.c_str();
    manager_change_config(&emanager_, path, type_ids);
}

void QualcommDecoder::feed(const char* buf, int n) {
    feed_binary(buf, n);
}

void QualcommDecoder::reset() {
    reset_binary();
}

bool QualcommDecoder::receive_log_packet(DecodedPacket& out) {
    std::string frame;
    bool crc_correct = false;

    while (get_next_frame(frame, crc_correct)) {
        if (!crc_correct) {
            fprintf(stderr, "MI(PACKET FAIL) dropped frame of %zu bytes: ", frame.size());
            for (size_t i = 0; i < frame.size(); i++)
                fprintf(stderr, "%02x ", (unsigned char) frame[i]);
            fprintf(stderr, "\n");
            continue;
        }
        check_frame_format(frame);

        if (is_custom_packet(frame.c_str(), frame.size())) {
            if (skip_decoding_)
                continue;
            out.type = CUSTOM_PACKET;
            out.timestamp = get_posix_timestamp();
            out.fields = decode_custom_packet(frame.c_str() + 2, frame.size() - 2);
            out.ok = !out.fields.empty();
            return true;
        }

        // Classify once; the export manager receives the type_id instead of
        // re-parsing the frame.
        bool b_log   = is_log_packet(frame.c_str(), frame.size());
        bool b_debug = !b_log && is_debug_packet(frame.c_str(), frame.size());
        int  type_id = -1;
        if (b_log && frame.size() >= 8) {
            // 8 = 2 (0x1000) + 2 (len1) + 2 (log_msg_len) + 2 (type_id)
            type_id = ((int) *((const unsigned short *) (frame.c_str() + 6))) & 0xFFFF;
        } else if (b_debug) {
            type_id = Modem_debug_message;
        }

        if (!manager_export_binary(&emanager_, type_id, frame.c_str(), frame.size()))
            continue;   // not in the enabled-type whitelist

        if (b_log) {
            out.type = LOG_PACKET;
            out.timestamp = get_posix_timestamp();
            out.fields = decode_log_packet(frame.c_str() + 2, frame.size() - 2,
                                           skip_decoding_);
            out.ok = !out.fields.empty();   // empty = sampling gate dropped it
            return true;
        } else if (b_debug) {
            // The raw debug msg has no standard log header; prepend a
            // synthetic one so decode_log_packet_modem can parse it.
            unsigned short n_size = frame.size() + 14;
            unsigned char tmp[14] = {
                0xFF, 0xFF, 0x00, 0x00, 0xeb, 0x1f,
                0x00, 0x00, 0x73, 0xB7, 0xB8, 0x65, 0xDD, 0x00
            };
            tmp[2] = (unsigned char) n_size;
            tmp[0] = (unsigned char) n_size;
            std::vector<char> s(n_size);
            memmove(s.data(), tmp, 14);
            memmove(s.data() + 14, frame.c_str(), frame.size());
            out.type = DEBUG_PACKET;
            out.timestamp = get_posix_timestamp();
            out.fields = decode_log_packet_modem(s.data(), n_size, skip_decoding_);
            out.ok = !out.fields.empty();
            return true;
        } else {
            continue;   // unrecognized frame type
        }
    }
    return false;   // buffer exhausted
}