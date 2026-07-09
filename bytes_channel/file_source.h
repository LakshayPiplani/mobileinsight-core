/* file_source.h
 * A plain-file ByteChannel: reads a .mi2log/.qmdl capture from disk.
 * Replaces Python's `open(path, "rb")` + manual .read(64) in
 * offline_replayer.py for the offline-replay path.
 */

#pragma once

#include <cstdio>
#include <string>
#include "bytes_channel.h"

class FileSource : public ByteChannel {
public:
    explicit FileSource(const std::string& path);   // sets fp_ = nullptr (nothing open yet)
    ~FileSource() override;   // calls close() so we never leak a FILE*

    // Open `path` for reading. Returns true on success.
    bool open() override;

    // Close the file if it is open. Safe to call when already closed.
    void close() override;

    // Returns true when a file is currently open.
    bool is_open() const override;

    // Read up to `n` bytes into `buf`. Not blocking: at EOF this returns 0,
    // which is exactly the signal MonitorBase::run() already treats as
    // "source exhausted, stop the loop" -- no offline-specific handling
    // needed there.
    // Returns the number of bytes actually read, 0 at EOF, or -1 on error.
    ssize_t read(char* buf, size_t n) override;

    // write() is intentionally not overridden: a file source never needs to
    // send config commands, so the ByteChannel default (returns false) is
    // correct -- OfflineReplayer::setup() never calls it.

private:
    std::string path_;
    FILE* fp_;
};
