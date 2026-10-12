// Veltrix Media - MediaRegistry.h
//
// Registry of imported media items. Owns MediaItem records and enforces
// unique ids and unique resolved paths. File existence is checked lazily
// (status is a snapshot, not a live property).

#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "veltrix/core/Id.h"
#include "veltrix/core/Result.h"
#include "veltrix/media/MediaItem.h"

namespace veltrix::media {

using veltrix::core::Result;
using veltrix::core::ResultVoid;

class MediaRegistry {
public:
    // Registers a new media item. Fails if the path is already registered.
    Result<veltrix::core::MediaId> register_media(std::string absolute_path);
    // Re-registers a media item with a specific id (used by deserialization).
    // Fails on duplicate id or duplicate path.
    Result<veltrix::core::MediaId> register_media_with_id(veltrix::core::MediaId id,
                                                          std::string absolute_path);

    // Registers a fully-formed item during deserialization: no existence
    // check (offline projects may legitimately reference missing files),
    // but id and path uniqueness are still enforced. Availability is
    // recomputed afterwards via refresh_availability().
    ResultVoid load_item(MediaItem item);

    ResultVoid remove_media(veltrix::core::MediaId id);
    const MediaItem* find(veltrix::core::MediaId id) const;
    MediaItem* find(veltrix::core::MediaId id);

    const std::vector<MediaItem>& items() const { return items_; }
    std::size_t size() const { return items_.size(); }

    // Rechecks file existence for every item; updates `missing` flags.
    // Returns the count of missing files.
    std::size_t refresh_availability();

private:
    std::vector<MediaItem> items_;
    std::unordered_map<veltrix::core::MediaId, std::size_t> index_;  // id -> items_ slot
    std::unordered_map<std::string, veltrix::core::MediaId> by_path_;
};

} // namespace veltrix::media
