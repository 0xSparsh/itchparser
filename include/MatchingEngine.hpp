#pragma once

// MatchingEngine - routes Itch events to per instrument OrderBooks
//
// Owns the InstrumentMap, shared MemoryPool<Order>, MemoryPool<PriceLevel>,
// and a pointer to the LockFreeLogger
//
//Error handling: unknown order IDs and pool exhaustion are logged and dropperd;
// the engine never crashes or corrupts the book.

#include "Order.hpp"
#include "OrderBook.hpp"
#include "MemoryPool.hpp"
#include "InstrumentMap.hpp"
#include "Logger.hpp"
#include "PriceLevel.hpp"
#include "Types.hpp"

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace itch {

// Direct Order Index - O(1) order lookup via direct array + overflow map
//
// ITCH order reference numbers are monotonically increasing (not necessarily sequential)
// Fast path is power of two array
//
//      Order* orders[id & mask]
//
// Benefits over a hash map: no hashing, no probing on the common path, better
// sequential locality. The array alone is insufficient when IDs wrap the
// capacity window or collide; an overflow unordered_map holds those cases.
//
// insert / find / erase check the array first, then the overflow. Overflow
// traffic is cold-path only (collisions and unknown-order lookups).

class DirectOrderIndex {
public:
    static constexpr std::size_t kCapacity = 1u << 25;      // 2^25
    static constexpr std::size_t kMask     = kCapacity - 1;

    DirectOrderIndex() : slots_(kCapacity, nullptr) {
        overflow_.reserve(1024);
    }

    bool insert(OrderReferenceNumber id, Order* o) {
        Order*& slot = slots_[id & kMask];
        if (slot == nullptr) [[likely]] {
            slot = o;
            ++count_;
            return true;
        }

        if (slot->order_id == id) [[unlikely]] {
            return false;   // duplicate add
        }

        auto [it, inserted] = overflow_.emplace(id, o);
        if (!inserted) [[unlikely]] return false;
        ++count_;
        ++collisions_;
        return true;
    }

    // Fast path: one array load + compare
    // miss falls through to overflow
    [[nodiscard]] Order* find(OrderReferenceNumber id) const noexcept {
        Order* o = slots_[id & kMask];
        if (o && o->order_id == id) [[likely]] return o;
        auto it = overflow_.find(id);
        if (it != overflow_.end()) return it->second;
        return nullptr;
    }

    // returns true if the id was present and removed
    bool erase(OrderReferenceNumber id) noexcept {
        Order*& slot = slots_[id & kMask];
        if (slot && slot->order_id == id) [[likely]] {
            slot = nullptr;
            --count_;
            return true;
        }
        auto it = overflow_.find(id);
        if (it != overflow_.end()) {
            overflow_.erase(it);
            --count_;
            return true;
        }
        return false;
    }

    void clear() noexcept {
        std::fill(slots_.begin(), slots_.end(), nullptr);
        overflow_.clear();
        count_ = 0;
    }

    [[nodiscard]] std::size_t size() const noexcept { return count_; }
    [[nodiscard]] std::size_t overflow_size() const noexcept { return overflow_.size(); }
    [[nodiscard]] std::uint64_t collison() const noexcept { return collisions_; }
    static constexpr std::size_t capacity() noexcept { return kCapacity; }

private:
    // Heap allocated so construction does not blow the stack
    std::vector<Order*> slots_;
    std::unordered_map<OrderReferenceNumber, Order*> overflow_;
    std::size_t count_{0};
    std::uint64_t collisions_{0};
};

// Aggregated statistics. Plain uint64_t (not atomic) because the engine is
// single-threaded, the driver reads them after the parse loop finishes.
// Rejection counters are the primary debugging tool for pool sizing and
// lifecycle bugs.

struct EngineStats {
    // Accepted events (book was mutated)
    std::uint64_t add_orders{0};                  // 'A'
    std::uint64_t add_orders_with_mpid{0};        // 'F'
    std::uint64_t order_executed{0};              // 'E'
    std::uint64_t order_executed_with_price{0};   // 'C'
    std::uint64_t order_cancel{0};                // 'X'
    std::uint64_t order_delete{0};                // 'D'
    std::uint64_t order_replace{0};               // 'U'
    std::uint64_t non_cross_trades{0};            // 'P' (informational)
    std::uint64_t cross_trades{0};                // 'Q'
    std::uint64_t broken_trades{0};               // 'B' (informational)
    std::uint64_t instruments_registered{0};

    // Rejected events (book unchanged)
    std::uint64_t add_rejected_pool_exhausted{0};
    std::uint64_t add_rejected_level_pool_exhausted{0};
    std::uint64_t replace_rejected_pool_exhausted{0};
    std::uint64_t duplicate_adds{0};
    std::uint64_t locate_mismatches{0};
    std::uint64_t level_missing{0};
    std::uint64_t unknown_order_events{0};

    // Pool / index lifecycle
    std::uint64_t live_orders{0};
    std::uint64_t live_levels{0};
    std::uint64_t peak_live_orders{0};
    std::uint64_t peak_live_levels{0};

    // Legacy aggregate kept for report compatibility
    std::uint64_t pool_exhausted{0};
};

class MatchingEngine {
public:
    MatchingEngine();

    MatchingEngine(const MatchingEngine&) = delete;
    MatchingEngine& operator=(const MatchingEngine&) = delete;

    MatchingEngine(MatchingEngine&&) = delete;
    MatchingEngine& operator=(MatchingEngine&&) = delete;

    void on_stock_directory() {}

    void on_trading_action() {}

    void on_add_order() {}

    void on_order_executed() {}

    void on_order_executed_with_price() {}

    void on_order_cancel() {}

    void on_order_delete() {}

    void on_order_replace() {}

    void on_non_cross_trade() {}

    void on_cross_trade() {}

    void on_broken_trade() {}



private:
    LockFreeLogger&         logger_;
    InstrumentMap           instruments_;
    MemoryPool<Order>       order_pool_;
    MemoryPool<PriceLevel>  level_pool_;
    DirectOrderIndex        order_index_;
    EngineStats             stats_{};
};

} // namespace itch