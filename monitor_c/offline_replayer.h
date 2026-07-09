/**
 * @file offline_replayer.h
 * @brief Offline monitor that reads a .mi2log/.qmdl file from disk and
 *        decodes packets from it. No DIAG protocol is spoken -- setup()
 *        just opens the file.
 */

#pragma once
#include "monitor_base.h"
#include "../bytes_channel/file_source.h"
#include "../decoder/qualcomm_decoder/qualcomm_decoder.h"
#include "../bytes_proto/qualcomm/consts.h"
#include <memory>

class OfflineReplayer : public MonitorBase {
public:
    OfflineReplayer(std::unique_ptr<FileSource> src,
                    std::unique_ptr<Decoder> decoder,
                    const MonitorConfig& config);

    std::vector<std::string> available_log_types() const override;

protected:
    bool setup() override;
};
