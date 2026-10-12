// Unit tests - LRU cache: budget, eviction, generation invalidation, stats.

#include <catch2/catch.hpp>

#include <memory>
#include <string>
#include <vector>

#include "veltrix/cache/LruCache.h"

using namespace veltrix;

namespace {
struct Blob {
    std::string payload;
};
std::uint64_t weigh(const Blob& b) { return b.payload.size(); }
} // namespace

TEST_CASE("Put/get round-trip and hit/miss accounting", "[cache]") {
    cache::LruCache<Blob> cache(1000, core::CacheEvictionPolicy::Lru, &weigh);
    auto blob = std::make_shared<Blob>();
    blob->payload = "hello";
    REQUIRE(cache.put("k", std::move(blob), 1));
    auto got = cache.get("k", 1);
    REQUIRE(got != nullptr);
    REQUIRE(got->payload == "hello");
    REQUIRE(cache.get("k", 1) != nullptr);
    REQUIRE(cache.get("missing", 1) == nullptr);
    const auto s = cache.stats();
    REQUIRE(s.hits == 2);
    REQUIRE(s.misses == 1);
    REQUIRE(s.current_bytes == 5);
    REQUIRE(s.entries == 1);
}

TEST_CASE("Budget enforcement evicts in LRU order", "[cache]") {
    cache::LruCache<Blob> cache(10, core::CacheEvictionPolicy::Lru, &weigh);
    auto a = std::make_shared<Blob>();
    a->payload = "aaaaa"; // 5
    auto b = std::make_shared<Blob>();
    b->payload = "bbbbb"; // 5
    REQUIRE(cache.put("a", std::move(a), 1));
    REQUIRE(cache.put("b", std::move(b), 1));
    REQUIRE(cache.stats().entries == 2);
    // Touch "a" so LRU order becomes b, a.
    REQUIRE(cache.get("a", 1) != nullptr);
    auto c = std::make_shared<Blob>();
    c->payload = "ccccccc"; // 7 -> must evict both b and a (5+5+7 > 10)
    REQUIRE(cache.put("c", std::move(c), 1));
    REQUIRE(cache.get("a", 1) == nullptr);
    REQUIRE(cache.get("b", 1) == nullptr);
    REQUIRE(cache.get("c", 1) != nullptr);
    REQUIRE(cache.stats().evictions == 2);
}

TEST_CASE("Entry larger than budget is refused", "[cache]") {
    cache::LruCache<Blob> cache(4, core::CacheEvictionPolicy::Lru, &weigh);
    auto big = std::make_shared<Blob>();
    big->payload = "toolarge"; // 8 > 4
    REQUIRE_FALSE(cache.put("big", std::move(big), 1));
    REQUIRE(cache.stats().entries == 0);
}

TEST_CASE("Generation mismatch invalidates entries", "[cache]") {
    cache::LruCache<Blob> cache(100, core::CacheEvictionPolicy::Lru, &weigh);
    auto blob = std::make_shared<Blob>();
    blob->payload = "x";
    REQUIRE(cache.put("k", blob, 1));
    // Same generation: hit.
    REQUIRE(cache.get("k", 1) != nullptr);
    // Timeline edit bumps the generation: stale entries disappear.
    REQUIRE(cache.get("k", 2) == nullptr);
    REQUIRE(cache.stats().entries == 0);
}

TEST_CASE("FIFO policy does not promote on get", "[cache]") {
    cache::LruCache<Blob> cache(15, core::CacheEvictionPolicy::Fifo, &weigh);
    auto a = std::make_shared<Blob>();
    a->payload = "aaaaa"; // 5
    auto b = std::make_shared<Blob>();
    b->payload = "bbbbb"; // 5
    REQUIRE(cache.put("a", std::move(a), 1));
    REQUIRE(cache.put("b", std::move(b), 1));
    REQUIRE(cache.get("a", 1) != nullptr); // would promote under LRU
    auto c = std::make_shared<Blob>();
    c->payload = "cccccccc"; // 8; 5+5+8=18 > 15 -> evicts oldest ("a")
    REQUIRE(cache.put("c", std::move(c), 1));
    REQUIRE(cache.get("a", 1) == nullptr);
    REQUIRE(cache.get("b", 1) != nullptr);
}

TEST_CASE("Evicted entries stay alive through shared ownership", "[cache]") {
    cache::LruCache<Blob> cache(10, core::CacheEvictionPolicy::Lru, &weigh);
    auto held = std::make_shared<const Blob>();
    {
        auto blob = std::make_shared<Blob>();
        blob->payload = "keep"; // 4 bytes
        const std::shared_ptr<const Blob> as_const = blob;
        held = as_const;
        REQUIRE(cache.put("k", as_const, 1));
    }
    auto other = std::make_shared<Blob>();
    other->payload = "evictit-"; // 8 bytes; 4+8=12 > 10 evicts "k"
    REQUIRE(cache.put("o", std::move(other), 1));
    REQUIRE(cache.get("k", 1) == nullptr);
    REQUIRE(held->payload == "keep"); // still alive via shared ownership
}

TEST_CASE("Clear empties the cache", "[cache]") {
    cache::LruCache<Blob> cache(100, core::CacheEvictionPolicy::Lru, &weigh);
    auto blob = std::make_shared<Blob>();
    blob->payload = "z";
    cache.put("k", std::move(blob), 1);
    cache.clear();
    REQUIRE(cache.stats().entries == 0);
    REQUIRE(cache.stats().current_bytes == 0);
}
