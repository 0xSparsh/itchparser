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

inline void emit_frame() {}

inline void emit_msg() {}

inline void fill_header() {}

inline void put_be64() {}

inline void put_be32() {}

inline void put_stock8() {}

inline wire::StockDirectoryMsg make_stock_dir() {}

inline wire::AddOrderMsg make_add() {}

inline wire::AddOrderMPIDMsg make_add_mpid() {}

inline wire::OrderExecutedMsg make_exec() {}

inline wire::OrderExecutedWithPriceMsg make_exec_px() {}

inline wire::OrderCancelMsg make_cancel() {}

inline wire::OrderDeleteMsg make_delete() {}

inline wire::OrderReplaceMsg make_replace() {}

inline wire::SystemEventMsg make_system() {}

inline wire::NonCrossTradeMsg make_non_cross() {}

inline wire::CrossTradeMsg make_cross() {}

inline wire::BrokenTradeMsg make_broken() {}

struct TestEngine {
    LockFreeLogger logger{"dev/null"};
    MatchingEngine engine{logger, 1u << 14, 1u << 12};
    Stats stats{};
    Parser parser{};

    TestEngine() { stats.reset(); }

    // Parse a buffer, return bytes consumed
    std::size_t run(const std::vector<std::byte>& buf) {
        return parser.run(buf.data(), buf.size(), engine, stats);
    }

    std::size_t run(std::byte* p, std::size_t n) {
        return parser.run(p, n, engine, stats);
    }

};

} // namespace itch::test