cmake_minimum_required(VERSION 3.14)
project(block_painter CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(block_painter
    main.cc
    block_painter.cc
    json_parser.cc
)

target_include_directories(block_painter PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
