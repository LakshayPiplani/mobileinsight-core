CXX      := g++
CXXFLAGS := -std=c++17 -Wall -fPIC -I.
# Cross-compilation switch: overrides CXX/AR/LDFLAGS when TARGET=android.
include ../android.mk


SRCS := serial_port.cpp file_source.cpp diag_fifo_source.cpp
OBJS := $(SRCS:.cpp=.o)
LIB  := libbytes_channel.a

all: $(LIB)

$(LIB): $(OBJS)
	$(AR) rcs $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

serial_port.o:     serial_port.cpp serial_port.h bytes_channel.h
file_source.o:     file_source.cpp file_source.h bytes_channel.h
diag_fifo_source.o: diag_fifo_source.cpp diag_fifo_source.h bytes_channel.h

clean:
	rm -f $(OBJS) $(LIB)
