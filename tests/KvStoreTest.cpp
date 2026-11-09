#include <catch2/catch_test_macros.hpp>

#include <string>

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
