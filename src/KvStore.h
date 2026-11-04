#ifndef SPARKDB_KVSTORE_H
#define SPARKDB_KVSTORE_H

#include <optional>
#include <string>
#include <unordered_map>

namespace sparkDB {
    class KvStore {
    public:
        std::optional<std::string> Get(const std::string &key) const;

        // Returns true if the key is new, false if an existing value was replaced.
        bool Set(const std::string &key, const std::string &value);

        // Returns true if the key existed.
        bool Delete(const std::string &key);

    private:
        std::unordered_map<std::string, std::string> storage_;
    };
}

#endif //SPARKDB_KVSTORE_H
