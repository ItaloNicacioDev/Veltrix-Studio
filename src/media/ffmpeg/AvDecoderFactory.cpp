// Veltrix FFmpeg layer - AvDecoderFactory.cpp

#include "AvDecoderFactory.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
}

#include "AvAudioDecoder.h"
#include "AvVideoDecoder.h"

namespace veltrix::media::ffmpeg {

using veltrix::core::ErrorCode;
using veltrix::core::failure;
using veltrix::core::Result;

Result<std::unique_ptr<veltrix::playback::IVideoDecoder>>
AvDecoderFactory::create_video_decoder() {
    return Result<std::unique_ptr<veltrix::playback::IVideoDecoder>>(
        std::make_unique<AvVideoDecoder>());
}

Result<std::unique_ptr<veltrix::playback::IAudioDecoder>>
AvDecoderFactory::create_audio_decoder() {
    return Result<std::unique_ptr<veltrix::playback::IAudioDecoder>>(
        std::make_unique<AvAudioDecoder>());
}

const char* AvDecoderFactory::engine_name() const {
    if (engine_name_.empty()) {
        engine_name_ = std::string("libav ") +
            "format=" + av_version_info() +
            " (avformat " + std::to_string(avformat_version()) +
            ", avcodec " + std::to_string(avcodec_version()) +
            ", avutil " + std::to_string(avutil_version()) + ")";
    }
    return engine_name_.c_str();
}

} // namespace veltrix::media::ffmpeg
