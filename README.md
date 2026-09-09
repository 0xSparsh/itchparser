# itch-parser

A high-performance NASDAQ ITCH 5.0 feed parser and limit order book reconstruction engine written in C++23.

## Overview

itch-parser parses NASDAQ TotalView-ITCH 5.0 binary market data and reconstructs a full, per-instrument, price-time-ordered limit order book in memory. ITCH is an order-by-order depth protocol: every order submission, cancellation, modification, and execution arrives as a discrete binary message, and the receiver is responsible for maintaining book state itself - there is no server-side book snapshot.

The pipeline mmaps the raw ITCH file, walks it message-by-message with a zero-allocation dispatch loop, decodes each of the 23 ITCH 5.0 message types from their big-endian wire format, and feeds the decoded events into a matching engine that applies them to the correct instrument's order book using fixed-capacity object pools (no `malloc`/`new` in the hot path).

**Status: the parser, wire decoding, and matching engine are complete and working end-to-end.** The project has been run against real NASDAQ sample data - see [Results](#results) below. Automated tests and benchmarks are the main remaining gap (see [Roadmap](#roadmap)).

## Results

Run against `12302019.NASDAQ_ITCH50`, an ~8 GB real NASDAQ sample data file, on a single thread, no parallelism:

```
Elapsed wall-clock      : 91.733 s
Total messages          : 268,744,780
Total payload bytes     : 7,713,918,349 (7356.57 MiB)
Throughput (msgs/sec)   : 2.93 M
Throughput (MiB/sec)    : 85.78
Parse errors            : 0
Truncated messages      : 0
```

All 23 message types were decoded and dispatched correctly (breakdown of AddOrder, OrderDelete, OrderReplace, OrderExecuted, NOII, etc. available in the run report). The matching engine processed every accepted event with:

- **0** pool-exhausted rejections (order pool sized at 16.7M slots, peak live orders observed: 1.92M; level pool sized at 4.2M slots, peak live levels observed: 784K)
- **0** post-run live orders/levels - the book fully drains via the closing cross, which is the expected end-of-day invariant and the primary self-check that the engine isn't leaking or double-freeing pool slots
- A direct order index (array fast path, small `unordered_map` overflow for hash collisions) handling all 268M lookups with a modest, bounded collision count

This is a single-threaded, single-pass run with no I/O overlap or multi-core parallelism - it's the baseline the project is being benchmarked and optimized from, not a ceiling.

## Features

- Packed C++ structs for all 23 ITCH 5.0 message types, matching the wire layout exactly (no padding, no copies beyond the unavoidable unaligned field loads)
- Portable big-endian -> host-endian conversion, using compiler byte-swap intrinsics with a constexpr-safe fallback for compile-time evaluation
- Zero-copy file access via `mmap`, with `madvise(MADV_SEQUENTIAL)` and page prefaulting for large input files
- A single-pass byte-to-message dispatch loop: length-prefixed framing, per-type length validation, and a `switch`-based dispatcher the compiler can inline
- Cache-line-aligned `Order` structs (`sizeof(Order) <= 64 bytes`, `static_assert`-enforced) to avoid false sharing and keep hot-path lookups to one cache line
- A price-level order book: intrusive doubly-linked lists of price levels (sorted by price, best bid/ask at the head) and intrusive doubly-linked lists of orders within each level
- A custom open-addressing flat hash map (`FlatHashMap`) for price → level lookups, and a two-tier direct-indexed array + overflow hash map (`DirectOrderIndex`) for order-ID → order lookups
- Fixed-capacity object pools (`MemoryPool<T>`) for both `Order` and `PriceLevel` - one contiguous allocation per pool, an intrusive free list, no per-message heap traffic
- A lock-free logger for diagnostics that doesn't sit in the hot path
- Detailed self-reporting: per-message-type stats, matching-engine accept/reject counters, pool lifecycle diagnostics, and an automatic leak-detection check comparing live-order counters against pool occupancy
- C++23, built with `-O3 -march=native -flto`

## Build

### Requirements

- CMake >= 3.16
- A C++23-capable compiler (GCC or Clang; the project uses `__builtin_bswap*` and other GCC/Clang builtins)
- Linux or another platform supporting POSIX `mmap`

### Compile

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Run

```sh
./build/itch_parser <path-to-itch-file>
```

The input must be a raw, decompressed ITCH file (e.g. `12302019.NASDAQ_ITCH50`). Gzip is not supported - decompress first with `gunzip -k <file>.gz`.

Options:

```
--order-pool-size N     Order pool capacity (default: 16777216). Power of two.
--level-pool-size N     PriceLevel pool capacity (default: 4194304). Power of two.
--log-path PATH         Where to write the diagnostic log (default: itch.log).
--help, -h              Show this message.
```

Example:

```sh
./build/itch_parser data/12302019.NASDAQ_ITCH50
./build/itch_parser --order-pool-size 8388608 data/12302019.NASDAQ_ITCH50
```

On completion the process prints a full parse report (throughput, per-message-type breakdown) and a matching-engine report (accepted/rejected event counts, pool lifecycle diagnostics, a top-of-book sample for the first 10 active instruments).

## Project Layout

```
include/
  WireFormat.hpp      Packed on-wire structs for all 23 ITCH 5.0 message types
  Endian.hpp           Big-endian -> host conversion helpers
  MMapFile.hpp         mmap-based zero-copy file interface
  Parser.hpp           Byte-to-message dispatch loop and per-type handlers
  Types.hpp            Shared aliases, enums, and tunable constants
  Order.hpp            Cache-line-aligned Order struct
  PriceLevel.hpp       Intrusive doubly-linked order list per price level
  OrderBook.hpp        Per-instrument order book, price-sorted level lists, FlatHashMap
  InstrumentMap.hpp    StockLocate -> Instrument direct index
  MatchingEngine.hpp   Routes decoded events to the right OrderBook; DirectOrderIndex; EngineStats
  MemoryPool.hpp       Fixed-capacity object pool with an intrusive free list
  Stats.hpp            Parser-level throughput/latency/message-count tracking
  Logger.hpp           Lock-free diagnostic logger
  ITCHProcessor.hpp    Top-level driver: mmap -> parse -> report

src/
  main.cpp             CLI entry point, argument parsing
  MMapFile.cpp         MMapFile implementation

data/
  Sample ITCH files (not checked in - see Data below)

tests/       (planned - see Roadmap)
benchmark/   (planned - see Roadmap)
```

The core components are intentionally kept lightweight and largely header-only so that the parser and order book can be composed without unnecessary abstraction overhead.

## Architecture

```
ITCH binary file
       |
       v
    mmap() + madvise(SEQUENTIAL) + prefault
       |
       v
Parser::run() - length-prefixed framing loop over the mapped buffer
       |
       v
dispatch(type) - switch on message type, per-type length validation
       |
       v
Endian conversion (load_be16/32/48/64) + wire struct decode
       |
       v
MatchingEngine::on_*() - locate the instrument, route to its OrderBook
       |
       v
OrderBook - DirectOrderIndex lookup, PriceLevel insert/update/remove,
            MemoryPool<Order> / MemoryPool<PriceLevel> acquire/release
       |
       v
In-memory market state (best bid/ask per instrument, full depth)
```

See [DESIGN.md](DESIGN.md) for the reasoning behind each of these components - why a direct-indexed order lookup instead of a plain hash map, how the pools are sized, why orders are cache-line aligned, and the tradeoffs made at each layer.

## Order Book

The order book is designed around the access patterns of an order-by-order market data feed: adds, cancels, and deletes dominate (together >94% of message volume in the sample run above), and every one of them needs O(1) order lookup and O(1) level insert/remove at the best bid/ask.

- **Orders** live in a fixed pool and are cache-line aligned, linked into their price level via an intrusive doubly-linked list (`next_`/`prev_` embedded in `Order` itself - no separate list-node allocation).
- **Price levels** are kept in a price-sorted intrusive doubly-linked list per side (best bid/ask at the list head, so top-of-book reads are O(1)), and are also indexed by a flat open-addressing hash map (`FlatHashMap<Price, PriceLevel*>`) for O(1) price → level lookup on adds.
- **Order lookup** (needed on every Execute/Cancel/Delete/Replace) goes through `DirectOrderIndex`: a `2^25`-slot direct array keyed by `order_id & mask` for the common case, with a small `unordered_map` overflow for the rare collision or for IDs that don't fit the direct window.

## Data

NASDAQ provides sample ITCH 5.0 historical data files for download. Place downloaded files in the `data/` directory, e.g.:

```
data/
└── 12302019.NASDAQ_ITCH50
```

The `12302019.NASDAQ_ITCH50` sample is approximately 8 GB, making it useful for testing the parser against a realistic workload.

Data can be downloaded from the NASDAQ ITCH data repository: https://emi.nasdaq.com/ITCH/Nasdaq%20ITCH/

## Performance Goals

- Minimal per-message overhead: no heap allocation, no dynamic dispatch, no copies beyond unavoidable unaligned field loads
- Sequential mmap access with the kernel readahead hints it needs to prefetch effectively
- Cache-friendly hot-path structures (cache-line-aligned orders, intrusive linked lists instead of pointer-chasing through separate node allocations)
- A foundation for reproducible benchmarking against large real-world ITCH datasets (see Roadmap)

## Roadmap

The core pipeline works end-to-end; the next milestones are proving and measuring that:

- [ ] Unit tests: endian conversion round-trips, `MemoryPool` exhaustion/reuse, `DirectOrderIndex` collision handling, `FlatHashMap` insert/erase/probe-chain repair
- [ ] Order book correctness tests: hand-built message sequences with known expected book state (add/cancel/delete/replace/execute, partial fills, closing-cross book clear)
- [ ] An independent correctness check against a second, deliberately simple reference implementation on a smaller file
- [ ] Benchmarks: per-message-type decode cost, add-order latency percentiles (not just aggregate throughput), throughput sensitivity to pool size and order-index load factor
- [ ] CI (build + test on push)
- [ ] Optional: pipeline the parse loop and matching engine across threads via a lock-free queue

## Disclaimer

This project is intended for research, experimentation, and performance engineering. It is not intended for live trading or production market-data consumption in its current state.