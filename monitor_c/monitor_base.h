/**
 * @file monitor_base.h
 * @author Lakshay Piplani (CSE, Penn State)
 * @date 2026-07-06
 * @brief Abstract class for Monitor class that:
 *          1. Reads raw bytes from the bytes sources
 *          2. Write config to the bytes source (eg enable/disable logs)
 *          3. Pushes bytes to the decoder
*/

#pragma once
#include <memory>
#include <vector>
#include <string>
#include <functional>
#include "../bytes_channel/bytes_channel.h"
#include "monitor_config.h"
#include "../decoder/decoder.h"

class MonitorBase {
public:
    MonitorBase(std::unique_ptr<ByteChannel> source,
                std::unique_ptr<Decoder> decoder,
                const MonitorConfig& config);
    virtual ~MonitorBase() = default;

    virtual std::vector<std::string> available_log_types() const = 0;

    void set_packet_handler(std::function<bool(const DecodedPacket&)> handler);
    void run();

protected:
    std::unique_ptr<ByteChannel> source_;
    std::unique_ptr<Decoder>     decoder_;
    MonitorConfig                config_;

    virtual bool setup() = 0;

private:
    std::function<bool(const DecodedPacket&)> handler_;
};
