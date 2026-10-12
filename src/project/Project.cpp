// Veltrix Project - Project.cpp

#include "veltrix/project/Project.h"

#include "veltrix/core/MathUtils.h"

namespace veltrix::project {

using veltrix::core::ErrorCode;
using veltrix::core::failure;
using veltrix::core::ResultVoid;

Project Project::create(std::string name) {
    auto v = VideoSettings::create(1920, 1080, 30, 1);
    auto a = AudioSettings::create(48000, 2);
    Project p;
    p.id_ = veltrix::core::ProjectId::generate();
    p.name_ = std::move(name);
    if (v.is_ok()) p.video_ = v.take_value();
    if (a.is_ok()) p.audio_ = a.take_value();
    p.build_default_sequence();
    return p;
}

veltrix::core::Result<Project> Project::create_validated(
    std::string name, std::int32_t width, std::int32_t height,
    std::int64_t fps_num, std::int64_t fps_den) {
    auto v = VideoSettings::create(width, height, fps_num, fps_den);
    if (v.is_error())
        return veltrix::core::failure<Project>(v.error().code, v.error().message);
    auto a = AudioSettings::create(48000, 2);
    if (a.is_error())
        return veltrix::core::failure<Project>(a.error().code, a.error().message);
    Project p;
    p.id_ = veltrix::core::ProjectId::generate();
    p.name_ = std::move(name);
    p.video_ = v.take_value();
    p.audio_ = a.take_value();
    p.build_default_sequence();
    return veltrix::core::Result<Project>(std::move(p));
}

// A project ALWAYS carries a valid sequence (non-null id, valid timebase
// matching the project fps). Persistence and validation rely on it.
void Project::build_default_sequence() {
    auto tb = timeline::Timebase::from_fps(video_.fps.num(), video_.fps.den());
    if (tb.is_error())
        tb = timeline::Timebase::from_fps(30, 1); // defensive; fps is validated
    auto seq = timeline::Sequence::create("Main", tb.take_value(), 0);
    if (seq.is_ok()) sequence_ = seq.take_value();
}

ResultVoid Project::validate() const {
    if (name_.empty())
        return failure(ErrorCode::ValidationFailed, "project name is empty");
    if (video_.width <= 0 || video_.height <= 0)
        return failure(ErrorCode::ValidationFailed, "video resolution invalid");
    if (video_.fps.is_zero() || video_.fps.is_negative())
        return failure(ErrorCode::ValidationFailed, "video fps invalid");
    if (audio_.sample_rate <= 0 || audio_.channels <= 0)
        return failure(ErrorCode::ValidationFailed, "audio settings invalid");

    // Every clip must reference a registered media item.
    for (const auto& track : sequence_.tracks()) {
        for (const auto& clip : track.clips) {
            if (clip.media.is_null())
                return failure(ErrorCode::ValidationFailed,
                               "clip references null media", clip.id.to_string());
            if (!media_.find(clip.media))
                return failure(ErrorCode::ValidationFailed,
                               "clip references unregistered media",
                               clip.media.to_string());
            if (clip.duration <= 0)
                return failure(ErrorCode::ValidationFailed,
                               "clip duration must be > 0", clip.id.to_string());
            if (clip.start < 0)
                return failure(ErrorCode::ValidationFailed,
                               "clip start must be >= 0", clip.id.to_string());
        }
    }
    return veltrix::core::success();
}

} // namespace veltrix::project
