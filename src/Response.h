#ifndef SPARKDB_RESPONSE_H
#define SPARKDB_RESPONSE_H

#include <string>

namespace sparkDB {
    enum class ResponseType {
        Simple, // short status, such as OK
        Error,
        Integer,
        Bulk, // a stored value, which may contain any byte
        Null,
    };

    struct Response {
        ResponseType type;
        std::string text; // integers are stored as text; empty for Null
    };

    // Formats a reply for the REPL, for example (integer) 1
    std::string ToDisplay(const Response &response);

    // Encodes a reply as RESP2, the bytes sent to a client, for example :1\r\n
    std::string ToResp(const Response &response);
}

#endif //SPARKDB_RESPONSE_H
