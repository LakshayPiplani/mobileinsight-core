#pragma once
#include "../../export_manager/export_manager.h"
#include "../decoder.h"

class QualcommDecoder : public Decoder {
    ExportManagerState emanager_;
    bool skip_decoding_ = false;
    bool verbose_ = false;
public:
    QualcommDecoder();
    ~QualcommDecoder() override;
    void configure(const MonitorConfig& config) override;
    void feed(const char* buf, int n) override;
    bool receive_log_packet(DecodedPacket& out) override;
    void reset() override;
};