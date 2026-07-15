# Rebuilding `ws_dissector` for Android with the EA0/null_decipher fix

Parent: [CLAUDE_WS_ANDROID_PLAN.md](CLAUDE_WS_ANDROID_PLAN.md) (getting `ws_dissector` running on-device — this doc picks up a limitation found during that plan's live validation)

**Status (2026-07-15): COMPLETE & VERIFIED ON-DEVICE.** The arm32 `ws_dissector` binary was rebuilt with the EA0 fix, the probe passed (Step 5.2), and a live `android_qc_capture` run (Step 5.3) confirmed protocol-416 NAS-5GS packets now fully decode — including `Security mode complete` and the post-security-mode messages that previously logged `fails to decode protocol 416`.

---

## The problem in one sentence

Standalone `nas-5gs` packets (5G NAS OTA container, AWW **protocol 416**) come back envelope-only — logged as `Error: ws_dissector fails to decode protocol 416` in `out_with_ws_narrowed.txt` — because the on-device `ws_dissector` binary never turns on the `nas-5gs.null_decipher` preference, even though the `libwireshark.so` it uses already contains the EA0-decode logic.

---

# SOLUTION (reproducible recipe)

All commands run on the build **VM** unless noted. Concrete paths are the ones used on this VM.

### Prerequisites (one-time host setup)

```bash
sudo apt-get install -y gettext          # provides msgfmt (a host build tool glib's configure requires)
```

### 1. glib 2.54.3 source (portable headers)

```bash
cd ~/mi-dev/mobileinsight-core
wget https://download.gnome.org/sources/glib/2.54/glib-2.54.3.tar.xz
tar -xf glib-2.54.3.tar.xz
```

### 2. Generate the arm32 `glibconfig.h`

Create `glib-2.54.3/android.cache` with these values (the first 16 are the upstream MobileInsight recipe; the last 5 we added to get past `configure`'s dependency gates — see reasoning):

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
ac_cv_func_iconv_open=yes
gt_cv_func_dgettext_libc=yes
gt_cv_func_ngettext_libc=yes
ac_cv_header_libintl_h=yes
ac_cv_func_bind_textdomain_codeset=yes
```

Then run `configure` far enough to emit the header (we do **not** build glib):

```bash
cd ~/mi-dev/mobileinsight-core/glib-2.54.3
export NDK_BIN=/home/vagrant/android-ndk-r19b/toolchains/llvm/prebuilt/linux-x86_64/bin
export PATH="$NDK_BIN:$PATH"

CC="$NDK_BIN/armv7a-linux-androideabi21-clang" \
LIBFFI_CFLAGS=" " LIBFFI_LIBS=" " \
./configure \
  --host=arm-linux-androideabi --build=x86_64-linux-gnu \
  --cache-file=android.cache \
  --with-pcre=internal --enable-included-printf --disable-dependency-tracking
```

Verify: `grep GLIB_SIZEOF_VOID_P glib/glibconfig.h` → must be **4** (32-bit ARM).

### 3. Fix the reused x86 Wireshark `config.h`

The `wireshark-3.4.0/config.h` was generated for the x86-64 desktop build and defines an Intel-only flag that drags in x86 SIMD headers. Edit `~/mi-dev/mobileinsight-core/wireshark-3.4.0/config.h`, changing:

```c
#define HAVE_SSE4_2 1
```
to:
```c
/* #undef HAVE_SSE4_2 */
```

### 4. Remove the dead pcap include

`<pcap.h>` is included but never used (the only DLT reference is a Wireshark *preference string*, `DLT=148`, not the pcap API). Delete this line from `ws_dissector.cpp`:

```c
#include <pcap.h>
```

### 5. Cross-compile and link

```bash
cd ~/mi-dev/mobileinsight-core/ws_dissector
export NDK_BIN=/home/vagrant/android-ndk-r19b/toolchains/llvm/prebuilt/linux-x86_64/bin
WS=~/mi-dev/mobileinsight-core/wireshark-3.4.0
GLIB=~/mi-dev/mobileinsight-core/glib-2.54.3

# ensure the prebuilt arm32 .so's are present (pinned commit; contain the null_decipher pref)
./fetch_android_prebuilt.sh

$NDK_BIN/armv7a-linux-androideabi21-clang++ -fPIE -pie -static-libstdc++ \
    ws_dissector.cpp packet-aww.cpp \
    -I"$WS" -I"$GLIB" -I"$GLIB/glib" -I"$GLIB/gmodule" \
    -L android_prebuilt/lib \
    -lwireshark -lwiretap -lwsutil -lglib-2.0 \
    -Wl,--allow-shlib-undefined \
    -o android_pie_ws_dissector_v2
```

### 6. Verify the built binary

```bash
strings android_pie_ws_dissector_v2 | grep null_decipher     # expect BOTH the pref string and the error string
readelf -d android_pie_ws_dissector_v2 | grep NEEDED         # expect the 4 ws/glib .so's + libc/libm/libdl, NO libc++_shared.so
```

Both checks passed on 2026-07-15: the binary contains `nas-5gs.null_decipher:TRUE` (the original prebuilt binary contained neither string), and depends only on `libwireshark/libwiretap/libwsutil/libglib-2.0/libm/libdl/libc`.

### 7. Deploy + verify on-device (Step 5, in progress)

See "On-device verification" below.

---

# WHY — root cause and reasoning

## Root cause: capability present in the library, switch never flipped

Verified with `strings` (not assumed):

1. The pref **and its decode logic already exist** in the shipped Android `libwireshark.so` — `g_nas_5gs_null_decipher`, `null_decipher`, and the description strings *"Try to detect and decode 5G-EA0 ciphered messages"* / *"...NULL (5G-EEA0)"* are all present. So **no `.so` needs rebuilding**.
2. The shipped `android_pie_ws_dissector` binary (pinned 2020 commit `eda41f7`) contains **no** `null_decipher` string — it predates the June 2026 commit that added the `prefs_set_pref("nas-5gs.null_decipher:TRUE")` call at `ws_dissector.cpp:154-161`.

Because this stripped-down `main()` sets prefs via `prefs_set_pref()` and never calls `read_prefs()`, the preference cannot be injected at runtime via a config file or the wire protocol — it must be compiled in. Hence: **recompile only the two thin wrapper files** (`ws_dissector.cpp`, `packet-aww.cpp`) from current source and push the new binary. The four heavy `.so`s stay unchanged.

This closes the original plan's biggest open risk ("were the `.so`s built from this patched lineage?") — fact (1) proves the pref is in the exact `.so` we link.

## The real build obstacle was glib *headers* — not libs

The two `.cpp` files are thin wrappers over Wireshark's C API. The **compile/link split** is the key mental model:

| Phase | Needs | Source |
|---|---|---|
| Compile (`.cpp`→`.o`) | declarations (prototypes, structs, macros) | header trees: `wireshark-3.4.0/`, `glib-2.54.3/` |
| Link (`.o`→binary) | implementations (machine code) | prebuilt `.so`s: `libwireshark.so`, `libglib-2.0.so`, … |

So `wireshark-3.4.0/` is needed as the **header source** even though we only recompile two files — those files are written *against* Wireshark's API (`epan_init`, `prefs_set_pref`, …).

A `.so` is a **link target, not a compile input** — you can't `#include` a shared object. The `mobileinsight-libs` `ws-3.4` repo ships only `lib/*.so` (no `include/` tree), so having `libglib-2.0.so` there doesn't solve the compile-time need for glib headers. glib headers come in two flavors: the *portable* ones (`glib.h` & friends, identical everywhere) and the single ABI-specific **`glibconfig.h`**, which must match the arm32 `.so`.

## Generating `glibconfig.h`: `android.cache` + the gettext saga

Cross-compiling `glibconfig.h` is hard only because glib's `configure` wants to *run* test binaries on the target (stack-growth direction, pid handling, etc.) — impossible on an x86 host. The fix is the **`android.cache`** from the upstream MobileInsight glib-for-Android recipe (<https://zwyuan.github.io/2016/07/17/cross-compile-glib-for-android/>, by `zwyuan` — a `ws_dissector/Makefile` author, i.e. the recipe that likely built these very `.so`s). It pre-answers those probes so `configure` reads them from cache, then generates `glibconfig.h` from compile-only checks (sizes, byte order) the cross-compiler *can* resolve.

The base 16 cache values weren't enough — `configure` hit a series of dependency gates. Each is an Android/bionic gap that is **irrelevant to `glibconfig.h`** (which only encodes type sizes and byte order), so we cached past each one:

| `configure` error | Why it happens on Android | Fix added to cache |
|---|---|---|
| No `iconv()` implementation | bionic lacks `iconv_open` until API 28 | `ac_cv_func_iconv_open=yes` |
| No gettext support (jumped straight from `libintl.h`) | bionic has no `libintl.h`; glib's libc-gettext probes are **nested inside** the `libintl.h`-found branch, so a missing header skips them entirely | `ac_cv_header_libintl_h=yes` + `gt_cv_func_dgettext_libc=yes` + `gt_cv_func_ngettext_libc=yes` |
| Still no gettext (after entering the branch) | glib needs a **triad**: `ngettext` **and** `dgettext` **and** `bind_textdomain_codeset` in libc; the third was `no` | `ac_cv_func_bind_textdomain_codeset=yes` |
| `msgfmt... no` → error | `msgfmt` is a genuine **host build tool**, not a target lib | `apt-get install gettext` (not a cache fake) |

Also passed via env vars / flags rather than cache: `LIBFFI_CFLAGS=" " LIBFFI_LIBS=" "` (skip the target-libffi pkg-config check — libffi is only needed to *build* gobject, which we don't), and `--with-pcre=internal --enable-included-printf` (use glib's bundled copies instead of target libs). Result: `configure` completed and `config.status` wrote `glib/glibconfig.h` with `GLIB_SIZEOF_VOID_P 4`, `GLIB_SIZEOF_LONG 4`, `GLIB_SIZEOF_SIZE_T 4`, `G_BYTE_ORDER G_LITTLE_ENDIAN` — a correct 32-bit ARM header.

**Toolchain note:** the VM has **NDK r19b** at `/home/vagrant/android-ndk-r19b` (the standalone GCC toolchain the old `Makefile` references is gone). The clang drivers are per-API-level wrappers (`armv7a-linux-androideabi<API>-clang`); we use API **21**. For `glibconfig.h` the API level is irrelevant (ABI is fixed by target arch), so this matches the `.so`'s lineage fine.

## Two more compile-time snags

- **x86 `config.h`**: the reused `wireshark-3.4.0/config.h` is the desktop build's, and `#define HAVE_SSE4_2 1` made `wsutil/ws_mempbrk.h` include `<emmintrin.h>` → x86 `__builtin_ia32_*` errors on ARM. Fix = un-define it in `config.h` (Wireshark has a portable `ws_mempbrk` fallback). Note: a command-line `-UHAVE_SSE4_2` does **not** work, because the source `#include "config.h"` re-defines it — the edit must be in `config.h` itself.
- **Dead pcap include**: `ws_dissector.cpp` had `#include <pcap.h>` but uses no pcap API (`grep` confirmed the only match is the include line; the `DLT=148` you may remember is a Wireshark *preference string* for the user-DLT table, not the pcap macro). Removed the include rather than expose the whole host `/usr/include` (which would risk the cross-compile picking up host system headers).

## Include-path assembly

glib's *source* layout differs from an installed layout, so the public headers need **two** `-I`s plus one for `gmodule`:
- `-I"$GLIB"` resolves `<glib/...>`; `-I"$GLIB/glib"` resolves `<glib.h>` and our generated `<glibconfig.h>` (both live in `glib/`); `-I"$GLIB/gmodule"` resolves `<gmodule.h>` (a separate sub-library dir), which `wsutil/plugins.h` includes. This mirrors the old `Makefile`'s `-I$(GLIB_SRC) -I$(GLIB_SRC)/glib -I$(GLIB_SRC)/gmodule`.

## Link: deferring glib's newer-libc symbols to the device

The link initially failed with `undefined reference` to `mempcpy`, `feof_unlocked`, `__fsetlocking`, `strtod_l` — **from `libglib-2.0.so`, not our code**. These are newer bionic additions (`mempcpy`@23, `strtod_l`@26/`LIBC_O`, `feof_unlocked`@28) absent from the **API-21 stub libc** we link against. lld's default `--no-allow-shlib-undefined` checks the shared libs' own externals at link time and rejected them.

Rather than assume, we **verified the device**: API **30** (Android 11), ABI list includes `armeabi-v7a`, and pulling its 32-bit `libc.so` and running `llvm-nm -D` showed all four symbols **defined (`T`)**. So they're resolved at runtime by the device's real (newer) libc. Fix = `-Wl,--allow-shlib-undefined`, which only relaxes the check on the shared libraries' internal externals (our own references are still fully checked). We kept the compiler at API 21 (our own code uses only basic libc present since 21); `-static-libstdc++` avoids a runtime `libc++_shared.so` dependency.

---

# On-device verification (Step 5)

**Actual device layout** (on the connected "Nord" phone; base dir is `/data/local/tmp/ws_tester/`, *not* the `/data/local/tmp/mi/` the parent plan sketched):
```
/data/local/tmp/ws_tester/
  android_qc_capture                 (capture pipeline binary)
  android_ws_dissector_probe         (arm64 probe)
  mi2log/                            (raw capture logs)
  out_with_ws.txt                    (decoded output)
  out_with_ws_narrowed.txt           (decoded output — where the 416 failures were seen)
  ws_dissector/
    android_pie_ws_dissector         (canonical name the pipeline invokes)
    android_pie_ws_dissector_v2      (the rebuilt binary — pushed here)
    android_pie_ws_dissector.orig    (backup of original, created during the 5.3 swap)
    lib/                             (8 arm32 .so files)
```

File transfer VM → Mac → device: the VM's `/vagrant` shared folder **is** the Mac dir `~/mobileinsight-mobile_withwireshark/`, and the Mac is the `adb` host.

**5.1 — deploy** (libs already on device, so only the ~18 KB binary is pushed):
```bash
# VM: stage into the shared folder (appears on the Mac at ~/mobileinsight-mobile_withwireshark/)
cp ~/mi-dev/mobileinsight-core/ws_dissector/android_pie_ws_dissector_v2 /vagrant/
# Mac (from ~/mobileinsight-mobile_withwireshark):
adb push android_pie_ws_dissector_v2 /data/local/tmp/ws_tester/
adb shell 'mv /data/local/tmp/ws_tester/android_pie_ws_dissector_v2 /data/local/tmp/ws_tester/ws_dissector/ \
           && chmod 755 /data/local/tmp/ws_tester/ws_dissector/android_pie_ws_dissector_v2'
```

**5.2 — probe = pref-layer gate. ✅ PASSED (2026-07-15).** The probe spawns the binary; the binary exits 1 if `prefs_set_pref` fails, so a `PASS` proves the `null_decipher` pref resolved on-device (and that the deferred libc symbols are present at runtime). Run from `/data/local/tmp/ws_tester/`:
```bash
adb shell
cd /data/local/tmp/ws_tester/
./android_ws_dissector_probe ./ws_dissector/android_pie_ws_dissector_v2 ./ws_dissector/lib/
```
Result: PDML for the 2-byte probe with `aww.proto: 416` and a real `nas-5gs` subtree (EPD 126, Plain NAS message), ending in `PASS`, with **no** `Failed to set nas-5gs.null_decipher.` line. The trailing `[Malformed Packet: NAS-5GS]` is expected (2-byte payload = header only). Confirms startup, on-device linking (`--allow-shlib-undefined` bet), and pref resolution.

**5.3 — full-data proof. ✅ PASSED (2026-07-15).** Swap the fixed binary into the pipeline's canonical name (back up the original), then re-run the capture:
```bash
adb shell
cd /data/local/tmp/ws_tester/ws_dissector
mv android_pie_ws_dissector android_pie_ws_dissector.orig
cp android_pie_ws_dissector_v2 android_pie_ws_dissector
chmod 755 android_pie_ws_dissector
# then, from /data/local/tmp/ws_tester/, re-run android_qc_capture into a new out file
```
`android_qc_capture` regenerates `Diag.cfg` on each run from the 5G `type_names` compiled into [android_qc_capture.cpp:139-143](examples/android_qc_capture.cpp#L139-L143) (`AndroidQcMonitor::setup()` → `generate_diag_cfg()`, overwrite) — so the `../Diag.cfg` arg is a *write target*, not a supplied file; a clean A/B comes from reusing the same `android_qc_capture` binary, not from preserving the file.

**Result** (`out_with_ws_narrowed_v2.txt`): **0** × `fails to decode protocol 416` (buggy baseline `logs_out_with_ws_narrowed.txt` had 36), and the full 5G NAS sequence decodes with real content — `Registration request/accept/complete`, `Authentication request/response`, `Identity request/response`, `Security mode command/complete`, `Configuration update command`, `DL/UL NAS transport`, `Deregistration`. A `Security mode complete` packet dissects end-to-end (decoded `IMEISV`, nested `NAS message container` → `Registration request`), proving the post-security-mode EA0-marked-plain messages now decode. Note: baseline and fixed runs are separate live captures (different signaling, 84 vs 46 packets), so this is a "works now" confirmation, not a byte-identical diff.

---

## Environment / key paths (all three machines)

**Build VM** (`vagrant@vagrant`), everything under `~/mi-dev/mobileinsight-core/` (= `/home/vagrant/mi-dev/mobileinsight-core/`):
- `wireshark-3.4.0/` — pre-configured Wireshark source/headers (has `config.h`)
- `glib-2.54.3/` — downloaded glib source; `android.cache` + generated `glib/glibconfig.h` live here
- `ws_dissector/` — the two `.cpp` files; build output `android_pie_ws_dissector_v2`; prebuilt libs under `android_prebuilt/lib/*.so` (gitignored; `fetch_android_prebuilt.sh`, pinned commit `eda41f7`)
- NDK: `/home/vagrant/android-ndk-r19b`; arm32 clang at `toolchains/llvm/prebuilt/linux-x86_64/bin/armv7a-linux-androideabi21-clang(++)`

**Mac** (`lakshay-MacBookAir`) — the `adb` host, device plugged in here:
- Working dir `~/mobileinsight-mobile_withwireshark/`, which **is** the VM's `/vagrant` shared folder (the VM↔Mac bridge for moving files)

**Device** ("Nord" phone, Android 11 / API 30, `armeabi-v7a` supported):
- Base dir `/data/local/tmp/ws_tester/` (see the layout under "On-device verification")
- Also at `/data/local/tmp/`: `Diag.cfg`, `diag_revealer`, `diag_revealer_fifo`, `libdiag.so`, `adb_bridge`, `android_qc_capture`, `mi2log/` (the capture toolchain)

## Critical files

- `ws_dissector/ws_dissector.cpp` (pref-setting at `:154-161`; removed dead `#include <pcap.h>`), `ws_dissector/packet-aww.cpp` — the two recompiled files
- `ws_dissector/android_prebuilt/lib/*.so` — prebuilt arm32 libs (verified to contain `null_decipher`)
- `~/mi-dev/mobileinsight-core/glib-2.54.3/android.cache` — the 21-line cross-compile cache
- `~/mi-dev/mobileinsight-core/wireshark-3.4.0/config.h` — edited to un-define `HAVE_SSE4_2`
- `examples/android_ws_dissector_probe.cpp` — the go/no-go probe
- <https://zwyuan.github.io/2016/07/17/cross-compile-glib-for-android/> — source of `android.cache`
