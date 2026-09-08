cmake_minimum_required(VERSION 3.16)

project(itch_parser LANGUAGES CXX)

# Build configuration
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)        # use -std=c++23 not -std=gnu++23

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Compiler flags — aggressive optimisation for low-latency hot paths
add_compile_options(
    -Wall -Wextra -Wpedantic
    -Wno-unused-parameter          # we have many "future-proof" parameters
    -Wno-unused-variable
    -Wno-missing-field-initializers
)

# Release/RelWithDebInfo flags
add_compile_options(
    $<$<CONFIG:Release>:-O3>
    $<$<CONFIG:Release>:-march=native>     # use whatever SIMD the host CPU has
    $<$<CONFIG:Release>:-mtune=native>
    $<$<CONFIG:Release>:-funroll-loops>
    $<$<CONFIG:Release>:-fno-semantic-interposition>
    $<$<CONFIG:Release>:-flto>             # whole-program link-time optimisation

    # RelWithDebInfo keeps debug info (-g) but otherwise matches Release.
    $<$<CONFIG:RelWithDebInfo>:-O3>
    $<$<CONFIG:RelWithDebInfo>:-march=native>
    $<$<CONFIG:RelWithDebInfo>:-g3>
    $<$<CONFIG:RelWithDebInfo>:-ggdb3>
)

# Linker flags for LTO consistency.
add_link_options(
    $<$<CONFIG:Release>:-flto>
)

add_executable(itch_parser
    src/main.cpp
    src/MMapFile.cpp
)

target_include_directories(itch_parser PRIVATE
    include
)

target_link_libraries(itch_parser PRIVATE
    pthread                            # for std::thread in the logger
)