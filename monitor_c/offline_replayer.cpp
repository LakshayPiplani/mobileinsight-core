/**
 * @file offline_replayer.cpp
 * @brief Implementation of OfflineReplayer.
 */

#include "offline_replayer.h"

OfflineReplayer::OfflineReplayer(
    std::unique_ptr<FileSource> src,
    std::unique_ptr<Decoder> decoder,
    const MonitorConfig &config)
    : MonitorBase(std::move(src), std::move(decoder), config)
{
}

// Same list QualcommDesktopMonitor exposes. This is presently Qualcomm-only
// (see decoder_ construction at the call site); revisit if/when an
// MtkDecoder-backed replay path exists (parent plan Step 5) and this needs
// to reflect whichever decoder was actually injected.
std::vector<std::string> OfflineReplayer::available_log_types() const {
    std::vector<std::string> result;
    for (int i = 0; i < LogPacketTypeID_To_Name_n; i++)
        if (LogPacketTypeID_To_Name[i].b_public)
            result.push_back(LogPacketTypeID_To_Name[i].name);
    return result;
}

bool OfflineReplayer::setup() {
    // No DIAG handshake for a file: just open it.
    return source_->open();
}
