// Veltrix Cache - LruCache.h
//
// Thread-safe LRU cache with a byte budget and stable string keys. The
// cache stores shared_ptrs so in-flight users keep their entry alive even
// after eviction (safe release). Two policies: LRU and FIFO
// (veltrix::core::CacheEvictionPolicy).
//
// Invariants:
//   * total_bytes_ <= budget_bytes_ after every operation;
//   * an entry handed out (shared_ptr) stays valid regardless of eviction;
//   * statistics are exact (hit/miss/eviction counters).

#pragma once

#include <cstdint>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

#include "veltrix/core/AppConfig.h"

namespace veltrix::cache {

struct CacheStats {
    std::uint64_t hits = 0;
    std::uint64_t misses = 0;
    std::uint64_t evictions = 0;
    std::uint64_t current_bytes = 0;
    std::uint64_t budget_bytes = 0;
    std::size_t entries = 0;
};

template <typename T>
class LruCache {
public:
    using Weigher = std::function<std::uint64_t(const T&)>;

    LruCache(std::uint64_t budget_bytes, veltrix::core::CacheEvictionPolicy policy,
             Weigher weigher)
        : budget_(budget_bytes), policy_(policy), weigher_(std::move(weigher)) {}

    // Looks up a key; on hit, promotes it (LRU). Returns the shared entry
    // or null. `generation` keys invalidation (timeline edits bump it).
    std::shared_ptr<const T> get(const std::string& key, std::uint64_t generation) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = map_.find(key);
        if (it == map_.end()) { ++stats_.misses; return nullptr; }
        Entry& entry = it->second;
        if (entry.generation != generation) {
            // stale by timeline edit: drop
            total_bytes_ -= entry.weight;
            order_.erase(entry.order_it);
            map_.erase(it);
            ++stats_.misses;
            return nullptr;
        }
        ++stats_.hits;
        if (policy_ == veltrix::core::CacheEvictionPolicy::Lru)
            promote(entry);
        return entry.value;
    }

    // Inserts/replaces a key. Evicts (LRU/FIFO) until the new entry fits.
    // Returns false (and does not store) when the entry alone exceeds the
    // budget. New entries go to the back of the eviction order.
    bool put(const std::string& key, std::shared_ptr<const T> value,
             std::uint64_t generation) {
        if (!value) return false;
        const std::uint64_t weight = weigher_(*value);
        if (weight > budget_) return false;
        std::lock_guard<std::mutex> lock(mutex_);
        erase_key_locked(key);
        total_bytes_ += weight;
        order_.push_back(key);
        map_[key] = Entry{std::move(value), weight, generation, std::prev(order_.end())};
        evict_over_budget_locked();
        return true;
    }

    void invalidate(const std::string& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        erase_key_locked(key);
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        map_.clear();
        order_.clear();
        total_bytes_ = 0;
    }

    CacheStats stats() const {
        std::lock_guard<std::mutex> lock(mutex_);
        CacheStats s = stats_;
        s.current_bytes = total_bytes_;
        s.budget_bytes = budget_;
        s.entries = map_.size();
        return s;
    }

private:
    struct Entry {
        std::shared_ptr<const T> value;
        std::uint64_t weight;
        std::uint64_t generation;
        std::list<std::string>::iterator order_it;
    };

    void promote(Entry& entry) {
        order_.splice(order_.end(), order_, entry.order_it);
        entry.order_it = std::prev(order_.end());
    }

    void erase_key_locked(const std::string& key) {
        auto it = map_.find(key);
        if (it == map_.end()) return;
        total_bytes_ -= it->second.weight;
        order_.erase(it->second.order_it);
        map_.erase(it);
    }

    void evict_over_budget_locked() {
        while (total_bytes_ > budget_ && !order_.empty()) {
            const std::string victim = order_.front();
            auto it = map_.find(victim);
            if (it != map_.end()) {
                total_bytes_ -= it->second.weight;
                map_.erase(it);
            }
            order_.pop_front();
            ++stats_.evictions;
        }
    }

    mutable std::mutex mutex_;
    std::uint64_t budget_;
    veltrix::core::CacheEvictionPolicy policy_;
    Weigher weigher_;
    std::unordered_map<std::string, Entry> map_;
    std::list<std::string> order_; // front = evict first
    std::uint64_t total_bytes_ = 0;
    mutable CacheStats stats_;
};

} // namespace veltrix::cache
