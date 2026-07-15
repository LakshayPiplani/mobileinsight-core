# Rebuilding `ws_dissector` for Android with the EA0/null_decipher fix

Parent: [CLAUDE_WS_ANDROID_PLAN.md](CLAUDE_WS_ANDROID_PLAN.md) (getting `ws_dissector` running on-device — this doc picks up a limitation found during that plan's live validation)

---

## TL;DR — what needs to be done

The bug is that the on-device `ws_dissector` binary never turns on the `nas-5gs.null_decipher` preference, even though the library it uses already knows how to decode EA0. Fix = rebuild only the small binary from current source and push it. Concretely, on the VM:

1. Get glib **2.54.3** source into a durable location (supplies the portable glib headers). — *Step 1*
2. Generate an arm32 `glibconfig.h` via `configure --cache-file=android.cache` (the one ABI-specific glib header; must match the prebuilt `.so`). Hand-write is the fallback. — *Step 2*
3. Cross-compile `ws_dissector.cpp` + `packet-aww.cpp` with NDK r19b arm32 clang, linking the **existing** `android_prebuilt/lib/*.so`. — *Steps 3–4*
4. Verify at the pref layer (no `"Failed to set nas-5gs.null_decipher."`, real MM name in PDML), then probe, then full `android_qc_capture`. — *Step 5*

What you do **not** do: rebuild `libwireshark/libwiretap/libwsutil/libglib-2.0.so` (already contain the fix), build glib itself, or build libiconv/libffi/gettext.

## Context

While validating [CLAUDE_WS_ANDROID_PLAN.md](CLAUDE_WS_ANDROID_PLAN.md) on real hardware, we found that **standalone `nas-5gs` AWW dissections consistently fail to produce any inner decoded content** (envelope-only PDML — no message name), while the *same kind* of NAS content reached as a nested sub-dissection from within `nr-rrc` (e.g. `RRCSetupComplete`'s embedded `dedicatedNAS-Message`) decodes fine. This affects standalone `5G_NR_NAS_MM5G_Plain_OTA_Container_Msg` / `..._Incoming_Msg` / `..._Outgoing_Msg` packets specifically — confirmed reproducible even for packets whose own header byte explicitly marks them **Plain** (not ciphered), ruling out a literal "it's encrypted" explanation.

The user identified the actual cause: **`nas-5gs.null_decipher`**, a Wireshark preference ("Try to detect and decode 5G-EA0 ciphered messages") that must be explicitly turned on — they had already rebuilt *desktop* Wireshark with this preference enabled and fixed the identical class of problem there, but had not yet done so for the Android prebuilt binary.

## Root cause, now pinned down empirically

The defect is **not** a missing capability in the libraries — it's that the running process never turns the capability on. Two facts, both verified this session with `strings` (not assumed):

1. **The preference and its decode logic already exist in the shipped Android `libwireshark.so`.** `strings android_prebuilt/lib/libwireshark.so` shows the pref variable `g_nas_5gs_null_decipher`, the pref name `null_decipher`, and both human-readable strings *"Try to detect and decode 5G-EA0 ciphered messages"* and *"This should work when the NAS ciphering algorithm is NULL (5G-EEA0)"*. So the EA0-detection code is compiled in and waiting.

2. **The shipped `android_pie_ws_dissector` binary never flips the switch.** `strings android_prebuilt/bin/android_pie_ws_dissector` contains **no** `null_decipher` and **no** `"Failed to set nas-5gs.null_decipher."` string. That binary was built from the pinned 2020 source (`eda41f7`, 2020-11-21), which predates the June 2026 change that added the `prefs_set_pref("nas-5gs.null_decipher:TRUE")` call.

The pref-setting code exists only in the *current* source, at `ws_dissector/ws_dissector.cpp:154-161`. And because this stripped-down `main()` sets prefs manually via `prefs_set_pref()` and never calls `read_prefs()`, there is **no config-file or wire-protocol way to inject the preference at runtime** — the pref must be baked in at compile time.

**Conclusion:** the fix is to **recompile only the two thin wrapper files** (`ws_dissector.cpp`, `packet-aww.cpp`) from current source and push the resulting binary. The four heavy `.so`s (`libwireshark`/`libwiretap`/`libwsutil`/`libglib-2.0`) are already correct and must **not** be rebuilt.

## What this closes vs. the original plan

- **Original Open Risk #1** ("were the `.so`s actually built from this patched lineage, or will `prefs_set_pref` silently no-op?") is now **closed**: fact (1) above proves the pref is present in the exact `.so` we link against. The failure mode it warned about — the pref name not resolving at runtime — would surface as the explicit `"Failed to set nas-5gs.null_decipher."` line, which we now use as a deliberate go/no-go signal (see Step 5).
- **Original Open Risk #2** (hand-written `glibconfig.h` might be subtly wrong) is now **replaced by a generate-don't-guess approach** using the upstream MobileInsight glib recipe (see "The glib-header gap" below).

## The one real build obstacle: glib *headers* (not libs)

The two wrapper files don't `#include <glib.h>` directly (their only visible glib use is `g_new` at `ws_dissector.cpp:68`), **but** every `epan/*.h` and `wiretap/*.h` they do include pulls in `<glib.h>` transitively, and glib types (`gint`, `gboolean`, `GHashTable *`, …) define the field layout of structs these files touch by hand — `frame_data`, `packet_info`, `epan_dissect_t`. Those layouts **must** match how `libwireshark.so` was compiled, or you get silent struct-layout corruption across the process's own calls rather than a clean error. So the glib headers — and specifically an ABI-correct `glibconfig.h` — are a hard requirement for the compile step.

Two distinctions that matter:

- **A `.so` is a link target, not a compile input.** `android_prebuilt/lib/libglib-2.0.so` is consumed at the *link* step via `-lglib-2.0`. You cannot `#include` a shared object — it carries no declarations, macros, or struct definitions. The `mobileinsight-libs` `ws-3.4` repo ships only `lib/*.so` (confirmed: no `include/` tree, no headers), so it does not solve the compile-time header need.
- **glib headers come in two flavors.** The *portable* headers (`glib.h` and its `glib/*.h` friends) are identical on every platform and come straight from the glib 2.54.3 source tree. Only **`glibconfig.h` is ABI-specific** and must be generated for the 32-bit ARM Android target — it is the single file that has to match the prebuilt `.so`.

### How to get an ABI-correct `glibconfig.h`: generate it, don't hand-write it

Reference: <https://zwyuan.github.io/2016/07/17/cross-compile-glib-for-android/>, by Zengwen Yuan (`zwyuan`) — one of the authors credited in `ws_dissector/Makefile`, so this is very likely the exact recipe that produced the `libglib-2.0.so` we link against. Using it keeps `glibconfig.h` in the same lineage as the `.so`.

Cross-compiling `glibconfig.h` is normally hard because glib's `configure` wants to **run** small test binaries on the target to discover values it can't get by compiling alone (stack-growth direction, `getpwuid_r` variants, pid handling). You can't execute ARM binaries on an x86 build host. The recipe's answer is an **`android.cache`** file that pre-seeds every such answer, so `configure` reads them from cache instead of executing anything. `configure` then generates `glibconfig.h` from `glibconfig.h.in` using those cached values plus compile-only checks (sizeof, byte order — which the cross-compiler resolves by compiling, no execution needed). Verbatim contents from the page:

```
glib_cv_long_long_format=ll
glib_cv_stack_grows=no
glib_cv_sane_realloc=yes
glib_cv_have_strlcpy=no
glib_cv_va_val_copy=yes
glib_cv_rtldglobal_broken=no
glib_cv_uscore=no
glib_cv_monotonic_clock=no
ac_cv_func_nonposix_getpwuid_r=no
ac_cv_func_posix_getpwuid_r=no
ac_cv_func_posix_getgrgid_r=no
glib_cv_use_pid_surrogate=yes
ac_cv_func_printf_unix98=no
ac_cv_func_vsnprintf_c99=yes
ac_cv_func_realloc_0_nonnull=yes
ac_cv_func_realloc_works=yes
```

This also settles the two fields the earlier plan flagged as "it depends": `g_pid_type` handling is driven by `glib_cv_use_pid_surrogate=yes`, and the thread impl is POSIX (standard on Android).

**What we can drop from the page's full recipe, and why:** the page builds *all* of glib for Android (NDK r10e + a standalone GCC toolchain, plus libiconv/libffi/gettext). We need almost none of it, because we already have the prebuilt `libglib-2.0.so` and only want the header:

| The page does this | Do we need it? | Why / why not |
|---|---|---|
| NDK r10e + standalone GCC toolchain | ❌ | `glibconfig.h`'s ABI values are fixed by the **target arch** (32-bit LE ARM), not the compiler, so our NDK r19b arm32 clang emits the same header. |
| `android-19` platform level | ❌ | `glibconfig.h` is ABI-level-independent; API level doesn't change it. |
| Build libiconv / libffi / gettext | ❌ | Those are needed to *build the glib libraries*. We link the prebuilt `.so`, so we never build glib itself. |
| `make && make install` (full glib build) | ❌ | We need `configure` to run only far enough to **emit `glibconfig.h`**, then stop. |
| `android.cache` + `--cache-file=android.cache` | ✅ | This is the one essential piece — it makes the generated header deterministic and matched to the `.so`'s lineage. |

## What's needed to do the rebuild, and what's already confirmed available

| Requirement | Status |
|---|---|
| arm32 clang toolchain | ✅ NDK r19b (already used for our arm64 `android.mk`) ships `armv7a-linux-androideabi21-clang++` under `toolchains/llvm/prebuilt/linux-x86_64/bin/` — no need for the deprecated standalone GCC toolchain the old `ws_dissector/Makefile` references. The prebuilt `.so`s are ELF 32-bit ARM (armeabi-v7a), so we must target arm32, not the arm64 the rest of the project uses. |
| Wireshark 3.4.0 headers | ✅ Present and pre-configured on the VM at `~/mi-dev/mobileinsight-core/wireshark-3.4.0/` (`config.h` present — `cmake -DBUILD_wireshark=OFF .` already run there). `config.h` checked for host-specific (x86-64) landmines: no `WORDS_BIGENDIAN`/`SIZEOF_*` macros; visible `HAVE_*` flags are optional-feature detections (Kerberos, libssh, libcap, libnl — all irrelevant to our two thin wrappers). Low risk. |
| Existing Android `.so` files | ✅ Fetched per `CLAUDE_WS_ANDROID_PLAN.md` (`ws_dissector/android_prebuilt/lib/*.so`), linked against unchanged. |
| glib 2.54.3 headers matching the `.so`'s ABI | ⚠️ To be produced via the `android.cache` generate approach above. No durable glib tree exists on the VM — the earlier scratch download is ephemeral. Plan: check out glib 2.54.3 source into the repo (portable headers) and generate `glibconfig.h` with `configure --cache-file=android.cache`. |

## Plan

Each step below states what it does and why it's needed.

1. **Obtain the glib 2.54.3 source tree.** *Why:* it supplies the portable headers (`glib.h` + `glib/*.h`) the Wireshark headers include transitively, and it's the tree whose `configure` generates `glibconfig.h`. Use 2.54.3 specifically to match the prebuilt `libglib-2.0.so`. Put it somewhere durable (in-repo or a fixed VM path), not scratch, since the header path must survive across sessions.

2. **Generate `glibconfig.h` for the arm32 Android target.** *Why:* it's the only ABI-specific glib header, and it must match the prebuilt `.so`. Drop in the `android.cache` above (`chmod a-x android.cache`) and run configure only far enough to emit the header, pointing at the NDK r19b arm32 clang:
   ```
   CC=armv7a-linux-androideabi21-clang ./configure \
       --host=arm-linux-androideabi --build=x86_64-linux-gnu \
       --cache-file=android.cache --with-pcre=no
   ```
   The deliverable is the generated `glibconfig.h`, not a build. **Fallback:** glib 2.54's `configure` may bail on a hard dependency (e.g. libffi, required by gobject) *before* writing `glibconfig.h`. If so, hand-write `glibconfig.h` — but note this is now low-risk, because every "it depends" value is pinned by the `android.cache` above (`use_pid_surrogate=yes`, `stack_grows=no`, etc.), leaving only pure, well-known 32-bit-LE-ARM ABI constants (`GLIB_SIZEOF_VOID_P=4`, `GLIB_SIZEOF_LONG=4`, `G_BYTE_ORDER=G_LITTLE_ENDIAN`).

3. **Assemble the include path.** *Why:* the compiler needs the Wireshark headers, the portable glib headers, and the generated `glibconfig.h` all visible together. Combine the glib source root + the generated `glibconfig.h` into one include dir, alongside the Wireshark 3.4.0 tree.

4. **Cross-compile the two wrapper files, linking the existing `.so`s.** *Why:* this is the actual fix — a binary built from current source that calls `prefs_set_pref("nas-5gs.null_decipher:TRUE")`, linked against the unchanged libraries that already contain the decode logic.
   ```
   armv7a-linux-androideabi21-clang++ -fPIE -pie \
       ws_dissector.cpp packet-aww.cpp \
       -I<wireshark-3.4.0 tree> \
       -I<glib source root> -I<dir holding generated glibconfig.h> \
       -L ws_dissector/android_prebuilt/lib \
       -lwireshark -lwiretap -lwsutil -lglib-2.0 \
       -o android_pie_ws_dissector_v2
   ```
   - `-fPIE -pie` — API 21+ Android refuses non-PIE executables; this reproduces the `_pie` variant. (Clang defaults to PIE at API 21, but be explicit.)
   - `-L ws_dissector/android_prebuilt/lib` — link the *existing* prebuilt libs, not `/usr/local/lib`. Reusing the libs and replacing only the binary is the whole strategy.
   - Mixing a clang++-built binary with the GCC-built 2020 `.so`s is safe here: Wireshark exports a **C** ABI (`extern "C"`) and `libwireshark` is C, so there's no C++ name-mangling or libc++/libstdc++ clash to worry about.

5. **Verify at the preference layer first, then the probe, then a full capture.** *Why:* test the actual defect before trusting anything broad. Feed the new binary one of the standalone NAS packets that failed in `out_with_ws_narrowed.txt` and confirm:
   - it does **not** print `"Failed to set nas-5gs.null_decipher."` — that line means the pref name didn't resolve, i.e. wrong lib lineage (the detectable failure mode from old Risk #1); and
   - the PDML now carries a real MM message name instead of envelope-only.

   Then push the new binary alongside the existing `.so`s (same layout as before, no `.so` changes) and re-run `examples/android_ws_dissector_probe` as the go/no-go gate, same pattern as `CLAUDE_WS_ANDROID_PLAN.md` Step 2. Finally re-run `android_qc_capture` and confirm standalone `nas-5gs` packets now decode with real message names, matching the RRC-embedded ones that already worked.

## Open risks

- glib 2.54's `configure` may not reach `glibconfig.h` generation without the full dependency chain present — mitigated by the hand-write fallback in Step 2, which is now low-risk because the `android.cache` pins the non-obvious values.
- Exact API level (21) is assumed for consistency with the rest of this project's Android tooling — not verified against whatever level the original 2020 `.so` build targeted (the page used `android-19`). The `ws_dissector` binary runs as a separate process from our arm64 code, so a small API-level mismatch is low-impact, but worth noting.

## Critical files / locations

- `ws_dissector/ws_dissector.cpp` (pref-setting at `:154-161`), `ws_dissector/packet-aww.cpp` — the two files being recompiled
- `ws_dissector/android_prebuilt/lib/*.so` — existing Android libs to link against (from `CLAUDE_WS_ANDROID_PLAN.md`); already verified to contain the `null_decipher` pref + decode logic
- `ws_dissector/android_prebuilt/bin/android_pie_ws_dissector` — the stale 2020 binary that lacks the pref-setting call (the thing being replaced)
- `ws_dissector/fetch_android_prebuilt.sh` — fetches the pinned-commit prebuilt `.so`s + binary; extracts `lib/*.so` only (no headers)
- VM: `~/mi-dev/mobileinsight-core/wireshark-3.4.0/` — pre-configured Wireshark 3.4.0 source/headers
- <https://zwyuan.github.io/2016/07/17/cross-compile-glib-for-android/> — upstream glib-for-Android recipe (source of the `android.cache`)
- `install-ubuntu.sh:100-106` — the desktop build recipe this Android build adapts
