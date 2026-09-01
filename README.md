# itch-parser

A high-performance NASDAQ ITCH 5.0 feed parser and limit order book reconstruction engine written in C++23.

## Overview

itch-parser is a high-performance C++23 implementation for parsing NASDAQ ITCH 5.0 market data and reconstructing the full order book in memory.

ITCH is a binary market data protocol that provides order-by-order depth, including order submissions, cancellations, modifications, executions, and other market events. This project is designed to process large ITCH data files efficiently while keeping the core data structures cache-friendly and allocation-efficient.

The project is currently a work in progress. The wire-format layer, memory-mapped file access, and order book data structures are implemented. The byte-to-message parser and end-to-end event processing are still under development.

## Features

- Packed C++ structs for all 23 ITCH 5.0 message types
- Portable big-endian to host-endian conversion
- Zero-copy file access using mmap
- Memory prefaulting and madvise hints for large input files
- Cache-line-aligned order structures
- Price-level based order book representation
- Flat hash maps for fast lookup
- Object pools for efficient memory management
- C++23 implementation with a focus on performance and predictable memory usage

## Status

- [x] On-wire packed structs for all 23 ITCH 5.0 message types
- [x] Big-endian to host-endian conversion
- [x] Portable byte swapping without SSE
- [x] Zero-copy mmap file access
- [x] Prefault and madvise optimizations
- [x] Order book data structures
- [x] Cache-line-aligned orders
- [x] Price levels
- [x] Flat hash map
- [x] Object pools
- [ ] Byte-to-message dispatch loop
- [ ] End-to-end parser
- [ ] Trade and execution processing
- [ ] Tests
- [ ] Benchmarks

## Build

### Requirements

- CMake >= 3.10
- C++23-compatible compiler
- Linux or another platform supporting the required mmap functionality

### Compile

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The current executable exercises the endian-conversion helpers in `src/main.cpp`. Full ITCH parsing is not yet wired into the executable.

## Project Layout

```
include/
  WireFormat.hpp      ITCH 5.0 wire-format definitions
  Endian.hpp          Big-endian conversion utilities
  MMapFile.hpp        Memory-mapped file interface
  Parser.hpp          Message parser interface

src/
  main.cpp            Executable entry point
  MMapFile.cpp        Memory-mapped file implementation

data/
  Sample ITCH files

tests/
  Planned unit and integration tests

benchmark/
  Planned performance benchmarks
```

The core components are intentionally kept lightweight and largely header-only so that the parser and order book can be composed without unnecessary abstraction overhead.

## Architecture

The processing pipeline is intended to follow this structure:

```
ITCH binary file
       |
       v
    mmap()
       |
       v
Byte-level message parser
       |
       v
Message-specific wire format
       |
       v
Endian conversion
       |
       v
Order book / trade processing
       |
       v
In-memory market state
```

The parser will consume the memory-mapped file sequentially, identify each ITCH message by its message type, decode the corresponding wire structure, and apply the event to the appropriate instrument's order book.

## Order Book

The order book is designed around the access patterns of an order-by-order market data feed.

Orders are stored in cache-line-aligned structures, while price levels provide an efficient representation of the bid and ask sides. Flat hash maps are used for fast order and instrument lookup, and object pools reduce the overhead of frequent dynamic allocations.

The goal is to keep the hot path predictable and minimize unnecessary pointer chasing, allocations, and memory-management overhead while processing large ITCH files.

## Data

NASDAQ provides sample ITCH 5.0 historical data files for download.

Place downloaded files in the `data/` directory. For example:

```
data/
└── 12302019.NASDAQ_ITCH50
```

The `12302019.NASDAQ_ITCH50` sample is approximately 8 GB, making it useful for testing the parser against a realistic workload.

Data can be downloaded from the NASDAQ ITCH data repository:

https://emi.nasdaq.com/ITCH/Nasdaq%20ITCH/

## Performance Goals

Performance is a primary design consideration for this project. The implementation aims to:

- Process ITCH messages with minimal per-message overhead
- Avoid unnecessary memory allocations in the hot path
- Make efficient use of CPU caches
- Sequentially consume large memory-mapped files
- Keep message decoding and order book updates lightweight
- Provide a foundation for reproducible benchmarking against large real-world ITCH datasets

Benchmarks and profiling infrastructure will be added as the parser reaches end-to-end functionality.

## Roadmap

The next major milestone is completing the parser and connecting it to the existing order book implementation.

Planned work includes:

- Implement the byte-to-message dispatch loop.
- Decode all supported ITCH message types.
- Connect decoded events to instrument order books.
- Implement order additions, cancellations, modifications, and executions.
- Add trade and market-event processing.
- Add correctness tests using known ITCH message sequences.
- Add benchmarks using large NASDAQ ITCH datasets.
- Profile and optimize the complete processing pipeline.

## Disclaimer

This project is intended for research, experimentation, and performance engineering. It is not intended for live trading or production market-data consumption in its current state.