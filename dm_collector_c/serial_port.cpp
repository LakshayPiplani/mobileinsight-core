/* serial_port.cpp
 * Implementation of SerialPort.
 * See serial_port.h for the public interface.
 */

#include "serial_port.h"

#include <fcntl.h>     // open(), O_RDWR, O_NOCTTY
#include <unistd.h>    // read(), write(), close()
#include <termios.h>   // tcgetattr(), tcsetattr(), cfmakeraw(), cfsetispeed()
#include <cstring>     // memset()
#include <cstdio>      // perror(), fprintf()

// =============================================================================
// Constructor / Destructor
// =============================================================================

SerialPort::SerialPort() : fd_(-1) {
    // Member initialiser list syntax `: fd_(-1)` is the idiomatic C++ way to
    // set member variables before the constructor body runs.
    // It is equivalent to writing  fd_ = -1;  inside the braces, but faster
    // because it initialises rather than default-constructs then assigns.
}

SerialPort::~SerialPort() {
    // RAII: the destructor guarantees cleanup even if an exception is thrown
    // or the caller forgets to call close() explicitly.
    close();
}

// =============================================================================
// open()
// =============================================================================

bool SerialPort::open(const std::string& path, int baud_rate) {
    // ::open() is the POSIX system call (the leading :: avoids any name
    // collision with our own open() method).
    //
    // Flags:
    //   O_RDWR   – open for both reading and writing
    //   O_NOCTTY – don't make this fd the process's "controlling terminal"
    //              (avoids unwanted signal delivery from the modem)
    fd_ = ::open(path.c_str(), O_RDWR | O_NOCTTY);
    if (fd_ < 0) {
        perror("SerialPort::open");   // prints "SerialPort::open: <errno text>"
        return false;
    }

    // -------------------------------------------------------------------------
    // termios: the POSIX API for configuring serial port parameters.
    // We read the current settings, modify what we need, then write them back.
    // -------------------------------------------------------------------------
    struct termios tty;
    memset(&tty, 0, sizeof(tty));

    if (tcgetattr(fd_, &tty) != 0) {
        perror("SerialPort::open tcgetattr");
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    // cfmakeraw() puts the port in "raw" mode:
    //   – disables canonical (line-buffered) input
    //   – disables echo
    //   – disables signal generation (Ctrl-C etc.)
    //   – passes bytes through exactly as received
    // This matches what Python's serial.Serial does internally.
    cfmakeraw(&tty);

    // Set the baud rate on both the input and output paths.
    speed_t baud = to_baud_constant(baud_rate);
    cfsetispeed(&tty, baud);
    cfsetospeed(&tty, baud);

    // Enable RTS/CTS hardware flow control – matches Python's rtscts=True.
    tty.c_cflag |= CRTSCTS;

    // Apply settings immediately (TCSANOW = "now", no drain/flush).
    if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
        perror("SerialPort::open tcsetattr");
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    return true;
}

// =============================================================================
// close()
// =============================================================================

void SerialPort::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;   // mark as closed so is_open() returns false
    }
}

// =============================================================================
// is_open()
// =============================================================================

bool SerialPort::is_open() const {
    return fd_ >= 0;
}

// =============================================================================
// read()
// =============================================================================

ssize_t SerialPort::read(char* buf, size_t n) {
    // TODO ---------------------------------------------------------------
    // Call the POSIX read() system call and return its result.
    //
    // Prototype (from <unistd.h>):
    //   ssize_t read(int fd, void *buf, size_t count);
    //
    //   fd    – the file descriptor to read from  → use fd_
    //   buf   – pointer to a buffer to fill        → use buf  (already char*)
    //   count – maximum bytes to read              → use n
    //
    // Return value:
    //   > 0  number of bytes actually placed in buf (may be less than n)
    //   = 0  end of file (shouldn't happen on a serial port)
    //   -1   error (errno is set; callers should check)
    //
    // One line of code is enough here.
    return (is_open() ? ::read(fd_, buf, n) : -1);
    // --------------------------------------------------------------------
}

// =============================================================================
// write()
// =============================================================================

ssize_t SerialPort::write(const char* buf, size_t n) {
    // TODO ---------------------------------------------------------------
    // Call the POSIX write() system call and return its result.
    //
    // Prototype (from <unistd.h>):
    //   ssize_t write(int fd, const void *buf, size_t count);
    //
    //   fd    – file descriptor to write to  → use fd_
    //   buf   – data to send                 → use buf
    //   count – number of bytes to send      → use n
    //
    // Return value:
    //   >= 0  bytes actually written (may be less than n on some errors)
    //   -1    error
    //
    // One line of code is enough here.
    return (is_open() ? ::write(fd_, buf, n) : -1);
    // --------------------------------------------------------------------
}

// =============================================================================
// to_baud_constant()  [private]
// =============================================================================

speed_t SerialPort::to_baud_constant(int baud_rate) {
    // termios doesn't accept raw integers for baud rates – it uses symbolic
    // constants defined in <termios.h>.  This function translates between the
    // two worlds.
    switch (baud_rate) {
        case 9600:   return B9600; break;
        case 115200: return B115200; break;
        case 460800: return B460800; break;
        case 921600: return B921600; break;
        case 4000000: return B4000000; break;

        // TODO ---------------------------------------------------------------
        // Add cases for the baud rates the Qualcomm modem actually uses.
        // The symbolic constants follow the pattern B<number>.
        // Common ones you'll need:
        //
        //   115200  → B115200
        //   460800  → B460800
        //   921600  → B921600
        //   4000000 → B4000000   (high-speed Qualcomm DM port)
        //
        // Hint: all of these are defined in <termios.h> on Linux.
        //       Run `grep -r "B460800" /usr/include/` to confirm they exist
        //       on your system.
        // --------------------------------------------------------------------

        default:
            fprintf(stderr,
                    "SerialPort: unknown baud rate %d, falling back to 9600\n",
                    baud_rate);
            return B9600;
    }
}
