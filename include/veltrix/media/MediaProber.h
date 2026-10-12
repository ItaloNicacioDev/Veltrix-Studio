// Veltrix Media - MediaProber.h
//
// Probes technical metadata by locating ffprobe at RUNTIME (PATH or
// VELTRIX_FFPROBE env var, or explicit path). The core does NOT link
// libav* at this stage (ADR-006): this keeps the core license-clean
// (LGPL/GPL questions deferred) and the build dependency-free.
//
// Failure modes are explicit: binary missing, timeout, unparsable output,
// unsupported stream layout.

#pragma once

#include <string>

#include "veltrix/core/CancellationToken.h"
#include "veltrix/core/Result.h"
#include "veltrix/media/MediaItem.h"

namespace veltrix::media {

class MediaProber {
public:
    // Looks for ffprobe: explicit path > VELTRIX_FFPROBE env > PATH.
    // Windows appends ".exe" when the plain name is not found.
    static veltrix::core::Result<std::string> locate_ffprobe(const std::string& explicit_path);

    // Runs ffprobe -show_streams -show_format and fills `item.streams`
    // and `item.duration_us`. Id/path/kind are not touched.
    // `timeout_seconds` bounds the whole execution (0 = no timeout).
    static veltrix::core::ResultVoid probe(MediaItem& item,
                                           const std::string& ffprobe_path,
                                           int timeout_seconds);
};

} // namespace veltrix::media
