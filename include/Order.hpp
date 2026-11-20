#pragma once

#include "Types.hpp"

#include <cstdint>
#include <type_traits>

namespace itch {

struct PriceLevel;  // forward declaration - back-pointer only

struct alignas(kCacheLineSize) Order {
    OrderReferenceNumber order_id;
    StockLocate          locate;

    Price                price_cents;       // Price is in Price(4) per spec
    Shares               shares_remaining;
    Side                 side;
    Timestamp            added_at_ns;

    MPID                 mpid;

    Order* next_;
    Order* prev_;

    // The level this order currently rests in. Maintained by
    // PriceLevel::push_back / unlink; lets Execute/Cancel/Delete reach the
    // level directly instead of going through the price -> level map.
    PriceLevel* level_;

    Order (OrderReferenceNumber id, StockLocate loc, Price px,
           Shares qty, Side s, Timestamp ts, MPID attribution) noexcept
        : order_id(id)
        , locate(loc)
        , price_cents(px)
        , shares_remaining(qty)
        , side(s)
        , added_at_ns(ts)
        , mpid(attribution)
        , next_(nullptr)
        , prev_(nullptr)
        , level_(nullptr)
    {}

    Order() noexcept = default;

    // Tiny helper to check if the order is completely filled
    [[nodiscard]] bool dead() const noexcept {
        return shares_remaining == 0;
    }

    bool reduce(Shares qty) noexcept {
        if (qty >= shares_remaining) [[unlikely]] {
            shares_remaining = 0;
            return true;
        }

        shares_remaining -= qty;
        return false;
    }
};

static_assert(sizeof(Order) == kCacheLineSize,
              "Order must exactly fill one cache line (64 bytes): "
              "id(8)+locate(2)+pad(2)+price(4)+shares(4)+side(1)+pad(3)+"
              "ts(8)+mpid(4)+pad(4)+next(8)+prev(8)+level(8) == 64");

static_assert(std::is_trivially_copyable_v<Order>,
              "Must be trivially copyable because MemoryPool is just raw bytes.");

} // namespace itch