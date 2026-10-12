// Veltrix Playback - Decoder.h
//
// Public decode contracts (REVISED in Fase 2 — replaces the Phase-1 stub;
// the stub was never consumed by any code or test).
//
// Two focused interfaces instead of one mixed one:
//   * IVideoDecoder — video frames with seek
//   * IAudioDecoder — audio blocks with seek
// Plus IVideoFrameRequestor (FrameRequestor.h): media-id + position ->
// frame, the minimal API a future UI consumes for preview.
//
// Error semantics (REUSED from core, see Result.h):
//   NotFound            — file missing/unreadable OR end of stream
//   Unsupported         — no (video|audio) stream, unknown codec
//   ParseError          — file opens but is corrupt mid-stream
//   OutOfRange          — seek target beyond the end (not clamped)
//   Cancelled           — token fired before completion
// End of stream is SIGNALED AS failure(NotFound, "end of stream"):
// documented convention, no exceptions, callers distinguish by state.

#pragma once

#include <memory>
#include <string>

#include "veltrix/core/CancellationToken.h"
#include "veltrix/core/Result.h"
#include "veltrix/playback/MediaFrames.h"

namespace veltrix::playback {

enum class SeekMode : std::uint8_t {
    Exact,    // decode forward to the frame at/just-after the target
    Keyframe,  // jump to the previous keyframe (fast preview path)
};

// ---- video -----------------------------------------------------------------

class IVideoDecoder {
public:
    virtual ~IVideoDecoder() = default;

    // Opens the file and reads the stream layout. A second open fails
    // with Conflict. Does not decode frames.
    virtual veltrix::core::ResultVoid open(const std::string& path) = 0;
    virtual bool is_open() const = 0;

    // Stream description discovered at open().
    virtual const MediaStreamInfo& stream_info() const = 0;

    // Positions the decoder. `target_ticks` is in the VIDEO stream
    // timebase. Exact decodes forward to the target frame; Keyframe lands
    // on the previous keyframe. Seeking beyond the end fails OutOfRange.
    virtual veltrix::core::ResultVoid seek(std::int64_t target_ticks,
                                           SeekMode mode) = 0;

    // Decodes the next frame in presentation order. End of stream:
    // failure(NotFound, "end of stream"). Cancellation: token polled
    // between packets; aborts return failure(Cancelled,...) and leave the
    // decoder usable (close() still required once).
    virtual veltrix::core::Result<VideoFrame> read_frame(
        const veltrix::core::CancellationToken& token) = 0;

    virtual std::int64_t frames_decoded() const = 0;
    virtual void close() = 0;
};

// ---- audio -----------------------------------------------------------------

class IAudioDecoder {
public:
    virtual ~IAudioDecoder() = default;

    virtual veltrix::core::ResultVoid open(const std::string& path) = 0;
    virtual bool is_open() const = 0;
    virtual const MediaStreamInfo& stream_info() const = 0;

    // target_ticks in the AUDIO stream timebase (1/sample_rate).
    virtual veltrix::core::ResultVoid seek(std::int64_t target_ticks,
                                            SeekMode mode) = 0;

    // Decodes the next block (variable size, planar float32).
    // End of stream: failure(NotFound, "end of stream").
    virtual veltrix::core::Result<AudioBlock> read_block(
        const veltrix::core::CancellationToken& token) = 0;

    virtual std::int64_t frames_decoded() const = 0;
    virtual void close() = 0;
};

// ---- factory ----------------------------------------------------------------

// Real decoders when the engine is available (veltrix-ffmpeg).
// failure(Unsupported, ...) when the build has no decode engine — callers
// degrade to "preview unavailable", never crash.
class IDecoderFactory {
public:
    virtual ~IDecoderFactory() = default;
    virtual veltrix::core::Result<std::unique_ptr<IVideoDecoder>>
    create_video_decoder() = 0;
    virtual veltrix::core::Result<std::unique_ptr<IAudioDecoder>>
    create_audio_decoder() = 0;
    // Human-readable engine id for diagnostics ("libav 63.x.y" or "none").
    virtual const char* engine_name() const = 0;
};

} // namespace veltrix::playback
