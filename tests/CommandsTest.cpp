#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "Commands.h"

using namespace sparkDB;

namespace {
    // Runs a command and returns its display form, to keep tests short.
    std::string Run(KvStore &store, const std::vector<std::string> &args) {
        return ToDisplay(Execute(store, args));
    }
}

TEST_CASE("PING") {
    KvStore store;

    REQUIRE(Run(store, {"PING"}) == "PONG");
    REQUIRE(Run(store, {"PING", "hello"}) == "\"hello\"");
    REQUIRE(Run(store, {"PING", "a", "b"}) == "(error) ERR wrong number of arguments for 'ping' command");
}

TEST_CASE("GET, SET, and DEL") {
    KvStore store;

    REQUIRE(Run(store, {"GET", "a"}) == "(nil)");
    REQUIRE(Run(store, {"SET", "a", "1"}) == "OK");
    REQUIRE(Run(store, {"GET", "a"}) == "\"1\"");
    REQUIRE(Run(store, {"DEL", "a"}) == "(integer) 1");
    REQUIRE(Run(store, {"DEL", "a"}) == "(integer) 0");
    REQUIRE(Run(store, {"GET", "a"}) == "(nil)");
}

TEST_CASE("A stored value that looks like a reply stays distinct") {
    KvStore store;

    Run(store, {"SET", "a", "(nil)"});
    REQUIRE(Execute(store, {"GET", "a"}).type == ResponseType::Bulk);
    REQUIRE(Execute(store, {"GET", "missing"}).type == ResponseType::Null);
}

TEST_CASE("Command names are case-insensitive") {
    KvStore store;

    REQUIRE(Run(store, {"sEt", "a", "1"}) == "OK");
    REQUIRE(Run(store, {"get", "a"}) == "\"1\"");
}

TEST_CASE("Wrong argument counts return an error") {
    KvStore store;

    REQUIRE(Run(store, {"GET"}) == "(error) ERR wrong number of arguments for 'get' command");
    REQUIRE(Run(store, {"SET", "a"}) == "(error) ERR wrong number of arguments for 'set' command");
    REQUIRE(Run(store, {"DEL", "a", "b"}) == "(error) ERR wrong number of arguments for 'del' command");
}

TEST_CASE("Unknown commands return an error") {
    KvStore store;

    REQUIRE(Run(store, {"NOPE"}) == "(error) ERR unknown command 'NOPE'");
}
