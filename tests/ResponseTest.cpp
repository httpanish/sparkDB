#include <catch2/catch_test_macros.hpp>

#include <string>

#include "Response.h"

using namespace sparkDB;

TEST_CASE("ToDisplay formats each reply kind") {
    REQUIRE(ToDisplay({ResponseType::Simple, "OK"}) == "OK");
    REQUIRE(ToDisplay({ResponseType::Error, "ERR bad"}) == "(error) ERR bad");
    REQUIRE(ToDisplay({ResponseType::Integer, "1"}) == "(integer) 1");
    REQUIRE(ToDisplay({ResponseType::Bulk, "hi"}) == "\"hi\"");
    REQUIRE(ToDisplay({ResponseType::Null, ""}) == "(nil)");
}

TEST_CASE("ToResp encodes each reply kind") {
    REQUIRE(ToResp({ResponseType::Simple, "OK"}) == "+OK\r\n");
    REQUIRE(ToResp({ResponseType::Error, "ERR bad"}) == "-ERR bad\r\n");
    REQUIRE(ToResp({ResponseType::Integer, "1"}) == ":1\r\n");
    REQUIRE(ToResp({ResponseType::Bulk, "hi"}) == "$2\r\nhi\r\n");
    REQUIRE(ToResp({ResponseType::Null, ""}) == "$-1\r\n");
}

TEST_CASE("ToResp sends an empty bulk string, which differs from null") {
    REQUIRE(ToResp({ResponseType::Bulk, ""}) == "$0\r\n\r\n");
}

TEST_CASE("ToResp keeps any byte in a bulk string") {
    const std::string data("a\r\n\0b", 5);
    REQUIRE(ToResp({ResponseType::Bulk, data}) == "$5\r\n" + data + "\r\n");
}

TEST_CASE("ToResp removes line breaks from errors so they can't fake a reply") {
    REQUIRE(ToResp({ResponseType::Error, "ERR a\r\n+OK"}) == "-ERR a  +OK\r\n");
}
