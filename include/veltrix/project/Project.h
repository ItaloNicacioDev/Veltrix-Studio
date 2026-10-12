// Veltrix Project - Project.h
//
// The editing document: settings, media registry, one sequence (multiple
// sequences are a documented future extension). The project is a value
// type; persistence lives in ProjectStore (JSON, schema_version).

#pragma once

#include <string>

#include "veltrix/core/Id.h"
#include "veltrix/media/MediaRegistry.h"
#include "veltrix/project/ProjectSettings.h"
#include "veltrix/timeline/Sequence.h"

namespace veltrix::project {

using veltrix::core::Result;
using veltrix::core::ResultVoid;

class Project {
public:
    Project() = default;

    // Creates a project with a generated id and default settings.
    static Project create(std::string name);
    static veltrix::core::Result<Project> create_validated(
        std::string name, std::int32_t width, std::int32_t height,
        std::int64_t fps_num, std::int64_t fps_den);

    // Deserialization-only: restores a persisted project id. Rejects null.
    void restore_id(veltrix::core::ProjectId id) {
        if (!id.is_null()) id_ = id;
    }

    const veltrix::core::ProjectId& id() const { return id_; }
    const std::string& name() const { return name_; }
    void set_name(std::string name) { name_ = std::move(name); }

    const VideoSettings& video() const { return video_; }
    VideoSettings& video() { return video_; }
    const AudioSettings& audio() const { return audio_; }
    AudioSettings& audio() { return audio_; }

    veltrix::media::MediaRegistry& media() { return media_; }
    const veltrix::media::MediaRegistry& media() const { return media_; }

    // The sequence document. A project owns exactly one sequence in this
    // stage (multi-sequence support is on the roadmap).
    veltrix::timeline::Sequence& sequence() { return sequence_; }
    const veltrix::timeline::Sequence& sequence() const { return sequence_; }
    void set_sequence(veltrix::timeline::Sequence s) { sequence_ = std::move(s); }

    // Structural validation (no disk IO): checks settings ranges, sequence
    // invariants and that every clip references a registered media item.
    veltrix::core::ResultVoid validate() const;

private:
    // Creates the default sequence matching the project video fps.
    // Called by both factories so a project is always complete.
    void build_default_sequence();
    veltrix::core::ProjectId id_;
    std::string name_;
    VideoSettings video_;
    AudioSettings audio_;
    veltrix::media::MediaRegistry media_;
    veltrix::timeline::Sequence sequence_;
};

} // namespace veltrix::project
