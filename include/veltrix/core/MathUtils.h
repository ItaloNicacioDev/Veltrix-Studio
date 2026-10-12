// Veltrix Core - MathUtils.h
//
// Overflow-checked integer arithmetic. The temporal model builds every
// conversion on these primitives so a corrupted file or absurd value fails
// with ErrorCode::OutOfRange instead of wrapping around silently.

#pragma once

#include <cstdint>
#include <limits>

#include "veltrix/core/Result.h"

namespace veltrix::core {

using i64 = std::int64_t;

inline Result<i64> checked_add(i64 a, i64 b) {
    // Overflow-safe test without __int128 (MSVC does not provide it).
    if (b > 0 && a > std::numeric_limits<i64>::max() - b)
        return failure<i64>(ErrorCode::OutOfRange, "integer add overflow");
    if (b < 0 && a < std::numeric_limits<i64>::min() - b)
        return failure<i64>(ErrorCode::OutOfRange, "integer add underflow");
    return Result<i64>(a + b);
}

inline Result<i64> checked_sub(i64 a, i64 b) {
    if (b < 0 && a > std::numeric_limits<i64>::max() + b)
        return failure<i64>(ErrorCode::OutOfRange, "integer sub overflow");
    if (b > 0 && a < std::numeric_limits<i64>::min() + b)
        return failure<i64>(ErrorCode::OutOfRange, "integer sub underflow");
    return Result<i64>(a - b);
}

inline Result<i64> checked_mul(i64 a, i64 b) {
    if (a == 0 || b == 0) return Result<i64>(0);
    if (a > 0) {
        if (b > 0 && a > std::numeric_limits<i64>::max() / b)
            return failure<i64>(ErrorCode::OutOfRange, "integer mul overflow");
        if (b < 0 && a < std::numeric_limits<i64>::min() / b)
            return failure<i64>(ErrorCode::OutOfRange, "integer mul underflow");
    } else {
        if (b > 0 && a < std::numeric_limits<i64>::min() / b)
            return failure<i64>(ErrorCode::OutOfRange, "integer mul underflow");
        if (b < 0 && a < std::numeric_limits<i64>::max() / b)
            return failure<i64>(ErrorCode::OutOfRange, "integer mul overflow");
    }
    return Result<i64>(a * b);
}

// Floor division/modulo consistent for negative operands.
inline i64 floor_div(i64 a, i64 b) {
    i64 q = a / b;
    i64 r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) --q;
    return q;
}

inline i64 floor_mod(i64 a, i64 b) {
    i64 r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) r += b;
    return r;
}

} // namespace veltrix::core
