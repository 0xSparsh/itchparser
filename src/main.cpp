#include "ITCHProcessor.hpp"
#include "Logger.hpp"
#include "MatchingEngine.hpp"

#include <cerrno>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>

namespace {

void print_usage(const char* argv0) {
    std::fprintf(stderr,
        "Usage: %s [options] <path-to-itch-file>\n"
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
        "  %s 12302019.NASDAQ_ITCH50\n"
        "  %s --order-pool-size 8388608 12302019.NASDAQ_ITCH50\n",
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
                std::fprintf(stderr, "Error: --order-pool-size must be a power-of-two > 0\n");
                return EXIT_FAILURE;
            }
        } else if (arg == "--level-pool-size" && i + 1 < argc) {
            if (!parse_size(argv[++i], level_pool_size) || !is_pow2(level_pool_size)) {
                std::fprintf(stderr, "Error: --level-pool-size must be a power-of-two > 0\n");
                return EXIT_FAILURE;
            }
        } else if (arg == "--log-path" && i + 1 < argc) {
            log_path = argv[++i];
        } else if (arg.size() >= 2 && arg[0] == '-' && arg[1] == '-') {
            std::fprintf(stderr, "Error: unknown option '%s'\n", arg.c_str());
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

    std::fprintf(stderr, "[ITCHProcessor] config:\n");
    std::fprintf(stderr, "  input             : %s\n", path.c_str());
    std::fprintf(stderr, "  log path          : %s\n", log_path.c_str());
    std::fprintf(stderr, "  order pool size   : %zu slots (%.1f MB)\n",
                 order_pool_size,
                 static_cast<double>(order_pool_size * sizeof(itch::Order)) / (1024.0 * 1024.0));
    std::fprintf(stderr, "  level pool size   : %zu slots (%.1f MB)\n",
                 level_pool_size,
                 static_cast<double>(level_pool_size * sizeof(itch::PriceLevel)) / (1024.0 * 1024.0));
    std::fprintf(stderr, "\n");

    itch::LockFreeLogger logger(log_path.c_str());
    itch::MatchingEngine engine(logger, order_pool_size, level_pool_size);

    if (path.size() > 3 && path.compare(path.size() - 3, 3, ".gz") == 0) {
        std::fprintf(stderr,
            "[ITCHProcessor] error: gzip input is not supported "
            "(gzip support removed).\n"
            "  Decompress first: gunzip -k %s\n",
            path.c_str());
        return EXIT_FAILURE;
    }
    const std::string& bin_path = path;

    itch::MMapFile mmap(bin_path.c_str());
    if (!mmap.valid()) {
        std::fprintf(stderr, "[ITCHProcessor] mmap(%s) failed\n", bin_path.c_str());
        return EXIT_FAILURE;
    }
    mmap.advise_sequential();

    logger.start();

    std::fprintf(stderr, "[ITCHProcessor] parsing %zu MiB from %s...\n",
                 mmap.size() / (1024 * 1024), bin_path.c_str());

    itch::Stats stats;
    stats.reset();
    itch::Parser parser;
    parser.run(mmap.data(), mmap.size(), engine, stats);
    stats.finish();

    logger.stop();

    std::fprintf(stderr, "[ITCHProcessor] parse complete. Generating reports...\n");
    std::fflush(stderr);

    stats.print_report(stdout);
    std::fflush(stdout);

    std::fprintf(stderr, "[ITCHProcessor] generating matching engine report...\n");
    std::fflush(stderr);

    // Manually print the engine report (we can't reuse ITCHProcessor's
    // private method, so we inline a simpler version here).
    {
        const auto& es = engine.stats();
        const std::size_t op_cap = engine.order_pool_capacity();
        const std::size_t lp_cap = engine.level_pool_capacity();
        const std::size_t op_free = engine.order_pool_free();
        const std::size_t lp_free = engine.level_pool_free();

        std::fprintf(stdout,
            "================================================================\n"
            "Matching Engine Report\n"
            "================================================================\n"
            "  Instruments registered   : %llu\n"
            "\n"
            "  ---- Accepted events (mutated the book) ----\n"
            "  Add-Order events         : %llu  (no MPID: %llu, with MPID: %llu)\n"
            "  Order-Executed events    : %llu\n"
            "  Order-Executed w/ Price  : %llu\n"
            "  Order-Cancel events      : %llu\n"
            "  Order-Delete events      : %llu\n"
            "  Order-Replace events     : %llu\n"
            "  Non-Cross Trade events   : %llu  (informational)\n"
            "  Cross Trade events       : %llu  (clears the book)\n"
            "  Broken Trade events      : %llu  (informational)\n"
            "\n"
            "  ---- Rejected events (did NOT mutate the book) ----\n"
            "  Add-Order rejected (order pool exhausted)     : %llu\n"
            "  Add-Order rejected (level pool/map exhausted) : %llu\n"
            "  Replace rejected (order pool exhausted)       : %llu\n"
            "  Duplicate Add (id already live)               : %llu\n"
            "  Locate mismatch (E/X/D/U vs home)             : %llu\n"
            "  Level missing (order found, level gone)       : %llu\n"
            "  Unknown-order events (E/X/D/U on missing id)  : %llu\n"
            "  Total pool-exhausted events                   : %llu\n"
            "\n"
            "  ---- Pool lifecycle diagnostics ----\n"
            "  Order pool      : %10zu / %10zu free  (peak live: %llu)\n"
            "  Level pool      : %10zu / %10zu free  (peak live: %llu)\n"
            "  Live orders now : %llu   (should be ~0 after end-of-day cross)\n"
            "  Live levels now : %llu\n"
            "  Order index     : %zu live / %zu slots  (overflow: %zu, collisions: %llu)\n"
            "----------------------------------------------------------------\n",
            static_cast<unsigned long long>(es.instruments_registered),

            static_cast<unsigned long long>(es.add_orders + es.add_orders_with_mpid),
            static_cast<unsigned long long>(es.add_orders),
            static_cast<unsigned long long>(es.add_orders_with_mpid),
            static_cast<unsigned long long>(es.order_executed),
            static_cast<unsigned long long>(es.order_executed_with_price),
            static_cast<unsigned long long>(es.order_cancel),
            static_cast<unsigned long long>(es.order_delete),
            static_cast<unsigned long long>(es.order_replace),
            static_cast<unsigned long long>(es.non_cross_trades),
            static_cast<unsigned long long>(es.cross_trades),
            static_cast<unsigned long long>(es.broken_trades),

            static_cast<unsigned long long>(es.add_rejected_pool_exhausted),
            static_cast<unsigned long long>(es.add_rejected_level_pool_exhausted),
            static_cast<unsigned long long>(es.replace_rejected_pool_exhausted),
            static_cast<unsigned long long>(es.duplicate_adds),
            static_cast<unsigned long long>(es.locate_mismatches),
            static_cast<unsigned long long>(es.level_missing),
            static_cast<unsigned long long>(es.unknown_order_events),
            static_cast<unsigned long long>(es.pool_exhausted),

            op_free, op_cap,
            static_cast<unsigned long long>(es.peak_live_orders),
            lp_free, lp_cap,
            static_cast<unsigned long long>(es.peak_live_levels),
            static_cast<unsigned long long>(es.live_orders),
            static_cast<unsigned long long>(es.live_levels),
            engine.order_index_size(), engine.order_index_capacity(),
            engine.order_index_overflow(),
            static_cast<unsigned long long>(engine.order_index_collisions()));
        std::fflush(stdout);

        // Leak-detection sanity check
        if (es.live_orders != 0 && op_free == op_cap) {
            std::fprintf(stdout,
                "  WARNING: live_orders=%llu but order pool is fully free -\n"
                "           this indicates an accounting bug in the matching engine.\n",
                static_cast<unsigned long long>(es.live_orders));
        } else if (es.live_orders == 0 && op_free < op_cap) {
            std::fprintf(stdout,
                "  WARNING: live_orders=0 but order pool has %zu slots in use —\n"
                "           this indicates an Order was acquired but never released\n"
                "           (a leak in the matching engine).\n",
                op_cap - op_free);
        } else {
            std::fprintf(stdout,
                "  Pool lifecycle: OK (live_orders counter matches pool state).\n");
        }
        std::fprintf(stdout, "----------------------------------------------------------------\n");
        std::fflush(stdout);

        // Print BBO for a few well-known symbols - useful smoke test.
        // We limit to 10 instruments so this is fast even with 8,906
        // registered instruments
        std::fprintf(stdout, "Top-of-book sample (first 10 instruments with live orders):\n");
        std::fflush(stdout);
        std::size_t shown = 0;
        engine.instruments().for_each([&](itch::Instrument* inst) {
            if (shown >= 10) return;
            if (!inst->book || inst->book->live_orders() == 0) return;
            char sym[9] = {};
            std::memcpy(sym, inst->symbol.data(), 8);
            sym[8] = '\0';
            std::fprintf(stdout,
                "  %-8s  bid=%u x %llu   ask=%u x %llu   live_orders=%zu\n",
                sym,
                inst->book->best_bid_price(),
                static_cast<unsigned long long>(inst->book->best_bid_shares()),
                inst->book->best_ask_price(),
                static_cast<unsigned long long>(inst->book->best_ask_shares()),
                inst->book->live_orders());
            ++shown;
        });
        std::fprintf(stdout, "================================================================\n");
        std::fflush(stdout);
    }

    std::fprintf(stderr, "[ITCHProcessor] done.\n");
    return EXIT_SUCCESS;
}