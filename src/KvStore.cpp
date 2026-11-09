#include "KvStore.h"

#include <string_view>
#include <utility>

namespace sparkDB {
    namespace {
        constexpr std::size_t kInitialBuckets = 4;

        std::size_t Hash(const std::string &key) {
            return std::hash<std::string_view>{}(key);
        }
    }

    KvStore::KvStore() : buckets_(kInitialBuckets) {
    }

    KvStore::~KvStore() {
        // Unlink each chain in a loop; recursive destruction through next could overflow the stack.
        for (auto &head: buckets_) {
            while (head) {
                head = std::move(head->next);
            }
        }
    }

    KvStore::KvStore(KvStore &&other) noexcept : KvStore() {
        *this = std::move(other);
    }

    // Swapping hands our old nodes to other, whose destructor frees them without recursion.
    KvStore &KvStore::operator=(KvStore &&other) noexcept {
        std::swap(buckets_, other.buckets_);
        std::swap(size_, other.size_);
        return *this;
    }

    std::unique_ptr<KvStore::Node> *KvStore::FindSlot(const std::size_t hash, const std::string &key) {
        auto *slot = &buckets_[hash & (buckets_.size() - 1)];
        while (*slot && ((*slot)->hash != hash || (*slot)->key != key)) {
            slot = &(*slot)->next;
        }
        return slot;
    }

    void KvStore::Grow() {
        std::vector<std::unique_ptr<Node>> grown(buckets_.size() * 2);
        for (auto &head: buckets_) {
            while (head) {
                auto node = std::move(head);
                head = std::move(node->next);
                auto &target = grown[node->hash & (grown.size() - 1)];
                node->next = std::move(target);
                target = std::move(node);
            }
        }
        buckets_ = std::move(grown);
    }

    std::optional<std::string> KvStore::Get(const std::string &key) const {
        const std::size_t hash = Hash(key);
        for (const Node *node = buckets_[hash & (buckets_.size() - 1)].get(); node; node = node->next.get()) {
            if (node->hash == hash && node->key == key) {
                return node->value;
            }
        }
        return std::nullopt;
    }

    bool KvStore::Set(const std::string &key, const std::string &value) {
        const std::size_t hash = Hash(key);
        if (auto *slot = FindSlot(hash, key); *slot) {
            (*slot)->value = value;
            return false;
        }
        if (size_ == buckets_.size()) {
            Grow();
        }
        auto &head = buckets_[hash & (buckets_.size() - 1)];
        head = std::make_unique<Node>(Node{hash, key, value, std::move(head)});
        ++size_;
        return true;
    }

    bool KvStore::Delete(const std::string &key) {
        auto *slot = FindSlot(Hash(key), key);
        if (!*slot) {
            return false;
        }
        *slot = std::move((*slot)->next);
        --size_;
        return true;
    }

    std::size_t KvStore::Size() const {
        return size_;
    }

    std::size_t KvStore::BucketCount() const {
        return buckets_.size();
    }
}
