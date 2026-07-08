/**
 * @file monitor_base.cpp
 * @author Lakshay Piplani (CSE, Penn State)
 * @date 2026-07-06
 * @brief Implementation of the base monitor
*/

#include "monitor_base.h"
#include "../bytes_proto/qualcomm/hdlc.h"
#include <cerrno>
#include <cstdio>

MonitorBase::MonitorBase(std::unique_ptr<ByteChannel> source,
                         std::unique_ptr<Decoder> decoder,
                         const MonitorConfig& config)
    : source_(std::move(source)), decoder_(std::move(decoder)), config_(config)
{}

void MonitorBase::set_packet_handler(std::function<bool(const DecodedPacket&)> handler) {
    handler_ = std::move(handler);
}

void MonitorBase::run() {
    // Also controls the (very chatty — full-buffer-per-call) HDLC framing
    // dump in bytes_proto/qualcomm/hdlc.cpp.
    set_hdlc_verbose(config_.verbose);

    if (config_.verbose) fprintf(stderr, "[monitor] starting setup()\n");
    if (!setup()) {
        fprintf(stderr, "[monitor] setup() failed - device not opened or "
                        "DIAG config commands rejected; nothing will be read\n");
        return;
    }
    decoder_->configure(config_);
    if (config_.verbose) fprintf(stderr, "[monitor] setup complete, decoder configured, entering read loop\n");

    char buf[64];
    long total_read = 0, total_packets = 0;
    while (true) {
        ssize_t got;
        got = source_->read(buf, sizeof(buf));
        if (config_.verbose) {
            fprintf(stderr, "[monitor] read() -> %zd bytes", got);
            if (got > 0) {
                fprintf(stderr, ":");
                for (ssize_t i = 0; i < got; i++)
                    fprintf(stderr, " %02x", (unsigned char) buf[i]);
            }
            fprintf(stderr, "\n");
        }
        if (got < 0) { if (errno == EINTR) continue; break; }
        if (got == 0) { break; }
        total_read += got;

        decoder_->feed(buf, got);
        DecodedPacket pkt;
        while (decoder_->receive_log_packet(pkt)) {
            total_packets++;
            if (handler_ && !handler_(pkt)) {
                if (config_.verbose)
                    fprintf(stderr, "[monitor] handler requested stop after %ld packet(s), %ld byte(s) read\n",
                            total_packets, total_read);
                return;
            }
        }
    }
    if (config_.verbose)
        fprintf(stderr, "[monitor] read loop ended (EOF/error) after %ld packet(s), %ld byte(s) read\n",
                total_packets, total_read);
}


