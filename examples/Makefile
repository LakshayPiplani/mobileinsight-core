CXX      := g++
CXXFLAGS := -std=c++17 -Wall -g -I..
# Cross-compilation switch: overrides CXX/AR/LDFLAGS when TARGET=android
# (propagates to the deps recursion below via MAKEFLAGS). Remember to
# `make clean-all` when switching between host and android builds.
include ../android.mk

LIBS := ../decoder/qualcomm_decoder/libqualcomm_decoder.a \
        ../export_manager/libexport_manager.a \
        ../bytes_channel/libbytes_channel.a \
        ../bytes_proto/qualcomm/libqualcomm_proto.a \
        ../ws_dissector_client/libws_dissector_client.a

MONITOR_OBJS := ../monitor_c/monitor_base.o ../monitor_c/qualcomm_desktop_monitor.o \
                ../monitor_c/offline_replayer.o ../monitor_c/android_qc_monitor.o

# Directories that build the prerequisite libraries / objects, each via its
# own Makefile. Recursed into before linking so a fresh checkout builds with
# just `make -C examples`.
DEPDIRS := ../bytes_proto/qualcomm \
           ../bytes_channel \
           ../export_manager \
           ../ws_dissector_client \
           ../decoder/qualcomm_decoder \
           ../monitor_c

all: replay_mi2log serialtest android_qc_capture android_ws_dissector

.PHONY: deps
deps:
	@for d in $(DEPDIRS); do $(MAKE) -C $$d || exit 1; done

replay_mi2log: replay_mi2log.cpp deps
	$(CXX) $(CXXFLAGS) replay_mi2log.cpp $(MONITOR_OBJS) $(LIBS) $(LDFLAGS) -o $@

serialtest: serialtest.cpp deps
	$(CXX) $(CXXFLAGS) serialtest.cpp $(MONITOR_OBJS) $(LIBS) $(LDFLAGS) -o $@

android_qc_capture: android_qc_capture.cpp deps
	$(CXX) $(CXXFLAGS) android_qc_capture.cpp $(MONITOR_OBJS) $(LIBS) $(LDFLAGS) -o $@

android_ws_dissector_probe: android_ws_dissector_probe.cpp deps
        $(CXX) $(CXXFLAGS) android_ws_dissector_probe.cpp ../ws_dissector_client/libws_dissector_client.a $(LDFLAGS) -o $@

clean:
	rm -f replay_mi2log serialtest android_qc_capture decoded_packets.txt android_ws_dissector

# Also clean the prerequisite libraries.
clean-all: clean
	@for d in $(DEPDIRS); do $(MAKE) -C $$d clean || exit 1; done

.PHONY: all clean clean-all
