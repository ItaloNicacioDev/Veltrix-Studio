// Veltrix Timeline - TimelineEditor.h
//
// Validated, undoable editing operations over a Sequence. The editor OWNS
// the sequence (the editing document); undo/redo uses full snapshots of the
// Sequence - simple, correct, adequate at this scale. A command/diff model
// can replace it later behind the same API (docs/ADR/ADR-003-undo-model.md).
//
// Every mutating operation:
//   1. validates the request against the CURRENT state (no partial edits);
//   2. snapshots the state onto the undo stack;
//   3. applies the mutation.
// On any validation failure the sequence is left untouched.
//
// Invariant: after a successful operation, all Sequence invariants hold
// (sorted, non-overlapping clips, positive durations, non-null media).

#pragma once

#include <string>
#include <utility>
#include <vector>

#include "veltrix/core/Id.h"
#include "veltrix/core/Result.h"
#include "veltrix/timeline/Sequence.h"

namespace veltrix::timeline {

using veltrix::core::Result;
using veltrix::core::ResultVoid;

class TimelineEditor {
public:
    // Takes ownership of the sequence document.
    explicit TimelineEditor(Sequence sequence);
    TimelineEditor() = default;

    // ---- clip operations ----------------------------------------------------
    // Inserts a new clip; the editor assigns its id. Fails on overlap or
    // invalid arguments without touching the sequence.
    Result<veltrix::core::ClipId> insert_clip(veltrix::core::TrackId track,
                                              veltrix::core::MediaId media,
                                              EditTicks start, EditTicks duration,
                                              EditTicks source_in);
    ResultVoid remove_clip(veltrix::core::TrackId track, veltrix::core::ClipId clip);
    // Moves a clip (same track) to a new start; validates no overlap.
    ResultVoid move_clip(veltrix::core::TrackId track, veltrix::core::ClipId clip,
                         EditTicks new_start);
    // Splits a clip at `position` (timeline ticks, strictly inside the
    // clip). The left part keeps the id; the right part gets a new id.
    Result<veltrix::core::ClipId> split_clip(veltrix::core::TrackId track,
                                             veltrix::core::ClipId clip,
                                             EditTicks position);
    // Trims the clip extent keeping the clip END anchored; source_in and
    // start move together so the media mapping stays consistent.
    ResultVoid trim_clip(veltrix::core::TrackId track, veltrix::core::ClipId clip,
                         EditTicks new_source_in, EditTicks new_duration);

    // ---- track operations ---------------------------------------------------
    Result<veltrix::core::TrackId> add_track(TrackKind kind, std::string name);
    ResultVoid remove_track(veltrix::core::TrackId track);
    ResultVoid rename_track(veltrix::core::TrackId track, std::string name);

    // ---- undo / redo --------------------------------------------------------
    bool can_undo() const { return !undo_stack_.empty(); }
    bool can_redo() const { return !redo_stack_.empty(); }
    ResultVoid undo();
    ResultVoid redo();
    std::size_t undo_depth() const { return undo_stack_.size(); }
    std::size_t redo_depth() const { return redo_stack_.size(); }

    // ---- document access ----------------------------------------------------
    const Sequence& sequence() const { return sequence_; }
    Sequence take_sequence() { return std::move(sequence_); }
    // Replaces the document; clears history.
    ResultVoid load(Sequence sequence);

private:
    void push_undo(const char* op_name);

    Sequence sequence_;
    std::vector<std::pair<std::string, Sequence>> undo_stack_;
    std::vector<std::pair<std::string, Sequence>> redo_stack_;
    static constexpr std::size_t kMaxHistory = 128;
};

} // namespace veltrix::timeline
