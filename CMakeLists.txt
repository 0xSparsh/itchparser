cmake_minimum_required(VERSION 3.10)

project(itch_parser)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(itch_parser
    src/main.cpp
)

target_include_directories(itch_parser PRIVATE
    include
)
