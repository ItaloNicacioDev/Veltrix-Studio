// Veltrix Core - AppConfig.h
//
// Runtime application configuration. A small, explicit value type - not a
// service locator. Loaded from an optional JSON file; every field has a
// safe default so the core runs with no configuration present.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "veltrix/core/Result.h"

namespace veltrix::core {

enum class CacheEvictionPolicy : std::uint8_t { Lru = 0, Fifo };

struct AppConfig {
    // Logging
    std::string log_level = "info";        // trace|debug|info|warning|error

    // Cache
    std::uint64_t frame_cache_budget_bytes = 512ull * 1024 * 1024; // 512 MiB
    std::uint64_t metadata_cache_budget_bytes = 32ull * 1024 * 1024;
    CacheEvictionPolicy cache_eviction = CacheEvictionPolicy::Lru;

    // Media probing
    std::string ffprobe_path;              // empty -> discover on PATH / env
    std::uint32_t probe_timeout_seconds = 30;

    // Jobs
    std::uint32_t job_worker_threads = 1;  // foundation uses a single worker

    // Parses a JSON document into a config. Unknown keys are ignored;
    // malformed values fall back to defaults and are reported as warnings
    // in `warnings`.
    static Result<AppConfig> from_json(const std::string& json_text,
                                       std::vector<std::string>& warnings);

    // Serializes the config back to a JSON document.
    std::string to_json() const;
};

} // namespace veltrix::core
