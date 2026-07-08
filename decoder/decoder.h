#pragma once
#include "field_value.h"
#include "../monitor_c/monitor_config.h"

enum PacketType { LOG_PACKET, DEBUG_PACKET, CUSTOM_PACKET };

struct DecodedPacket {
    PacketType type;
    double     timestamp = -1.0; // POSIX time; -1.0 if not set
    FieldList  fields;           // fully decoded tree; empty if ok == false
    bool       ok = false;
};

class Decoder {
public:
    virtual ~Decoder() = default;
    virtual void configure(const MonitorConfig& config) = 0;
    virtual void feed(const char* buf, int n) = 0;
    virtual bool receive_log_packet(DecodedPacket& out) = 0; // false = buffer empty
    virtual void reset() = 0;
};
