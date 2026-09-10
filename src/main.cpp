#include <chrono>
#include <iostream>

#include "kvstore.hpp"

int main() {
    std::cout << "Starting tiny-kvstore demo...\n";
    KVStore store(2);
    store.set("user", "alice");
    store.set("role", "admin");
    if (auto user = store.get("user")) {
        std::cout << "Retrieved key 'user': " << *user << "\n";
    }
    store.set("session", "active");
    std::cout << "Testing LRU eviction:\n";
    std::cout << "  'user' still present: "
              << (store.get("user").has_value() ? "yes" : "no") << "\n";
    std::cout << "  'role' still present (should be evicted): "
              << (store.get("role").has_value() ? "yes" : "no") << "\n";
    const std::string snapshot_file = "snapshot.bin";
    if (store.dump(snapshot_file)) {
        std::cout << "Saved snapshot to " << snapshot_file << "\n";
    }
    std::cout << "Demo completed successfully.\n";
    return 0;
}