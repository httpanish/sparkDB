#include "KvStore.h"

namespace sparkDB {
    std::optional<std::string> KvStore::Get(const std::string &key) const {
        const auto it = storage_.find(key);
        if (it == storage_.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    bool KvStore::Set(const std::string &key, const std::string &value) {
        return storage_.insert_or_assign(key, value).second;
    }

    bool KvStore::Delete(const std::string &key) {
        return storage_.erase(key) > 0;
    }
}
