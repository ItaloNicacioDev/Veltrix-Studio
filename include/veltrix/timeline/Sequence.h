// Veltrix Timeline - Sequence.h
//
// A sequence owns tracks and defines the temporal interpretation:
//   * timebase  : seconds-per-tick (one tick == one frame at the sequence fps)
//   * duration  : explicit sequence length in ticks (may exceed last clip end)
//
// The sequence is a plain value type; editing operations live in
// TimelineEditor (validated, undoable) and queries in TimelineQuery.
// Sequence-level operations (add/remove track) return Result and keep
// the object consistent on failure.

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "veltrix/core/Id.h"
#include "veltrix/core/Result.h"
#include "veltrix/timeline/Time.h"
#include "veltrix/timeline/Track.h"

namespace veltrix::timeline {

using veltrix::core::Result;
using veltrix::core::ResultVoid;

class Sequence {
public:
    Sequence() = default;

    static Result<Sequence> create(std::string name, const Timebase& timebase,
                                   EditTicks duration);
    // Same as create(), but restores a previously persisted id. Used by
    // deserialization only; rejects null ids.
    static Result<Sequence> create_with_id(veltrix::core::SequenceId id,
                                           std::string name, const Timebase& timebase,
                                           EditTicks duration);

    // ---- identity / metadata ------------------------------------------------
    const veltrix::core::SequenceId& id() const { return id_; }
    const std::string& name() const { return name_; }
    void set_name(std::string name) { name_ = std::move(name); }
    const Timebase& timebase() const { return timebase_; }

    // ---- tracks ---------------------------------------------------------------
    Result<veltrix::core::TrackId> add_track(TrackKind kind, std::string name);
    ResultVoid remove_track(veltrix::core::TrackId track);
    ResultVoid rename_track(veltrix::core::TrackId track, std::string name);
    const Track* find_track(veltrix::core::TrackId track) const;
    Track* find_track(veltrix::core::TrackId track);

    const std::vector<Track>& tracks() const { return tracks_; }

    // ---- duration --------------------------------------------------------------
    EditTicks duration() const;
    // The explicitly stored duration (may be less than content extent).
    EditTicks explicit_duration() const { return explicit_duration_; }
    ResultVoid set_duration(EditTicks ticks);
    // Duration implied by clip placement (max clip end across tracks).
    EditTicks content_duration() const;

    // ---- clips (structural ops are in TimelineEditor; these are the raw
    //      primitives used by the editor and by serialization) -----------------
    // Insert validating non-overlap; keeps clips sorted by start.
    ResultVoid insert_clip(veltrix::core::TrackId track, Clip clip);
    ResultVoid remove_clip(veltrix::core::TrackId track, veltrix::core::ClipId clip);
    const Clip* find_clip(veltrix::core::TrackId track, veltrix::core::ClipId clip) const;
    // All clips on all tracks active at time t.
    std::vector<const Clip*> clips_active_at(EditTicks t) const;

    bool operator==(const Sequence& o) const;

private:
    veltrix::core::SequenceId id_;
    std::string name_;
    Timebase timebase_;
    EditTicks explicit_duration_ = 0;
    std::vector<Track> tracks_;
};

} // namespace veltrix::timeline
