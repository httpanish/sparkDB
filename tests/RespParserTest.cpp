#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "RespParser.h"

using sparkDB::ParseRequest;
using sparkDB::ParseStatus;
using Args = std::vector<std::string>;
using namespace std::string_literals;

namespace {
    void RequireError(const std::string &input, const std::string &reason) {
        const auto result = ParseRequest(input);
        REQUIRE(result.status == ParseStatus::Error);
        REQUIRE(result.error == "ERR Protocol error: " + reason);
    }

    std::string Bulk(const std::string &value) {
        return "$" + std::to_string(value.size()) + "\r\n" + value + "\r\n";
    }
}

TEST_CASE("ParseRequest parses a complete command") {
    const std::string input = "*2\r\n$3\r\nGET\r\n$3\r\nfoo\r\n";
    const auto result = ParseRequest(input);
    REQUIRE(result.status == ParseStatus::Complete);
    REQUIRE(result.args == Args{"GET", "foo"});
    REQUIRE(result.consumed == input.size());
}

TEST_CASE("ParseRequest returns Incomplete for every prefix of a command") {
    const std::string input = "*2\r\n$3\r\nGET\r\n$3\r\nfoo\r\n";
    for (std::size_t size = 0; size < input.size(); ++size) {
        REQUIRE(ParseRequest(input.substr(0, size)).status == ParseStatus::Incomplete);
    }
}

TEST_CASE("ParseRequest parses pipelined commands one at a time") {
    const std::string first = "*1\r\n$4\r\nPING\r\n";
    const std::string second = "*2\r\n$3\r\nGET\r\n$1\r\na\r\n";
    const std::string input = first + second;

    const auto result = ParseRequest(input);
    REQUIRE(result.status == ParseStatus::Complete);
    REQUIRE(result.args == Args{"PING"});
    REQUIRE(result.consumed == first.size());

    const auto next = ParseRequest(input.substr(result.consumed));
    REQUIRE(next.status == ParseStatus::Complete);
    REQUIRE(next.args == Args{"GET", "a"});
    REQUIRE(next.consumed == second.size());
}

TEST_CASE("ParseRequest treats arguments as binary-safe") {
    const std::string value = "a\0b\r\nc"s;
    const auto result = ParseRequest("*3\r\n" + Bulk("SET") + Bulk(value) + Bulk(""));
    REQUIRE(result.status == ParseStatus::Complete);
    REQUIRE(result.args == Args{"SET", value, ""});
}

TEST_CASE("ParseRequest consumes an empty array with no arguments") {
    const auto result = ParseRequest("*0\r\n");
    REQUIRE(result.status == ParseStatus::Complete);
    REQUIRE(result.args.empty());
    REQUIRE(result.consumed == 4);
}

TEST_CASE("ParseRequest rejects malformed array headers") {
    RequireError("PING\r\n", "expected '*'");
    RequireError("*abc\r\n", "invalid length");
    RequireError("*\r\n", "invalid length");
    RequireError("*-1\r\n", "invalid length");
    RequireError("*+1\r\n", "invalid length");
    RequireError("* 1\r\n", "invalid length");
    RequireError("*1 \r\n", "invalid length");
    RequireError("*99999999999999999999999\r\n", "invalid length");
}

TEST_CASE("ParseRequest rejects malformed bulk strings") {
    RequireError("*1\r\n+PING\r\n", "expected '$'");
    RequireError("*1\r\n$abc\r\n", "invalid length");
    RequireError("*1\r\n$-1\r\n", "invalid length");
    RequireError("*1\r\n$3\r\nGETxx", "expected CRLF");
    RequireError("*1\r\n$3\r\nGET\rX", "expected CRLF");
}

TEST_CASE("ParseRequest rejects a wrong prefix before the line is complete") {
    RequireError("x", "expected '*'");
    RequireError("*1\r\n+", "expected '$'");
}

TEST_CASE("ParseRequest waits for the whole line or argument before other errors") {
    REQUIRE(ParseRequest("*a").status == ParseStatus::Incomplete);
    REQUIRE(ParseRequest("*1\r\n$a").status == ParseStatus::Incomplete);
    REQUIRE(ParseRequest("*1\r\n$3\r\nGETx").status == ParseStatus::Incomplete);
}

TEST_CASE("ParseRequest limits the length line to 32 bytes") {
    RequireError("*" + std::string(31, '0'), "invalid length");
    RequireError("*" + std::string(40, '0') + "1\r\n", "invalid length");

    // Exactly 32 bytes including the CRLF is allowed.
    const auto result = ParseRequest("*" + std::string(28, '0') + "1\r\n" + Bulk("PING"));
    REQUIRE(result.status == ParseStatus::Complete);
    REQUIRE(result.args == Args{"PING"});
}

TEST_CASE("ParseRequest limits the number of arguments") {
    std::string input = "*1024\r\n";
    for (int i = 0; i < 1024; ++i) {
        input += Bulk("a");
    }
    const auto result = ParseRequest(input);
    REQUIRE(result.status == ParseStatus::Complete);
    REQUIRE(result.args.size() == 1024);

    RequireError("*1025\r\n", "length too large");
}

TEST_CASE("ParseRequest limits the size of an argument") {
    const std::string value(1024 * 1024, 'a');
    const auto result = ParseRequest("*1\r\n" + Bulk(value));
    REQUIRE(result.status == ParseStatus::Complete);
    REQUIRE(result.args == Args{value});

    RequireError("*1\r\n$1048577\r\n", "length too large");
}
