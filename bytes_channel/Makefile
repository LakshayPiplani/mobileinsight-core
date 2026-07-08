CXX      := g++
CXXFLAGS := -std=c++17 -Wall -fPIC -I.

SRCS := serial_port.cpp
OBJS := $(SRCS:.cpp=.o)
LIB  := libbytes_channel.a

all: $(LIB)

$(LIB): $(OBJS)
	ar rcs $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

serial_port.o: serial_port.cpp serial_port.h bytes_channel.h

clean:
	rm -f $(OBJS) $(LIB)
