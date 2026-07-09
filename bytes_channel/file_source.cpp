/* file_source.cpp
 * Implementation of FileSource.
 * See file_source.h for the public interface.
 */

#include "file_source.h"

FileSource::FileSource(const std::string& path) : path_(path), fp_(nullptr) {
}

FileSource::~FileSource() {
    close();
}

bool FileSource::open() {
    fp_ = fopen(path_.c_str(), "rb");
    if (!fp_) {
        perror(("FileSource::open. Cannot open " + path_).c_str());
        return false;
    }
    return true;
}

void FileSource::close() {
    if (fp_) {
        fclose(fp_);
        fp_ = nullptr;
    }
}

bool FileSource::is_open() const {
    return fp_ != nullptr;
}

ssize_t FileSource::read(char* buf, size_t n) {
    if (!fp_)
        return -1;
    return (ssize_t) fread(buf, 1, n, fp_);   // 0 at EOF; fread doesn't block
}
