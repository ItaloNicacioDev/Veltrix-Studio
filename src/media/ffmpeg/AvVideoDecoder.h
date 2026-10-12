// Veltrix FFmpeg layer - AvVideoDecoder.h (internal)
//
// IVideoDecoder over libavformat/libavcodec/libswscale. Produces packed
// RGBA8 frames (the one format with a verified conversion path — ADR-008;
// sws_scale handles any libav pixel format we can open).

#pragma once

#include <memory>
#include <string>

#include "veltrix/core/CancellationToken.h"
#include "veltrix/core/Result.h"
#include "veltrix/playback/Decoder.h"
#include "veltrix/playback/MediaFrames.h"

#include "AvGuards.h"

namespace veltrix::media::ffmpeg {

class AvVideoDecoder final : public veltrix::playback::IVideoDecoder {
public:
    AvVideoDecoder() = default;
    ~AvVideoDecoder() override { close(); }

    // IVideoDecoder
    veltrix::core::ResultVoid open(const std::string& path) override;
    bool is_open() const override { return format_.ctx != nullptr; }
    const veltrix::playback::MediaStreamInfo& stream_info() const override {
        return info_;
    }
    veltrix::core::ResultVoid seek(std::int64_t target_ticks,
                                   veltrix::playback::SeekMode mode) override;
    veltrix::core::Result<veltrix::playback::VideoFrame> read_frame(
        const veltrix::core::CancellationToken& token) override;
    std::int64_t frames_decoded() const override { return frames_decoded_; }
    void close() override;

private:
    // One-time setup after open(): finds the best video stream, sets up
    // the codec context and the RGBA conversion.
    veltrix::core::ResultVoid setup_video_pipeline();
    // Converts an decoded AVFrame (native pix fmt) to a packed RGBA8
    // VideoFrame. Uses the frame's OWN width/height (containers may
    // change size mid-stream; the caller is told per-frame).
    veltrix::core::Result<veltrix::playback::VideoFrame> convert_to_rgba(
        const AVFrame& av_frame);

    FormatCtxGuard format_;
    CodecCtxGuard codec_;
    FrameGuard frame_;
    PacketGuard packet_;
    SwsGuard sws_;
    int video_stream_index_ = -1;
    bool eof_reached_ = false;
    bool end_of_stream_signaled_ = false;
    std::int64_t frames_decoded_ = 0;
    // Exact-seek forward decode: after seek(Exact), read_frame discards
    // frames until pts >= this target (set to -1 once satisfied or when
    // not in Exact mode). Cancellation still polls per packet.
    std::int64_t exact_target_ticks_ = -1;
    std::int32_t sws_src_width_ = 0;   // sws is rebuilt if the frame size changes
    std::int32_t sws_src_height_ = 0;
    AVPixelFormat sws_src_format_ = AV_PIX_FMT_NONE;
    veltrix::playback::MediaStreamInfo info_;
};

} // namespace veltrix::media::ffmpeg
