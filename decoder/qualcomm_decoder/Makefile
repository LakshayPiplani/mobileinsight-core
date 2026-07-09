CXX      := g++
CXXFLAGS := -std=c++17 -Wall -fPIC -I. -I../../bytes_proto/qualcomm
# Cross-compilation switch: overrides CXX/AR/LDFLAGS when TARGET=android.
include ../../android.mk


# dm_collector_c.cpp is the legacy CPython extension — excluded; it is
# deleted at Step 6 of the parent migration plan.
SRCS := log_packet.cpp qualcomm_decoder.cpp
OBJS := $(SRCS:.cpp=.o)
LIB  := libqualcomm_decoder.a

all: $(LIB)

$(LIB): $(OBJS)
	$(AR) rcs $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

log_packet.o:        log_packet.cpp log_packet.h log_packet_helper.h \
                     ../field_value.h
qualcomm_decoder.o:  qualcomm_decoder.cpp qualcomm_decoder.h log_packet.h \
                     ../decoder.h ../field_value.h \
                     ../../export_manager/export_manager.h \
                     ../../bytes_proto/qualcomm/hdlc.h \
                     ../../bytes_proto/qualcomm/utils.h \
                     ../../bytes_proto/qualcomm/consts.h

clean:
	rm -f $(OBJS) $(LIB)

.PHONY: all clean