#include <catch2/catch_test_macros.hpp>

#include <random>
#include <string>
#include <unordered_map>

#include "KvStore.h"

using sparkDB::KvStore;

TEST_CASE("Get on a missing key returns nothing") {
    const KvStore store;
    REQUIRE_FALSE(store.Get("missing").has_value());
}

TEST_CASE("Set stores a value and reports whether the key is new") {
    KvStore store;
    REQUIRE(store.Set("key", "first"));
    REQUIRE(store.Get("key") == "first");

    REQUIRE_FALSE(store.Set("key", "second"));
    REQUIRE(store.Get("key") == "second");
}

TEST_CASE("Delete removes a key and reports whether it existed") {
    KvStore store;
    store.Set("key", "value");

    REQUIRE(store.Delete("key"));
    REQUIRE_FALSE(store.Get("key").has_value());
    REQUIRE_FALSE(store.Delete("key"));
}

TEST_CASE("Keys and values may contain null bytes") {
    KvStore store;
    const std::string key("a\0b", 3);
    const std::string value("c\0d", 3);

    store.Set(key, value);

    REQUIRE(store.Get(key) == value);
    REQUIRE_FALSE(store.Get("a").has_value());
}

TEST_CASE("A new store is empty and starts with 4 buckets") {
    const KvStore store;
    REQUIRE(store.Size() == 0);
    REQUIRE(store.BucketCount() == 4);
}

TEST_CASE("Size counts keys") {
    KvStore store;
    store.Set("a", "1");
    store.Set("b", "2");
    REQUIRE(store.Size() == 2);

    store.Set("a", "3");
    REQUIRE(store.Size() == 2);

    REQUIRE(store.Delete("a"));
    REQUIRE(store.Size() == 1);

    REQUIRE_FALSE(store.Delete("missing"));
    REQUIRE(store.Size() == 1);
}

TEST_CASE("The table doubles before the key that would pass load factor 1") {
    KvStore store;
    for (const char *key: {"a", "b", "c", "d"}) {
        store.Set(key, "v");
    }
    REQUIRE(store.BucketCount() == 4);

    store.Set("e", "v");
    REQUIRE(store.BucketCount() == 8);

    for (const char *key: {"a", "b", "c", "d", "e"}) {
        REQUIRE_FALSE(store.Set(key, "new"));
    }
    REQUIRE(store.BucketCount() == 8);
}

TEST_CASE("The table keeps doubling as keys are added") {
    KvStore store;
    for (int i = 0; i < 1000; ++i) {
        REQUIRE(store.Set("key" + std::to_string(i), "value" + std::to_string(i)));
    }
    REQUIRE(store.Size() == 1000);
    REQUIRE(store.BucketCount() == 1024);
    for (int i = 0; i < 1000; ++i) {
        REQUIRE(store.Get("key" + std::to_string(i)) == "value" + std::to_string(i));
    }
}

TEST_CASE("Deleting keys never shrinks the table") {
    KvStore store;
    for (int i = 0; i < 100; ++i) {
        store.Set(std::to_string(i), "v");
    }
    const std::size_t buckets = store.BucketCount();

    for (int i = 0; i < 100; ++i) {
        REQUIRE(store.Delete(std::to_string(i)));
    }
    REQUIRE(store.Size() == 0);
    REQUIRE(store.BucketCount() == buckets);
    for (int i = 0; i < 100; ++i) {
        REQUIRE_FALSE(store.Get(std::to_string(i)).has_value());
    }
}

TEST_CASE("Many keys can be stored and half of them deleted") {
    constexpr int count = 100000;
    KvStore store;
    for (int i = 0; i < count; ++i) {
        store.Set("key" + std::to_string(i), std::to_string(i));
    }
    for (int i = 0; i < count; ++i) {
        REQUIRE(store.Get("key" + std::to_string(i)) == std::to_string(i));
    }

    for (int i = 0; i < count; i += 2) {
        REQUIRE(store.Delete("key" + std::to_string(i)));
    }
    REQUIRE(store.Size() == count / 2);
    for (int i = 0; i < count; ++i) {
        if (i % 2 == 0) {
            REQUIRE_FALSE(store.Get("key" + std::to_string(i)).has_value());
        } else {
            REQUIRE(store.Get("key" + std::to_string(i)) == std::to_string(i));
        }
    }
}

TEST_CASE("The empty key and keys with null bytes are independent") {
    KvStore store;
    const std::string empty;
    const std::string nul("\0", 1);
    const std::string ab("a\0b", 3);
    const std::string ac("a\0c", 3);

    REQUIRE(store.Set(empty, "empty"));
    REQUIRE(store.Set(nul, "nul"));
    REQUIRE(store.Set(ab, "ab"));
    REQUIRE(store.Set(ac, "ac"));
    REQUIRE(store.Size() == 4);

    REQUIRE(store.Get(empty) == "empty");
    REQUIRE(store.Get(nul) == "nul");
    REQUIRE(store.Get(ab) == "ab");
    REQUIRE(store.Get(ac) == "ac");

    REQUIRE(store.Delete(ab));
    REQUIRE_FALSE(store.Get(ab).has_value());
    REQUIRE(store.Get(ac) == "ac");

    REQUIRE(store.Delete(empty));
    REQUIRE_FALSE(store.Get(empty).has_value());
    REQUIRE(store.Get(nul) == "nul");
}

TEST_CASE("Random operations match std::unordered_map") {
    constexpr int keySpace = 500;
    std::mt19937 rng(12345);
    std::uniform_int_distribution<int> pickKey(0, keySpace - 1);
    std::uniform_int_distribution<int> pickOp(0, 2);

    KvStore store;
    std::unordered_map<std::string, std::string> reference;

    for (int i = 0; i < 100000; ++i) {
        const std::string key = "k" + std::to_string(pickKey(rng));
        switch (pickOp(rng)) {
            case 0: {
                const std::string value = std::to_string(i);
                const bool isNew = reference.insert_or_assign(key, value).second;
                REQUIRE(store.Set(key, value) == isNew);
                break;
            }
            case 1: {
                const auto it = reference.find(key);
                const auto expected = it == reference.end()
                                          ? std::nullopt
                                          : std::optional<std::string>(it->second);
                REQUIRE(store.Get(key) == expected);
                break;
            }
            default:
                REQUIRE(store.Delete(key) == (reference.erase(key) > 0));
                break;
        }
    }

    REQUIRE(store.Size() == reference.size());
    for (int k = 0; k < keySpace; ++k) {
        const std::string key = "k" + std::to_string(k);
        const auto it = reference.find(key);
        const auto expected = it == reference.end() ? std::nullopt : std::optional<std::string>(it->second);
        REQUIRE(store.Get(key) == expected);
    }
}
