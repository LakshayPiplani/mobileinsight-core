# MobileInsight Architecture

MobileInsight is an event-driven pipeline for below-IP mobile network monitoring and analysis.
Raw bytes from a cellular modem (Qualcomm DM / MTK) travel through a C/C++ decode layer into
a Python pub/sub system where pluggable analyzers consume decoded packets and emit higher-level
events, KPIs, and protocol state.

---

## Control Flow

```
User script
  │
  ├── OfflineReplayer() / OnlineMonitor()     ← create monitor (trace source)
  │     set_input_path(file) or open serial
  │     enable_log("LTE_RRC_OTA_Packet")      ← whitelist log types
  │
  ├── Analyzer().set_source(monitor)          ← register each analyzer
  │     calls monitor.register(self)
  │     adds analyzer to monitor.to_list
  │
  └── monitor.run()                           ← starts the blocking event loop
        │
        ├── [loop] read next raw frame
        ├── dm_collector_c.decode(frame)      ← C extension: HDLC + binary decode
        ├── [if OTA] ws_dissector binary      ← subprocess: ASN.1/RRC → XML
        ├── DMLogPacket(decoded_dict)
        ├── Event(timestamp, type_id, pkt)
        └── Element.send(event)
              │
              └── for each module in to_list:
                    module.recv(self, event)  ← fires registered callbacks
```

---

## Data Flow

```
Modem / .mi2log file
        │  raw HDLC bytes
        ▼
┌─────────────────────────────────────────────┐
│  dm_collector_c  (CPython C extension)      │
│  hdlc.cpp      → deframe HDLC              │
│  log_packet.cpp → dispatch to per-type .h  │
│  lte_rrc.h / nr_mac.h / lte_phy.h / ...   │
│  → Python dict  {field: value, ...}         │
└─────────────────────────────────────────────┘
        │  decoded dict
        │
        │  [if type_id needs ASN.1 decode]
        ▼
┌─────────────────────────────────────────────┐
│  ws_dissector  (standalone C++ binary)      │
│  embeds libwireshark / libwsutil            │
│  packet-aww.cpp registers custom dissector  │
│  → XML string  e.g. "<proto name=lte-rrc…>" │
│  → stored as decoded_dict['Msg']            │
└─────────────────────────────────────────────┘
        │  dict (+ optional XML string in 'Msg')
        ▼
  DMLogPacket(decoded_dict)
  dm_log_packet.py wraps the dict;
  .decode() returns it; .data returns self
        │
        ▼
  Event(timestamp, type_id, DMLogPacket)
        │
        │  Element.send()  →  pub/sub dispatch
        ▼
┌─────────────────────────────────────────────────────────────────┐
│  Analyzer callbacks (registered via add_source_callback)        │
│                                                                 │
│  MsgLogger          dump decoded XML to file / stdout           │
│                                                                 │
│  LteRrcAnalyzer.__rrc_filter:                                   │
│    msg.data.decode() → dict                                     │
│    ET.XML(dict['Msg']) → xml_tree                               │
│    state_machine.update_state(xml_msg)                          │
│       → RRC_IDLE ↔ RRC_CRX ↔ RRC_SDRX ↔ RRC_LDRX             │
│    __callback_sib_config → profile.update(cell config)         │
│    __callback_rrc_reconfig → profile.update(meas config)       │
│    self.send(xml_msg) → downstream analyzers                    │
│    send_to_coordinator(Event 'rrc state' / 'rsrp' / 'rsrq')   │
│                                                                 │
│  NrRrcAnalyzer / WcdmaRrcAnalyzer / LteNasAnalyzer  (similar)  │
│                                                                 │
│  KpiAnalyzer subclasses (depend on protocol analyzers):         │
│    include_analyzer('LteRrcAnalyzer', [self.callback])          │
│    receive forwarded XML events from protocol analyzers         │
│    store_kpi() → SQLite  (./dbs/Kpi.db on desktop)             │
│    upload_kpi() → cloud  (Android async thread)                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## Key Classes

| Class | File | Role |
|---|---|---|
| `Element` | `mobile_insight/element.py` | Base pub/sub primitive. Owns `to_list`, `send(event)`, `recv()`, shared logger. |
| `Event` | `mobile_insight/element.py` | Data envelope: `(timestamp, type_id, data)` passed between pipeline stages. |
| `Monitor` | `mobile_insight/monitor/monitor.py` | Abstract trace source. Subclasses implement `run()` to read raw bytes and call `send()`. |
| `OfflineReplayer` | `mobile_insight/monitor/offline_replayer.py` | Replays `.mi2log` / `.qmdl` files. Primary tool for offline analysis. |
| `OnlineMonitor` | `mobile_insight/monitor/online_monitor.py` | Live serial DM device monitor. |
| `DMLogPacket` | `mobile_insight/monitor/dm_collector/dm_endec/dm_log_packet.py` | Python wrapper around the dict returned by `dm_collector_c`. Optionally invokes `ws_dissector`. |
| `WsDissector` | `mobile_insight/monitor/dm_collector/dm_endec/ws_dissector.py` | Subprocess bridge to the `ws_dissector` binary for ASN.1/RRC XML decoding. |
| `Analyzer` | `mobile_insight/analyzer/analyzer.py` | Base consumer. Manages singleton registry, `include_analyzer` dependency wiring, `set_source`. |
| `ProtocolAnalyzer` | `mobile_insight/analyzer/protocol_analyzer.py` | Adds `Profile` (config tracking) and `StateMachine` (protocol state) to `Analyzer`. |
| `LteRrcAnalyzer` | `mobile_insight/analyzer/lte_rrc_analyzer.py` | Parses LTE RRC XML; drives RRC state machine; emits decoded events downstream. |
| `NrRrcAnalyzer` | `mobile_insight/analyzer/nr_rrc_analyzer.py` | Same for 5G NR RRC. |
| `LteNasAnalyzer` | `mobile_insight/analyzer/lte_nas_analyzer.py` | LTE NAS/EMM protocol state and events. |
| `StateMachine` | `mobile_insight/analyzer/state_machine.py` | Generic protocol state machine. Transition functions are predicates on `Event`. |
| `Profile` / `ProfileHierarchy` | `mobile_insight/analyzer/profile.py` | Declarative current-config store. Keyed paths like `LteRrcProfile:cell_freq.idle.serv_config`. |
| `KpiAnalyzer` | `mobile_insight/analyzer/kpi/kpi_analyzer.py` | Base KPI analyzer. Provides `register_kpi`, `store_kpi`, local SQLite DB, async cloud upload. |
| `KPIManager` | `mobile_insight/analyzer/kpi/kpi_manager.py` | Auto-discovers all `KpiAnalyzer` subclasses via `inspect`; unified `enable_kpi` / `local_query_kpi` interface. |

---

## Extension Points

### Adding a new log type (binary C-level)

1. Add a header `dm_collector_c/<type>.h` using the macro patterns in `parser_template.h`.
2. Register the log type code in `dm_collector_c/log_packet.cpp`'s dispatch table.
3. Rebuild the C extension: `python3 setup.py build_ext --inplace`.

### Adding a new RRC/NAS message decode (ASN.1)

1. Edit `ws_dissector/ws_dissector.cpp` or `ws_dissector/packet-aww.cpp`.
2. Rebuild: see the `g++` invocation in `install-ubuntu.sh` (links `libwireshark`, `libwsutil`, `libwiretap`).

### Adding a new Analyzer

1. Subclass `Analyzer` (or `ProtocolAnalyzer` if you need Profile + StateMachine).
2. In `__init__`, call `add_source_callback(self.__my_filter)`.
3. In the callback, check `msg.type_id` and process `msg.data.decode()` dict or parsed XML.
4. Add the class to `mobile_insight/analyzer/__init__.py`'s `__all__`.
5. Wire it in a script: `my_analyzer.set_source(monitor)`.

To chain analyzers (one consumes another's output):
```python
# inside MyAnalyzer.__init__:
self.include_analyzer('LteRrcAnalyzer', [self.__on_rrc_msg])
```
`include_analyzer` looks up the singleton, wires `__on_rrc_msg` to receive events that
`LteRrcAnalyzer` forwards via its own `self.send(event)` call.

### Adding a new KPI

1. Subclass `KpiAnalyzer` directly (`__bases__[0]` must be `KpiAnalyzer`).
2. In `__init__`, call `self.register_kpi(type, name, callback)` for each KPI.
3. Add to `mobile_insight/analyzer/kpi/__init__.py`'s imports.
4. `KPIManager` will auto-discover it on next instantiation via `inspect`.

---

## Build Artifacts

| Artifact | Built by | Role |
|---|---|---|
| `dm_collector_c.so` | `python3 setup.py build_ext` | CPython C extension. HDLC deframing + binary log-packet parsing for all 2G/3G/4G/5G log types. |
| `ws_dissector` binary | Wireshark CMake / `g++` in `install-ubuntu.sh` | Standalone process. Embeds `libwireshark` to decode OTA RRC/NAS messages to XML via Wireshark dissectors. |
| `mi-gui` | `install-ubuntu.sh` copies `gui/mobile_insight_gui.py` | Desktop GUI launcher. Installed to `/usr/local/bin/mi-gui`. |

The two binary artifacts are **independently built** and communicate only at runtime:
`dm_collector_c` produces a dict with a raw message bytes field; `ws_dissector.py` pipes
those bytes to the `ws_dissector` subprocess and reads back XML.
