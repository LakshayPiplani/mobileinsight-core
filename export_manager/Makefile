CXX      := g++
CXXFLAGS := -std=c++17 -Wall -fPIC -I.
# Cross-compilation switch: overrides CXX/AR/LDFLAGS when TARGET=android.
include ../android.mk


SRCS := export_manager.cpp
OBJS := $(SRCS:.cpp=.o)
LIB  := libexport_manager.a

all: $(LIB)

$(LIB): $(OBJS)
	$(AR) rcs $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

export_manager.o: export_manager.cpp export_manager.h \
                  ../bytes_proto/qualcomm/utils.h ../bytes_proto/qualcomm/hdlc.h

clean:
	rm -f $(OBJS) $(LIB)
