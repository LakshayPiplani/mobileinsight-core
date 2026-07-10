# android.mk — opt-in Android cross-compilation, shared by every component
# Makefile (each does `include <path>/android.mk` right after its own
# CXX/CXXFLAGS defaults; assignments here override them when active).
#
# Usage, from any component dir or examples/:
#   make TARGET=android [NDK=/path/to/android-ndk] [API=21]
#
# TARGET/NDK/API given on the command line propagate automatically to the
# recursive sub-makes (examples' `deps` loop) via MAKEFLAGS.
#
# Targets arm64-v8a / android-21 — the same pair as diag_revealer's
# jni/Application.mk, so both binaries run on the same device.
#
# NDK lookup order: explicit NDK=… > $ANDROID_NDK_HOME > $ANDROID_NDK_ROOT.
#
# NOTE: objects/libs/binaries share file names between host and Android
# builds. When switching targets run `make -C examples clean-all` first, or
# the link will mix stale objects of the wrong architecture.

# Host-build defaults (the ordinary `make` path).
AR      ?= ar
LDFLAGS ?=

ifeq ($(TARGET),android)
  NDK ?= $(or $(ANDROID_NDK_HOME),$(ANDROID_NDK_ROOT))
  ifeq ($(strip $(NDK)),)
    $(error TARGET=android: no NDK found. Pass NDK=/path/to/android-ndk or set ANDROID_NDK_HOME)
  endif

  API ?= 21
  TOOLCHAIN := $(NDK)/toolchains/llvm/prebuilt/linux-x86_64/bin

  # Target-prefixed clang wrapper (NDK r19+): bakes in --target and sysroot.
  CXX := $(TOOLCHAIN)/aarch64-linux-android$(API)-clang++
  ifeq ($(wildcard $(CXX)),)
    $(error TARGET=android: $(CXX) not found -- check NDK path and API level (try `ls $(TOOLCHAIN) | grep aarch64`))
  endif

  # Archiver: llvm-ar if this NDK ships it, else the GNU binutils ar that
  # r19 keeps under toolchains/<abi>-4.9, else the host ar (archiving is
  # architecture-agnostic, so this last resort still works).
  AR := $(firstword \
          $(wildcard $(TOOLCHAIN)/llvm-ar) \
          $(wildcard $(NDK)/toolchains/aarch64-linux-android-4.9/prebuilt/linux-x86_64/bin/aarch64-linux-android-ar) \
          ar)

  # Static libc++ so pushed binaries have zero runtime .so dependencies on
  # the phone (bionic itself is always present).
  LDFLAGS += -static-libstdc++

  # log_packet_helper.h redefines printf() to __android_log_print() whenever
  # __ANDROID__ is defined (true for every NDK clang invocation, unlike the
  # host build) -- a native binary has nowhere to send stdout debug prints
  # on-device anyway, so this redirects them to logcat instead. liblog.so
  # provides that symbol.
  LDFLAGS += -llog

  $(info [android.mk] cross-compiling: CXX=$(CXX))
endif
