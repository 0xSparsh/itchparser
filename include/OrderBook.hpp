#pragma once

#include "MemoryPool.hpp"
#include "Types.hpp"
#include "PriceLevel.hpp"
#include "Order.hpp"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>

namespace itch {

class OrderBook {
public:
    OrderBook() noexcept = default;

    OrderBook(const OrderBook&)            = delete;
    OrderBook& operator=(const OrderBook&) = delete;

    // Add a resting order.
    // Returns 0 on failure (level pool exhausted),
    //         1 if added to an existing level,
    //         2 if a new level was created.
    int add_order(Order* o, MemoryPool<PriceLevel>& level_pool) noexcept {
        const Side  s  = o->side;
        const Price px = o->price_cents;

        PriceLevel* lvl = static_cast<PriceLevel*>(find_level(s, px));
        bool created = false;
        if (!lvl) [[unlikely]] {
            lvl = level_pool.acquire();
            if (!lvl) [[unlikely]] {
                return 0;
            }

            new (lvl) PriceLevel{};
            lvl->reset(px, s);
            insert_level_sorted(s, lvl);
            insert_level_into_map(s, lvl);
            created = true;
        }

        lvl->push_back(o);   // sets o->level_
        ++live_order_count_;
        return created ? 2 : 1;
    }

    // Apply a (partial) fill.
    // Returns -1 if the order is not linked to a level,
    //         0 if partially filled (order still live),
    //         1 if fully filled and the level remains,
    //         2 if fully filled and the level was removed.
    int execute_order(Order* o, Shares qty,
                      MemoryPool<PriceLevel>& level_pool) noexcept {
        PriceLevel* lvl = o->level_;        // direct - no hash lookup
        if (!lvl) [[unlikely]] return -1;

        const Shares filled = (qty >= o->shares_remaining)
                            ? o->shares_remaining : qty;
        lvl->total_shares -= filled;
        const bool now_dead = o->reduce(qty);

        if (!now_dead) {
            return 0;
        }
        lvl->unlink(o);                     // deducts 0: remainder is 0
        --live_order_count_;
        if (lvl->empty()) {
            remove_level_from_list(o->side, lvl);
            erase_level_from_map(o->side, lvl);
            level_pool.release(lvl);
            return 2;
        }

        return 1;
    }

    // Cancel uses the same logic as execute.
    int cancel_order(Order* o, Shares qty,
                     MemoryPool<PriceLevel>& level_pool) noexcept {
        return execute_order(o, qty, level_pool);
    }

    // Remove an order completely.
    // Returns 0 if the order is not linked to a level,
    //         1 if removed and the level remains,
    //         2 if removed and the level was removed.
    int remove_order(Order* o, MemoryPool<PriceLevel>& level_pool) noexcept {
        PriceLevel* lvl = o->level_;        // direct - no hash lookup
        if (!lvl) [[unlikely]] {
            return 0;
        }
        lvl->unlink(o);                     // deducts the full remainder
        --live_order_count_;
        if (lvl->empty()) {
            remove_level_from_list(o->side, lvl);
            erase_level_from_map(o->side, lvl);
            level_pool.release(lvl);
            return 2;
        }
        return 1;
    }

    [[nodiscard]] Price best_bid_price() const noexcept {
        return best_bid_ ? best_bid_->price_cents : 0;
    }

    [[nodiscard]] Price best_ask_price() const noexcept {
        return best_ask_ ? best_ask_->price_cents : 0;
    }

    [[nodiscard]] std::uint64_t best_bid_shares() const noexcept {
        return best_bid_ ? best_bid_->total_shares : 0;
    }

    [[nodiscard]] std::uint64_t best_ask_shares() const noexcept {
        return best_ask_ ? best_ask_->total_shares : 0;
    }

    [[nodiscard]] std::size_t live_orders() const noexcept {
        return live_order_count_;
    }

    [[nodiscard]] std::size_t live_levels() const noexcept {
        return bid_level_count_ + ask_level_count_;
    }

    [[nodiscard]] const PriceLevel* best_bid_level() const noexcept {
        return best_bid_;
    }

    [[nodiscard]] const PriceLevel* best_ask_level() const noexcept {
        return best_ask_;
    }

    // Visit every live order (bids then asks, best price first).
    template <typename Fn>
    void for_each_order(Fn&& fn) const {
        for (const PriceLevel* lvl = best_bid_; lvl; lvl = lvl->next_level) {
            for (const Order* o = lvl->head_order; o; o = o->next_) {
                fn(const_cast<Order*>(o));
            }
        }

        for (const PriceLevel* lvl = best_ask_; lvl; lvl = lvl->next_level) {
            for (const Order* o = lvl->head_order; o; o = o->next_) {
                fn(const_cast<Order*>(o));
            }
        }
    }

    // Release all price levels (used by the closing-cross drain).
    void clear_levels(MemoryPool<PriceLevel>& level_pool) noexcept {
        PriceLevel* lvl = best_bid_;
        while (lvl) {
            PriceLevel* next = lvl->next_level;
            level_pool.release(lvl);
            lvl = next;
        }

        lvl = best_ask_;
        while (lvl) {
            PriceLevel* next = lvl->next_level;
            level_pool.release(lvl);
            lvl = next;
        }

        reset_metadata();
    }

    void clear_and_release(MemoryPool<PriceLevel>& level_pool) noexcept {
        clear_levels(level_pool);
    }

    void reset_metadata() noexcept {
        best_bid_  = nullptr;
        best_ask_  = nullptr;
        bid_level_count_ = 0;
        ask_level_count_ = 0;
        live_order_count_ = 0;
        bid_map_.clear();
        ask_map_.clear();
    }

private:
    // Only the ADD path needs a price->level lookup now.
    [[nodiscard]] void* find_level(Side s, Price px) const noexcept {
        return (s == Side::Buy) ? bid_map_.find(px) : ask_map_.find(px);
    }

    void insert_level_into_map(Side s, PriceLevel* lvl) noexcept {
        if (s == Side::Buy) {
            bid_map_.insert(lvl->price_cents, lvl);
            ++bid_level_count_;
        } else {
            ask_map_.insert(lvl->price_cents, lvl);
            ++ask_level_count_;
        }
    }

    void erase_level_from_map(Side s, PriceLevel* lvl) noexcept {
        if (s == Side::Buy) {
            bid_map_.erase(lvl->price_cents);
            --bid_level_count_;
        } else {
            ask_map_.erase(lvl->price_cents);
            --ask_level_count_;
        }
    }

    // Insert level in price order (bids descending, asks ascending).
    // Linear scan from the best end: book activity concentrates near the
    // top of the book, so the scan is short in practice. (If profiling
    // ever shows this hot, the fix is a skiplist or a per-symbol price
    // bucket array - see the guide's performance section.)
    void insert_level_sorted(Side s, PriceLevel* lvl) noexcept {
        if (s == Side::Buy) {
            PriceLevel* prev = nullptr;
            PriceLevel* cur  = best_bid_;
            while (cur && cur->price_cents > lvl->price_cents) {
                prev = cur;
                cur  = cur->next_level;
            }

            lvl->prev_level = prev;
            lvl->next_level = cur;
            if (prev) prev->next_level = lvl;
            else      best_bid_        = lvl;
            if (cur)  cur->prev_level  = lvl;
        } else {
            PriceLevel* prev = nullptr;
            PriceLevel* cur  = best_ask_;
            while (cur && cur->price_cents < lvl->price_cents) {
                prev = cur;
                cur  = cur->next_level;
            }

            lvl->prev_level = prev;
            lvl->next_level = cur;
            if (prev) prev->next_level = lvl;
            else      best_ask_        = lvl;
            if (cur)  cur->prev_level  = lvl;
        }
    }

    void remove_level_from_list(Side s, PriceLevel* lvl) noexcept {
        if (lvl->prev_level) lvl->prev_level->next_level = lvl->next_level;
        else {
            if (s == Side::Buy) best_bid_ = lvl->next_level;
            else                best_ask_ = lvl->next_level;
        }

        if (lvl->next_level) lvl->next_level->prev_level = lvl->prev_level;
        lvl->prev_level = nullptr;
        lvl->next_level = nullptr;
    }

    LevelMap    bid_map_{};   // FlatMap<Price, PriceLevel*> - grows on demand
    LevelMap    ask_map_{};

    PriceLevel* best_bid_{nullptr};
    PriceLevel* best_ask_{nullptr};

    std::size_t bid_level_count_{0};
    std::size_t ask_level_count_{0};
    std::size_t live_order_count_{0};
};

} // namespace itch