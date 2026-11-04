#include "RespParser.h"

#include <charconv>
#include <optional>
#include <utility>

namespace sparkDB {
    namespace {
        constexpr std::size_t kMaxArgs = 1024;
        constexpr std::size_t kMaxArgSize = 1024 * 1024;
        constexpr std::size_t kMaxLengthLine = 32;

        ParseResult Incomplete() {
            return {ParseStatus::Incomplete, {}, 0, ""};
        }

        ParseResult Error(const std::string &reason) {
            return {ParseStatus::Error, {}, 0, "ERR Protocol error: " + reason};
        }

        // Reads "<prefix><digits>\r\n" at pos into length and moves pos past it.
        // Returns nullopt on success, otherwise the result the parser should return.
        std::optional<ParseResult> ReadLength(std::string_view buffer, char prefix, std::size_t max, std::size_t &pos,
                                              std::size_t &length) {
            if (pos >= buffer.size()) {
                return Incomplete();
            }
            if (buffer[pos] != prefix) {
                return Error(std::string("expected '") + prefix + "'");
            }

            const std::string_view rest = buffer.substr(pos, kMaxLengthLine);
            const std::size_t lineEnd = rest.find("\r\n");
            if (lineEnd == std::string_view::npos) {
                return rest.size() < kMaxLengthLine ? Incomplete() : Error("invalid length");
            }

            // Unsigned from_chars rejects signs and empty input.
            const char *last = rest.data() + lineEnd;
            const auto [end, ec] = std::from_chars(rest.data() + 1, last, length);
            if (ec != std::errc{} || end != last) {
                return Error("invalid length");
            }
            if (length > max) {
                return Error("length too large");
            }

            pos += lineEnd + 2;
            return std::nullopt;
        }
    }

    ParseResult ParseRequest(std::string_view buffer) {
        std::size_t pos = 0;
        std::size_t count = 0;
        if (auto result = ReadLength(buffer, '*', kMaxArgs, pos, count)) {
            return *result;
        }

        std::vector<std::string> args;
        for (std::size_t i = 0; i < count; ++i) {
            std::size_t size = 0;
            if (auto result = ReadLength(buffer, '$', kMaxArgSize, pos, size)) {
                return *result;
            }

            // The data itself is never inspected, so it may contain \r\n or \0.
            if (buffer.size() < pos + size + 2) {
                return Incomplete();
            }
            if (buffer.substr(pos + size, 2) != "\r\n") {
                return Error("expected CRLF");
            }

            args.emplace_back(buffer.substr(pos, size));
            pos += size + 2;
        }

        return {ParseStatus::Complete, std::move(args), pos, ""};
    }
}
