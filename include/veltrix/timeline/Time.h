// Veltrix Timeline - Time.h
//
// Temporal model for editing. DECISION (ADR-002): edit positions are
// integer ticks ("EditTicks") interpreted against an explicit Timebase
// (a Rational "seconds per tick"). Floating point is NEVER used inside the
// model; double appears only at IO boundaries (ffprobe, user input) through
// explicit, documented rounding functions.
//
//   - A tick is the smallest editable unit. A Timebase built "from fps"
//     makes one tick == one frame of that rate, including NTSC rates
//     (30000/1001 stays EXACT; no 29.97 rounding drift).
//   - Converting between timebases goes through exact Rational seconds,
//     so editing at 23.976 fps and previewing at 30 fps cannot accumulate
//     rounding errors within int64 range.
//
// Invariants:
//   * Rational: den > 0, gcd(|num|, den) == 1; zero is 0/1.
//   * Timebase: tick > 0.
//   * All arithmetic is overflow-checked (veltrix::core::checked_*).

#pragma once

#include <cstdint>
#include <string>

#include "veltrix/core/Result.h"

namespace veltrix::timeline {

using veltrix::core::ErrorCode;
using veltrix::core::Result;
using veltrix::core::failure;

// Exact rational number num/den.
class Rational {
public:
    Rational() : num_(0), den_(1) {}

    static Result<Rational> from(std::int64_t num, std::int64_t den) {
        if (den == 0)
            return failure<Rational>(ErrorCode::InvalidArgument, "rational denominator is zero");
        if (num == 0) return Result<Rational>(Rational(0, 1));
        // Normalize sign into the numerator, then divide by the gcd.
        if (den < 0) {
            if (num == std::numeric_limits<std::int64_t>::min())
                return failure<Rational>(ErrorCode::OutOfRange, "rational normalization overflow");
            num = -num;
            den = -den;
        }
        std::int64_t a = num < 0 ? -num : num;
        std::int64_t b = den;
        while (b != 0) { const std::int64_t t = a % b; a = b; b = t; }
        if (a > 1) { num /= a; den /= a; }
        return Result<Rational>(Rational(num, den));
    }

    static Rational zero() { return Rational(); }
    static Rational one() { return Rational(1, 1); }

    std::int64_t num() const { return num_; }
    std::int64_t den() const { return den_; }
    bool is_zero() const { return num_ == 0; }
    bool is_negative() const { return num_ < 0; }

    double to_double() const { return static_cast<double>(num_) / static_cast<double>(den_); }
    std::string to_string() const; // "num/den"

    Result<Rational> add(const Rational& o) const;
    Result<Rational> sub(const Rational& o) const;
    Result<Rational> mul(const Rational& o) const;
    Result<Rational> div(const Rational& o) const;
    Result<Rational> negated() const { return from(-num_, den_); }

    int compare(const Rational& o) const;

    bool operator==(const Rational& o) const { return num_ == o.num_ && den_ == o.den_; }
    bool operator!=(const Rational& o) const { return !(*this == o); }

private:
    Rational(std::int64_t num, std::int64_t den) : num_(num), den_(den) {}
    std::int64_t num_;
    std::int64_t den_;
};

// Edit position/duration unit. Interpretation depends on the Timebase of
// the sequence it belongs to. Negative values are representable (needed by
// conversions and media offsets) but clip positions are validated >= 0.
using EditTicks = std::int64_t;

// Seconds per tick as an exact rational.
class Timebase {
public:
    Timebase() : tick_(Rational::zero()) {} // null timebase; is_valid() == false

    // Creates a timebase where one tick == one frame of num/den fps.
    // Examples: from_fps(24,1), from_fps(30000,1001) (NTSC 29.97).
    static Result<Timebase> from_fps(std::int64_t fps_num, std::int64_t fps_den);
    // Creates a timebase from a raw "seconds per tick" rational.
    static Result<Timebase> from_seconds_per_tick(std::int64_t num, std::int64_t den);

    bool is_valid() const { return !tick_.is_zero() && !tick_.is_negative(); }
    const Rational& seconds_per_tick() const { return tick_; }
    Rational fps() const; // frames per second == 1/tick

    // Exact conversions.
    Rational seconds_for_ticks(EditTicks ticks) const;         // exact
    Result<EditTicks> ticks_for_seconds_exact(const Rational& seconds) const; // lossless only
    Result<EditTicks> ticks_for_seconds_rounded(const Rational& seconds) const;

    // Boundary convenience for doubles (ffprobe durations, user input).
    // Documented rounding: nearest tick, ties away from zero.
    Result<EditTicks> ticks_for_double_seconds(double seconds) const;

    // Frame index for a tick. By construction one tick == one frame,
    // so frame == tick; kept as a named function for readability at call
    // sites and to allow validation to evolve independently.
    std::int64_t frame_for_ticks(EditTicks ticks) const { return ticks; }

    // SMPTE-ish timecode string "HH:MM:SS:FF" at this timebase.
    Result<std::string> timecode_for_ticks(EditTicks ticks) const;

    static const Timebase& fps_23_976();
    static const Timebase& fps_24();
    static const Timebase& fps_25();
    static const Timebase& fps_29_97();
    static const Timebase& fps_30();
    static const Timebase& fps_50();
    static const Timebase& fps_59_94();
    static const Timebase& fps_60();

private:
    explicit Timebase(Rational tick) : tick_(tick) {}
    Rational tick_;
};

// Equality helper for serialization/tests.
inline bool operator==(const Timebase& a, const Timebase& b) {
    return a.seconds_per_tick() == b.seconds_per_tick();
}
inline bool operator!=(const Timebase& a, const Timebase& b) { return !(a == b); }

} // namespace veltrix::timeline
