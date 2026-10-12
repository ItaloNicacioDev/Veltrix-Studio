// Veltrix FFmpeg layer - AvDecoderFactory.h (internal)
//
// IDecoderFactory implementation when the FFmpeg SDK was found at
// configure time. engine_name() reports the linked libav* versions.

#pragma once

#include <memory>
#include <string>

#include "veltrix/playback/Decoder.h"

namespace veltrix::media::ffmpeg {

class AvDecoderFactory final : public veltrix::playback::IDecoderFactory {
public:
    veltrix::core::Result<std::unique_ptr<veltrix::playback::IVideoDecoder>>
    create_video_decoder() override;
    veltrix::core::Result<std::unique_ptr<veltrix::playback::IAudioDecoder>>
    create_audio_decoder() override;
    const char* engine_name() const override;

private:
    std::string engine_name_; // "libav format=.. codec=.. util=.."
};

} // namespace veltrix::media::ffmpeg
