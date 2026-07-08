/**
 * @file bytes_channel.h
 * @author Lakshay Piplani (CSE, Penn State)
 * @date 2026-07-06
 * @brief Abstract bidirectional byte channel. Concrete implementations: SerialPort, FifoReader, FileReader.
*/

#pragma once
#include <string>
#include <unistd.h>

class ByteChannel {
public:
    virtual ~ByteChannel() = default;
    virtual bool open() { return false; }
    virtual ssize_t read(char* buf, size_t n) = 0;
    virtual bool write(const char* /*buf*/, size_t /*n*/) { return false; }
    virtual bool is_open() const { return false; }
    virtual void close() {}
};
