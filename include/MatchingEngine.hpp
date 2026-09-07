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
    MatchingEngine(LockFreeLogger& logger,
                   std::size_t order_pool_size = kOrderPoolSize,
                   std::size_t price_level_pool_size = kPriceLevelPoolSize)
        : logger_(logger)
        , order_pool_(order_pool_size)
        , level_pool_(price_level_pool_size)
    {}

    MatchingEngine(const MatchingEngine&) = delete;
    MatchingEngine& operator=(const MatchingEngine&) = delete;

    MatchingEngine(MatchingEngine&&) = delete;
    MatchingEngine& operator=(MatchingEngine&&) = delete;

    void on_stock_directory(StockLocate locate, const char* symbol,
                            MarketCategory mc, FinancialStatusIndicator fsi,
                            RoundLotSize rls, RoundLotsOnly rlo,
                            Authenticity auth,
                            ShortScaleThresholdIndicator ssti,
                            IPOFlag ipo, LULDReferencePriceTier luld,
                            ETPFlag etp, ETPLeverageFactor elf,
                            InverseIndicator inv) {
        const bool is_new = (instruments_[locate] == nullptr);
        Instrument* inst = instruments_.get_or_create(locate);
        inst->set_symbol(symbol);
        inst->marketCategory       = mc;
        inst->financialStatus      = fsi;
        inst->roundLotSize         = rls;
        inst->roundLotsOnly        = rlo;
        inst->authenticity         = auth;
        inst->shortScale           = ssti;
        inst->ipoFlag              = ipo;
        inst->luldTier             = luld;
        inst->etpFlag              = etp;
        inst->etpLeverageFactor    = elf;
        inst->inverseIndicator     = inv;
        if (is_new) ++stats_.instruments_registered;
    }

    // Stock trading action - update trading state
    void on_trading_action(StockLocate locate, TradingState state, 
                           const char* /*symbol*/, const char* /*reason*/) {
        if (Instrument* inst = instruments_[locate]; inst) [[likely]] {
            inst->tradingState = state;
        }
    }

    // Update short scale restriction
    void on_reg_sho(StockLocate locate, RegSHOAction action) {
        if (Instrument* inst = instruments_[locate]; inst) [[likely]] {
            inst->regSHOAction = action;
        }
    }

    // Add order (A/F)
    // On rejection: order-pool exhaust , level-pool exhaust or duplicate id
    // On level pool failure, the acquired Order is released to avoid a leak
    void on_add_order(StockLocate locate, OrderReferenceNumber id,
                      BuySellIndicator bsi, Shares qty, Price px,
                      Timestamp ts, MPID mpid, bool attributed) {
        if (order_index_.find(id) != nullptr) [[unlikely]] {
            ++stats_.duplicate_adds;
            ++stats_.unknown_order_events;
            return;
        }

        Instrument* inst = instruments_[locate];
        if (!inst) [[unlikely]] {
            inst = instruments_.get_or_create(locate);
        }

        OrderBook& book = inst->ensure_book();

        void* mem = order_pool_.acquire();
        if (!mem) [[unlikely]] {
            ++stats_.add_rejected_pool_exhausted;
            ++stats_.pool_exhausted;
            return;
        }
        Order* o = new (mem) Order(id, locate, px, qty,
                                    to_side(bsi), ts,
                                    attributed ? mpid : MPID{' ', ' ', ' ', ' '});
        const int add_rc = book.add_order(o, level_pool_);
        if (add_rc == 0) [[unlikely]] {
            ++stats_.add_rejected_level_pool_exhausted;
            ++stats_.pool_exhausted;
            order_pool_.release(o);
            return;
        }
        if (add_rc == 2) {
            ++stats_.live_levels;
            if (stats_.live_levels > stats_.peak_live_levels) [[unlikely]] {
                stats_.peak_live_levels = stats_.live_levels;
            }
        }

        // Pre-checked for duplicates; failure here rolls back the book insert
        if (!order_index_.insert(id, o)) [[unlikely]] {
            ++stats_.duplicate_adds;
            ++stats_.unknown_order_events;
            const int rm = book.remove_order(o, level_pool_);
            if (rm == 2 && stats_.live_levels > 0) --stats_.live_levels;
            order_pool_.release(o);
            return;
        }

        ++stats_.live_orders;
        if (stats_.live_orders > stats_.peak_live_orders) [[unlikely]] {
            stats_.peak_live_orders = stats_.live_orders;
        }

        if (attributed) ++stats_.add_orders_with_mpid;
        else            ++stats_.add_orders;
    }

    // Order Executed ('E') - partial or full fill
    void on_order_executed(StockLocate locate, OrderReferenceNumber id,
                           Shares exec_qty, MatchNumber /*match*/) {
        Order* o = order_index_.find(id);
        if (!o) [[unlikely]] { ++stats_.unknown_order_events; return; }
        if (o->locate != locate) [[unlikely]] {
            ++stats_.locate_mismatches;
            locate = o->locate;
        }

        Instrument* inst = instruments_[locate];
        if (!inst || !inst->book) [[unlikely]] { ++stats_.unknown_order_events; return; }
        OrderBook& book = *inst->book;

        const int rc = book.execute_order(o, exec_qty, level_pool_);
        if (rc < 0) [[unlikely]] {
            ++stats_.level_missing;
            ++stats_.unknown_order_events;
            return;
        }
        ++stats_.order_executed;
        if (rc > 0) {
            order_index_.erase(id);
            order_pool_.release(o);
            if (stats_.live_orders > 0) --stats_.live_orders;
            if (rc == 2 && stats_.live_levels > 0) --stats_.live_levels;
        }
    }

    // Order Executed With Price ('C')
    // Book mechanics identical to 'E', execution price is informational only
    void on_order_executed_with_price(StockLocate locate,
                                      OrderReferenceNumber id,
                                      Shares exec_qty,
                                      MatchNumber /*match*/,
                                      Printable /*printable*/,
                                      ExecutionPrice /*exec_px*/) {
        Order* o = order_index_.find(id);
        if (!o) [[unlikely]] { ++stats_.unknown_order_events; return; }
        if (o->locate != locate) [[unlikely]] {
            ++stats_.locate_mismatches;
            locate = o->locate;
        }

        Instrument* inst = instruments_[locate];
        if (!inst || !inst->book) [[unlikely]] { ++stats_.unknown_order_events; return; }
        OrderBook& book = *inst->book;

        const int rc = book.execute_order(o, exec_qty, level_pool_);
        if (rc < 0) [[unlikely]] {
            ++stats_.level_missing;
            ++stats_.unknown_order_events;
            return;
        }
        ++stats_.order_executed_with_price;
        if (rc > 0) {
            order_index_.erase(id);
            order_pool_.release(o);
            if (stats_.live_orders > 0) --stats_.live_orders;
            if (rc == 2 && stats_.live_levels > 0) --stats_.live_levels;
        }
    }

    // Order Cancel ('X') - partial cancel
    void on_order_cancel(StockLocate locate, OrderReferenceNumber id,
                         Shares cancelled_qty) {
        Order* o = order_index_.find(id);
        if (!o) [[unlikely]] { ++stats_.unknown_order_events; return; }
        if (o->locate != locate) [[unlikely]] {
            ++stats_.locate_mismatches;
            locate = o->locate;
        }

        Instrument* inst = instruments_[locate];
        if (!inst || !inst->book) [[unlikely]] { ++stats_.unknown_order_events; return; }
        OrderBook& book = *inst->book;

        const int rc = book.cancel_order(o, cancelled_qty, level_pool_);
        if (rc < 0) [[unlikely]] {
            ++stats_.level_missing;
            ++stats_.unknown_order_events;
            return;
        }
        ++stats_.order_cancel;
        if (rc > 0) {
            order_index_.erase(id);
            order_pool_.release(o);
            if (stats_.live_orders > 0) --stats_.live_orders;
            if (rc == 2 && stats_.live_levels > 0) --stats_.live_levels;
        }
    }

    // Order Delete ('D') - full removal
    void on_order_delete(StockLocate locate, OrderReferenceNumber id) {
        Order* o = order_index_.find(id);
        if (!o) [[unlikely]] { ++stats_.unknown_order_events; return; }
        if (o->locate != locate) [[unlikely]] {
            ++stats_.locate_mismatches;
            locate = o->locate;
        }

        Instrument* inst = instruments_[locate];
        if (!inst || !inst->book) [[unlikely]] { ++stats_.unknown_order_events; return; }
        OrderBook& book = *inst->book;

        const int rc = book.remove_order(o, level_pool_);
        if (rc == 0) [[unlikely]] {
            ++stats_.level_missing;
            ++stats_.unknown_order_events;
            return;
        }
        order_index_.erase(id);
        order_pool_.release(o);
        if (stats_.live_orders > 0) --stats_.live_orders;
        if (rc == 2 && stats_.live_levels > 0) --stats_.live_levels;
        ++stats_.order_delete;
    }

    // Order Replace ('U') - cancel-replace
    // Acquire new slot first; on any failure leave the old order untouched.
    // Side and MPID are carried over from the old order
    void on_order_replace(StockLocate locate,
                          OrderReferenceNumber old_id,
                          OrderReferenceNumber new_id,
                          Shares new_qty, Price new_px,
                          Timestamp ts) {
        Order* old = order_index_.find(old_id);
        if (!old) [[unlikely]] { ++stats_.unknown_order_events; return; }
        StockLocate home = old->locate;
        if (home != locate) [[unlikely]] {
            ++stats_.locate_mismatches;
            locate = home;
        }

        if (order_index_.find(new_id) != nullptr) [[unlikely]] {
            ++stats_.duplicate_adds;
            ++stats_.unknown_order_events;
            return;
        }

        Instrument* inst = instruments_[locate];
        if (!inst || !inst->book) [[unlikely]] { ++stats_.unknown_order_events; return; }
        OrderBook& book = *inst->book;

        void* mem = order_pool_.acquire();
        if (!mem) [[unlikely]] {
            ++stats_.replace_rejected_pool_exhausted;
            ++stats_.pool_exhausted;
            return;
        }

        const Side side = old->side;
        const MPID mpid = old->mpid;

        const int rm = book.remove_order(old, level_pool_);
        if (rm == 0) [[unlikely]] {
            ++stats_.level_missing;
            ++stats_.unknown_order_events;
            order_pool_.release(static_cast<Order*>(mem));
            return;
        }
        order_index_.erase(old_id);
        order_pool_.release(old);
        if (stats_.live_orders > 0) --stats_.live_orders;
        if (rm == 2 && stats_.live_levels > 0) --stats_.live_levels;

        Order* new_order = new (mem) Order(new_id, locate, new_px, new_qty,
                                            side, ts, mpid);
        const int add_rc = book.add_order(new_order, level_pool_);
        if (add_rc == 0) [[unlikely]] {
            ++stats_.add_rejected_level_pool_exhausted;
            ++stats_.pool_exhausted;
            order_pool_.release(new_order);
            return;
        }
        if (add_rc == 2) {
            ++stats_.live_levels;
            if (stats_.live_levels > stats_.peak_live_levels) [[unlikely]] {
                stats_.peak_live_levels = stats_.live_levels;
            }
        }
        if (!order_index_.insert(new_id, new_order)) [[unlikely]] {
            ++stats_.duplicate_adds;
            const int rb = book.remove_order(new_order, level_pool_);
            if (rb == 2 && stats_.live_levels > 0) --stats_.live_levels;
            order_pool_.release(new_order);
            return;
        }
        ++stats_.live_orders;
        if (stats_.live_orders > stats_.peak_live_orders) [[unlikely]] {
            stats_.peak_live_orders = stats_.live_orders;
        }
        ++stats_.order_replace;
    }

    // Non-Cross Trade ('P') - informational; does not affect the visible book
    void on_non_cross_trade(StockLocate /*locate*/, Shares /*qty*/,
                            Price /*px*/, MatchNumber /*match*/) {
        ++stats_.non_cross_trades;
    }

    // Cross Trade ('Q'). Only the closing cross clears the book; opening and
    // halt crosses are informational (resting orders remain for continuous trading).
    void on_cross_trade(StockLocate locate, CrossType xtype) {
        ++stats_.cross_trades;
        if (xtype != CrossType::Closing) return;

        Instrument* inst = instruments_[locate];
        if (!inst || !inst->book) [[unlikely]] return;
        OrderBook& book = *inst->book;

        const std::size_t book_levels = book.live_levels();

        thread_local std::vector<Order*> snapshot;
        snapshot.clear();
        book.for_each_order([&](Order* o) {
            snapshot.push_back(o);
        });
        for (Order* o : snapshot) {
            order_index_.erase(o->order_id);
            order_pool_.release(o);
        }
        if (stats_.live_orders >= snapshot.size()) {
            stats_.live_orders -= snapshot.size();
        } else {
            stats_.live_orders = 0;
        }
        if (stats_.live_levels >= book_levels) {
            stats_.live_levels -= book_levels;
        } else {
            stats_.live_levels = 0;
        }
        book.clear_levels(level_pool_);
    }

    // Broken Trade ('B') - informational; the break is final (order is not re-added).
    void on_broken_trade(StockLocate /*locate*/, MatchNumber /*match*/) {
        ++stats_.broken_trades;
    }

    // Accessors
    [[nodiscard]] const EngineStats& stats() const noexcept { return stats_; }
    [[nodiscard]] InstrumentMap&     instruments() noexcept { return instruments_; }
    [[nodiscard]] std::size_t order_pool_free() const noexcept {
        return order_pool_.free_count();
    }
    [[nodiscard]] std::size_t level_pool_free() const noexcept {
        return level_pool_.free_count();
    }
    [[nodiscard]] std::size_t order_pool_capacity() const noexcept {
        return order_pool_.capacity();
    }
    [[nodiscard]] std::size_t level_pool_capacity() const noexcept {
        return level_pool_.capacity();
    }
    [[nodiscard]] std::size_t order_index_size() const noexcept {
        return order_index_.size();
    }
    [[nodiscard]] std::size_t order_index_overflow() const noexcept {
        return order_index_.overflow_size();
    }
    [[nodiscard]] std::uint64_t order_index_collisions() const noexcept {
        return order_index_.collisions();
    }
    [[nodiscard]] static constexpr std::size_t order_index_capacity() noexcept {
        return DirectOrderIndex::kCapacity;
    }

private:
    LockFreeLogger&         logger_;
    InstrumentMap           instruments_;
    MemoryPool<Order>       order_pool_;
    MemoryPool<PriceLevel>  level_pool_;
    DirectOrderIndex        order_index_;
    EngineStats             stats_{};
};

} // namespace itch