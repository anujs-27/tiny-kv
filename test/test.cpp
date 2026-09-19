#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#include "kvstore.hpp"

using namespace std::chrono_literals;

TEST(KVStore, BasicGetAndSet) {
    KVStore db(5);
    db.set("key1", "value1");
    db.set("key2", "value2");

    EXPECT_EQ(db.get("key1"), "value1");
    EXPECT_EQ(db.get("key2"), "value2");
    EXPECT_FALSE(db.get("key3").has_value());
}

TEST(KVStore, Removal) {
    KVStore db(5);
    db.set("key1", "value1");

    EXPECT_TRUE(db.del("key1"));
    EXPECT_FALSE(db.del("key1"));
    EXPECT_FALSE(db.get("key1").has_value());
}

TEST(KVStore, ExpirationAndTTL) {
    KVStore db(5);
    db.set("key1", "value1");
    db.set("key2", "value2", 50ms);

    std::this_thread::sleep_for(100ms);

    EXPECT_EQ(db.get("key1"), "value1");
    EXPECT_FALSE(db.get("key2").has_value());
}

TEST(KVStore, BackgroundSweeper) {
    KVStore db(5);
    db.set("key1", "value1", 50ms);

    std::this_thread::sleep_for(120ms);

    EXPECT_EQ(db.cleanup_expired(), 0);
    EXPECT_FALSE(db.get("key1").has_value());
}

TEST(KVStore, SweeperRescheduling) {
    KVStore db(5);
    db.set("k_long", "v_long", 5000ms);
    db.set("k_short", "v_short", 60ms);

    std::this_thread::sleep_for(120ms);

    EXPECT_FALSE(db.get("k_short").has_value());
    EXPECT_TRUE(db.get("k_long").has_value());
}

TEST(KVStore, DeleteBeforeWakeup) {
    KVStore db(5);
    db.set("key1", "value1", 200ms);
    EXPECT_TRUE(db.del("key1"));

    std::this_thread::sleep_for(250ms);
    EXPECT_FALSE(db.get("key1").has_value());
}

TEST(KVStore, EvictionPolicyLRU) {
    KVStore db(3);
    db.set("key1", "value1");
    db.set("key2", "value2");
    db.set("key3", "value3");
    db.set("key1", "value1");
    db.set("key4", "value4");

    EXPECT_TRUE(db.get("key1").has_value());
    EXPECT_FALSE(db.get("key2").has_value());
    EXPECT_TRUE(db.get("key3").has_value());
    EXPECT_TRUE(db.get("key4").has_value());
}

TEST(KVStore, LRUSelfSpliceGuard) {
    KVStore db(3);
    db.set("key1", "value1");
    db.set("key2", "value2");

    EXPECT_EQ(db.get("key2"), "value2");
    EXPECT_EQ(db.get("key2"), "value2");
    db.set("key2", "value2_updated");

    db.set("key3", "value3");
    db.set("key4", "value4");

    EXPECT_FALSE(db.get("key1").has_value());
    EXPECT_EQ(db.get("key2"), "value2_updated");
}

TEST(KVStore, ZeroCapacityBehavior) {
    KVStore db(0);
    db.set("key1", "value1");
    EXPECT_FALSE(db.get("key1").has_value());
}

TEST(KVStore, PersistenceDumpAndLoad) {
    const std::filesystem::path filename = "store_test.bin";
    std::filesystem::remove(filename);

    {
        KVStore db(5);
        db.set("token", "c2VjcmV0XzEyMw==", 50ms);
        db.set("name", "Alice");
        std::this_thread::sleep_for(100ms);
        EXPECT_TRUE(db.dump(filename));
    }

    {
        KVStore db(5);
        ASSERT_TRUE(db.load(filename));
        EXPECT_FALSE(db.get("token").has_value());

        auto name = db.get("name");
        ASSERT_TRUE(name.has_value());
        EXPECT_EQ(*name, "Alice");

        EXPECT_FALSE(db.get("nonexistent").has_value());
    }

    std::filesystem::remove(filename);
}

TEST(KVStore, LoadWakesSweeper) {
    const std::filesystem::path filename = "sweeper_load_test.bin";
    std::filesystem::remove(filename);

    {
        KVStore source(5);
        source.set("persisted", "data");
        EXPECT_TRUE(source.dump(filename));
    }

    KVStore target(5);
    target.set("expiring", "val", 5000ms);
    ASSERT_TRUE(target.load(filename));

    EXPECT_FALSE(target.get("expiring").has_value());
    EXPECT_TRUE(target.get("persisted").has_value());
    EXPECT_EQ(target.cleanup_expired(), 0);

    std::filesystem::remove(filename);
}

TEST(KVStore, ConcurrencyStress) {
    constexpr size_t capacity = 50;
    KVStore db(capacity);
    constexpr int num_threads = 8;
    constexpr int ops_per_thread = 500;
    constexpr int key_space = 30;

    std::vector<std::thread> workers;
    workers.reserve(num_threads);

    for (int t = 0; t < num_threads; ++t) {
        workers.emplace_back([&db, t]() {
            for (int i = 0; i < ops_per_thread; ++i) {
                std::string key = "key_" + std::to_string((t + i) % key_space);
                if (i % 3 == 0) {
                    db.set(key, "val_" + std::to_string(i));
                } else if (i % 3 == 1) {
                    if (auto val = db.get(key); val.has_value()) {
                        EXPECT_FALSE(val->empty());
                    }
                } else {
                    db.del(key);
                }
            }
        });
    }

    for (auto& worker : workers) {
        worker.join();
    }

    db.set("canary_key", "canary_val");
    auto res = db.get("canary_key");
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(*res, "canary_val");
}

TEST(KVStore, DestructionUnderLoad) {
    constexpr size_t capacity = 100;
    std::atomic<bool> keep_running{true};
    auto store = std::make_unique<KVStore>(capacity);

    std::vector<std::jthread> workers;
    for (int i = 0; i < 4; ++i) {
        workers.emplace_back([&store, &keep_running, i]() {
            while (keep_running.load(std::memory_order_relaxed)) {
                store->set("k" + std::to_string(i), "v", 10ms);
                store->get("k" + std::to_string(i));
            }
        });
    }

    std::this_thread::sleep_for(20ms);
    keep_running.store(false, std::memory_order_relaxed);
    workers.clear();

    EXPECT_NO_THROW(store.reset());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}