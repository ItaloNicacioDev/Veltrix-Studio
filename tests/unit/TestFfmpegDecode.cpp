// tests - FFmpeg decode layer tests (TestFfmpegDecode.cpp)
//
// FIXTURE POLICY: media files are GENERATED at test time by invoking
// ffmpeg (lavfi synthetic sources — no third-party content, freely
// redistributable). Generation needs the ffmpeg BINARY; discovery reuses
// MediaProber's runtime search (PATH / VELTRIX_FFPROBE / explicit).
// When no ffmpeg binary is available, these tests SKIP with an explicit
// message (documented limitation, not a silent pass).
//
// The decode ENGINE tests are additionally guarded: they need
// veltrix-ffmpeg (built when the FFmpeg SDK was found at configure time).

#include <catch2/catch.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include "veltrix/core/CancellationToken.h"
#include "veltrix/media/MediaProber.h"
#include "veltrix/playback/Decoder.h"
#include "veltrix/playback/MediaFrames.h"

#include "media/ffmpeg/AvDecoderFactory.h"

using namespace veltrix;
using veltrix::media::ffmpeg::AvDecoderFactory;
using veltrix::playback::AudioBlock;
using veltrix::playback::IVideoDecoder;
using veltrix::playback::MediaStreamInfo;
using veltrix::playback::PixelFormat;
using veltrix::playback::SeekMode;
using veltrix::playback::VideoFrame;

namespace {

std::filesystem::path fixture_dir() {
    const auto dir = std::filesystem::temp_directory_path() / "veltrix-decode-tests";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir;
}

// Runs the ffmpeg binary to generate a fixture. Returns the file path or
// an empty path when ffmpeg is unavailable.
std::filesystem::path generate_fixture(const std::string& name,
                                       const std::string& args) {
    static std::string ffmpeg_path = [] {
        auto located = media::MediaProber::locate_ffprobe("");
        if (located.is_error()) return std::string();
        // ffprobe lives next to ffmpeg in every known distribution.
        std::string p = located.value();
        const auto slash = p.find_last_of("/\\");
        p = p.substr(0, slash == std::string::npos ? 0 : slash + 1) + "ffmpeg";
#ifdef _WIN32
        p += ".exe";
#endif
        std::error_code ec;
        if (!std::filesystem::exists(std::filesystem::path(p), ec)) return std::string();
        return p;
    }();

    if (ffmpeg_path.empty()) return {};

    const auto out = fixture_dir() / name;
    if (std::filesystem::exists(out)) return out; // cached from a prior run

    // std::system on Windows chokes on a leading double quote (cmd /c
    // treats it as a title); use short-path-free 8.3-free workaround:
    // wrap explicitly with cmd /c and doubled quotes only around args.
    std::string cmd;
#ifdef _WIN32
    cmd = "cmd /c \"\"";
    cmd += ffmpeg_path;
    cmd += "\" -y -hide_banner -loglevel error ";
    cmd += args;
    cmd += " \"";
    cmd += out.string();
    cmd += "\"\"";
#else
    cmd = "\"" + ffmpeg_path + "\" -y -hide_banner -loglevel error " + args +
          " \"" + out.string() + "\"";
#endif
    const int code = std::system(cmd.c_str());
    std::error_code ec;
    if (code != 0 || !std::filesystem::exists(out, ec)) return {};
    return out;
}

// 3 s, 320x240, 30 fps H.264 + 44.1 kHz STEREO AAC (synthetic lavfi).
std::filesystem::path fixture_av() {
    return generate_fixture("av_3s.mp4",
        "-f lavfi -i testsrc2=duration=3:size=320x240:rate=30 "
        "-f lavfi -i \"sine=frequency=440:duration=3\" "
        "-af \"aformat=channel_layouts=stereo\" "
        "-c:v libx264 -preset ultrafast -pix_fmt yuv420p "
        "-c:a aac -b:a 128k -shortest");
}

// 2 s, 160x120, 24 fps H.264, NO audio.
std::filesystem::path fixture_video_only() {
    return generate_fixture("video_only_2s.mp4",
        "-f lavfi -i testsrc2=duration=2:size=160x120:rate=24 "
        "-c:v libx264 -preset ultrafast -pix_fmt yuv420p");
}

// 3 s, 48 kHz stereo AAC, NO video.
std::filesystem::path fixture_audio_only() {
    return generate_fixture("audio_only_3s.m4a",
        "-f lavfi -i sine=frequency=880:duration=3 "
        "-ac 2 -ar 48000 -c:a aac -b:a 128k");
}

} // namespace

// ---------------------------------------------------------------------------

TEST_CASE("Video decode: dimensions, format, pts order, EOS", "[decode][ffmpeg]") {
    const auto file = fixture_av();
    if (file.empty()) {
        WARN("SKIP: ffmpeg binary not found — fixture generation impossible");
        return;
    }
    AvDecoderFactory factory;
    auto decoder_result = factory.create_video_decoder();
    REQUIRE(decoder_result.is_ok());
    auto decoder = decoder_result.take_value();

    SECTION("open reports stream info") {
        auto opened = decoder->open(file.string());
        REQUIRE(opened.is_ok());
        const auto& info = decoder->stream_info();
        REQUIRE(info.has_video);
        REQUIRE(info.video_width == 320);
        REQUIRE(info.video_height == 240);
        REQUIRE(info.video_fps_num == 30);
        REQUIRE(info.video_fps_den == 1);
        REQUIRE(info.video_timebase_den >= info.video_timebase_num); // sane tb
        REQUIRE(info.has_audio); // fixture has audio too
    }

    SECTION("frames decode with exact packed-RGBA8 layout and monotonic pts") {
        REQUIRE(decoder->open(file.string()).is_ok());
        core::CancellationToken token;
        std::int64_t count = 0;
        std::int64_t last_pts = VideoFrame::NO_TIMESTAMP;
        std::int64_t total_bytes = 0;
        bool eos = false;
        while (count < 200) { // safety bound; fixture has ~90 frames
            auto frame = decoder->read_frame(token);
            if (frame.is_error()) {
                // The ONLY acceptable end: end of stream.
                REQUIRE(frame.error().code == core::ErrorCode::NotFound);
                REQUIRE(frame.error().message.find("end of stream") !=
                        std::string::npos);
                eos = true;
                break;
            }
            const VideoFrame& f = frame.value();
            REQUIRE(f.width == 320);
            REQUIRE(f.height == 240);
            REQUIRE(f.format == PixelFormat::R8G8B8A8_Unorm);
            // Packed layout contract: exactly w*h*4 bytes.
            REQUIRE(f.data->bytes.size() ==
                    static_cast<std::size_t>(f.width) * f.height * 4);
            if (f.pts_ticks != VideoFrame::NO_TIMESTAMP) {
                if (last_pts != VideoFrame::NO_TIMESTAMP)
                    REQUIRE(f.pts_ticks > last_pts); // strictly increasing
                last_pts = f.pts_ticks;
            }
            total_bytes += static_cast<std::int64_t>(f.data->bytes.size());
            ++count;
        }
        REQUIRE(eos);
        REQUIRE(count >= 80); // 3 s @ 30 fps = 90 frames (encoder slack)
        REQUIRE(count <= 95);
        REQUIRE(total_bytes == count * 320 * 240 * 4);
    }

    SECTION("first frame is a keyframe") {
        REQUIRE(decoder->open(file.string()).is_ok());
        core::CancellationToken token;
        auto frame = decoder->read_frame(token);
        REQUIRE(frame.is_ok());
        REQUIRE(frame.value().keyframe);
    }
}

TEST_CASE("Video seek: start, middle, end", "[decode][ffmpeg][seek]") {
    const auto file = fixture_av();
    if (file.empty()) {
        WARN("SKIP: ffmpeg binary not found");
        return;
    }
    AvDecoderFactory factory;
    auto decoder = factory.create_video_decoder().take_value();
    REQUIRE(decoder->open(file.string()).is_ok());
    const auto& info = decoder->stream_info();
    // tb = 1/15360 for typical mp4 h264 (fps 30, tb may vary): use fps-based
    // ticks: 1 frame = tb_den/(fps) ... safer: seek by stream tb directly.
    const std::int64_t tb_num = info.video_timebase_num;
    const std::int64_t tb_den = info.video_timebase_den;
    core::CancellationToken token;

    SECTION("seek to middle (1.5 s) lands at-or-after target") {
        const std::int64_t target =
            (tb_den * 3) / (tb_num * 2); // 1.5 s in stream ticks (approx)
        REQUIRE(decoder->seek(target, SeekMode::Exact).is_ok());
        auto frame = decoder->read_frame(token);
        REQUIRE(frame.is_ok());
        REQUIRE(frame.value().pts_ticks != VideoFrame::NO_TIMESTAMP);
        // Lands at-or-after 1.5s: never before.
        REQUIRE(frame.value().pts_ticks >= target - 1);
        // And not absurdly far: within 2 s.
        REQUIRE(frame.value().pts_ticks < target + 2 * tb_den / tb_num);
    }

    SECTION("seek to end (beyond last frame) fails OutOfRange or EOS") {
        // 60 s is way beyond a 3 s file.
        const std::int64_t target = 60 * tb_den / tb_num;
        auto seeked = decoder->seek(target, SeekMode::Exact);
        if (seeked.is_ok()) {
            // Some demuxers allow the seek; the read then hits EOS.
            auto frame = decoder->read_frame(token);
            if (frame.is_ok()) {
                // frame decoded: pts must not exceed duration
                REQUIRE(frame.value().pts_ticks <=
                        info.duration_ticks * info.duration_timebase_den /
                            info.duration_timebase_num);
            }
        } else {
            REQUIRE(seeked.error().code == core::ErrorCode::OutOfRange);
        }
    }

    SECTION("seek to start then decode full file again") {
        REQUIRE(decoder->seek(0, SeekMode::Exact).is_ok());
        std::int64_t count = 0;
        for (;;) {
            auto frame = decoder->read_frame(token);
            if (frame.is_error()) break;
            ++count;
            if (count > 100) break;
        }
        REQUIRE(count >= 80); // full re-decode after seek works
    }

    SECTION("keyframe seek lands on/before target") {
        const std::int64_t target = (tb_den * 2) / tb_num; // 2 s
        REQUIRE(decoder->seek(target, SeekMode::Keyframe).is_ok());
        auto frame = decoder->read_frame(token);
        REQUIRE(frame.is_ok());
        // Keyframe mode returns the landing frame; pts <= target.
        if (frame.value().pts_ticks != VideoFrame::NO_TIMESTAMP)
            REQUIRE(frame.value().pts_ticks <= target + 1);
    }
}

TEST_CASE("Audio decode: channels, sample counts, EOS", "[decode][ffmpeg][audio]") {
    const auto file = fixture_av();
    if (file.empty()) {
        WARN("SKIP: ffmpeg binary not found");
        return;
    }
    AvDecoderFactory factory;
    auto decoder = factory.create_audio_decoder().take_value();
    REQUIRE(decoder->open(file.string()).is_ok());
    const auto& info = decoder->stream_info();
    REQUIRE(info.has_audio);
    REQUIRE(info.audio_sample_rate == 44100);
    REQUIRE(info.audio_channels == 2);

    core::CancellationToken token;
    std::int64_t blocks = 0;
    std::int64_t frames = 0;
    std::int64_t expected_min = 44100 * 2; // >= 2 s of samples
    for (;;) {
        auto block = decoder->read_block(token);
        if (block.is_error()) {
            REQUIRE(block.error().code == core::ErrorCode::NotFound);
            REQUIRE(block.error().message.find("end of stream") != std::string::npos);
            break;
        }
        const AudioBlock& b = block.value();
        REQUIRE(b.channels == 2);
        REQUIRE(b.sample_rate == 44100);
        REQUIRE(b.frames > 0);
        // Buffer contract: frames * channels * sizeof(float).
        REQUIRE(b.data->bytes.size() ==
                static_cast<std::size_t>(b.frames) * b.channels * sizeof(float));
        frames += b.frames;
        ++blocks;
        if (blocks > 5000) break; // safety
    }
    REQUIRE(frames >= expected_min);
    REQUIRE(frames <= 44100 * 4); // no more than 4 s worth
    REQUIRE(blocks >= 1);
}

TEST_CASE("Audio-only file: video decoder refuses, audio decodes", "[decode][ffmpeg]") {
    const auto file = fixture_audio_only();
    if (file.empty()) {
        WARN("SKIP: ffmpeg binary not found");
        return;
    }
    AvDecoderFactory factory;

    auto video = factory.create_video_decoder().take_value();
    auto vopen = video->open(file.string());
    REQUIRE(vopen.is_error());
    REQUIRE(vopen.error().code == core::ErrorCode::Unsupported);
    REQUIRE_FALSE(video->is_open()); // failed open leaves nothing dangling

    auto audio = factory.create_audio_decoder().take_value();
    REQUIRE(audio->open(file.string()).is_ok());
    REQUIRE(audio->stream_info().has_audio);
    REQUIRE_FALSE(audio->stream_info().has_video);
    core::CancellationToken token;
    std::int64_t frames = 0;
    for (;;) {
        auto block = audio->read_block(token);
        if (block.is_error()) break;
        frames += block.value().frames;
        if (frames > 48000 * 4) break;
    }
    REQUIRE(frames >= 48000 * 2);
}

TEST_CASE("Video-only file: audio decoder refuses, video decodes", "[decode][ffmpeg]") {
    const auto file = fixture_video_only();
    if (file.empty()) {
        WARN("SKIP: ffmpeg binary not found");
        return;
    }
    AvDecoderFactory factory;

    auto audio = factory.create_audio_decoder().take_value();
    auto aopen = audio->open(file.string());
    REQUIRE(aopen.is_error());
    REQUIRE(aopen.error().code == core::ErrorCode::Unsupported);

    auto video = factory.create_video_decoder().take_value();
    REQUIRE(video->open(file.string()).is_ok());
    REQUIRE(video->stream_info().has_video);
    REQUIRE_FALSE(video->stream_info().has_audio);
    core::CancellationToken token;
    std::int64_t count = 0;
    for (;;) {
        auto frame = video->read_frame(token);
        if (frame.is_error()) break;
        REQUIRE(frame.value().width == 160);
        REQUIRE(frame.value().height == 120);
        ++count;
        if (count > 60) break;
    }
    REQUIRE(count >= 40); // 2 s @ 24 fps = 48
}

TEST_CASE("Invalid and missing files fail explicitly", "[decode][ffmpeg][errors]") {
    AvDecoderFactory factory;
    auto video = factory.create_video_decoder().take_value();

    // Missing file.
    auto missing = video->open("Z:/definitely/not/there.mp4");
    REQUIRE(missing.is_error());
    REQUIRE(missing.error().code == core::ErrorCode::NotFound);

    // Not-media file (text).
    const auto bad = fixture_dir() / "not_media.txt";
    {
        std::ofstream out(bad, std::ios::binary);
        out << "this is definitely not a media file, sorry";
    }
    auto invalid = video->open(bad.string());
    REQUIRE(invalid.is_error());
    REQUIRE((invalid.error().code == core::ErrorCode::ParseError ||
             invalid.error().code == core::ErrorCode::Unsupported));

    // Empty file.
    const auto empty = fixture_dir() / "empty.mp4";
    { std::ofstream out(empty, std::ios::binary); }
    auto empty_res = video->open(empty.string());
    REQUIRE(empty_res.is_error());

    // After a failed open the decoder is reusable (guards cleaned up).
    const auto good = fixture_av();
    if (!good.empty()) {
        REQUIRE(video->open(good.string()).is_ok());
        video->close();
        REQUIRE_FALSE(video->is_open());
        // close is idempotent.
        video->close();
    }
}

TEST_CASE("Cancellation aborts decode mid-stream", "[decode][ffmpeg][cancel]") {
    const auto file = fixture_av();
    if (file.empty()) {
        WARN("SKIP: ffmpeg binary not found");
        return;
    }
    AvDecoderFactory factory;
    auto video = factory.create_video_decoder().take_value();
    REQUIRE(video->open(file.string()).is_ok());

    core::CancellationToken token;
    // Decode a few frames fine...
    for (int i = 0; i < 5; ++i)
        REQUIRE(video->read_frame(token).is_ok());
    // ...then cancel: subsequent reads must return Cancelled promptly.
    token.cancel();
    auto aborted = video->read_frame(token);
    REQUIRE(aborted.is_error());
    REQUIRE(aborted.error().code == core::ErrorCode::Cancelled);

    // Cancellation does NOT destroy decoder state: close() still safe.
    video->close();
    REQUIRE_FALSE(video->is_open());
}

TEST_CASE("Repeated open/close cycles leak nothing detectable", "[decode][ffmpeg]") {
    const auto file = fixture_av();
    if (file.empty()) {
        WARN("SKIP: ffmpeg binary not found");
        return;
    }
    AvDecoderFactory factory;
    for (int cycle = 0; cycle < 25; ++cycle) {
        auto video = factory.create_video_decoder().take_value();
        REQUIRE(video->open(file.string()).is_ok());
        core::CancellationToken token;
        REQUIRE(video->read_frame(token).is_ok());
        video->close();
        // Decoder destruction happens at loop-scope end: RAII only.
    }
    SUCCEED("25 open/decode/close cycles completed");
}

TEST_CASE("Decoder destroyed mid-flight (no use-after-free)", "[decode][ffmpeg]") {
    const auto file = fixture_av();
    if (file.empty()) {
        WARN("SKIP: ffmpeg binary not found");
        return;
    }
    AvDecoderFactory factory;
    for (int i = 0; i < 10; ++i) {
        auto video = factory.create_video_decoder().take_value();
        REQUIRE(video->open(file.string()).is_ok());
        core::CancellationToken token;
        auto frame = video->read_frame(token); // decode one frame
        REQUIRE(frame.is_ok());
        // Destroy WITHOUT close(): destructor must clean everything.
        video.reset();
    }
    SUCCEED("10 destructions without close() completed");
}

TEST_CASE("Engine reports linked libav versions", "[decode][ffmpeg]") {
    AvDecoderFactory factory;
    const std::string name = factory.engine_name();
    REQUIRE(name.find("libav") == 0);
}
