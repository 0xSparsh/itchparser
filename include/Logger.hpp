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
#include <unistd.h>
#include <vector>
#include <cstdlib>
#include <algorithm>

namespace itch {

enum class LogLevel : std::uint8_t {
    Trace = 0,
    Debug = 1,
    Info  = 2,
    Warn  = 3,
    Error = 4,
    Fatal = 5
};

struct alignas(64) LogEntry {
    std::uint64_t timestamp_ns;
    LogLevel      level;
    char          tag[7];
    char          message[240];
};

static_assert(sizeof(LogEntry) == 256);
static_assert(alignof(LogEntry) == 64);

class LockFreeLogger {
public:
    LockFreeLogger(const char* path,
                   std::size_t capacity = 65536)
        : path_(path)
        , capacity_(capacity)
        , mask_(capacity - 1)
        , ring_(capacity)
    {
        if (capacity == 0 || (capacity & (capacity - 1)) != 0) {
            std::fprintf(stderr,
                "[Logger] capacity must be power-of-two, got %zu\n",
                capacity);
            std::abort();
        }
    }

    ~LockFreeLogger() { stop(); }

    LockFreeLogger(const LockFreeLogger&)            = delete;
    LockFreeLogger& operator=(const LockFreeLogger&) = delete;
    LockFreeLogger(LockFreeLogger&&)                 = delete;
    LockFreeLogger& operator=(LockFreeLogger&&)      = delete;

    void start() {
        if (running_.load(std::memory_order_acquire)) return;

        fd_ = ::open(path_, O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, 0644);     // 0644 is a file permission number. 
        if (fd_ < 0) {
            std::fprintf(stderr, "[Logger] open(%s) failed: %s\n",
                         path_, std::strerror(errno));
            return;
        }

        running_.store(true, std::memory_order_release);
        drain_thread_ = std::thread([this] { drain_loop(); });
    }

    void stop() noexcept {
        if (!running_.exchange(false, std::memory_order_acq_rel)) return;

        if (drain_thread_.joinable())
            drain_thread_.join();

        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
    }

    // SPSC producer: never blocks or allocates
    template <std::size_t N>
    void log(LogLevel level, const char (&tag)[N],
             std::string_view msg) noexcept {
        if (static_cast<std::uint8_t>(level) <
            min_level_.load(std::memory_order_relaxed)) {
            return;
        }

        const std::size_t h = head_.load(std::memory_order_relaxed);
        const std::size_t t = tail_.load(std::memory_order_acquire);

        if (h - t >= capacity_) [[unlikely]] {
            dropped_.fetch_add(1, std::memory_order_relaxed);
            return;
        }

        LogEntry& e = ring_[h & mask_];

        e.timestamp_ns = static_cast<std::uint64_t>(
            std::chrono::steady_clock::now().time_since_epoch().count());
        e.level = level;

        copy_tag(e.tag, tag);
        copy_msg(e.message, msg);

        head_.store(h + 1, std::memory_order_release);
    }

    template <std::size_t N>
    void info(const char (&tag)[N], std::string_view msg) noexcept {
        log(LogLevel::Info, tag, msg);
    }

    template <std::size_t N>
    void warn(const char (&tag)[N], std::string_view msg) noexcept {
        log(LogLevel::Warn, tag, msg);
    }

    template <std::size_t N>
    void error(const char (&tag)[N], std::string_view msg) noexcept {
        log(LogLevel::Error, tag, msg);
    }

    template <std::size_t N>
    void debug(const char (&tag)[N], std::string_view msg) noexcept {
        log(LogLevel::Debug, tag, msg);
    }

    void set_min_level(LogLevel l) noexcept {
        min_level_.store(static_cast<std::uint8_t>(l),
                         std::memory_order_relaxed);
    }

    [[nodiscard]] std::uint64_t dropped() const noexcept {
        return dropped_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] std::uint64_t produced() const noexcept {
        return head_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] std::uint64_t flushed() const noexcept {
        return tail_.load(std::memory_order_relaxed);
    }

private:
    void drain_loop() noexcept {
        constexpr std::size_t kBatch = 64;
        std::array<char, sizeof(LogEntry) * kBatch> scratch{};

        while (running_.load(std::memory_order_acquire)) {
            const std::size_t h = head_.load(std::memory_order_acquire);
            const std::size_t t = tail_.load(std::memory_order_relaxed);

            if (t == h) {
                std::this_thread::sleep_for(
                    std::chrono::microseconds(50));
                continue;
            }

            const std::size_t count =
                std::min<std::size_t>(kBatch, h - t);

            char* out = scratch.data();

            for (std::size_t i = 0; i < count; ++i) {
                const LogEntry& e = ring_[(t + i) & mask_];

                int n = std::snprintf(
                    out,
                    sizeof(LogEntry),
                    "%020llu %s [%.*s] %.*s\n",
                    static_cast<unsigned long long>(e.timestamp_ns),
                    level_to_str(e.level),
                    6,
                    e.tag,
                    static_cast<int>(sizeof(LogEntry::message)),
                    e.message);

                if (n <= 0) continue;

                if (static_cast<std::size_t>(n) > sizeof(LogEntry))
                    n = static_cast<int>(sizeof(LogEntry));

                out += n;
            }

            write_batch(scratch.data(),
                        static_cast<std::size_t>(out - scratch.data()));

            tail_.store(t + count, std::memory_order_release);
        }

        while (true) {
            const std::size_t h = head_.load(std::memory_order_acquire);
            const std::size_t t = tail_.load(std::memory_order_relaxed);

            if (t == h) break;

            const std::size_t count =
                std::min<std::size_t>(kBatch, h - t);

            char* out = scratch.data();

            for (std::size_t i = 0; i < count; ++i) {
                const LogEntry& e = ring_[(t + i) & mask_];

                int n = std::snprintf(
                    out,
                    sizeof(LogEntry),
                    "%020llu %s [%.*s] %.*s\n",
                    static_cast<unsigned long long>(e.timestamp_ns),
                    level_to_str(e.level),
                    6,
                    e.tag,
                    static_cast<int>(sizeof(LogEntry::message)),
                    e.message);

                if (n <= 0) continue;

                if (static_cast<std::size_t>(n) > sizeof(LogEntry))
                    n = static_cast<int>(sizeof(LogEntry));

                out += n;
            }

            write_batch(scratch.data(),
                        static_cast<std::size_t>(out - scratch.data()));

            tail_.store(t + count, std::memory_order_release);
        }
    }

    void write_batch(const char* data, std::size_t size) noexcept {
        if (size == 0 || fd_ < 0) return;

        std::size_t written = 0;

        while (written < size) {
            const ssize_t n =
                ::write(fd_, data + written, size - written);

            if (n < 0) {
                if (errno == EINTR) continue;
                break;
            }

            written += static_cast<std::size_t>(n);
        }
    }

    static const char* level_to_str(LogLevel l) noexcept {
        switch (l) {
            case LogLevel::Trace: return "TRACE";
            case LogLevel::Debug: return "DEBUG";
            case LogLevel::Info:  return "INFO ";
            case LogLevel::Warn:  return "WARN ";
            case LogLevel::Error: return "ERROR";
            case LogLevel::Fatal: return "FATAL";
        }

        return "?    ";
    }

    template <std::size_t N>
    static void copy_tag(char (&dst)[7],
                         const char (&src)[N]) noexcept {
        constexpr std::size_t kCopy =
            (N - 1 < 6) ? (N - 1) : 6;

        for (std::size_t i = 0; i < kCopy; ++i)
            dst[i] = src[i];

        for (std::size_t i = kCopy; i < 7; ++i)
            dst[i] = '\0';
    }

    static void copy_msg(char (&dst)[240],
                         std::string_view src) noexcept {
        const std::size_t n =
            (src.size() < 239) ? src.size() : 239;

        if (n > 0)
            std::memcpy(dst, src.data(), n);

        dst[n] = '\0';
    }

    // Producer-owned state
    alignas(kCacheLineSize) const char* path_;
    const std::size_t capacity_;
    const std::size_t mask_;
    int fd_{-1};

    alignas(kCacheLineSize) std::vector<LogEntry> ring_;

    // SPSC producer/consumer counters
    alignas(kCacheLineSize) std::atomic<std::size_t> head_{0};
    alignas(kCacheLineSize) std::atomic<std::size_t> tail_{0};

    alignas(kCacheLineSize) std::atomic<std::uint64_t> dropped_{0};
    alignas(kCacheLineSize) std::atomic<std::uint8_t> min_level_{
        static_cast<std::uint8_t>(LogLevel::Info)
    };

    alignas(kCacheLineSize) std::atomic<bool> running_{false};
    std::thread drain_thread_;
};

} // namespace itch
