#include <gtest/gtest.h>

#include <array>
#include <cstring>
#include <cstdint>

#include "Endian.hpp"

namespace itch {
namespace {

TEST(Bswap, Bswap16) {
    EXPECT_EQ(bswap16(0x1234u), 0x3412u);
    EXPECT_EQ(bswap16(0xABCDu), 0xCDABu);
    EXPECT_EQ(bswap16(bswap16(0xABCDu)), 0xABCDu);
    EXPECT_EQ(bswap16(0x0000u), 0x0000u);
    EXPECT_EQ(bswap16(0xF1D3u), 0xD3F1u);
}

TEST(Bswap, Bswap32) {
    EXPECT_EQ(bswap32(0x12345678u), 0x78563412u);
    EXPECT_EQ(bswap32(bswap32(0xDEADBEEFu)), 0xDEADBEEFu);
    EXPECT_EQ(bswap32(0x00000000u), 0x00000000u);
    EXPECT_EQ(bswap32(0xFFFFFFFFu), 0xFFFFFFFFu);
    EXPECT_EQ(bswap32(0x00000001u), 0x10000000u);
}

TEST(Bswap, Bswap64) {
    EXPECT_EQ(bswap64(0x0102030405060708ULL), 0x0807060504030201ULL);
    EXPECT_EQ(bswap64(bswap64(0x0102030405060708ULL)), 0x0102030405060708ULL);
    EXPECT_EQ(bswap64(0ULL), 0ULL);
    EXPECT_EQ(bswap64(~0ULL), ~0ULL);
}

TEST(LoadBE, Load16KnownBytes) {
    const std::uint8_t raw[2] = {0x00, 0x01};
    EXPECT_EQ(load_be16(raw), 1u);
    const std::uint8_t raw2[2] = {0x12, 0x34};
    EXPECT_EQ(load_be16(raw2), 0x1234u);
}

TEST(LoadBE, Load32KnownBytes) {
    const std::uint8_t raw[4] = {0x00, 0x00, 0x03, 0xE8};
    EXPECT_EQ(load_be32(raw), 1000u);
    const std::uint8_t raw2[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    EXPECT_EQ(load_be32(raw2), 0xDEADBEEFu);
}

TEST(LoadBE, Load64KnownBytes) {
    const std::uint8_t raw[8] = {0, 0, 0, 0, 0, 0, 0x03, 0xE8};
    EXPECT_EQ(load_be64(raw), 1000ULL);
    const std::uint8_t hi[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    EXPECT_EQ(load_be64(hi), ~0ULL);
}

TEST(LoadBE, Load48TimestampSixBytes) {
    // ITCH timestamps are 6 bytes. 0x00000123456789 -> that exact value.
    const unsigned char raw[6] = {0x00, 0x01, 0x23, 0x45, 0x67, 0x89};
    EXPECT_EQ(load_be48(raw), 0x00000123456789ULL);
}

TEST(LoadBE, Load48AllOnes) {
    const unsigned char raw[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    EXPECT_EQ(load_be48(raw), 0xFFFFFFFFFFFFULL);
}

TEST(LoadBE, Load48Zero) {
    const unsigned char raw[6] = {0, 0, 0, 0, 0, 0};
    EXPECT_EQ(load_be48(raw), 0ULL);
}

TEST(LoadBE, UnalignedSafeViaMemcpy) {
    // Historically a reinterpret_cast<uint64_t*> would fault on unaligned
    // mmap addresses; load_beN must work at any offset.
    alignas(8) std::uint8_t buf[16] = {};
    buf[1] = 0x12;
    buf[2] = 0x34;
    EXPECT_EQ(load_be16(buf + 1), 0x1234u);
    buf[3] = 0x56;
    buf[4] = 0x78;
    EXPECT_EQ(load_be32(buf + 1), 0x12345678u);
}

TEST(LoadBE, AcceptsByteAndCharPointers) {
    const std::byte braw[2] = {std::byte{0x00}, std::byte{0x2A}};
    EXPECT_EQ(load_be16(braw), 42u);
    const char craw[2] = {0x00, 0x2A};
    EXPECT_EQ(load_be16(craw), 42u);
}

TEST(LoadBE, RoundTripWithHostOrder) {
    // Encode host value as BE bytes manually, decode must match.
    for (std::uint32_t v : {0u, 1u, 255u, 65535u, 100000u, 0xFFFFFFFFu}) {
        std::uint8_t raw[4] = {
            static_cast<std::uint8_t>(v >> 24),
            static_cast<std::uint8_t>(v >> 16),
            static_cast<std::uint8_t>(v >> 8),
            static_cast<std::uint8_t>(v),
        };
        EXPECT_EQ(load_be32(raw), v) << "v=" << v;
    }
}

} // namespace 
} // namespace itch