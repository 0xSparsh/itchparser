#include <benchmark/benchmark.h>

#include "Logger.hpp"
#include "MatchingEngine.hpp"
#include "MMapFile.hpp"
#include "Parser.hpp"
#include "Stats.hpp"
#include "Types.hpp"
#include "WireFormat.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace itch {
namespace {

// Every benchmark uses 20000 messages per iteration.
constexpr std::size_t kMessages = 20000;

// tiny helpers to build fake ITCH buffers

inline void emit_frame(std::vector<std::byte>& out, const void* data,
                       std::size_t len) {
    out.push_back(static_cast<std::byte>((len >> 8) & 0xFF));
    out.push_back(static_cast<std::byte>(len & 0xFF));
    const auto* p = static_cast<const std::byte*>(data);
    out.insert(out.end(), p, p + len);
}

inline void put_be64(wire::Byte (&a)[8], std::uint64_t v) {
    for (int i = 0; i < 8; ++i)
        a[i] = static_cast<wire::Byte>((v >> (56 - 8 * i)) & 0xFF);
}

inline void put_be32(wire::Byte (&a)[4], std::uint32_t v) {
    a[0] = static_cast<wire::Byte>((v >> 24) & 0xFF);
    a[1] = static_cast<wire::Byte>((v >> 16) & 0xFF);
    a[2] = static_cast<wire::Byte>((v >> 8) & 0xFF);
    a[3] = static_cast<wire::Byte>(v & 0xFF);
}

// A fake 'A' (Add Order) message. Numbers are stored big-endian.
inline wire::AddOrderMsg make_add(std::uint64_t oid, char side,
                                  std::uint32_t shares, std::uint32_t price,
                                  std::uint64_t ts) {
    wire::AddOrderMsg m{};
    m.hdr.messageType = 'A';
    m.hdr.stockLocateHi = 0;
    m.hdr.stockLocateLo = 1;  // instrument 1
    put_be64(m.orderReferenceNumber, oid);
    m.buySellIndicator = static_cast<wire::Byte>(side);
    put_be32(m.shares, shares);
    std::memset(m.stock.data(), ' ', 8);
    std::memcpy(m.stock.data(), "BENCH", 5);
    put_be32(m.price, price);
    for (int i = 0; i < 6; ++i)
        m.hdr.ts[i] = static_cast<std::uint8_t>((ts >> (40 - 8 * i)) & 0xFF);
    return m;
}

inline wire::OrderDeleteMsg make_delete(std::uint64_t oid) {
    wire::OrderDeleteMsg m{};
    m.hdr.messageType = 'D';
    m.hdr.stockLocateHi = 0;
    m.hdr.stockLocateLo = 1;
    put_be64(m.orderReferenceNumber, oid);
    return m;
}

// Buffer of kMessages Add Order messages with ids starting at id_base.
inline std::vector<std::byte> build_adds(std::uint64_t id_base) {
    std::vector<std::byte> buf;
    buf.reserve(kMessages * 40);
    for (std::size_t i = 0; i < kMessages; ++i) {
        auto m = make_add(id_base + i, (i & 1) ? 'S' : 'B', 100,
                          15000 + static_cast<std::uint32_t>(i % 64), 2000 + i);
        emit_frame(buf, &m, sizeof(m));
    }
    return buf;
}

// Buffer with a mix: add, add, delete, repeat (so the book stays small).
inline std::vector<std::byte> build_mixed(std::uint64_t id_base) {
    std::vector<std::byte> buf;
    buf.reserve(kMessages * 40);
    std::uint64_t next = id_base;
    for (std::size_t i = 0; i < kMessages; ++i) {
        if (i % 3 == 2) {
            // Delete an order we added two steps ago (it is still live).
            auto m = make_delete(next - 2);
            emit_frame(buf, &m, sizeof(m));
        } else {
            auto m = make_add(next, (i & 1) ? 'S' : 'B', 100,
                              15000 + static_cast<std::uint32_t>(i % 64),
                              2000 + i);
            emit_frame(buf, &m, sizeof(m));
            ++next;
        }
    }
    return buf;
}

// One shared parser + engine. Ids keep increasing so iterations never clash.
struct SharedParser {
    LockFreeLogger logger{"/dev/null"};  // never started: no log thread
    MatchingEngine engine{logger, 1u << 20, 1u << 18};
    Parser parser{};
    Stats stats{};
    std::uint64_t base{1};
    std::uint64_t total_bytes{0};
    SharedParser() { stats.reset(); }
};

// Delete every live order so the next pass starts from an empty book.
inline void drain(MatchingEngine& engine) {
    std::vector<std::pair<StockLocate, OrderReferenceNumber>> ids;
    engine.instruments().for_each([&](Instrument* inst) {
        if (!inst->book) return;
        inst->book->for_each_order(
            [&](Order* o) { ids.emplace_back(o->locate, o->order_id); });
    });
    for (const auto& [loc, id] : ids) engine.on_order_delete(loc, id);
};

// Time: parsing Add Order messages.
static void BM_Parser_Adds(benchmark::State& state) {
    SharedParser p;
    for (auto _ : state) {
        state.PauseTiming();
        std::vector<std::byte> buf = build_adds(p.base);
        p.base += kMessages;
        p.stats.reset();
        state.ResumeTiming();
        std::size_t n =
            p.parser.run(buf.data(), buf.size(), p.engine, p.stats);
        benchmark::DoNotOptimize(n);
        benchmark::ClobberMemory();
        state.PauseTiming();  // drain the adds so the pool never fills up
        for (std::size_t i = 0; i < kMessages; ++i)
            p.engine.on_order_delete(1, p.base - kMessages + i);
        state.ResumeTiming();
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) *
                            static_cast<std::int64_t>(kMessages));
    state.SetBytesProcessed(static_cast<std::int64_t>(state.iterations()) *
                            static_cast<std::int64_t>(
                                kMessages * (2 + sizeof(wire::AddOrderMsg))));
}
BENCHMARK(BM_Parser_Adds);

// Time: parsing a mix of adds and deletes.
static void BM_Parser_Mixed(benchmark::State& state) {
    SharedParser p;
    for (auto _ : state) {
        state.PauseTiming();
        std::vector<std::byte> buf = build_mixed(p.base);
        // build_mixed consumes ~2/3 * kMessages fresh ids per buffer.
        p.base += kMessages;
        p.stats.reset();
        state.ResumeTiming();
        std::size_t n =
            p.parser.run(buf.data(), buf.size(), p.engine, p.stats);
        benchmark::DoNotOptimize(n);
        benchmark::ClobberMemory();
        p.total_bytes += buf.size();
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) *
                            static_cast<std::int64_t>(kMessages));
    state.SetBytesProcessed(static_cast<std::int64_t>(p.total_bytes));
}
BENCHMARK(BM_Parser_Mixed);

// Time: parsing the first 16 MiB of the real data file, if it exists.
// Skipped automatically when there is no file.
static void BM_Parser_RealFile(benchmark::State& state) {
    std::string path;
    if (const char* env = std::getenv("ITCH_DATA"); env && *env) {
        path = env;
    } else if (std::filesystem::is_regular_file(
                   "data/12302019.NASDAQ_ITCH50")) {
        path = "data/12302019.NASDAQ_ITCH50";
    } else if (std::filesystem::is_regular_file(
                   "../data/12302019.NASDAQ_ITCH50")) {
        path = "../data/12302019.NASDAQ_ITCH50";
    }
    if (path.empty()) {
        state.SkipWithMessage("no ITCH file (set ITCH_DATA=... to enable)");
        return;
    }
    auto mapped = MMapFile::open(path.c_str());
    if (!mapped) {
        state.SkipWithMessage("mmap failed");
        return;
    }
    MMapFile mmap = std::move(*mapped);
    constexpr std::size_t kSlice = 16u << 20;
    const std::size_t slice = mmap.size() < kSlice ? mmap.size() : kSlice;
    LockFreeLogger logger("/dev/null");
    MatchingEngine engine(logger, 1u << 22, 1u << 20);  // roomy pools
    Parser parser;
    Stats stats;
    std::uint64_t total_msgs = 0;
    for (auto _ : state) {
        stats.reset();
        std::size_t n = parser.run(mmap.data(), slice, engine, stats);
        benchmark::DoNotOptimize(n);
        benchmark::ClobberMemory();
        total_msgs += stats.total_messages;
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(total_msgs));
    state.SetBytesProcessed(static_cast<std::int64_t>(slice));
}
BENCHMARK(BM_Parser_RealFile)
    ->Iterations(1)
    ->UseRealTime()
    ->Unit(benchmark::kSecond);

}  // namespace
}  // namespace itch

BENCHMARK_MAIN();
