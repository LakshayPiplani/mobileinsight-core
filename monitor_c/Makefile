CXX      := g++
# No Python includes needed: the decoder layer is fully PyObject-free.
CXXFLAGS := -std=c++17 -Wall -Wextra -fPIC -I..
# Cross-compilation switch: overrides CXX/AR/LDFLAGS when TARGET=android.
include ../android.mk


SRCS := monitor_base.cpp qualcomm_desktop_monitor.cpp offline_replayer.cpp android_qc_monitor.cpp
OBJS := $(SRCS:.cpp=.o)

all: $(OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

monitor_base.o:             monitor_base.cpp monitor_base.h \
                            monitor_config.h ../decoder/qualcomm_decoder/qualcomm_decoder.h
qualcomm_desktop_monitor.o: qualcomm_desktop_monitor.cpp qualcomm_desktop_monitor.h \
                            monitor_base.h ../bytes_channel/serial_port.h \
                            ../bytes_proto/qualcomm/hdlc.h ../bytes_proto/qualcomm/log_config.h
offline_replayer.o:         offline_replayer.cpp offline_replayer.h \
                            monitor_base.h ../bytes_channel/file_source.h \
                            ../decoder/qualcomm_decoder/qualcomm_decoder.h \
                            ../bytes_proto/qualcomm/consts.h
android_qc_monitor.o:       android_qc_monitor.cpp android_qc_monitor.h \
                            monitor_base.h ../bytes_channel/diag_fifo_source.h \
                            ../decoder/qualcomm_decoder/qualcomm_decoder.h \
                            ../bytes_proto/qualcomm/hdlc.h ../bytes_proto/qualcomm/log_config.h \
                            ../bytes_proto/qualcomm/consts.h

clean:
	rm -f $(OBJS)
