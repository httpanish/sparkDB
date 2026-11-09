#ifndef SPARKDB_KVSTORE_H
#define SPARKDB_KVSTORE_H

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace sparkDB {
    // A chained hash table from string keys to string values.
    class KvStore {
    public:
        KvStore();
        ~KvStore();

        KvStore(const KvStore &) = delete;
        KvStore &operator=(const KvStore &) = delete;
        KvStore(KvStore &&) noexcept = default;
        KvStore &operator=(KvStore &&) noexcept = default;

        std::optional<std::string> Get(const std::string &key) const;

        // Returns true if the key is new, false if an existing value was replaced.
        bool Set(const std::string &key, const std::string &value);

        // Returns true if the key existed.
        bool Delete(const std::string &key);

        std::size_t Size() const;
        std::size_t BucketCount() const;

    private:
        struct Node {
            std::size_t hash; // cached, so resizing never rehashes a key
            std::string key;
            std::string value;
            std::unique_ptr<Node> next;
        };

        // Returns the owner of the node holding key, or of the empty end of its chain.
        std::unique_ptr<Node> *FindSlot(std::size_t hash, const std::string &key);

        // Doubles the bucket count, relinking every node by its cached hash.
        void Grow();

        std::vector<std::unique_ptr<Node>> buckets_; // each bucket is the head of a chain
        std::size_t size_ = 0;
    };
}

#endif //SPARKDB_KVSTORE_H
