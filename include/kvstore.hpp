#pragma once
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <list>
#include <mutex>
#include <optional>
#include <set>
#include <stop_token>
#include <string>
#include <syncstream>
#include <thread>
#include <unordered_map>

class KVStore {
   public:
    KVStore(size_t capacity) : cap_(capacity) {
        sweeper_thread_ = std::jthread([this](std::stop_token st) {
            sweeper_loop(st);
        });
    }
    ~KVStore();

    void set(std::string key, std::string value,
             std::optional<std::chrono::milliseconds> life = std::nullopt);
    [[nodiscard]] std::optional<std::string> get(const std::string& key);
    bool del(const std::string& key);
    size_t cleanup_expired();
    bool dump(const std::string& filepath) const;
    bool load(const std::string& filepath);
    void sweeper_loop(std::stop_token stop_token);

   private:
    struct Entry {
        std::string value;
        std::optional<std::chrono::steady_clock::time_point> expires_at;
        std::list<std::string>::iterator cache_map;
    };

    size_t cap_;
    std::unordered_map<std::string, Entry> store_;
    std::list<std::string> recently_used_;
    std::set<std::pair<std::chrono::steady_clock::time_point, std::string>> earliest_expiry_;
    mutable std::mutex mutex_;
    std::condition_variable_any cv_;
    std::jthread sweeper_thread_;

    void set_unlocked(
        std::string key, std::string value,
        std::optional<std::chrono::milliseconds> life = std::nullopt);
};