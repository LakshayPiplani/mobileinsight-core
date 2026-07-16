# DIAG capture on GKI phones (no `/dev/diag`) — findings

Context: while extending the on-device `ws_dissector`/EA0 work ([CLAUDE_ANDROID_WS_BUILD.md](CLAUDE_ANDROID_WS_BUILD.md)) to a second phone, the DIAG **capture front-end** failed in a way that turned out to be a fundamentally different device architecture. This doc records what we found and the viable paths. **None of it affects the ws_dissector/EA0 fix**, which is capture-source-independent — this is purely about *getting the raw DIAG byte stream* on a modern device.

Two devices in play:
- **"Nord"** — has the classic Qualcomm `diagchar` kernel driver → `/dev/diag` → on-device capture works (this is where the EA0 fix was validated end-to-end).
- **"hiphi"** — SM8450 / Snapdragon 8 Gen 1, Android 11, **GKI kernel**, rooted (Magisk). **No `/dev/diag`.** This doc is about hiphi.

---

## TL;DR / verdict

hiphi has **no `diagchar` kernel char device**. Instead it runs Qualcomm's **userspace diag stack**: a daemon **`diag-router`** reads the modem over `qrtr` sockets and exposes the DIAG stream to clients through the **`libdiag.so`** client library (the same library `diag_mdlog` and the `diag_*_sample` programs use). So **on-device capture is feasible** — not via `/dev/diag`, but via a `libdiag` client. The user's "hook it in memory on-device" instinct is exactly what `libdiag` is for.

**Recommended routes** (no kernel work, no USB tether, no displacing `diag-router`) — **both confirmed on hiphi 2026-07-16:**
- **Route A (fastest) ✅ inputs confirmed:** run the vendor logger **`diag_mdlog`** → `.qmdl` → decode **offline** with the fixed `ws_dissector` ([monitor_c/offline_replayer.cpp](monitor_c/offline_replayer.cpp)). `diag_mdlog -h` confirms `-f <mask file> -o <dir> -s <MB> -c`; ready mask configs live in `/vendor/etc/*.cfg` (e.g. `Connection_Issues_V16.cfg`, `4G_5G_Throughput_light_V1.cfg`, `QC_default.cfg`, `default.cfg`).
- **Route B (proper) ✅ proven live:** port `diag_revealer` to a **`libdiag`/DCI client** (model on `diag_dci_sample` / `diag_callback_sample`). `diag_dci_sample` was run on-device and did the full flow — `Diag_LSM_Init` → connect to `diag-router` → register DCI client → `MPSS UP` → set a log mask → **receive live log packets from MSM (the modem)**. Swap the sample's test log code (`0x115F`) for the 5G NAS log codes. Headers aren't on-device → pull `libdiag` headers + the `diag_dci_sample.c` source from **CodeLinaro `qcom-opensource/diag`**, link against `/vendor/lib64/libdiag.so`.

**Suggested order:** Route A first (fast end-to-end proof of decode-with-EA0-fix on this phone; verify `offline_replayer` ingests `.qmdl` vs `mi2log`), then Route B for native live capture.

---

## The problem

`diag_revealer` (and the whole `android_qc_capture` front-end) opens **`/dev/diag`**. On hiphi that node doesn't exist, so `diag_revealer` prints `open diag dev: No such file or directory` and nothing is captured.

Two separate blockers were hit, in order:

1. **SELinux blocks the FIFO for the `shell` domain.** Running the capture as the unprivileged `shell` user fails at `mkfifo`:
   ```
   avc: denied { create } for comm="mkfifo" ... scontext=u:r:shell:s0 tcontext=u:object_r:shell_data_file:s0 tclass=fifo_file permissive=0
   ```
   The tell: the same process *wrote `Diag.cfg` (a regular file) successfully* one line earlier — so it's not directory permissions, it's SELinux denying `fifo_file` creation for `shell`. **Fix: run the whole capture as root (`su`)** — the `magisk` domain isn't subject to that denial. (DIAG needs root anyway.)

2. **`/dev/diag` does not exist** (the real blocker, below).

---

## Investigation evidence (chronological)

**Chipset — it *is* Qualcomm, so chipset isn't the issue:**
```
ro.soc.model=SM8450   ro.board.platform=taro   ro.soc.manufacturer=QTI
```

**No diag char device / driver:**
```
ls /dev/diag                → No such file or directory
ls /dev | grep -i diag      → only ffs-diag, ffs-diag-1, ffs-diag-2   (USB FunctionFS, NOT /dev/diag)
cat /proc/devices | grep diag → (nothing)   # no char major registered → diagchar not loaded
find … -name "*diag*.ko"    → only usb_f_diag.ko   # USB gadget function, NOT the diag core
lsmod | grep diag           → usb_f_diag           # loaded, used by the USB gadget
```
So `mknod /dev/diag` **cannot help** — there is no registered diag driver behind it. `usb_f_diag` is only the USB-gadget function that streams DIAG to a *tethered host*; it does not create an on-device char device.

**The userspace diag stack (the key discovery):**
```
/proc/mounts:      diag /dev/ffs-diag functionfs …    # diag USB implemented via FunctionFS (userspace)
/config/usb_gadget/g1/functions:  diag.diag, ffs.diag, … (+ _mdm, _mdm2)
ps -A | grep diag: system  1768  … S diag-router      # userspace diag daemon (the /dev/diag replacement)
/proc/devices:     253 rpmsg, 504 glinkpkt            # peripheral transports exist as generic char majors
```

**`diag-router`'s open files (`/proc/1768/fd`) — how it actually works:**
- Holds the **USB FunctionFS endpoints**: `ffs-diag/ep0-2`, `ffs-diag-1/ep0-2`, `ffs-diag-2/ep0-2`.
- Holds **~65 sockets** (`socket:[…]`) + `hwbinder` + `eventfd`.
- Holds **no `/dev/diag`, no `glinkpkt`/`rpmsg`/`smd` char fd**.

→ `diag-router` reaches the modem's DIAG service over **sockets (`AF_QIPCRTR`/qrtr)**, not a char device, and bridges to USB via FunctionFS. The raw DIAG stream lives entirely inside `diag-router`'s userspace address space. Pipeline:
```
modem DIAG service ──qrtr sockets──> diag-router (userspace) ──ffs-diag──> USB host
                                              └──libdiag sockets──> local client apps
```

**The client interface exists and is sample-backed (the payoff):**
```
/vendor/lib/libdiag.so   /vendor/lib64/libdiag.so     # the diag CLIENT library
/vendor/lib/libdiagjni.so …
/vendor/bin/diag-router                                # the daemon
/vendor/bin/diag_mdlog                                 # production logger → .qmdl (uses libdiag)
/vendor/bin/diag_socket_log  diag_uart_log
/vendor/bin/diag_callback_sample                       # sample: register callback, receive diag stream
/vendor/bin/diag_dci_sample                            # sample: DCI (Diag Consumer Interface) client
```
`/proc/net/unix | grep -i diag` showed no *named* diag listener — `libdiag` clients connect over abstract/unnamed sockets, so that's expected and not a contradiction.

---

## Architecture conclusion

`/dev/diag` wasn't merely removed — it was **replaced by a library API**. On this GKI device the entire diag userspace-facing interface is `libdiag.so` talking to `diag-router` over sockets. A client:
1. `Diag_LSM_Init()` — attach to `diag-router`.
2. set the log masks (the same 5G NAS `type_names` we already generate).
3. switch to callback/memory-device mode + register a callback.
4. receive DIAG log frames **in memory** — no `/dev/diag`, no USB, no root-owned FIFO gymnastics.

`diag_mdlog` and `diag_callback_sample` are working proof this path is available.

---

## Routes forward

| Route | What | Effort | Notes |
|---|---|---|---|
| **A — vendor logger** | `diag_mdlog` → `.qmdl` on-device → decode offline with fixed `ws_dissector` | Low | Reuses vendor capture + our decoder; likely little/no new code. Need to drive its mask config and wire `.qmdl` into `offline_replayer.cpp`. |
| **B — libdiag client** | Port `diag_revealer` to a `libdiag` client (model on `diag_callback_sample`) for live capture | Medium | The "right" on-device answer; needs the `libdiag` headers (from a QC LA/vendor diag package). Makes `android_qc_capture` work natively. |
| Restore `/dev/diag` | Build/flash a kernel or module with `CONFIG_DIAGCHAR` | High | Then `diag_revealer` works unchanged; often impractical on locked SM8450. |
| Tethered USB | Let `diag-router`→USB do its job; read on a host PC (QCSuper / MobileInsight desktop) | Low–Med | Works today; not on-device. Linux host enumerates the QC diag port more easily than macOS. |
| ~~mknod /dev/diag~~ | — | — | **Impossible**: no diag driver registered in `/proc/devices`. |
| ~~USB pseudo-loopback~~ | Emulate a USB host on-device to read the diag gadget | — | **Wrong layer + missing kernel support** — see below. |

---

## Why the "pseudo USB device / read our own USB" idea does not work

(Recorded because it's a natural instinct.) In short: **USB is host-driven and a gadget can't read its own output; emulating a host on-device needs kernel features GKI lacks; and it's a huge detour to bytes `libdiag` already hands you directly.**

1. **USB is asymmetric (host vs gadget).** The phone is the *gadget*; it never initiates transfers. An IN endpoint only emits when a *host* requests it — there is no device-side "read" of your own gadget output.
2. **Reading the stream requires a USB host.** For the phone to consume its own diag USB output it must also be a host to itself (loopback). That needs a virtual HCD (`dummy_hcd`) — a kernel test module not enabled on GKI — plus a userspace USB host stack and a diag-over-USB host reader. And `usb_f_diag` is bound to the real hardware controller (DWC3), not a virtual host.
3. **Wrong layer.** The USB path frames the diag stream into the diag-USB transport only for a host to un-frame it — arriving at the same bytes `diag-router` already holds. `libdiag` gives you those bytes with no USB and far less code.
4. **FunctionFS doesn't help.** Being the `ffs-diag` implementer means reading from the *modem source* and *writing* to the IN endpoint (which only drains when a host reads). That's just Route B (be a diag client) minus the USB — USB adds nothing.
5. **Collateral.** The diag USB function shares the gadget config with adb; reconfiguring it can drop your adb shell, and `diag-router` already owns the `ffs-diag` endpoints.

The only case where USB is the right answer is a **real external host** (tethered PC) — the self/pseudo version is strictly dominated by the `libdiag` client route.

---

## Next steps (route selection done — both proven 2026-07-16)

**Route A (do first):**
```bash
su -c 'mkdir -p /data/local/tmp/qmdl'
su -c '/vendor/bin/diag_mdlog -f /vendor/etc/Connection_Issues_V16.cfg -o /data/local/tmp/qmdl -s 100 -c'
# drive live 5G signaling; Ctrl-C to stop; pull the .qmdl and decode offline with the fixed ws_dissector
```
- Verify `offline_replayer.cpp` ingests `.qmdl` (vs MobileInsight `mi2log`); add a small format bridge if needed.
- Pick/craft a mask `.cfg` that includes the 5G NAS log codes (start broad with `Connection_Issues_V16.cfg` / `default.cfg`, filter at decode time).

**Route B (native live capture):**
- Get `libdiag` headers (`diag_lsm.h`, `diag_lsm_dci.h`, `diagpkt.h`, …) + `diag_dci_sample.c` from **CodeLinaro `qcom-opensource/diag`**; link `/vendor/lib64/libdiag.so`.
- Write a `diag_revealer` replacement: `Diag_LSM_Init` → register DCI client → set DCI log masks for the 5G NAS log codes (the same types in [android_qc_capture.cpp:139-143](examples/android_qc_capture.cpp#L139-L143)) → receive log packets in the callback → feed `android_qc_capture`'s decode loop.
- Run as root (SELinux; DIAG access).

## Key device facts (hiphi)

- SM8450 / taro / QTI, Android 11, GKI kernel, rooted via Magisk (`su` → `u:r:magisk:s0`).
- `diag-router` pid 1768 (user `system`) — the userspace diag daemon.
- `libdiag.so` (32/64) + `diag_mdlog` + `diag_*_sample` under `/vendor/{lib,lib64,bin}`.
- DIAG USB = FunctionFS (`/dev/ffs-diag*`), gadget functions `ffs.diag` / `diag.diag`.
- Peripheral transports: `rpmsg` (major 253), `glinkpkt` (major 504); modem diag reached via `qrtr` sockets, not a char node.
- SELinux Enforcing; `shell` domain cannot create `fifo_file` → capture must run as root.
