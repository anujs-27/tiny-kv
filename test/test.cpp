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

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}