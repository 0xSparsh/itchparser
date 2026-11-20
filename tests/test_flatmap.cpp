#include <gtest/gtest.h>

#include "FlatMap.hpp"

#include <cstdint>
#include <vector>

namespace itch {
namespace {

// kEmpty = 0xFFFFFFFF mirrors the production LevelMap configuration:
// ITCH Price(4) values never reach 0xFFFFFFFF (max 2,000,000,000).
using SmallMap = FlatMap<std::uint32_t, void*, 0xFFFFFFFFu, 64>;

TEST(FlatMap, InitialSlotsArePowerOfTwo) {
    // 64 initial slots, capacity doubles on demand.
    SmallMap m;
    EXPECT_EQ(m.capacity(), 0u);   // nothing allocated until first insert
    int x = 1;
    EXPECT_TRUE(m.insert(1, &x));
    EXPECT_EQ(m.capacity(), 64u);
    EXPECT_TRUE((64u & 63u) == 0u);
}

TEST(FlatMap, InsertThenFind) {
    SmallMap m;
    int x = 1;
    int y = 2;
    EXPECT_TRUE(m.insert(100, &x));
    EXPECT_TRUE(m.insert(200, &y));
    EXPECT_EQ(m.find(100), &x);
    EXPECT_EQ(m.find(200), &y);
    EXPECT_EQ(m.size(), 2u);
}

TEST(FlatMap, FindMissingReturnsNull) {
    SmallMap m;
    EXPECT_EQ(m.find(1), nullptr);
    EXPECT_EQ(m.find(6), nullptr);
    EXPECT_EQ(m.find(0xFFFFFFFEu), nullptr);
}

TEST(FlatMap, InsertDuplicateKeepsFirstValue) {
    SmallMap m;
    int x = 1, y = 2;
    EXPECT_TRUE(m.insert(42, &x));
    EXPECT_FALSE(m.insert(42, &y));   // not overwritten: caller decides
    EXPECT_EQ(m.find(42), &x);
    EXPECT_EQ(m.size(), 1u);          // size unchanged on duplicate
}

TEST(FlatMap, EraseRemovesAndShrinks) {
    SmallMap m;
    int x = 1;
    m.insert(7, &x);
    EXPECT_EQ(m.size(), 1u);
    EXPECT_TRUE(m.erase(7));
    EXPECT_EQ(m.size(), 0u);
    EXPECT_EQ(m.find(7), nullptr);
}

TEST(FlatMap, EraseMissingIsNoOp) {
    SmallMap m;
    int x = 1;
    m.insert(7, &x);
    EXPECT_NO_FATAL_FAILURE(m.erase(999));
    EXPECT_EQ(m.size(), 1u);
    EXPECT_EQ(m.find(7), &x);
}

// Deleting an entry must not break the probe chain of entries that collided
// past it: backward-shift repair keeps every remaining key reachable, and no
// tombstones are left behind.
TEST(FlatMap, EraseRepairsProbeChainNoTombstones) {
    SmallMap m;
    std::vector<int> vals(40);
    for (int i = 0; i < 40; ++i) {
        vals[i] = i;
        ASSERT_TRUE(m.insert(static_cast<std::uint32_t>(i + 1), &vals[i]));
    }
    EXPECT_EQ(m.size(), 40u);

    for (int i = 0; i < 40; i += 2)
        m.erase(static_cast<std::uint32_t>(i + 1));
    EXPECT_EQ(m.size(), 20u);

    for (int i = 1; i < 40; i += 2) {
        void* p = m.find(static_cast<std::uint32_t>(i + 1));
        ASSERT_NE(p, nullptr) << "lost key " << (i + 1)
                              << " after probe-chain repair";
        EXPECT_EQ(*static_cast<int*>(p), i);
    }
    for (int i = 0; i < 40; i += 2)
        EXPECT_EQ(m.find(static_cast<std::uint32_t>(i + 1)), nullptr);
}

TEST(FlatMap, ClearResetsAll) {
    SmallMap m;
    int x = 1;

    for (int i = 0; i < 20; ++i) {
        m.insert(static_cast<uint32_t>(i + 1), &x);
    }
    EXPECT_EQ(m.size(), 20u);
    m.clear();
    EXPECT_EQ(m.size(), 0u);
    for (int i = 0; i < 20; ++i) {
        EXPECT_EQ(m.find(static_cast<uint32_t>(i + 1)), nullptr);
    }

    // Check reuseability after clear
    EXPECT_TRUE(m.insert(1, &x));
    EXPECT_EQ(m.find(1), &x);
}

TEST(FlatMap, ForEachVisitsEveryEntry) {
    SmallMap m;
    int vals[10];

    for (int i = 0; i < 10; ++i) {
        vals[i] = i * 10;
        m.insert(static_cast<std::uint32_t>(100 + i), &vals[i]);
    }

    int count = 0;
    long sum = 0;
    m.for_each([&](std::uint32_t k, void* v) {
        ++count;
        sum += *static_cast<int*>(v);
        EXPECT_GE(k, 100u);
    });
    EXPECT_EQ(count, 10);
    EXPECT_EQ(sum, 450);
}

// 0 is a valid key (the empty sentinel is 0xFFFFFFFF); the largest key that
// is not the sentinel must also work.
TEST(FlatMap, HandlesZeroKeyAndMaxKey) {
    SmallMap m;
    int a = 1, b = 2;
    EXPECT_TRUE(m.insert(0, &a));
    EXPECT_TRUE(m.insert(0xFFFFFFFEu, &b));
    EXPECT_EQ(m.find(0), &a);
    EXPECT_EQ(m.find(0xFFFFFFFEu), &b);
}

// Grows past the initial capacity while keeping every entry reachable.
TEST(FlatMap, GrowsPastInitialCapacity) {
    SmallMap m;
    constexpr int N = 200;   // 64 slots -> 128 -> 256 while inserting
    std::vector<int> vals(N);
    for (int i = 0; i < N; ++i) {
        vals[i] = i;
        ASSERT_TRUE(m.insert(static_cast<std::uint32_t>(i * 7919 + 13),
                             &vals[i]));
    }
    EXPECT_EQ(m.size(), (std::size_t)N);
    EXPECT_GE(m.capacity(), 256u);
    for (int i = 0; i < N; ++i) {
        void* p = m.find(static_cast<std::uint32_t>(i * 7919 + 13));
        ASSERT_NE(p, nullptr);
        EXPECT_EQ(*static_cast<int*>(p), i);
    }
}

// Stress: many keys spread across the keyspace, erase half, re-check all.
TEST(FlatMap, BulkInsertFindEraseStress) {
    SmallMap m;
    constexpr int N = 4000;
    std::vector<int> vals(N);
    for (int i = 0; i < N; ++i) {
        vals[i] = i;
        ASSERT_TRUE(m.insert(static_cast<std::uint32_t>(i * 7919 + 13),
                             &vals[i]));
    }
    EXPECT_EQ(m.size(), (std::size_t)N);

    for (int i = 0; i < N; i += 2)
        ASSERT_TRUE(m.erase(static_cast<std::uint32_t>(i * 7919 + 13)));
    EXPECT_EQ(m.size(), (std::size_t)N / 2);

    for (int i = 0; i < N; ++i) {
        void* p = m.find(static_cast<std::uint32_t>(i * 7919 + 13));
        if (i % 2 == 0) {
            EXPECT_EQ(p, nullptr) << "key " << i << " survived its erase";
        } else {
            ASSERT_NE(p, nullptr) << "lost key " << i;
            EXPECT_EQ(*static_cast<int*>(p), i);
        }
    }
}

} // namespace
} // namespace itch
