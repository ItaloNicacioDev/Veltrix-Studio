// Veltrix Timeline - Clip.h
//
// A clip references a media item and occupies [start, start + duration) on
// a track, reading from the media source starting at `source_in`.
//
// Invariants (enforced by Timeline operations):
//   * position >= 0, duration > 0;
//   * clips on the same track never overlap;
//   * source_in >= 0 (may extend past media end only when the timeline
//     allows explicit freeze/stretch - not in this stage).

#pragma once

#include <optional>
#include <string>

#include "veltrix/core/Id.h"
#include "veltrix/core/Result.h"
#include "veltrix/timeline/Time.h"

namespace veltrix::timeline {

struct Clip {
    veltrix::core::ClipId id;
    veltrix::core::MediaId media;
    veltrix::timeline::EditTicks start = 0;     // timeline position (ticks)
    veltrix::timeline::EditTicks duration = 0;  // length on timeline (ticks)
    veltrix::timeline::EditTicks source_in = 0; // offset into the media (ticks)

    bool is_active_at(EditTicks t) const { return t >= start && t < start + duration; }

    // Structural equality (ids included) - used by round-trip tests.
    bool operator==(const Clip&) const = default;
};

} // namespace veltrix::timeline
