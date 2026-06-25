# ws_dissector

A small C++ program that wraps Wireshark's dissection engine (`libwireshark`/`libwsutil`/`libwiretap`) so MobileInsight can decode standardized 3GPP messages (RRC, NAS) without re-implementing ASN.1/PER decoding itself.

It is built and consumed entirely outside the Python package: [install-ubuntu.sh](../install-ubuntu.sh) compiles it and installs the binary to `/usr/local/bin/ws_dissector`; [`WSDissector`](../mobile_insight/monitor/dm_collector/dm_endec/README.md) (Python side) spawns it as a long-lived subprocess and talks to it over stdin/stdout.

## Files

- **`ws_dissector.cpp`** — `main()`. Initializes a minimal `libwireshark` session (`epan_init`, `wtap_init`), registers a custom dissector for a private "user DLT" (`prefs_set_pref("uat:user_dlts:...")`), then loops reading framed requests from stdin and writing PDML XML to stdout.
- **`packet-aww.cpp` / `packet-aww.h`** — defines the "AWW" (Automator Wireshark Wrapper) protocol: a bespoke Wireshark dissector, **not a standard**, that exists solely to map a numeric protocol ID to the real Wireshark dissector that should decode the payload (e.g. `201` → `lte-rrc.ul.dcch`, `416` → `nas-5gs`).

## Wire protocol (stdin → ws_dissector → stdout)

Each request is `[4 bytes BE: AWW protocol number][4 bytes BE: payload length][payload bytes]` — no pcap file, no L2/L3 framing, just the raw 3GPP message body (`ws_dissector.cpp:156-210`). The protocol number indexes the table in `init_proto_names()` (`packet-aww.cpp:26-83`), which must be kept in sync with `WSDissector.SUPPORTED_TYPES` in [`dm_endec/ws_dissector.py`](../mobile_insight/monitor/dm_collector/dm_endec/ws_dissector.py).

For PDCP-LTE signaling (protocol IDs 300/301), `ws_dissector.cpp` and `packet-aww.cpp` hand-construct a synthetic `pdcp_lte_info` header before payload bytes, because the real `pdcp-lte` Wireshark dissector expects metadata (direction, channel type, ROHC config) that a raw capture wouldn't otherwise provide.

`ws_dissector.cpp` reads the request into an in-memory frame via `tvb_new_real_data()` (`pkt_encap = WTAP_ENCAP_USER1`) — bypassing Wireshark's normal file-reading layer entirely — dissects it with `epan_dissect_run()`, and writes the resulting tree as PDML XML via `write_pdml_proto_tree()`, followed by a literal sentinel line `===___===` that the Python side uses to detect "end of this message's output" (`ws_dissector.cpp:96,211`).

## Control/data flow

```
WSDissector.decode_msg(msg_type, bytes)   [Python, dm_endec/ws_dissector.py]
  -> writes [type][len][bytes] to ws_dissector's stdin
  -> dissect_aww() in packet-aww.cpp looks up type in "aww.proto" dissector table
  -> hands payload to the real dissector (lte-rrc, nr-rrc, nas-eps, nas-5gs, pdcp-lte, ...)
  -> ws_dissector.cpp writes PDML XML + "===___===" to stdout
  -> WSDissector.decode_msg reads until the sentinel, returns the XML string
```

This is the second of MobileInsight's two decode paths — see [dm_collector_c/README.md](../dm_collector_c/README.md) for the first (binary field decoding in C). Only messages whose Qualcomm-defined header MobileInsight cannot fully parse itself (full RRC/NAS PDUs) get routed here; everything else is decoded directly by `dm_collector_c`.

## Preferences

Wireshark dissector preferences (e.g. NAS-5GS's "Try to detect and decode 5G-EA0 ciphered messages", `nas-5gs.null_decipher`) are **not** loaded from any preferences file here — `ws_dissector.cpp` never calls `read_prefs()`/`epan_load_settings()`. Every dissector preference runs at Wireshark's compiled-in default except `user_dlts`, which is set explicitly via `prefs_set_pref()`. To change a preference's behavior, add another `prefs_set_pref("<module>.<pref>:<value>", &errmsg)` call next to the existing `user_dlts` one and recompile.

## Build

See the `g++` invocation in [install-ubuntu.sh](../install-ubuntu.sh) (links against `-lwireshark -lwsutil -lwiretap`, headers from the bundled Wireshark 3.4.0 source tree). To rebuild after editing these files without rerunning the full installer:

```
cd ws_dissector
g++ ws_dissector.cpp packet-aww.cpp -o ws_dissector $(pkg-config --libs --cflags glib-2.0) \
    -I"$WIRESHARK_SRC_PATH" -L/usr/local/lib -lwireshark -lwsutil -lwiretap
sudo cp ws_dissector /usr/local/bin/
```
