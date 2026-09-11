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
#include <print>
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
            std::println(stderr, "[ITCHProcessor] mmap({}) failed", path);
            return false;
        }

        mmap.advise_sequential();

        logger_.start();

        std::println(stderr, "[ITCHProcessor] parsing {} MiB fron {}...",
                     mmap.size() / (1024 * 1024), path);
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

        std::println(out,
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
            engine_.order_index_size(), engine_.order_index_capacity(),
            engine_.order_index_overflow(),
            engine_.order_index_collisions());

        // Leak-detection
        if(es.live_orders != 0 && op_free == op_cap) {
            std::println(out,
                "  WARNING: live_orders={} but order pool is fully free —\n"
                "           this indicates an accounting bug in the matching engine.",
                es.live_orders);
        } else if (es.live_orders == 0 && op_free < op_cap) {
            std::println(out,
                "  WARNING: live_orders=0 but order pool has {} slots in use —\n"
                "           this indicates an Order was acquired but never released\n"
                "           (a leak in the matching engine).",
                op_cap - op_free);
        } else {
            std::println(out,
                "  Pool lifecycle: OK (live_orders counter matches pool state).");
        }
        std::println(out, "----------------------------------------------------------------");

        // Print BBO for a few well-known symbols — useful smoke test.
        std::println(out, "Top-of-book sample (first 10 instruments with live orders):");
        std::size_t shown = 0;
        engine_.instruments().for_each([&](Instrument* inst) {
            if (shown >= 10) return;
            if (!inst->book || inst->book->live_orders() == 0) return;
            char sym[9] = {};
            std::memcpy(sym, inst->symbol.data(), 8);
            sym[8] = '\0';
            std::println(out,
                "  {:<8}  bid={} x {}   ask={} x {}   live_orders={}",
                sym,
                inst->book->best_bid_price(),
                inst->book->best_bid_shares(),
                inst->book->best_ask_price(),
                inst->book->best_ask_shares(),
                inst->book->live_orders());
            ++shown;
        });
        std::println(out, "================================================================");
    }

    std::string     log_path_;
    LockFreeLogger  logger_;
    MatchingEngine  engine_;
    Stats           stats_;
};

} // namespace itch