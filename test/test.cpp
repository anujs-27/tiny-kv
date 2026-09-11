#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <thread>

#include "kvstore.hpp"

TEST(KVStoreTest, GetAndSet) {
    KVStore db(5);
    db.set("key1", "value1");
    db.set("key2", "value2");
    EXPECT_EQ(db.get("key1"), "value1");
    EXPECT_EQ(db.get("key2"), "value2");
}

TEST(KVStoreTest, TTL) {
    KVStore db(5);
    db.set("key1", "value1");
    db.set("key2", "value2", std::chrono::milliseconds(50));
    std::this_thread::sleep_for(std::chrono::milliseconds(51));
    EXPECT_EQ(db.get("key1"), "value1");
    EXPECT_FALSE(db.get("key2").has_value());
}

TEST(KVStoreTest, LRU) {
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

TEST(KVStoreTest, DumpAndLoad) {
    const std::string filename = "store.bin";
    std::filesystem::remove(filename);
    {
        KVStore db(5);
        db.set("token", "c2VjcmV0XzEyMw==", std::chrono::milliseconds(50));
        db.set("name", "Alice");
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
        EXPECT_TRUE(db.dump(filename));
    }
    {
        KVStore db(5);
        ASSERT_TRUE(db.load(filename));
        EXPECT_FALSE(db.get("token").has_value());
        ASSERT_TRUE(db.get("name").has_value());
        EXPECT_EQ(*db.get("name"), "Alice");
        EXPECT_FALSE(db.get("nonexistent").has_value());
    }
}

TEST(KVStoreTest, ConcurrentReadWriteStress) {
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
                    auto val = db.get(key);
                    if (val.has_value()) {
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

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}