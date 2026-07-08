CXX      := g++
CXXFLAGS := -std=c++17 -Wall -I..

LIBS := ../decoder/qualcomm_decoder/libqualcomm_decoder.a \
        ../export_manager/libexport_manager.a \
        ../bytes_channel/libbytes_channel.a \
        ../bytes_proto/qualcomm/libqualcomm_proto.a \
        ../ws_dissector_client/libws_dissector_client.a

MONITOR_OBJS := ../monitor_c/monitor_base.o ../monitor_c/qualcomm_desktop_monitor.o

all: replay_mi2log

replay_mi2log: replay_mi2log.cpp $(MONITOR_OBJS) $(LIBS)
	$(CXX) $(CXXFLAGS) $< $(MONITOR_OBJS) $(LIBS) -o $@

clean:
	rm -f replay_mi2log decoded_packets.txt

.PHONY: all clean
