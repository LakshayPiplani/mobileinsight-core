/* serial_port.cpp
 * Implementation of SerialPort.
 * See serial_port.h for the public interface.
 */

#include "serial_port.h"

#include <fcntl.h>     // open(), O_RDWR, O_NOCTTY
#include <unistd.h>    // read(), write(), close()
#include <termios.h>   // tcgetattr(), tcsetattr(), cfmakeraw(), cfsetispeed()
#include <sys/ioctl.h> // ioctl(), TIOCMBIS, TIOCM_DTR, TIOCM_RTS
#include <cstring>     // memset()
#include <cstdio>      // perror(), fprintf()

// =============================================================================
// Constructor / Destructor
// =============================================================================

SerialPort::SerialPort(const std::string& path, int baud_rate) : serial_path_(path), baud_rate_(baud_rate), fd_(-1) {
}

SerialPort::~SerialPort() {
    // RAII: the destructor guarantees cleanup even if an exception is thrown
    // or the caller forgets to call close() explicitly.
    close();
}

// =============================================================================
// open()
// =============================================================================

bool SerialPort::open() {
    // ::open() is the POSIX system call (the leading :: avoids any name
    // collision with our own open() method).
    //
    // Flags:
    //   O_RDWR   – open for serial_path_both reading and writing
    //   O_NOCTTY – don't make this fd the process's "controlling terminal"
    //              (avoids unwanted signal delivery from the modem)
    fd_ = ::open(serial_path_.c_str(), O_RDWR | O_NOCTTY);
    if (fd_ < 0) {
        perror("SerialPort::open. Cannot open Serial Port.");   // prints "SerialPort::open: <errno text>"
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
    speed_t baud = to_baud_constant(baud_rate_);
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

    // Assert DTR + RTS ("terminal ready"). pyserial's Serial() raises both
    // lines by default on open (dtr=True, rts=True are the defaults; the
    // original tool additionally passed dsrdtr=True) — bare POSIX open() +
    // termios does not do this on its own. Many USB DIAG/modem ports gate
    // whether they actually start streaming unsolicited log traffic on DTR
    // being asserted, even though basic command/response traffic can still
    // flow without it — so config ACKs can work while no logs ever appear.
    int status = 0;
    if (ioctl(fd_, TIOCMGET, &status) == 0) {
        status |= TIOCM_DTR | TIOCM_RTS;
        if (ioctl(fd_, TIOCMSET, &status) != 0)
            perror("SerialPort::open ioctl(TIOCMSET) - failed to assert DTR/RTS");
    } else {
        perror("SerialPort::open ioctl(TIOCMGET)");
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
    return (is_open() ? ::read(fd_, buf, n) : -1);
}

// =============================================================================
// write()
// =============================================================================

bool SerialPort::write(const char* buf, size_t n) {
    return (is_open() ? ::write(fd_, buf, n) >= 0 : false);
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

        default:
            fprintf(stderr,
                    "SerialPort: unknown baud rate %d, falling back to 9600\n",
                    baud_rate);
            return B9600;
    }
}
