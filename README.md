# tiny-kv
A thread-safe, in-memory key-value store written in C++20. Features LRU eviction, passive and background TTL expiration, and binary persistence behind an HTTP interface.

## Features
* **LRU Eviction**: Discards least-recently used keys when reaching capacity.
* **TTL Support**: Optional millisecond expiration per key via passive checks and a background cleaner thread.
* **Binary Persistence**: Dumps and loads store snapshots to/from disk with payload length checks.
* **Thread Safety**: Thread-safe operations via read-write locking and clean thread lifecycle management
* **HTTP API**: Built using `cpp-httplib` with basic input sanitization and payload limits.

## API Endpoints
* `GET /kv/{key}` - Get a value (404 if missing or expired)
* `PUT /kv/{key}?life={ms}` - Store raw body value (optional TTL in milliseconds)
* `DELETE /kv/{key}` - Delete a key
* `POST /admin/dump?path={file}` - Save cache snapshot to binary file (defaults to `snapshot.bin`)
* `POST /admin/load?path={file}` - Load snapshot from file and wake the cleaner thread

## Build and Run
Requires a C++20 compiler and CMake 3.20+.

```bash
cmake -B build
cmake --build build
```

Run the server (default capacity is 1000 items):

```bash
./build/tinykv
./build/tinykv 5000
```

## Running Tests
Build and run GoogleTest suite:

```bash
ctest --test-dir build --output-on-failure
```