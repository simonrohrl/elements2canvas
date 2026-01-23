cmake_minimum_required(VERSION 3.14)
project(text_painter CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_executable(text_painter
    src/main.cc
    src/text_painter.cc
    src/json_parser.cc
    src/decoration_line_painter.cc
    src/text_decoration_info.cc
    src/text_decoration_painter.cc
)

target_include_directories(text_painter PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
