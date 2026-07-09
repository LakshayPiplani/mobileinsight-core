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
#include <chrono>
#include <ctime>
#include <vector>

// Wall-clock seconds since an arbitrary fixed point (monotonic) -- the C++
// analog of Python's time.perf_counter().
static double perf_wall_now() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

// Process CPU seconds -- the C++ analog of Python's time.process_time().
static double perf_cpu_now() {
    struct timespec ts;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
    return (double) ts.tv_sec + (double) ts.tv_nsec / 1e9;
}

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

    // --- performance counters ---
    // Mirrors the sandwiched counters in the ea0_option Python branch: the
    // timed window opens before the read and closes right after this read's
    // frames are fully decoded (read -> feed -> drain receive_log_packet),
    // matching Python's read -> feed_binary -> receive_log_packet ->
    // DMLogPacket. Handler dispatch (the Event/self.send() analog) happens
    // after the window closes, so it isn't charged to cpu/pkt.
    long   perf_pkts    = 0;
    long   perf_reads   = 0;
    double perf_cpu_acc = 0.0;
    double perf_wall0   = perf_wall_now();
    // ----------------------------

    while (true) {
        double t_cpu0 = config_.perf_interval > 0 ? perf_cpu_now() : 0.0;

        ssize_t got;
        got = source_->read(buf, sizeof(buf));
        perf_reads++;
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
        std::vector<DecodedPacket> batch;
        DecodedPacket pkt;
        while (decoder_->receive_log_packet(pkt)) {
            batch.push_back(std::move(pkt));
        }

        if (config_.perf_interval > 0)
            perf_cpu_acc += perf_cpu_now() - t_cpu0;   // sandwich: close

        for (DecodedPacket& p : batch) {
            total_packets++;
            perf_pkts++;
            if (config_.perf_interval > 0 && perf_pkts % config_.perf_interval == 0) {
                double wall = perf_wall_now() - perf_wall0;
                fprintf(stderr, "[PERF] pkts=%ld reads=%ld | "
                                "wall=%.3fs cpu=%.3fs | "
                                "%.1f pkt/s  cpu/pkt=%.3fms\n",
                        perf_pkts, perf_reads, wall, perf_cpu_acc,
                        perf_pkts / wall, perf_cpu_acc / perf_pkts * 1000.0);
            }
            if (handler_ && !handler_(p)) {
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


