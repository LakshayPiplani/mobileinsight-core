/* android_qc_capture.cpp
 * Live-capture driver for AndroidQcMonitor (analogous to serialtest.cpp for
 * the desktop serial path). Spawns diag_revealer, reads the chronicle FIFO,
 * decodes, and dumps every DecodedPacket to a text file.
 *
 * Usage:
 *   android_qc_capture <diag_revealer> [fifo] [diag_cfg] [log_dir] \
 *                      [ws_dissector_exe] [ws_dissector_lib_dir] [out] [-v] [--no-su]
 *
 * Defaults are relative paths suitable for an on-device shell session:
 *   fifo     ./diag_revealer_fifo
 *   diag_cfg ./Diag.cfg
 *   log_dir  ./mi2log
 *   out      android_decoded.txt
 *
 * ws_dissector_exe/ws_dissector_lib_dir fall back to $WS_DISSECTOR /
 * $WS_DISSECTOR_LIB if not given; if neither is set, raw_msg fields are
 * dumped as hex only (dissection stays optional, same as serialtest.cpp).
 *
 * --no-su spawns diag_revealer directly through /bin/sh instead of
 * `su -c` via /system/bin/sh -- for desktop testing against a mock
 * diag_revealer (no /dev/diag, no root).
 */

#include "../monitor_c/android_qc_monitor.h"
#include "../bytes_proto/qualcomm/consts.h"
#include "../ws_dissector_client/ws_dissector_client.h"

#include <cstdio>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <memory>
#include <string>
#include <vector>
#include <unistd.h>
#include <sstream>

// Protocol name carried by a raw_msg type_hint ("raw_msg/PROTO" -> "PROTO"),
// or "" if the hint is not a raw_msg.
static std::string raw_msg_proto(const std::string &type_hint) {
    const std::string prefix = "raw_msg/";
    if (type_hint.compare(0, prefix.size(), prefix) == 0)
        return type_hint.substr(prefix.size());
    return "";
}

static void dump_fields(std::ofstream &out, const FieldList &fields, int indent, WsDissector* ws) {
    const std::string pad(indent * 2, ' ');
    for (const FieldEntry &e : fields) {
        out << pad << e.name;
        if (!e.type_hint.empty())
            out << " [" << e.type_hint << "]";
        out << ": ";
        if (const auto *i = std::get_if<int64_t>(&e.value.data)) {
            out << *i << "\n";
        } else if (const auto *d = std::get_if<double>(&e.value.data)) {
            out << *d << "\n";
        } else if (const auto *s = std::get_if<std::string>(&e.value.data)) {
            out << *s << "\n";
        } else if (const auto *raw = std::get_if<std::vector<uint8_t>>(&e.value.data)) {
            out << raw->size() << " bytes:";
            char hex[4];
            for (uint8_t b : *raw) {
                snprintf(hex, sizeof(hex), " %02x", b);
                out << hex;
            }
            out << "\n";

            // Expand a raw_msg PDU via ws_dissector, if one is available.
            std::string proto = raw_msg_proto(e.type_hint);
            if (ws && ws->is_running() && !proto.empty() && !raw->empty()) {
                std::string xml = ws->decode(proto, raw->data(), raw->size());
                if (!xml.empty()) {
                        out << pad << "  --- ws_dissector (" << proto << ") ---\n";
                        std::istringstream iss(xml);
                        std::string line;
                        while (std::getline(iss, line))
                            out << pad << "  " << line << "\n";
                    } else {
                        out << pad << "  --- ws_dissector: no output for "
                            << proto << " ---\n";
                    }
            }
        } else if (const auto *sub = std::get_if<FieldList>(&e.value.data)) {
            out << "\n";
            dump_fields(out, *sub, indent + 1, ws);
        }
    }
}

static const char *packet_type_name(PacketType t) {
    switch (t) {
        case LOG_PACKET:    return "LOG";
        case DEBUG_PACKET:  return "DEBUG";
        case CUSTOM_PACKET: return "CUSTOM";
    }
    return "?";
}

int main(int argc, char **argv) {
    bool verbose = false;
    bool use_su  = true;
    std::vector<std::string> pos;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "-v" || a == "--verbose") verbose = true;
        else if (a == "--no-su")           use_su = false;
        else pos.push_back(a);
    }
    if (pos.empty()) {
        fprintf(stderr, "usage: %s <diag_revealer> [fifo] [diag_cfg] [log_dir] [android_pie_ws_dissector] [ws libs] [out] [-v] [--no-su]\n",
                argv[0]);
        return 1;
    }

    DiagRevealerParams params;
    params.exe_path      = pos[0];
    params.fifo_path     = pos.size() > 1 ? pos[1] : "./diag_revealer_fifo";
    params.diag_cfg_path = pos.size() > 2 ? pos[2] : "./Diag.cfg";
    params.log_dir       = pos.size() > 3 ? pos[3] : "./mi2log";
    params.use_su        = use_su;
    if (!use_su)
        params.shell_path = "/bin/sh";   // desktop mock run; no /system/bin/sh
    const std::string ws_path = pos.size() > 4 ? pos[4] : (getenv("WS_DISSECTOR") ? getenv("WS_DISSECTOR") : "");
    const std::string ws_lib = pos.size() > 5 ? pos[5] : (getenv("WS_DISSECTOR_LIB") ? getenv("WS_DISSECTOR_LIB") : "");
    const std::string out_path = pos.size() > 6 ? pos[6] : "android_decoded.txt";


    // Whitelist every known type, same as replay_mi2log.cpp, so packet
    // counts from a mock replay are directly comparable. For a real capture
    // you may want to trim this to the types of interest (fewer SET_MASK
    // entries in Diag.cfg -> less modem load).
    MonitorConfig cfg;
    cfg.verbose = verbose;
    cfg.perf_interval = 10;
    // for (int i = 0; i < LogPacketTypeID_To_Name_n; i++)
    //     cfg.type_names.push_back(LogPacketTypeID_To_Name[i].name);
    cfg.type_names.push_back(std::string("5G_NR_RRC_OTA_Packet"));
    cfg.type_names.push_back(std::string("5G_NR_NAS_SM_Plain_OTA_Incoming_Msg"));
    cfg.type_names.push_back(std::string("5G_NR_NAS_SM_Plain_OTA_Outgoing_Msg"));
    cfg.type_names.push_back(std::string("5G_NR_NAS_MM_Plain_OTA_Incoming_Msg"));
    cfg.type_names.push_back(std::string("5G_NR_NAS_MM_Plain_OTA_Outgoing_Msg"));


    std::ofstream out(out_path);
    if (!out) {
        fprintf(stderr, "cannot open %s for writing\n", out_path.c_str());
        return 1;
    }


    //Create a WsDissector
    WsDissector ws;
    WsDissector *wsp = nullptr;
    if (ws_path.empty()) {
        fprintf(stderr, "[ws_dissector] no path given (5th positional arg / $WS_DISSECTOR); "
                        "raw_msg fields -> hex only\n");
    } else {
        fprintf(stderr, "[ws_dissector] path = %s\n", ws_path.c_str());
        fprintf(stderr, "[ws_dissector] lib  = %s\n", ws_lib.empty() ? "(default loader path)" : ws_lib.c_str());
        // Is the binary actually there and executable?
        if (access(ws_path.c_str(), X_OK) != 0) {
            perror("[ws_dissector] access");
            fprintf(stderr, "[ws_dissector] executable not found/executable; raw_msg -> hex only\n");
        } else if (!ws.start(ws_path, ws_lib)) {
            fprintf(stderr, "[ws_dissector] fork/exec failed; raw_msg -> hex only\n");
        } else {
            // start() only forked; exec failure shows up as a dead child.
            // Probe with a tiny known message to confirm it actually responds.
            const uint8_t probe[] = {0x7e, 0x00};   // NAS-5GS EPD byte + spare
            std::string reply = ws.decode("nas-5gs", probe, sizeof(probe));
            if (reply.empty()) {
                fprintf(stderr, "[ws_dissector] started but no response to probe "
                                "(bad binary or missing libwireshark?); raw_msg -> hex only\n");
                ws.stop();
            } else {
                wsp = &ws;
                fprintf(stderr, "[ws_dissector] OK - process responded to probe "
                                "(%zu bytes of PDML); dissection ENABLED\n", reply.size());
            }
        }
    }
    int n_packets = 0;
    auto handler = [&](const DecodedPacket &pkt) -> bool {
        ++n_packets;
        out << "===== Packet " << n_packets
            << " | type=" << packet_type_name(pkt.type)
            << " | ok=" << (pkt.ok ? "true" : "false")
            << " | recv_timestamp=" << std::fixed << std::setprecision(6)
            << pkt.timestamp << " =====\n";
        dump_fields(out, pkt.fields, 0, wsp);
        out << "\n";
        // Live capture ends with Ctrl-C, before ~ofstream flushes.
        out.flush();
        fprintf(stderr, "\r[android_qc] decoded %d packet(s)", n_packets);
        return true;
    };

    AndroidQcMonitor monitor(std::make_unique<DiagFifoSource>(params.fifo_path),
                             std::make_unique<QualcommDecoder>(),
                             cfg, params);
    monitor.set_packet_handler(handler);
    monitor.run();   // setup() (Diag.cfg -> mkfifo -> spawn -> open) -> read loop

    printf("\ndecoded %d packet(s) -> %s\n", n_packets, out_path.c_str());
    return 0;
}
