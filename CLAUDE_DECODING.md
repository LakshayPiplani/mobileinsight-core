# MobileInsight Decoding Pipeline

Reference document for the C++ migration. Covers both directions of the DIAG protocol:
outbound commands (monitor → modem) and inbound log packets (modem → decoded output).

---

## 1. Type ID Structure

Every Qualcomm log packet type is a 16-bit integer split into two fields:

```
 15      12 11          0
 ┌────────┬─────────────┐
 │equip_id│   item_id   │
 └────────┴─────────────┘
    4 bits    12 bits
```

| equip_id | Radio access technology |
|----------|------------------------|
| `0x1`    | CDMA / 1xEV            |
| `0x4`    | WCDMA                  |
| `0x7`    | UMTS                   |
| `0xB`    | LTE                    |

`get_equip_id(type_id)` and `get_item_id(type_id)` in `log_config.cpp` extract these.

SET_MASK commands are sent **per equip_id bucket** — the modem expects one bitmask
per radio technology, not one global mask. This is why `sort_type_ids()` groups them.

---

## 2. Outbound: Monitor → Modem

```
MonitorConfig::type_names  (e.g. ["LTE_PHY_PDCCH_Decoding_Result", ...])
         │
         ▼ find_ids()                              consts.h
  string → int type IDs        ◄── LogPacketTypeID_To_Name[] (name ↔ int, ~250 entries)
         │
         ├─ Modem_debug_message? ──► encode_log_config(DEBUG_WCDMA_L1, ids)
         │                                    │
         ▼ sort_type_ids()                    │
  sort + deduplicate + bucket by equip_id     │
         │                                    │
         ▼ (per bucket)                       │
  encode_log_config(SET_MASK, bucket)         │
         │                                    │
         └──────────────┬─────────────────────┘
                        ▼ encode_hdlc_frame()     hdlc.cpp
                  CRC16 append (little-endian)
                  byte-stuff (0x7d escaping)
                  append 0x7e delimiter
                        │
                        ▼ source_->write()
                   SerialPort::write()  →  modem USB serial
```

### HDLC encoding detail

Two bytes are special: `0x7e` (frame delimiter) and `0x7d` (escape).
If either appears in the payload or CRC bytes, it is escaped:

```
0x7e  →  0x7d 0x5e      (0x7e XOR 0x20)
0x7d  →  0x7d 0x5d      (0x7d XOR 0x20)
```

Full frame on the wire (before escaping):
```
[ payload bytes ] [ CRC low byte ] [ CRC high byte ] 0x7e
```
CRC16 is CCITT (polynomial 0x11021, bit-reversed table in `hdlc.cpp`).

### encode_log_config ops used

| Op               | Effect                                    |
|------------------|-------------------------------------------|
| `DISABLE`        | Tell modem: stop all logging              |
| `SET_MASK`       | Enable specific item IDs for one equip_id |
| `DEBUG_WCDMA_L1` | Enable WCDMA L1 debug msgs (RSCP)         |

`disable_log_all()` is always called first in `setup()` before `enable_log()`.
This ensures the modem starts from a clean state.

---

## 3. Inbound: Modem → Decoded Packet

```
modem USB serial
        │
        ▼ source_->read()                          serial_port.cpp
   raw bytes (chunk, up to 64B at a time)
        │
        ▼ feed_binary()                            hdlc.cpp
   append to static std::string buffer
        │
        ▼ get_next_frame()                         hdlc.cpp
   find 0x7e delimiter in buffer
   extract bytes up to delimiter, erase from buffer
        │
        ▼ unescape()                               (internal to hdlc.cpp)
   scan for 0x7d → XOR next byte with 0x20
        │
        ▼ CRC check
   strip last 2 bytes (CRC, little-endian)
   calc_crc() on frame → compare
   set crc_correct flag
        │
   crc_correct == false → log dropped frame, skip
        │
        ▼ check_frame_format()                     hdlc.cpp
   strip 8-byte Android prefix if present:
   \x98\x01\x00\x00\x01\x00\x00\x00
        │
        ├── is_custom_packet()?  ──────────────────────────────┐
        │                                                       │
        ├── manager_export_binary()  ← ExportManagerState      │
        │   check whitelist → if match: re-HDLC-encode         │
        │   and fwrite() to .mi2log                            │
        │                                                       │
        ├── is_log_packet()?  ──────────────────────────────┐  │
        │                                                    │  │
        └── is_debug_packet()? → prepend 14-byte header  ─┐ │  │
                                                           │ │  │
              ┌────────────────────────────────────────────┘ │  │
              ▼                                              │  │
   decode_log_packet_modem()        decode_log_packet()  ◄──┘  │
              │                            │                    │
              └────────────┬───────────────┘   decode_custom_packet() ◄──┘
                           │
                           ▼
                   PyObject* (Python list of (field_name, value) tuples)
```

### Packet type identification

| Predicate          | First bytes          | Decode function          |
|--------------------|----------------------|--------------------------|
| `is_custom_packet` | `0x98 0x01 ...`      | `decode_custom_packet()` |
| `is_log_packet`    | `0x10 0x00 ...`      | `decode_log_packet()`    |
| `is_debug_packet`  | `0x79 ...`           | prepend 14B header → `decode_log_packet_modem()` |

Unknown packet types are silently skipped (`continue` in the loop).

### debug_packet header

Debug packets lack a standard DIAG header. `get_next_packet()` prepends a synthetic
14-byte header before calling `decode_log_packet_modem()` so the same decode path works:

```
bytes 0-1:   0xFF 0xFF          (flags)
bytes 2-3:   total length (little-endian)
bytes 4-5:   0xEB 0x1F          (type_id = Modem_debug_message = 0x1FEB)
bytes 6-13:  fixed pattern      (timestamp placeholder)
bytes 14+:   original frame bytes
```

---

## 4. decode_log_packet internals

```
raw frame bytes (after HDLC decode, type 0x10 0x00)
        │
        ▼ _decode_by_fmt(LogPacketHeaderFmt, ...)
  skip 2B | log_msg_len 2B | type_id 2B | QCDM timestamp 8B
        │
        ▼ _map_result_field_to_name()
  type_id int → LogPacketType enum (e.g. LTE_PHY_PDCCH_Decoding_Result)
        │
        ▼ on_demand_decode(b + offset, ...)
  switch (type_id):
    case LTE_PHY_PDCCH_Decoding_Result:
        _decode_by_fmt(LtePHYPDCCHDecodingResultFmt, ...)
        _decode_lte_phy_pdcch_payload(...)
    case WCDMA_RRC_States:
        _decode_by_fmt(WcdmaRrcStatesFmt, ...)
    ... (~250 cases in log_packet.cpp)
        │
        ▼
  Python list: [(field_name, value), ...]
```

### Fmt struct

Every packet type has one or more `Fmt[]` arrays in `log_packet.h` / per-type headers.
Each entry describes one field:

```cpp
struct Fmt {
    FmtType    type;        // UINT, BYTE_STREAM, BIT_STREAM, QCDM_TIMESTAMP, SKIP, ...
    const char *field_name;
    int        len;         // bytes (or bits for BIT_STREAM)
};
```

`_decode_by_fmt()` walks the array, reads `len` bytes per entry, converts according to
`type`, and appends `(field_name, value)` to the result list.

### QCDM timestamp

The 8-byte QCDM timestamp in every log packet header is Qualcomm-specific.
It counts 1/32768-second ticks since Jan 6, 1980 (GPS epoch).
`log_packet_helper.h` converts it to a Python datetime / posix float.

---

## 5. Export Manager

```
DMCollector::configure(config)
        │
        ▼ find_ids()  (type_names → IdVector)
  manager_change_config(&emanager_, log_output_path, type_ids)
        │
        ├── close old .mi2log if path changed
        └── fopen new .mi2log in binary write mode
            whitelist = set<int>(type_ids)

manager_export_binary(&emanager_, frame, length)    called per frame
        │
        ▼ get_log_type()
  read type_id from frame bytes
        │
  whitelist.count(type_id) > 0?
        ├── yes: encode_hdlc_frame() → fwrite() to .mi2log  →  return true
        └── no:  return false   (frame skipped by get_next_packet)
```

`manager_export_binary` returns `false` for frames not in the whitelist.
`get_next_packet` uses this as a **filter gate**: if export returns false, the frame
is not decoded and not dispatched to the callback. Only whitelisted types flow through.

---

## 6. Question: does the new DMCollector need log_config?

Short answer: **no**.

| Module          | Uses log_config for                              |
|-----------------|--------------------------------------------------|
| Monitor         | `encode_log_config()` → build DIAG wire commands |
| DMCollector     | nothing — export filter uses `IdVector` directly |

`DMCollector::configure()` calls `manager_change_config()` which takes an `IdVector`.
It does not call `encode_log_config()` at all. The modem-command side lives entirely
in the monitor's `setup()` (`disable_log_all` / `enable_log`).

**What is shared between monitor and decoder?**

`MonitorConfig` is the shared struct:

```
MonitorConfig::type_names       →  monitor: build SET_MASK commands for modem
                                →  decoder: build whitelist for .mi2log export

MonitorConfig::log_output_path  →  decoder only: path for .mi2log file

MonitorConfig::port_path        →  monitor only: serial port device
MonitorConfig::baud_rate        →  monitor only: serial port speed
MonitorConfig::skip_decoding    →  decoder only: sampling / decode skipping
```

No additional shared config file is needed. `MonitorConfig` already captures everything
both sides need. The decoder's `configure(config)` extracts only what it needs
(`type_names`, `log_output_path`). The monitor's `setup()` extracts only what it needs
(`type_names`, and source_ was constructed from `port_path`/`baud_rate` at factory time).

---

## 7. Files at a glance

| File                    | Responsibility                                          |
|-------------------------|---------------------------------------------------------|
| `hdlc.cpp`              | HDLC encode/decode, CRC16, `feed_binary`, `get_next_frame` |
| `log_config.cpp`        | DIAG command encoding (`encode_log_config`), equip_id math |
| `log_packet.cpp`        | Per-type decode logic, 12k lines, ~250 packet types     |
| `log_packet.h`          | `Fmt[]` arrays for all packet types                     |
| `export_manager.cpp`    | `.mi2log` file write, whitelist filter                  |
| `consts.h`              | `LogPacketTypeID_To_Name[]`, `LogPacketType` enum       |
| `utils.h`               | `find_ids`, `IdVector`, `ARRAY_SIZE`, `ValueName`       |
| `dm_collector.cpp`      | Thin orchestrator: `feed`, `configure`, `get_next_packet` |