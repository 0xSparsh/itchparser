#pragma once

#include "WireFormat.hpp"
#include "MatchingEngine.hpp"
#include "Logger.hpp"
#include "Stats.hpp"
#include "Parser.hpp"

#include <vector>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>

namespace itch::test {

// Raw byte buffer builders

inline void emit_frame(std::vector<std::byte>& out, const void* payload,
                       std::size_t len) {
    out.push_back(static_cast<std::byte>((len >> 8) & 0xFF));
    out.push_back(static_cast<std::byte>(len & 0xFF));
    const auto* p = static_cast<const std::byte*>(payload);
    out.insert(out.end(), p, p + len);
}

template <typename Msg>
inline void emit_msg(std::vector<std::byte>& out, const Msg& m) {
    emit_frame(out, &m, sizeof(m));
}

inline void fill_header(wire::CommonHeader& h, char type, std::uint16_t loc,
                        std::uint64_t ts, std::uint16_t tracking = 1) {
    h.messageType = static_cast<wire::Byte>(type);
    h.stockLocateHi = static_cast<std::uint8_t>((loc >> 8) & 0xFF);
    h.stockLocateLo = static_cast<std::uint8_t>(loc & 0xFF);
    h.trackingHi = static_cast<std::uint8_t>((tracking >> 8) & 0xFF);
    h.trackingLo = static_cast<std::uint8_t>(tracking & 0xFF);
    h.ts[0] = static_cast<std::uint8_t>((ts >> 40) & 0xFF);
    h.ts[1] = static_cast<std::uint8_t>((ts >> 32) & 0xFF);
    h.ts[2] = static_cast<std::uint8_t>((ts >> 24) & 0xFF);
    h.ts[3] = static_cast<std::uint8_t>((ts >> 16) & 0xFF);
    h.ts[4] = static_cast<std::uint8_t>((ts >> 8) & 0xFF);
    h.ts[5] = static_cast<std::uint8_t>(ts & 0xFF);
}

inline void put_be64(wire::Byte (&a)[8], std::uint64_t v) {
    for (int i = 0; i < 8; ++i)
        a[i] = static_cast<wire::Byte>((v >> (56 - 8 * i)) & 0xFF);
}

inline void put_be32(wire::Byte (&a)[4], std::uint32_t v) {
    a[0] = static_cast<wire::Byte>((v >> 24) & 0xFF);
    a[1] = static_cast<wire::Byte>((v >> 16) & 0xFF);
    a[2] = static_cast<wire::Byte>((v >> 8) & 0xFF);
    a[3] = static_cast<wire::Byte>(v & 0xFF);
}

inline void put_stock8(wire::Alpha<8>& dst, const char* sym) {
    std::memset(dst.data(), ' ', 8);
    std::memcpy(dst.data(), sym, std::min(std::strlen(sym), std::size_t(8)));
}

// --- message factories -----------------------------------------------------

inline wire::StockDirectoryMsg make_stock_dir(std::uint16_t loc,
                                              std::uint64_t ts,
                                              const char* symbol) {
    wire::StockDirectoryMsg m{};
    fill_header(m.hdr, 'R', loc, ts);
    put_stock8(m.stock, symbol);
    m.marketCategory = 'Q';
    m.financialStatusIndicator = 'N';
    put_be32(m.roundLotSize, 100);
    m.roundLotsOnly = 'N';
    m.issueClassification = 'C';
    m.issueSubType = {' ', ' '};
    m.authenticity = 'P';
    m.shortScaleThresholdIndicator = 'N';
    m.ipoFlag = 'N';
    m.luldReferencePriceTier = '1';
    m.etpFlag = 'N';
    put_be32(m.etpLeverageFactor, 1);
    m.inverseIndicator = 'N';
    return m;
}

inline wire::AddOrderMsg make_add(std::uint16_t loc, std::uint64_t ts,
                                  std::uint64_t oid, char side,
                                  std::uint32_t shares, std::uint32_t price,
                                  const char* symbol = "AAPL") {
    wire::AddOrderMsg m{};
    fill_header(m.hdr, 'A', loc, ts);
    put_be64(m.orderReferenceNumber, oid);
    m.buySellIndicator = static_cast<wire::Byte>(side);
    put_be32(m.shares, shares);
    put_stock8(m.stock, symbol);
    put_be32(m.price, price);
    return m;
}

inline wire::AddOrderMPIDMsg make_add_mpid(std::uint16_t loc, std::uint64_t ts,
                                           std::uint64_t oid, char side,
                                           std::uint32_t shares,
                                           std::uint32_t price,
                                           const char* symbol = "AAPL",
                                           const char* mpid = "NSDQ") {
    wire::AddOrderMPIDMsg m{};
    fill_header(m.hdr, 'F', loc, ts);
    put_be64(m.orderReferenceNumber, oid);
    m.buySellIndicator = static_cast<wire::Byte>(side);
    put_be32(m.shares, shares);
    put_stock8(m.stock, symbol);
    put_be32(m.price, price);
    std::memset(m.attribution.data(), ' ', 4);
    std::memcpy(m.attribution.data(), mpid,
                std::min(std::strlen(mpid), std::size_t(4)));
    return m;
}

inline wire::OrderExecutedMsg make_exec(std::uint16_t loc, std::uint64_t ts,
                                        std::uint64_t oid, std::uint32_t qty,
                                        std::uint64_t match = 9001) {
    wire::OrderExecutedMsg m{};
    fill_header(m.hdr, 'E', loc, ts);
    put_be64(m.orderReferenceNumber, oid);
    put_be32(m.executedShares, qty);
    put_be64(m.matchNumber, match);
    return m;
}

inline wire::OrderExecutedWithPriceMsg make_exec_px(
    std::uint16_t loc, std::uint64_t ts, std::uint64_t oid, std::uint32_t qty,
    std::uint32_t px, std::uint64_t match = 9002) {
    wire::OrderExecutedWithPriceMsg m{};
    fill_header(m.hdr, 'C', loc, ts);
    put_be64(m.orderReferenceNumber, oid);
    put_be32(m.executedShares, qty);
    put_be64(m.matchNumber, match);
    m.printable = 'Y';
    put_be32(m.executionPrice, px);
    return m;
}

inline wire::OrderCancelMsg make_cancel(std::uint16_t loc, std::uint64_t ts,
                                        std::uint64_t oid, std::uint32_t qty) {
    wire::OrderCancelMsg m{};
    fill_header(m.hdr, 'X', loc, ts);
    put_be64(m.orderReferenceNumber, oid);
    put_be32(m.cancelledShares, qty);
    return m;
}

inline wire::OrderDeleteMsg make_delete(std::uint16_t loc, std::uint64_t ts,
                                        std::uint64_t oid) {
    wire::OrderDeleteMsg m{};
    fill_header(m.hdr, 'D', loc, ts);
    put_be64(m.orderReferenceNumber, oid);
    return m;
}

inline wire::OrderReplaceMsg make_replace(std::uint16_t loc, std::uint64_t ts,
                                          std::uint64_t old_id,
                                          std::uint64_t new_id,
                                          std::uint32_t qty,
                                          std::uint32_t px) {
    wire::OrderReplaceMsg m{};
    fill_header(m.hdr, 'U', loc, ts);
    put_be64(m.originalOrderReferenceNumber, old_id);
    put_be64(m.newOrderReferenceNumber, new_id);
    put_be32(m.shares, qty);
    put_be32(m.price, px);
    return m;
}

inline wire::SystemEventMsg make_system(char code, std::uint64_t ts = 1000) {
    wire::SystemEventMsg m{};
    fill_header(m.hdr, 'S', 0, ts);
    m.eventCode = static_cast<wire::Byte>(code);
    return m;
}

inline wire::NonCrossTradeMsg make_non_cross(std::uint16_t loc,
                                             std::uint64_t ts) {
    wire::NonCrossTradeMsg m{};
    fill_header(m.hdr, 'P', loc, ts);
    put_be64(m.orderReferenceNumber, 0);
    m.buySellIndicator = 'B';
    put_be32(m.shares, 100);
    put_stock8(m.stock, "AAPL");
    put_be32(m.price, 15005);
    put_be64(m.matchNumber, 9002);
    return m;
}

inline wire::CrossTradeMsg make_cross(std::uint16_t loc, std::uint64_t ts,
                                      char xtype = 'C') {
    wire::CrossTradeMsg m{};
    fill_header(m.hdr, 'Q', loc, ts);
    std::memset(m.shares, 0, 8);
    m.shares[6] = 0x03;
    m.shares[7] = 0xE8;  // 1000
    put_stock8(m.stock, "AAPL");
    put_be32(m.crossPrice, 15000);
    put_be64(m.matchNumber, 9003);
    m.crossType = static_cast<wire::Byte>(xtype);
    return m;
}

inline wire::BrokenTradeMsg make_broken(std::uint16_t loc, std::uint64_t ts,
                                        std::uint64_t match = 9002) {
    wire::BrokenTradeMsg m{};
    fill_header(m.hdr, 'B', loc, ts);
    put_be64(m.matchNumber, match);
    return m;
}

// ---------------------------------------------------------------------------
// Test engine: small pools so unit tests stay in megabytes, not gigabytes.
// Production uses 2^24 / 2^22; tests use 2^14 / 2^12 by default.
// ---------------------------------------------------------------------------

struct TestEngine {
    LockFreeLogger logger{"/dev/null"};
    MatchingEngine engine{logger, 1u << 14, 1u << 12};
    Stats stats{};
    Parser parser{};

    TestEngine() { stats.reset(); }

    // Parse a buffer, return bytes consumed.
    std::size_t run(const std::vector<std::byte>& buf) {
        return parser.run(buf.data(), buf.size(), engine, stats);
    }

    std::size_t run(const std::byte* p, std::size_t n) {
        return parser.run(p, n, engine, stats);
    }
};

// Build the 4-message book: R + 3x A 
inline std::vector<std::byte> canonical_book() {
    std::vector<std::byte> buf;
    emit_msg(buf, make_stock_dir(1, 1000, "AAPL"));
    emit_msg(buf, make_add(1, 2000, 1001, 'B', 100, 15000));
    emit_msg(buf, make_add(1, 3000, 1002, 'B', 200, 14950));
    emit_msg(buf, make_add(1, 4000, 2001, 'S', 150, 15025));
    return buf;
}

} // namespace itch::test