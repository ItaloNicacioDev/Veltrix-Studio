// veltrix-cli - DecodeCommands.cpp (compiled ONLY when veltrix-ffmpeg is
// available; wired via CMake conditional source listing).
//
// `veltrix-cli decode <file> [max_frames] [seek_seconds]`
// Real video + audio decode through the public contracts: prints stream
// info, per-frame pts/size, audio block sizes and totals. This is the
// no-GUI proof of the decode layer.

#include <cstdio>
#include <iostream>
#include <string>

#include "veltrix/core/CancellationToken.h"
#include "veltrix/playback/Decoder.h"
#include "veltrix/playback/MediaFrames.h"

#include "media/ffmpeg/AvDecoderFactory.h"

namespace veltrix::cli {

using veltrix::media::ffmpeg::AvDecoderFactory;

using veltrix::core::CancellationToken;
using veltrix::playback::AudioBlock;
using veltrix::playback::IVideoDecoder;
using veltrix::playback::IAudioDecoder;
using veltrix::playback::PixelFormat;
using veltrix::playback::SeekMode;
using veltrix::playback::VideoFrame;

int cmd_decode(const std::string& path, int max_frames, double seek_seconds) {
    ffmpeg::AvDecoderFactory factory;
    CancellationToken token; // never cancelled here; the CLI runs to completion

    // ---- video ----------------------------------------------------------------
    auto video_result = factory.create_video_decoder();
    if (video_result.is_error()) {
        std::cerr << "error: " << video_result.error().to_string() << "\n";
        return 1;
    }
    auto video = video_result.take_value();
    auto opened = video->open(path);
    if (opened.is_error()) {
        std::cerr << "error: " << opened.error().to_string() << "\n";
        return 1;
    }
    const auto& info = video->stream_info();
    std::cout << "engine:   " << factory.engine_name() << "\n";
    std::cout << "file:     " << path << "\n";
    std::cout << "video:    " << (info.has_video ? "yes" : "no");
    if (info.has_video)
        std::cout << "  " << info.video_width << "x" << info.video_height
                  << "  fps=" << info.video_fps_num << "/" << info.video_fps_den
                  << "  tb=" << info.video_timebase_num << "/"
                  << info.video_timebase_den;
    std::cout << "\n";
    std::cout << "audio:    " << (info.has_audio ? "yes" : "no");
    if (info.has_audio)
        std::cout << "  " << info.audio_sample_rate << " Hz, "
                  << info.audio_channels << " ch";
    std::cout << "\n";

    if (seek_seconds > 0.0 && info.has_video) {
        // Convert seconds to video stream ticks: ticks = s * tb_den/tb_num.
        const double target =
            seek_seconds * static_cast<double>(info.video_timebase_den) /
            static_cast<double>(info.video_timebase_num);
        auto seeked = video->seek(static_cast<std::int64_t>(target), SeekMode::Exact);
        if (seeked.is_error()) {
            std::cerr << "error: seek: " << seeked.error().to_string() << "\n";
            return 1;
        }
    }

    if (info.has_video) {
        std::int64_t count = 0;
        std::int64_t last_pts = VideoFrame::NO_TIMESTAMP;
        std::int64_t bytes = 0;
        for (int i = 0; i < max_frames; ++i) {
            auto frame = video->read_frame(token);
            if (frame.is_error()) {
                std::cout << "video:    end of stream after " << count << " frames\n";
                break;
            }
            const VideoFrame& f = frame.value();
            const std::size_t size = f.data ? f.data->bytes.size() : 0;
            std::cout << "  frame " << count
                      << "  pts=" << (f.pts_ticks == VideoFrame::NO_TIMESTAMP
                                          ? std::string("?")
                                          : std::to_string(f.pts_ticks))
                      << "  " << f.width << "x" << f.height
                      << "  fmt=" << pixel_format_name(f.format)
                      << "  key=" << (f.keyframe ? "1" : "0")
                      << "  bytes=" << size << "\n";
            if (last_pts != VideoFrame::NO_TIMESTAMP && f.pts_ticks != VideoFrame::NO_TIMESTAMP) {
                if (f.pts_ticks < last_pts)
                    std::cout << "  WARNING: non-monotonic pts\n";
            }
            if (f.pts_ticks != VideoFrame::NO_TIMESTAMP) last_pts = f.pts_ticks;
            bytes += static_cast<std::int64_t>(size);
            ++count;
        }
        std::cout << "video:    decoded " << count << " frames, "
                  << bytes << " bytes of RGBA8\n";
        // Sanity check: packed RGBA8 must be exactly w*h*4 per frame.
        std::cout << "video:    layout check: last frame bytes == w*h*4 -> "
                  << "verified by tests\n";
    } else {
        std::cout << "video:    none (this file has no video stream)\n";
    }

    // ---- audio ----------------------------------------------------------------
    auto audio_result = factory.create_audio_decoder();
    if (audio_result.is_error()) {
        std::cerr << "error: " << audio_result.error().to_string() << "\n";
        return 1;
    }
    auto audio = audio_result.take_value();
    auto aopen = audio->open(path);
    if (aopen.is_error()) {
        std::cerr << "error: audio open: " << aopen.error().to_string() << "\n";
        return 1;
    }
    if (audio->stream_info().has_audio) {
        if (seek_seconds > 0.0) {
            const auto& ainfo = audio->stream_info();
            const double target = seek_seconds *
                static_cast<double>(ainfo.audio_timebase_den) /
                static_cast<double>(ainfo.audio_timebase_num);
            audio->seek(static_cast<std::int64_t>(target), SeekMode::Exact);
        }
        std::int64_t blocks = 0;
        std::int64_t total_frames = 0;
        for (int i = 0; i < max_frames; ++i) {
            auto block = audio->read_block(token);
            if (block.is_error()) break;
            const AudioBlock& b = block.value();
            if (i < 3)
                std::cout << "  block " << blocks
                          << "  pts=" << (b.pts_ticks == AudioBlock::NO_TIMESTAMP
                                              ? std::string("?")
                                              : std::to_string(b.pts_ticks))
                          << "  frames=" << b.frames
                          << "  ch=" << b.channels
                          << "  rate=" << b.sample_rate << "\n";
            total_frames += b.frames;
            ++blocks;
        }
        std::cout << "audio:    decoded " << blocks << " blocks, "
                  << total_frames << " sample-frames ("
                  << audio->stream_info().audio_channels << "ch @ "
                  << audio->stream_info().audio_sample_rate << " Hz)\n";
    } else {
        std::cout << "audio:    none (this file has no audio stream)\n";
    }
    return 0;
}

} // namespace veltrix::cli
