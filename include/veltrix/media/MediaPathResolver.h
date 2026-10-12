// Veltrix Media - MediaPathResolver.h
//
// IMediaPathResolver over MediaRegistry: maps a MediaId to its file path.

#pragma once

#include <string>

#include "veltrix/core/Id.h"
#include "veltrix/core/Result.h"
#include "veltrix/media/MediaRegistry.h"
#include "veltrix/playback/FrameRequestor.h"

namespace veltrix::media {

class MediaPathResolver final : public veltrix::playback::IMediaPathResolver {
public:
    explicit MediaPathResolver(const MediaRegistry& registry) : registry_(registry) {}

    veltrix::core::Result<std::string> resolve(
        veltrix::core::MediaId id) const override {
        const MediaItem* item = registry_.find(id);
        if (!item)
            return veltrix::core::failure<std::string>(
                veltrix::core::ErrorCode::NotFound, "media not registered",
                id.to_string());
        if (item->missing)
            return veltrix::core::failure<std::string>(
                veltrix::core::ErrorCode::MediaUnavailable,
                "media file is missing on disk", item->path);
        return veltrix::core::Result<std::string>(item->path);
    }

private:
    const MediaRegistry& registry_;
};

} // namespace veltrix::media
