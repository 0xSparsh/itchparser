#include "ITCHProcessor.hpp"
#include "Logger.hpp"
#include "MatchingEngine.hpp"

#include <cerrno>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <print>
#include <string>

namespace {

void print_usage(const char* argv0) {
    std::println(stderr,
        "Usage: {} [options] <path-to-itch-file>\n"
        "\n"
        "  <path-to-itch-file> must be a raw decompressed ITCH file\n"
        "  (e.g. 12302019.NASDAQ_ITCH50). Gzip (.gz) is NOT supported —\n"
        "  decompress first with: gunzip -k <file>.gz\n"
        "\n"
        "Options:\n"
        "  --order-pool-size N     Order pool capacity (default: 16777216). Power of two.\n"
        "  --level-pool-size N     PriceLevel pool capacity (default: 4194304). Power of two.\n"
        "  --log-path PATH         Where to write the diagnostic log (default: itch.log).\n"
        "  --help, -h              Show this message.\n"
        "\n"
        "Example:\n"
        "  {} 12302019.NASDAQ_ITCH50\n"
        "  {} --order-pool-size 8388608 12302019.NASDAQ_ITCH50",
        argv0, argv0, argv0);
}

bool parse_size(const char* s, std::size_t& out) {
    char* end = nullptr;
    errno = 0;
    unsigned long long v = std::strtoull(s,&end, 10);
    if (errno != 0 || end == s || *end != '\0' || v == 0) return false;
    out = static_cast<std::size_t>(v);
    return true;
}

bool is_pow2(std::size_t n) { return n > 0 && (n & (n - 1)) == 0; }

} // namespace

int main(int argc, char** argv) {
    std::string path;
    std::string log_path = "itch.log";
    std::size_t order_pool_size = itch::kOrderPoolSize;
    std::size_t level_pool_size = itch::kPriceLevelPoolSize;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return EXIT_SUCCESS;
        } else if (arg == "--order-pool-size" && i + 1 < argc) {
            if (!parse_size(argv[++i], order_pool_size) || !is_pow2(order_pool_size)) {
                std::println(stderr, "Error: --order-pool-size must be a power-of-two > 0");
                return EXIT_FAILURE;
            }
        } else if (arg == "--level-pool-size" && i + 1 < argc) {
            if (!parse_size(argv[++i], level_pool_size) || !is_pow2(level_pool_size)) {
                std::println(stderr, "Error: --level-pool-size must be a power-of-two > 0");
                return EXIT_FAILURE;
            }
        } else if (arg == "--log-path" && i + 1 < argc) {
            log_path = argv[++i];
        } else if (arg.size() >= 2 && arg[0] == '-' && arg[1] == '-') {
            std::println(stderr, "Error: unknown option '{}'", arg);
            print_usage(argv[0]);
            return EXIT_FAILURE;
        } else {
            path = arg;
        }
    }

    if (path.empty()) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    std::println(stderr, "[ITCHProcessor] config:");
    std::println(stderr, "  input             : {}", path);
    std::println(stderr, "  log path          : {}", log_path);
    std::println(stderr, "  order pool size   : {} slots ({:.1f} MB)",
                 order_pool_size,
                 static_cast<double>(order_pool_size * sizeof(itch::Order)) / (1024.0 * 1024.0));
    std::println(stderr, "  level pool size   : {} slots ({:.1f} MB)",
                 level_pool_size,
                 static_cast<double>(level_pool_size * sizeof(itch::PriceLevel)) / (1024.0 * 1024.0));
    std::println(stderr, "");

    itch::LockFreeLogger logger(log_path.c_str());
    itch::MatchingEngine engine(logger, order_pool_size, level_pool_size);

    if (path.ends_with(".gz")) {
        std::println(stderr,
            "[ITCHProcessor] error: gzip input is not supported "
            "(gzip support removed).\n"
            "  Decompress first: gunzip -k {}",
            path);
        return EXIT_FAILURE;
    }
    const std::string& bin_path = path;

    auto mapped = itch::MMapFile::open(bin_path.c_str());
    if (!mapped) {
        std::println(stderr, "[ITCHProcessor] mmap({}) failed: {}",
                     bin_path, mapped.error().message());
        return EXIT_FAILURE;
    }
    itch::MMapFile mmap = std::move(*mapped);
    mmap.advise_sequential();

    logger.start();

    std::println(stderr, "[ITCHProcessor] parsing {} MiB from {}...",
                 mmap.size() / (1024 * 1024), bin_path);

    itch::Stats stats;
    stats.reset();
    itch::Parser parser;
    parser.run(mmap.data(), mmap.size(), engine, stats);
    stats.finish();

    logger.stop();

    std::println(stderr, "[ITCHProcessor] parse complete. Generating reports...");
    std::fflush(stderr);

    stats.print_report(stdout);
    std::fflush(stdout);

    std::println(stderr, "[ITCHProcessor] generating matching engine report...");
    std::fflush(stderr);

    // Manually print the engine report (we can't reuse ITCHProcessor's
    // private method, so we inline a simpler version here).
    {
        const auto& es = engine.stats();
        const std::size_t op_cap = engine.order_pool_capacity();
        const std::size_t lp_cap = engine.level_pool_capacity();
        const std::size_t op_free = engine.order_pool_free();
        const std::size_t lp_free = engine.level_pool_free();

        std::println(stdout,
            "================================================================\n"
            "Matching Engine Report\n"
            "================================================================\n"
            "  Instruments registered   : {}\n"
            "\n"
            "  ---- Accepted events (mutated the book) ----\n"
            "  Add-Order events         : {}  (no MPID: {}, with MPID: {})\n"
            "  Order-Executed events    : {}\n"
            "  Order-Executed w/ Price  : {}\n"
            "  Order-Cancel events      : {}\n"
            "  Order-Delete events      : {}\n"
            "  Order-Replace events     : {}\n"
            "  Non-Cross Trade events   : {}  (informational)\n"
            "  Cross Trade events       : {}  (clears the book)\n"
            "  Broken Trade events      : {}  (informational)\n"
            "\n"
            "  ---- Rejected events (did NOT mutate the book) ----\n"
            "  Add-Order rejected (order pool exhausted)     : {}\n"
            "  Add-Order rejected (level pool/map exhausted) : {}\n"
            "  Replace rejected (order pool exhausted)       : {}\n"
            "  Duplicate Add (id already live)               : {}\n"
            "  Locate mismatch (E/X/D/U vs home)             : {}\n"
            "  Level missing (order found, level gone)       : {}\n"
            "  Unknown-order events (E/X/D/U on missing id)  : {}\n"
            "  Total pool-exhausted events                   : {}\n"
            "\n"
            "  ---- Pool lifecycle diagnostics ----\n"
            "  Order pool      : {:10} / {:10} free  (peak live: {})\n"
            "  Level pool      : {:10} / {:10} free  (peak live: {})\n"
            "  Live orders now : {}   (should be ~0 after end-of-day cross)\n"
            "  Live levels now : {}\n"
            "  Order index     : {} live / {} slots  (overflow: {}, collisions: {})\n"
            "----------------------------------------------------------------",
            es.instruments_registered,

            es.add_orders + es.add_orders_with_mpid,
            es.add_orders,
            es.add_orders_with_mpid,
            es.order_executed,
            es.order_executed_with_price,
            es.order_cancel,
            es.order_delete,
            es.order_replace,
            es.non_cross_trades,
            es.cross_trades,
            es.broken_trades,

            es.add_rejected_pool_exhausted,
            es.add_rejected_level_pool_exhausted,
            es.replace_rejected_pool_exhausted,
            es.duplicate_adds,
            es.locate_mismatches,
            es.level_missing,
            es.unknown_order_events,
            es.pool_exhausted,

            op_free, op_cap,
            es.peak_live_orders,
            lp_free, lp_cap,
            es.peak_live_levels,
            es.live_orders,
            es.live_levels,
            engine.order_index_size(), engine.order_index_capacity(),
            engine.order_index_overflow(),
            engine.order_index_collisions());
        std::fflush(stdout);

        // Leak-detection sanity check
        if (es.live_orders != 0 && op_free == op_cap) {
            std::println(stdout,
                "  WARNING: live_orders={} but order pool is fully free -\n"
                "           this indicates an accounting bug in the matching engine.",
                es.live_orders);
        } else if (es.live_orders == 0 && op_free < op_cap) {
            std::println(stdout,
                "  WARNING: live_orders=0 but order pool has {} slots in use —\n"
                "           this indicates an Order was acquired but never released\n"
                "           (a leak in the matching engine).",
                op_cap - op_free);
        } else {
            std::println(stdout,
                "  Pool lifecycle: OK (live_orders counter matches pool state).");
        }
        std::println(stdout, "----------------------------------------------------------------");
        std::fflush(stdout);

        // Print BBO for a few well-known symbols - useful smoke test.
        // We limit to 10 instruments so this is fast even with 8,906
        // registered instruments
        std::println(stdout, "Top-of-book sample (first 10 instruments with live orders):");
        std::fflush(stdout);
        std::size_t shown = 0;
        engine.instruments().for_each([&](itch::Instrument* inst) {
            if (shown >= 10) return;
            if (!inst->book || inst->book->live_orders() == 0) return;
            char sym[9] = {};
            std::memcpy(sym, inst->symbol.data(), 8);
            sym[8] = '\0';
            std::println(stdout,
                "  {:<8}  bid={} x {}   ask={} x {}   live_orders={}",
                sym,
                inst->book->best_bid_price(),
                inst->book->best_bid_shares(),
                inst->book->best_ask_price(),
                inst->book->best_ask_shares(),
                inst->book->live_orders());
            ++shown;
        });
        std::println(stdout, "================================================================");
        std::fflush(stdout);
    }

    std::println(stderr, "[ITCHProcessor] done.");
    return EXIT_SUCCESS;
}