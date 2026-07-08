/* ws_dissector_client.h
 *
 * C++ replacement for the Python WSDissector wrapper
 * (mobile_insight/monitor/dm_collector/dm_endec/ws_dissector.py).
 *
 * Spawns the ws_dissector program as a long-lived subprocess and talks to it
 * over stdin/stdout using the AWW (Automator Wireshark Wrapper) framing:
 *
 *     request : [AWW proto number : u32 BE][payload length : u32 BE][payload]
 *     response: PDML XML, then a literal sentinel line "===___==="
 *
 * The AWW proto number is looked up from a decoder type_hint protocol name
 * (the string after "raw_msg/", e.g. "nas-5gs", "nr-rrc.dl.dcch"). The name
 * -> number table mirrors WSDissector.SUPPORTED_TYPES and must stay in sync
 * with ws_dissector/packet-aww.cpp::init_proto_names().
 *
 * No Wireshark headers are needed to build this client; it only speaks the
 * wire protocol. The actual libwireshark-based ws_dissector binary is built
 * separately (see ws_dissector/).
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <sys/types.h>   // pid_t

class WsDissector {
public:
    WsDissector() = default;
    ~WsDissector();

    WsDissector(const WsDissector&) = delete;
    WsDissector& operator=(const WsDissector&) = delete;

    // Launch the ws_dissector subprocess at executable_path.
    // lib_path, if non-empty, is prepended to LD_LIBRARY_PATH in the child
    // (needed when libwireshark lives outside the default loader path).
    // Returns false if the process could not be started.
    bool start(const std::string& executable_path,
               const std::string& lib_path = "");

    bool is_running() const { return pid_ > 0; }

    // AWW protocol number for a decoder protocol name, or -1 if unsupported.
    static int aww_type(const std::string& proto_name);

    // Dissect `len` bytes of `data` as protocol `proto_name`.
    // Returns the PDML XML (sentinel stripped), or "" on error / unsupported
    // type / oversized payload / dead subprocess.
    std::string decode(const std::string& proto_name,
                       const uint8_t* data, size_t len);

    // Flush + close stdin (lets the child exit) and reap it.
    void stop();

private:
    int         to_child_   = -1;  // parent -> child stdin
    int         from_child_ = -1;  // child stdout -> parent
    pid_t       pid_        = -1;
    std::string readbuf_;          // carries child stdout across reads
};
