// Veltrix Playback - MediaFrames.h
//
// Public value types for decoded media. Core-owned formats — NO FFmpeg
// types leak here (ADR-008: AV* stays inside src/media/ffmpeg/).
//
// Ownership: every buffer is a std::shared_ptr<FrameBuffer> — decode
// results may be cached/shared; nobody mutates a frame after decode.

#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "veltrix/core/Result.h"

namespace veltrix::playback {

// Reference-counted byte buffer. Data is owned; alignment >= the decode
// requirement (32 bytes on x86 SIMD paths).
struct FrameBuffer {
    std::vector<std::uint8_t> bytes;
};

// Pixel formats the core understands TODAY. Deliberately minimal: only
// formats with a verified, tested conversion path exist here. Anything
// else is converted at the ffmpeg layer to RGBA8 (or refused, with an
// explicit Unsupported error, when conversion is not implemented).
enum class PixelFormat : std::uint8_t {
    Unknown = 0,
    R8G8B8A8_Unorm, // 4 bytes/pixel, single packed plane, row-major.
                    // Stride = width * 4 ALWAYS in this format (packed).
};

inline const char* pixel_format_name(PixelFormat f) {
    switch (f) {
        case PixelFormat::R8G8B8A8_Unorm: return "rgba8";
        case PixelFormat::Unknown: return "unknown";
    }
    return "?";
}

// One decoded video frame. Layout contract:
//   * format: R8G8B8A8_Unorm
//   * memory: width * height * 4 bytes, packed (stride == width*4)
//   * data: shared ownership; valid while any shared_ptr lives
//   * pts_ticks: presentation timestamp in the DECODER timebase
//     (timebase_num/timebase_den below) — the caller converts to sequence
//     ticks. NO_TIMESTAMP means "unknown/missing" (explicit).
struct VideoFrame {
    static constexpr std::int64_t NO_TIMESTAMP = INT64_MIN;

    std::shared_ptr<FrameBuffer> data;
    std::int32_t width = 0;
    std::int32_t height = 0;
    PixelFormat format = PixelFormat::Unknown;
    std::int64_t pts_ticks = NO_TIMESTAMP;
    std::int64_t duration_ticks = 0; // best-effort; 0 when unknown
    bool keyframe = false;
};

// Audio: deinterleaved planar float32 (matches veltrix::audio::AudioBuffer
// conventions). One AudioBlock = decoded chunk of variable size.
struct AudioBlock {
    static constexpr std::int64_t NO_TIMESTAMP = INT64_MIN;

    std::shared_ptr<FrameBuffer> data; // frames_per_channel * channels * sizeof(float)
    std::int32_t channels = 0;
    std::int32_t sample_rate = 0;
    std::int64_t frames = 0;           // frames per channel
    std::int64_t pts_ticks = NO_TIMESTAMP; // in 1/sample_rate "timebase"
    std::int64_t duration_ticks = 0;   // == frames when sample_rate known
};

// A media file's decode-relevant description, obtained by opening it.
struct MediaStreamInfo {
    // Video (valid when has_video)
    bool has_video = false;
    std::int32_t video_width = 0;
    std::int32_t video_height = 0;
    std::int64_t video_fps_num = 0;   // 0 = unknown/variable
    std::int64_t video_fps_den = 0;
    std::int64_t video_timebase_num = 0; // stream timebase for pts_ticks
    std::int64_t video_timebase_den = 0;

    // Audio (valid when has_audio)
    bool has_audio = false;
    std::int32_t audio_channels = 0;
    std::int32_t audio_sample_rate = 0;
    std::int64_t audio_timebase_num = 0;  // == 1/sample_rate
    std::int64_t audio_timebase_den = 0;

    // Container duration in timebase of the stream it came from; 0 = unknown.
    std::int64_t duration_ticks = 0;
    std::int64_t duration_timebase_num = 0;
    std::int64_t duration_timebase_den = 0;
};

} // namespace veltrix::playback
