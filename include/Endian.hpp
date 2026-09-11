#pragma once

// Big-endian (network-byte order) to host conversion.
// NASDAQ ITCH 5.0 mandates big-endian for all integer fields.
// On little-endian hosts (x86-64, ARM) every load must byte-swap.

#include <bit>         // std::endian, std::byteswap
#include <cstdint>     // fixed width integer types
#include <cstring>     // std::memcpy
#include <type_traits> // std::is_pointer_v

namespace itch {

// Compile-time detection of host endianness.
inline constexpr bool kIsLittleEndian =
    (std::endian::native == std::endian::little);

// Byte swap a 16-bit integer.
[[nodiscard]] constexpr std::uint16_t bswap16(std::uint16_t v) noexcept {
    return std::byteswap(v);
}

// Byte swap a 32-bit integer.
[[nodiscard]] constexpr std::uint32_t bswap32(std::uint32_t v) noexcept {
    return std::byteswap(v);
}

// Byte swap a 64-bit integer.
[[nodiscard]] constexpr std::uint64_t bswap64(std::uint64_t v) noexcept {
    return std::byteswap(v);
}

// Compile-time proof that the swaps work in constant evaluation
static_assert(bswap16(0x1234u) == 0x3412u);
static_assert(bswap32(0x12345678u) == 0x78563412u);
static_assert(bswap64(0x0102030405060708ULL) == 0x0807060504030201ULL);

// Network-to-host load helpers.
// Read N bytes from an unaligned big-endian source and return the
// host-order integer. memcpy (not reinterpret_cast) keeps unaligned mmap
// addresses safe and compiles to the same single load instruction.
// Accepts any byte-like pointer via template.

// Loads a 16-bit big-endian integer.
template <typename Ptr>
[[nodiscard]] inline std::uint16_t load_be16(const Ptr p) noexcept {
    // Compile-time guard so a non-pointer argument fails here with a clear
    // message instead of deep inside memcpy.
    static_assert(std::is_pointer_v<Ptr>, "load_be16 requires a pointer");
    std::uint16_t v;
    std::memcpy(&v, p, sizeof(v));
    if constexpr (kIsLittleEndian) {
        v = bswap16(v);
    }
    return v;
}

template <typename Ptr>
[[nodiscard]] inline std::uint32_t load_be32(const Ptr p) noexcept {
    static_assert(std::is_pointer_v<Ptr>, "load_be32 requires a pointer");
    std::uint32_t v;
    std::memcpy(&v, p, sizeof(v));
    if constexpr (kIsLittleEndian) {
        v = bswap32(v);
    }
    return v;
}

template <typename Ptr>
[[nodiscard]] inline std::uint64_t load_be64(const Ptr p) noexcept {
    static_assert(std::is_pointer_v<Ptr>, "load_be64 requires a pointer");
    std::uint64_t v;
    std::memcpy(&v, p, sizeof(v));
    if constexpr (kIsLittleEndian) {
        v = bswap64(v);
    }
    return v;
}

// ITCH timestamps are 6 bytes, and there is no 6-byte integer type, so this
// stays a manual byte-by-byte build. (Loading 8 bytes and shifting would
// over-read 2 bytes past the field, which is unsafe at the end of the
// mapped buffer.)

template <typename Ptr>
[[nodiscard]] inline std::uint64_t load_be48(const Ptr p) noexcept {
    static_assert(std::is_pointer_v<Ptr>, "load_be48 requires a pointer");
    const unsigned char* cp = reinterpret_cast<const unsigned char*>(p);
    std::uint64_t v = 0;
    v |= static_cast<std::uint64_t>(cp[0]) << 40;
    v |= static_cast<std::uint64_t>(cp[1]) << 32;
    v |= static_cast<std::uint64_t>(cp[2]) << 24;
    v |= static_cast<std::uint64_t>(cp[3]) << 16;
    v |= static_cast<std::uint64_t>(cp[4]) << 8;
    v |= static_cast<std::uint64_t>(cp[5]);
    return v;
}

} // namespace itch
