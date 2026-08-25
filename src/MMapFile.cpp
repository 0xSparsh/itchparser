#include "MMapFile.hpp"

#include <cerrno>   // For errno
#include <cstdio>   // For fprintf
#include <cstring>  // For strerror
#include <cstddef>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace itch {

namespace {
// Error logging helper
// err simply means ERRNO which on a POSIX/LINUX sysem
[[maybe_unused]] void log_syserr(const char* what, int err) noexcept {
    const char* msg = std::strerror(err);
    std::fprintf(stderr, "[MMapFile] %s failed: %s (errno=%d)\n", what, msg ? msg : "?", err);
}

}

// O_RDONLY means open the file in read only mode
// O_CLOEXEC marks the file descriptor as close-on-exec, if the process later calls exec() then this descriptor will atomatically be closed
MMapFile::MMapFile(const char* path) noexcept {
    fd_ = ::open(path, O_RDONLY | O_CLOEXEC);

    if (fd_ < 0) {
        log_syserr("open", errno);
        return;
    }

    struct stat st{};
    if (::fstat(fd_, &st) != 0) {
        log_syserr("fstat", errno);
        ::close(fd_);
        fd_ = -1;
        return;
    }

    if (!S_ISREG(st.st_mode)) {
        std::fprintf(stderr, "[MMapFile] not a regular file: %s\n", path);
        ::close(fd_);
        fd_ = -1;
        return;
    }

    if (st.st_size <= 0) {
        std::fprintf(stderr, "[MMapFile] empty file: %s\n", path);
        ::close(fd_);
        fd_ = -1;
        return;
    }

    // We need to static_cast st.st_size cause it is off_t type 
    // off_t is a POSIX standard signed integer data type to represent file size and block offsets
    size_ = static_cast<std::size_t>(st.st_size);
    void* p = mmap(nullptr, size_, PROT_READ, MAP_PRIVATE | MAP_NORESERVE, fd_, 0);

    if (p == MAP_FAILED) {
        log_syserr("mmap" errno);
        ::close(fd_);
        fd_ = -1;
        return;
    }
    data_ = static_cast<const std::byte*>(p);

}

MMapFile::~MMapFile() { release(); }

MMapFile::MMapFile(MMapFile&& other) noexcept
    : data_(other.data_), size_(other.size_), fd_(other.fd_) {
    other.data_ = nullptr; 
    other.size_ = 0;
    other.fd_ = -1;
}

MMapFile& MMapFile::operator=(MMapFile&& other) noexcept {
    if (this != &other) {
        // Release the current mapping/file
        release();

        // Take ownership of other's resources
        data_ = other.data_;
        size_ = other.size;
        fd_ = other.fd_;

        // Empty the source object
        other.data_ = nullptr;
        other.size_ = 0;
        other.fd_ = -1;
    }
    return *this;
}

void MMapFile::release() noexcept {
    if (data_) {
        ::munmap(const_cast<std::byte*>(data_), size_);
        data_ = nullptr;
    }

    if (fd >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    size_ = 0;
}

void MMapFile::advise_sequential() const noexcept {
    if (!data_) [[unlikely]] return;

    // Tell the kernel that we expect to access the mapping sequentially
    ::madvise(const_cast<std::byte*>(data_), size_, MADV_SEQUENTIAL);

    // Tell the kernel that we will probably need these pages soon
    ::madvise(const_cast<std::byte*>(data_), size_, MADV_WILLNEED);

#ifdef MADV_HUGEPAGE

// Ask the kernel to consider using huge pages
::madvise(const_cast<std::byte*>(data_), size_, MADV_HUGEPAGE);
#endif
}

std::size_t MMapFile::prefault() {
    if (!data_) [[unlikely]] return 0;

    // 4096 is typical linux page size
    constexpr std::size_t kPage = 4096;
    std::size_t touched = 0;

    for (std::size_t off = 0; off < size_; off += kPage) {
        volatile std::byte b = data[off];
        (void)b;
        touched += kPage;
    }
    return touched;
}

} // namespace itch
