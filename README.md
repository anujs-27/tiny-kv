# tiny-kv
![CI](https://github.com/anujs-27/tiny-kv/actions/workflows/ci.yml/badge.svg)
![Standard](https://img.shields.io/badge/C%2B%2B-20-blue.svg)
![License](https://img.shields.io/badge/License-MIT-green.svg)

A thread-safe, in-memory key-value store written in C++20. Features LRU eviction, passive and background TTL expiration, and binary persistence behind an HTTP interface.

## Features
* **LRU Eviction**: Discards least-recently used keys when reaching capacity.
* **TTL Support**: Optional millisecond expiration per key via passive checks and a background cleaner thread.
* **Binary Persistence**: Dumps and loads store snapshots to/from disk with payload length checks.
* **Thread Safety**: Thread-safe synchronization via `std::mutex` across HTTP workers and the background sweeper.
* **HTTP API**: REST endpoints implemented with cpp-httplib.

## API Endpoints
### CRUD API Endpoints
* `GET /kv/{key}` - Get a value (404 if missing or expired)
* `PUT /kv/{key}?life={ms}` - Store raw body value (optional TTL in milliseconds)
* `DELETE /kv/{key}` - Delete a key

### ADMIN API Endpoints
> These require ```TINYKV_ADMIN``` env variable set as the bearer token.
* `POST /admin/dump?path={file}` - Save cache snapshot to binary file (defaults to `snapshot.bin`)
* `POST /admin/load?path={file}` - Load snapshot from file and wake the cleaner thread

## Build and Run
Requires a C++20 compiler and CMake 3.20+.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Usage:
```bash
export TINYKV_ADMIN="very-secret-token"
./build/tinykv [capacity] --verbose|-v
```
> The default capacity of the server is 1000 items if capacity is not set.

## Build with sanitisers
```bash
# Memory leaks and bounds checking (ASan)
cmake -B build-asan -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON
cmake --build build-asan

# Data race detection (TSan)
cmake -B build-tsan -DCMAKE_BUILD_TYPE=Debug -DENABLE_TSAN=ON
cmake --build build-tsan
```

## Running Tests
Build and run GoogleTest suite:

```bash
ctest --test-dir build --output-on-failure
```