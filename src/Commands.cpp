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

        std::string WrongArgCount(const std::string &name) {
            return "ERR wrong number of arguments for '" + name + "' command";
        }
    }

    std::string Execute(KvStore &store, const std::vector<std::string> &args) {
        const std::string name = ToLower(args[0]);

        if (name == "get") {
            if (args.size() != 2) {
                return WrongArgCount(name);
            }
            return store.Get(args[1]).value_or("(nil)");
        }

        if (name == "set") {
            if (args.size() != 3) {
                return WrongArgCount(name);
            }
            store.Set(args[1], args[2]);
            return "OK";
        }

        if (name == "del") {
            if (args.size() != 2) {
                return WrongArgCount(name);
            }
            return store.Delete(args[1]) ? "1" : "0";
        }

        return "ERR unknown command '" + args[0] + "'";
    }
}
