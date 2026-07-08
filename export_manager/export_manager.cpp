/* export_manager.cpp
 * Author: Jiayao Li
 * The ExportManagerState struct imitates LogManagerState in
 * diag_revealer.c . Since they are similar in functionality, hopefully
 * we can merge them in the future.
 */

#include "export_manager.h"

#include "../bytes_proto/qualcomm/hdlc.h"

void
manager_init_state (struct ExportManagerState *pstate) {
    pstate->log_fp = NULL;
    pstate->filename = "";
    pstate->whitelist.clear();
    return;
}

bool
manager_export_binary (struct ExportManagerState *pstate, int type_id,
                       const char *b, size_t length) {

    if (pstate->whitelist.count(type_id) > 0) { // filter

        if (pstate->log_fp != NULL) {
            std::string frame = encode_hdlc_frame(b, (int) length);
            size_t cnt = fwrite(frame.c_str(), sizeof(char), frame.size(), pstate->log_fp);
            (void)cnt;
        }
        return true;
    }
    else
        return false;
}

void
manager_close (struct ExportManagerState *pstate) {
    if (pstate->log_fp != NULL) {
        fclose(pstate->log_fp);
        pstate->log_fp = NULL;
        pstate->filename = "";
    }
}

void
manager_change_config (struct ExportManagerState *pstate,
                        const char *new_path, const IdVector &whitelist) {
    if (pstate->log_fp != NULL && new_path != NULL && pstate->filename != new_path) {   // close old file
        manager_close(pstate);
    }
    if (pstate->log_fp == NULL && new_path != NULL) {   // open new file if necessary
        pstate->log_fp = fopen(new_path, "wb");
        pstate->filename = new_path;
    }
    pstate->whitelist.clear();
    pstate->whitelist.insert(whitelist.begin(), whitelist.end());
}
