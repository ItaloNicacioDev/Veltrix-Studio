// Unit tests - temporal model (Rational, Timebase, ticks).
// Covers: NTSC exactness, cross-timebase conversion, rounding policies,
// negative ticks, timecode, overflow paths.

#include <catch2/catch.hpp>

#include <limits>

#include "veltrix/core/MathUtils.h"
#include "veltrix/timeline/Time.h"

using namespace veltrix;
using namespace veltrix::timeline;

TEST_CASE("Rational normalizes and validates", "[time]") {
    auto r = Rational::from(6, 4);
    REQUIRE(r.is_ok());
    REQUIRE(r.value().num() == 3);
    REQUIRE(r.value().den() == 2);

    auto zero_den = Rational::from(1, 0);
    REQUIRE(zero_den.is_error());

    auto neg = Rational::from(-2, -4);
    REQUIRE(neg.is_ok());
    REQUIRE(neg.value().num() == 1);
    REQUIRE(neg.value().den() == 2);
}

TEST_CASE("NTSC 29.97 is exact, no floating drift", "[time]") {
    auto tb = Timebase::from_fps(30000, 1001);
    REQUIRE(tb.is_ok());
    const Timebase& ntsc = tb.value();
    // seconds_per_tick = 1001/30000 exactly.
    REQUIRE(ntsc.seconds_per_tick().num() == 1001);
    REQUIRE(ntsc.seconds_per_tick().den() == 30000);
    // 1 hour of NTSC ticks = 30000*3600/1001... in exact rational seconds:
    const auto fps = ntsc.fps();
    REQUIRE(fps.num() == 30000);
    REQUIRE(fps.den() == 1001);
}

TEST_CASE("Tick<->seconds conversions are exact at integer fps", "[time]") {
    auto tb = Timebase::from_fps(25, 1);
    REQUIRE(tb.is_ok());
    auto s = Rational::from(10, 1); // 10 seconds
    auto ticks = tb.value().ticks_for_seconds_exact(s.value());
    REQUIRE(ticks.is_ok());
    REQUIRE(ticks.value() == 250);
    auto back = tb.value().seconds_for_ticks(250);
    REQUIRE(back == s.value());
}

TEST_CASE("Rounded conversion handles half-away-from-zero", "[time]") {
    auto tb = Timebase::from_fps(30, 1);
    REQUIRE(tb.is_ok());
    const Timebase& t = tb.value();
    auto s1 = Rational::from(1, 2); // 0.5 s at 30fps -> 15 ticks exactly
    REQUIRE(t.ticks_for_seconds_rounded(s1.value()).value() == 15);
    auto s2 = Rational::from(1, 2 + 1); // 1/3 s -> 10 ticks exactly at 30fps
    REQUIRE(t.ticks_for_seconds_rounded(s2.value()).value() == 10);
    // 0.5 tick: 1/60 s at 30 fps = 0.5 tick -> ties away -> 1
    auto s3 = Rational::from(1, 60);
    REQUIRE(t.ticks_for_seconds_rounded(s3.value()).value() == 1);
    auto s3neg = Rational::from(-1, 60);
    REQUIRE(t.ticks_for_seconds_rounded(s3neg.value()).value() == -1);
}

TEST_CASE("Double boundary conversion rejects non-finite", "[time]") {
    auto tb = Timebase::from_fps(24, 1);
    REQUIRE(tb.is_ok());
    REQUIRE(tb.value().ticks_for_double_seconds(1.0).value() == 24);
    REQUIRE(tb.value().ticks_for_double_seconds(0.0416666667).value() == 1);
    REQUIRE(tb.value().ticks_for_double_seconds(std::numeric_limits<double>::quiet_NaN()).is_error());
    REQUIRE(tb.value().ticks_for_double_seconds(std::numeric_limits<double>::infinity()).is_error());
}

TEST_CASE("Timebase rejects invalid fps", "[time]") {
    REQUIRE(Timebase::from_fps(0, 1).is_error());
    REQUIRE(Timebase::from_fps(30, 0).is_error());
    REQUIRE(Timebase::from_fps(-30, 1).is_error());
}

TEST_CASE("Negative ticks are representable at the model level", "[time]") {
    auto tb = Timebase::from_fps(24, 1);
    REQUIRE(tb.is_ok());
    // Model can represent negatives (media offsets); editing layers
    // validate >= 0 separately.
    REQUIRE(tb.value().seconds_for_ticks(-5).is_negative());
}

TEST_CASE("Timecode at integer fps", "[time]") {
    auto tb = Timebase::from_fps(25, 1);
    REQUIRE(tb.is_ok());
    auto tc = tb.value().timecode_for_ticks(0);
    REQUIRE(tc.is_ok());
    REQUIRE(tc.value() == "00:00:00:00");
    auto tc2 = tb.value().timecode_for_ticks(25 * 60 + 12); // 1min12frames
    REQUIRE(tc2.is_ok());
    REQUIRE(tc2.value() == "00:01:00:12");
    // Negative ticks refuse timecode.
    REQUIRE(tb.value().timecode_for_ticks(-1).is_error());
}

TEST_CASE("Cross-timebase conversion keeps exactness", "[time]") {
    // 2 seconds at 24 fps = 48 ticks; at 30 fps = 60 ticks. Through exact
    // rational seconds the mapping is lossless both ways.
    auto t24 = Timebase::from_fps(24, 1);
    auto t30 = Timebase::from_fps(30, 1);
    REQUIRE(t24.is_ok());
    REQUIRE(t30.is_ok());
    auto seconds = t24.value().seconds_for_ticks(48);
    auto ticks30 = t30.value().ticks_for_seconds_exact(seconds);
    REQUIRE(ticks30.is_ok());
    REQUIRE(ticks30.value() == 60);
}

TEST_CASE("Fractional-fps timecode reports unsupported explicitly", "[time]") {
    auto ntsc = Timebase::from_fps(30000, 1001);
    REQUIRE(ntsc.is_ok());
    auto tc = ntsc.value().timecode_for_ticks(100);
    REQUIRE(tc.is_error());
    REQUIRE(tc.error().code == veltrix::core::ErrorCode::Unsupported);
}

TEST_CASE("Presets are distinct and valid", "[time]") {
    REQUIRE(Timebase::fps_24() == Timebase::fps_24());
    REQUIRE(!(Timebase::fps_24() == Timebase::fps_25()));
    REQUIRE(Timebase::fps_29_97().fps().num() == 30000);
    REQUIRE(Timebase::fps_59_94().fps().den() == 1001);
}

TEST_CASE("Checked arithmetic catches overflow", "[time]") {
    using veltrix::core::checked_add;
    using veltrix::core::checked_mul;
    constexpr auto kMax = std::numeric_limits<std::int64_t>::max();
    REQUIRE(checked_add(kMax, 1).is_error());
    REQUIRE(checked_add(kMax - 1, 1).is_ok());
    REQUIRE(checked_mul(kMax, 2).is_error());
    REQUIRE(checked_mul(3, 4).value() == 12);
}
