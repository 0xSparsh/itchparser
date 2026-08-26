#include "Messages.hpp"
#include "Endian.hpp"

#include <iostream>

int main() {
    uint16_t x{0x1234};
    uint32_t y{0x12345678};
    uint64_t z{0x1234567890123456};

    std::cout << std::hex << itch::bswap16(x) << '\n';
    std::cout << std::hex << itch::bswap32(y) << '\n';
    std::cout << std::hex << itch::bswap64(z) << '\n';
}