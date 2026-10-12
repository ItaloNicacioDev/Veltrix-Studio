// Veltrix Timeline - Track.h
//
// Ordered list of non-overlapping clips. Video/audio nature is a track
// property; ordering (z-order for video) is given by track order in the
// sequence.

#pragma once

#include <vector>

#include "veltrix/core/Id.h"
#include "veltrix/core/Result.h"
#include "veltrix/timeline/Clip.h"

namespace veltrix::timeline {

enum class TrackKind : std::uint8_t { Video = 0, Audio };

struct Track {
    veltrix::core::TrackId id;
    std::string name;
    TrackKind kind = TrackKind::Video;
    bool muted = false;
    bool locked = false;
    std::vector<Clip> clips; // kept sorted by start
};

} // namespace veltrix::timeline
