#pragma once

#include "Order.hpp"
#include "Types.hpp"

#include <cstdint>

namespace itch {

struct PriceLevel {
    Price       price_cents;
    uint64_t    total_shares;
    Order*      head_order;
    Order*      tail_order;
    uint32_t    order_count;
    Side        side;

    // Intrusive doubly linked-list for acive prices
    PriceLevel* prev_level;
    PriceLevel* next_level;

    PriceLevel() noexcept
        : price_cents(0)
        , total_shares(0)
        , head_order(nullptr)
        , tail_order(nullptr)
        , order_count(0)
        , side(Side::Buy)
        , prev_level(nullptr)
        , next_level(nullptr)
    {}

    void reset(Price px, Side s) noexcept {
        price_cents  = px;
        total_shares = 0;
        head_order   = nullptr;
        tail_order   = nullptr;
        order_count  = 0;
        side         = s;
        prev_level   = nullptr;
        next_level   = nullptr;
    }

    void push_back(Order* o) noexcept {
        o->next_ = nullptr;
        o->prev_ = tail_order;

        if (tail_order) {
            tail_order->next_ = o;
        }
        else {
            head_order = o;
        }

        tail_order = o;
        total_shares += o->shares_remaining;
        ++order_count;
    }

    void unlink(Order* o) noexcept {
        if (o->prev_) {
            o->prev_->next_ = o->next_;
        }
        else {
            head_order = o->next_;
        }

        if (o->next_) {
            o->next_->prev_ = o->prev_;
        }
        else {
            tail_order = o->prev_;
        }

        o->next_ = nullptr;
        o->prev_ = nullptr;
        --order_count;
    }

    void unlink_and_deduct(Order* o) noexcept {
        total_shares = o->shares_remaining;
        unlink(o);
    }

    [[nodiscard]] bool empty() const noexcept {
        return order_count == 0;
    }
};

static_assert(sizeof(PriceLevel) <= 2 * kCacheLineSize,
              "Price Level grew beyond 2 cache lines");

} // namespace itch