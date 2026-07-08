CXX      := g++
CXXFLAGS := -std=c++17 -Wall -fPIC -I.

SRCS := ws_dissector_client.cpp
OBJS := $(SRCS:.cpp=.o)
LIB  := libws_dissector_client.a

all: $(LIB)

$(LIB): $(OBJS)
	ar rcs $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

ws_dissector_client.o: ws_dissector_client.cpp ws_dissector_client.h

clean:
	rm -f $(OBJS) $(LIB)

.PHONY: all clean
