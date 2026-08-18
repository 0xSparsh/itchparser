#pragma once

#include<cstdint>

using MessageType = char;
using StockLocate = uint16_t;
using TrackingNumber = uint16_t;
using Timestamp = uint64_t;
using Stock = char[8];
using RoundLotSize = uint32_t;
using IssueClassification = char;
using IssueSubType = char[2];
using ETPLeverageFactor = uint32_t;
using Reserved = char;
using Reason = char[4];
using MPID = char[4];
using Level1 = uint64_t;
using Level2 = uint64_t;
using Level3 = uint64_t;
using IPOQuotationReleaseTime = uint64_t;
using IPOPrice = uint32_t;
using AuctionCollarReferencePrice = uint32_t;
using UpperAuctionCollarPrice = uint32_t;
using LowerAuctionCollarPrice = uint32_t;
using AuctionCollarExtension = uint32_t;

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
    Penalty = 'P'
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

