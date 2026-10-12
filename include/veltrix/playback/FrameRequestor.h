// Veltrix Playback - FrameRequestor.h
//
// Minimal preview-request API (Fase 2): the piece a future Qt Quick UI
// consumes for "frame at position" without knowing FFmpeg exists.
//
// Execution model (documented contract of THIS phase):
//   * request_frame() registers intent (monotonic id + cancellation
//     token) and cancels any previous request — the "user seeked again"
//     flow. It does NOT decode.
//   * await_frame() performs the decode on the CALLING thread, polling
//     the token between packets (seek + decode of one frame is fast and
//     latency-critical; queueing on the single job worker would only
//     add delay). All decoder resources are released before returning
//     (RAII scope) — destruction during in-flight decode cannot happen
//     by construction.
//   * Stale rejection: if the handle is no longer the latest request,
//     await_frame refuses with Conflict("superseded") WITHOUT decoding —
//     an old request can never overwrite a newer seek's result.
//   * For background use (thumbnails, batch), wrap a request in a
//     jobs::FunctionJob — the tokens compose naturally.
//   * Decoders are created per request; open cost is measured in the
//     benchmarks. A decoder cache is a later optimization behind this
//     same API.

#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <utility>

#include "veltrix/core/CancellationToken.h"
#include "veltrix/core/Id.h"
#include "veltrix/core/Result.h"
#include "veltrix/playback/Decoder.h"
#include "veltrix/playback/MediaFrames.h"
#include "veltrix/timeline/Time.h"

namespace veltrix::playback {

// Resolves a MediaId to a file path. Implemented over MediaRegistry.
class IMediaPathResolver {
public:
    virtual ~IMediaPathResolver() = default;
    virtual veltrix::core::Result<std::string> resolve(
        veltrix::core::MediaId id) const = 0;
};

struct FrameRequestHandle {
    std::uint64_t id = 0;
    std::shared_ptr<core::CancellationToken> token;
};

class FrameRequestor {
public:
    FrameRequestor(IDecoderFactory& factory, IMediaPathResolver& resolver)
        : factory_(factory), resolver_(resolver) {}

    // Registers a frame request at position `ticks` (ticks of the
    // MEDIA's own video timebase; timeline code converts before calling).
    // Cancels any previous request. Returns the handle.
    FrameRequestHandle request_frame(veltrix::core::MediaId media,
                                     timeline::EditTicks ticks);

    // Decodes on the calling thread and returns the RGBA8 frame.
    //   Conflict     — handle superseded by a newer request (stale)
    //   NotFound     — media unregistered / end-of-stream position
    //   Unsupported  — no decode engine / no video stream
    //   Cancelled    — token fired (including during decode)
    //   OutOfRange   — position beyond the media
    veltrix::core::Result<VideoFrame> await_frame(const FrameRequestHandle& handle);

    const char* engine_name() const { return factory_.engine_name(); }

    std::uint64_t total_requests() const {
        return total_requests_.load(std::memory_order_relaxed);
    }
    std::uint64_t stale_rejects() const {
        return stale_rejects_.load(std::memory_order_relaxed);
    }

private:
    IDecoderFactory& factory_;
    IMediaPathResolver& resolver_;
    std::atomic<std::uint64_t> next_request_id_{1};
    std::atomic<std::uint64_t> total_requests_{0};
    std::atomic<std::uint64_t> stale_rejects_{0};
    // Latest request. request_frame stores; await_frame compares. Mutex:
    // request and await may come from different threads (UI + job worker).
    std::mutex state_mutex_;
    std::uint64_t latest_id_ = 0;
    veltrix::core::MediaId latest_media_;
    timeline::EditTicks latest_ticks_ = 0;
    std::shared_ptr<core::CancellationToken> latest_token_;
};

} // namespace veltrix::playback
