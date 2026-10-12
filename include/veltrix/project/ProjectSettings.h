// Veltrix Project - ProjectSettings.h
//
// Project-level audio/video configuration: resolution, frame rate (exact
// rational), audio sample rate and channel count.

#pragma once

#include <cstdint>

#include "veltrix/timeline/Time.h"

namespace veltrix::project {

using veltrix::timeline::Rational;

struct VideoSettings {
    std::int32_t width = 1920;
    std::int32_t height = 1080;
    Rational fps = Rational::one(); // exact; e.g. NTSC = 30000/1001
    bool progressive = true;

    static veltrix::core::Result<VideoSettings> create(std::int32_t width,
                                                       std::int32_t height,
                                                       std::int64_t fps_num,
                                                       std::int64_t fps_den);
};

struct AudioSettings {
    std::int32_t sample_rate = 48000;
    std::int32_t channels = 2;

    static veltrix::core::Result<AudioSettings> create(std::int32_t sample_rate,
                                                        std::int32_t channels);
};

} // namespace veltrix::project
