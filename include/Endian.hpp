#pragma once

#include <type_traits>
#include <cstdint>
#include <concepts>
#include <iostream>

namespace itch {

// Compile time detection of host endianess
inline constexpr bool kIsLittleEndian = (std::endian::native == std::endian::little);



}