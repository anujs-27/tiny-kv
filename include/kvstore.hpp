#pragma once
#include <chrono>
#include <fstream>
#include <list>
#include <optional>
#include <string>
#include <unordered_map>

class KVStore {
   public:
    void set(std::string key, std::string value,
             std::optional<std::chrono::milliseconds> life = std::nullopt);
    [[nodiscard]] std::optional<std::string> get(const std::string& key);
    bool del(const std::string& key);
    size_t cleanup_expired();
    bool dump(const std::string& filepath);
    bool load(const std::string& filepath);
    KVStore(size_t capacity) : cap_(capacity) {};

   private:
    struct Entry {
        std::string value;
        std::optional<std::chrono::steady_clock::time_point> expires_at;
        std::list<std::string>::iterator cache_map;
    };
    std::unordered_map<std::string, Entry> store_;
    size_t cap_;
    std::list<std::string> recently_used_;
};