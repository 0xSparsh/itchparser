#pragma once

#include "OrderBook.hpp"
#include "Types.hpp"

#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <new>
#include <memory>

namespace itch {

// Represents one traded instrument

// Stores static information from the Stock Directory Message, current
// trading state and the instrument's live OrderBook

//The OrderBook is lazily created on the first order for this instrument

struct Instrument {
    StockLocate                 locate{0};
    Stock                       symbol{};
    MarketCategory              marketCategory{MarketCategory::NotAvailable};
    FinancialStatusIndicator    financialStatus{FinancialStatusIndicator::Normal};
    RoundLotSize                roundLotSize{100};
    RoundLotsOnly               roundLotsOnly{RoundLotsOnly::No};
    Authenticity                authenticity{Authenticity::Production};
    ShortScaleThresholdIndicator schortScale{ShortScaleThresholdIndicator::NotRestricted};
    IPOFlag                     ipoFlag{IPOFlag::NotAvailable};
    LULDReferencePriceTier      luldTier{LULDReferencePriceTier::NotAvailable};
    ETPFlag                     etpFlag{ETPFlag::NotAvailable};
    ETPLeverageFactor           etpLeverageFactor{1};
    InverseIndicator            inverseIndicator{InverseIndicator::NotInverseETP};

    // Current trading state - updated by Stock Trading Action Messgae
    TradingState                tradingState{TradingState::Halted};
    RegSHOAction                regSHOAction{RegSHOAction::NoPriceTest};

    // Created only when the instrument receives its first order
    std::unique_ptr<OrderBook> book;

    OrderBook& ensure_book() {
        if (!book) [[unlikely]] {
            book = std::make_unique<OrderBook>();
        }

        return *book;
    }

    // Copy the 8-byte ITCH symbol field
    void set_symbol(const char* src) noexcept {
        std::memcpy(symbol.data(), src, symbol.size());
    }
};

// Maps StockLocate directly to Instrument
//
// StockLocate is uint16_t so the entire possible range can be represented by a fixed array.
// The map owns all the instrument objects it creates 
class InstrumentMap {
public:
    InstrumentMap() noexcept = default;

    ~InstrumentMap() {
        for (Instrument* p : slots_) {
            if (p) {
                p->~Instrument();
                ::operator delete(p, std::align_val_t{alignof(Instrument)});
            }
        }
    }

    InstrumentMap(const InstrumentMap&) = delete;
    InstrumentMap& operator=(const InstrumentMap&) = delete;

    InstrumentMap(InstrumentMap&&) = delete;
    InstrumentMap& operator=(InstrumentMap&&) = delete;

    [[nodiscard]]
    Instrument* operator[](StockLocate locate) const noexcept {
        return slots_[locate];
    }

    Instrument* get_or_create(StockLocate locate) {
        if (Instrument* p = slots_[locate]; p) [[likely]] {
            return p;
        }

        void* mem = ::operator new(sizeof(Instrument),
                                   std::align_val_t{alignof(Instrument)});
        Instrument* p = new (mem) Instrument{};
        p->locate = locate;
        slots_[locate] = p;
        return p;
    }

    // Number of distinct instruments currently registered
    [[nodiscard]] std::size_t size() const noexcept {
        return count_;
    }

private:
    std::array<Instrument*, kMaxInstruments> slots_{};

    std::size_t count_{0};
};

} // namespace itch