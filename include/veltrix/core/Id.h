// Veltrix Core - Id.h
//
// Strongly typed identifiers. Ids are 64-bit opaque values; they are
// serialized with the objects they identify, so uniqueness must hold across
// sessions, not only inside one process. Generation mixes a process random
// seed with a monotonic counter, which makes collisions practically
// impossible without depending on platform GUIDs.
//
// Invariant: a default-constructed id is "null"; is_null() is the only
// supported way to test for it.

#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <random>
#include <string>

namespace veltrix::core {

namespace detail {
inline std::uint64_t id_seed() {
    static const std::uint64_t seed = [] {
        std::random_device rd;
        const std::uint32_t r1 = rd(), r2 = rd(), r3 = rd(), r4 = rd();
        const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        const std::uint32_t extra = static_cast<std::uint32_t>(
            static_cast<std::uint64_t>(now) >> 16);
        std::seed_seq seq{r1, r2, r3, r4, extra};
        std::uint64_t value = 0;
        seq.generate(reinterpret_cast<std::uint32_t*>(&value),
                     reinterpret_cast<std::uint32_t*>(&value) + 2);
        return value | 1; // never zero
    }();
    return seed;
}

inline std::uint64_t next_id() {
    static std::atomic<std::uint64_t> counter{1};
    const std::uint64_t n = counter.fetch_add(1, std::memory_order_relaxed);
    return (id_seed() << 1) ^ n ^ (n << 29);
}
} // namespace detail

#define VELTRIX_DEFINE_ID(Name)                                        \
    struct Name {                                                      \
        std::uint64_t value = 0;                                       \
        bool is_null() const { return value == 0; }                   \
        static Name generate() { return Name{detail::next_id()}; }     \
        std::string to_string() const { return std::to_string(value); } \
    };                                                                 \
    inline bool operator==(Name a, Name b) { return a.value == b.value; } \
    inline bool operator!=(Name a, Name b) { return !(a == b); }       \
    inline bool operator<(Name a, Name b) { return a.value < b.value; }

VELTRIX_DEFINE_ID(MediaId)
VELTRIX_DEFINE_ID(SequenceId)
VELTRIX_DEFINE_ID(TrackId)
VELTRIX_DEFINE_ID(ClipId)
VELTRIX_DEFINE_ID(PluginId)
VELTRIX_DEFINE_ID(JobId)
VELTRIX_DEFINE_ID(ProjectId)

#undef VELTRIX_DEFINE_ID

} // namespace veltrix::core

namespace std {
template <> struct hash<veltrix::core::MediaId> {
    size_t operator()(const veltrix::core::MediaId& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.value);
    }
};
template <> struct hash<veltrix::core::SequenceId> {
    size_t operator()(const veltrix::core::SequenceId& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.value);
    }
};
template <> struct hash<veltrix::core::TrackId> {
    size_t operator()(const veltrix::core::TrackId& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.value);
    }
};
template <> struct hash<veltrix::core::ClipId> {
    size_t operator()(const veltrix::core::ClipId& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.value);
    }
};
template <> struct hash<veltrix::core::ProjectId> {
    size_t operator()(const veltrix::core::ProjectId& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.value);
    }
};
} // namespace std
