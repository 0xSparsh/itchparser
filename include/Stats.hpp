#pragma once

// Throughput and latency tracker for the parser hot path
//
// During a parse run we want to know:-
// * Messages per second (sustained and peak)
// * Bytes-per-second 
// * Per-message-type counts
// * End to end wall clock time

// All counters are plain uint16_t 

#include "Types.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>

namespace itch {

// 256 slots - one for every possible byte value of the message type tag
struct Stats {
    // Per-message type counters
    std::array<std::uint64_t, 256> per_type{};
    std::uint64_t total_messages{0};
    std::uint64_t total_bytes{0};           // payload bytes (exclused 2 byte length prefix)
    std::uint64_t total_framing_bytes{0};   // 2-byte length prefix
    std::uint64_t parse_errors{0};          // unknown message type or truncated payload
    std::uint64_t truncated_messages{0};    // payload shorter than expected for type

    std::chrono::steady_clock::time_point start_time{};
    std::chrono::steady_clock::time_point end_time{};

    // reset all counters. called once before each parse run
    void reset() noexcept {
        per_type.fill(0);
        total_messages = 0;
        total_bytes = 0;
        total_framing_bytes = 0;
        parse_errors = 0;
        truncated_messages = 0;
        start_time = std::chrono::steady_clock::now();
        end_time = start_time;
    }

    // Marks the end of a parse run. Captuees the end timestamp
    void finish() noexcept {
        end_time = std::chrono::steady_clock::now();
    }

    void on_message(char type, std::uint32_t payload_size) noexcept {
        per_type[static_cast<unsigned char>(type)]++;
        total_messages++;
        total_bytes += payload_size;
        total_framing_bytes += 2;   // 2 byte length prefix per message
    }

    void on_parse_error() noexcept { parse_errors++; }

    void on_truncated() noexcept { truncated_messages++; }

    [[nodiscard]] double elapsed_seconds() const noexcept {
        return std::chrono::duration<double>(end_time - start_time).count();
    }

    [[nodsicard]] double msgs_per_sec() const noexcept {
        const double s = elapsed_seconds();
        return (s > 0.0) ? static_cast<double>(total_messages) / s : 0.0;
    }

    [[nodiscard]] double mb_per_sec() const noexcept {
        const double s = elapsed_seconds();
        if (s <= 0.0) return 0.0;
        const double bytes = static_cast<double>(total_bytes + total_framing_bytes);
        return bytes / (1024.0 * 1024.0) / s;
    }

    void print_report(std::FILE* out) const {
        const double s = elapsed_seconds();         // Calculate total run time
        std::fprintf(out,
            "================================================================\n"
            "ITCH Parser Run Report\n"
            "================================================================\n"
            "  Elapsed wall-clock      : %.3f s\n"
            "  Total messages          : %llu\n"
            "  Total payload bytes     : %llu (%.2f MiB)\n"
            "  Total framing bytes     : %llu\n"
            "  Throughput (msgs/sec)   : %.2f M\n"
            "  Throughput (MiB/sec)    : %.2f\n"
            "  Parse errors            : %llu\n"
            "  Truncated messages      : %llu\n"
            "----------------------------------------------------------------\n"
            "Per-message-type breakdown:\n",
            s,
            static_cast<unsigned long long>(total_messages),
            static_cast<unsigned long long>(total_bytes),
            static_cast<double>(total_bytes) / (1024.0 * 1024.0),
            static_cast<unsigned long long>(total_framing_bytes),
            msgs_per_sec() / 1e6,
            mb_per_sec(),
            static_cast<unsigned long long>(parse_errors),
            static_cast<unsigned long long>(truncated_messages));

        // Temporary row : Message type + count
        struct Row {
            unsigned char type;
            std::uint64_t count;
        };

        std::array<Row, 256> rows{};

        // Copy the 256 counters into rows
        for (std::size_t i = 0; i < 256; ++i) {
            rows[i] = {static_cast<unsigned char>(i), per_type[i]};
        }

        // Sort rows from highest count to lowest count.
        for (std::size_t i = 0; i < 256; ++i) {
            std::size_t best = i;
            for (std::size_t j = i + 1; j < 256; ++j) {
                if (rows[j].count > rows[best].count) {
                    best = j;
                }
            }

            if (best != i) {
                std::swap(rows[i], rows[best]);
            }
        }

        // Print messages that were actually seen
        for (std::size_t  i = 0; i < 256; ++i) {
            if (rows[i].count == 0) break;

            const char tag = static_cast<char>(rows[i].type);
            const char pct = (total_messages > 0) 
                ? 100.0 * static_cast<double>(rows[i].count)
                        / static_cast<double>(total_messages) : 0.0;

            std::fprintf(out, "  '%c' (0x%02X) %-30s : %12llu  (%5.2f%%)\n",
                         (tag >= 32 && tag < 127) ? tag : '.',
                         rows[i].type,
                         type_name(static_cast<char>(rows[i].type)),
                         static_cast<unsigned long long>(rows[i].count),
                         pct);
        }
        std::fprintf(out, "================================================================\n");
    }

    [[nodiscard]] static constexpr const char* type_name(char t) noexcept {
        switch (t) {
            case 'S': return "SystemEvent";
            case 'R': return "StockDirectory";
            case 'H': return "StockTradingAction";
            case 'Y': return "RegSHO";
            case 'L': return "MarketParticipantPosition";
            case 'V': return "MWCBDeclineLevel";
            case 'W': return "MWCBStatus";
            case 'K': return "IPOQuotingPeriodUpdate";
            case 'J': return "LULDAuctionCollar";
            case 'h': return "OperationalHalt";
            case 'A': return "AddOrder";
            case 'F': return "AddOrderMPID";
            case 'E': return "OrderExecuted";
            case 'C': return "OrderExecutedWithPrice";
            case 'X': return "OrderCancel";
            case 'D': return "OrderDelete";
            case 'U': return "OrderReplace";
            case 'P': return "NonCrossTrade";
            case 'Q': return "CrossTrade";
            case 'B': return "BrokenTrade";
            case 'I': return "NOII";
            case 'N': return "RPII";
            case 'O': return "DLCR";
            default:  return "Unknown";
        }
    }
};

} // namespace itch