cmake_minimum_required(VERSION 3.14)
project(block_painter CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_executable(block_painter
    src/main.cc
    src/block_painter.cc
    src/json_parser.cc
)

target_include_directories(block_painter PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
