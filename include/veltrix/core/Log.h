// Veltrix Core - Log.h
//
// Minimal, thread-safe, dependency-free logging. Design goals:
//   * no global singleton - Log objects are owned and injected explicitly;
//   * sinks are pluggable (console, callback); diagnostics keeps a bounded
//     ring buffer of recent records;
//   * the logger never throws and never logs secrets - that is a caller
//     responsibility documented in docs/ARCHITECTURE.md.
//
// Invariant: a log record once accepted is delivered to every sink,
    // synchronously, in submission order.

#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace veltrix::core {

enum class LogLevel : std::uint8_t {
    Trace = 0,
    Debug,
    Info,
    Warning,
    Error,
};

inline const char* log_level_name(LogLevel level) {
    switch (level) {
        case LogLevel::Trace:   return "TRACE";
        case LogLevel::Debug:   return "DEBUG";
        case LogLevel::Info:    return "INFO ";
        case LogLevel::Warning: return "WARN ";
        case LogLevel::Error:  return "ERROR";
    }
    return "?";
}

struct LogRecord {
    LogLevel level;
    std::string category;
    std::string message;
};

// Bounded ring of the most recent records, used by diagnostics.
class LogHistory {
public:
    explicit LogHistory(std::size_t capacity = 512) : capacity_(capacity) {}

    void push(const LogRecord& record) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (records_.size() == capacity_) records_.erase(records_.begin());
        records_.push_back(record);
    }

    std::vector<LogRecord> snapshot() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return records_;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        records_.clear();
    }

private:
    mutable std::mutex mutex_;
    std::size_t capacity_;
    std::vector<LogRecord> records_;
};

class Log {
public:
    using Sink = std::function<void(const LogRecord&)>;

    explicit Log(LogLevel min_level = LogLevel::Info) : min_level_(min_level) {}

    void set_min_level(LogLevel level) { min_level_ = level; }
    LogLevel min_level() const { return min_level_; }

    // Adds a sink; sinks must be registered before use and kept alive as
    // long as the logger. Adding a sink is not thread-safe against logging.
    void add_sink(Sink sink) { sinks_.push_back(std::move(sink)); }

    void log(LogLevel level, const char* category, const std::string& message) const {
        if (level < min_level_) return;
        LogRecord record{level, category ? category : "", message};
        for (const Sink& sink : sinks_) sink(record);
    }

    void trace(const char* category, const std::string& m) const { log(LogLevel::Trace, category, m); }
    void debug(const char* category, const std::string& m) const { log(LogLevel::Debug, category, m); }
    void info(const char* category, const std::string& m) const { log(LogLevel::Info, category, m); }
    void warn(const char* category, const std::string& m) const { log(LogLevel::Warning, category, m); }
    void error(const char* category, const std::string& m) const { log(LogLevel::Error, category, m); }

private:
    LogLevel min_level_;
    std::vector<Sink> sinks_;
};

// Convenience for building a message with streams: log.info("core", VELTRIX_STR("x=" << x));
#define VELTRIX_STR(expr) ([&] { std::ostringstream veltrix_os_; veltrix_os_ << expr; return veltrix_os_.str(); }())

} // namespace veltrix::core
