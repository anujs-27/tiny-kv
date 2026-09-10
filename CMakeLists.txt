cmake_minimum_required(VERSION 3.20)
project(tiny-kvstore LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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
    target_compile_options(tinykv_core PUBLIC "$<$<CONFIG:Debug>:-fsanitize=address,undefined>")
    target_link_options(tinykv_core PUBLIC "$<$<CONFIG:Debug>:-fsanitize=address,undefined>")
endif()

add_executable(tinykv src/main.cpp)
target_link_libraries(tinykv PRIVATE tinykv_core)

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(tinykv_test test/test.cpp)
target_link_libraries(tinykv_test PRIVATE tinykv_core gtest_main)

include(GoogleTest)
gtest_discover_tests(tinykv_test)