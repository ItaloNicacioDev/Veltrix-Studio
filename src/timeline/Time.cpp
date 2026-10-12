// Veltrix Timeline - Time.cpp

#include "veltrix/timeline/Time.h"

#include <cmath>
#include <cstdio>
#include <limits>

#include "veltrix/core/MathUtils.h"

namespace veltrix::timeline {

using veltrix::core::checked_add;
using veltrix::core::checked_mul;
using veltrix::core::checked_sub;

std::string Rational::to_string() const {
    char buf[48];
    std::snprintf(buf, sizeof(buf), "%lld/%lld",
                  static_cast<long long>(num_), static_cast<long long>(den_));
    return buf;
}

Result<Rational> Rational::add(const Rational& o) const {
    // a/b + c/d = (a*d + c*b) / (b*d), normalized by from().
    auto ad = checked_mul(num_, o.den_);
    auto cb = checked_mul(o.num_, den_);
    if (ad.is_error() || cb.is_error())
        return failure<Rational>(ErrorCode::OutOfRange, "rational add overflow");
    auto numer = checked_add(ad.take_value(), cb.take_value());
    auto denom = checked_mul(den_, o.den_);
    if (numer.is_error() || denom.is_error())
        return failure<Rational>(ErrorCode::OutOfRange, "rational add overflow");
    return from(numer.take_value(), denom.take_value());
}

Result<Rational> Rational::sub(const Rational& o) const {
    auto neg = o.negated();
    if (neg.is_error()) return neg;
    return add(neg.take_value());
}

Result<Rational> Rational::mul(const Rational& o) const {
    auto n = checked_mul(num_, o.num_);
    auto d = checked_mul(den_, o.den_);
    if (n.is_error() || d.is_error())
        return failure<Rational>(ErrorCode::OutOfRange, "rational mul overflow");
    return from(n.take_value(), d.take_value());
}

Result<Rational> Rational::div(const Rational& o) const {
    if (o.is_zero())
        return failure<Rational>(ErrorCode::InvalidArgument, "rational division by zero");
    auto n = checked_mul(num_, o.den_);
    auto d = checked_mul(den_, o.num_);
    if (n.is_error() || d.is_error())
        return failure<Rational>(ErrorCode::OutOfRange, "rational div overflow");
    return from(n.take_value(), d.take_value());
}

int Rational::compare(const Rational& o) const {
    // a/b vs c/d: a*d vs c*b (denominators are always positive).
    auto lhs = checked_mul(num_, o.den_);
    auto rhs = checked_mul(o.num_, den_);
    if (lhs.is_ok() && rhs.is_ok()) {
        const std::int64_t l = lhs.value(), r = rhs.value();
        return l < r ? -1 : (l > r ? 1 : 0);
    }
    // Overflow fallback: long double comparison (documented: values that
    // overflow int64 cross-multiplication are astronomically out of the
    // editor's operating range and only used for ordering decisions).
    const long double l = static_cast<long double>(num_) * static_cast<long double>(o.den_);
    const long double r = static_cast<long double>(o.num_) * static_cast<long double>(den_);
    return l < r ? -1 : (l > r ? 1 : 0);
}

// ---- Timebase ---------------------------------------------------------------

Result<Timebase> Timebase::from_fps(std::int64_t fps_num, std::int64_t fps_den) {
    if (fps_num <= 0)
        return failure<Timebase>(ErrorCode::InvalidArgument, "fps numerator must be > 0");
    if (fps_den <= 0)
        return failure<Timebase>(ErrorCode::InvalidArgument, "fps denominator must be > 0");
    // seconds per tick = fps_den / fps_num  (e.g. NTSC: 1001/30000).
    auto tick = Rational::from(fps_den, fps_num);
    if (tick.is_error()) return failure<Timebase>(tick.error().code, tick.error().message);
    return Result<Timebase>(Timebase(tick.take_value()));
}

Result<Timebase> Timebase::from_seconds_per_tick(std::int64_t num, std::int64_t den) {
    auto tick = Rational::from(num, den);
    if (tick.is_error()) return failure<Timebase>(tick.error().code, tick.error().message);
    const Rational t = tick.value();
    if (t.is_zero() || t.is_negative())
        return failure<Timebase>(ErrorCode::InvalidArgument,
                                 "seconds-per-tick must be > 0");
    return Result<Timebase>(Timebase(t));
}

Rational Timebase::fps() const {
    auto one = Rational::from(1, 1);
    if (one.is_error() || tick_.is_zero()) return Rational::zero();
    auto f = one.take_value().div(tick_);
    return f.is_ok() ? f.take_value() : Rational::zero();
}

Rational Timebase::seconds_for_ticks(EditTicks ticks) const {
    auto t = Rational::from(ticks, 1);
    if (t.is_error()) return Rational::zero();
    auto s = t.take_value().mul(tick_);
    return s.is_ok() ? s.take_value() : Rational::zero();
}

Result<EditTicks> Timebase::ticks_for_seconds_exact(const Rational& seconds) const {
    // ticks = seconds / tick = seconds * (den/num). num > 0, den > 0.
    auto tick_inv = Rational::from(tick_.den(), tick_.num());
    if (tick_inv.is_error())
        return failure<EditTicks>(tick_inv.error().code, tick_inv.error().message);
    auto ticks = seconds.mul(tick_inv.take_value());
    if (ticks.is_error()) return failure<EditTicks>(ticks.error().code, ticks.error().message);
    const Rational& t = ticks.value();
    if (t.den() != 1)
        return failure<EditTicks>(ErrorCode::OutOfRange,
                                   "seconds value does not map to a whole tick");
    return Result<EditTicks>(t.num());
}

Result<EditTicks> Timebase::ticks_for_seconds_rounded(const Rational& seconds) const {
    // ticks = seconds * den / num rounded to nearest (ties away from zero).
    auto numer = checked_mul(seconds.num(), tick_.den());
    auto denom = checked_mul(seconds.den(), tick_.num());
    if (numer.is_error() || denom.is_error())
        return failure<EditTicks>(ErrorCode::OutOfRange, "seconds->ticks overflow");
    const std::int64_t n = numer.value(), d = denom.value();
    if (d == 0) return failure<EditTicks>(ErrorCode::InvalidArgument, "zero denominator");
    auto two_n = checked_mul(n, 2);
    auto two_d = checked_mul(d, 2);
    if (two_n.is_error() || two_d.is_error())
        return failure<EditTicks>(ErrorCode::OutOfRange, "seconds->ticks overflow");
    if (n >= 0) {
        auto numr = checked_add(two_n.value(), d);
        if (numr.is_error()) return failure<EditTicks>(numr.error().code, numr.error().message);
        return Result<EditTicks>(veltrix::core::floor_div(numr.value(), two_d.value()));
    }
    auto pos = checked_mul(-n, 2);
    if (pos.is_error()) return failure<EditTicks>(pos.error().code, pos.error().message);
    auto numr = checked_add(pos.value(), d);
    if (numr.is_error()) return failure<EditTicks>(numr.error().code, numr.error().message);
    return Result<EditTicks>(-veltrix::core::floor_div(numr.value(), two_d.value()));
}

Result<EditTicks> Timebase::ticks_for_double_seconds(double seconds) const {
    if (std::isnan(seconds) || std::isinf(seconds))
        return failure<EditTicks>(ErrorCode::InvalidArgument, "seconds is not finite");
    // ticks ~= seconds / tick.to_double(); round-half-away from zero.
    const double raw = seconds / tick_.to_double();
    if (std::fabs(raw) > 9.0e18)
        return failure<EditTicks>(ErrorCode::OutOfRange, "seconds value out of tick range");
    const double rounded = raw >= 0.0 ? std::floor(raw + 0.5) : std::ceil(raw - 0.5);
    return Result<EditTicks>(static_cast<std::int64_t>(rounded));
}

Result<std::string> Timebase::timecode_for_ticks(EditTicks ticks) const {
    if (ticks < 0)
        return failure<std::string>(ErrorCode::InvalidArgument,
                                    "timecode requires non-negative ticks");
    // 1 tick == 1 frame at this timebase.
    Rational fps = this->fps();
    if (fps.is_zero())
        return failure<std::string>(ErrorCode::InvalidArgument, "timebase has no fps");
    // Total frames; split H:M:S:F with frames-per-second = fps.num()/fps.den()
    // (integer when fps.den()==1; for NTSC the frame field still counts ticks
    // of this timebase, which is what editors display).
    const std::int64_t fps_den = fps.den();
    const std::int64_t fps_num = fps.num();
    if (fps_den != 1)
        return failure<std::string>(ErrorCode::Unsupported,
                                    "fractional-fps drop-frame timecode not implemented");
    const std::int64_t per_second = fps_num;
    const std::int64_t per_minute = per_second * 60;
    const std::int64_t per_hour = per_minute * 60;
    const std::int64_t hours = ticks / per_hour;
    std::int64_t rest = ticks % per_hour;
    const std::int64_t minutes = rest / per_minute;
    rest %= per_minute;
    const std::int64_t secs = rest / per_second;
    const std::int64_t frames = rest % per_second;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%02lld:%02lld:%02lld:%02lld",
                  static_cast<long long>(hours), static_cast<long long>(minutes),
                  static_cast<long long>(secs), static_cast<long long>(frames));
    return Result<std::string>(std::string(buf));
}

// Presets are exact rationals; construction cannot fail, but the asserts
// document the expectation.
#define VELTRIX_TB_PRESET(Name, N, D)                                    \
    const Timebase& Timebase::Name() {                                  \
        static const Timebase tb = [] {                                 \
            auto r = Timebase::from_fps(N, D);                          \
            return r.is_ok() ? r.take_value() : Timebase();             \
        }();                                                            \
        return tb;                                                       \
    }

VELTRIX_TB_PRESET(fps_23_976, 24000, 1001)
VELTRIX_TB_PRESET(fps_24, 24, 1)
VELTRIX_TB_PRESET(fps_25, 25, 1)
VELTRIX_TB_PRESET(fps_29_97, 30000, 1001)
VELTRIX_TB_PRESET(fps_30, 30, 1)
VELTRIX_TB_PRESET(fps_50, 50, 1)
VELTRIX_TB_PRESET(fps_59_94, 60000, 1001)
VELTRIX_TB_PRESET(fps_60, 60, 1)

#undef VELTRIX_TB_PRESET

} // namespace veltrix::timeline
