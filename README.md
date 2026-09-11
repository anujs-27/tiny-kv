# Tiny-KV

A simple in-memory key-value store built in C++20 with LRU eviction, key expiration, and binary file persistence. It serves as a hands-on project to explore thread synchronization, memory management, and writing automated tests with GoogleTest.

## Features
- **C++20 Implementation**: Leverages modern C++ features.
- **LRU Eviction**: Automatically evicts the least recently used keys when capacity is reached.
- **Key Expiration (TTL)**: Supports expiration times for cached keys.
- **Binary Persistence**: Save and load the state using binary file serialization.
- **Thread-Safe**: Designed for safe concurrent access.
- **Tested**: Comprehensive test suite written using GoogleTest.

## Getting Started
### Prerequisites
- A C++20 compatible compiler (GCC, Clang, or MSVC)
- CMake (version 3.14 or higher)

### Building the Project
```bash
mkdir build && cd build
cmake ..
cmake --build .