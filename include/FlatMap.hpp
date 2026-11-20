#pragma once

#include "Types.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>

namespace itch {

struct Order;       // forward decl - OrderIndex stores Order*
struct PriceLevel;  // forward decl - LevelMap stores PriceLevel*

template <typename KeyT, typename ValT,
          KeyT kEmpty, std::size_t kInitialSlots = 64>
class FlatMap {
    static_assert(kInitialSlots >= 16 && (kInitialSlots & (kInitialSlots - 1)) == 0,
                  "kInitialSlots must be a power of two >= 16");
    static_assert(std::is_trivially_copyable_v<KeyT> && std::is_trivially_copyable_v<ValT>,
                  "FlatMap is a raw-byte table; keys/values must be trivially copyable");

public:
    FlatMap() noexcept = default;
    ~FlatMap() = default;

    FlatMap(const FlatMap&) = delete;
    FlatMap& operator=(const FlatMap&) = delete;

    // nullptr when absent (pointer value types).
    [[nodiscard]] ValT find(KeyT k) const noexcept {
        if (slots_ == nullptr) [[unlikely]] return ValT{};
        const std::size_t mask = cap_ - 1;
        std::size_t i = hash(k) & mask;
        while (true) {
            const Entry& e = slots_[i];
            if (e.key == kEmpty) return ValT{};
            if (e.key == k) return e.val;
            i = (i + 1) & mask;
        }
    }

    // Returns true  -> inserted.
    // Returns false -> key already present; value is NOT overwritten.
    bool insert(KeyT k, ValT v) noexcept {
        if (slots_ == nullptr) [[unlikely]] {
            allocate(kInitialSlots);
        }
        else if ((size_ + 1) * 10 >= cap_ * 7) [[unlikely]] {
            grow();
        }

        const std::size_t mask = cap_ - 1;
        std::size_t i = hash(k) & mask;
        while (true) {
            Entry& e = slots_[i];
            if (e.key == kEmpty) {
                e.key = k;
                e.val = v;
                ++size_;
                return true;
            }
            if (e.key == k) {
                return false;   // duplicate: caller decides what to do
            }
            i = (i + 1) & mask;
        }
    }

    // Backward-shift deletion: entries that would become unreachable when
    // this slot is emptied are moved back, so future probes never stop
    // early.
    bool erase(KeyT k) noexcept {
        if (slots_ == nullptr) [[unlikely]] return false;
        const std::size_t mask = cap_ - 1;
        std::size_t i = hash(k) & mask;

        // find the key first
        while (true) {
            Entry& e = slots_[i];
            if (e.key == kEmpty) return false;
            if (e.key == k) {
                e.key = kEmpty;
                e.val = ValT{};
                --size_;
                break;
            }
            i = (i + 1) & mask;
        }

        // repair the probe chain starting after slot i
        std::size_t j = (i + 1) & mask;
        while (slots_[j].key != kEmpty) {
            Entry& moved = slots_[j];
            const std::size_t home = hash(moved.key) & mask;
            // Move `moved` back into slot i unless its home position lies
            // cyclically in (i, j] - if it does, moving it would put it in
            // front of its home and break its own probe chain.
            const bool home_in_interval =
                (i < j) ? (home > i && home <= j)
                        : (home > i || home <= j);
            if (!home_in_interval) {
                slots_[i] = moved;
                moved.key = kEmpty;
                moved.val = ValT{};
                i = j;
            }
            j = (j + 1) & mask;
        }
        return true;
    }

    void clear() noexcept {
        if (slots_ == nullptr) return;
        for (std::size_t i = 0; i < cap_; ++i) {
            slots_[i].key = kEmpty;
            slots_[i].val = ValT{};
        }
        size_ = 0;
    }

    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    [[nodiscard]] std::size_t capacity() const noexcept { return cap_; }

    template <typename Fn>
    void for_each(Fn&& fn) const {
        if (slots_ == nullptr) return;
        for (std::size_t i = 0; i < cap_; ++i) {
            if (slots_[i].key != kEmpty) fn(slots_[i].key, slots_[i].val);
        }
    }

private:
    struct Entry {
        KeyT key;
        ValT val;
    };

    static constexpr std::uint64_t hash(KeyT k) noexcept {
        // Fibonacci hashing: one multiply, one shift. Good enough for
        // sequential ids and dense price keys alike.
        std::uint64_t x = static_cast<std::uint64_t>(k);
        x ^= x >> 33;
        x *= 0xff51afd7ed558ccdULL;
        x ^= x >> 33;
        x *= 0xc4ceb9fe1a85ec53ULL;
        x ^= x >> 33;
        return x;
    }

    void allocate(std::size_t new_cap) noexcept {
        assert((new_cap & (new_cap - 1)) == 0);
        slots_ = std::make_unique<Entry[]>(new_cap);  // value-initialized
        for (std::size_t i = 0; i < new_cap; ++i) {
            slots_[i].key = kEmpty;   // make_unique zero-fills; kEmpty may not be 0
            slots_[i].val = ValT{};
        }
        cap_ = new_cap;
    }

    void grow() noexcept {
        std::unique_ptr<Entry[]> old = std::move(slots_);
        const std::size_t old_cap = cap_;
        allocate(cap_ * 2);

        const std::size_t mask = cap_ - 1;
        for (std::size_t oi = 0; oi < old_cap; ++oi) {
            if (old[oi].key == kEmpty) continue;
            std::size_t i = hash(old[oi].key) & mask;
            while (slots_[i].key != kEmpty) i = (i + 1) & mask;
            slots_[i] = old[oi];
        }
        // size_ is unchanged
    }

    std::unique_ptr<Entry[]> slots_{nullptr};
    std::size_t cap_{0};
    std::size_t size_{0};
};

// The two project-wide instantiations.
// LevelMap: price -> level, one pair per OrderBook side. 64 initial slots
// (1 KiB) covers the vast majority of books without ever growing.
using LevelMap = FlatMap<Price, PriceLevel*, kEmptyPriceKey, 64>;

// OrderIndex: order id -> order, shared across all books. Starts at 64 Ki
// slots (1 MiB) and doubles as the live-order count grows.
using OrderIndex = FlatMap<OrderReferenceNumber, Order*, 0, 1u << 16>;

} // namespace itch
