#pragma once

#include<cstdint>
#include<array>

using MessageType = char;

using StockLocate = uint16_t;
using TrackingNumber = uint16_t;
using Timestamp = uint64_t;

using Stock = std::array<char,8>;

using RoundLotSize = uint32_t;
using IssueClassification = char;
using IssueSubType = std::array<char,2>;
using ETPLeverageFactor = uint32_t;

using Reserved = char;
using Reason = std::array<char,4>;
using MPID = std::array<char,4>;

using Level1 = uint64_t;
using Level2 = uint64_t;
using Level3 = uint64_t;

using IPOQuotationReleaseTime = uint32_t;
using IPOPrice = uint32_t;

using AuctionCollarReferencePrice = uint32_t;
using UpperAuctionCollarPrice = uint32_t;
using LowerAuctionCollarPrice = uint32_t;
using AuctionCollarExtension = uint32_t;

using OrderReferenceNumber = uint64_t;
using Shares = uint32_t;
using Price = uint32_t;

using Attribution = std::array<char,4>;

using ExecutedShares = uint32_t;
using MatchNumber = uint64_t;
using ExecutionPrice = uint32_t;

using CancelledShares = uint32_t;
using NewOrderReferenceNumber = uint64_t;

using CrossPrice = uint32_t;

using PairedShares = uint64_t;
using ImbalanceShares = uint64_t;

using FarPrice = uint32_t;
using NearPrice = uint32_t;
using CurrentReferencePrice = uint32_t;

using MinimumAllowablePrice = uint32_t;
using MaximumAllowablePrice = uint32_t;
using NearExecutionPrice = uint32_t;
using NearExecutionTime = uint64_t;
using LowerPriceRangeCollar = uint32_t;
using UpperPriceRangeCollar = uint32_t;

enum class EventCode : char {
    MessagesStart = 'O',
    SystemHoursStart = 'S',
    MarketHoursStart = 'Q',
    MarketHoursEnd = 'M',
    SystemHoursEnd = 'E',
    MessagesEnd = 'C'
};

enum class MarketCategory : char {
    // NASDAQ Listed Instruments
    NasdaqSelect = 'Q',
    NasdaqGlobal = 'G',
    NasdaqCaptial = 'S',

    // Non-NASDAQ Listed Instruments
    NYSE = 'N',
    AMEX = 'A',
    ARCA = 'P',
    TEXAS = 'M',
    BATS = 'Z',
    IEX = 'V',
    NotAvailable = ' '
};

enum class FinancialStatusIndicator : char {
    // NASDAQ Listed Instruments
    Deficient = 'D',
    Delinquent = 'E',
    Bankrupt = 'Q',
    Suspended = 'S',
    DeficientAndBankrupt = 'G',
    DeficientAndDelinquent = 'H',
    DelinquentAndBankrupt = 'J',
    DeficientDelinquentBankrupt = 'K',
    CreationRedemptionSuspended = 'C',
    Normal = 'N',

    // Non-NASDAQ Listed Instruments
    NotAvailable = ' ' 
};

enum class RoundLotsOnly : char {
    Yes = 'Y',
    No = 'N'
};

enum class Authenticity : char {
    Production = 'P',
    Test = 'T'
};

enum class ShortScaleThresholdIndicator : char {
    Restricted = 'Y',
    NotRestricted = 'N'
};

enum class IPOFlag : char {
    // NASDAQ Listed Instruments
    NewIPO = 'Y',
    NotNewIPO = 'N',
    NonIPONewList = 'Z',

    // Non NASDAQ Listed Instruments
    NotAvailable = ' '
};

enum class LULDReferencePriceTier : char {
    Tier1 = '1',
    Tier2 = '2',
    NotAvailable = ' '
};

enum class ETPFlag : char {
    ETP = 'Y',
    NonETP = 'N',
    NotAvailable = ' '
};

enum class InverseIndicator : char {
    InverseETP = 'Y',
    NotInverseETP = 'N'
};

enum class TradingState : char {
    Halted = 'H',
    Paused = 'P',
    Quotation = 'Q',
    Trading = 'T'
};

enum class RegSHOAction : char {
    NoPriceTest = '0',
    PriceTestInEffect = '1',
    PriceTestRemains = '2'
};

enum class PrimaryMarketMaker : char {
    Yes = 'Y',
    No = 'N'
};

enum class MarketMakerMode : char {
    Normal = 'N',
    Passive = 'P',
    Syndicate = 'S',
    PreSyndicate = 'R',
    Penalty = 'L'
};

enum class MarketParticipantState : char {
    Active = 'A',
    Excused = 'E',
    Withdrawn = 'W',
    Suspended = 'S',
    Deleted = 'D'
};

enum class BreachedLevel : char {
    Level1 = '1',
    Level2 = '2',
    Level3 = '3'
};

enum class IPOQuotationReleaseQualifier : char {
    Anticipate = 'A',
    Canceled = 'C'
};

enum class MarketCode : char {
    Nasdaq = 'Q',
    Texas = 'B',
    PSX = 'X'
};

enum class OperationHaltAction : char {
    Halted = 'H',
    Resumed = 'T'
};

enum class BuySellIndicator : char {
    BuyOrder = 'B',
    SellOrder = 'S'
};

enum class Printable : char {
    Yes = 'Y',
    No = 'N'
};

enum class CrossType : char {
    Opening = 'O',
    Closing = 'C',
    Halted = 'H'
};

enum class ImbalanceDirection : char {
    BuyImbalance = 'B',
    SellImbalance = 'S',
    NoImbalance = 'N',
    Insufficient = 'O',
    Paused = 'P'
};

enum class PriceVariationIndicator : char {
    LessThan1Percent = 'L',
    OneTo1_99Percent = '1',
    TwoTo2_99Percent = '2',
    ThreeTo3_99Percent = '3',
    FourTo4_99Percent = '4',
    FiveTo5_99Percent = '5',
    SixTo6_99Percent = '6',
    SevenTo7_99Percent = '7',
    EightTo8_99Percent = '8',
    NineTo9_99Percent = '9',
    TenTo19_99Percent = 'A',
    TwentyTo29_99Percent = 'B',
    ThirtyPercentOrGreater = 'C',
    CannotBeCalculated = ' '
};

enum class InterestFlag : char {
    BuySide = 'B',
    SellSide = 'S',
    BothSides = 'A',
    None = 'N'
};

enum class OpenEligibilityStatus : char {
    Eligible = 'Y',
    NotEligible = 'N'
};

enum class IssueSubTypeValues : char {

};