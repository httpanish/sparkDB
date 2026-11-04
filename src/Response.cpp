#include "Response.h"

namespace sparkDB {
    namespace {
        // Simple strings and errors end at the first line break, so they can't contain one.
        std::string RemoveLineBreaks(std::string text) {
            for (char &c : text) {
                if (c == '\r' || c == '\n') {
                    c = ' ';
                }
            }
            return text;
        }
    }

    std::string ToDisplay(const Response &response) {
        switch (response.type) {
            case ResponseType::Simple:
                return response.text;
            case ResponseType::Error:
                return "(error) " + response.text;
            case ResponseType::Integer:
                return "(integer) " + response.text;
            case ResponseType::Bulk:
                return "\"" + response.text + "\"";
            case ResponseType::Null:
                return "(nil)";
        }
        return "";
    }

    std::string ToResp(const Response &response) {
        switch (response.type) {
            case ResponseType::Simple:
                return "+" + RemoveLineBreaks(response.text) + "\r\n";
            case ResponseType::Error:
                return "-" + RemoveLineBreaks(response.text) + "\r\n";
            case ResponseType::Integer:
                return ":" + response.text + "\r\n";
            case ResponseType::Bulk:
                return "$" + std::to_string(response.text.size()) + "\r\n" + response.text + "\r\n";
            case ResponseType::Null:
                return "$-1\r\n";
        }
        return "";
    }
}
