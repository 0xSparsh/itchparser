#include <gtest/gtest.h>

#include "MatchingEngine.hpp"
#include "test_helper.hpp"

namespace itch {
namespace {

using test::TestEngine;

static void reg(TestEngine& t, StockLocate loc = 1, const char* sym = "AAPL") {
    char s[8];
    std::memset(s, ' ', 8);
    std::memcpy(s, sym, std::min(std::strlen(sym), std::size_t(8)));
    t.engine.on_stock_directory(
        loc, s, MarketCategory::NasdaqSelect,
        FinancialStatusIndicator::Normal, 100, RoundLotsOnly::No,
        Authenticity::Production,
        ShortScaleThresholdIndicator::NotRestricted, IPOFlag::NotNewIPO,
        LULDReferencePriceTier::Tier1, ETPFlag::NonETP, 1,
        InverseIndicator::NotInverseETP);
}

static MPID spaces() { return MPID{' ', ' ', ' ', ' '}; }

TEST(Engine, StockDirectoryRegistersInstrument) {
    TestEngine t;
    reg(t, 5, "MSFT");
    EXPECT_NE(t.engine.instruments()[5], nullptr);
    EXPECT_EQ(t.engine.stats().instruments_registered, 1u);
    EXPECT_EQ(std::memcmp(t.engine.instruments()[5]->symbol.data(), "MSFT    ", 8), 0);
    EXPECT_EQ(t.engine.instruments()[5]->roundLotSize, 100u);
}

TEST(Engine, DuplicateDirectoryDoesNotDoubleCount) {
    TestEngine t;
    reg(t, 1);
    reg(t, 1);
    EXPECT_EQ(t.engine.stats().instruments_registered, 1u);
}

TEST(Engine, TradingActionUpdatesState) {
    TestEngine t;
    reg(t, 1);
    t.engine.on_trading_action(1, TradingState::Trading, "AAPL", "    ");
    EXPECT_EQ(t.engine.instruments()[1]->tradingState, TradingState::Trading);
}

TEST(Engine, TradingActionUnknownLocateIgnored) {
    TestEngine t;
    EXPECT_NO_FATAL_FAILURE(
        t.engine.on_trading_action(999, TradingState::Trading, "ZZZZ", "    "));
    EXPECT_EQ(t.engine.instruments()[999], nullptr);
}

TEST(Engine, RegSHOUpdatesAction) {
    TestEngine t;
    reg(t, 1);
    t.engine.on_reg_sho(1, RegSHOAction::PriceTestInEffect);
    EXPECT_EQ(t.engine.instruments()[1]->regSHOAction,
              RegSHOAction::PriceTestInEffect);
}

TEST(Engine, AddOrderCreatesBookAndLevel) {
    TestEngine t;
    reg(t, 1);
    t.engine.on_add_order(1, 1001, BuySellIndicator::BuyOrder, 100, 15000, 1,
                          spaces(), false);
    auto* inst = t.engine.instruments()[1];
    ASSERT_NE(inst->book, nullptr);
    EXPECT_EQ(inst->book->best_bid_price(), 15000u);
    EXPECT_EQ(inst->book->best_bid_shares(), 100u);
    EXPECT_EQ(t.engine.stats().add_orders, 1u);
    EXPECT_EQ(t.engine.stats().live_orders, 1u);
    EXPECT_EQ(t.engine.stats().live_levels, 1u);
    EXPECT_EQ(t.engine.stats().peak_live_orders, 1u);
}

TEST(Engine, AddOrderWithMPIDAttributed) {
    TestEngine t;
    reg(t, 1);
    MPID m{'N', 'S', 'D', 'Q'};
    t.engine.on_add_order(1, 2001, BuySellIndicator::SellOrder, 150, 15025, 1,
                          m, true);
    EXPECT_EQ(t.engine.stats().add_orders_with_mpid, 1u);
    EXPECT_EQ(t.engine.stats().add_orders, 0u);
    auto* inst = t.engine.instruments()[1];
    Order* found = nullptr;
    inst->book->for_each_order([&](Order* o) { found = o; });
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->mpid, m);
}

TEST(Engine, AddOrderAutoCreatesInstrumentWithoutDirectory) {
    TestEngine t;  // no directory first
    t.engine.on_add_order(77, 1001, BuySellIndicator::BuyOrder, 10, 100, 1,
                          spaces(), false);
    EXPECT_NE(t.engine.instruments()[77], nullptr);
    EXPECT_EQ(t.engine.instruments()[77]->book->live_orders(), 1u);
}

TEST(Engine, DuplicateAddRejectedAndCounted) {
    TestEngine t;
    reg(t, 1);
    t.engine.on_add_order(1, 1001, BuySellIndicator::BuyOrder, 100, 15000, 1,
                          spaces(), false);
    t.engine.on_add_order(1, 1001, BuySellIndicator::BuyOrder, 100, 15000, 2,
                          spaces(), false);
    EXPECT_EQ(t.engine.stats().duplicate_adds, 1u);
    EXPECT_EQ(t.engine.stats().live_orders, 1u);  // second add dropped
}

TEST(Engine, OrderPoolExhaustionCountedNoCrash) {
    LockFreeLogger logger("/dev/null");
    MatchingEngine eng(logger, /*order pool*/ 2, /*level pool*/ 16);
    char s[8];
    std::memset(s, ' ', 8);
    std::memcpy(s, "AAPL", 4);
    eng.on_stock_directory(1, s, MarketCategory::NasdaqSelect,
                           FinancialStatusIndicator::Normal, 100,
                           RoundLotsOnly::No, Authenticity::Production,
                           ShortScaleThresholdIndicator::NotRestricted,
                           IPOFlag::NotNewIPO, LULDReferencePriceTier::Tier1,
                           ETPFlag::NonETP, 1, InverseIndicator::NotInverseETP);
    eng.on_add_order(1, 1, BuySellIndicator::BuyOrder, 10, 100, 1, spaces(), false);
    eng.on_add_order(1, 2, BuySellIndicator::BuyOrder, 10, 101, 1, spaces(), false);
    eng.on_add_order(1, 3, BuySellIndicator::BuyOrder, 10, 102, 1, spaces(), false);
    EXPECT_EQ(eng.stats().add_rejected_pool_exhausted, 1u);
    EXPECT_EQ(eng.stats().pool_exhausted, 1u);
    EXPECT_EQ(eng.stats().live_orders, 2u);
}

TEST(Engine, ExecutedPartialReducesLeavesLive) {
    TestEngine t;
    reg(t, 1);
    t.engine.on_add_order(1, 1001, BuySellIndicator::BuyOrder, 100, 15000, 1,
                          spaces(), false);
    t.engine.on_order_executed(1, 1001, 40, 9001);
    EXPECT_EQ(t.engine.stats().order_executed, 1u);
    EXPECT_EQ(t.engine.stats().live_orders, 1u);
    EXPECT_EQ(t.engine.instruments()[1]->book->best_bid_shares(), 60u);
}

TEST(Engine, ExecutedFullRemovesAndReleases) {
    TestEngine t;
    reg(t, 1);
    const std::size_t free_before = t.engine.order_pool_free();  // before add
    t.engine.on_add_order(1, 1001, BuySellIndicator::BuyOrder, 100, 15000, 1,
                          spaces(), false);
    EXPECT_EQ(t.engine.order_pool_free(), free_before - 1);
    t.engine.on_order_executed(1, 1001, 100, 9001);
    EXPECT_EQ(t.engine.stats().live_orders, 0u);
    EXPECT_EQ(t.engine.stats().live_levels, 0u);
    EXPECT_EQ(t.engine.order_pool_free(), free_before);  // slot recycled
    EXPECT_EQ(t.engine.order_index_size(), 0u);
}

TEST(Engine, ExecutedUnknownIdCounted) {
    TestEngine t;
    reg(t, 1);
    t.engine.on_order_executed(1, 424242, 10, 9001);
    EXPECT_EQ(t.engine.stats().unknown_order_events, 1u);
    EXPECT_EQ(t.engine.stats().order_executed, 0u);
}

TEST(Engine, ExecutedWithPriceSameMechanics) {
    TestEngine t;
    reg(t, 1);
    t.engine.on_add_order(1, 1001, BuySellIndicator::BuyOrder, 100, 15000, 1,
                          spaces(), false);
    t.engine.on_order_executed_with_price(1, 1001, 100, 9002, Printable::Yes,
                                          15010);
    EXPECT_EQ(t.engine.stats().order_executed_with_price, 1u);
    EXPECT_EQ(t.engine.stats().live_orders, 0u);
}

TEST(Engine, CancelPartialKeepsAlive) {
    TestEngine t;
    reg(t, 1);
    t.engine.on_add_order(1, 1002, BuySellIndicator::BuyOrder, 200, 14950, 1,
                          spaces(), false);
    t.engine.on_order_cancel(1, 1002, 25);
    EXPECT_EQ(t.engine.stats().order_cancel, 1u);
    EXPECT_EQ(t.engine.stats().live_orders, 1u);
    EXPECT_EQ(t.engine.instruments()[1]->book->best_bid_shares(), 175u);
}

TEST(Engine, CancelUnknownIdCounted) {
    TestEngine t;
    t.engine.on_order_cancel(1, 777, 10);
    EXPECT_EQ(t.engine.stats().unknown_order_events, 1u);
}

TEST(Engine, DeleteRemovesCompletely) {
    TestEngine t;
    reg(t, 1);
    t.engine.on_add_order(1, 2001, BuySellIndicator::SellOrder, 150, 15025, 1,
                          spaces(), false);
    t.engine.on_order_delete(1, 2001);
    EXPECT_EQ(t.engine.stats().order_delete, 1u);
    EXPECT_EQ(t.engine.instruments()[1]->book->best_ask_price(), 0u);
    EXPECT_EQ(t.engine.stats().live_orders, 0u);
}

TEST(Engine, DeleteUnknownIdCounted) {
    TestEngine t;
    t.engine.on_order_delete(1, 31337);
    EXPECT_EQ(t.engine.stats().unknown_order_events, 1u);
    EXPECT_EQ(t.engine.stats().order_delete, 0u);
}

TEST(Engine, LocateMismatchSelfHealsToHomeLocate) {
    TestEngine t;
    reg(t, 1);
    reg(t, 2);
    t.engine.on_add_order(1, 1001, BuySellIndicator::BuyOrder, 100, 15000, 1,
                          spaces(), false);
    // Feed claims locate=2 but order lives on locate=1: engine redirects.
    t.engine.on_order_executed(2, 1001, 100, 9001);
    EXPECT_EQ(t.engine.stats().locate_mismatches, 1u);
    EXPECT_EQ(t.engine.stats().order_executed, 1u);
    EXPECT_EQ(t.engine.stats().live_orders, 0u);
}

TEST(Engine, ReplaceMovesOrderPreservesSideAndMpid) {
    TestEngine t;
    reg(t, 1);
    MPID m{'N', 'S', 'D', 'Q'};
    t.engine.on_add_order(1, 1001, BuySellIndicator::SellOrder, 100, 15000, 1,
                          m, true);
    t.engine.on_order_replace(1, 1001, 3001, 200, 15010, 2);
    EXPECT_EQ(t.engine.stats().order_replace, 1u);
    EXPECT_EQ(t.engine.stats().live_orders, 1u);
    auto* inst = t.engine.instruments()[1];
    EXPECT_EQ(inst->book->best_ask_price(), 15010u);
    Order* found = nullptr;
    inst->book->for_each_order([&](Order* o) { found = o; });
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->order_id, 3001u);
    EXPECT_EQ(found->side, Side::Sell);  // carried over
    EXPECT_EQ(found->mpid, m);           // carried over
}

TEST(Engine, ReplaceUnknownOldIdCounted) {
    TestEngine t;
    t.engine.on_order_replace(1, 111, 222, 10, 100, 1);
    EXPECT_EQ(t.engine.stats().unknown_order_events, 1u);
    EXPECT_EQ(t.engine.stats().order_replace, 0u);
}

TEST(Engine, ReplaceDuplicateNewIdRejectedOldSurvives) {
    TestEngine t;
    reg(t, 1);
    t.engine.on_add_order(1, 1001, BuySellIndicator::BuyOrder, 100, 15000, 1,
                          spaces(), false);
    t.engine.on_add_order(1, 3001, BuySellIndicator::BuyOrder, 100, 15001, 1,
                          spaces(), false);
    t.engine.on_order_replace(1, 1001, 3001, 50, 15002, 2);  // new id live
    EXPECT_EQ(t.engine.stats().duplicate_adds, 1u);
    EXPECT_EQ(t.engine.stats().live_orders, 2u);  // old untouched
}

TEST(Engine, NonCrossTradeIsInformationalOnly) {
    TestEngine t;
    reg(t, 1);
    t.engine.on_add_order(1, 1001, BuySellIndicator::BuyOrder, 100, 15000, 1,
                          spaces(), false);
    t.engine.on_non_cross_trade(1, 100, 15005, 9002);
    EXPECT_EQ(t.engine.stats().non_cross_trades, 1u);
    EXPECT_EQ(t.engine.stats().live_orders, 1u);  // book untouched
}

TEST(Engine, BrokenTradeIsInformationalOnly) {
    TestEngine t;
    t.engine.on_broken_trade(1, 9002);
    EXPECT_EQ(t.engine.stats().broken_trades, 1u);
}

TEST(Engine, OpeningCrossDoesNotClearBook) {
    TestEngine t;
    reg(t, 1);
    t.engine.on_add_order(1, 1001, BuySellIndicator::BuyOrder, 100, 15000, 1,
                          spaces(), false);
    t.engine.on_cross_trade(1, CrossType::Opening);
    EXPECT_EQ(t.engine.stats().cross_trades, 1u);
    EXPECT_EQ(t.engine.stats().live_orders, 1u);
}

TEST(Engine, ClosingCrossWipesBookAndIndex) {
    TestEngine t;
    reg(t, 1);
    t.engine.on_add_order(1, 1001, BuySellIndicator::BuyOrder, 100, 15000, 1,
                          spaces(), false);
    t.engine.on_add_order(1, 1002, BuySellIndicator::BuyOrder, 200, 14950, 1,
                          spaces(), false);
    ASSERT_EQ(t.engine.stats().live_orders, 2u);
    t.engine.on_cross_trade(1, CrossType::Closing);
    EXPECT_EQ(t.engine.stats().cross_trades, 1u);
    EXPECT_EQ(t.engine.stats().live_orders, 0u);
    EXPECT_EQ(t.engine.stats().live_levels, 0u);
    EXPECT_EQ(t.engine.instruments()[1]->book->live_orders(), 0u);
    EXPECT_EQ(t.engine.order_index_size(), 0u);
    // Pools fully recycled 
    EXPECT_EQ(t.engine.order_pool_free(), t.engine.order_pool_capacity());
    EXPECT_EQ(t.engine.level_pool_free(), t.engine.level_pool_capacity());
}

TEST(Engine, LiveCountersMatchPoolStateInvariant) {
    TestEngine t;
    reg(t, 1);
    for (std::uint64_t i = 1; i <= 100; ++i)
        t.engine.on_add_order(1, 1000 + i, BuySellIndicator::BuyOrder, 10,
                              14000 + (i % 7), i, spaces(), false);
    EXPECT_EQ(t.engine.stats().live_orders, t.engine.order_index_size());
    EXPECT_EQ(t.engine.order_pool_capacity() - t.engine.order_pool_free(),
              t.engine.stats().live_orders);
}

TEST(Engine, PeakTrackingMonotonic) {
    TestEngine t;
    reg(t, 1);
    for (std::uint64_t i = 1; i <= 50; ++i)
        t.engine.on_add_order(1, 1000 + i, BuySellIndicator::BuyOrder, 10,
                              15000, i, spaces(), false);
    EXPECT_EQ(t.engine.stats().peak_live_orders, 50u);
    for (std::uint64_t i = 1; i <= 25; ++i)
        t.engine.on_order_delete(1, 1000 + i);
    EXPECT_EQ(t.engine.stats().peak_live_orders, 50u);  // peak never drops
    EXPECT_EQ(t.engine.stats().live_orders, 25u);
}
    
} // namespace
} // namespace itch