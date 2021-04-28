cmake_minimum_required(VERSION 3.7.1)
project(v8-with-seastar VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(libs/seastar)
include_directories(include)
include_directories(libs/v8/include)

file(GLOB V8_LIB_MONOLIT "libs/v8/lib/libv8_monolith.a")

add_executable(v8-with-seastar main.cc)
target_link_libraries(v8-with-seastar Seastar::seastar ${V8_LIB_MONOLIT})
