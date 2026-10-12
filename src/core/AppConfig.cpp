// Veltrix Core - AppConfig.cpp

#include "veltrix/core/AppConfig.h"

#include <algorithm>
#include <cctype>

#include <nlohmann/json.hpp>

#include "veltrix/core/Log.h"

namespace veltrix::core {

namespace {
LogLevel parse_log_level_name(const std::string& name, bool& ok) {
    ok = true;
    std::string lower;
    lower.reserve(name.size());
    for (char c : name) lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    if (lower == "trace")   return LogLevel::Trace;
    if (lower == "debug")   return LogLevel::Debug;
    if (lower == "info")    return LogLevel::Info;
    if (lower == "warning" || lower == "warn") return LogLevel::Warning;
    if (lower == "error")   return LogLevel::Error;
    ok = false;
    return LogLevel::Info;
}
} // namespace

Result<AppConfig> AppConfig::from_json(const std::string& json_text,
                                       std::vector<std::string>& warnings) {
    nlohmann::json doc;
    try {
        doc = nlohmann::json::parse(json_text);
    } catch (const nlohmann::json::parse_error& e) {
        return failure<AppConfig>(ErrorCode::ParseError,
                                  std::string("invalid config JSON: ") + e.what());
    }
    if (!doc.is_object())
        return failure<AppConfig>(ErrorCode::ParseError, "config root must be a JSON object");

    AppConfig config;

    auto read_string = [&](const char* key, std::string& out) {
        if (auto it = doc.find(key); it != doc.end() && it->is_string())
            out = it->get<std::string>();
        else if (it != doc.end())
            warnings.push_back(std::string("config: '") + key + "' ignored (expected string)");
    };
    auto read_u64 = [&](const char* key, std::uint64_t& out) {
        if (auto it = doc.find(key); it != doc.end() && it->is_number_unsigned())
            out = it->get<std::uint64_t>();
        else if (it != doc.end())
            warnings.push_back(std::string("config: '") + key + "' ignored (expected non-negative integer)");
    };
    auto read_u32 = [&](const char* key, std::uint32_t& out) {
        std::uint64_t tmp = out;
        read_u64(key, tmp);
        out = static_cast<std::uint32_t>(tmp);
    };

    read_string("log_level", config.log_level);
    bool ok = false;
    parse_log_level_name(config.log_level, ok);
    if (!ok) warnings.push_back("config: unknown log_level '" + config.log_level + "', using 'info'");

    if (auto cache = doc.find("cache"); cache != doc.end() && cache->is_object()) {
        if (auto it = cache->find("frame_budget_bytes"); it != cache->end() && it->is_number_unsigned())
            config.frame_cache_budget_bytes = it->get<std::uint64_t>();
        if (auto it = cache->find("metadata_budget_bytes"); it != cache->end() && it->is_number_unsigned())
            config.metadata_cache_budget_bytes = it->get<std::uint64_t>();
        if (auto it = cache->find("eviction"); it != cache->end() && it->is_string()) {
            std::string ev = it->get<std::string>();
            std::transform(ev.begin(), ev.end(), ev.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (ev == "lru") config.cache_eviction = CacheEvictionPolicy::Lru;
            else if (ev == "fifo") config.cache_eviction = CacheEvictionPolicy::Fifo;
            else warnings.push_back("config: unknown cache.eviction '" + ev + "'");
        }
    }

    if (auto media = doc.find("media"); media != doc.end() && media->is_object()) {
        if (auto it = media->find("ffprobe_path"); it != media->end() && it->is_string())
            config.ffprobe_path = it->get<std::string>();
        if (auto it = media->find("probe_timeout_seconds"); it != media->end() && it->is_number_unsigned())
            config.probe_timeout_seconds = static_cast<std::uint32_t>(it->get<std::uint64_t>());
    }

    read_u32("job_worker_threads", config.job_worker_threads);
    if (config.job_worker_threads == 0) {
        config.job_worker_threads = 1;
        warnings.push_back("config: job_worker_threads must be >= 1, using 1");
    }

    return Result<AppConfig>(std::move(config));
}

std::string AppConfig::to_json() const {
    nlohmann::json doc;
    doc["log_level"] = log_level;
    doc["cache"] = {
        {"frame_budget_bytes", frame_cache_budget_bytes},
        {"metadata_budget_bytes", metadata_cache_budget_bytes},
        {"eviction", cache_eviction == CacheEvictionPolicy::Lru ? "lru" : "fifo"},
    };
    doc["media"] = {
        {"ffprobe_path", ffprobe_path},
        {"probe_timeout_seconds", probe_timeout_seconds},
    };
    doc["job_worker_threads"] = job_worker_threads;
    return doc.dump(2);
}

} // namespace veltrix::core
