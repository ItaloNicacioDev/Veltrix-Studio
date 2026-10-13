// Veltrix Media - MediaItem.h
//
// A media item is a PERSISTENT reference to an imported file. It stores
// only durable data (path, kind, technical metadata). Transient processing
// state (decoders, cached frames) lives in playback/render services and
// is never serialized.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "veltrix/core/Id.h"

namespace veltrix::media {

enum class MediaKind : std::uint8_t {
    Unknown = 0,
    Video,
    Audio,
    Image,
    ImageSequence,
};

inline const char* media_kind_name(MediaKind k) {
    switch (k) {
        case MediaKind::Unknown:        return "unknown";
        case MediaKind::Video:          return "video";
        case MediaKind::Audio:          return "audio";
        case MediaKind::Image:          return "image";
        case MediaKind::ImageSequence:  return "image_sequence";
    }
    return "?";
}

// One stream inside a media file (video or audio), as reported by probing.
struct StreamInfo {
    std::int32_t index = -1;
    std::string codec_name;
    // Video:
    std::int32_t width = 0;
    std::int32_t height = 0;
    // Audio:
    std::int32_t sample_rate = 0;
    std::int32_t channels = 0;
    // Exact frame rate / sample duration as reported by the container.
    std::int64_t fps_num = 0;
    std::int64_t fps_den = 0;
    // Stream timebase (e.g. "1/12288"): positions in the DECODER are in
    // these units — seconds = ticks * tb_num / tb_den.
    std::int64_t timebase_num = 0;
    std::int64_t timebase_den = 0;
    std::int64_t duration_us = 0; // microseconds when known; 0 = unknown
};

struct MediaItem {
    veltrix::core::MediaId id;
    std::string path;            // absolute path at import time
    std::string display_name;    // file name without directory
    MediaKind kind = MediaKind::Unknown;

    // Availability snapshot (see MediaRegistry::refresh_availability).
    bool missing = false;

    // Probed technical metadata; empty when probing has not run/failed.
    std::vector<StreamInfo> streams;
    std::int64_t duration_us = 0; // container duration; 0 = unknown
};

} // namespace veltrix::media
