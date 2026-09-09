#include <gtest/gtest.h>

#include "WireFormat.hpp"

namespace itch {
namespace {

TEST(WireFormat, StructSizesMatchSpec) {
    EXPECT_EQ(sizeof(wire::CommonHeader), 11u);
    EXPECT_EQ(sizeof(wire::SystemEventMsg), 12u);
    EXPECT_EQ(sizeof(wire::StockDirectoryMsg), 39u);
    EXPECT_EQ(sizeof(wire::StockTradingActionMsg), 25u);
    EXPECT_EQ(sizeof(wire::RegSHOMsg), 20u);
    EXPECT_EQ(sizeof(wire::MarketParticipantPositionMsg), 26u);
    EXPECT_EQ(sizeof(wire::MWCBDeclineLevelMsg), 35u);
    EXPECT_EQ(sizeof(wire::MWCBStatusMsg), 12u);
    EXPECT_EQ(sizeof(wire::IPOQuotingPeriodUpdateMsg), 28u);
    EXPECT_EQ(sizeof(wire::LULDAuctionCollarMsg), 35u);
    EXPECT_EQ(sizeof(wire::OperationalHaltMsg), 21u);
    EXPECT_EQ(sizeof(wire::AddOrderMsg), 36u);
    EXPECT_EQ(sizeof(wire::AddOrderMPIDMsg), 40u);
    EXPECT_EQ(sizeof(wire::OrderExecutedMsg), 31u);
    EXPECT_EQ(sizeof(wire::OrderExecutedWithPriceMsg), 36u);
    EXPECT_EQ(sizeof(wire::OrderCancelMsg), 23u);
    EXPECT_EQ(sizeof(wire::OrderDeleteMsg), 19u);
    EXPECT_EQ(sizeof(wire::OrderReplaceMsg), 35u);
    EXPECT_EQ(sizeof(wire::NonCrossTradeMsg), 44u);
    EXPECT_EQ(sizeof(wire::CrossTradeMsg), 40u);
    EXPECT_EQ(sizeof(wire::BrokenTradeMsg), 19u);
    EXPECT_EQ(sizeof(wire::NOIIMsg), 50u);
    EXPECT_EQ(sizeof(wire::RetailPriceImprovementIndicatorMsg), 20u);
    EXPECT_EQ(sizeof(wire::DLCRMsg), 48u);
}

TEST(WireFormat, PayloadSizeRegistryComplete) {
    // All 23 spec message types resolve to their struct size.
    EXPECT_EQ(wire::wire_payload_size('S'), sizeof(wire::SystemEventMsg));
    EXPECT_EQ(wire::wire_payload_size('R'), sizeof(wire::StockDirectoryMsg));
    EXPECT_EQ(wire::wire_payload_size('H'), sizeof(wire::StockTradingActionMsg));
    EXPECT_EQ(wire::wire_payload_size('Y'), sizeof(wire::RegSHOMsg));
    EXPECT_EQ(wire::wire_payload_size('L'), sizeof(wire::MarketParticipantPositionMsg));
    EXPECT_EQ(wire::wire_payload_size('V'), sizeof(wire::MWCBDeclineLevelMsg));
    EXPECT_EQ(wire::wire_payload_size('W'), sizeof(wire::MWCBStatusMsg));
    EXPECT_EQ(wire::wire_payload_size('K'), sizeof(wire::IPOQuotingPeriodUpdateMsg));
    EXPECT_EQ(wire::wire_payload_size('J'), sizeof(wire::LULDAuctionCollarMsg));
    EXPECT_EQ(wire::wire_payload_size('h'), sizeof(wire::OperationalHaltMsg));
    EXPECT_EQ(wire::wire_payload_size('A'), sizeof(wire::AddOrderMsg));
    EXPECT_EQ(wire::wire_payload_size('F'), sizeof(wire::AddOrderMPIDMsg));
    EXPECT_EQ(wire::wire_payload_size('E'), sizeof(wire::OrderExecutedMsg));
    EXPECT_EQ(wire::wire_payload_size('C'), sizeof(wire::OrderExecutedWithPriceMsg));
    EXPECT_EQ(wire::wire_payload_size('X'), sizeof(wire::OrderCancelMsg));
    EXPECT_EQ(wire::wire_payload_size('D'), sizeof(wire::OrderDeleteMsg));
    EXPECT_EQ(wire::wire_payload_size('U'), sizeof(wire::OrderReplaceMsg));
    EXPECT_EQ(wire::wire_payload_size('P'), sizeof(wire::NonCrossTradeMsg));
    EXPECT_EQ(wire::wire_payload_size('Q'), sizeof(wire::CrossTradeMsg));
    EXPECT_EQ(wire::wire_payload_size('B'), sizeof(wire::BrokenTradeMsg));
    EXPECT_EQ(wire::wire_payload_size('I'), sizeof(wire::NOIIMsg));
    EXPECT_EQ(wire::wire_payload_size('N'), sizeof(wire::RetailPriceImprovementIndicatorMsg));
    EXPECT_EQ(wire::wire_payload_size('O'), sizeof(wire::DLCRMsg));
}

TEST(WireFormat, PayloadSizeUnknownReturnsZero) {
    EXPECT_EQ(wire::wire_payload_size('Z'), 0u);
    EXPECT_EQ(wire::wire_payload_size('\0'), 0u);
    EXPECT_EQ(wire::wire_payload_size('z'), 0u);
    EXPECT_EQ(wire::wire_payload_size('a'), 0u);  // lowercase 'a' != 'A'
}

TEST(WireFormat, PackedNoPadding) {
    // Packed structs must have no alignment holes: size == sum of members.
    // CommonHeader: 1 + 1 + 1 + 1 + 1 + 6 = 11.
    static_assert(sizeof(wire::CommonHeader) ==
                  sizeof(wire::Byte) * 5 + 6);
    SUCCEED();
}

} // namespace
} // namespace itch