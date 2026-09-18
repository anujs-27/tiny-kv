#include "kvstore.hpp"

#include <utility>

static constexpr size_t MAX_KEY_LEN = 64 * 1024;
static constexpr size_t MAX_VAL_LEN = 16 * 1024 * 1024;

void KVStore::set(std::string key, std::string value,
                  std::optional<std::chrono::milliseconds> life) {
    {
        std::unique_lock<std::shared_mutex> guard(rw_mutex_);
        set_unlocked(std::move(key), std::move(value), life);
    }
    cv_.notify_one();
}

std::optional<std::string> KVStore::get(const std::string& key) {
    std::unique_lock<std::shared_mutex> guard(rw_mutex_);
    auto iter = store_.find(key);
    if (iter == store_.end()) return std::nullopt;

    if (iter->second.expires_at.has_value()) {
        if (std::chrono::steady_clock::now() >=
            iter->second.expires_at.value()) {
            earliest_expiry_.erase({iter->second.expires_at.value(), key});
            recently_used_.erase(iter->second.cache_map);
            store_.erase(iter);
            return std::nullopt;
        }
    }

    recently_used_.splice(recently_used_.begin(), recently_used_,
                          iter->second.cache_map);
    return iter->second.value;
}

bool KVStore::del(const std::string& key) {
    std::unique_lock<std::shared_mutex> guard(rw_mutex_);
    auto iter = store_.find(key);
    if (iter == store_.end()) {
        return false;
    }
    if (iter->second.expires_at.has_value()) {
        earliest_expiry_.erase({iter->second.expires_at.value(), key});
    }
    recently_used_.erase(iter->second.cache_map);
    store_.erase(iter);
    return true;
}

size_t KVStore::cleanup_expired() {
    std::unique_lock<std::shared_mutex> guard(rw_mutex_);
    size_t deleted = 0;
    auto now = std::chrono::steady_clock::now();
    while (!earliest_expiry_.empty() && earliest_expiry_.begin()->first <= now) {
        auto [expiry, key] = *earliest_expiry_.begin();
        earliest_expiry_.erase(earliest_expiry_.begin());  // O(1) amortized
        auto iter = store_.find(key);
        if (iter != store_.end()) {
            recently_used_.erase(iter->second.cache_map);
            store_.erase(iter);
            ++deleted;
        }
    }
    return deleted;
}

bool KVStore::dump(const std::string& filepath) const {
    std::shared_lock<std::shared_mutex> guard(rw_mutex_);
    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    auto now = std::chrono::steady_clock::now();
    for (const auto& [key, entry] : store_) {
        if (entry.expires_at.has_value() && entry.expires_at.value() <= now) {
            continue;
        }

        size_t key_len = key.size();
        file.write(reinterpret_cast<const char*>(&key_len), sizeof(key_len));
        file.write(key.data(), static_cast<std::streamsize>(key_len));

        size_t val_len = entry.value.size();
        file.write(reinterpret_cast<const char*>(&val_len), sizeof(val_len));
        file.write(entry.value.data(), static_cast<std::streamsize>(val_len));
    }
    return file.good();
}

bool KVStore::load(const std::string& filepath) {
    std::unique_lock<std::shared_mutex> guard(rw_mutex_);
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) return false;
    std::vector<std::pair<std::string, std::string>> staged_entries;
    while (file.peek() != EOF) {
        size_t key_len = 0;
        file.read(reinterpret_cast<char*>(&key_len), sizeof(key_len));
        if (!file) return false;
        if (key_len == 0 || key_len > MAX_KEY_LEN) {
            return false;
        }
        std::string key;
        key.resize(key_len);
        file.read(key.data(), static_cast<std::streamsize>(key_len));
        if (!file) return false;
        size_t val_len = 0;
        file.read(reinterpret_cast<char*>(&val_len), sizeof(val_len));
        if (!file) return false;
        if (val_len > MAX_VAL_LEN) {
            return false;
        }
        std::string value;
        value.resize(val_len);
        file.read(value.data(), static_cast<std::streamsize>(val_len));
        if (!file) return false;
        staged_entries.emplace_back(std::move(key), std::move(value));
    }
    earliest_expiry_.clear();
    store_.clear();
    recently_used_.clear();
    for (auto& [k, v] : staged_entries) {
        set_unlocked(std::move(k), std::move(v), std::nullopt);
    }
    return true;
}

void KVStore::set_unlocked(std::string key, std::string value,
                           std::optional<std::chrono::milliseconds> life) {
    if (cap_ == 0) return;

    auto iter = store_.find(key);
    auto expires_at =
        life.has_value()
            ? std::optional(std::chrono::steady_clock::now() + life.value())
            : std::nullopt;

    if (iter != store_.end()) {
        if (iter->second.expires_at.has_value()) {
            earliest_expiry_.erase({iter->second.expires_at.value(), key});
        }

        iter->second.value = std::move(value);
        iter->second.expires_at = expires_at;
        recently_used_.splice(recently_used_.begin(), recently_used_,
                              iter->second.cache_map);

        if (expires_at.has_value()) {
            earliest_expiry_.insert({expires_at.value(), key});
        }
        return;
    }

    if (store_.size() >= cap_) {
        if (!recently_used_.empty()) {
            const std::string lru_key = recently_used_.back();
            auto iter = store_.find(lru_key);
            if (iter != store_.end() && iter->second.expires_at.has_value()) {
                earliest_expiry_.erase({iter->second.expires_at.value(), lru_key});
            }
            store_.erase(lru_key);
            recently_used_.pop_back();
        }
    }

    recently_used_.push_front(key);
    if (expires_at.has_value()) {
        earliest_expiry_.insert({expires_at.value(), key});
    }
    store_[std::move(key)] = Entry{.value = std::move(value),
                                   .expires_at = expires_at,
                                   .cache_map = recently_used_.begin()};
}

KVStore::~KVStore() {
    sweeper_thread_.request_stop();
    cv_.notify_all();
}

void KVStore::sweeper_loop(std::stop_token stop_token) {
    while (!stop_token.stop_requested()) {
        std::unique_lock<std::shared_mutex> lock(rw_mutex_);
        auto now = std::chrono::steady_clock::now();
        while (!earliest_expiry_.empty() && earliest_expiry_.begin()->first <= now) {
            auto [expiry, key] = *earliest_expiry_.begin();
            earliest_expiry_.erase(earliest_expiry_.begin());
            auto iter = store_.find(key);
            if (iter != store_.end()) {
                recently_used_.erase(iter->second.cache_map);
                store_.erase(iter);
            }
        }

        if (earliest_expiry_.empty()) {
            cv_.wait(lock, [&]() {
                return stop_token.stop_requested() || !earliest_expiry_.empty();
            });
        } else {
            auto next_deadline = earliest_expiry_.begin()->first;
            cv_.wait_until(lock, next_deadline, [&]() {
                if (stop_token.stop_requested() || earliest_expiry_.empty()) {
                    return true;
                }
                return std::chrono::steady_clock::now() >= next_deadline ||
                       earliest_expiry_.begin()->first < next_deadline;
            });
        }
    }
}