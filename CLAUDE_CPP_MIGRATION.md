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
| Qualcomm decoder | `decoder/qualcomm_decoder/qualcomm_decoder.{h,cpp}` | `QualcommDecoder : Decoder` — fully PyObject-free; `receive_log_packet`/`reset` implemented; builds into `libqualcomm_decoder.a` |
| Log-packet parsers | `decoder/qualcomm_decoder/log_packet.{h,cpp}` + `*.h` | ~90 per-type binary parsers; unchanged |
| Export | `export_manager/export_manager.{h,cpp}` | independent top-level module (`libexport_manager.a`); caller passes `type_id` — no frame re-parsing |
| Python extension | `decoder/qualcomm_decoder/dm_collector_c.cpp` | legacy CPython extension; stays until Step 6 |
| Monitor base | `monitor_c/monitor_base.{h,cpp}` | `MonitorBase` with `run()`, `set_packet_handler()` |
| Qualcomm desktop monitor | `monitor_c/qualcomm_desktop_monitor.{h,cpp}` | `setup()` fully implemented: `disable_log_all` → `enable_log`, each command awaiting its DIAG response frame (`await_response()` — the modem drops back-to-back commands). **Validated live on hardware 2026-07-08**: 68 NR RRC/NAS packets over `/dev/ttyUSB0`, SIB1 byte-identical to the Android reference capture |
| Monitor build | `monitor_c/Makefile` | links `libbytes_channel.a + libqualcomm_proto.a` |
| Android Qc monitor | `monitor_c/android_qc_monitor.{h,cpp}` | `setup()`: Diag.cfg generation → mkfifo → spawn diag_revealer (`su -c`) → open FIFO. **Mock-validated end-to-end 2026-07-09** (14404 pkts, identical to OfflineReplayer baseline); on-device run pending |

### 1.2 Remaining in Step 2

| Task | Detail |
|---|---|
| ✅ `decoder/field_value.h` | New header: `FieldEntry`, `FieldList`, `FieldValue` variant |
| ✅ Rewrite `_decode_by_fmt` in `log_packet_helper.h` | Signature `PyObject* result` → `FieldList& result`; all `Py_BuildValue`/`PyList_Append` replaced |
| ✅ Rewrite `_decode_*` helpers in `log_packet.cpp` + all per-type headers | Done (Phases B+C, 2026-07-08); compiles clean, smoke-tested |
| ✅ Rewrite `decode_log_packet`, `decode_custom_packet`, `decode_log_packet_modem` | Return `FieldList` instead of `PyObject*` |
| ✅ `QualcommDecoder::receive_log_packet` | Frame extraction loop (HDLC unwrap, classify, call C++ decode, populate `DecodedPacket`) |
| ✅ `QualcommDecoder::reset` | Calls `reset_binary()` from `hdlc.h` |
| ✅ Remove `PyObject*` from `qualcomm_decoder.{h,cpp}` | `Python.h` + `get_next_packet` deleted; `~QualcommDecoder` closes the export file |
| ✅ Fix `qualcomm_decoder.cpp` includes | `"../../bytes_proto/qualcomm/"` + `"../../export_manager/"` paths; decoder Makefile added (`libqualcomm_decoder.a`) |

**Step 2 is complete** (2026-07-08): all five components build clean and link
together; end-to-end smoke test (HDLC wire bytes → `DecodedPacket` tree +
`.mi2log` export) passes. See the decoder plan §0 for verification detail.

### 1.3 Known bugs found during testing (not yet fixed)

- **`Modem_debug_message` decode crashes**: `_search_result_int(result,
  "Version")` in `_decode_modem_debug_msg` (`log_packet.cpp` ~line 9457) hits
  `assert(item != NULL)` in `log_packet_helper.h:95` for at least some debug
  packets in `examples/offline_log_example.mi2log` (crashes at packet
  ~8748/8770 replaying that file — the packets immediately before are also
  `Modem_debug_message` and decode fine, so it's data-dependent, not
  universal). Not a Step 3 regression — same decoder code path either monitor
  uses; confirmed by checking the two other test captures contain **zero**
  `Modem_debug_message` frames, so this is simply the first file that ever
  exercised this branch. `log_packet_helper.h` has ~10 other bare `assert()`
  call sites following the identical pattern discovered earlier for odd
  `UINT` widths (see decoder plan): likely compiled out via `-DNDEBUG` in the
  official Python extension builds, live/fatal here. Needs investigation
  before Step 3's `replay_mi2log` can be trusted on arbitrary captures
  containing debug messages.

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
| `FileSource` | `bytes_channel/file_source.{h,cpp}` | plain file `read()` | ✅ done |
| `DiagFifoSource` | `bytes_channel/diag_fifo_source.{h,cpp}` | FIFO chronicle-unwrap; `diag_revealer` spawned by the monitor | ✅ done (Step 4a) |
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

**Step 2 — Decoder + MonitorBase + QualcommDesktopMonitor** ✅ complete 2026-07-08
- `decoder/decoder.h` — `Decoder` ABC + `DecodedPacket` + `PacketType` ✅
- `monitor_c/monitor_base.{h,cpp}` — `run()`, `set_packet_handler()` ✅
- `monitor_c/qualcomm_desktop_monitor.{h,cpp}` — `setup()` with DIAG commands ✅
- `decoder/qualcomm_decoder/` — fully PyObject-free, `libqualcomm_decoder.a` ✅
- `QualcommDecoder::receive_log_packet` + `reset` implemented; `get_next_packet` deleted ✅
- Full-stack link + end-to-end smoke test passed ✅

**Step 3 — OfflineReplayer + FileSource** ✅ complete 2026-07-09
- `bytes_channel/file_source.{h,cpp}` — plain `fopen`/`fread`; 0 at EOF (no
  offline-specific handling needed in `MonitorBase::run()`) ✅
- `monitor_c/offline_replayer.{h,cpp}` — `setup()` just opens the file, no
  DIAG handshake ✅
- `examples/replay_mi2log.cpp` now uses the real `FileSource`/`OfflineReplayer`
  (the old `FileMockSerialPort` subclass-of-`SerialPort` hack is gone) ✅
- Regression-tested against 3 captures: `lp_EA0_Jun30_DeregCheck.mi2log` (78
  pkts) and `test_log_dl_retx.mi2log` (14404 pkts) match prior counts exactly.
  `offline_log_example.mi2log` hit a **pre-existing decoder bug**, unrelated
  to this step (see §1.2 below) — first file in this project to contain
  `Modem_debug_message` frames, which no prior test exercised.
- Directory-of-files replay (Python's `OfflineReplayer` accepts a dir and
  iterates `.mi2log`/`.qmdl` inside, `reset()` between files) intentionally
  **not** ported — out of scope per this table's "plain file `read()`"
  description; `MonitorBase::run()` is non-virtual so multi-file iteration
  with per-file decoder reset would need base-class changes. Revisit if
  needed.

**Step 4 — AndroidQcMonitor + DiagFifoSource** ← 4a + 4b + local 4c done (2026-07-09); on-device validation pending

Replaces `mobile_insight/monitor/android_dev_diag_monitor.py`. Requires a
rooted Android device (`/dev/diag` is root-only); the `diag_revealer` helper
we spawn is the one at `mobileinsight-mobile_withwireshark/diag_revealer/qcom/`
— **we own and may modify that source**, so the FIFO wire format below is a
private protocol between it and `DiagFifoSource` (change one, change both).

*4a. `bytes_channel/diag_fifo_source.{h,cpp}`* ✅ done + unit-tested
- `DiagFifoSource : ByteChannel`. `diag_revealer` reads `/dev/diag` and relays
  each message over a FIFO wrapped in a "chronicle" header. Wire format (from
  `diag_revealer.c`'s write loop, issued as 4 separate `write()` syscalls per
  LOG record): `[type:int16 LE][len:int16 LE]` then, for TYPE_LOG(=1),
  `[ts:double LE][payload: len-8]`; for TYPE_START/END_LOG_FILE(=2/3),
  `[filename: len]`.
- `read()` runs a byte-wise state machine unwrapping that framing and hands
  back **only** payload bytes, queued via `pending_` so a >64B payload spans
  several `MonitorBase::run()` reads (its buffer is 64B). Never returns a
  spurious 0 on a partial chronicle field (only on true FIFO EOF) — a naive
  1-read-return-what-you-got would look like EOF and kill a live capture,
  because those 4 writes cross pipe-buffer boundaries.
- **No decoder bypass needed.** Verified the FIFO `payload` bytes are genuine
  HDLC frames (0x7e-delimited, CRC16): `diag_revealer.c` writes the *same*
  `(buf_read+offset+4, msg_len)` region to both the FIFO (line 1198) and its
  on-disk `.mi2log` (line 1211, `fwrite`), and files from that on-disk path
  already decode correctly through our HDLC-based `QualcommDecoder`. So
  `DiagFifoSource` → `decoder_->feed()` is identical to the `SerialPort` path;
  `QualcommDecoder`/`MonitorBase`/`Decoder` ABC need zero changes. (The
  earlier idea of a decoder-level "already-framed, skip HDLC" bypass is
  dropped — unnecessary. If HDLC ever moves into `bytes_channel`, revisit.)
- Unit test (scratchpad `test_diag_fifo.cpp`): real fifo + mock writer using
  the exact 4-write-per-record framing with inter-write delays to force
  fragmentation; a 300B payload + a 3B payload + START/END rotation records;
  reads through a 64B buffer. Verifies byte-exact reconstruction, multi-call
  drain actually exercised (6 data reads for 303B), clean EOF, ts capture.

*4b. `monitor_c/android_qc_monitor.{h,cpp}`* ✅ done (2026-07-09)
- `AndroidQcMonitor : MonitorBase`, source = `DiagFifoSource`, decoder =
  `QualcommDecoder`. Ctor takes a `DiagRevealerParams` struct with the
  process-lifecycle params that are the monitor's concern (not the
  channel's): `exe_path`, `fifo_path`, `diag_cfg_path`, `log_dir`,
  `log_cut_size` (0.5 MB default, Python's "DO NOT CHANGE" value), plus two
  testability knobs: `shell_path` (default `/system/bin/sh` = ANDROID_SHELL)
  and `use_su` (default true; false spawns directly, for desktop mock runs
  without root). Mirrors how `send_command`/`await_response` live on
  `QualcommDesktopMonitor`, not `SerialPort`.
- `setup()` (ordering mirrors Python `run()`):
  1. Generate `Diag.cfg` from `config_.type_names` — ported
     `dm_collector_c_generate_diag_cfg()`: 12× `DIAG_BEGIN_*`,
     `DISABLE_DEBUG` + `DISABLE` (that order), `DEBUG_WCDMA_L1` iff
     `Modem_debug_message` requested, `SET_MASK` batches via
     `sort_type_ids` + `encode_log_config`, `DIAG_END_6000`; each message
     HDLC-encoded and `fwrite`n (same on-disk format as
     `send_msg_to_pyobj`). Python-parity fallbacks: `type_names[0]=="all"`
     expands to all public types; empty `type_names` reuses an existing
     Diag.cfg on disk, else fails.
  2. `make_fifo()`: unlink-if-exists then `mkfifo(0666)`; EEXIST tolerated;
     `su -c mknod <path> p` fallback on EPERM.
  3. `spawn_diag_revealer()`: dir prep (`chmod`/`mkdir`/`chmod` via shell,
     failures tolerated, exactly like Python), then fork/exec
     `sh -c "su -c <exe> <Diag.cfg> <fifo> <outdir> <cutsize>"` — the exact
     Python `Popen("su -c " + cmd, shell=True)` form, unquoted args and all.
     Stores the wrapper-shell pid (`revealer_pid()`; it is `su`'s pid, not
     diag_revealer's — same limitation Python has).
  4. `source_->open()` on the FIFO (blocks until diag_revealer opens the
     write end).
  Then `MonitorBase::run()`'s existing read/feed/decode loop just works.
- Compiles clean with `-Wall -Wextra`; added to `monitor_c/Makefile` and
  linked into `examples/`.
- **Deliberately deferred** (not core to "decode live Android diag"):
  - *Liveness watchdog / auto-restart.* Python's `DiagRevealerDaemon` polls
    `ps | grep diag_revealer` every 5s because it loses the child PID (spawns
    via `su -c "..."`, so the tracked proc is `su`). In C++ a dead
    diag_revealer surfaces as FIFO EOF → `read()` returns 0 →
    `MonitorBase::run()` stops cleanly (detection is free). Auto-restart is a
    resilience follow-up.
  - *`new_diag_log` file-rotation events.* `DiagFifoSource` already parses
    START/END_LOG_FILE records but discards them; surfacing them (Python emits
    a `new_diag_log` event) needs a callback hook — not needed for decoding.
  - *diag_revealer teardown* (kill on exit): mirror Python's `_stop_collection`
    later; `~AndroidQcMonitor` should at least close the FIFO.

*4c. Testing* — local mock end-to-end ✅ passed (2026-07-09):
- `examples/android_qc_capture.cpp` (built by `examples/Makefile`) — driver
  analogous to `serialtest.cpp`: whitelists all types (comparable to
  `replay_mi2log`), dumps every packet, `--no-su` switches to `/bin/sh` +
  direct spawn for desktop testing.
- Mock (scratchpad `mock_diag_revealer.py`): same argv contract as the real
  diag_revealer; streams `test_log_dl_retx.mi2log` into the fifo with the
  exact 4-writes-per-record chronicle framing, bracketed by START/END
  rotation records; asserts Diag.cfg exists+non-empty before streaming.
- Result: **14404 packets, count and dump content identical** to the
  `OfflineReplayer` baseline on the same file (diff clean after stripping
  recv timestamps) at ~3370 pkt/s. So Diag.cfg generation, mkfifo, spawn,
  blocking FIFO open, chronicle unwrap, decode, and clean EOF stop are all
  exercised in one run.
- Diag.cfg sanity: 21 HDLC frames for the all-types list = 12 headers + 2
  disables + 1 `DEBUG_WCDMA_L1` + 5 `SET_MASK` batches + 1 end; first frame
  `1d 1c 3b 7e` as expected.
- **Remaining, on-device only:** `su -c` spawn with the real diag_revealer +
  `/dev/diag` on the rooted phone; byte-exact Diag.cfg vs a
  Python-`generate_diag_cfg` reference (the compiled `dm_collector_c`
  extension only exists on the VM/phone).

*4d. Android cross-compilation* ✅ wired (2026-07-09) — `android.mk` at the
repo root, included by all 7 Makefiles (each right after its CXX/CXXFLAGS
defaults; `ar rcs` became `$(AR) rcs`; examples' link lines gained
`$(LDFLAGS)`).
- `make -C examples TARGET=android [NDK=/path/to/ndk] [API=21]` — TARGET/NDK
  propagate to the recursive `deps` sub-makes via MAKEFLAGS (verified, all 7
  activate). NDK lookup: explicit `NDK=` > `$ANDROID_NDK_HOME` >
  `$ANDROID_NDK_ROOT`; clear `$(error)` if absent or the clang wrapper is
  missing.
- Targets arm64-v8a / android-21 via the NDK's target-prefixed
  `aarch64-linux-android21-clang++` (r19+ layout; the VM has r19b at
  `~/android-ndk-r19b` with `ANDROID_NDK_HOME` set). `-static-libstdc++`
  gives dependency-free binaries. AR resolves to llvm-ar > r19's GNU
  binutils ar > host ar.
- **Gotcha:** host and Android builds share object/lib/binary file names —
  always `make -C examples clean-all` when switching targets, or the link
  mixes architectures (fails loudly, but confusingly).
- Not testable on this dev box (no NDK): verified via fake-NDK dry-run
  (correct compiler/ar/ldflags in every dir) + untouched host rebuild.

**First real cross-build on the VM (2026-07-09), bugs found + fixed:**
- Two pre-existing files used `errno`/`EINTR` without `#include <cerrno>`
  (`ws_dissector_client/ws_dissector_client.cpp`,
  `bytes_channel/serial_port.cpp`) — silently worked on host because glibc's
  headers transitively pull in `<cerrno>`; bionic's don't. Fixed both
  (scanned the rest of the tree for the same pattern — clean).
- Link failure: `undefined reference to __android_log_print` from
  `log_packet.o`. Root cause: `log_packet_helper.h:23-26` redefines
  `printf()` to `__android_log_print()` whenever `__ANDROID__` is defined
  (legacy code, predates this migration) — true for every NDK clang
  invocation but never true on the host build, so this path had never been
  exercised. `__android_log_print` lives in `liblog.so`, not libc. Fixed by
  adding `LDFLAGS += -llog` to `android.mk`'s android branch.
- Both fixes are host-invisible (guarded by `TARGET=android` or genuinely
  missing includes that host headers papered over) — host rebuild
  reconfirmed clean after each.

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
