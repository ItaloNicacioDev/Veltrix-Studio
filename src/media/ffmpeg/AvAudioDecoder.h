// Veltrix FFmpeg layer - AvAudioDecoder.h (internal)

#pragma once

#include <string>

#include "veltrix/core/CancellationToken.h"
#include "veltrix/core/Result.h"
#include "veltrix/playback/Decoder.h"
#include "veltrix/playback/MediaFrames.h"

#include "AvGuards.h"

namespace veltrix::media::ffmpeg {

class AvAudioDecoder final : public veltrix::playback::IAudioDecoder {
public:
    AvAudioDecoder() = default;
    ~AvAudioDecoder() override { close(); }

    veltrix::core::ResultVoid open(const std::string& path) override;
    bool is_open() const override { return format_.ctx != nullptr; }
    const veltrix::playback::MediaStreamInfo& stream_info() const override {
        return info_;
    }
    veltrix::core::ResultVoid seek(std::int64_t target_ticks,
                                   veltrix::playback::SeekMode mode) override;
    veltrix::core::Result<veltrix::playback::AudioBlock> read_block(
        const veltrix::core::CancellationToken& token) override;
    std::int64_t frames_decoded() const override { return frames_decoded_; }
    void close() override;

private:
    veltrix::core::ResultVoid setup_audio_pipeline();
    // Converts one decoded AVFrame to a planar-float32 AudioBlock.
    veltrix::core::Result<veltrix::playback::AudioBlock> convert_to_planar_f32(
        const AVFrame& av_frame);

    FormatCtxGuard format_;
    CodecCtxGuard codec_;
    FrameGuard frame_;
    PacketGuard packet_;
    SwrGuard swr_;
    int audio_stream_index_ = -1;
    bool eof_reached_ = false;
    bool end_of_stream_signaled_ = false;
    std::int64_t frames_decoded_ = 0;
    veltrix::playback::MediaStreamInfo info_;
};

} // namespace veltrix::media::ffmpeg
