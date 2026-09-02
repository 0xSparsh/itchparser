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

    void on_message() noexcept {
        
    }

    void on_parse_error() noexcept {}

    void on_truncated() noexcept {}
};

} // namespace itch