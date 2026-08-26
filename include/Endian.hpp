#pragma once

// Big-endian (network-byte order) to native little-endian conversion
//
// NASDAQ ITCH 5.0 mandates big-endian for all integer field
// On x86-64 / ARM little endian targets we must byte swap on every load.

#include <bit>  // For std::endian
#include <type_traits>  // For is_constant_evaluated and is_pointer_v
#include <cstdint>  // For fxixed width integer types

namespace itch {

// Compile time detection of host endianess
inline constexpr bool kIsLittleEndian = (std::endian::native == std::endian::little);

// Byte swap a 16 bit integer
[[nodiscard]] constexpr std::uint16_t bswap16(std::uint16_t v) noexcept {

// If the compiler supports these builtins then we use them
#if defined(__GNUC__) || defined(__clang__)
    // __builtin_bswap16 reverses the byte order of 16 bit integer
    // If compile time evaluation == true then skip builtin and use portable C++ expression
    // If compile time evaluation == false then use builtin
    if (!std::is_constant_evaluated()) {
        return __builtin_bswap16(v);
    }
#endif
    // This is a manual implementation
    // Suppose v = 0x1234 which is equal to 00010010 00110100
    // v >> 8 does 00000000 00010010 which is 0x0012
    // v << 8 does 00110100 00000000 which is 0x3400
    // Then using OR operator to make 00110100 00010010 which is equal to 0x3412
    return static_cast<std::uint16_t>((v >> 8) | (v << 8));
}

// Byte swap a 32 bit integer
[[nodiscard]] constexpr std::uint32_t bswap32(std::uint32_t v) noexcept {

// If the compiler supports these builtins then we use them
#if defined(__GNUC__) || defined(__clang__)
    if (!std::is_constant_evaluated()) {
        return __builtin_bswap32(v);
    }
#endif
    return ((v & 0x000000FFu) << 24) |
           ((v & 0x0000FF00u) <<  8) |
           ((v & 0x00FF0000u) >>  8) |
           ((v & 0xFF000000u) >> 24);
}

// Byte swap a 64 bit integer
[[nodiscard]] constexpr std::uint64_t bswap64(std::uint64_t v) noexcept {

// If the compiler supports these builtins then we use them
#if defined(__GNUC__) || defined(__clang__)
    if (!std::is_constant_evaluated()) {
        return __builtin_bswap64(v);
    }
#endif
    return ((v & 0x00000000000000FFull) << 56) |
           ((v & 0x000000000000FF00ull) << 40) |
           ((v & 0x0000000000FF0000ull) << 24) |
           ((v & 0x00000000FF000000ull) <<  8) |
           ((v & 0x000000FF00000000ull) >>  8) |
           ((v & 0x0000FF0000000000ull) >> 24) |
           ((v & 0x00FF000000000000ull) >> 40) |
           ((v & 0xFF00000000000000ull) >> 56);
}

// Network to host load helpers
// Read N bytes from an unaligned big-endian source and return the
// host-order integer.
// Accepts any byte-like pointer via template

template <typename Ptr>

// Loads a 16-bit big endian integer
// std::is_pointer_v is essentially just std::is_pointer without having to type the verbose std::is_pointer<T>::value syntax
// std::is_pointer_v under the hood is essentially :-
//
// template <class T>
// inline constexpr bool is_pointer_v = is_pointer<T>::value;

[[nodiscard]] inline std::uint16_t load_be16(const Ptr p) noexcept {

    // using static assert to enusre that Ptr is a pointer type in compile time
    // Helps with getting a clearer diagnostic instead of getting a confusing error
    // from somewhere within __builtin_memcpy
    static_assert(std::is_pointer_v<Ptr>, "load_be16 requires a pointer");
    std::uint16_t v;
    __builtin_memcpy(&v, p, sizeof(v)); // Using __builtin_memcpy instead of std::memcpy cause its directly provided by GCC/Clang
    if constexpr (kIsLittleEndian) {
        v = bswap16(v);
    }
    return v;
}

template <typename Ptr>
[[nodiscard]] inline std::uint32_t load_be32(const Ptr p) noexcept {
    static_assert(std::is_pointer_v<Ptr>, "load_be32 requires a pointer");
    std::uint32_t v;
    __builtin_memcpy(&v, p, sizeof(v));
    if constexpr (kIsLittleEndian) {
        v = bswap32(v);
    }
    return v;
}

template <typename Ptr> 
[[nodiscard]] inline std::uint64_t load_be64(const Ptr p) noexcept {
    static_assert(std::is_pointer_v<Ptr>, "load_be64 requires a pointer");
    std::uint64_t v;
    __builtin_memcpy(&v, p, sizeof(v));
    if constexpr (kIsLittleEndian) {
        v = bswap64(v);
    }
    return v;
}

// ITCH timestamps are 6 bytes so we load 8 bytes and shift down.
// This process is simply :-  pointer -> byte -> number
//
// The first casting to unsigned char* is because we want to view the memory as individual bytes
// Timestamp starts after 5 byte header (MessageType(1), StockLocate(2), TrackingNumber(2))
// So if we do something like reinterpret_cast<const uint64_t*>(p) then we're saying that
// there is a 64 bit integer starting at address e.g.0x1005 but 0x1005 isnt 8-byte aligned
//
// 
template <typename Ptr>
[[nodiscard]] inline std::uint64_t load_be48(const Ptr p) noexcept {
    static_assert(std::is_pointer_v<Ptr>, "load_be48 requires a pointer");
    const unsigned char* cp = reinterpret_cast<const unsigned char*>(p);
    uint64_t v = 0;
    v |= static_cast<std::uint64_t>(cp[0]) << 40;
    v |= static_cast<std::uint64_t>(cp[1]) << 32;
    v |= static_cast<std::uint64_t>(cp[2]) << 24;
    v |= static_cast<std::uint64_t>(cp[3]) << 16;
    v |= static_cast<std::uint64_t>(cp[4]) << 8;
    v |= static_cast<std::uint64_t>(cp[5]);
    return v;
}

} // namespace itch
