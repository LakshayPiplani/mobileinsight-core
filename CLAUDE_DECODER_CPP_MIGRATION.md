# Decoder C++ Migration Plan

Scope: remove all `PyObject*` from the decoder layer and replace the Python
result-list tree with a pure C++ `FieldList` tree. The decoder produces a
fully decoded `DecodedPacket` that is consumed entirely in C++. There is no
Python boundary in the decoder; the ws_dissector port is a separate deferred
step.

Parent plan: [CLAUDE_CPP_MIGRATION.md](CLAUDE_CPP_MIGRATION.md)

---

## 0. Implementation status (updated as tasks complete)

Survey of the tree on 2026-07-08 found two deviations from the plan text:

1. **`bytes_proto/qualcomm/utils.{h,cpp}` do not exist.** The Makefile and
   `consts.h` reference them, so `libqualcomm_proto.a` has never built.
   `IdVector`/`ValueName`/`find_ids`/`search_name` currently sit in
   `log_config.{h,cpp}`, and `map_typenames_to_ids` has an illegal `break`
   outside its loop. Fixing this is a prerequisite (Task 0).
2. **~60 per-type headers in `decoder/qualcomm_decoder/` also contain
   `PyObject*` decode code** (not just `log_packet.cpp`). They are `#include`d
   by `log_packet.cpp`, so Phases B/C cover them too.

Refactor landed 2026-07-08 (pre-Phase-C cleanup, all builds verified):

- **`export_manager/` is now an independent top-level module**
  (`libexport_manager.a`), out of `decoder/qualcomm_decoder/`. New signature:
  `manager_export_binary(pstate, int type_id, b, length)` — the decoder
  classifies each frame once and passes the `type_id`; the export manager no
  longer re-parses frames. Depends only on `bytes_proto/qualcomm`
  (`utils.h`, `hdlc.h`).
- **`is_log_packet` / `is_debug_packet` / `is_custom_packet` moved** from
  `log_packet.{h,cpp}` to `bytes_proto/qualcomm/consts.{h,cpp}` (shared
  protocol classification).
- **`hdlc.cpp` added to `libqualcomm_proto.a`** — its dead `#include
  <Python.h>` removed; the proto lib is now fully Python-free and provides
  `encode_hdlc_frame` to monitor + export_manager.
- **monitor_c compiles clean** (`-Wall -Wextra`): `enable_log_all()` decl
  fixed to `bool`; sign-compare loop fixed; `ByteChannel::write` params
  unnamed to silence `-Wunused-parameter`.

| # | Task | Status |
|---|---|---|
| 0 | Create `bytes_proto/qualcomm/utils.{h,cpp}`; slim `log_config.{h,cpp}`; fix `map_typenames_to_ids`; build `libqualcomm_proto.a` | ✅ done — lib builds clean (`-std=c++17 -Wall`) |
| A1 | Create `decoder/field_value.h` | ✅ done — compile+run smoke test passed |
| A2 | Update `decoder/decoder.h` — `DecodedPacket` gets `fields` + `ok` | ✅ done — `frame`/`skip_decoding` removed |
| A3 | Rewrite `log_packet_helper.h` helpers to `FieldList` | ✅ done — all helpers ported; `log_packet.h` top-level decls also switched to `FieldList` (needed for the header to parse); smoke test decodes `LogPacketHeaderFmt`, maps `type_id`→name, formats QCDM timestamp correctly |
| B | Port NR5G decode code (15 types, incl. per-type `nr_*.h` headers) | ✅ done — all 13 `nr_*.h` headers converted (script + spot fixes) and **compile clean as one TU** (`-std=c++17 -Wall`, no Python includes). `_decode_nr_rrc_ota` + `_nr_rrc_reconf_complete_to_ul_dcch` (now returns `std::vector<uint8_t>`, §6) hand-converted inside `log_packet.cpp`; that file only compiles as a whole, so its verification lands with Phase C. Conversion script: scratchpad `convert_pyobject.py` (paren-aware §5 substitution map). |
| C | Port LTE/WCDMA/GSM/CDMA/GNSS decode code (`log_packet.cpp` + remaining headers) | ✅ done 2026-07-08 — all 45 remaining per-type headers converted (script + `fix_carrier_temps.py` for temps orphaned by dead-decl removal) and compile as one TU; `log_packet.cpp` fully converted **including the top-level decode fns** (pulled forward from Phase D): `decode_log_packet` / `decode_custom_packet` / `decode_log_packet_modem` return `FieldList`, sampling gate returns empty list, `_decode_by_fmt_modem` uses `_qcdm_timestamp_to_iso`, `PyList_GetSlice(1,4)` → element move-slice. Compiles clean (`-std=c++17 -Wall`, zero errors; only pre-existing warnings: 2 multi-line comments + 4 `-Wparentheses` precedence bugs at `& 0x30 + x` sites, left as-is deliberately). End-to-end smoke test passed: `decode_log_packet` on synthetic WCDMA_RRC_States (flat + ValueName map) and LTE CMLIFMR v3 (nested list/dict, RSRP/RSRQ doubles). `parser_template.h` (unused reference file) not converted. |
| D | Implement `receive_log_packet`/`reset`; delete `get_next_packet`; drop `Python.h` from `qualcomm_decoder.h`; fix includes; decoder Makefile | ☐ pending — top-level decode fns already done in C |
| V | Full build verification (proto + decoder + monitor_c) | ☐ pending |

---

## 1. Background — What the Python object was

`decode_log_packet` returned a **`PyList`** where every element was a 3-tuple
`(name, value, type_hint)`. `_decode_by_fmt` appended one tuple per `Fmt`
entry. The type_hint determined how consumers interpreted the value:

| type_hint | value type | meaning |
|---|---|---|
| `""` | scalar (int, float, str, datetime) | ordinary leaf field |
| `"list"` | nested PyList | array of homogeneous records |
| `"dict"` | nested PyList | one record's mixed fields |
| `"raw_msg/PROTO"` | PyBytes | opaque bytes; PROTO names the dissector |

The top-level result for `get_next_packet(include_timestamp=True)` was an
additional outer tuple `(result_list, posix_double)`.

---

## 2. Why nested lists exist — and why Fmt[] is not the cause

**`Fmt[]` arrays are always flat.** Every entry decodes exactly one scalar
field and consumes N bytes. `FmtType` has no "array-of-records" kind.

Nesting is created entirely in the `_decode_*` C functions by a fixed pattern:

1. A **header `Fmt[]`** contains a count field (e.g. `"Number of Neighbor Cells"`)
   decoded as a plain `UINT`.
2. C code reads that count back with `_search_result_int`.
3. C code **loops N times**, each iteration running `_decode_by_fmt` on a
   fresh list using a **record `Fmt[]`**.
4. Each inner list is wrapped as `("name", inner_list, "dict")` and pushed
   into an outer list.
5. The outer list is wrapped as `("name", outer_list, "list")` and appended to
   the top-level result.

Nesting depth is bounded at **3 levels** (worst case: subpackets → frequencies
→ cells in LTE PHY IRAT). It is not infinite recursion.

### Concrete Fmt[] pairs that cause nesting

**1-level: LTE intra-freq neighbor cells**
```
LtePhyCmlifmrFmt_v3_Header[]     — log_packet.h:904  — contains "Number of Neighbor Cells" (UINT)
LtePhyCmlifmrFmt_v3_Neighbor_Cell[] — log_packet.h:931 — flat record, one cell
```
Loop at log_packet.cpp:1685.

**2-level: LTE PHY subpackets**
```
LtePhySubpktFmt[]                 — log_packet.h:965  — contains "Number of SubPackets" (UINT)
LtePhySubpktFmt_v1_SubpktHeader[] — log_packet.h:971  — per-subpacket header
```
Inner subpacket body decoded per-ID inside the loop.

**3-level: LTE PHY IRAT WCDMA (deepest)**
```
LtePhyIratSubPktFmt[]             — log_packet.h:1294 — subpacket header
LtePhyIratWCDMAFmt[]              — log_packet.h:1300 — contains "Number of frequencies" (UINT)
LtePhyIratWCDMACellMetaFmt[]      — log_packet.h:1306 — contains "Number of cells" (UINT)
LtePhyIratWCDMACellFmt[]          — log_packet.h:1312 — flat leaf, one cell
```
Three nested loops at log_packet.cpp:3517–3563.

---

## 3. C++ type design

### 3.1 Compiler requirement

All Makefiles have been updated to **`-std=c++17`** (was `-std=c++14`).
C++17 is a superset of C++14; no existing code breaks. The Vagrant VM uses
Ubuntu 20.04 with GCC 9.3, which has full C++17 support.

The only C++17 feature we use that C++14 lacks is `std::variant`.

### 3.2 `decoder/field_value.h` (new file)

Lives in `decoder/` — not in `decoder/qualcomm_decoder/` — because
`FieldList` is the shared currency of the decoder layer. Any future
`MtkDecoder` produces the same tree; it is not a Qualcomm concept.

```cpp
#pragma once
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

struct FieldEntry;
using FieldList = std::vector<FieldEntry>;

struct FieldValue {
    using V = std::variant<
        int64_t,               // all UINT fields (1/2/4/8 bytes, signed-extended)
        double,                // RSRP, RSRQ, BANDWIDTH, computed floats
        std::string,           // BYTE_STREAM hex, PLMN string, datetime as ISO-8601
        std::vector<uint8_t>,  // raw_msg bytes (opaque dissector payload)
        FieldList              // nested list or dict (recursive via std::variant)
    >;
    V data;
};

struct FieldEntry {
    std::string name;
    FieldValue  value;
    std::string type_hint; // "", "list", "dict", or "raw_msg/PROTO"
};
```

**Why `std::string` over `char*`:** `char*` would require manual `new[]` /
`delete[]` at every field site (277+ `PyList_New` call sites in `log_packet.cpp`
alone). `std::string` is RAII, copies correctly, and can hold arbitrary binary
data via `std::string(ptr, len)`. The existing code already uses `std::string`
internally (e.g. `type_str` in `_decode_nr_rrc_ota`).

**Why `std::vector<uint8_t>` for raw_msg bytes:** distinguishes binary payloads
from text strings unambiguously, so `std::visit` dispatch is unambiguous and the
future dissector port can pattern-match on type alone.

**Why `std::variant` is sound for recursion:** `std::variant` stores its
alternatives inline (no heap allocation for scalar/string/bytes alternatives).
The `FieldList` alternative is `std::vector<FieldEntry>`, which stores its
elements on the heap — so the variant itself has a fixed size (pointer + size +
capacity) and the recursion terminates through the heap pointer. GCC 9 compiles
this without issues.

### 3.3 Updated `DecodedPacket` in `decoder/decoder.h`

```cpp
enum PacketType { LOG_PACKET, DEBUG_PACKET, CUSTOM_PACKET };

struct DecodedPacket {
    PacketType type;
    double     timestamp; // POSIX time; -1.0 if not requested
    FieldList  fields;    // fully decoded tree; empty if ok == false
    bool       ok = false;
};
```

`frame` (raw bytes) and `skip_decoding` are removed. `ok` replaces the
"`Py_RETURN_NONE` means no packet" convention.

---

## 4. What stays completely unchanged

- All `Fmt[]` arrays in `decoder/qualcomm_decoder/log_packet.h`
- `FmtType` enum
- All `ValueName[]` arrays
- `LogPacketType` enum and `LogPacketTypeID_To_Name[]` in `bytes_proto/qualcomm/consts.h`
- The HDLC layer (`bytes_proto/qualcomm/hdlc.{h,cpp}`)
- The export manager (`export_manager/export_manager.{h,cpp}` — independent
  module; `manager_export_binary` now takes the caller-supplied `type_id`)
- The frame-extraction loop logic inside `receive_log_packet`

---

## 5. What changes — the substitution map

Every occurrence of the following patterns in `log_packet_helper.h` and
`log_packet.cpp` is mechanically replaced:

| Old (PyObject*) | New (FieldList) |
|---|---|
| `PyObject *result = PyList_New(0)` | `FieldList result` |
| `PyObject *sublist = PyList_New(0)` | `FieldList sublist` |
| `Py_BuildValue("I", ii)` → integer | `FieldValue{int64_t(ii)}` |
| `Py_BuildValue("K", iiii)` → uint64 | `FieldValue{int64_t(iiii)}` |
| `Py_BuildValue("f", f)` → float | `FieldValue{double(f)}` |
| `Py_BuildValue("s", str)` → string | `FieldValue{std::string(str)}` |
| `Py_BuildValue("sy#s", name, b, n, hint)` → raw bytes entry | `FieldList push with vector<uint8_t>(b, b+n)` |
| `PyList_Append(result, Py_BuildValue("(sOs)", name, val, hint))` | `result.push_back({name, val, hint})` |
| `_search_result_int(PyObject* result, const char* name)` | `_search_result_int(const FieldList& result, const char* name)` |
| `_replace_result(PyObject* result, name, PyObject* val)` | `_replace_result(FieldList& result, name, FieldValue val)` |
| `_map_result_field_to_name(PyObject* result, ...)` | `_map_result_field_to_name(FieldList& result, ...)` |
| `Py_DECREF(...)` | _(deleted — no reference counting)_ |
| `PyDateTime_IMPORT` / `PyDateTime_FromDateAndTime` | `std::string` ISO-8601 (see §6) |
| Function signature `PyObject* result` | `FieldList& result` |
| `decode_log_packet(…)` returns `PyObject*` | returns `FieldList` |

---

## 6. Special cases

### QCDM_TIMESTAMP

The original code computed a `PyDateTime` object by adding a delta to
`1980-01-06 00:00:00`. In C++ there is no `datetime` module. Represent it as
an ISO-8601 `std::string`, e.g. `"1980-01-06T00:04:23.125000"`. The formula
is identical (epoch 1980-01-06, divide raw ticks by 52428800.0). Use `<ctime>`
or `<chrono>` to format.

### raw_msg fields

`Py_BuildValue("sy#s", "Msg", b + offset, pdu_length, "raw_msg/LTE-RRC_DL_DCCH")`
becomes:
```cpp
result.push_back({
    "Msg",
    FieldValue{std::vector<uint8_t>(b + offset, b + offset + pdu_length)},
    "raw_msg/LTE-RRC_DL_DCCH"
});
```
The ws_dissector port is deferred. Until then the bytes sit in `FieldValue` as
`std::vector<uint8_t>` and the `type_hint` string carries the protocol name.

### `_nr_rrc_reconf_complete_to_ul_dcch`

Currently returns `char*` (heap-allocated, must be `delete[]`'d by caller).
Change to return `std::vector<uint8_t>` — same byte-manipulation logic, no
manual deallocation.

### `decode_log_packet` sampling-rate gate

The current code returns `Py_None` when the sampling gate drops a packet.
In C++ this becomes returning an empty `FieldList` with `DecodedPacket::ok =
false` from the caller. The gate logic itself (`prev` clock, `target_sampling_rate`)
is unchanged.

### `PLACEHOLDER` fields

`PLACEHOLDER` appends a zero-valued `int64_t` field with the given name and
`type_hint = ""`. Post-decode functions like `_decode_lte_phy_pdsch_*` then
call `_replace_result` to overwrite those placeholders with computed values.
This pattern works identically with `FieldList`.

---

## 7. Migration phases

### Phase A — Infrastructure (prerequisite for all packet types)

Files touched: `decoder/field_value.h` (new), `decoder/decoder.h`,
`decoder/qualcomm_decoder/log_packet_helper.h`

**A1.** Create `decoder/field_value.h` (§3.2).

**A2.** Update `decoder/decoder.h`: replace `frame` + `skip_decoding` in
`DecodedPacket` with `fields` + `ok` (§3.3).

**A3.** Rewrite the five core helpers in `log_packet_helper.h`:

| Helper | Change |
|---|---|
| `_decode_by_fmt` | `PyObject* result` → `FieldList& result`; all `Py_BuildValue`+`PyList_Append` replaced with `result.push_back(…)` |
| `_decode_by_fmt_modem` | same |
| `_search_result_int` | `PyObject*` → `const FieldList&`; iterate entries by name |
| `_search_result_uint` | same |
| `_replace_result` / `_replace_result_int` | `PyObject*` → `FieldList&` + `FieldValue` |
| `_map_result_field_to_name` | `PyObject*` → `FieldList&` |

Phase A compiles independently (no packet-type decode functions needed yet).

### Phase B — Pilot batch: NR5G packet types (15 types)

Port in order of increasing complexity:

| Step | Types | Why this order |
|---|---|---|
| B1 | `NR_RRC_OTA_Packet` | Simplest decoder: one header fmt + one raw_msg entry. Proves the full path end-to-end. |
| B2 | `NR_NAS_SM5G_*/MM5G_*` | Flat 1-level; same shape as LTE NAS. |
| B3 | `NR_MAC_RACH_Trigger`, `NR_L2_UL_BSR`, `NR_L2_UL_TB` | Flat 1-level. |
| B4 | `NR_MAC_UL_TB_Stats`, `NR_MAC_PDSCH_Stats`, `NR_MAC_UL_Physical_Channel_Schedule_Report` | 2-level subpacket arrays; validates nested FieldList construction. |
| B5 | `NR_ML1_Serving_Cell_Beam_Management`, `NR_ML1_Searcher_Measurement_Database_Update_Ext` | 2-level. |
| B6 | `NR_LL1_FW_Serving_FTL`, `NR_PDCP_UL_Control_Pdu`, `NR_RLC_DL_Stats` | Remaining flat types. |

For each type: change `_decode_<type>` signature from `PyObject*` to
`FieldList&`, replace all `Py_BuildValue`/`PyList_Append`/`Py_DECREF` per the
substitution map (§5). Add the type's `case` branch to `on_demand_decode`.

### Phase C — LTE and WCDMA types (~75 types)

Port in order of nesting complexity:

| Sub-phase | Types | Notes |
|---|---|---|
| C1 | Flat single-version (LTE NAS, LTE RRC basic) | ~20 types |
| C2 | Multi-version flat (LTE MAC, LTE RLC, LTE PDCP) | ~20 types; only version switch matters |
| C3 | 2-level subpacket arrays (LTE PHY serving cell, PUCCH, PUSCH) | ~20 types |
| C4 | 3-level deep (LTE PHY IRAT: subpacket → frequency → cell) | 5 types; hardest |
| C5 | WCDMA types | ~10 types |
| C6 | `decode_log_packet_modem` and `Modem_debug_message` | Uses `_decode_by_fmt_modem` variant |

### Phase D — QualcommDecoder wiring

Once all decode functions return `FieldList`:

1. Rewrite `decode_log_packet`, `decode_custom_packet`,
   `decode_log_packet_modem` to return `FieldList` instead of `PyObject*`.
2. Implement `QualcommDecoder::receive_log_packet` — the HDLC frame loop:
   - `get_next_frame` → classify (`is_log_packet` / `is_debug_packet` /
     `is_custom_packet`)
   - Call the appropriate top-level decode function
   - Populate `out.type`, `out.timestamp`, `out.fields`, `out.ok = true`
   - Return `false` when buffer is empty
3. Implement `QualcommDecoder::reset` — call `reset_binary()`.
4. Delete `get_next_packet` from `qualcomm_decoder.{h,cpp}`.
5. Drop `#include <Python.h>` from `qualcomm_decoder.h`.
6. Fix stale includes in `qualcomm_decoder.cpp`:
   - `"log_config.h"` → `"../../bytes_proto/qualcomm/log_config.h"`
   - `"hdlc.h"` → `"../../bytes_proto/qualcomm/hdlc.h"`
   - `"utils.h"` → `"../../bytes_proto/qualcomm/utils.h"`
   - `"consts.h"` → `"../../bytes_proto/qualcomm/consts.h"`

---

## 8. File layout after migration

```
decoder/
    field_value.h              ← NEW: FieldEntry, FieldList, FieldValue
    decoder.h                  ← UPDATED: DecodedPacket uses FieldList

decoder/qualcomm_decoder/
    log_packet.h               ← UNCHANGED: Fmt[], FmtType, ValueName[]
    log_packet_helper.h        ← REWRITTEN: all helpers use FieldList&
    log_packet.cpp             ← REWRITTEN: all _decode_* use FieldList&;
                                             top-level decode fns return FieldList
    qualcomm_decoder.h         ← UPDATED: drop Python.h + get_next_packet
    qualcomm_decoder.cpp       ← UPDATED: implement receive_log_packet, reset;
                                           fix includes

export_manager/
    export_manager.{h,cpp}     ← MOVED here (independent module, libexport_manager.a);
                                  manager_export_binary takes caller-supplied type_id
```

---

## 9. Deferred work (out of scope for this migration)

- **ws_dissector port:** `raw_msg` bytes in `FieldValue` are currently opaque.
  The dissector port will consume `FieldEntry.type_hint = "raw_msg/PROTO"` and
  replace the bytes entry with a decoded sub-tree. Architecture is already
  compatible — no changes to `FieldList` needed when that happens.
- **Python facade (Step 6 in parent plan):** a thin layer that walks
  `DecodedPacket.fields` and produces a Python dict. Much simpler than the
  current approach since the tree is already fully decoded.
- **Analyzer migration (Step 7 in parent plan):** lowest priority; not
  affected by this decoder work.