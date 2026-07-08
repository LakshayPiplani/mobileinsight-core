CXX      := g++
CXXFLAGS := -std=c++17 -Wall -fPIC -I.

SRCS := export_manager.cpp
OBJS := $(SRCS:.cpp=.o)
LIB  := libexport_manager.a

all: $(LIB)

$(LIB): $(OBJS)
	ar rcs $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

export_manager.o: export_manager.cpp export_manager.h \
                  ../bytes_proto/qualcomm/utils.h ../bytes_proto/qualcomm/hdlc.h

clean:
	rm -f $(OBJS) $(LIB)
