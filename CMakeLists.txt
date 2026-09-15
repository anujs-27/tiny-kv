cmake_minimum_required(VERSION 3.20)
project(tiny-kvstore LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_definitions(_DISABLE_STRING_ANNOTATION)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options("$<$<CONFIG:Debug>:-fsanitize=address,undefined>")
    add_link_options("$<$<CONFIG:Debug>:-fsanitize=address,undefined>")
endif()

find_package(Threads REQUIRED)

# Core library
add_library(tinykv_core STATIC
    src/kvstore.cpp
)

target_include_directories(tinykv_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(tinykv_core PRIVATE 
        -Wall -Wextra -Wpedantic -Wconversion -Wshadow
    )
endif()

# Server executable
add_executable(tinykv src/main.cpp)
target_link_libraries(tinykv PRIVATE 
    tinykv_core
    Threads::Threads
)

if (WIN32)
    target_link_libraries(tinykv PRIVATE ws2_32 wsock32)
endif()

# External dependencies
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(googletest)

# Test executable
enable_testing()

add_executable(tinykv_test test/test.cpp)
target_link_libraries(tinykv_test PRIVATE 
    tinykv_core 
    gtest_main  
    Threads::Threads
)

include(GoogleTest)
gtest_discover_tests(tinykv_test)