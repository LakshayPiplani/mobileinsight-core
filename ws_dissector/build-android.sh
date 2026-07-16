#!/usr/bin/env bash
#
# build-android.sh — cross-compile ws_dissector for Android (arm32 / armeabi-v7a).
#
# This is the ANDROID build entrypoint and is deliberately kept separate from the
# DESKTOP build (Makefile `ws_dissector` target + install-ubuntu.sh). The two share
# the same source (ws_dissector.cpp, packet-aww.cpp) but nothing else:
#
#                        DESKTOP                         ANDROID (this script)
#   toolchain            system g++/clang                NDK r19b clang (arm32)
#   glib                 system, via pkg-config          source headers + generated arm32 glibconfig.h
#   wireshark libs       locally built /usr/local/lib    prebuilt arm32 .so (android_prebuilt/lib)
#   config.h             native (correct for x86)        derived copy with x86-only HAVE_SSE4_2 off
#   output               /usr/local/bin/ws_dissector     ws_dissector/android_build/
#
# Rule of thumb: shared = source + the abstract API; per-platform = toolchain, ABI,
# config, and libraries. This script owns the second column for Android; it never
# mutates the shared Wireshark source tree.
#
# See CLAUDE_ANDROID_WS_BUILD.md for the full reasoning behind each step.
set -euo pipefail

# --- configurable inputs (defaults match the mobileinsight build VM) ---------
: "${NDK:=/home/vagrant/android-ndk-r19b}"
: "${API:=21}"                                              # armeabi-v7a API level
: "${WS_SRC:=$HOME/mi-dev/mobileinsight-core/wireshark-3.4.0}"
: "${GLIB_SRC:=$HOME/mi-dev/mobileinsight-core/glib-2.54.3}"

HERE="$(cd "$(dirname "$0")" && pwd)"                       # ws_dissector/
NDK_BIN="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin"
CC="$NDK_BIN/armv7a-linux-androideabi${API}-clang"
CXX="$NDK_BIN/armv7a-linux-androideabi${API}-clang++"
BUILD_DIR="$HERE/android_build"                            # dedicated output dir (task d)
OUT="$BUILD_DIR/android_pie_ws_dissector"

# --- sanity checks -----------------------------------------------------------
[ -x "$CXX" ]        || { echo "ERROR: NDK clang not found at $CXX (set \$NDK / \$API)"          >&2; exit 1; }
[ -f "$WS_SRC/config.h" ] || { echo "ERROR: Wireshark tree not found at \$WS_SRC=$WS_SRC"        >&2; exit 1; }
[ -d "$GLIB_SRC/glib" ]   || { echo "ERROR: glib source not found at \$GLIB_SRC=$GLIB_SRC"       >&2; exit 1; }
mkdir -p "$BUILD_DIR"

# --- 1. arm32 glibconfig.h (generate once, from the cross-compile cache) ------
# glib's configure can't RUN target test-binaries on an x86 host; android.cache
# pre-answers those probes so configure can still emit a correct arm32 glibconfig.h.
# (The extra iconv/gettext lines get past bionic dependency gates that don't affect
#  glibconfig.h itself — see the doc.)
if [ ! -f "$GLIB_SRC/glib/glibconfig.h" ]; then
    command -v msgfmt >/dev/null \
        || { echo "ERROR: msgfmt missing; run: sudo apt-get install -y gettext" >&2; exit 1; }
    echo "[build-android] generating arm32 glibconfig.h ..."
    cat > "$GLIB_SRC/android.cache" <<'EOF'
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
EOF
    ( cd "$GLIB_SRC" \
      && CC="$CC" LIBFFI_CFLAGS=" " LIBFFI_LIBS=" " PATH="$NDK_BIN:$PATH" \
         ./configure --host=arm-linux-androideabi --build=x86_64-linux-gnu \
            --cache-file=android.cache \
            --with-pcre=internal --enable-included-printf --disable-dependency-tracking )
fi
grep -q 'GLIB_SIZEOF_VOID_P 4' "$GLIB_SRC/glib/glibconfig.h" \
    || { echo "ERROR: glibconfig.h is not the 32-bit ARM one (VOID_P != 4)" >&2; exit 1; }

# --- 2. Android config.h WITHOUT mutating the shared Wireshark tree (task b) ---
# The reused config.h is the x86-64 desktop one; HAVE_SSE4_2 drags in Intel SIMD
# intrinsics (<emmintrin.h>) that don't exist on ARM. We derive an Android copy with
# it turned off and put $BUILD_DIR first on the include path so it SHADOWS the
# Wireshark one — the shared tree stays pristine. (A command-line -U/-D can't win,
# because the source #include "config.h" re-defines it.)
sed 's|^#define HAVE_SSE4_2 1|/* #undef HAVE_SSE4_2 (x86-only; off for ARM) */|' \
    "$WS_SRC/config.h" > "$BUILD_DIR/config.h"

# --- 3. prebuilt arm32 .so's to link against ---------------------------------
# These are the pinned, validated libs that already contain the null_decipher pref.
[ -d "$HERE/android_prebuilt/lib" ] || "$HERE/fetch_android_prebuilt.sh"

# --- 4. compile + link -------------------------------------------------------
# -fPIE -pie          : Android API 21+ requires position-independent executables
# -static-libstdc++   : bundle the tiny C++ runtime -> no libc++_shared.so on device
# -I"$BUILD_DIR" first: our config.h shadows Wireshark's (see step 2)
# two glib -I's       : source layout needs both the root (<glib/..>) and glib/ (<glib.h>, <glibconfig.h>)
# --allow-shlib-undefined: libglib's newer-bionic symbols (mempcpy/strtod_l/...) are
#                          resolved at runtime by the device's libc, not the API-21 stub
echo "[build-android] compiling ..."
"$CXX" -fPIE -pie -static-libstdc++ \
    "$HERE/ws_dissector.cpp" "$HERE/packet-aww.cpp" \
    -I"$BUILD_DIR" -I"$WS_SRC" -I"$GLIB_SRC" -I"$GLIB_SRC/glib" -I"$GLIB_SRC/gmodule" \
    -L"$HERE/android_prebuilt/lib" \
    -lwireshark -lwiretap -lwsutil -lglib-2.0 \
    -Wl,--allow-shlib-undefined \
    -o "$OUT"

# --- 5. sanity-verify the built binary ---------------------------------------
strings "$OUT" | grep -q 'nas-5gs.null_decipher:TRUE' \
    || { echo "ERROR: built binary is missing the null_decipher fix" >&2; exit 1; }

echo "[build-android] OK -> $OUT"
echo "[build-android] push it as the device's android_pie_ws_dissector (see CLAUDE_ANDROID_WS_BUILD.md Step 5)."
