#pragma once

#include <cstddef>
#include <span>
#include <string_view>

// Memory map the file instead of read() for zero copying
namespace itch {

class MMapFile
{
public:
    MMapFile() noexcept = default;
    explicit MMapFile(const char* path) noexcept;
    ~MMapFile();

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
};

} // namespace itch