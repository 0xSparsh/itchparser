#pragma once

// On wire packed message layout for NASDAQ itch 5.0
// Every struct is '__attribute__((packed))' and matches the spec
// byte layout exactly.

// Field Types use fixed width unsigned integers
// Big-endian values atre converted at the call site (Endian.hpp)
// We donot save them in host order inside these structs, so a struct instance 
// is a view onto the mmap'd file (zero-copy)

// Common Wire format is :-
// byte 0    - message type
// byte 1-2  - stock locate
// byte 3-4  - tracking number
// byte 5-11 - timestamp

#include <array>
#include <cstdint>

namespace itch::wire {

using Byte = uint8_t;
template <std::size_t N> using Alpha = std::array<char, N>;

// Common header present in every message (11 bytes total)
struct CommonHeader {
    Byte    messageType;        // offset 0
    std::uint8_t stockLocateHi; // offset 1
    std::uint8_t stockLocateLo;
    std::uint8_t trackingHi;    // offset 3
    std::uint8_t trackingLo;    
    Byte    ts[6];              // offset 5, 6 bytes (timestamp)
} __attribute__((packed));
static_assert(sizeof(CommonHeader) == 11);

// 1.1 System Event Message
struct SystemEventMsg {
    CommonHeader hdr;
    Byte    eventCode;  // offset 11
} __attribute__((packed));
static_assert(sizeof(SystemEventMsg) == 12);

// 1.2.1 Stock Directory Message
struct StockDirectoryMsg {
    CommonHeader hdr;
    Alpha<8>     stock;                         // offset 11
    Byte         marketCategory;                // offset 19
    Byte         financialStatusIndicator;      // offset 20
    std::uint8_t roundLotSize[4];               // offset 21
    Byte         roundLotsOnly;                 // offset 25
    Byte         issueClassification;           // offset 26
    Alpha<2>     issueSubType;                  // offset 27
    Byte         authenticity;                  // offset 29
    Byte         shortScaleThresholdIndicator;  // offset 30
    Byte         ipoFlag;                       // offset 31
    Byte         luldReferencePriceTier;        // offset 32
    Byte         etpFlag;                       // offset 33
    std::uint8_t etpLeverageFactor[4];          // offset 34
    Byte         inverseIndicator;              // offset 38
} __attribute__((packed));
static_assert(sizeof(StockDirectoryMsg) == 39);

// 1.2.2 Stock Trading Action
struct StockTradingActionMsg {
    CommonHeader hdr;
    Alpha<8>     stock;         // offset 11
    Byte         tradingState;  // offset 19
    Byte         reserved;      // offset 20
    Alpha<4>     reason;        // offset 21
} __attribute__((packed));
static_assert(sizeof(StockTradingActionMsg) == 25);

// 1.2.3 Reg SHO
struct RegSHOMsg {
    CommonHeader hdr;
    Alpha<8>     stock;          // offset 11
    Byte         regSHOAction;   // offset 19
} __attribute__((packed));
static_assert(sizeof(RegSHOMsg) == 20);

// 1.2.4 Market Participant Position 
struct MarketParticipantPositionMsg {
    CommonHeader hdr;
    Alpha<4>     mpid;                  // offset 11
    Alpha<8>     stock;                 // offset 15
    Byte         primaryMarketMaker;    // offset 23
    Byte         marketMakerMode;       // offset 24
    Byte         marketParticipantState;// offset 25
} __attribute__((packed));
static_assert(sizeof(MarketParticipantPositionMsg) == 26);

// 1.2.5.1 MWCB Decline Level
struct MWCBDeclineLevelMsg {
    CommonHeader hdr;
    std::uint8_t level1[8];   // offset 11
    std::uint8_t level2[8];   // offset 19
    std::uint8_t level3[8];   // offset 27
} __attribute__((packed));
static_assert(sizeof(MWCBDeclineLevelMsg) == 35);

// 1.2.5.2 MWCB Status 
struct MWCBStatusMsg {
    CommonHeader hdr;
    Byte         breachedLevel;   // offset 11
} __attribute__((packed));
static_assert(sizeof(MWCBStatusMsg) == 12);

// 1.2.6 IPO Quoting Period Update 
struct IPOQuotingPeriodUpdateMsg {
    CommonHeader hdr;
    Alpha<8>     stock;                       // offset 11
    std::uint8_t ipoQuotationReleaseTime[4];  // offset 19
    Byte         ipoQuotationReleaseQualifier;// offset 23
    std::uint8_t ipoPrice[4];                 // offset 24
} __attribute__((packed));
static_assert(sizeof(IPOQuotingPeriodUpdateMsg) == 28);

// 1.2.7 LULD Auction Collar 
struct LULDAuctionCollarMsg {
    CommonHeader hdr;
    Alpha<8>     stock;                       // offset 11
    std::uint8_t auctionCollarReferencePrice[4]; // offset 19
    std::uint8_t upperAuctionCollarPrice[4];     // offset 23
    std::uint8_t lowerAuctionCollarPrice[4];     // offset 27
    std::uint8_t auctionCollarExtension[4];      // offset 31
} __attribute__((packed));
static_assert(sizeof(LULDAuctionCollarMsg) == 35);

// 1.2.8 Operational Halt
struct OperationalHaltMsg {
    CommonHeader hdr;
    Alpha<8>     stock;              // offset 11
    Byte         marketCode;         // offset 19
    Byte         operationHaltAction;// offset 20
} __attribute__((packed));
static_assert(sizeof(OperationalHaltMsg) == 21);

// 1.3.1 Add Order No MPID
struct AddOrderMsg {
    CommonHeader hdr;
    std::uint8_t orderReferenceNumber[8]; // offset 11
    Byte         buySellIndicator;        // offset 19
    std::uint8_t shares[4];               // offset 20
    Alpha<8>     stock;                   // offset 24
    std::uint8_t price[4];                // offset 32
} __attribute__((packed));
static_assert(sizeof(AddOrderMsg) == 36);

// 1.3.2 Add Order With MPID Attribution
struct AddOrderMPIDMsg {
    CommonHeader hdr;
    std::uint8_t orderReferenceNumber[8]; // offset 11
    Byte         buySellIndicator;        // offset 19
    std::uint8_t shares[4];               // offset 20
    Alpha<8>     stock;                   // offset 24
    std::uint8_t price[4];                // offset 32
    Alpha<4>     attribution;             // offset 36
} __attribute__((packed));
static_assert(sizeof(AddOrderMPIDMsg) == 40);

// 1.4.1 Order Executed
struct OrderExecutedMsg {
    CommonHeader hdr;
    std::uint8_t orderReferenceNumber[8]; // offset 11
    std::uint8_t executedShares[4];       // offset 19
    std::uint8_t matchNumber[8];          // offset 23
} __attribute__((packed));
static_assert(sizeof(OrderExecutedMsg) == 31);

// 1.4.2 Order Executed With Price
struct OrderExecutedWithPriceMsg {
    CommonHeader hdr;
    std::uint8_t orderReferenceNumber[8]; // offset 11
    std::uint8_t executedShares[4];       // offset 19
    std::uint8_t matchNumber[8];          // offset 23
    Byte         printable;               // offset 31
    std::uint8_t executionPrice[4];       // offset 32
} __attribute__((packed));
static_assert(sizeof(OrderExecutedWithPriceMsg) == 36);

// 1.4.3 Order Cancel
struct OrderCancelMsg {
    CommonHeader hdr;
    std::uint8_t orderReferenceNumber[8]; // offset 11
    std::uint8_t cancelledShares[4];      // offset 19
} __attribute__((packed));
static_assert(sizeof(OrderCancelMsg) == 23);

// 1.4.4 Order Delete
struct OrderDeleteMsg {
    CommonHeader hdr;
    std::uint8_t orderReferenceNumber[8]; // offset 11
} __attribute__((packed));
static_assert(sizeof(OrderDeleteMsg) == 19);

// 1.4.5 Order Replace
struct OrderReplaceMsg {
    CommonHeader hdr;
    std::uint8_t originalOrderReferenceNumber[8]; // offset 11
    std::uint8_t newOrderReferenceNumber[8];      // offset 19
    std::uint8_t shares[4];                       // offset 27
    std::uint8_t price[4];                        // offset 31
} __attribute__((packed));
static_assert(sizeof(OrderReplaceMsg) == 35);

// 1.5.1 Trade Message (Non-Cross)
struct NonCrossTradeMsg {
    CommonHeader hdr;
    std::uint8_t orderReferenceNumber[8]; // offset 11
    Byte         buySellIndicator;        // offset 19
    std::uint8_t shares[4];               // offset 20
    Alpha<8>     stock;                   // offset 24
    std::uint8_t price[4];                // offset 32
    std::uint8_t matchNumber[8];          // offset 36
} __attribute__((packed));
static_assert(sizeof(NonCrossTradeMsg) == 44);

// 1.5.2 Cross Trade
struct CrossTradeMsg {
    CommonHeader hdr;
    std::uint8_t shares[8];          // offset 11
    Alpha<8>     stock;              // offset 19
    std::uint8_t crossPrice[4];      // offset 27
    std::uint8_t matchNumber[8];     // offset 31
    Byte         crossType;          // offset 39
} __attribute__((packed));
static_assert(sizeof(CrossTradeMsg) == 40);

// 1.5.3 Broken Trade
struct BrokenTradeMsg {
    CommonHeader hdr;
    std::uint8_t matchNumber[8]; // offset 11
} __attribute__((packed));
static_assert(sizeof(BrokenTradeMsg) == 19);

// 1.6 NOII
struct NOIIMsg {
    CommonHeader hdr;
    std::uint8_t pairedShares[8];         // offset 11
    std::uint8_t imbalanceShares[8];      // offset 19
    Byte         imbalanceDirection;      // offset 27
    Alpha<8>     stock;                   // offset 28
    std::uint8_t farPrice[4];             // offset 36
    std::uint8_t nearPrice[4];            // offset 40
    std::uint8_t currentReferencePrice[4];// offset 44
    Byte         crossType;               // offset 48
    Byte         priceVariationIndicator; // offset 49
} __attribute__((packed));
static_assert(sizeof(NOIIMsg) == 50);

// 1.7 RPII
struct RetailPriceImprovementIndicatorMsg {
    CommonHeader hdr;
    Alpha<8>     stock;          // offset 11
    Byte         interestFlag;   // offset 19
} __attribute__((packed));
static_assert(sizeof(RetailPriceImprovementIndicatorMsg) == 20);

// 1.8 DLCR
struct DLCRMsg {
    CommonHeader hdr;
    Alpha<8>     stock;                       // offset 11
    Byte         openEligibilityStatus;       // offset 19
    std::uint8_t minimumAllowablePrice[4];    // offset 20
    std::uint8_t maximumAllowablePrice[4];    // offset 24
    std::uint8_t nearExecutionPrice[4];       // offset 28
    std::uint8_t nearExecutionTime[8];        // offset 32
    std::uint8_t lowerPriceRangeCollar[4];    // offset 40
    std::uint8_t upperPriceRangeCollar[4];    // offset 44
} __attribute__((packed));
static_assert(sizeof(DLCRMsg) == 48);

// Compile time message type registry
namespace detail {
    struct MsgMeta {
        char type;
        std::size_t size;
    };

    // Create an array whose elements are MsgMeta objects
    inline constexpr MsgMeta kMsgMeta[] = {
        {'S', sizeof(SystemEventMsg)},
        {'R', sizeof(StockDirectoryMsg)},
        {'H', sizeof(StockTradingActionMsg)},
        {'Y', sizeof(RegSHOMsg)},
        {'L', sizeof(MarketParticipantPositionMsg)},
        {'V', sizeof(MWCBDeclineLevelMsg)},
        {'W', sizeof(MWCBStatusMsg)},
        {'K', sizeof(IPOQuotingPeriodUpdateMsg)},
        {'J', sizeof(LULDAuctionCollarMsg)},
        {'h', sizeof(OperationalHaltMsg)},
        {'A', sizeof(AddOrderMsg)},
        {'F', sizeof(AddOrderMPIDMsg)},
        {'E', sizeof(OrderExecutedMsg)},
        {'C', sizeof(OrderExecutedWithPriceMsg)},
        {'X', sizeof(OrderCancelMsg)},
        {'D', sizeof(OrderDeleteMsg)},
        {'U', sizeof(OrderReplaceMsg)},
        {'P', sizeof(NonCrossTradeMsg)},
        {'Q', sizeof(CrossTradeMsg)},
        {'B', sizeof(BrokenTradeMsg)},
        {'I', sizeof(NOIIMsg)},
        {'N', sizeof(RetailPriceImprovementIndicatorMsg)},
        {'O', sizeof(DLCRMsg)},
    };
} // namespace detail

[[nodiscard]] constexpr std::size_t wire_payload_size(char type) noexcept {
    for (const auto& m : detail::kMsgMeta) {
        if (m.type == type) [[unlikely]] return m.size;
    }
    return 0;
}

} // namespace itch::wire