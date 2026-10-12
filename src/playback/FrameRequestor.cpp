// Veltrix Playback - FrameRequestor.cpp

#include "veltrix/playback/FrameRequestor.h"

namespace veltrix::playback {

using veltrix::core::ErrorCode;
using veltrix::core::failure;
using veltrix::core::Result;

FrameRequestHandle FrameRequestor::request_frame(veltrix::core::MediaId media,
                                                timeline::EditTicks ticks) {
    FrameRequestHandle handle;
    handle.id = next_request_id_.fetch_add(1, std::memory_order_relaxed);
    handle.token = std::make_shared<veltrix::core::CancellationToken>();
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        // Cancel any previous request: its await will observe the fired
        // token (mid-decode) or the stale id (before decode starts).
        if (latest_token_) latest_token_->cancel();
        latest_id_ = handle.id;
        latest_media_ = media;
        latest_ticks_ = ticks;
        latest_token_ = handle.token;
    }
    total_requests_.fetch_add(1, std::memory_order_relaxed);
    return handle;
}

Result<VideoFrame> FrameRequestor::await_frame(const FrameRequestHandle& handle) {
    if (!handle.token)
        return failure<VideoFrame>(ErrorCode::InvalidArgument, "null handle");
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (handle.id != latest_id_) {
            stale_rejects_.fetch_add(1, std::memory_order_relaxed);
            return failure<VideoFrame>(ErrorCode::Conflict, "superseded by newer request");
        }
    }

    if (handle.token->is_cancelled())
        return failure<VideoFrame>(ErrorCode::Cancelled, "cancelled before decode");

    // Resolve the media file path.
    auto path = resolver_.resolve(latest_media_);
    if (path.is_error())
        return failure<VideoFrame>(path.error().code, path.error().message);

    // Create the decoder for this request (per-request lifetime; RAII).
    auto decoder_result = factory_.create_video_decoder();
    if (decoder_result.is_error())
        return failure<VideoFrame>(decoder_result.error().code,
                                    decoder_result.error().message);
    auto decoder = decoder_result.take_value();

    auto opened = decoder->open(path.value());
    if (opened.is_error())
        return failure<VideoFrame>(opened.error().code, opened.error().message);

    // Seek in the decoder's own timebase; caller passed media ticks.
    auto seeked = decoder->seek(latest_ticks_, SeekMode::Exact);
    if (seeked.is_error())
        return failure<VideoFrame>(seeked.error().code, seeked.error().message);

    // Decode forward to the frame AT the target (>= target).
    const auto& info = decoder->stream_info();
    bool got_frame = false;
    VideoFrame result;
    while (!got_frame) {
        if (handle.token->is_cancelled())
            return failure<VideoFrame>(ErrorCode::Cancelled, "cancelled during decode");
        auto frame = decoder->read_frame(*handle.token);
        if (frame.is_error()) {
            const auto& err = frame.error();
            if (err.code == ErrorCode::NotFound &&
                err.message.find("end of stream") != std::string::npos) {
                // Position at/after the last frame: report OutOfRange, not
                // a silent EOS loop.
                return failure<VideoFrame>(ErrorCode::OutOfRange,
                                           "position beyond last frame");
            }
            return failure<VideoFrame>(err.code, err.message);
        }
        VideoFrame f = frame.take_value();
        if (f.pts_ticks == VideoFrame::NO_TIMESTAMP) {
            // Missing PTS: cannot prove the frame matches the target —
            // accept the first frame after a fresh seek (documented
            // behavior; irregular-timestamp files are surfaced, not
            // guessed... but we must return SOMETHING after seek).
            if (decoder->frames_decoded() == 1) {
                result = std::move(f);
                got_frame = true;
            }
            continue;
        }
        if (f.pts_ticks >= latest_ticks_) {
            result = std::move(f);
            got_frame = true;
        }
        // else: frame before the target (keyframe landing) — keep decoding.
    }
    return Result<VideoFrame>(std::move(result));
}

} // namespace veltrix::playback
