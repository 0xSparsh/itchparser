#include <gtest/gtest.h>

#include "test_helper.hpp"

namespace itch {
namespace {

using test::TestEngine;

// checked_cast rejects a frame shorter than the message struct: the event
// is counted as truncated and the engine never sees it, while surrounding
// valid messages still parse.
TEST(Parser, TruncatedPayloadCountedAndSkipped) {
    TestEngine t;
    std::vector<std::byte> buf;
    test::emit_msg(buf, test::make_add(1, 2000, 1001, 'B', 100, 15000));
    auto short_add = test::make_add(1, 3000, 1002, 'B', 100, 15000);
    test::emit_frame(buf, &short_add, sizeof(short_add) - 5);  // 31 < 36
    test::emit_msg(buf, test::make_add(1, 4000, 1003, 'B', 100, 15000));

    const std::size_t n = t.run(buf);
    EXPECT_EQ(n, buf.size());
    EXPECT_EQ(t.stats.truncated_messages, 1u);
    EXPECT_EQ(t.engine.stats().add_orders, 2u);  // short one skipped
    EXPECT_EQ(t.engine.stats().live_orders, 2u);
}

// Unknown message types are framing-valid but unhandled: counted as parse
// errors without touching the book.
TEST(Parser, UnknownTypeCountedAsParseError) {
    TestEngine t;
    std::vector<std::byte> buf;
    const std::uint8_t payload[10] = {'Z', 0, 0, 0, 0, 0, 0, 0, 0, 0};
    test::emit_frame(buf, payload, sizeof(payload));

    const std::size_t n = t.run(buf);
    EXPECT_EQ(n, buf.size());
    EXPECT_EQ(t.stats.parse_errors, 1u);
    EXPECT_EQ(t.stats.total_messages, 1u);
    EXPECT_EQ(t.engine.stats().live_orders, 0u);
}

// Exact-size payloads still take the accept path end to end.
TEST(Parser, AddThenDeleteLifecycleThroughParser) {
    TestEngine t;
    std::vector<std::byte> buf;
    test::emit_msg(buf, test::make_add(1, 2000, 5001, 'S', 150, 15025));
    test::emit_msg(buf, test::make_delete(1, 3000, 5001));

    t.run(buf);
    EXPECT_EQ(t.stats.truncated_messages, 0u);
    EXPECT_EQ(t.stats.parse_errors, 0u);
    EXPECT_EQ(t.engine.stats().add_orders, 1u);
    EXPECT_EQ(t.engine.stats().order_delete, 1u);
    EXPECT_EQ(t.engine.stats().live_orders, 0u);
}

} // namespace
} // namespace itch
