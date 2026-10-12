// Unit tests - AppConfig parsing, playback state machine, audio mixing,
// color metadata, plugin api constant, io helpers.

#include <catch2/catch.hpp>

#include <filesystem>
#include <fstream>

#include "veltrix/audio/AudioTypes.h"
#include "veltrix/color/ColorContract.h"
#include "veltrix/core/AppConfig.h"
#include "veltrix/diagnostics/Diagnostics.h"
#include "veltrix/io/FileUtils.h"
#include "veltrix/playback/PlaybackEngine.h"

using namespace veltrix;

TEST_CASE("AppConfig parses a valid document", "[config]") {
    std::vector<std::string> warnings;
    auto config = core::AppConfig::from_json(R"({
        "log_level": "debug",
        "cache": {"frame_budget_bytes": 1048576, "eviction": "fifo"},
        "media": {"probe_timeout_seconds": 10},
        "job_worker_threads": 2
    })", warnings);
    REQUIRE(config.is_ok());
    REQUIRE(config.value().log_level == "debug");
    REQUIRE(config.value().frame_cache_budget_bytes == 1048576);
    REQUIRE(config.value().cache_eviction == core::CacheEvictionPolicy::Fifo);
    REQUIRE(config.value().probe_timeout_seconds == 10);
    REQUIRE(config.value().job_worker_threads == 2);
    REQUIRE(warnings.empty());
}

TEST_CASE("AppConfig rejects malformed JSON and roots", "[config]") {
    std::vector<std::string> warnings;
    REQUIRE(core::AppConfig::from_json("nope", warnings).is_error());
    REQUIRE(core::AppConfig::from_json("[1,2]", warnings).is_error());
}

TEST_CASE("AppConfig falls back with warnings on bad values", "[config]") {
    std::vector<std::string> warnings;
    auto config = core::AppConfig::from_json(R"({
        "log_level": "verbose",
        "job_worker_threads": 0
    })", warnings);
    REQUIRE(config.is_ok());
    REQUIRE(config.value().job_worker_threads == 1);
    REQUIRE_FALSE(warnings.empty());
    auto round = config.value().to_json();
    REQUIRE_FALSE(round.empty());
}

TEST_CASE("Playback state machine transitions", "[playback]") {
    playback::PlaybackStateMachine sm;
    REQUIRE(sm.state() == playback::PlayState::Stopped);
    REQUIRE(sm.play().is_ok());
    REQUIRE(sm.state() == playback::PlayState::Playing);
    REQUIRE(sm.pause().is_ok());
    REQUIRE(sm.state() == playback::PlayState::Paused);
    REQUIRE(sm.pause().is_error());
    REQUIRE(sm.play().is_ok());
    REQUIRE(sm.stop().is_ok());
    REQUIRE(sm.state() == playback::PlayState::Stopped);
    // Stop from paused also works.
    sm.play();
    sm.pause();
    REQUIRE(sm.stop().is_ok());
}

TEST_CASE("Audio gain applies with clipping", "[audio]") {
    audio::AudioBuffer buffer;
    buffer.channels = 1;
    buffer.sample_rate = 48000;
    buffer.frames = 4;
    buffer.data = {0.5f, -0.5f, 0.9f, -0.9f};
    audio::apply_gain(buffer, 2.0f);
    REQUIRE(buffer.data[0] == 1.0f);  // clipped
    REQUIRE(buffer.data[1] == -1.0f); // clipped
    REQUIRE(buffer.data[2] == 1.0f);
    REQUIRE(buffer.data[3] == -1.0f);
}

TEST_CASE("Audio mix_into validates and mixes", "[audio]") {
    audio::AudioBuffer dst;
    dst.channels = 2;
    dst.sample_rate = 48000;
    dst.frames = 2;
    dst.data = {0.5f, -0.5f, 0.25f, 0.25f};

    audio::AudioBuffer src;
    src.channels = 2;
    src.sample_rate = 48000;
    src.frames = 1;
    src.data = {0.25f, 0.25f};

    REQUIRE(audio::mix_into(dst, src, 1).is_ok());
    REQUIRE(dst.data[2] == 0.5f);
    REQUIRE(dst.data[3] == 0.5f);

    // Mismatched channel count: rejected.
    audio::AudioBuffer mono;
    mono.channels = 1;
    mono.sample_rate = 48000;
    mono.frames = 1;
    mono.data = {0.1f};
    REQUIRE(audio::mix_into(dst, mono, 0).is_error());
    // Mismatched sample rate: rejected (resampling is planned, not guessed).
    audio::AudioBuffer other_rate;
    other_rate.channels = 2;
    other_rate.sample_rate = 44100;
    other_rate.frames = 1;
    other_rate.data = {0.1f, 0.1f};
    REQUIRE(audio::mix_into(dst, other_rate, 0).is_error());
    // Negative offset: rejected.
    REQUIRE(audio::mix_into(dst, src, -1).is_error());
}

TEST_CASE("Color metadata validates strictly", "[color]") {
    REQUIRE(color::ColorMetadata::create(color::Primaries::BT709,
                                         color::TransferFunction::SRGB, 8, true)
                .is_ok());
    REQUIRE(color::ColorMetadata::create(color::Primaries::Unspecified,
                                         color::TransferFunction::SRGB, 8, true)
                .is_error());
    REQUIRE(color::ColorMetadata::create(color::Primaries::BT709,
                                         color::TransferFunction::PQ, 7, false)
                .is_error());
}

TEST_CASE("File utils atomic write and read round-trip", "[io]") {
    const auto dir = std::filesystem::temp_directory_path() / "veltrix-tests";
    std::filesystem::create_directories(dir);
    const auto path = (dir / "io_roundtrip.txt").string();
    REQUIRE(io::write_text_file_atomic(path, "hello veltrix").is_ok());
    auto read = io::read_text_file(path);
    REQUIRE(read.is_ok());
    REQUIRE(read.value() == "hello veltrix");
    REQUIRE(io::file_exists(path));
    REQUIRE(io::file_size(path).value() == 13);
    REQUIRE(io::read_text_file("Z:/no/such/file").is_error());
    std::filesystem::remove(dir / "io_roundtrip.txt");
}

TEST_CASE("Diagnostics report contains build identity", "[diagnostics]") {
    const std::string report = diagnostics::build_report();
    REQUIRE(report.find("Veltrix") != std::string::npos);
    REQUIRE(report.find("version:") != std::string::npos);
    REQUIRE(report.find("compiler:") != std::string::npos);
}
