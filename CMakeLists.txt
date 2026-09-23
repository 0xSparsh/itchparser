cmake_minimum_required(VERSION 3.16)

# Prefer GCC 14 when no compiler has been explicitly selected.
if(NOT DEFINED CMAKE_CXX_COMPILER)
    find_program(GXX_14 g++-14)
    if(GXX_14)
        set(CMAKE_CXX_COMPILER "${GXX_14}" CACHE FILEPATH "C++ compiler")
    endif()
endif()

project(itch_parser
    VERSION 1.0
    LANGUAGES CXX
)

# Project configuration
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "itch_parser requires Linux.")
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(ITCH_BUILD_TESTS "Build unit tests" ON)
option(ITCH_BUILD_BENCHMARKS "Build benchmarks" ON)

# Compiler configuration
add_library(itch_warnings INTERFACE)

target_compile_options(itch_warnings INTERFACE
    -Wall
    -Wextra
    -Wpedantic
    -Wno-unused-parameter
    -Wno-unused-variable
    -Wno-missing-field-initializers
)

add_library(itch_optimised INTERFACE)

target_compile_options(itch_optimised INTERFACE
    $<$<CONFIG:Release>:-O3>
    $<$<CONFIG:Release>:-march=native>
    $<$<CONFIG:Release>:-mtune=native>
    $<$<CONFIG:Release>:-funroll-loops>
    $<$<CONFIG:Release>:-fno-semantic-interposition>
    $<$<CONFIG:Release>:-flto>

    $<$<CONFIG:RelWithDebInfo>:-O3>
    $<$<CONFIG:RelWithDebInfo>:-march=native>
    $<$<CONFIG:RelWithDebInfo>:-mtune=native>
    $<$<CONFIG:RelWithDebInfo>:-g3>
    $<$<CONFIG:RelWithDebInfo>:-ggdb3>
)

target_link_options(itch_optimised INTERFACE
    $<$<CONFIG:Release>:-flto>
)

# C++23 <print> is required by the project.
include(CheckCXXSourceCompiles)

check_cxx_source_compiles([=[
    #include <print>

    int main()
    {
        std::println("hello {}", 42);
    }
]=] ITCH_HAS_PRINT)

if(NOT ITCH_HAS_PRINT)
    message(FATAL_ERROR
        "itch_parser requires a compiler with C++23 <print> support. "
        "Use GCC 14+ or Clang 20+ with GCC 14+ libstdc++."
    )
endif()

find_package(Threads REQUIRED)

# Core library
add_library(itch_core STATIC
    src/MMapFile.cpp
)

target_include_directories(itch_core
    PUBLIC
        "${PROJECT_SOURCE_DIR}/include"
)

target_compile_features(itch_core PUBLIC cxx_std_23)

target_link_libraries(itch_core
    PUBLIC
        Threads::Threads
        itch_warnings
        itch_optimised
)

# Main executable
add_executable(itch_parser
    src/main.cpp
)

target_link_libraries(itch_parser
    PRIVATE
        itch_core
)

# Tests
if(ITCH_BUILD_TESTS)
    include(FetchContent)

    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/v1.14.0.zip
    )

    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(googletest)

    enable_testing()
    include(GoogleTest)

    add_executable(itch_tests
        tests/test_endian.cpp
        tests/test_types.cpp
        tests/test_wire.cpp
        tests/test_engine.cpp
        tests/test_flathashmap.cpp
        tests/test_mmap.cpp
        tests/test_logger.cpp
        tests/test_parser.cpp
    )

    target_link_libraries(itch_tests
        PRIVATE
            itch_core
            GTest::gtest_main
    )

    gtest_discover_tests(itch_tests
        DISCOVERY_TIMEOUT 60
    )
endif()

# Benchmarks
if(ITCH_BUILD_BENCHMARKS)
    include(FetchContent)

    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        googlebenchmark
        URL https://github.com/google/benchmark/archive/v1.8.3.zip
    )

    FetchContent_MakeAvailable(googlebenchmark)

    foreach(benchmark IN ITEMS parser engine book)
        add_executable(bench_${benchmark}
            benchmark/bench_${benchmark}.cpp
        )

        target_link_libraries(bench_${benchmark}
            PRIVATE
                itch_core
                benchmark::benchmark
        )
    endforeach()
endif()

message(STATUS
    "itch_parser ${PROJECT_VERSION}: "
    "build=${CMAKE_BUILD_TYPE}, "
    "tests=${ITCH_BUILD_TESTS}, "
    "benchmarks=${ITCH_BUILD_BENCHMARKS}"
)
