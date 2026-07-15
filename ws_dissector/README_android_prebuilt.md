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

## Don't confuse this with `ws_dissector/Makefile`'s `android` target

That Makefile has its own `android`/`android_pie_ws_dissector` build rules —
a **from-scratch NDK cross-compile** of Wireshark + glib + libpcap for
32-bit ARM, using a Vagrant environment (`github.com/luckiday/wireshark-for-android`)
nobody has run recently. It produces same-named output files, but in a
different directory (`ws_dissector/` itself, not `ws_dissector/android_prebuilt/`).
This README's directory is **fetched prebuilt**, not built here — don't mix
the two up.
