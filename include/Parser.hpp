#pragma once

#include "Endian.hpp"
#include "WireFormat.hpp"
#include "Types.hpp"
#include "MatchingEngine.hpp"
#include "Stats.hpp"

#include <chrono>
#include <vector>
#include <cstdint>
#include <cstring>
#include <expected>
#include <span>
#include <string_view>

namespace itch {

// Every wire handler starts the same way: the length-prefixed frame may be
// shorter than the message struct for its type (corrupt/truncated feed).
// checked_cast centralises that guard so no handler can forget the check or
// get the size wrong. The [[nodiscard]] expected forces each call site to
// unwrap the result — the failure path stays explicit at every handler.
enum class ParseError : std::uint8_t { Truncated };

class Parser {
public:
    Parser() noexcept = default;

    std::size_t run(const std::byte* buf, std::size_t len,
                    MatchingEngine& engine, Stats& stats) {
        const std::byte* p   = buf;
        const std::byte* end = buf + len;

        while (p + 2 <= end) {
            // Read 2-byte big-endian payload length 
            const std::uint16_t payload_len = itch::load_be16(p);
            p += 2;

            // Bounds-check: payload must fit in remaining buffer 
            if (p + payload_len > end) [[unlikely]] {
                // Truncated trailing message
                stats.on_parse_error();
                break;
            }

            // Guard zero-length payload (no type byte to read)
            // A zero length prefix carries no message type; reading *p
            // would misinterpret the next message's length byte as a type
            // and desync framing. Count and skip without dispatch.
            if (payload_len == 0) [[unlikely]] {
                stats.on_parse_error();
                continue;
            }

            // Decode the message type (first byte of payload)
            const char type = static_cast<char>(*p);

            // Dispatch
            // We pass the payload pointer (p) and the payload_len; each
            // handler unwraps it through checked_cast, which rejects
            // payloads shorter than the expected wire struct for the type.
            dispatch(type, p, payload_len, engine, stats);

            // ---- 6. Advance past the payload ----
            p += payload_len;
        }

        return static_cast<std::size_t>(p - buf);
    }

private:
    // Length-check + reinterpret cast in one step. Returns the typed view
    // on success, ParseError::Truncated when the frame is too short.
    // Inlined into every handler — no extra call in the hot path.
    template <typename Msg>
    [[nodiscard]] static std::expected<const Msg*, ParseError>
    checked_cast(const std::byte* p, std::uint16_t len) noexcept {
        if (len < sizeof(Msg)) [[unlikely]] {
            return std::unexpected(ParseError::Truncated);
        }
        return reinterpret_cast<const Msg*>(p);
    }

    // Dispatch on message type. Using a switch rather than a function
    // pointer table because the compiler can inline the handlers
    // when it sees the call sites are small and the branch predictor
    // learns the message type distribution at run time 
    static void dispatch(char type, const std::byte* payload,
                         std::uint16_t payload_len,
                         MatchingEngine& engine, Stats& stats) {
        stats.on_message(type, payload_len);

        switch (type) {
            case 'S': handle_system_event       (payload, payload_len, engine, stats); break;
            case 'R': handle_stock_directory     (payload, payload_len, engine, stats); break;
            case 'H': handle_stock_trading_action (payload, payload_len, engine, stats); break;
            case 'Y': handle_reg_sho              (payload, payload_len, engine, stats); break;
            case 'L': handle_market_participant   (payload, payload_len, engine, stats); break;
            case 'V': handle_mwcb_decline         (payload, payload_len, engine, stats); break;
            case 'W': handle_mwcb_status          (payload, payload_len, engine, stats); break;
            case 'K': handle_ipo_quoting_period   (payload, payload_len, engine, stats); break;
            case 'J': handle_luld_auction_collar  (payload, payload_len, engine, stats); break;
            case 'h': handle_operational_halt     (payload, payload_len, engine, stats); break;
            case 'A': handle_add_order            (payload, payload_len, engine, stats, /*attributed=*/false); break;
            case 'F': handle_add_order            (payload, payload_len, engine, stats, /*attributed=*/true ); break;
            case 'E': handle_order_executed       (payload, payload_len, engine, stats); break;
            case 'C': handle_order_executed_price (payload, payload_len, engine, stats); break;
            case 'X': handle_order_cancel         (payload, payload_len, engine, stats); break;
            case 'D': handle_order_delete         (payload, payload_len, engine, stats); break;
            case 'U': handle_order_replace        (payload, payload_len, engine, stats); break;
            case 'P': handle_non_cross_trade      (payload, payload_len, engine, stats); break;
            case 'Q': handle_cross_trade          (payload, payload_len, engine, stats); break;
            case 'B': handle_broken_trade         (payload, payload_len, engine, stats); break;
            case 'I': handle_noii                 (payload, payload_len, engine, stats); break;
            case 'N': handle_rpii                 (payload, payload_len, engine, stats); break;
            case 'O': handle_dlcr                 (payload, payload_len, engine, stats); break;
            default:
                // Unknown message type - log and skip. The ITCH 5.0 spec
                // is stable but Nasdaq occasionally adds new types; we
                // count them as parse errors so they're visible in the
                // report but we don't crash.
                stats.on_parse_error();
                break;
        };    
    }

    static void handle_system_event(const std::byte* p, std::uint16_t len,
                                    MatchingEngine& engine, Stats& stats) {
        (void)engine;
        auto m = checked_cast<wire::SystemEventMsg>(p, len);
        if (!m) { stats.on_truncated(); return; }
        // System events don't affect the book — we just decode the
        // event code for completeness.
        (void)(*m)->eventCode;
    }

    static void handle_stock_directory(const std::byte* p, std::uint16_t len,
                                       MatchingEngine& engine, Stats& stats) {
        auto m = checked_cast<wire::StockDirectoryMsg>(p, len);
        if (!m) { stats.on_truncated(); return; }
        engine.on_stock_directory(
            itch::load_be16(&(*m)->hdr.stockLocateHi),
            (*m)->stock.data(),
            static_cast<MarketCategory>((*m)->marketCategory),
            static_cast<FinancialStatusIndicator>((*m)->financialStatusIndicator),
            itch::load_be32((*m)->roundLotSize),
            static_cast<RoundLotsOnly>((*m)->roundLotsOnly),
            static_cast<Authenticity>((*m)->authenticity),
            static_cast<ShortScaleThresholdIndicator>((*m)->shortScaleThresholdIndicator),
            static_cast<IPOFlag>((*m)->ipoFlag),
            static_cast<LULDReferencePriceTier>((*m)->luldReferencePriceTier),
            static_cast<ETPFlag>((*m)->etpFlag),
            itch::load_be32((*m)->etpLeverageFactor),
            static_cast<InverseIndicator>((*m)->inverseIndicator));
    }

    static void handle_stock_trading_action(const std::byte* p, std::uint16_t len,
                                            MatchingEngine& engine, Stats& stats) {
        auto m = checked_cast<wire::StockTradingActionMsg>(p, len);
        if (!m) { stats.on_truncated(); return; }
        engine.on_trading_action(
            itch::load_be16(&(*m)->hdr.stockLocateHi),
            static_cast<TradingState>((*m)->tradingState),
            (*m)->stock.data(),
            (*m)->reason.data());
    }

    static void handle_reg_sho(const std::byte* p, std::uint16_t len,
                               MatchingEngine& engine, Stats& stats) {
        auto m = checked_cast<wire::RegSHOMsg>(p, len);
        if (!m) { stats.on_truncated(); return; }
        engine.on_reg_sho(
            itch::load_be16(&(*m)->hdr.stockLocateHi),
            static_cast<RegSHOAction>((*m)->regSHOAction));
    }

    static void handle_market_participant(const std::byte* p, std::uint16_t len,
                                          MatchingEngine& engine, Stats& stats) {
        (void)engine;
        auto m = checked_cast<wire::MarketParticipantPositionMsg>(p, len);
        if (!m) { stats.on_truncated(); return; }
        // Market participant position doesn't affect the book.
        (void)m;
    }

    static void handle_mwcb_decline(const std::byte* p, std::uint16_t len,
                                    MatchingEngine& engine, Stats& stats) {
        (void)engine;
        auto m = checked_cast<wire::MWCBDeclineLevelMsg>(p, len);
        if (!m) { stats.on_truncated(); return; }
        (void)m;
    }

    static void handle_mwcb_status(const std::byte* p, std::uint16_t len,
                                   MatchingEngine& engine, Stats& stats) {
        (void)engine;
        auto m = checked_cast<wire::MWCBStatusMsg>(p, len);
        if (!m) { stats.on_truncated(); return; }
        (void)m;
    }

    static void handle_ipo_quoting_period(const std::byte* p, std::uint16_t len,
                                          MatchingEngine& engine, Stats& stats) {
        (void)engine;
        auto m = checked_cast<wire::IPOQuotingPeriodUpdateMsg>(p, len);
        if (!m) { stats.on_truncated(); return; }
        (void)m;
    }

    static void handle_luld_auction_collar(const std::byte* p, std::uint16_t len,
                                           MatchingEngine& engine, Stats& stats) {
        (void)engine;
        auto m = checked_cast<wire::LULDAuctionCollarMsg>(p, len);
        if (!m) { stats.on_truncated(); return; }
        (void)m;
    }

    static void handle_operational_halt(const std::byte* p, std::uint16_t len,
                                        MatchingEngine& engine, Stats& stats) {
        (void)engine;
        auto m = checked_cast<wire::OperationalHaltMsg>(p, len);
        if (!m) { stats.on_truncated(); return; }
        (void)m;
    }

    // Add Order - both 'A' (no MPID) and 'F' (with MPID) go through
    // this handler. The `attributed` flag tells the engine whether to
    // populate the MPID field on the resulting Order struct.

    static void handle_add_order(const std::byte* p, std::uint16_t len,
                                 MatchingEngine& engine, Stats& stats, bool attributed) {
        if (attributed) {
            auto m = checked_cast<wire::AddOrderMPIDMsg>(p, len);
            if (!m) { stats.on_truncated(); return; }
            engine.on_add_order(
                itch::load_be16(&(*m)->hdr.stockLocateHi),
                itch::load_be64((*m)->orderReferenceNumber),
                static_cast<BuySellIndicator>((*m)->buySellIndicator),
                itch::load_be32((*m)->shares),
                itch::load_be32((*m)->price),
                itch::load_be48((*m)->hdr.ts),
                (*m)->attribution,
                /*attributed=*/true);
        } else {
            auto m = checked_cast<wire::AddOrderMsg>(p, len);
            if (!m) { stats.on_truncated(); return; }
            // For unattributed orders, pass an empty MPID (spaces).
            // The engine will copy this into the Order struct.
            const MPID empty_mpid{' ', ' ', ' ', ' '};
            engine.on_add_order(
                itch::load_be16(&(*m)->hdr.stockLocateHi),
                itch::load_be64((*m)->orderReferenceNumber),
                static_cast<BuySellIndicator>((*m)->buySellIndicator),
                itch::load_be32((*m)->shares),
                itch::load_be32((*m)->price),
                itch::load_be48((*m)->hdr.ts),
                empty_mpid,
                /*attributed=*/false);
        }
    }

    static void handle_order_executed(const std::byte* p, std::uint16_t len,
                                      MatchingEngine& engine, Stats& stats) {
        auto m = checked_cast<wire::OrderExecutedMsg>(p, len);
        if (!m) { stats.on_truncated(); return; }
        engine.on_order_executed(
            itch::load_be16(&(*m)->hdr.stockLocateHi),
            itch::load_be64((*m)->orderReferenceNumber),
            itch::load_be32((*m)->executedShares),
            itch::load_be64((*m)->matchNumber));
    }

    static void handle_order_executed_price(const std::byte* p, std::uint16_t len,
                                            MatchingEngine& engine, Stats& stats) {
        auto m = checked_cast<wire::OrderExecutedWithPriceMsg>(p, len);
        if (!m) { stats.on_truncated(); return; }
        engine.on_order_executed_with_price(
            itch::load_be16(&(*m)->hdr.stockLocateHi),
            itch::load_be64((*m)->orderReferenceNumber),
            itch::load_be32((*m)->executedShares),
            itch::load_be64((*m)->matchNumber),
            static_cast<Printable>((*m)->printable),
            itch::load_be32((*m)->executionPrice));
    }

    static void handle_order_cancel(const std::byte* p, std::uint16_t len,
                                    MatchingEngine& engine, Stats& stats) {
        auto m = checked_cast<wire::OrderCancelMsg>(p, len);
        if (!m) { stats.on_truncated(); return; }
        engine.on_order_cancel(
            itch::load_be16(&(*m)->hdr.stockLocateHi),
            itch::load_be64((*m)->orderReferenceNumber),
            itch::load_be32((*m)->cancelledShares));
    }

    static void handle_order_delete(const std::byte* p, std::uint16_t len,
                                    MatchingEngine& engine, Stats& stats) {
        auto m = checked_cast<wire::OrderDeleteMsg>(p, len);
        if (!m) { stats.on_truncated(); return; }
        engine.on_order_delete(
            itch::load_be16(&(*m)->hdr.stockLocateHi),
            itch::load_be64((*m)->orderReferenceNumber));
    }

    static void handle_order_replace(const std::byte* p, std::uint16_t len,
                                     MatchingEngine& engine, Stats& stats) {
        auto m = checked_cast<wire::OrderReplaceMsg>(p, len);
        if (!m) { stats.on_truncated(); return; }
        engine.on_order_replace(
            itch::load_be16(&(*m)->hdr.stockLocateHi),
            itch::load_be64((*m)->originalOrderReferenceNumber),
            itch::load_be64((*m)->newOrderReferenceNumber),
            itch::load_be32((*m)->shares),
            itch::load_be32((*m)->price),
            itch::load_be48((*m)->hdr.ts));
    }

    static void handle_non_cross_trade(const std::byte* p, std::uint16_t len,
                                       MatchingEngine& engine, Stats& stats) {
        auto m = checked_cast<wire::NonCrossTradeMsg>(p, len);
        if (!m) { stats.on_truncated(); return; }
        engine.on_non_cross_trade(
            itch::load_be16(&(*m)->hdr.stockLocateHi),
            itch::load_be32((*m)->shares),
            itch::load_be32((*m)->price),
            itch::load_be64((*m)->matchNumber));
    }

    static void handle_cross_trade(const std::byte* p, std::uint16_t len,
                                   MatchingEngine& engine, Stats& stats) {
        auto m = checked_cast<wire::CrossTradeMsg>(p, len);
        if (!m) { stats.on_truncated(); return; }
        engine.on_cross_trade(
            itch::load_be16(&(*m)->hdr.stockLocateHi),
            static_cast<CrossType>((*m)->crossType));
    }

    static void handle_broken_trade(const std::byte* p, std::uint16_t len,
                                    MatchingEngine& engine, Stats& stats) {
        auto m = checked_cast<wire::BrokenTradeMsg>(p, len);
        if (!m) { stats.on_truncated(); return; }
        engine.on_broken_trade(
            itch::load_be16(&(*m)->hdr.stockLocateHi),
            itch::load_be64((*m)->matchNumber));
    }

    static void handle_noii(const std::byte* p, std::uint16_t len,
                            MatchingEngine& engine, Stats& stats) {
        (void)engine;
        auto m = checked_cast<wire::NOIIMsg>(p, len);
        if (!m) { stats.on_truncated(); return; }
        (void)m;
        // NOII doesn't affect the book.
    }

    static void handle_rpii(const std::byte* p, std::uint16_t len,
                            MatchingEngine& engine, Stats& stats) {
        (void)engine;
        auto m = checked_cast<wire::RetailPriceImprovementIndicatorMsg>(p, len);
        if (!m) { stats.on_truncated(); return; }
        (void)m;
    }

    static void handle_dlcr(const std::byte* p, std::uint16_t len,
                            MatchingEngine& engine, Stats& stats) {
        (void)engine;
        auto m = checked_cast<wire::DLCRMsg>(p, len);
        if (!m) { stats.on_truncated(); return; }
        (void)m;
    }
};

} // namespace itch
