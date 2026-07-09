/* diag_fifo_source.cpp
 * Implementation of DiagFifoSource. See diag_fifo_source.h for the wire
 * format and design notes.
 */

#include "diag_fifo_source.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

DiagFifoSource::DiagFifoSource(const std::string& fifo_path) : path_(fifo_path) {
}

DiagFifoSource::~DiagFifoSource() {
    close();
}

bool DiagFifoSource::open() {
    // Blocking open+read: matches Python's os.open(path, os.O_RDONLY)
    // ("Blocking mode: save CPU" per the reference comment) -- this call
    // itself blocks until a writer (diag_revealer) opens the other end.
    fd_ = ::open(path_.c_str(), O_RDONLY);
    if (fd_ < 0) {
        perror(("DiagFifoSource::open. Cannot open fifo " + path_).c_str());
        return false;
    }
    return true;
}

void DiagFifoSource::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

bool DiagFifoSource::is_open() const {
    return fd_ >= 0;
}

void DiagFifoSource::consume_byte(unsigned char b, std::string& payload_out) {
    acc_.push_back((char) b);
    if (acc_.size() < need_)
        return;   // field still incomplete

    switch (field_) {
        case F_TYPE: {
            int16_t v;
            memcpy(&v, acc_.data(), sizeof(v));
            msg_type_ = v;
            acc_.clear();
            field_ = F_LEN;
            need_ = sizeof(int16_t);
            return;
        }
        case F_LEN: {
            int16_t v;
            memcpy(&v, acc_.data(), sizeof(v));
            msg_len_ = v;
            acc_.clear();
            if (msg_type_ == TYPE_LOG) {
                field_ = F_TS;
                need_ = sizeof(double);
            } else if (msg_type_ == TYPE_START_LOG_FILE || msg_type_ == TYPE_END_LOG_FILE) {
                field_ = F_FILENAME;
                need_ = (size_t) (msg_len_ > 0 ? msg_len_ : 0);
                if (need_ == 0) {
                    // Empty filename: nothing more to accumulate for this
                    // record; go straight back to a fresh header.
                    field_ = F_TYPE;
                    need_ = sizeof(int16_t);
                }
            } else {
                // Unknown record type: diag_revealer only ever emits
                // TYPE_LOG/START/END, so this should not happen in
                // practice. Reset to header rather than risk hanging on
                // an unbounded/garbage field length.
                fprintf(stderr, "DiagFifoSource: unknown chronicle msg_type %d, resyncing\n",
                        (int) msg_type_);
                field_ = F_TYPE;
                need_ = sizeof(int16_t);
            }
            return;
        }
        case F_TS: {
            double ts;
            memcpy(&ts, acc_.data(), sizeof(ts));
            last_ts_ = ts;
            acc_.clear();
            field_ = F_PAYLOAD;
            need_ = (size_t) (msg_len_ - (int) sizeof(double));
            if ((int) need_ <= 0) {
                // Degenerate zero-length payload: nothing to hand back,
                // but the record is otherwise complete.
                field_ = F_TYPE;
                need_ = sizeof(int16_t);
            }
            return;
        }
        case F_PAYLOAD: {
            payload_out.append(acc_);
            acc_.clear();
            field_ = F_TYPE;
            need_ = sizeof(int16_t);
            return;
        }
        case F_FILENAME: {
            // Log-file-rotation notice (diag_revealer started/ended a
            // local .mi2log file). No decoder-relevant payload; just
            // resync to the next header. (A future enhancement could
            // surface this via a callback, matching Python's
            // "new_diag_log" event -- not needed for basic decoding.)
            acc_.clear();
            field_ = F_TYPE;
            need_ = sizeof(int16_t);
            return;
        }
    }
}

ssize_t DiagFifoSource::read(char* buf, size_t n) {
    if (fd_ < 0)
        return -1;

    // Drain anything already unwrapped-but-undelivered before blocking on
    // more fifo data -- see header comment: a single chronicle record's
    // payload can span several read() calls when the caller's buffer is
    // smaller than the payload (MonitorBase::run() uses 64 bytes).
    while (pending_.empty()) {
        unsigned char raw[256];
        ssize_t got = ::read(fd_, raw, sizeof(raw));
        if (got < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (got == 0)
            return 0;   // true EOF: diag_revealer closed its end of the fifo

        for (ssize_t i = 0; i < got; i++)
            consume_byte(raw[i], pending_);   // appends any completed payload(s)
    }

    size_t to_copy = pending_.size() < n ? pending_.size() : n;
    memcpy(buf, pending_.data(), to_copy);
    pending_.erase(0, to_copy);
    return (ssize_t) to_copy;
}
