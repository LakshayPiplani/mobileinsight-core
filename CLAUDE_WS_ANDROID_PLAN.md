# Get `ws_dissector` Running On-Device (Android)

Parent plan: [CLAUDE_CPP_MIGRATION.md](CLAUDE_CPP_MIGRATION.md)
Related: [CLAUDE_DECODER_CPP_MIGRATION.md](CLAUDE_DECODER_CPP_MIGRATION.md) §9 (ws_dissector port, deferred work)
Follow-on: [CLAUDE_ANDROID_WS_BUILD.md](CLAUDE_ANDROID_WS_BUILD.md) — rebuilding `ws_dissector` for Android
with the EA0/`null_decipher` fix, a limitation found during this plan's live validation.

## Status (updated as tasks complete)

- **Step 1** ✅ done — `ws_dissector/fetch_android_prebuilt.sh` pins commit
  `eda41f72b56341e3ab44c3c5847eb9661cb2644f` of `mobileinsight-libs` (`ws-3.4`),
  fetches the 8 `.so` + `android_pie_ws_dissector` into
  `ws_dissector/android_prebuilt/` (gitignored), with `file`/`readelf -d`
  self-check. Documented in `ws_dissector/README_android_prebuilt.md` (kept
  outside the gitignored dir so it's actually tracked).
- **Step 2** ✅ **PASSED on real hardware** (2026-07-14) — `examples/android_ws_dissector_probe.cpp`
  built for arm64 (`TARGET=android`), pushed alongside the arm32 prebuilt
  artifacts, run standalone on the phone: `WsDissector::start()` +
  `.decode("nas-5gs", {0x7e,0x00})` returned real PDML (aww envelope
  correctly framed `aww.proto=416`/`aww.data_len=2`), sentinel recognized,
  probe printed `PASS`. Confirms `LD_LIBRARY_PATH` resolution, `epan_init()`
  needing no extra data files, and bidirectional AWW framing all work
  on-device. (The child's stderr — `Error: ws_dissector fails to decode
  protocol 416.` — is expected: the probe payload is a 2-byte placeholder,
  not a complete NAS-5GS PDU; the framing/transport layer being validated
  here worked correctly regardless.)
- **Step 3** ✅ **PASSED on real hardware** (2026-07-14) — `examples/android_qc_capture.cpp`
  wired to `ws_dissector_client` following the `serialtest.cpp` pattern
  (`raw_msg_proto()` helper, `dump_fields()` extended with a `WsDissector*`,
  two new optional trailing args / `$WS_DISSECTOR` + `$WS_DISSECTOR_LIB` env
  fallback, startup probe-and-fallback sequence). Bugs found in review and
  fixed before shipping: a duplicate dead `FieldList` branch, a hardcoded
  personal absolute path where the env-var fallback belonged, a stale/wrong
  diagnostic message copy-pasted from `serialtest.cpp`, missing
  `<sstream>`/`<unistd.h>` includes (same silently-hidden-by-glibc class of
  bug as the `<cerrno>` issues found earlier in this project), and a stale
  top-of-file usage comment.
  - **Live capture result**: 1637 packets decoded, 50 `raw_msg` fields
    successfully expanded via real PDML across multiple distinct NR RRC
    channels (`bcch.dl.sch`, `ul.ccch`, `dl.ccch`, `ul.dcch`, `dl.dcch`,
    `bcch.bch`).
  - **Full validation**: a real `nr-rrc.ul.dcch` → `RRCSetupComplete` →
    embedded `dedicatedNAS-Message` decoded end-to-end into a complete,
    semantically-correct 5GS NAS message: `nas_5gs.mm.message_type` =
    `"Message type: Registration request (0x41)"`, with full field detail
    (initial registration, 5G-GUTI, AMF region/set ID, 5G-TMSI, UE security
    capabilities). This is a genuine 3GPP message name recovered from a live
    capture — the exact proof this plan existed to establish, and precisely
    what the downstream CSV `Signal` column (out of scope for this plan) will
    draw from.

**This plan is complete.** The full pipeline — `/dev/diag` → `diag_revealer`
→ FIFO → `DiagFifoSource` → `QualcommDecoder` → `raw_msg` bytes →
`ws_dissector` (real `libwireshark`, prebuilt Android artifact) → PDML with
real 3GPP message names — is proven on real hardware.

---

## Context

We're building an Android SDK around the already-validated C++ DIAG capture
pipeline (`AndroidQcMonitor`, proven live on-device: 624 packets, zero decode
failures). Part of that SDK's output is a CSV `Signal` column carrying exact
message names like `[RRC] Paging` / `[RRC] RRC Reconfiguration`.

Our own decoder (`log_packet.cpp`) only resolves the *channel type* for RRC/NAS
OTA packets (e.g. `nr-rrc.dl.ccch`, `nr-rrc.rrc_reconf`) — it deliberately does
not parse the embedded message itself. That's because those OTA packets carry
a genuine, standards-based 3GPP payload (ASN.1 PER-encoded RRC/NAS PDUs per TS
38.331 / TS 24.501), which is a completely different decoding problem from our
own hand-written parser for Qualcomm's proprietary DIAG log format. Rather than
reimplement an ASN.1 PER decoder plus the 3GPP grammar (a perpetual-maintenance
undertaking), we already reuse Wireshark's `epan` dissection engine via a small
existing wrapper, `ws_dissector`, validated end-to-end against a *desktop*
`libwireshark` build on the VM. It is not yet proven on the phone itself.

This plan scopes **only** getting `ws_dissector` working on-device — not the
CSV serializer, not the AIDL/JNI SDK layer (those are separate, already-
discussed follow-ons).

## Key discovery: no cross-compilation needed

The sibling repo `mobileinsight-mobile_withwireshark`'s `deploy.py` (lines
22-39) reveals the app's build already solves this by fetching **prebuilt**
Android artifacts rather than cross-compiling Wireshark from source:

```python
LIBS_GIT = 'https://github.com/mobile-insight/mobileinsight-libs.git'
LIBS_BRANCH = 'ws-3.4'
```

Cloned and inspected directly (still live, last commit 2020-11-21):

- `lib.tar.gz` → 8 shared libs: `libgio-2.0.so`, `libglib-2.0.so`,
  `libgobject-2.0.so`, `libgmodule-2.0.so`, `libgthread-2.0.so`,
  `libwsutil.so`, `libwireshark.so`, `libwiretap.so`
- `bin/android_pie_ws_dissector` — the dissector executable itself
- All are **ELF 32-bit ARM (armeabi-v7a)**, not arm64. `readelf -d` on the
  executable shows `NEEDED: libwireshark.so, libwiretap.so, libwsutil.so,
  libglib-2.0.so, libm.so, libc.so, libdl.so` — **no RPATH baked in**, and no
  libc++/libgnustl dependency.
- Version: Wireshark **3.4.0** — matches the actively-maintained desktop
  baseline (`install-ubuntu.sh`'s `ws_ver=3.4.0`), not the stale `3.2.7`
  string still sitting in `ws_dissector.cpp`.
- No gnutls/gcrypt linked — TLS/crypto dissection unsupported, consistent
  with the in-tree `ws_dissector/Makefile`'s Android build recipe.

**The 32-bit/64-bit mismatch doesn't matter**: `ws_dissector` runs as a
completely separate child process, communicating with our arm64 code purely
over pipes (the AWW wire protocol) — never linked in-process. This works as
long as the device supports running 32-bit ARM binaries (true for virtually
all current arm64-v8a phones via multilib; a longer-term risk only if 32-bit
support disappears from future devices — not a blocker now).

**Even better: our existing client code needs zero changes.**
`ws_dissector_client/ws_dissector_client.cpp`'s `start()` (lines 74-121)
already does exactly what this artifact needs — `setenv("LD_LIBRARY_PATH",
lib_path + ":" + existing, 1)` in the child before `execl()`, no `su` wrapper
(ws_dissector needs no root — it's just decoding bytes we already have). This
was written for the desktop case but happens to be exactly right here too.

**Confirmed against the legacy Python implementation**, which used the exact
same design (single flat directory holding both the binary and its `.so`
dependencies, `LD_LIBRARY_PATH` set before spawn, no root):
- Build time (`deploy.py::run_config()`): copies both the 8 `.so` files and
  the `bin/*` executables into one directory, `app/data`, inside the
  Kivy/python-for-android app's source tree.
- Install/first-run (`main_utils.py::init_libs()`): `get_files_dir()` resolves
  to the app's private on-device storage (via `Context.getFilesDir()`);
  `chmod 755` is applied to the executables there, since files extracted from
  an app bundle don't carry the exec bit automatically.
- Runtime (`android_dev_diag_monitor.py` → `ws_dissector.py::WSDissector.init_proc()`):
  a plain `subprocess.Popen` (no `su`) with `env["LD_LIBRARY_PATH"] =
  ws_library_path + ":" + ...` set before spawn — the exact mechanism our
  `WsDissector::start()` already replicates in C++.

## What's genuinely untested

`ws_dissector.cpp:main()` calls `wtap_init(TRUE)` + `epan_init(NULL, NULL,
TRUE)` and never calls `read_prefs()` — no external prefs file is loaded by
design, which reduces risk, but whether `epan_init` probes for any data files
(protocol registries, plugin directories) relative to the binary is genuinely
unknown for this specific prebuilt artifact. This needs one empirical check
before wiring anything into the capture pipeline, not an assumption either way.

## Implementation steps

**1. Fetch script + vendored location** (do not commit ~50MB of binaries to
git history):
- New: `ws_dissector/fetch_android_prebuilt.sh` — shallow-clones a **pinned
  commit SHA** (not just the `ws-3.4` branch, which can move) of
  `mobile-insight/mobileinsight-libs`, extracts `lib.tar.gz`, copies the 8
  `.so` files to `ws_dissector/android_prebuilt/lib/` and
  `android_pie_ws_dissector` to `ws_dissector/android_prebuilt/bin/` (skip
  `android_ws_dissector` and the irrelevant `tcpdump`). Ends with a
  self-check printing `file`/`readelf -d` output against the facts above.
- New: `ws_dissector/android_prebuilt/README.md` — what's here, how it got
  here, why 32-bit is fine, and an explicit note distinguishing this from
  `ws_dissector/Makefile`'s existing **source-build** `android`/
  `android_pie_ws_dissector` targets (same output filenames, different
  directory, via a from-scratch Vagrant/`luckiday/wireshark-for-android`
  cross-compile nobody has run recently) — avoid future confusion between
  "fetched prebuilt" and "built here."
- Modified: `.gitignore` — add `ws_dissector/android_prebuilt/`.

**2. Standalone on-device probe (the go/no-go gate)** — isolates "does
ws_dissector work at all" from "does the whole capture pipeline work," so a
failure here can't be confused with FIFO/diag_revealer issues:
- New: `examples/ws_dissector_probe.cpp` — minimal harness, argv
  `<path-to-android_pie_ws_dissector> <path-to-lib-dir>`. Constructs a
  `WsDissector`, calls `start()`, sends the same known-good probe
  `serialtest.cpp` already uses (`{0x7e,0x00}` as `"nas-5gs"`), prints the
  returned PDML and a clear `PASS`/`FAIL` line. No dependency on
  `MonitorBase`/`QualcommDecoder`/FIFOs at all.
- Modified: `examples/Makefile` — add a `ws_dissector_probe` target linking
  only `libws_dissector_client.a` (no `MONITOR_OBJS`, no decoder libs); add
  to `all:`/`clean:`.
- Build with the existing `TARGET=android` flow (already proven — produces
  the working arm64 `android_qc_capture` binary).

**On-device layout** (mirrors the already-proven `/data/local/tmp` location
used for the 624-packet validation run):
```
/data/local/tmp/mi/
  ws_dissector_probe                (new, arm64)
  ws_dissector/
    android_pie_ws_dissector         (arm32, fetched)
    lib/*.so                          (arm32, fetched, 8 files)
```
```bash
adb shell getprop ro.product.cpu.abilist   # confirm armeabi-v7a is listed first
adb push ws_dissector/android_prebuilt/bin/android_pie_ws_dissector /data/local/tmp/mi/ws_dissector/
adb push ws_dissector/android_prebuilt/lib/. /data/local/tmp/mi/ws_dissector/lib/
adb shell chmod 755 /data/local/tmp/mi/ws_dissector/android_pie_ws_dissector
adb shell chmod 644 /data/local/tmp/mi/ws_dissector/lib/*.so
adb shell /data/local/tmp/mi/ws_dissector_probe /data/local/tmp/mi/ws_dissector/android_pie_ws_dissector /data/local/tmp/mi/ws_dissector/lib
```
If this fails, the fix is in artifact placement / `LD_LIBRARY_PATH` / missing
data files — not in any code written after this point, since nothing past
this step has been touched yet.

**3. Wire into `examples/android_qc_capture.cpp`** (only after step 2 passes)
— port the exact pattern `serialtest.cpp` already implements, reusing it
verbatim rather than inventing a new mechanism:
- Add `#include "../ws_dissector_client/ws_dissector_client.h"`.
- Add `raw_msg_proto()` helper (identical to `serialtest.cpp:19-24`).
- Extend `dump_fields()` to take an optional `WsDissector *ws` and add the
  same "decode raw_msg via ws if available, else hex-only" branch
  (`serialtest.cpp:49-63`).
- Add two new **trailing optional** positional args (`ws_path`, `ws_lib`),
  falling back to `$WS_DISSECTOR`/`$WS_DISSECTOR_LIB` env vars, appended
  after the existing `out_path` slot so existing invocations are unaffected.
- Add the identical startup sequence: `access(X_OK)` check → `ws.start()` →
  probe-with-known-message → keep on success / `ws.stop()` + fall back to
  hex-only on failure (`serialtest.cpp:103-132`), reusing the stderr
  messaging verbatim.
- No changes needed to `monitor_c/android_qc_monitor.{h,cpp}` — ws_dissector
  only touches the display/dump layer, exactly as in `serialtest.cpp`.

## Verification

1. **Probe gate**: `ws_dissector_probe` returns PDML for the `nas-5gs` probe,
   no stderr complaints about missing libs/data files.
2. **Regression check**: re-run `android_qc_capture` on the phone with the
   same all-types whitelist as the already-validated 624-packet run, now with
   `$WS_DISSECTOR`/`$WS_DISSECTOR_LIB` set. Expect the same packet count and
   `ok=true` rate — no regression from adding ws wiring.
3. **New content check**: every `raw_msg/nr-rrc.*` / `raw_msg/nas-5gs` field
   now has a `--- ws_dissector (proto) ---` PDML block appended.
4. **Superset diff**: new output should be a strict superset of the prior
   624-packet dump (same fields unchanged, only PDML blocks newly appended)
   after stripping recv timestamps — same technique already used for the
   mock-vs-`OfflineReplayer` comparison earlier in this project.
5. **Message-name spot check**: inspect a handful of PDML blocks for
   `showname`/`field name` attributes containing actual 3GPP message names
   (e.g. `rrcReconfiguration`, `paging`) — this is the actual proof the CSV
   `Signal` column work depends on, even though the serializer itself is out
   of scope here.
6. Log the outcome as a new dated entry in `CLAUDE_CPP_MIGRATION.md` under
   Step 4, matching the project's existing documentation convention.

## Explicitly out of scope

- AIDL/JNI/bound-service/AAR Android SDK layer
- The CSV serializer itself
- Cross-compiling Wireshark 3.4.0 from source for arm64 (only needed if
  32-bit multilib support disappears from target devices — flag for later)
- Upgrading past Wireshark 3.4.0
- Any modification to `ws_dissector/ws_dissector.cpp` / `packet-aww.cpp`
- `ws_dissector` process lifecycle/teardown across app lifecycle (deferred
  the same way `diag_revealer` teardown already is, per the migration plan)

## Critical files

- `ws_dissector_client/ws_dissector_client.{h,cpp}` — reused unmodified
- `examples/serialtest.cpp` — reference pattern, not modified
- `examples/android_qc_capture.cpp` — modified (step 3)
- `examples/Makefile` — modified (new probe target)
- New: `ws_dissector/fetch_android_prebuilt.sh`,
  `ws_dissector/android_prebuilt/README.md`, `examples/ws_dissector_probe.cpp`
- `.gitignore` — modified
