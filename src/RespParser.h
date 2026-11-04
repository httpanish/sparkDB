#ifndef SPARKDB_RESPPARSER_H
#define SPARKDB_RESPPARSER_H

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace sparkDB {
    enum class ParseStatus { Complete, Incomplete, Error };

    struct ParseResult {
        ParseStatus status;
        std::vector<std::string> args; // Complete; empty for *0
        std::size_t consumed; // Complete: bytes to remove from the buffer
        std::string error; // Error: full reply text, without the leading '-' or CRLF
    };

    // Parses one RESP2 array of bulk strings from the start of the buffer.
    ParseResult ParseRequest(std::string_view buffer);
}

#endif //SPARKDB_RESPPARSER_H
