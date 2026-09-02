#pragma once

#include "Types.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <string_view>
#include <thread>
#include <vector>

namespace itch {

enum class LogLevel : uint8_t {
    Trace = 0,
    Debug = 1,
    Info  = 2,
    Warn  = 3,
    Error = 4,
    Fatal = 5
};

struct alignas(64) LogEntry {
    uint64_t timestamp_ns;
    LogLevel level;
    char     tag[7];
    char     message[240];
};

static_assert(sizeof(LogEntry) == 256);
static_assert(alignof(LogEntry) == 64);

class LockFreeLogger {
public:
    LockFreeLogger() {}

    ~LockFreeLogger() {}

    LockFreeLogger(const LockFreeLogger&) = delete;
    LockFreeLogger& operator=(const LockFreeLogger&) = delete;

    LockFreeLogger(LockFreeLogger&&) = delete;
    LockFreeLogger& operator=(LockFreeLogger&&) = delete;

    void start() noexcept {}

    void stop() noexcept {}

    void log() noexcept {}

    void info() noexcept {}

    void warn() noexcept {}

    void error() noexcept {}
    
    void debug() noexcept {}

    void set_min_level() noexcept {}

    uint64_t dropped() noexcept {}

    uint64_t produced() noexcept {}

    uint64_t flushed() noexcept {}

private:
    void drain() noexcept {}

    void write_batch() noexcept {}

    static const char* level_to_str() noexcept {}
    
    static void copy_msg() noexcept {}

    // Pre-Owned State
    alignas(kCacheLineSize) const char* path_;
    const std::size_t capacity_;
    const std::size_t mask_;
    int fd_{-1};

    alignas(kCacheLineSize) std::vector<LogEntry> ring_;

    // SPSC producer/consumer counters
    alignas(kCacheLineSize) std::atomic<std::size_t> head_{0};
    alignas(kCacheLineSize) std::atomic<std::size_t> tail_{0};


};

} // namespace itch