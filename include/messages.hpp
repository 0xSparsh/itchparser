#pragma once

#include<iostream>
#include<vector>
#include<variant>
#include<cstdint>

#include"Types.hpp"

struct SystemEventMessage {
    MessageType messageType = 'S';      // Always "S" 
    StockLocate stockLocate;            // Always 0 for system wide messages
    TrackingNumber trackingNumber;      // NASDAQ internal tracking number
    Timestamp timestamp;                // Nanoseconds since midnight 2^64 = 18,446,744,073,709,551,616 nanoseconds
    EventCode eventCode;                // Event Codes to indicate the start or end of messages, system hours and market hours
};

struct StockDirectoryMessage {
    MessageType messageType = 'R';
    StockLocate stockLocate;
    TrackingNumber trackingNumber;
    Timestamp timestamp;
    Stock stock;
    MarketCategory marketCategory;
    FinancialStatusIndicator financialStatusIndicator;
    RoundLotSize roundLotSize;
    IssueClassification issueClassification;
    IssueSubType issueSubType;
    Authenticity authenticity;
    ShortScaleThresholdIndicator shortScaleThresholdIndicator;
    IPOFlag ipoFlag;
    LULDReferencePriceTier luldReferencePriceTier;
    ETPFlag etpFlag;
    ETPLeverageFactor etpLeverageFactor;
    InverseIndicator inverseIndicator;
};

struct StockTradingAction {
    MessageType messageType = 'H';
    StockLocate stockLocate;
    TrackingNumber trackingNumber;
    Stock stock;
    TradingState tradingState;
    Reserved reserved;
    Reason reason;
};

struct RegShoMessage {
    MessageType messageType = 'Y';
    StockLocate stockLocate;
    TrackingNumber trackingNumber;
    Timestamp timestamp;
    Stock stock;
    RegSHOAction regSHOAction;
};

struct MarketParticipantPositionMessage {
    MessageType messageType = 'L';
    StockLocate stockLocate;
    TrackingNumber trackingNumber;
    Timestamp timestamp;
    MPID mpid;
    Stock stock;
    PrimaryMarketMaker primaryMarketMaker;
    MarketMakerMode marketMakerMode;
    MarketParticipantState marketParticipantState;
};

struct MWCBDeclineLevelMessage {
    MessageType messageType = 'V';
    StockLocate stockLocate;
    TrackingNumber trackingNumber;
    Timestamp timestamp;

    // Levels are Price(8) which means 8 implied decimals
    Level1 level1;
    Level2 level2;
    Level3 level3;
};

struct MWCBStatusMessage {
    MessageType messageType = 'W';
    StockLocate stockLocate = 0;
    TrackingNumber trackingNumber;
    Timestamp timestamp;
    BreachedLevel breachedLevel;
};

struct IPOQuotingPeriodUpdateMessage {
    MessageType messageType = 'K';
    StockLocate stockLocate = 0;
    TrackingNumber trackingNumber;
    Timestamp timestamp;
    Stock stock;
    IPOQuotationReleaseTime ipoQuotationReleaseTime;
    IPOQuotationReleaseQualifier ipoQuotationReleaseQualifier;
    IPOPrice ipoPrice;
};

struct LULDAuctionCollarMessage {
    MessageType messageType = 'J';
    StockLocate stockLocate;
    TrackingNumber trackingNumber;
    Timestamp timestamp;
    Stock stock;
    AuctionCollarReferencePrice auctionCollarReferencePrice;
    UpperAuctionCollarPrice upperAuctionCollarPrice;
    LowerAuctionCollarPrice lowerAuctionCollarPrice;
    AuctionCollarExtension auctionCollarExtension;
};

struct OperationHaltMessage {
    MessageType messageType = 'h';
    StockLocate stockLocate;
    TrackingNumber trackingNumber;
    Timestamp timestamp;
    Stock stock;
    MarketCode marketCode;
    OperationHaltAction operationHaltAction;
};

struct AddOrderMessage {

};

struct AddOrderMPIDAttribution {

};

struct OrderExecutedMessage {

};

struct OrderExecutedWithPriceMessage {

};

struct OrderCancelMessage {

};

struct OrderDeleteMessgae {

};

struct OrderReplaceMessage {

};

struct NonCrossTradeMessage {

};

struct CrossTradeMessage {

};

struct BrokenTradeMessage {

};

struct NOIIMessage {

};

struct RetailPriceImprovementIndicatorMessage {

};

struct DLCRMessage {

};

