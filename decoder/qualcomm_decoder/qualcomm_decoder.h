#pragma once
#include <Python.h>
#include "../../export_manager/export_manager.h"
#include "../decoder.h"

class QualcommDecoder : public Decoder {
    ExportManagerState emanager_;
public:
    QualcommDecoder();
    void configure(const MonitorConfig& config) override;
    void feed(const char* buf, int n) override;
    bool receive_log_packet(DecodedPacket& out) override;  // TODO: replace get_next_packet
    void reset() override;

    // kept until receive_log_packet is fully implemented
    PyObject* get_next_packet(bool skip_decoding, bool include_timestamp);
};
