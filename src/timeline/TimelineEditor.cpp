// Veltrix Timeline - TimelineEditor.cpp

#include "veltrix/timeline/TimelineEditor.h"

#include <algorithm>

namespace veltrix::timeline {

using veltrix::core::ErrorCode;
using veltrix::core::failure;
using veltrix::core::ResultVoid;

TimelineEditor::TimelineEditor(Sequence sequence) : sequence_(std::move(sequence)) {}

void TimelineEditor::push_undo(const char* op_name) {
    undo_stack_.emplace_back(op_name, sequence_);
    if (undo_stack_.size() > kMaxHistory)
        undo_stack_.erase(undo_stack_.begin());
    redo_stack_.clear();
}

// ---- clip operations ---------------------------------------------------------

Result<veltrix::core::ClipId> TimelineEditor::insert_clip(veltrix::core::TrackId track,
                                                          veltrix::core::MediaId media,
                                                          EditTicks start, EditTicks duration,
                                                          EditTicks source_in) {
    if (duration <= 0)
        return failure<veltrix::core::ClipId>(ErrorCode::InvalidArgument,
                                              "clip duration must be > 0");
    if (start < 0)
        return failure<veltrix::core::ClipId>(ErrorCode::InvalidArgument,
                                              "clip start must be >= 0");
    if (source_in < 0)
        return failure<veltrix::core::ClipId>(ErrorCode::InvalidArgument,
                                              "clip source_in must be >= 0");
    if (media.is_null())
        return failure<veltrix::core::ClipId>(ErrorCode::InvalidArgument,
                                              "clip needs a media reference");
    const Track* track_ptr = sequence_.find_track(track);
    if (!track_ptr)
        return failure<veltrix::core::ClipId>(ErrorCode::NotFound,
                                              "track not found", track.to_string());

    // Validate overlap BEFORE mutating so failure leaves the state intact.
    const EditTicks new_end = start + duration;
    for (const Clip& existing : track_ptr->clips) {
        if (start < existing.start + existing.duration && new_end > existing.start)
            return failure<veltrix::core::ClipId>(ErrorCode::Conflict,
                                                  "clip overlaps existing clip",
                                                  existing.id.to_string());
    }

    Clip clip;
    clip.id = veltrix::core::ClipId::generate();
    clip.media = media;
    clip.start = start;
    clip.duration = duration;
    clip.source_in = source_in;
    const veltrix::core::ClipId new_id = clip.id;

    push_undo("insert_clip");
    auto res = sequence_.insert_clip(track, std::move(clip));
    if (res.is_error()) { // defensive: unreachable after validation above
        undo();
        return failure<veltrix::core::ClipId>(res.error().code, res.error().message);
    }
    return Result<veltrix::core::ClipId>(new_id);
}

ResultVoid TimelineEditor::remove_clip(veltrix::core::TrackId track,
                                       veltrix::core::ClipId clip) {
    if (!sequence_.find_clip(track, clip))
        return failure(ErrorCode::NotFound, "clip not found", clip.to_string());
    push_undo("remove_clip");
    return sequence_.remove_clip(track, clip);
}

ResultVoid TimelineEditor::move_clip(veltrix::core::TrackId track,
                                     veltrix::core::ClipId clip_id, EditTicks new_start) {
    const Clip* clip = sequence_.find_clip(track, clip_id);
    if (!clip) return failure(ErrorCode::NotFound, "clip not found", clip_id.to_string());
    if (new_start < 0)
        return failure(ErrorCode::InvalidArgument, "clip start must be >= 0");
    if (new_start == clip->start) return veltrix::core::success();

    // Validate: the new extent must not overlap any OTHER clip on the track.
    const EditTicks new_end = new_start + clip->duration;
    const Track* t = sequence_.find_track(track);
    for (const Clip& existing : t->clips) {
        if (existing.id == clip_id) continue;
        if (new_start < existing.start + existing.duration && new_end > existing.start)
            return failure(ErrorCode::Conflict, "move would overlap clip",
                           existing.id.to_string());
    }

    push_undo("move_clip");
    Track* mut = sequence_.find_track(track);
    auto it = std::find_if(mut->clips.begin(), mut->clips.end(),
                           [&](const Clip& c) { return c.id == clip_id; });
    it->start = new_start;
    std::stable_sort(mut->clips.begin(), mut->clips.end(),
                     [](const Clip& a, const Clip& b) { return a.start < b.start; });
    return veltrix::core::success();
}

Result<veltrix::core::ClipId> TimelineEditor::split_clip(veltrix::core::TrackId track,
                                                         veltrix::core::ClipId clip_id,
                                                         EditTicks position) {
    const Clip* clip = sequence_.find_clip(track, clip_id);
    if (!clip)
        return failure<veltrix::core::ClipId>(ErrorCode::NotFound, "clip not found",
                                               clip_id.to_string());
    if (position <= clip->start || position >= clip->start + clip->duration)
        return failure<veltrix::core::ClipId>(
            ErrorCode::InvalidArgument, "split position must be strictly inside the clip");

    push_undo("split_clip");
    Track* mut = sequence_.find_track(track);
    auto it = std::find_if(mut->clips.begin(), mut->clips.end(),
                           [&](const Clip& c) { return c.id == clip_id; });

    Clip right = *it;
    right.id = veltrix::core::ClipId::generate();
    right.start = position;
    right.duration = clip->start + clip->duration - position;
    right.source_in = clip->source_in + (position - clip->start);
    it->duration = position - it->start;

    auto res = sequence_.insert_clip(track, right);
    if (res.is_error()) { // adjacent, non-overlapping by construction
        undo();
        return failure<veltrix::core::ClipId>(res.error().code, res.error().message);
    }
    return Result<veltrix::core::ClipId>(right.id);
}

ResultVoid TimelineEditor::trim_clip(veltrix::core::TrackId track,
                                     veltrix::core::ClipId clip_id,
                                     EditTicks new_source_in, EditTicks new_duration) {
    const Clip* clip = sequence_.find_clip(track, clip_id);
    if (!clip) return failure(ErrorCode::NotFound, "clip not found", clip_id.to_string());
    if (new_duration <= 0)
        return failure(ErrorCode::InvalidArgument, "clip duration must be > 0");
    if (new_source_in < 0)
        return failure(ErrorCode::InvalidArgument, "source_in must be >= 0");
    // Trim keeps the clip END anchored: start moves with the trim delta.
    const EditTicks delta = new_source_in - clip->source_in;
    const EditTicks new_start = clip->start + delta;
    if (new_start < 0)
        return failure(ErrorCode::InvalidArgument, "trim would move clip start before 0");

    // Overlap validation against every other clip on the track.
    const Track* t = sequence_.find_track(track);
    const EditTicks new_end = new_start + new_duration;
    for (const Clip& existing : t->clips) {
        if (existing.id == clip_id) continue;
        if (new_start < existing.start + existing.duration && new_end > existing.start)
            return failure(ErrorCode::Conflict, "trim would overlap clip",
                           existing.id.to_string());
    }

    push_undo("trim_clip");
    Track* mut = sequence_.find_track(track);
    auto it = std::find_if(mut->clips.begin(), mut->clips.end(),
                           [&](const Clip& c) { return c.id == clip_id; });
    it->start = new_start;
    it->duration = new_duration;
    it->source_in = new_source_in;
    std::stable_sort(mut->clips.begin(), mut->clips.end(),
                     [](const Clip& a, const Clip& b) { return a.start < b.start; });
    return veltrix::core::success();
}

// ---- track operations -------------------------------------------------------

Result<veltrix::core::TrackId> TimelineEditor::add_track(TrackKind kind, std::string name) {
    push_undo("add_track");
    auto res = sequence_.add_track(kind, std::move(name));
    if (res.is_error()) undo();
    return res;
}

ResultVoid TimelineEditor::remove_track(veltrix::core::TrackId track) {
    if (!sequence_.find_track(track))
        return failure(ErrorCode::NotFound, "track not found", track.to_string());
    push_undo("remove_track");
    return sequence_.remove_track(track);
}

ResultVoid TimelineEditor::rename_track(veltrix::core::TrackId track, std::string name) {
    if (!sequence_.find_track(track))
        return failure(ErrorCode::NotFound, "track not found", track.to_string());
    push_undo("rename_track");
    return sequence_.rename_track(track, std::move(name));
}

// ---- undo / redo -------------------------------------------------------------

ResultVoid TimelineEditor::undo() {
    if (undo_stack_.empty())
        return failure(ErrorCode::Conflict, "nothing to undo");
    // Classic two-stack snapshot undo: save the CURRENT state for redo,
    // then restore the last pre-operation snapshot.
    redo_stack_.emplace_back(std::string(), sequence_);
    sequence_ = std::move(undo_stack_.back().second);
    undo_stack_.pop_back();
    return veltrix::core::success();
}

ResultVoid TimelineEditor::redo() {
    if (redo_stack_.empty())
        return failure(ErrorCode::Conflict, "nothing to redo");
    undo_stack_.emplace_back(std::string(), sequence_);
    sequence_ = std::move(redo_stack_.back().second);
    redo_stack_.pop_back();
    return veltrix::core::success();
}

// ---- document ----------------------------------------------------------------

ResultVoid TimelineEditor::load(Sequence sequence) {
    sequence_ = std::move(sequence);
    undo_stack_.clear();
    redo_stack_.clear();
    return veltrix::core::success();
}

} // namespace veltrix::timeline
