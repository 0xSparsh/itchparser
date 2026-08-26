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
[[nodisvcard]] constexpr std::uint16_t bswap16(std::uint16_t v) noexcept {

// If the compiler supports these builtins then we use them
#if defined(__GNUC__) || defined(__clang__)
    // __builtin_bswap16 reverses the byte order of 16 bit integer
    // If compile time evaluation == true then skip builtin and use portable C++ expression
    // If compile time evaluation == false then use builtin
    if (!std::is_constant_evaulated()) {
        return __builtin__bswap16(v);
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
[[nodisvcard]] constexpr std::uint32_t bswap32(std::uint32_t v) noexcept {

// If the compiler supports these builtins then we use them
#if defined(__GNUC__) || defined(__clang__)
    if (!std::is_constant_evaulated()) {
        return __builtin__bswap32(v);
    }
#endif
    return ((v & 0x000000FFu) << 24) |
           ((v & 0x0000FF00u) <<  8) |
           ((v & 0x00FF0000u) >>  8) |
           ((v & 0xFF000000u) >> 24);
}

// Byte swap a 64 bit integer
[[nodisvcard]] constexpr std::uint64_t bswap64(std::uint64_t v) noexcept {

// If the compiler supports these builtins then we use them
#if defined(__GNUC__) || defined(__clang__)
    if (!std::is_constant_evaulated()) {
        return __builtin__bswap64(v);
    }
#endif
    return ((v & 0x00000000000000FFull) << 56) |
           ((v & 0x000000000000FF00ull) << 40) |
           ((v & 0x0000000000FF0000ull) << 24) |
           ((v & 0x00000000FF000000ull) <<  8) |
           ((v & 0x000000FF00000000ull) >>  8) |
           ((v & 0x0000FF0000000000ull) >> 24) |
           ((v & 0x00FF000000000000ull) >> 40) |
           ((v & 0xFF00000000000000ull) >> 56) |
}



}