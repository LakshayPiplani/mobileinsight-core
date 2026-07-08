CXX      := g++
CXXFLAGS := -std=c++17 -Wall -g -I..

LIBS := ../decoder/qualcomm_decoder/libqualcomm_decoder.a \
        ../export_manager/libexport_manager.a \
        ../bytes_channel/libbytes_channel.a \
        ../bytes_proto/qualcomm/libqualcomm_proto.a \
        ../ws_dissector_client/libws_dissector_client.a

MONITOR_OBJS := ../monitor_c/monitor_base.o ../monitor_c/qualcomm_desktop_monitor.o

# Directories that build the prerequisite libraries / objects, each via its
# own Makefile. Recursed into before linking so a fresh checkout builds with
# just `make -C examples`.
DEPDIRS := ../bytes_proto/qualcomm \
           ../bytes_channel \
           ../export_manager \
           ../ws_dissector_client \
           ../decoder/qualcomm_decoder \
           ../monitor_c

all: replay_mi2log serialtest

.PHONY: deps
deps:
	@for d in $(DEPDIRS); do $(MAKE) -C $$d || exit 1; done

replay_mi2log: replay_mi2log.cpp deps
	$(CXX) $(CXXFLAGS) replay_mi2log.cpp $(MONITOR_OBJS) $(LIBS) -o $@

serialtest: serialtest.cpp deps
	$(CXX) $(CXXFLAGS) serialtest.cpp $(MONITOR_OBJS) $(LIBS) -o $@

clean:
	rm -f replay_mi2log serialtest decoded_packets.txt

# Also clean the prerequisite libraries.
clean-all: clean
	@for d in $(DEPDIRS); do $(MAKE) -C $$d clean || exit 1; done

.PHONY: all clean clean-all
