/* replay_mi2log.cpp
 *
 * Replays a .mi2log capture through the full monitor stack:
 *
 *   FileSource (reads the .mi2log)
 *       -> OfflineReplayer::run()
 *       -> QualcommDecoder (HDLC unwrap + decode to FieldList)
 *       -> packet handler dumps every DecodedPacket to a text file
 *
 * Usage: ./replay_mi2log [input.mi2log] [output.txt]
 */

#include "../monitor_c/offline_replayer.h"
#include "../bytes_proto/qualcomm/consts.h"
#include "../ws_dissector_client/ws_dissector_client.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <unistd.h>   // access()

// ---------------------------------------------------------------------------
// Recursive FieldList -> text dump
// ---------------------------------------------------------------------------
// Protocol name carried by a raw_msg type_hint ("raw_msg/PROTO" -> "PROTO"),
// or "" if the hint is not a raw_msg.
static std::string raw_msg_proto(const std::string &type_hint) {
    const std::string prefix = "raw_msg/";
    if (type_hint.compare(0, prefix.size(), prefix) == 0)
        return type_hint.substr(prefix.size());
    return "";
}

// `ws` may be null (dissection disabled); then raw_msg bytes are just dumped.
static void dump_fields(std::ofstream &out, const FieldList &fields, int indent,
                        WsDissector *ws) {
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
    const std::string in_path =
        argc > 1 ? argv[1]
                 : "/home/lakshay/mobileinsight-mobile_withwireshark/lp_EA0_Jun29.mi2log";
    const std::string out_path = argc > 2 ? argv[2] : "decoded_packets.txt";

    // ---- optional ws_dissector for raw_msg (RRC/NAS) PDU expansion ----
    // Path from argv[3] or $WS_DISSECTOR; libwireshark dir from argv[4] or
    // $WS_DISSECTOR_LIB. If unset or the process won't start, raw_msg fields
    // are dumped as hex only (previous behavior).
    std::string ws_path = argc > 3 ? argv[3] : (getenv("WS_DISSECTOR") ? getenv("WS_DISSECTOR") : "");
    std::string ws_lib  = argc > 4 ? argv[4] : (getenv("WS_DISSECTOR_LIB") ? getenv("WS_DISSECTOR_LIB") : "");
    WsDissector ws;
    WsDissector *wsp = nullptr;
    if (ws_path.empty()) {
        fprintf(stderr, "[ws_dissector] no path given (argv[3] / $WS_DISSECTOR); "
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

    // ---- MonitorConfig ----
    // type_names doubles as the decoder's export/decode whitelist, so list
    // every known type explicitly ("all" would match nothing there).
    MonitorConfig cfg;
    cfg.port_path = in_path;
    cfg.perf_interval = 10;   // print a [PERF] line every 10 packets
    for (int i = 0; i < LogPacketTypeID_To_Name_n; ++i)
        cfg.type_names.push_back(LogPacketTypeID_To_Name[i].name);

    // ---- output + handler ----
    std::ofstream out(out_path);
    if (!out) {
        fprintf(stderr, "cannot open %s for writing\n", out_path.c_str());
        return 1;
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
        return true;   // keep running; false would stop the monitor
    };

    // ---- wire up the stack and run ----
    OfflineReplayer monitor(std::make_unique<FileSource>(in_path),
                            std::make_unique<QualcommDecoder>(),
                            cfg);
    monitor.set_packet_handler(handler);
    monitor.run();   // setup() -> configure() -> read/feed/decode until EOF

    printf("decoded %d packet(s) from %s -> %s\n",
           n_packets, in_path.c_str(), out_path.c_str());
    return 0;
}
