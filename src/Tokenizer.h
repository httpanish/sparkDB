#ifndef SPARKDB_TOKENIZER_H
#define SPARKDB_TOKENIZER_H

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sparkDB {
    // Splits a line on whitespace. Double-quoted tokens may contain spaces and
    // the escapes \n, \t, \r, \\ and \". Returns nullopt if the quoting is invalid.
    std::optional<std::vector<std::string>> Tokenize(std::string_view line);
}

#endif //SPARKDB_TOKENIZER_H
