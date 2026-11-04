#include "Commands.h"

#include <cctype>

namespace sparkDB {
    namespace {
        std::string ToLower(std::string text) {
            for (char &c : text) {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            return text;
        }

        Response WrongArgCount(const std::string &name) {
            return {ResponseType::Error, "ERR wrong number of arguments for '" + name + "' command"};
        }
    }

    Response Execute(KvStore &store, const std::vector<std::string> &args) {
        const std::string name = ToLower(args[0]);

        if (name == "ping") {
            if (args.size() == 1) {
                return {ResponseType::Simple, "PONG"};
            }
            if (args.size() == 2) {
                return {ResponseType::Bulk, args[1]};
            }
            return WrongArgCount(name);
        }

        if (name == "get") {
            if (args.size() != 2) {
                return WrongArgCount(name);
            }
            const auto value = store.Get(args[1]);
            if (!value) {
                return {ResponseType::Null, ""};
            }
            return {ResponseType::Bulk, *value};
        }

        if (name == "set") {
            if (args.size() != 3) {
                return WrongArgCount(name);
            }
            store.Set(args[1], args[2]);
            return {ResponseType::Simple, "OK"};
        }

        if (name == "del") {
            if (args.size() != 2) {
                return WrongArgCount(name);
            }
            return {ResponseType::Integer, store.Delete(args[1]) ? "1" : "0"};
        }

        return {ResponseType::Error, "ERR unknown command '" + args[0] + "'"};
    }
}
