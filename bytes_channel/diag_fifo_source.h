/* diag_fifo_source.h
 * Reads Qualcomm DIAG log data relayed over a FIFO by the `diag_revealer`
 * Android helper (root-only; reads /dev/diag on-device). Replaces Python's
 * ChronicleProcessor + manual os.read(fifo) loop in android_dev_diag_monitor.py.
 *
 * Wire format (see diag_revealer.c's write-to-fifo loop): each record is
 *   [type:int16 LE][len:int16 LE]
 * followed by, for a log record (type == TYPE_LOG):
 *   [timestamp:double LE][payload: len-8 bytes]
 * or for a log-file-rotation record (type == TYPE_START/END_LOG_FILE):
 *   [filename: len bytes]
 *
 * The `payload` bytes are genuine HDLC-framed diag frames (0x7e-delimited,
 * CRC16-suffixed) -- diag_revealer relays them verbatim from /dev/diag, it
 * does not re-encode them. So DiagFifoSource hands unwrapped payload bytes
 * straight to Decoder::feed(), exactly like SerialPort does; no decoder-side
 * bypass is needed. (Confirmed by cross-referencing diag_revealer.c's
 * fwrite() of the identical buf_read+offset+4/msg_len bytes to its own
 * on-disk .mi2log writer against files from this exact pipeline that our
 * HDLC-based QualcommDecoder has already decoded correctly.)
 */

#pragma once

#include <cstdint>
#include <string>
#include "bytes_channel.h"

class DiagFifoSource : public ByteChannel {
public:
    explicit DiagFifoSource(const std::string& fifo_path);
    ~DiagFifoSource() override;

    // Open the fifo path for reading (blocking). The fifo node itself
    // (mknod) is the caller's responsibility -- AndroidQcMonitor::setup()
    // creates it and spawns diag_revealer before calling this, mirroring
    // Python's _mkfifo() + _start_diag_revealer() ordering.
    bool open() override;
    void close() override;
    bool is_open() const override;

    // Returns unwrapped diag payload bytes, chronicle framing stripped
    // internally. A single chronicle record's payload can be much larger
    // than the caller's buffer (MonitorBase::run() reads 64 bytes at a
    // time), so bytes are queued internally and drained across multiple
    // read() calls -- exactly like a raw stream source (SerialPort);
    // frame boundaries are re-established downstream by the HDLC decoder,
    // not by this class. Loops across the underlying raw fifo reads until
    // it has bytes to report or hits a terminal condition -- never
    // returns 0 merely because one raw read happened to land on a partial
    // chronicle field (routine: diag_revealer issues 4 separate write()
    // calls per log record, so a reader can genuinely see e.g. just the
    // 2-byte type field on its own). Returns 0 only on true fifo EOF
    // (writer closed its end), -1 on error.
    ssize_t read(char* buf, size_t n) override;

    // Most recent per-record timestamp reported by diag_revealer (POSIX
    // seconds), or -1.0 if none seen yet. Informational only -- the
    // decoder timestamps packets itself; exposed for parity with Python's
    // get_last_diag_revealer_ts().
    double last_diag_revealer_ts() const { return last_ts_; }

private:
    static const int16_t TYPE_LOG             = 1;
    static const int16_t TYPE_START_LOG_FILE  = 2;
    static const int16_t TYPE_END_LOG_FILE    = 3;

    enum Field { F_TYPE, F_LEN, F_TS, F_PAYLOAD, F_FILENAME };

    std::string path_;
    int fd_ = -1;
    double last_ts_ = -1.0;

    // Unwrapped payload bytes ready to hand to the caller but not yet
    // delivered, because a prior read() call's buffer was smaller than
    // what was available. Drained front-first by read().
    std::string pending_;

    // Chronicle state machine, mirrors ChronicleProcessor in
    // android_dev_diag_monitor.py: accumulate `need_` more bytes into
    // `acc_` for the current field, then act once it's complete.
    Field field_ = F_TYPE;
    int16_t msg_type_ = 0;
    int msg_len_ = 0;       // "len" from the wire header
    std::string acc_;
    size_t need_ = sizeof(int16_t);

    // Feed one raw fifo byte through the state machine. Appends to
    // `payload_out` whenever a LOG record's payload completes (may
    // append more than once if `payload_out` accumulates across many
    // calls -- callers just keep calling this per raw byte). Non-payload
    // records (START/END_LOG_FILE) and in-progress fields leave
    // `payload_out` untouched.
    void consume_byte(unsigned char b, std::string& payload_out);
};
