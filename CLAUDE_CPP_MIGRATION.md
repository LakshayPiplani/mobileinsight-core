# C++ Migration Plan

> **Decoder sub-plan:** the full detail of removing `PyObject*` from the
> decoder layer — `FieldValue` design, substitution map, phase order, special
> cases — lives in [CLAUDE_DECODER_CPP_MIGRATION.md](CLAUDE_DECODER_CPP_MIGRATION.md).

Scope decision (confirmed with project owner, 2026-07-06): this is ultimately
a full-stack migration (monitor layer **and** analyzers), but analyzers are
lowest priority. Active work is the **Monitor layer** — the four concrete ways
MobileInsight acquires raw DM bytes before they reach a decoder:

1. Read from a `.mi2log`/`.qmdl` file (desktop **and** Android) — `OfflineReplayer`
2. Read from USB serial on desktop (Qualcomm) — `QualcommDesktopMonitor`
3. Read from a diag port via `diag_revealer` on Android (Qualcomm) — `AndroidQcMonitor`
4. Read MTK `muxraw` log files on Android (MediaTek) — `AndroidMtkMonitor`

Everything downstream of "decoded packet dict" (Analyzer, KpiAnalyzer, state
machines, `Profile`, XML/ASN.1 via `ws_dissector`) stays in Python for now.

---

## 1. Current State of the Codebase

### 1.1 Done (untracked, on `cpp_migration`)

| Component | File(s) | Notes |
|---|---|---|
| Channel interface | `bytes_channel/bytes_channel.h` | `ByteChannel` ABC |
| Serial port I/O | `bytes_channel/serial_port.{h,cpp}` | POSIX `termios` RAII; replaces `pyserial` |
| Channel build | `bytes_channel/Makefile` | produces `libbytes_channel.a` |
| HDLC framing | `bytes_proto/qualcomm/hdlc.{h,cpp}` | moved from `dm_collector_c/`; unescaping/CRC |
| Protocol data | `bytes_proto/qualcomm/consts.{h,cpp}` | `LogPacketType` enum, `LogPacketTypeID_To_Name[]`, `_n` |
| Protocol data | `bytes_proto/qualcomm/log_config.{h,cpp}` | `encode_log_config()`, `sort_type_ids()`, `map_typenames_to_ids()` |
| Protocol data | `bytes_proto/qualcomm/utils.{h,cpp}` | `find_ids()`, `IdVector`, `ValueName` |
| Preferences | `monitor_c/monitor_config.h` | `MonitorConfig` plain struct |
| Decoder ABC + packet type | `decoder/decoder.h` | `Decoder` ABC + `DecodedPacket` + `PacketType` |
| Qualcomm decoder | `decoder/qualcomm_decoder/qualcomm_decoder.{h,cpp}` | `QualcommDecoder : Decoder` — still has `PyObject*`; Step 2 finish |
| Log-packet parsers | `decoder/qualcomm_decoder/log_packet.{h,cpp}` + `*.h` | ~90 per-type binary parsers; unchanged |
| Export | `export_manager/export_manager.{h,cpp}` | independent top-level module (`libexport_manager.a`); caller passes `type_id` — no frame re-parsing |
| Python extension | `decoder/qualcomm_decoder/dm_collector_c.cpp` | legacy CPython extension; stays until Step 6 |
| Monitor base | `monitor_c/monitor_base.{h,cpp}` | `MonitorBase` with `run()`, `set_packet_handler()` |
| Qualcomm desktop monitor | `monitor_c/qualcomm_desktop_monitor.{h,cpp}` | `setup()` fully implemented: `disable_log_all` → `enable_log` |
| Monitor build | `monitor_c/Makefile` | links `libbytes_channel.a + libqualcomm_proto.a` |

### 1.2 Remaining in Step 2

| Task | Detail |
|---|---|
| ✅ `decoder/field_value.h` | New header: `FieldEntry`, `FieldList`, `FieldValue` variant |
| ✅ Rewrite `_decode_by_fmt` in `log_packet_helper.h` | Signature `PyObject* result` → `FieldList& result`; all `Py_BuildValue`/`PyList_Append` replaced |
| ✅ Rewrite `_decode_*` helpers in `log_packet.cpp` + all per-type headers | Done (Phases B+C, 2026-07-08); compiles clean, smoke-tested |
| ✅ Rewrite `decode_log_packet`, `decode_custom_packet`, `decode_log_packet_modem` | Return `FieldList` instead of `PyObject*` |
| `QualcommDecoder::receive_log_packet` | Frame extraction loop (HDLC unwrap, classify, call C++ decode, populate `DecodedPacket`) |
| `QualcommDecoder::reset` | Call `reset_binary()` from `hdlc.h` |
| Remove `PyObject*` from `qualcomm_decoder.{h,cpp}` | Drop `#include <Python.h>`, delete `get_next_packet` once `receive_log_packet` is done |
| Fix `qualcomm_decoder.cpp` includes | Local `"log_config.h"` etc. → `"../../bytes_proto/qualcomm/"` paths |

---

## 2. Library Architecture

Six components. `bytes_proto/qualcomm` is the shared protocol layer consumed
by both monitor and decoder:

```
bytes_channel/                  ByteChannel ABC + concrete I/O (SerialPort, FileSource, …)
bytes_proto/qualcomm/           Qualcomm protocol data: hdlc, consts, utils, log_config
bytes_proto/mtk/                MTK protocol data (future)

export_manager/                 whitelist filter + .mi2log writer (links bytes_proto/qualcomm)

decoder/decoder.h               Decoder ABC + DecodedPacket + PacketType  (header-only)
decoder/qualcomm_decoder/       QualcommDecoder : Decoder  (links bytes_proto/qualcomm + export_manager)
decoder/mtk_decoder/            MtkDecoder : Decoder  (future, Step 5)

monitor_c/                      MonitorBase + concrete monitors
                                links: bytes_channel + bytes_proto/qualcomm + decoder
```

### 2.1 Channel library — `bytes_channel/`

Compiles to `libbytes_channel.a`. Interface:

```cpp
class ByteChannel {
public:
    virtual ~ByteChannel() = default;
    virtual bool    open()                           { return false; }
    virtual ssize_t read(char* buf, size_t n)        = 0;
    virtual bool    write(const char* buf, size_t n) { return false; }
    virtual bool    is_open() const                  { return false; }
    virtual void    close()                          {}
};
```

Design rules:
- `open()`, `is_open()`, `close()`, `write()` are non-pure with safe defaults.
  Returning `false` from `open()` is intentional: a channel that doesn't need
  opening should not be opened, and `false` causes `setup()` to fail visibly.
- Construction-time params go in the constructor, not in `open()`. `open()` is
  a no-arg trigger that uses state set at construction.

Concrete implementations, in build order:

| Class | File | Wraps | Status |
|---|---|---|---|
| `SerialPort` | `bytes_channel/serial_port.{h,cpp}` | POSIX `termios` fd | ✅ done |
| `FileSource` | `bytes_channel/file_source.{h,cpp}` | plain file `read()` | Step 3 |
| `DiagFifoSource` | `bytes_channel/diag_fifo_source.{h,cpp}` | FIFO + `diag_revealer` subprocess | Step 4 |
| `MtkMuxrawSource` | `bytes_channel/mtk_muxraw_source.{h,cpp}` | directory-poll + `.muxraw.tmp` read | Step 5 |

### 2.2 Protocol library — `bytes_proto/qualcomm/`

Compiles to `libqualcomm_proto.a`. No Python dependency. Shared by both the
monitor (sends DIAG commands) and the decoder (identifies and filters incoming
log packets). A parallel `bytes_proto/mtk/` will hold MTK-specific protocol
data when that work begins.

| File | Provides |
|---|---|
| `hdlc.{h,cpp}` | `encode_hdlc_frame()`, `feed_binary()`, `get_next_frame()` |
| `utils.{h,cpp}` | `IdVector`, `ValueName`, `find_ids()`, `search_name()` |
| `consts.{h,cpp}` | `LogPacketType` enum, `LogPacketTypeID_To_Name[]`, `LogPacketTypeID_To_Name_n`, `is_log_packet()`, `is_debug_packet()`, `is_custom_packet()` |
| `log_config.{h,cpp}` | `LogConfigOp` enum, `BinaryBuffer`, `encode_log_config()`, `get_equip_id()`, `get_item_id()`, `sort_type_ids()`, `map_typenames_to_ids()` |

`LogPacketTypeID_To_Name[]` is **defined in `consts.cpp`**, declared `extern`
in `consts.h`. Other TUs must use `LogPacketTypeID_To_Name_n` for the count —
`ARRAY_SIZE()` does not work on an `extern []` incomplete type.

### 2.3 Preferences — `monitor_c/monitor_config.h`

Plain data struct. Set by caller before constructing the monitor; no setters
needed on `MonitorBase`.

```cpp
struct MonitorConfig {
    std::string port_path;                 // serial port device (e.g. /dev/ttyUSB0)
    int         baud_rate    = 0;          // serial baud rate
    std::vector<std::string> type_names;   // log types to enable on modem
    bool        skip_decoding  = false;
    std::string log_output_path;           // path for .mi2log export; empty = no export
    double      sampling_rate  = 1.0;
};
```

**Which side uses which fields:**

| Field | Monitor | Decoder |
|---|---|---|
| `port_path`, `baud_rate` | constructs `SerialPort` at factory time | — |
| `type_names` | `enable_log()` → SET_MASK commands to modem | export whitelist filter |
| `log_output_path` | — | opens `.mi2log` file |
| `skip_decoding`, `sampling_rate` | — | decode skip / rate control |

### 2.4 Decoder library — `decoder/`

`decoder/decoder.h` is a single header-only file containing the ABC and packet
types. No CPython API anywhere in this library.

#### 2.4.1 Decoded-value tree

All decoding happens in C++. The Python boundary (Step 6) receives a finished
tree and converts it to a Python dict — it does not call `decode_log_packet`.

```cpp
// decoder/field_value.h  (new)

struct FieldEntry;
using FieldList = std::vector<FieldEntry>;

struct FieldValue {
    using V = std::variant<
        int64_t,              // UINT (all widths ≤ 8 bytes)
        double,               // RSRP, RSRQ, BANDWIDTH, float
        std::string,          // BYTE_STREAM, PLMN, datetime as ISO string
        std::vector<uint8_t>, // raw_msg bytes (opaque protocol payload)
        FieldList             // nested list or dict
    >;
    V data;
};

struct FieldEntry {
    std::string name;
    FieldValue  value;
    std::string type_hint; // "", "list", "dict", or "raw_msg/PROTO"
};
```

`type_hint` semantics (same as the original Python encoding):

| hint | meaning |
|---|---|
| `""` | scalar leaf (int, double, string) |
| `"list"` | value is a `FieldList`; all items are homogeneous (`"dict"` entries) |
| `"dict"` | value is a `FieldList` of mixed scalar/nested fields for one record |
| `"raw_msg/PROTO"` | value is raw bytes; PROTO names the Wireshark dissector |

#### 2.4.2 Packet and decoder interfaces

```cpp
enum PacketType { LOG_PACKET, DEBUG_PACKET, CUSTOM_PACKET };

struct DecodedPacket {
    PacketType type;
    double     timestamp; // POSIX time; -1.0 if not requested
    FieldList  fields;    // fully decoded tree; empty on failure
    bool       ok = false;
};

class Decoder {
public:
    virtual ~Decoder() = default;
    virtual void configure(const MonitorConfig& config) = 0;
    virtual void feed(const char* buf, int n) = 0;
    virtual bool receive_log_packet(DecodedPacket& out) = 0; // false = buffer empty
    virtual void reset() = 0;
};
```

`enable_logs()` / `disable_logs()` are **not** on `Decoder`. Those are modem
commands sent over the wire — they belong in each concrete monitor's `setup()`.

Concrete implementations:

| Class | Directory | Wraps | Status |
|---|---|---|---|
| `QualcommDecoder` | `decoder/qualcomm_decoder/` | hdlc + log_packet + export_manager | Step 2 — `field_value.h` + `_decode_by_fmt` rewrite + `receive_log_packet` pending |
| `MtkDecoder` | `decoder/mtk_decoder/` | port of `mtk_log_parser.py` | Step 5 |

### 2.5 Monitor library — `monitor_c/`

`MonitorBase` has two concrete methods (`run()`, `set_packet_handler()`);
everything else is pure virtual or injected:

```cpp
class MonitorBase {
public:
    MonitorBase(std::unique_ptr<ByteChannel> source,
                std::unique_ptr<Decoder> decoder,
                const MonitorConfig& config);
    virtual ~MonitorBase() = default;

    virtual std::vector<std::string> available_log_types() const = 0;

    void set_packet_handler(std::function<bool(const DecodedPacket&)> handler);
    void run();

protected:
    std::unique_ptr<ByteChannel> source_;
    std::unique_ptr<Decoder>     decoder_;
    MonitorConfig                config_;

    virtual bool setup() = 0;

private:
    std::function<bool(const DecodedPacket&)> handler_;
};
```

Note: `handler_` returns `bool` — returning `false` from the handler stops `run()`.

Concrete monitors:

| Class | Source | Decoder | `setup()` does | Replaces |
|---|---|---|---|---|
| `QualcommDesktopMonitor` | `SerialPort` | `QualcommDecoder` | `disable_log_all` → `enable_log` | `dm_collector.py` |
| `OfflineReplayer` | `FileSource` | `QualcommDecoder` or `MtkDecoder` | nothing | `offline_replayer.py` |
| `AndroidQcMonitor` | `DiagFifoSource` | `QualcommDecoder` | spawn `diag_revealer`, create FIFO | `android_dev_diag_monitor.py` |
| `AndroidMtkMonitor` | `MtkMuxrawSource` | `MtkDecoder` | start directory polling | `android_mtk_monitor.py` |

**Why one class per profile, not M×N?** Setup logic is tightly coupled to both
source type and chipset. The four profiles are a fixed, small table of real-world
configurations; invalid combinations simply don't exist as classes.

**Runtime selection:** all monitors compiled into the binary; a factory
instantiates the correct one from `MonitorConfig` at runtime.

---

## 3. Python Modules Being Replaced

| File | Replaced by |
|---|---|
| `mobile_insight/monitor/monitor.py` | `MonitorBase` (§2.5) |
| `mobile_insight/monitor/dm_collector/dm_collector.py` | `QualcommDesktopMonitor` + `SerialPort` + `QualcommDecoder` |
| `mobile_insight/monitor/offline_replayer.py` | `OfflineReplayer` + `FileSource` |
| `mobile_insight/monitor/android_dev_diag_monitor.py` | `AndroidQcMonitor` + `DiagFifoSource` |
| `mobile_insight/monitor/android_mtk_monitor.py` | `AndroidMtkMonitor` + `MtkMuxrawSource` |
| `mobile_insight/monitor/mtk_log_parser.py` | `MtkDecoder` |
| `mobile_insight/monitor/mtk_offline_replayer.py` | folded into `OfflineReplayer` |
| `mobile_insight/monitor/online_monitor.py` | shrinks to a C++ factory function |
| `mobile_insight/element.py` | unchanged — still the hand-off point to Python Analyzers |

---

## 4. Build Order

**Step 1 — Channel + Protocol libraries** ✅
- `bytes_channel/bytes_channel.h` — `ByteChannel` interface ✅
- `bytes_channel/serial_port.{h,cpp}` — `SerialPort : ByteChannel` ✅
- `bytes_channel/Makefile` → `libbytes_channel.a` ✅
- `bytes_proto/qualcomm/` — `libqualcomm_proto.a` (hdlc, utils, consts, log_config) ✅
- `monitor_c/monitor_config.h` — `MonitorConfig` struct ✅

**Step 2 — Decoder + MonitorBase + QualcommDesktopMonitor** ← active
- `decoder/decoder.h` — `Decoder` ABC + `DecodedPacket` + `PacketType` ✅
- `monitor_c/monitor_base.{h,cpp}` — `run()`, `set_packet_handler()` ✅
- `monitor_c/qualcomm_desktop_monitor.{h,cpp}` — `setup()` with DIAG commands ✅
- `decoder/qualcomm_decoder/qualcomm_decoder.{h,cpp}` — `QualcommDecoder : Decoder` ✅ (struct done)
- Implement `QualcommDecoder::receive_log_packet` — port `get_next_packet` logic, no `PyObject*` ← **next**
- Implement `QualcommDecoder::reset` — call `reset_binary()`  ← **next**
- Remove `#include <Python.h>` + `get_next_packet` from `qualcomm_decoder.h` ← **next**
- Fix `qualcomm_decoder.cpp` includes (`"log_config.h"` → `"../../bytes_proto/qualcomm/log_config.h"` etc.) ← **next**

**Step 3 — OfflineReplayer + FileSource**
- `bytes_channel/file_source.{h,cpp}`
- `monitor_c/offline_replayer.{h,cpp}`

**Step 4 — AndroidQcMonitor + DiagFifoSource**
- `bytes_channel/diag_fifo_source.{h,cpp}`
- `monitor_c/android_qc_monitor.{h,cpp}`

**Step 5 — AndroidMtkMonitor + MtkMuxrawSource + MtkDecoder**
Largest piece: `MtkDecoder` has no C++ reference — `mtk_log_parser.py` must
be ported from scratch.
- `bytes_channel/mtk_muxraw_source.{h,cpp}`
- `bytes_proto/mtk/` — MTK protocol library
- `decoder/mtk_decoder/mtk_decoder.{h,cpp}`
- `monitor_c/android_mtk_monitor.{h,cpp}`

**Step 6 — Facade + Python boundary**
Shrink `online_monitor.py` to a factory. The Python facade walks
`DecodedPacket.fields` (a finished `FieldList` tree) and converts it to a
Python dict — no call to `decode_log_packet` at the boundary, since decoding
is already complete in C++.

**Step 7 — Analyzers (deferred, lowest priority)**

---

## 5. Python Hand-off (resolved 2026-07-07)

No `PyObject*` construction, no CPython API calls anywhere in the
Channel/Protocol/Decoder/Monitor libraries.

All field decoding happens in C++ inside `QualcommDecoder::receive_log_packet`.
`DecodedPacket.fields` is a fully built `FieldList` tree by the time it crosses
the boundary. The Python facade in Step 6 does a mechanical walk of the tree to
produce a Python dict — no re-parsing, no `decode_log_packet` call at the
`.so` boundary.

**Impact on `log_packet.cpp`:** all `_decode_by_fmt` / `_decode_*` functions
need their signatures changed from `PyObject* result` to `FieldList& result`,
and every `Py_BuildValue` / `PyList_Append` / `_replace_result` call replaced
with the equivalent `FieldList` push. The per-packet decode logic (the ~90
`switch` branches in `on_demand_decode`) is otherwise unchanged.

The exact transport format across the Python boundary (JSON, msgpack, direct
CPython extension call) is deferred to Step 6 — it does not affect library
interfaces.
