#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "Tokenizer.h"

using sparkDB::Tokenize;
using Tokens = std::vector<std::string>;

TEST_CASE("Tokenize splits on whitespace") {
    REQUIRE(Tokenize("SET a b") == Tokens{"SET", "a", "b"});
    REQUIRE(Tokenize("  SET \t a  ") == Tokens{"SET", "a"});
}

TEST_CASE("Tokenize returns no tokens for a blank line") {
    REQUIRE(Tokenize("") == Tokens{});
    REQUIRE(Tokenize("   ") == Tokens{});
}

TEST_CASE("Tokenize handles quoted tokens and escapes") {
    REQUIRE(Tokenize(R"(SET a "b c\n")") == Tokens{"SET", "a", "b c\n"});
    REQUIRE(Tokenize(R"("say \"hi\" \\")") == Tokens{"say \"hi\" \\"});
    REQUIRE(Tokenize(R"("")") == Tokens{""});
}

TEST_CASE("Tokenize rejects invalid quoting") {
    REQUIRE_FALSE(Tokenize(R"(SET a "b)").has_value());
    REQUIRE_FALSE(Tokenize(R"("a"b)").has_value());
    REQUIRE_FALSE(Tokenize(R"("a\)").has_value());
}
