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

// Implementing a flat hash map for hot path lookups

// KeyT      - key
// ValueT    - value
// kCapacity - number of slots in table
// kEmpty    - special key value meaning the slot is empty

// usage :- FlatHashMap(uint64_t, Foo*, 1024, 0)
// kEmpty = 0 means the slot is empty
// RESTRICTION :- the map cannot legitimately store 0 as a key

template <typename KeyT, typename ValueT,
           std::size_t kCapacity, KeyT kEmpty>
class FlatHashMap {
    static_assert(kCapacity & (kCapacity - 1) == 0,
                  "kCapacity must be a power of 2");
public:

    // This default constructor makes two vectors keys_ and values_
    // The keys_ vector will be of size = kCapacity and the values will be intialized to 0
    // The values_ vector will be of capacity of size = kCapacity and the values will be initialized to nullptr
    FlatHashMap() noexcept
        : keys_(kCapacity, kEmpty)
        , values_(kcapacity, nullptr)
    {}

    bool insert(KeyT k, ValueT v) noexcept {
        const std::size_t mask = kCapacity - 1;
        std::size_t i = hash(k) & mask;
        for (std::size_t probe = 0; probe < kCapacity; ++probe) {
            if (keys_[i] == kEmpty) {
                keys_[i]   = k;
                values_[i] = v;
                ++size_;
                return true;
            }

            if (keys_[i] == k) {
                values_[i] = v;
                return false;
            }

            i = (i + 1) & mask;
        }

        assert(false && "FlatHashMap full — increase capacity");
        return false;
    }

    // Returns nullptr if not found.
    [[nodiscard]] ValueT find(KeyT k) const noexcept {
        const std::size_t mask = kCapacity - 1;
        std::size_t i = hash(k) & mask;
        for (std::size_t probe = 0; probe < kCapacity; ++probe) {
            if (keys_[i] == kEmpty) return nullptr;
            if (keys_[i] == k) return values_[i];
            i = (i + 1) & mask;
        }
        return nullptr;
    }

    // Remove a key and repair the probe chain.
    void erase(KeyT k) noexcept {
        const std::size_t mask = kCapacity - 1;
        std::size_t i = hash(k) & mask;
        for (std::size_t probe = 0; probe < kCapacity; ++probe) {
            if (keys_[i] == kEmpty) return;
            if (keys_[i] == k) {
                keys_[i]   = kEmpty;
                values_[i] = nullptr;
                --size_;

                std::size_t j = (i + 1) & mask;
                while (keys_[j] != kEmpty) {
                    KeyT   rk = keys_[j];
                    ValueT rv = values_[j];
                    keys_[j]   = kEmpty;
                    values_[j] = nullptr;
                    --size_;
                    insert(rk, rv);
                    j = (j + 1) & mask;
                }
                return;
            }

            i = (i + 1) & mask;
        }
    }

    void clear() noexcept {
        std::fill(keys_.begin(), keys_.end(), kEmpty);
        std::fill(values_.begin(), values_.end(), nullptr);
        size_ = 0;
    }

    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    static constexpr std::size_t Capacity = kCapacity;

    template <typename Fn>
    void for_each(Fn&& fn) const {
        for (std::size_t i = 0; i < kCapacity; ++i) {
            if (keys_[i] != kEmpty) {
                fn(keys_[i], values_[i]);
            }
        }
    }

private:
    static constexpr KeyT hash(KeyT k) noexcept {
        std::uint64_t x = static_cast<std::uint64_t>(k);
        x ^= x >> 33;
        x *= 0xff51afd7ed558ccdULL;
        x ^= x >> 33;
        x *= 0xc4ceb9fe1a85ec53ULL;
        x ^= x >> 33;
        return static_cast<KeyT>(x);
    }

    std::vector<KeyT>    keys_;
    std::vector<ValueT>  values_;
    std::size_t          size_{0};
};

// Per-instrument resting state.
class OrderBook {
public:
    static constexpr std::size_t kPriceLevelMapCapacity = 1u << 12;

    static constexpr Price   kEmptyPrice   = static_cast<Price>(-1);
    static constexpr OrderReferenceNumber kEmptyOrderId =
        static_cast<OrderReferenceNumber>(-1);

    using LevelMap = FlatHashMap<Price, PriceLevel*,
                                 kPriceLevelMapCapacity, kEmptyPrice>;

    OrderBook() noexcept = default;

    OrderBook(const OrderBook&)            = delete;
    OrderBook& operator=(const OrderBook&) = delete;

    // Add a resting order.
    bool add_order(Order* o, MemoryPool<PriceLevel>& level_pool) {
        const Side  s  = o->side;
        const Price px = o->price_cents;

        PriceLevel* lvl = find_level(s, px);
        if (!lvl) [[unlikely]] {
            lvl = level_pool.acquire();
            if (!lvl) [[unlikely]] {
                return false;
            }

            new (lvl) PriceLevel{};
            lvl->reset(px, s);
            insert_level_sorted(s, lvl);
            insert_level_into_map(s, lvl);
        }

        lvl->push_back(o);
        ++live_order_count_;
        return true;
    }

    // Apply a partial fill.
    bool execute_order(Order* o, Shares qty,
                       MemoryPool<PriceLevel>& level_pool) noexcept {
        PriceLevel* lvl = find_level(o->side, o->price_cents);
        if (!lvl) [[unlikely]] return false;

        const Shares filled = (qty >= o->shares_remaining)
                            ? o->shares_remaining : qty;
        lvl->total_shares -= filled;
        const bool now_dead = o->reduce(qty);

        if (now_dead) {
            lvl->unlink(o);
            --live_order_count_;
            if (lvl->empty()) {
                remove_level_from_list(o->side, lvl);
                erase_level_from_map(o->side, lvl);
                level_pool.release(lvl);
            }
        }

        return now_dead;
    }

    // Cancel uses the same logic as execute.
    bool cancel_order(Order* o, Shares qty,
                      MemoryPool<PriceLevel>& level_pool) noexcept {
        return execute_order(o, qty, level_pool);
    }

    // Remove an order completely.
    void remove_order(Order* o, MemoryPool<PriceLevel>& level_pool) noexcept {
        PriceLevel* lvl = find_level(o->side, o->price_cents);
        if (lvl) {
            lvl->unlink_and_deduct(o);
            --live_order_count_;
            if (lvl->empty()) {
                remove_level_from_list(o->side, lvl);
                erase_level_from_map(o->side, lvl);
                level_pool.release(lvl);
            }
        }
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

    // Visit every live order.
    template <typename Fn>
    void for_each_order(Fn&& fn) const {
        for (PriceLevel* lvl = best_bid_; lvl; lvl = lvl->next_level) {
            for (Order* o = lvl->head_order; o; o = o->next_) {
                fn(o);
            }
        }

        for (PriceLevel* lvl = best_ask_; lvl; lvl = lvl->next_level) {
            for (Order* o = lvl->head_order; o; o = o->next_) {
                fn(o);
            }
        }
    }

    // Release all price levels.
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

        best_bid_  = nullptr;
        best_ask_  = nullptr;
        bid_level_count_ = 0;
        ask_level_count_ = 0;
        live_order_count_ = 0;
        bid_map_.clear();
        ask_map_.clear();
    }

    void clear_and_release(MemoryPool<PriceLevel>& level_pool) noexcept {
        clear_levels(level_pool);
    }

    void clear_metadata() noexcept {
        best_bid_  = nullptr;
        best_ask_  = nullptr;
        bid_level_count_ = 0;
        ask_level_count_ = 0;
        live_order_count_ = 0;
        bid_map_.clear();
        ask_map_.clear();
    }

private:
    [[nodiscard]] PriceLevel* find_level(Side s, Price px) const noexcept {
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

    // Insert level in price order.
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

    LevelMap    bid_map_{};
    LevelMap    ask_map_{};

    PriceLevel* best_bid_{nullptr};
    PriceLevel* best_ask_{nullptr};

    std::size_t bid_level_count_{0};
    std::size_t ask_level_count_{0};
    std::size_t live_order_count_{0};
};

} // namespace itch