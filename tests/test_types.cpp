#include <gtest/gtest.h>

#include "Types.hpp"

#include <utility>

namespace itch {
namespace {

TEST(Types, ToSideMapping) {
    EXPECT_EQ(to_side(BuySellIndicator::BuyOrder), Side::Buy);
    EXPECT_EQ(to_side(BuySellIndicator::SellOrder), Side::Sell);
    // Any non-'B' byte maps to Sell (defensive else-branch)
    EXPECT_EQ(to_side(static_cast<BuySellIndicator>('X')), Side::Sell);
}

TEST(Types, SideWireEncoding) {
    // NOTE: in this codebase Side carries the wire values '0'/'1',
    // unlike a 0/1 array index. Pinned to the actual definition.
    EXPECT_EQ(std::to_underlying(Side::Buy), static_cast<std::uint8_t>('0'));
    EXPECT_EQ(std::to_underlying(Side::Sell), static_cast<std::uint8_t>('1'));
}

TEST(Types, CapacityConstants) {
    EXPECT_EQ(kMaxInstruments, 65536u);
    EXPECT_EQ(kOrderPoolSize, 1u << 24);
    EXPECT_EQ(kPriceLevelPoolSize, 1u << 22);
    EXPECT_EQ(kCacheLineSize, 64u);
    // Pool sizes must be powers of two (masking replaces modulo).
    EXPECT_EQ(kOrderPoolSize & (kOrderPoolSize - 1), 0u);
    EXPECT_EQ(kPriceLevelPoolSize & (kPriceLevelPoolSize - 1), 0u);
}

TEST(Types, WireEnumValuesMatchSpec) {
    EXPECT_EQ(std::to_underlying(EventCode::MessagesStart), 'O');
    EXPECT_EQ(std::to_underlying(EventCode::SystemHoursStart), 'S');
    EXPECT_EQ(std::to_underlying(MarketCategory::NasdaqSelect), 'Q');
    EXPECT_EQ(std::to_underlying(FinancialStatusIndicator::Normal), 'N');
    EXPECT_EQ(std::to_underlying(TradingState::Trading), 'T');
    EXPECT_EQ(std::to_underlying(TradingState::Halted), 'H');
    EXPECT_EQ(std::to_underlying(RegSHOAction::NoPriceTest), '0');
    EXPECT_EQ(std::to_underlying(BuySellIndicator::BuyOrder), 'B');
    EXPECT_EQ(std::to_underlying(BuySellIndicator::SellOrder), 'S');
    EXPECT_EQ(std::to_underlying(Printable::Yes), 'Y');
    EXPECT_EQ(std::to_underlying(CrossType::Closing), 'C');
    EXPECT_EQ(std::to_underlying(CrossType::Opening), 'O');
    EXPECT_EQ(std::to_underlying(Side::Buy), '0');
}

} // namespace
} // namespace itch