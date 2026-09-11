#pragma once

#include <cstddef>
#include <expected>
#include <span>
#include <string_view>
#include <system_error>

// Memory map the file instead of read() for zero copying
namespace itch {

class MMapFile
{
public:
    MMapFile() noexcept = default;

    explicit MMapFile(const char* path) noexcept;
    ~MMapFile();

    // Preferred entry point. The returned expected forces the caller to
    // unwrap success/failure, unlike the constructor, this cannot be
    // used without handling the error. OS failures carry the original
    // errno; semantic rejections (not a file / empty file) report
    // std::errc::invalid_argument.
    [[nodiscard]] static std::expected<MMapFile, std::error_code>
    open(const char* path) noexcept {
        MMapFile m;
        if (std::error_code ec = m.open_into(path); ec) {
            return std::unexpected(ec);
        }
        return m;
    }

    MMapFile(const MMapFile& other) = delete;
    MMapFile& operator=(const MMapFile& other) = delete;

    MMapFile(MMapFile&& other) noexcept;
    MMapFile& operator=(MMapFile&& other) noexcept;

    [[nodiscard]] const std::byte* data() const noexcept { return data_; }
    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    [[nodiscard]] bool valid() const noexcept { return data_ != nullptr; }
    [[nodiscard]] std::span<const std::byte> span() const noexcept {
        return {data_, size_};
    }

    void advise_sequential() const noexcept;
    std::size_t prefault() const noexcept;

private:
    const std::byte* data_ = nullptr;
    std::size_t size_ = 0;
    int fd_ = -1;

    void release() noexcept;

    // Shared open logic behind both the legacy constructor and open().
    // Returns an empty error_code on success. Every failure path also
    // prints a specific diagnostic to stderr, as before.
    std::error_code open_into(const char* path) noexcept;
};

} // namespace itch