#pragma once

#include "Types.hpp"

#include <cstdint>
#include <type_traits>

namespace itch {

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
    {}

    Order() noexcept = default;

    // Tiny helper to check if the order is completely filled
    [[nodiscard]] bool dead() const noexcept {
        return shares_remaining == 0;
    }

    bool cancel(Shares qty) noexcept {
        if (qty >= shares_remaining) [[unlikely]] {
            shares_remaining = 0;
            return true;
        }

        shares_remaining -= qty;
        return false;
    }
};

static_assert(sizeof(Order) <= kCacheLineSize,
              "Order must fit in a cache line to avoid false sharing.");
    
static_assert(std::is_trivially_copyable_v<Order>,
              "Must be trivially copyable because MemoryPool is just raw bytes.");

} // namespace itch