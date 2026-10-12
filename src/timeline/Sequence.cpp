// Veltrix Timeline - Sequence.cpp

#include "veltrix/timeline/Sequence.h"

#include <algorithm>

#include "veltrix/core/MathUtils.h"

namespace veltrix::timeline {

using veltrix::core::ErrorCode;
using veltrix::core::failure;
using veltrix::core::ResultVoid;
using veltrix::core::i64;

Result<Sequence> Sequence::create(std::string name, const Timebase& timebase,
                                  EditTicks duration) {
    if (!timebase.is_valid())
        return failure<Sequence>(ErrorCode::InvalidArgument, "sequence timebase is invalid");
    if (duration < 0)
        return failure<Sequence>(ErrorCode::InvalidArgument, "sequence duration must be >= 0");
    Sequence s;
    s.id_ = veltrix::core::SequenceId::generate();
    s.name_ = std::move(name);
    s.timebase_ = timebase;
    s.explicit_duration_ = duration;
    return Result<Sequence>(std::move(s));
}

Result<Sequence> Sequence::create_with_id(veltrix::core::SequenceId id,
                                          std::string name, const Timebase& timebase,
                                          EditTicks duration) {
    if (id.is_null())
        return failure<Sequence>(ErrorCode::InvalidArgument, "sequence id is null");
    auto s = create(std::move(name), timebase, duration);
    if (s.is_error()) return s;
    s.value().id_ = id;
    return s;
}

Result<veltrix::core::TrackId> Sequence::add_track(TrackKind kind, std::string name) {
    Track t;
    t.id = veltrix::core::TrackId::generate();
    t.kind = kind;
    t.name = std::move(name);
    tracks_.push_back(std::move(t));
    return Result<veltrix::core::TrackId>(tracks_.back().id);
}

ResultVoid Sequence::remove_track(veltrix::core::TrackId track) {
    auto it = std::find_if(tracks_.begin(), tracks_.end(),
                           [&](const Track& t) { return t.id == track; });
    if (it == tracks_.end())
        return failure(ErrorCode::NotFound, "track not found", track.to_string());
    tracks_.erase(it);
    return veltrix::core::success();
}

ResultVoid Sequence::rename_track(veltrix::core::TrackId track, std::string name) {
    Track* t = find_track(track);
    if (!t)
        return failure(ErrorCode::NotFound, "track not found", track.to_string());
    t->name = std::move(name);
    return veltrix::core::success();
}

const Track* Sequence::find_track(veltrix::core::TrackId track) const {
    auto it = std::find_if(tracks_.begin(), tracks_.end(),
                           [&](const Track& t) { return t.id == track; });
    return it == tracks_.end() ? nullptr : &*it;
}

Track* Sequence::find_track(veltrix::core::TrackId track) {
    auto it = std::find_if(tracks_.begin(), tracks_.end(),
                           [&](const Track& t) { return t.id == track; });
    return it == tracks_.end() ? nullptr : &*it;
}

EditTicks Sequence::duration() const {
    const EditTicks content = content_duration();
    return explicit_duration_ > content ? explicit_duration_ : content;
}

ResultVoid Sequence::set_duration(EditTicks ticks) {
    if (ticks < 0)
        return failure(ErrorCode::InvalidArgument, "duration must be >= 0");
    explicit_duration_ = ticks;
    return veltrix::core::success();
}

EditTicks Sequence::content_duration() const {
    EditTicks max_end = 0;
    for (const Track& t : tracks_)
        for (const Clip& c : t.clips) {
            auto end = veltrix::core::checked_add(c.start, c.duration);
            if (end.is_ok() && end.value() > max_end) max_end = end.value();
        }
    return max_end;
}

ResultVoid Sequence::insert_clip(veltrix::core::TrackId track_id, Clip clip) {
    Track* track = find_track(track_id);
    if (!track)
        return failure(ErrorCode::NotFound, "track not found", track_id.to_string());
    if (clip.duration <= 0)
        return failure(ErrorCode::InvalidArgument, "clip duration must be > 0",
                       clip.id.to_string());
    if (clip.start < 0)
        return failure(ErrorCode::InvalidArgument, "clip start must be >= 0",
                       clip.id.to_string());
    if (clip.media.is_null())
        return failure(ErrorCode::InvalidArgument, "clip has no media reference",
                       clip.id.to_string());
    if (find_clip(track_id, clip.id))
        return failure(ErrorCode::AlreadyExists, "clip id already exists on track",
                       clip.id.to_string());

    const EditTicks new_end = clip.start + clip.duration;
    for (const Clip& existing : track->clips) {
        const EditTicks ex_start = existing.start;
        const EditTicks ex_end = existing.start + existing.duration;
        if (clip.start < ex_end && new_end > ex_start)
            return failure(ErrorCode::Conflict, "clip overlaps existing clip",
                           existing.id.to_string());
    }
    auto pos = std::lower_bound(track->clips.begin(), track->clips.end(), clip.start,
                                [](const Clip& c, EditTicks start) { return c.start < start; });
    track->clips.insert(pos, std::move(clip));
    return veltrix::core::success();
}

ResultVoid Sequence::remove_clip(veltrix::core::TrackId track_id,
                                 veltrix::core::ClipId clip_id) {
    Track* track = find_track(track_id);
    if (!track)
        return failure(ErrorCode::NotFound, "track not found", track_id.to_string());
    auto it = std::find_if(track->clips.begin(), track->clips.end(),
                           [&](const Clip& c) { return c.id == clip_id; });
    if (it == track->clips.end())
        return failure(ErrorCode::NotFound, "clip not found", clip_id.to_string());
    track->clips.erase(it);
    return veltrix::core::success();
}

const Clip* Sequence::find_clip(veltrix::core::TrackId track_id,
                                veltrix::core::ClipId clip_id) const {
    const Track* track = find_track(track_id);
    if (!track) return nullptr;
    auto it = std::find_if(track->clips.begin(), track->clips.end(),
                           [&](const Clip& c) { return c.id == clip_id; });
    return it == track->clips.end() ? nullptr : &*it;
}

std::vector<const Clip*> Sequence::clips_active_at(EditTicks t) const {
    std::vector<const Clip*> out;
    for (const Track& track : tracks_)
        for (const Clip& clip : track.clips)
            if (clip.is_active_at(t)) out.push_back(&clip);
    return out;
}

bool Sequence::operator==(const Sequence& o) const {
    if (explicit_duration_ != o.explicit_duration_ ||
        !(timebase_ == o.timebase_) || tracks_.size() != o.tracks_.size())
        return false;
    for (std::size_t i = 0; i < tracks_.size(); ++i) {
        const Track& a = tracks_[i];
        const Track& b = o.tracks_[i];
        if (!(a.id == b.id) || a.name != b.name || a.kind != b.kind ||
            a.muted != b.muted || a.locked != b.locked || a.clips != b.clips)
            return false;
    }
    return true;
}

} // namespace veltrix::timeline
