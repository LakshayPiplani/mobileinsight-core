#include "../bytes_channel/serial_port.h"
#include "../monitor_c/qualcomm_desktop_monitor.h"
#include "../bytes_proto/qualcomm/consts.h"
#include "../ws_dissector_client/ws_dissector_client.h"

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include <unistd.h>   // access()

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

    // Pull out -v/--verbose wherever it appears, then treat the rest of argv
    // as positional (port, baud, ws_dissector, ws_lib, out_path).
    bool verbose = false;
    std::vector<std::string> pos;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "-v" || a == "--verbose") verbose = true;
        else pos.push_back(a);
    }

    const std::string ser_port  = pos.size() > 0 ? pos[0] : "/dev/ttyUSB0";
    const int baud_rate         = pos.size() > 1 ? atoi(pos[1].c_str()) : 9600;

    // ---- optional ws_dissector for raw_msg (RRC/NAS) PDU expansion ----
    // Path from positional arg 3 or $WS_DISSECTOR; libwireshark dir from
    // positional arg 4 or $WS_DISSECTOR_LIB. If unset or the process won't
    // start, raw_msg fields are dumped as hex only.
    std::string ws_path  = pos.size() > 2 ? pos[2] : (getenv("WS_DISSECTOR") ? getenv("WS_DISSECTOR") : "");
    std::string ws_lib   = pos.size() > 3 ? pos[3] : (getenv("WS_DISSECTOR_LIB") ? getenv("WS_DISSECTOR_LIB") : "");
    std::string out_path = pos.size() > 4 ? pos[4] : "serial_decoded.txt";

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
    cfg.port_path = ser_port;
    cfg.baud_rate = baud_rate;
    cfg.verbose = verbose;
    cfg.perf_interval = 10;   // print a [PERF] line every 10 packets
    // NOTE: do NOT point log_output_path at out_path. out_path is written by
    // our own std::ofstream (text dump) below; log_output_path is opened
    // separately by the decoder's export_manager (raw binary .mi2log,
    // fopen(path, "wb")). Pointing both at the same file gives two
    // independent writers truncating/appending the same path -> corruption.
    // Leave it unset (no .mi2log export) unless you pass a distinct path.
    //
    // Type whitelist: the five NR types from the validated Python capture
    // (ea0_option branch, lp_v2_EA0_Jul8.mi2log). All are equip-id 0xB, so
    // setup() sends exactly one SET_MASK — mirroring the known-working run.
    // Widen this list once single-batch operation is confirmed on hardware.
    cfg.type_names = {
        "5G_NR_RRC_OTA_Packet",
        "5G_NR_NAS_SM_Plain_OTA_Incoming_Msg",
        "5G_NR_NAS_SM_Plain_OTA_Outgoing_Msg",
        "5G_NR_NAS_MM_Plain_OTA_Incoming_Msg",
        "5G_NR_NAS_MM_Plain_OTA_Outgoing_Msg",
        "5G_NR_PDCP_UL_Control_Pdu",
        "5G_NR_RLC_DL_Stats",
        "5G_NR_MAC_UL_TB_Stats",
        "5G_NR_MAC_UL_Physical_Channel_Schedule_Report",
        "5G_NR_MAC_PDSCH_Stats",
        "5G_NR_MAC_RACH_Trigger"
    };

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
        // A live serial monitor runs until Ctrl-C, which kills the process
        // before ~ofstream flushes. Flush per packet so the capture survives.
        out.flush();
        fprintf(stderr, "\r[serialtest] decoded %d packet(s)", n_packets);
        return true;   // keep running; false would stop the monitor
    };

    // ---- wire up the stack and run ----
    QualcommDesktopMonitor monitor(std::make_unique<SerialPort>(cfg.port_path, cfg.baud_rate),
                                   std::make_unique<QualcommDecoder>(),
                                   cfg);
    monitor.set_packet_handler(handler);
    monitor.run();   // setup() -> configure() -> read/feed/decode until EOF

    printf("\ndecoded %d packet(s) from %s -> %s\n",
           n_packets, cfg.port_path.c_str(), out_path.c_str());
    if (n_packets == 0) {
        fprintf(stderr,
            "[serialtest] zero packets decoded. Common causes:\n"
            "  - modem is idle: LOG_F frames only appear during RRC/NAS/PHY\n"
            "    activity (e.g. attach, paging, a call/data session) - DIAG\n"
            "    config ACKs and other non-log chatter don't count\n"
            "  - capture ended (Ctrl-C) before any activity happened - try\n"
            "    triggering network activity or waiting longer\n"
            "  - setup() failed silently, or the port/phone isn't in DIAG mode\n"
            "Re-run with -v to see per-frame classification (LOG/DEBUG/CUSTOM/\n"
            "dropped) and confirm whether frames are arriving at all.\n");
    }
    return 0;
}

