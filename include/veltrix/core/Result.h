// Veltrix Core - Result.h
//
// Error handling foundation. Veltrix does NOT use exceptions as normal
// control flow (see docs/ARCHITECTURE.md, section "Error policy"): every
// fallible operation returns Result<T>, which carries either a value or an
// Error with a stable machine code plus a human readable message.
//
// Invariant: a Result holds exactly one of (value, error), never both/none.

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace veltrix::core {

// Stable, machine-readable error domains. Values are part of the public API:
// never renumber; only append.
enum class ErrorCode : std::uint8_t {
    Unknown = 0,
    InvalidArgument,
    OutOfRange,
    NotFound,
    AlreadyExists,
    Conflict,
    IoError,
    ParseError,
    VersionUnsupported,
    Unsupported,
    Cancelled,
    MediaUnavailable,
    ProbeFailed,
    PluginIncompatible,
    ValidationFailed,
};

// Human readable name for logging and diagnostics.
inline const char* error_code_name(ErrorCode code) {
    switch (code) {
        case ErrorCode::Unknown:              return "Unknown";
        case ErrorCode::InvalidArgument:      return "InvalidArgument";
        case ErrorCode::OutOfRange:           return "OutOfRange";
        case ErrorCode::NotFound:             return "NotFound";
        case ErrorCode::AlreadyExists:        return "AlreadyExists";
        case ErrorCode::Conflict:             return "Conflict";
        case ErrorCode::IoError:              return "IoError";
        case ErrorCode::ParseError:          return "ParseError";
        case ErrorCode::VersionUnsupported:   return "VersionUnsupported";
        case ErrorCode::Unsupported:          return "Unsupported";
        case ErrorCode::Cancelled:            return "Cancelled";
        case ErrorCode::MediaUnavailable:     return "MediaUnavailable";
        case ErrorCode::ProbeFailed:          return "ProbeFailed";
        case ErrorCode::PluginIncompatible:   return "PluginIncompatible";
        case ErrorCode::ValidationFailed:     return "ValidationFailed";
    }
    return "?";
}

// A single failure description. `context` optionally names the involved
// entity (id, path, field). Never put secrets or private paths in messages
// that may be exported by diagnostics reports.
struct Error {
    ErrorCode code{ErrorCode::Unknown};
    std::string message;
    std::string context;

    std::string to_string() const {
        std::string out = "[";
        out += error_code_name(code);
        out += "] ";
        out += message;
        if (!context.empty()) {
            out += " (";
            out += context;
            out += ")";
        }
        return out;
    }
};

inline Error make_error(ErrorCode code, std::string message, std::string context = {}) {
    return Error{code, std::move(message), std::move(context)};
}

template <typename T>
class [[nodiscard]] Result {
public:
    // Implicit construction from a value marks success.
    Result(T value) : storage_(std::in_place_index<0>, std::move(value)) {}

    // Use veltrix::core::failure<T>(...) to build errors explicitly.
    static Result success(T value) { return Result(std::move(value)); }
    static Result failure(Error error) { return Result(std::move(error)); }

    bool is_ok() const { return storage_.index() == 0; }
    bool is_error() const { return storage_.index() == 1; }
    explicit operator bool() const { return is_ok(); }

    const T& value() const & { return std::get<0>(storage_); }
    T& value() & { return std::get<0>(storage_); }
    T&& take_value() { return std::get<0>(std::move(storage_)); }

    const Error& error() const { return std::get<1>(storage_); }

    // Returns the value when present, otherwise `fallback`.
    T value_or(T fallback) const {
        return is_ok() ? std::get<0>(storage_) : std::move(fallback);
    }

private:
    explicit Result(Error error) : storage_(std::in_place_index<1>, std::move(error)) {}
    std::variant<T, Error> storage_;
};

// Result<void> specialization: carries only success or an Error.
class [[nodiscard]] ResultVoid {
public:
    ResultVoid() = default;  // success
    static ResultVoid success() { return ResultVoid(); }
    static ResultVoid failure(Error error) {
        ResultVoid r;
        r.error_ = std::move(error);
        return r;
    }

    bool is_ok() const { return !error_.has_value(); }
    bool is_error() const { return error_.has_value(); }
    explicit operator bool() const { return is_ok(); }
    const Error& error() const { return *error_; }

private:
    std::optional<Error> error_;
};

// Convenience factories. Prefer these over calling Result::failure directly.
template <typename T>
Result<T> failure(ErrorCode code, std::string message, std::string context = {}) {
    return Result<T>::failure(make_error(code, std::move(message), std::move(context)));
}

inline ResultVoid failure(ErrorCode code, std::string message, std::string context = {}) {
    return ResultVoid::failure(make_error(code, std::move(message), std::move(context)));
}

inline ResultVoid success() { return ResultVoid::success(); }

} // namespace veltrix::core
