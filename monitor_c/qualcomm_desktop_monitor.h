/**
 * @file qualcomm_desktop_monitor.h
 * @author Lakshay Piplani (CSE, Penn State)
 * @date 2026-07-06
 * @brief Desktop monitor that reads over Serial USB and decodes packets from Qualcomm DIAG monitor
*/

#pragma once
#include "monitor_base.h"
#include "../bytes_channel/serial_port.h"
#include "../decoder/qualcomm_decoder/qualcomm_decoder.h"
#include "../bytes_proto/qualcomm/log_config.h"
#include "../bytes_proto/qualcomm/hdlc.h"
#include "../bytes_proto/qualcomm/consts.h"
#include <memory>

class QualcommDesktopMonitor : public MonitorBase {
public:
    QualcommDesktopMonitor(std::unique_ptr<SerialPort> port,
                           std::unique_ptr<QualcommDecoder> decoder,
                            const MonitorConfig& config);

    std::vector<std::string> available_log_types() const override;

protected:
    bool setup() override;

private:
    bool send_command(const char* b, int length);
    // Wait (up to timeout_ms) for the modem's response frame to the command
    // just sent. DIAG is command-response: firing the next command before the
    // previous reply finishes gets it dropped by the modem (observed: 5
    // back-to-back SET_MASKs -> only the first one acked, no logs enabled
    // for the other equip IDs). Drained bytes are fed to the decoder so
    // nothing is lost. No-op for mock sources without a real fd.
    bool await_response(int timeout_ms);

    bool enable_log(const std::vector<std::string>& type_names);
    bool enable_log_all();
    bool disable_log_all();
    bool generate_log_config_msgs_serial(const std::vector<std::string>& type_names);

    SerialPort* serial_ = nullptr;  // borrowed view of source_; owned by MonitorBase
};

