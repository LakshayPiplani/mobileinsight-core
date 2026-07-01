/* serial_port.h
 * A thin RAII wrapper around a POSIX serial port file descriptor.
 * Replaces Python's serial.Serial for the hot path in dm_collector.
 */

// #pragma once is a non-standard but universally supported alternative to
// the traditional #ifndef/#define/#endif include guard.  It tells the
// compiler: "only process this file once per translation unit."
#pragma once

#include <string>
#include <termios.h>   // struct termios, speed_t, B9600 …
#include <unistd.h>    // ssize_t

class SerialPort {
public:
    // Constructor/destructor -------------------------------------------------

    SerialPort();   // sets fd_ = -1 (nothing open yet)
    ~SerialPort();  // calls close() so we never leak an fd

    // Public interface -------------------------------------------------------

    // Open the device at `path` (e.g. "/dev/ttyUSB0") at `baud_rate` bps.
    // Configures RTS/CTS hardware flow control to match Python's rtscts=True.
    // Returns true on success.
    bool open(const std::string& path, int baud_rate);

    // Close the file descriptor if it is open.
    void close();

    // Returns true when a port is currently open.
    bool is_open() const;

    // Returns the raw file descriptor. Needed by run_loop in dm_collector_c.cpp
    // to pass to Py_BEGIN_ALLOW_THREADS / ::read() directly.
    int fd() const { return fd_; }

    // Read up to `n` bytes into `buf`.
    // This is a blocking call: it will not return until at least 1 byte
    // arrives (or an error occurs).
    // Returns the number of bytes actually read, or -1 on error.
    ssize_t read(char* buf, size_t n);

    // Write `n` bytes from `buf` to the serial port.
    // Returns the number of bytes written, or -1 on error.
    ssize_t write(const char* buf, size_t n);

private:
    // The POSIX file descriptor.  -1 means "not open".
    int fd_;

    // Converts a human-readable baud rate (e.g. 9600) to the termios
    // constant the kernel expects (e.g. B9600).
    // This is private because callers deal in plain ints, not termios magic.
    static speed_t to_baud_constant(int baud_rate);
};
