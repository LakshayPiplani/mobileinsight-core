# Android prebuilt `ws_dissector` artifacts

`ws_dissector/android_prebuilt/` (gitignored, not committed) holds the files
`fetch_android_prebuilt.sh` downloads:

```
android_prebuilt/
  bin/android_pie_ws_dissector
  lib/libgio-2.0.so libglib-2.0.so libgobject-2.0.so libgmodule-2.0.so
      libgthread-2.0.so libwsutil.so libwireshark.so libwiretap.so
```

Run `./fetch_android_prebuilt.sh` from this directory to (re)populate it.

## Where these come from

`fetch_android_prebuilt.sh` clones a **pinned commit**
(`eda41f72b56341e3ab44c3c5847eb9661cb2644f`, 2020-11-21) of the `ws-3.4`
branch of `github.com/mobile-insight/mobileinsight-libs`, and aborts if the
branch tip has moved past that commit — so we never silently vendor a
different build than the one this was validated against. It extracts
`lib.tar.gz` and copies out only `android_pie_ws_dissector` (skipping the
non-PIE `android_ws_dissector` twin and the unrelated `tcpdump` binary also
present in that repo).

## Why 32-bit ARM is fine

All 9 files are ELF 32-bit ARM (armeabi-v7a), not arm64 — a mismatch against
the rest of this project's Android build (`android.mk`, arm64-v8a via a
modern NDK). This doesn't matter here: `ws_dissector` runs as a completely
separate child process, spoken to only over pipes (the AWW wire protocol in
`ws_dissector_client/`), never linked in-process with our arm64 code. Any
current arm64-v8a Android device runs 32-bit ARM binaries fine via multilib;
this would only become a problem if a future device dropped 32-bit support
entirely.

Wireshark version: **3.4.0** (matches the desktop build's `install-ubuntu.sh`
baseline, not the stale `3.2.7` string in `ws_dissector.cpp`). No gnutls/gcrypt
linked — TLS/crypto dissection is unsupported in this build.

## Prebuilt libs (here) vs. building the binary (`build-android.sh`)

This directory holds **fetched prebuilt** artifacts. It is *not* where the
`ws_dissector` binary is built. Two related-but-distinct things:

- **`build-android.sh`** (in `ws_dissector/`) — the current Android build
  entrypoint. It recompiles only the two `.cpp` wrapper files with NDK clang and
  **links against the prebuilt `.so`s in this directory**, emitting the binary to
  `ws_dissector/android_build/`. This is the validated path (see
  `CLAUDE_ANDROID_WS_BUILD.md`).
- **`ws_dissector/Makefile`'s stale `android` target** — an old from-scratch NDK
  cross-compile of Wireshark + glib + libpcap via a Vagrant env
  (`github.com/luckiday/wireshark-for-android`) that nobody has run recently and
  that predates `build-android.sh`. Ignore it in favor of `build-android.sh`.
