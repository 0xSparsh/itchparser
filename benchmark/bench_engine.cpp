#include <benchmark/benchmark.h>

#include "Logger.hpp"
#include "MatchingEngine.hpp"
#include "Types.hpp"

#include <cstddef>
#include <cstdint>

namespace itch {
namespace {

// Every benchmark uses 5000 orders per iteration.
constexpr std::size_t kOrders = 5000;

inline const MPID kSpaces{' ', ' ', ' ', ' '};

// One shared engine. Order ids keep increasing (base += kOrders) so an
// iteration never re-adds an id that is still live from before.
struct SharedEngine {
    LockFreeLogger logger{"/dev/null"};  // never started: no log thread
    MatchingEngine engine{logger, 1u << 20, 1u << 18};
    std::uint64_t base{1};
};

// Time: adding orders.
static void BM_Engine_Add(benchmark::State& state) {
    SharedEngine e;
    for (auto _ : state) {
        state.PauseTiming();
        const std::uint64_t b = e.base;
        e.base += kOrders;
        state.ResumeTiming();
        for (std::size_t i = 0; i < kOrders; ++i) {
            e.engine.on_add_order(1, b + i, BuySellIndicator::BuyOrder, 100,
                                  15000 + static_cast<Price>(i % 32), i,
                                  kSpaces, false);
        }
        benchmark::ClobberMemory();
        state.PauseTiming();
        for (std::size_t i = 0; i < kOrders; ++i)
            e.engine.on_order_delete(1, b + i);  // reset for next round
        state.ResumeTiming();
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) *
                            static_cast<std::int64_t>(kOrders));
}
BENCHMARK(BM_Engine_Add);

// Time: a full lifecycle per order (add -> execute part -> cancel part ->
// delete rest). Net zero, so ids can be reused every iteration.
static void BM_Engine_Lifecycle(benchmark::State& state) {
    SharedEngine e;
    for (auto _ : state) {
        for (std::size_t i = 0; i < kOrders; ++i) {
            const std::uint64_t id = 1 + i;
            e.engine.on_add_order(1, id, BuySellIndicator::BuyOrder, 100,
                                  15000, i, kSpaces, false);
            e.engine.on_order_executed(1, id, 10, 9000 + i);
            e.engine.on_order_cancel(1, id, 10);
            e.engine.on_order_delete(1, id);
        }
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) *
                            static_cast<std::int64_t>(4 * kOrders));
}
BENCHMARK(BM_Engine_Lifecycle);

// Time: replacing orders (cancel old id, add new id, same side).
static void BM_Engine_Replace(benchmark::State& state) {
    SharedEngine e;
    for (auto _ : state) {
        state.PauseTiming();
        for (std::size_t i = 0; i < kOrders; ++i)
            e.engine.on_add_order(1, 1 + i, BuySellIndicator::BuyOrder, 100,
                                  15000, i, kSpaces, false);
        state.ResumeTiming();
        for (std::size_t i = 0; i < kOrders; ++i)
            e.engine.on_order_replace(1, 1 + i, 1000000 + i, 100, 15010,
                                      200000 + i);
        benchmark::ClobberMemory();
        state.PauseTiming();
        for (std::size_t i = 0; i < kOrders; ++i)
            e.engine.on_order_delete(1, 1000000 + i);  // reset
        state.ResumeTiming();
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) *
                            static_cast<std::int64_t>(kOrders));
}
BENCHMARK(BM_Engine_Replace);

// Time: the closing cross, which wipes the whole book for an instrument.
static void BM_Engine_ClosingCross(benchmark::State& state) {
    SharedEngine e;
    for (auto _ : state) {
        state.PauseTiming();
        for (std::size_t i = 0; i < kOrders; ++i)
            e.engine.on_add_order(1, 1 + i, BuySellIndicator::BuyOrder, 100,
                                  15000 + static_cast<Price>(i % 64), i,
                                  kSpaces, false);
        state.ResumeTiming();
        e.engine.on_cross_trade(1, CrossType::Closing);
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) *
                            static_cast<std::int64_t>(kOrders));
}
BENCHMARK(BM_Engine_ClosingCross);

}  // namespace
}  // namespace itch

BENCHMARK_MAIN();
