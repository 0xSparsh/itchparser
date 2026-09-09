#include <gtest/gtest.h>

#include "OrderBook.hpp"

#include <cstdint>
#include <vector>

namespace itch {
namespace {

using PriceMap = FlatHashMap<Price, PriceLevel*, 64, 0xFFFFFFFFu>;
using SmallMap = FlatHashMap<std::uint32_t, void*, 64, 0xFFFFFFFFu>;

TEST(FlatHashMap, CapacityIsPowerOfTwo) {
    EXPECT_EQ(SmallMap::Capacity, 64u);
    EXPECT_EQ((64u & 63u), 0u);
    EXPECT_EQ(OrderBook::kPriceLevelMapCapacity, 8192u);
}

TEST(FlatHashMap, InsertThenFind) {
    SmallMap m;
    int x = 1;
    int y = 2;
    EXPECT_TRUE(m.insert(100, &x));
    EXPECT_TRUE(m.insert(200, &y));
    EXPECT_EQ(m.find(100), &x);
    EXPECT_EQ(m.find(200), &y);
    EXPECT_EQ(m.size(), 2u);
}

TEST(FlatHashMap, FindMissingReturnsNull) {
    SmallMap m;
    EXPECT_EQ(m.find(1), nullptr);
    EXPECT_EQ(m.find(6), nullptr);
    EXPECT_EQ(m.find(0xFFFFFFFEu), nullptr);
}

TEST(FlatHashMap, InsertDuplicateOverwritesReturnsFalse) {
    SmallMap m;
    int x = 1; y = 2;
    EXPECT_TRUE(m.insert(42, &x));
    EXPECT_FALSE(m.insert(42, &y));     // Overwrites path returns false
    EXPECT_EQ(m.find(42), &y);
    EXPECT_EQ(m.size(), 1u);        // size unchanged on overwrite
}

TEST(FlatHashMap, EraseRemovesAndShrinks) {
    SmallMap m;
    int x = 1;
    m.insert(7, &x);
    EXPECT_EQ(m.size(), 1u);
    m.erase(7);
    EXPECT_EQ(m.size(), 0u);
    EXPECET_EQ(m.find(), nullptr);
}

TEST(FlatHashMap, EraseMissingIsNoOp) {
    SmallMap m;
    int x = 1;
    m.insert(7, &x);
    EXPECT_NO_FATAL_FAILURE(m.erase(999));
    EXPECT_EQ(m.size(), 1u);
    EXPECT_EQ(m.find(7), &x);
}

TEST(FlatHashMap, EraseRepairsProbeChainNoTombstones) {
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

    for (int i = 0; i < 40; i += 2) {
        void* p = m.find(static_cast<std::uint32_t>(i + 1));
        ASSERT_NE(p, nullptr) << "lost key " << (i + 1)
                              << "after proble -chain repair";
        EXPECT_EQ(*static_cast<int*>(p), i);
    }

    for (int i = 0; i < 40; i += 2) {
        EXPECT_EQ(m.find(static_cast<std::uint32_t>(i + 1)), nullptr);
    }
}

TEST(FlatHashMap, ClearResetsAll) {
    SmallMap m;
    int x = 1;

    for (int i = 0; i < 20; ++i) {
        m.insert(static_cast<uint32_t>(i + 1), &x);
    }
    EXPECT_EQ(m.size(), 20u);
    m.clear();
    EXPECT_EQ(m.size(), 0u);
    for (int i = 0; i < 20; ++i) {
        EXPECT_EQ(m.find(i), nullptr);
    }

    // Check reuseability after clear
    EXPECT_TRUE(m.insert(1, &x));
    EXPECT_EQ(m.find(1), &x);
}

TEST(FlatHashMap, ForEachVisitsEveryEntry) {
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

TEST(FlatHashMap, HandlesZeroKeyAndMaxKey) {
    SmallMap m;
    int a = 1, b = 2;
    EXPECT_TRUE(m.insert(0, &a));  // 0 is a valid key (empty sentinel is -1)
    EXPECT_TRUE(m.insert(0xFFFFFFFEu, &b));
    EXPECT_EQ(m.find(0), &a);
    EXPECT_EQ(m.find(0xFFFFFFFEu), &b);
}

TEST(FlatHashMap, BulkInsertFindStress) {
    constexpr int N = 4000;  // fits in 8192-slot production map
    FlatHashMap<std::uint32_t, void*, 8192, 0xFFFFFFFFu> m;
    std::vector<int> vals(N);
    for (int i = 0; i < N; ++i) {
        vals[i] = i;
        ASSERT_TRUE(m.insert(static_cast<std::uint32_t>(i * 7919 + 13), &vals[i]));
    }
    EXPECT_EQ(m.size(), (std::size_t)N);
    for (int i = 0; i < N; ++i) {
        void* p = m.find(static_cast<std::uint32_t>(i * 7919 + 13));
        ASSERT_NE(p, nullptr);
        EXPECT_EQ(*static_cast<int*>(p), i);
    }
}

} // namespace
} // namespace itch