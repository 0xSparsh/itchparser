#pragma once

// On wire packed message layout for NASDAQ itch 5.0
// Every struct is '__attribute__((packed))' and matches the spec
// byte layout exactly.

// Field Types use fixed width unsigned integers
// Big-endian values atre converted at the call site (Endian.hpp)
// We donot save them in host order inside these structs, so a struct instance 
// is a view onto the mmap'd file (zero-copy)

// Common Wire format is :-
// byte 0    - message type
// byte 1-2  - stock locate
// byte 3-4  - tracking number
// byte 5-11 - timestamp

#include <array>
#include <cstdint>

namespace itch::wire {

using Byte = uint8_t;
template <std::size_t N> using Alpha = std::array<char, N>;

// Common header present in every message (11 bytes total)
struct CommonHeader {
    Byte    messageType;        // offset 0
    std::uint8_t stockLocateHi; // offset 1
    std::uint8_t stockLocateLo;
    std::uint8_t trackingHi;    // offset 3
    std::uint8_t trackingLo;    
    Byte    ts[6];              // offset 5, 6 bytes (timestamp)
} __attribute__((packed));
static_assert(sizeof(CommonHeader) == 11);

// 1.1 System Event Message
struct SystemEventMsg {
    CommonHeader hdr;
    Byte    eventCode;  // offset 11
} __attribute__((packed));
static_assert(sizeof(SystemEventMsg) == 12);

// 1.2.1 Stock Directory Message
struct StockDirectoryMsg {
    CommonHeader hdr;
    Alpha<8>     stock;
    Byte         marketCategory;
    Byte         financialStatusIndicator;
    std::uint8_t roundLotSize[4];
    Byte         roundLotsOnly;
    Byte         issueClassification;
    Alpha<2>     issueSubType;
} __attribute__((packed));

}