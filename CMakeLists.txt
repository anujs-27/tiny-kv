cmake_minimum_required(VERSION 3.20)
project(tiny-kvstore LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_definitions(_DISABLE_STRING_ANNOTATION)

option(ENABLE_ASAN "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)

if (ENABLE_ASAN AND ENABLE_TSAN)
    message(FATAL_ERROR "ASan and TSan cannot be enabled simultaneously")
endif()

if (ENABLE_ASAN)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
elseif (ENABLE_TSAN)
    add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
    add_link_options(-fsanitize=thread)
endif()

find_package(Threads REQUIRED)

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

add_executable(tinykv src/main.cpp)
target_link_libraries(tinykv PRIVATE 
    tinykv_core
    Threads::Threads
)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(tinykv PRIVATE 
        -Wall -Wextra -Wpedantic -Wconversion -Wshadow
    )
endif()

if (WIN32)
    target_link_libraries(tinykv PRIVATE ws2_32 wsock32)
endif()

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(tinykv_test test/test.cpp)
target_link_libraries(tinykv_test PRIVATE 
    tinykv_core 
    gtest_main  
    Threads::Threads
)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(tinykv_test PRIVATE 
        -Wall -Wextra -Wpedantic -Wconversion -Wshadow
    )
endif()

include(GoogleTest)
gtest_discover_tests(tinykv_test)