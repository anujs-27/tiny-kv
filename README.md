# tiny-kv

A simple in-memory key-value store written in C++20. I built this to understand how to implement thread-safe data structures, an LRU cache eviction policy, and basic binary persistence from scratch, backed by a simple HTTP interface.

## Features

* **LRU Eviction**: Evicts least-recently used keys when max capacity is hit.
* **TTL Support**: Optional expiration per key in milliseconds.
* **Binary Persistence**: Dumps and loads the cache to/from disk in binary format.
* **Thread Safe**: Protects internal hash table and list access for concurrent operations.
* **HTTP Interface**: Built with `cpp-httplib`.

## API Endpoints

* `GET /kv/{key}` - Get a value (404 if missing or expired)
* `PUT /kv/{key}?life={ms}` - Store a value from the request body (optional TTL)
* `DELETE /kv/{key}` - Delete a key
* `POST /admin/dump?path={file}` - Save cache state to a binary file (defaults to `snapshot.bin`)
* `POST /admin/load?path={file}` - Load cache state from a binary file

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