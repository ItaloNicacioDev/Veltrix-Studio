// veltrix-benchmarks — decode measurements (Fase 2, brief item 9).
//
// Measures with std::chrono on REAL decode paths:
//   * open time        (avformat+codec init)
//   * time to first frame
//   * seek + time to the requested frame
//   * sequential decode throughput
//   * buffer memory footprint (exact, since buffers are w*h*4)
//
// Environment/codec/resolution are printed with each measurement
// (no arbitrary performance targets — evidence only).

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>

#include "veltrix/core/CancellationToken.h"
#include "veltrix/media/MediaProber.h"
#include "veltrix/playback/Decoder.h"

#include "media/ffmpeg/AvDecoderFactory.h"

using Clock = std::chrono::steady_clock;
using namespace veltrix;

namespace {

double ms_since(Clock::time_point t) {
    return std::chrono::duration<double, std::milli>(Clock::now() - t).count();
}

std::string find_ffmpeg_binary() {
    // Same discovery the tests use (ffprobe dir -> ffmpeg).
    auto located = media::MediaProber::locate_ffprobe("");
    if (located.is_error()) return {};
    std::string p = located.value();
    const auto slash = p.find_last_of("/\\");
    p = p.substr(0, slash == std::string::npos ? 0 : slash + 1) + "ffmpeg";
#ifdef _WIN32
    p += ".exe";
#endif
    std::error_code ec;
    return std::filesystem::exists(std::filesystem::path(p), ec) ? p : std::string{};
}

bool make_fixture(const std::filesystem::path& out) {
    std::error_code ec;
    if (std::filesystem::exists(out, ec)) return true;
    const std::string ffmpeg = find_ffmpeg_binary();
    if (ffmpeg.empty()) return false;
    const std::string cmd =
        "\"" + ffmpeg + "\" -y -hide_banner -loglevel error "
        "-f lavfi -i testsrc2=duration=10:size=1920x1080:rate=30 "
        "-f lavfi -i \"sine=frequency=440:duration=10\" "
        "-af \"aformat=channel_layouts=stereo\" "
        "-c:v libx264 -preset veryfast -pix_fmt yuv420p "
        "-c:a aac -b:a 128k -shortest \"" + out.string() + "\"";
    return std::system(cmd.c_str()) == 0;
}

} // namespace

int main() {
    const auto dir = std::filesystem::temp_directory_path() / "veltrix-decode-tests";
    std::filesystem::create_directories(dir);
    const auto file = dir / "bench_1080p30_10s.mp4";
    if (!make_fixture(file)) {
        std::cout << "benchmarks: ffmpeg binary unavailable — skip decode bench\n";
        return 0;
    }

    media::ffmpeg::AvDecoderFactory factory;
    core::CancellationToken token;

    std::cout << "veltrix decode benchmarks\n";
    std::cout << "fixture: 1080p H.264 + AAC stereo, 10 s @ 30 fps (lavfi)\n";
#ifdef _WIN32
    std::cout << "platform: Windows x64";
#else
    std::cout << "platform: Linux x64";
#endif
    std::cout << "  engine: " << factory.engine_name() << "\n\n";

    // ---- 1. open + first frame ----------------------------------------------
    {
        auto decoder = factory.create_video_decoder().take_value();
        const auto t0 = Clock::now();
        auto opened = decoder->open(file.string());
        const double open_ms = ms_since(t0);
        if (opened.is_error()) {
            std::cout << "open failed: " << opened.error().to_string() << "\n";
            return 1;
        }
        const auto t1 = Clock::now();
        auto frame = decoder->read_frame(token);
        const double first_frame_ms = ms_since(t1);
        std::cout << "open_ms=" << open_ms
                  << "  first_frame_ms=" << first_frame_ms << "\n";
    }

    // ---- 2. seek + requested frame -------------------------------------------
    {
        auto decoder = factory.create_video_decoder().take_value();
        decoder->open(file.string());
        const auto& info = decoder->stream_info();
        const std::int64_t target =
            (info.video_timebase_den * 5) / info.video_timebase_num; // 5 s
        const auto t0 = Clock::now();
        decoder->seek(target, playback::SeekMode::Exact);
        auto frame = decoder->read_frame(token);
        const double seek_ms = ms_since(t0);
        std::cout << "seek_to_5s_plus_frame_ms=" << seek_ms
                  << "  (pts=" << frame.value().pts_ticks << ")\n";

        const auto t1 = Clock::now();
        decoder->seek(0, playback::SeekMode::Exact);
        auto f0 = decoder->read_frame(token);
        std::cout << "seek_to_0_plus_frame_ms=" << ms_since(t1) << "\n";
    }

    // ---- 3. sequential throughput --------------------------------------------
    {
        auto decoder = factory.create_video_decoder().take_value();
        decoder->open(file.string());
        std::int64_t frames = 0;
        std::int64_t bytes = 0;
        const auto t0 = Clock::now();
        for (;;) {
            auto frame = decoder->read_frame(token);
            if (frame.is_error()) break;
            bytes += static_cast<std::int64_t>(frame.value().data->bytes.size());
            ++frames;
        }
        const double total_ms = ms_since(t0);
        std::cout << "sequential: frames=" << frames
                  << "  total_ms=" << total_ms
                  << "  fps=" << (total_ms > 0 ? frames * 1000.0 / total_ms : 0.0)
                  << "  rgba_mb=" << (bytes / 1.0e6) << "\n";
    }

    // ---- 4. audio throughput ---------------------------------------------------
    {
        auto decoder = factory.create_audio_decoder().take_value();
        decoder->open(file.string());
        std::int64_t samples = 0;
        std::int64_t blocks = 0;
        const auto t0 = Clock::now();
        for (;;) {
            auto block = decoder->read_block(token);
            if (block.is_error()) break;
            samples += block.value().frames;
            ++blocks;
        }
        const double total_ms = ms_since(t0);
        const auto& info = decoder->stream_info();
        std::cout << "audio: blocks=" << blocks
                  << "  samples=" << samples
                  << "  realtime_x=" << (total_ms > 0
                      ? (static_cast<double>(samples) / info.audio_sample_rate) /
                            (total_ms / 1000.0)
                      : 0.0)
                  << "  (" << info.audio_sample_rate << " Hz, "
                  << info.audio_channels << "ch)\n";
    }
    return 0;
}
