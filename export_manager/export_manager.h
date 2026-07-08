/* export_manager.h
 * Author: Jiayao Li
 * Manage the output of logs: filter frames against a type-ID whitelist and
 * append the ones that pass to a .mi2log file.
 *
 * Moved out of decoder/qualcomm_decoder/ (2026-07-08): this is an independent
 * module. It does not classify frames itself — the caller (the decoder, which
 * has already classified the frame) supplies the type ID. Depends only on
 * bytes_proto/qualcomm for IdVector and HDLC framing.
 */

#pragma once

#include "../bytes_proto/qualcomm/utils.h"

#include <set>
#include <string>
#include <cstdio>

// Manage the output of logs.
struct ExportManagerState {
    FILE *log_fp;   // Point to the current log.
    std::string filename;
    std::set<int> whitelist;
};

// Must be called before usage
void manager_init_state (struct ExportManagerState *pstate);

// new_path == NULL closes/disables the output file; whitelist is the set of
// type IDs to export (from MonitorConfig::type_names, mapped to IDs).
void manager_change_config (struct ExportManagerState *pstate,
                            const char *new_path, const IdVector &whitelist);

// Whitelist check + write. type_id is the DIAG type the caller already
// determined (Modem_debug_message for debug frames, -1 for unrecognized).
// Returns true iff type_id is whitelisted; the frame is written only when an
// output file is configured.
bool manager_export_binary (struct ExportManagerState *pstate, int type_id,
                            const char *b, size_t length);
