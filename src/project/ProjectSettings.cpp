// Veltrix Project - ProjectSettings.cpp

#include "veltrix/project/ProjectSettings.h"

#include <cmath>

namespace veltrix::project {

using veltrix::core::ErrorCode;
using veltrix::core::Result;
using veltrix::core::failure;

Result<VideoSettings> VideoSettings::create(std::int32_t width, std::int32_t height,
                                             std::int64_t fps_num, std::int64_t fps_den) {
    if (width <= 0 || height <= 0)
        return failure<VideoSettings>(ErrorCode::InvalidArgument, "resolution must be positive");
    if (width > 16384 || height > 16384)
        return failure<VideoSettings>(ErrorCode::OutOfRange, "resolution too large");
    auto fps = Rational::from(fps_num, fps_den);
    if (fps.is_error())
        return failure<VideoSettings>(fps.error().code, fps.error().message);
    if (fps.value().is_zero() || fps.value().is_negative())
        return failure<VideoSettings>(ErrorCode::InvalidArgument, "fps must be > 0");
    VideoSettings s;
    s.width = width;
    s.height = height;
    s.fps = fps.take_value();
    return Result<VideoSettings>(std::move(s));
}

Result<AudioSettings> AudioSettings::create(std::int32_t sample_rate, std::int32_t channels) {
    if (sample_rate <= 0)
        return failure<AudioSettings>(ErrorCode::InvalidArgument, "sample_rate must be > 0");
    if (sample_rate > 384000)
        return failure<AudioSettings>(ErrorCode::OutOfRange, "sample_rate too large");
    if (channels <= 0 || channels > 32)
        return failure<AudioSettings>(ErrorCode::InvalidArgument, "channels must be in [1, 32]");
    AudioSettings s;
    s.sample_rate = sample_rate;
    s.channels = channels;
    return Result<AudioSettings>(std::move(s));
}

} // namespace veltrix::project
