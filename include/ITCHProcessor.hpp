#pragma once 

#include "Logger.hpp"
#include "MatchingEngine.hpp"
#include "Parser.hpp"
#include "MMapFile.hpp"
#include "Stats.hpp"
#include "Types.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>

namespace itch {

class ITCHProcessor {
public:
    explicit ITCHProcessor(std::string_view log_path = "itch.log")
        : log_path_(log_path)
        , logger_(log_path_.c_str())
        , engine_(logger_)
    {}

    ~ITCHProcessor() { logger_.stop(); }

    ITCHProcessor(const ITCHProcessor&) = delete;
    ITCHProcessor& operator=(const ITCHProcessor&) = delete;

    bool run(const std::string& path) {
        MMapFile mmap(path.c_str());
        if (!mmap.valid()) {
            std::fprintf(stderr, "[ITCHProcessor] mmap(%s) failed\n",
                         path.c_str());
            return false;
        }

        mmap.advise_sequential();

        logger_.start();

        std::fprintf(stderr, "[ITCHProcessor] parsing %zu MiB fron %s...\n",
                     mmap.size() / (1024 * 1024), path.c_str());
        stats_.reset();
        Parser parser;
        parser.run(mmap.data(), mmap.size(), engine_, stats_);
        stats_.finish();
        
        logger_.stop();

        stats_.print_report(stdout);
        print_engine_report(stdout);

        return true;
    }

    [[nodiscard]] const Stats& stats() const noexcept { return stats_; }
    [[nodiscard]] const EngineStats& engine_stats() const noexcept { return engine_.stats(); }
    [[nodiscard]] MatchingEngine& engine() noexcept { return engine_; }
    [[nodiscard]] LockFreeLogger& logger() noexcept { return logger_; }

private:

    void print_engine_report(std::FILE* out) {
        const auto& es = engine_.stats();
        const std::size_t op_cap = engine_.order_pool_capacity();
        const std::size_t lp_cap = engine_.level_pool_capacity();
        const std::size_t op_free = engine_.order_pool_free();
        const std::size_t lp_free = engine_.level_pool_free();

        std::fprintf(out,
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
            engine_.order_index_size(), engine_.order_index_capacity(),
            engine_.order_index_overflow(),
            static_cast<unsigned long long>(engine_.order_index_collisions()));

        // Leak-detection
        if(es.live_orders != 0 && op_free == op_cap) {
            std::fprintf(out,
                "  WARNING: live_orders=%llu but order pool is fully free —\n"
            "           this indicates an accounting bug in the matching engine.\n",
            static_cast<unsigned long long>(es.live_orders));
        } else if (es.live_orders == 0 && op_free < op_cap) {
            std::fprintf(out,
                "  WARNING: live_orders=0 but order pool has %zu slots in use —\n"
            "           this indicates an Order was acquired but never released\n"
            "           (a leak in the matching engine).\n",
            op_cap - op_free);
        } else {
            std::fprintf(out,
                "  Pool lifecycle: OK (live_orders counter matches pool state).\n");
        }
        std::fprintf(out, "----------------------------------------------------------------\n");

        // Print BBO for a few well-known symbols — useful smoke test.
        std::fprintf(out, "Top-of-book sample (first 10 instruments with live orders):\n");
        std::size_t shown = 0;
        engine_.instruments().for_each([&](Instrument* inst) {
            if (shown >= 10) return;
            if (!inst->book || inst->book->live_orders() == 0) return;
            char sym[9] = {};
            std::memcpy(sym, inst->symbol.data(), 8);
            sym[8] = '\0';
            std::fprintf(out,
                "  %-8s  bid=%u x %llu   ask=%u x %llu   live_orders=%zu\n",
                sym,
                inst->book->best_bid_price(),
                static_cast<unsigned long long>(inst->book->best_bid_shares()),
                inst->book->best_ask_price(),
                static_cast<unsigned long long>(inst->book->best_ask_shares()),
                inst->book->live_orders());
            ++shown;
        });
        std::fprintf(out, "================================================================\n");
    }

    std::string     log_path_;
    LockFreeLogger  logger_;
    MatchingEngine  engine_;
    Stats           stats_;
};

} // namespace itch