#include <catch2/catch_test_macros.hpp>

#include "Commands.h"

using sparkDB::Execute;
using sparkDB::KvStore;

TEST_CASE("GET, SET, and DEL") {
    KvStore store;

    REQUIRE(Execute(store, {"GET", "a"}) == "(nil)");
    REQUIRE(Execute(store, {"SET", "a", "1"}) == "OK");
    REQUIRE(Execute(store, {"GET", "a"}) == "1");
    REQUIRE(Execute(store, {"DEL", "a"}) == "1");
    REQUIRE(Execute(store, {"DEL", "a"}) == "0");
    REQUIRE(Execute(store, {"GET", "a"}) == "(nil)");
}

TEST_CASE("Command names are case-insensitive") {
    KvStore store;

    REQUIRE(Execute(store, {"sEt", "a", "1"}) == "OK");
    REQUIRE(Execute(store, {"get", "a"}) == "1");
}

TEST_CASE("Wrong argument counts return an error") {
    KvStore store;

    REQUIRE(Execute(store, {"GET"}) == "ERR wrong number of arguments for 'get' command");
    REQUIRE(Execute(store, {"SET", "a"}) == "ERR wrong number of arguments for 'set' command");
    REQUIRE(Execute(store, {"DEL", "a", "b"}) == "ERR wrong number of arguments for 'del' command");
}

TEST_CASE("Unknown commands return an error") {
    KvStore store;

    REQUIRE(Execute(store, {"NOPE"}) == "ERR unknown command 'NOPE'");
}
