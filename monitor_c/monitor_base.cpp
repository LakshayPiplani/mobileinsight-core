/**
 * @file monitor_base.cpp
 * @author Lakshay Piplani (CSE, Penn State)
 * @date 2026-07-06
 * @brief Implementation of the base monitor
*/

#include "monitor_base.h"
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
    fprintf(stderr, "Trying to run monitor\n");

    if (!setup()) 
    { fprintf(stderr, "Failed monitor setup\n"); 
        return ;
    };
    decoder_->configure(config_);
    fprintf(stderr, "Configured decoder\n");

    char buf[64];
    while (true) {
        ssize_t got;
        got = source_->read(buf, sizeof(buf));
        if (got < 0) { if (errno == EINTR) continue; break; }
        if (got == 0) { break; }

        decoder_->feed(buf, got);
        DecodedPacket pkt;
        while (decoder_->receive_log_packet(pkt)) {
            if (handler_ && !handler_(pkt)) return;
        }
    }
}


