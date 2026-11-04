#include "Tokenizer.h"

#include <utility>

namespace sparkDB {
    namespace {
        bool IsSpace(const char c) {
            return c == ' ' || c == '\t' || c == '\r' || c == '\n';
        }

        char Unescape(const char c) {
            switch (c) {
                case 'n': return '\n';
                case 't': return '\t';
                case 'r': return '\r';
                default: return c;
            }
        }
    }

    std::optional<std::vector<std::string>> Tokenize(const std::string_view line) {
        std::vector<std::string> tokens;
        std::size_t i = 0;

        while (i < line.size()) {
            if (IsSpace(line[i])) {
                ++i;
                continue;
            }

            std::string token;

            if (line[i] == '"') {
                ++i;
                while (i < line.size() && line[i] != '"') {
                    if (line[i] == '\\' && i + 1 < line.size()) {
                        ++i;
                        token += Unescape(line[i]);
                    } else {
                        token += line[i];
                    }
                    ++i;
                }
                if (i == line.size()) {
                    return std::nullopt; // unclosed quote
                }
                ++i; // closing quote
                if (i < line.size() && !IsSpace(line[i])) {
                    return std::nullopt; // text straight after a closing quote
                }
            } else {
                while (i < line.size() && !IsSpace(line[i])) {
                    token += line[i];
                    ++i;
                }
            }

            tokens.push_back(std::move(token));
        }

        return tokens;
    }
}
