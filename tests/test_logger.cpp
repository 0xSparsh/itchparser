#include <gtest/gtest.h>

#include "Logger.hpp"

#include <fcntl.h>
#include <thread>
#include <atomic>

namespace itch {
namespace {

TEST(Logger, CapacityMustBePowerOfTwoDeath) {
    EXPECT_DEATH({ LockFreeLogger l("/dev/null", 1000); }, ".*power.*");
    EXPECT_DEATH({ LockFreeLogger l("/dev/null", 0); }, ".*");
}

TEST(Logger, StartStopIdempotent) {
    LockFreeLogger l("/dev/null");
    EXPECT_NO_FATAL_FAILURE(l.start());
    EXPECT_NO_FATAL_FAILURE(l.start());  // second start no-op
    EXPECT_NO_FATAL_FAILURE(l.stop());
    EXPECT_NO_FATAL_FAILURE(l.stop());  // second stop no-op
}

TEST(Logger, LogWithoutStartDoesNotCrash) {
    LockFreeLogger l("/dev/null");
    EXPECT_NO_FATAL_FAILURE(l.info("TAG", "message before start"));
    EXPECT_GE(l.produced(), 1u);
    // Never started: nothing flushed, nothing dropped (capacity huge).
    EXPECT_EQ(l.flushed(), 0u);
    EXPECT_EQ(l.dropped(), 0u);
}

TEST(Logger, MessagesBelowMinLevelFiltered) {
    LockFreeLogger l("/dev/null");
    l.set_min_level(LogLevel::Error);
    l.info("TAG", "this is info — filtered");
    l.debug("TAG", "debug — filtered");
    EXPECT_EQ(l.produced(), 0u);
    l.error("TAG", "error — kept");
    EXPECT_EQ(l.produced(), 1u);
}

TEST(Logger, DropOnFullCounted) {
    const std::size_t cap = 8;
    LockFreeLogger l("/dev/null", cap);
    // Do NOT start: tail never advances, so ring must fill then drop.
    for (int i = 0; i < 20; ++i) l.info("TAG", "fill");
    EXPECT_EQ(l.produced(), cap);       // only cap accepted
    EXPECT_EQ(l.dropped(), 20u - cap);  // rest dropped, counted
}

TEST(Logger, FileReceivesFlushedContent) {
    char tmpl[] = "/tmp/itch_log_XXXXXX";
    int fd = ::mkstemp(tmpl);
    ::close(fd);
    std::string path = tmpl;
    {
        LockFreeLogger l(path.c_str(), 64);
        l.start();
        l.info("ENG", "hello-engine");
        l.warn("PAR", "hello-parser");
        // stop() drains residue before joining.
        l.stop();
        EXPECT_EQ(l.flushed(), l.produced());
        EXPECT_GT(l.produced(), 0u);
    }
    FILE* f = ::fopen(path.c_str(), "r");
    ASSERT_NE(f, nullptr);
    char buf[8192] = {};
    size_t n = ::fread(buf, 1, sizeof(buf) - 1, f);
    ::fclose(f);
    ::unlink(path.c_str());
    EXPECT_GT(n, 0u);
    EXPECT_NE(::strstr(buf, "hello-engine"), nullptr);
    EXPECT_NE(::strstr(buf, "hello-parser"), nullptr);
    EXPECT_NE(::strstr(buf, "INFO"), nullptr);
    EXPECT_NE(::strstr(buf, "WARN"), nullptr);
}

TEST(Logger, LongMessageTruncatedNotOverflow) {
    char tmpl[] = "/tmp/itch_loglong_XXXXXX";
    int fd = ::mkstemp(tmpl);
    ::close(fd);
    std::string path = tmpl;
    {
        LockFreeLogger l(path.c_str(), 64);
        l.start();
        std::string big(1000, 'Q');
        l.info("TAG", big);  // 1000 chars into a 240-char slot
        l.stop();
    }
    ::unlink(path.c_str());
    SUCCEED();  // absence of ASan/overflow is the assertion
}

TEST(Logger, ConcurrentProducerSingleConsumer) {
    LockFreeLogger l("/dev/null", 1u << 12);
    l.start();
    std::atomic<bool> go{false};
    std::thread prod([&] {
        while (!go.load()) std::this_thread::yield();
        for (int i = 0; i < 5000; ++i) l.info("THR", "concurrent message");
    });
    go.store(true);
    prod.join();
    l.stop();
    // Accepted pushes == flushed (stop() drains residue); attempts ==
    // accepted + dropped. Ring is 4096, attempts 5000, so some drops are
    // expected — but every attempt is accounted for.
    EXPECT_EQ(l.produced(), l.flushed());
    EXPECT_EQ(l.produced() + l.dropped(), 5000u);
}

TEST(Logger, LogEntryLayoutSingleCacheLineMultiple) {
    EXPECT_EQ(sizeof(LogEntry), 256u);
    EXPECT_EQ(alignof(LogEntry), 64u);
}

} // namespace
} // namespace itch