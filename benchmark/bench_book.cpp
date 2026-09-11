#include <benchmark/benchmark.h>

#include "MemoryPool.hpp"
#include "Order.hpp"
#include "OrderBook.hpp"
#include "PriceLevel.hpp"
#include "Types.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace itch {
namespace {

// Every benchmark uses 5000 orders per iteration.
constexpr std::size_t kOrders = 5000;

inline const MPID kSpaces{' ', ' ', ' ', ' '};

// Add n orders at one price into the book. Returns the Order pointers so the
// caller can clean up afterwards.
inline std::vector<Order*> fill_book(OrderBook& book, MemoryPool<Order>& orders,
                                     MemoryPool<PriceLevel>& levels,
                                     std::size_t n, Price price) {
    std::vector<Order*> ptrs;
    ptrs.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        Order* o = new (orders.acquire())
            Order(1 + i, 1, price, 100, Side::Buy, i, kSpaces);
        book.add_order(o, levels);
        ptrs.push_back(o);
    }
    return ptrs;
}

// Remove every order and hand the slots back to the pools.
inline void empty_book(OrderBook& book, MemoryPool<Order>& orders,
                       MemoryPool<PriceLevel>& levels,
                       std::vector<Order*>& ptrs) {
    for (Order* o : ptrs) {
        book.remove_order(o, levels);
        orders.release(o);
    }
    ptrs.clear();
}

// Time: adding orders to an existing price level (the common case).
static void BM_Book_Add(benchmark::State& state) {
    MemoryPool<Order> orders(kOrders + 64);
    MemoryPool<PriceLevel> levels(64);
    OrderBook book;
    for (auto _ : state) {
        state.PauseTiming();  // setup is not measured
        state.ResumeTiming();  // only the adds below are measured
        std::vector<Order*> ptrs = fill_book(book, orders, levels, kOrders, 15000);
        benchmark::ClobberMemory();
        state.PauseTiming();  // cleanup is not measured either
        empty_book(book, orders, levels, ptrs);
        state.ResumeTiming();
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) *
                            static_cast<std::int64_t>(kOrders));
}
BENCHMARK(BM_Book_Add);

// Time: add an order, then fully execute it (net zero, nothing to clean up).
static void BM_Book_AddExecute(benchmark::State& state) {
    MemoryPool<Order> orders(kOrders + 64);
    MemoryPool<PriceLevel> levels(kOrders + 64);
    OrderBook book;
    for (auto _ : state) {
        for (std::size_t i = 0; i < kOrders; ++i) {
            Order* o = new (orders.acquire())
                Order(1 + i, 1, 15000 + static_cast<Price>(i % 16), 100,
                      Side::Buy, i, kSpaces);
            book.add_order(o, levels);
            book.execute_order(o, 100, levels);  // full fill removes it
            orders.release(o);
        }
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) *
                            static_cast<std::int64_t>(2 * kOrders));
}
BENCHMARK(BM_Book_AddExecute);

// Time: add an order, then delete it (net zero, nothing to clean up).
static void BM_Book_AddDelete(benchmark::State& state) {
    MemoryPool<Order> orders(kOrders + 64);
    MemoryPool<PriceLevel> levels(kOrders + 64);
    OrderBook book;
    for (auto _ : state) {
        for (std::size_t i = 0; i < kOrders; ++i) {
            Order* o = new (orders.acquire())
                Order(1 + i, 1, 15000 + static_cast<Price>(i % 16), 100,
                      Side::Buy, i, kSpaces);
            book.add_order(o, levels);
            book.remove_order(o, levels);
            orders.release(o);
        }
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) *
                            static_cast<std::int64_t>(2 * kOrders));
}
BENCHMARK(BM_Book_AddDelete);

// Time: reading best bid/ask (top of book). The ClobberMemory() inside the
// loop matters: without it the compiler notices the book never changes and
// hoists the reads out of the loop, reporting a fake sub-cycle time.
static void BM_Book_TopOfBook(benchmark::State& state) {
    MemoryPool<Order> orders(kOrders + 64);
    MemoryPool<PriceLevel> levels(1024);
    OrderBook book;
    std::vector<Order*> ptrs = fill_book(book, orders, levels, kOrders, 15000);
    std::uint64_t acc = 0;
    for (auto _ : state) {
        for (std::size_t i = 0; i < kOrders; ++i) {
            acc += book.best_bid_price() + book.best_ask_shares();
            benchmark::ClobberMemory();
        }
    }
    benchmark::DoNotOptimize(acc);
    empty_book(book, orders, levels, ptrs);
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) *
                            static_cast<std::int64_t>(kOrders));
}
BENCHMARK(BM_Book_TopOfBook);

}  // namespace
}  // namespace itch

BENCHMARK_MAIN();
