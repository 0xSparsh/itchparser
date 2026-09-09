#include <gtest/gtest.h>

#include "Types.hpp"

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
    EXPECT_EQ(static_cast<std::uint8_t>(Side::Buy), static_cast<std::uint8_t>('0'));
    EXPECT_EQ(static_cast<std::uint8_t>(Side::Sell), static_cast<std::uint8_t>('1'));
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
    EXPECT_EQ(static_cast<char>(EventCode::MessagesStart), 'O');
    EXPECT_EQ(static_cast<char>(EventCode::SystemHoursStart), 'S');
    EXPECT_EQ(static_cast<char>(MarketCategory::NasdaqSelect), 'Q');
    EXPECT_EQ(static_cast<char>(FinancialStatusIndicator::Normal), 'N');
    EXPECT_EQ(static_cast<char>(TradingState::Trading), 'T');
    EXPECT_EQ(static_cast<char>(TradingState::Halted), 'H');
    EXPECT_EQ(static_cast<char>(RegSHOAction::NoPriceTest), '0');
    EXPECT_EQ(static_cast<char>(BuySellIndicator::BuyOrder), 'B');
    EXPECT_EQ(static_cast<char>(BuySellIndicator::SellOrder), 'S');
    EXPECT_EQ(static_cast<char>(Printable::Yes), 'Y');
    EXPECT_EQ(static_cast<char>(CrossType::Closing), 'C');
    EXPECT_EQ(static_cast<char>(CrossType::Opening), 'O');
    EXPECT_EQ(static_cast<char>(Side::Buy), '0');
}

} // namespace
} // namespace itch