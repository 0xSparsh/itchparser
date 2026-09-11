cmake_minimum_required(VERSION 3.16)

project(itch_parser LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# This project relies on Linux/POSIX system interfaces.
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "itch_parser supports Linux only.")
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(ITCH_BUILD_TESTS "Build unit tests" ON)
option(ITCH_BUILD_BENCHMARKS "Build benchmarks" ON)

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

find_package(Threads REQUIRED)

add_library(itch_core STATIC
    src/MMapFile.cpp
)

target_include_directories(itch_core
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(itch_core
    PUBLIC
        itch_warnings
        itch_optimised
        Threads::Threads
)

add_executable(itch_parser
    src/main.cpp
)

target_link_libraries(itch_parser
    PRIVATE
        itch_core
)

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

if(ITCH_BUILD_BENCHMARKS)
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        googlebenchmark
        URL https://github.com/google/benchmark/archive/v1.8.3.zip
    )

    FetchContent_MakeAvailable(googlebenchmark)

    foreach(bench parser engine book)
        add_executable(bench_${bench}
            benchmark/bench_${bench}.cpp
        )

        target_link_libraries(bench_${bench}
            PRIVATE
                itch_core
                benchmark::benchmark
        )
    endforeach()
endif()

message(STATUS
    "itch_parser: ${CMAKE_BUILD_TYPE}, tests=${ITCH_BUILD_TESTS}"
)
