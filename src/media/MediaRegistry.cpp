// Veltrix Media - MediaRegistry.cpp

#include "veltrix/media/MediaRegistry.h"

#include <algorithm>
#include <filesystem>
#include <system_error>

namespace veltrix::media {

using veltrix::core::ErrorCode;
using veltrix::core::failure;
using veltrix::core::ResultVoid;

namespace {
bool file_exists(const std::string& path) {
    if (path.empty()) return false;
    std::error_code ec;
    return std::filesystem::exists(std::filesystem::u8path(path), ec) && !ec;
}
} // namespace

Result<veltrix::core::MediaId> MediaRegistry::register_media(std::string absolute_path) {
    return register_media_with_id(veltrix::core::MediaId::generate(), std::move(absolute_path));
}

Result<veltrix::core::MediaId> MediaRegistry::register_media_with_id(
    veltrix::core::MediaId id, std::string absolute_path) {
    if (id.is_null())
        return failure<veltrix::core::MediaId>(ErrorCode::InvalidArgument, "media id is null");
    if (absolute_path.empty())
        return failure<veltrix::core::MediaId>(ErrorCode::InvalidArgument, "media path is empty");
    if (index_.count(id))
        return failure<veltrix::core::MediaId>(ErrorCode::AlreadyExists,
                                               "media id already registered", id.to_string());
    if (by_path_.count(absolute_path))
        return failure<veltrix::core::MediaId>(ErrorCode::AlreadyExists,
                                               "path already registered", absolute_path);
    if (!file_exists(absolute_path))
        return failure<veltrix::core::MediaId>(ErrorCode::MediaUnavailable,
                                               "media file does not exist", absolute_path);

    MediaItem item;
    item.id = id;
    item.path = std::move(absolute_path);
    item.display_name = std::filesystem::u8path(item.path).filename().string();
    items_.push_back(std::move(item));
    index_[id] = items_.size() - 1;
    by_path_[items_.back().path] = id;
    return Result<veltrix::core::MediaId>(id);
}

ResultVoid MediaRegistry::load_item(MediaItem item) {
    if (item.id.is_null())
        return failure(ErrorCode::InvalidArgument, "media id is null");
    if (item.path.empty())
        return failure(ErrorCode::InvalidArgument, "media path is empty");
    if (index_.count(item.id))
        return failure(ErrorCode::AlreadyExists, "media id already registered",
                       item.id.to_string());
    if (by_path_.count(item.path))
        return failure(ErrorCode::AlreadyExists, "path already registered", item.path);
    item.missing = false; // snapshot: recomputed by refresh_availability()
    const std::string path = item.path;
    items_.push_back(std::move(item));
    index_[items_.back().id] = items_.size() - 1;
    by_path_[path] = items_.back().id;
    return veltrix::core::success();
}

ResultVoid MediaRegistry::remove_media(veltrix::core::MediaId id) {
    auto it = index_.find(id);
    if (it == index_.end())
        return failure(ErrorCode::NotFound, "media not found", id.to_string());
    const std::string path = items_[it->second].path;
    items_.erase(items_.begin() + static_cast<std::ptrdiff_t>(it->second));
    by_path_.erase(path);
    // Rebuild the index (compact vector, cheap at foundation scale).
    index_.clear();
    for (std::size_t i = 0; i < items_.size(); ++i) index_[items_[i].id] = i;
    return veltrix::core::success();
}

const MediaItem* MediaRegistry::find(veltrix::core::MediaId id) const {
    auto it = index_.find(id);
    return it == index_.end() ? nullptr : &items_[it->second];
}

MediaItem* MediaRegistry::find(veltrix::core::MediaId id) {
    auto it = index_.find(id);
    return it == index_.end() ? nullptr : &items_[it->second];
}

std::size_t MediaRegistry::refresh_availability() {
    std::size_t missing = 0;
    for (MediaItem& item : items_) {
        item.missing = !file_exists(item.path);
        if (item.missing) ++missing;
    }
    by_path_; // (keeps path->id map consistent; no dup paths can appear)
    return missing;
}

} // namespace veltrix::media
