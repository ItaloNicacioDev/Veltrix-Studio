// Veltrix Render - RenderGraph.h
//
// CONTRACT for the processing graph. Nodes produce one output frame for a
// given sequence time; the evaluator resolves active clips, builds the
// chain and composes layers bottom-up (track order). CPU/GPU specifics
// live in future backend implementations behind these interfaces.
//
// Status: interfaces + TimelineEvaluator (timeline -> active clip
// resolution) implemented and tested. Frame compositing (blend, transform)
// is planned; no pixel work exists yet (docs/MODULES.md).

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "veltrix/core/Id.h"
#include "veltrix/core/Result.h"
#include "veltrix/timeline/Sequence.h"

namespace veltrix::render {

// Opaque image produced/consumed by nodes. Concrete pixel types are a
// backend concern; the graph moves ownership via shared_ptr.
struct RenderImage {
    int width = 0;
    int height = 0;
    std::shared_ptr<void> pixels; // backend-specific
    std::uint64_t generation = 0; // timeline edit generation for caching
};

// One node in the processing graph (source, filter or compositor).
class IRenderNode {
public:
    virtual ~IRenderNode() = default;
    virtual const std::string& name() const = 0;
    // Renders the node output at sequence time `ticks`.
    virtual veltrix::core::Result<RenderImage> render(std::int64_t ticks) = 0;
};

// Resolves which clips are active at a given sequence time and in which
// order they must be composed (bottom track first, per z-order).
class TimelineEvaluator {
public:
    explicit TimelineEvaluator(const veltrix::timeline::Sequence& sequence)
        : sequence_(sequence) {}

    // Active video clips at `ticks`, ordered bottom..top for compositing.
    // Video tracks only; audio mixing has its own path (veltrix/audio).
    std::vector<const veltrix::timeline::Clip*> active_video_clips(
        std::int64_t ticks) const {
        std::vector<const veltrix::timeline::Clip*> out;
        for (const auto& track : sequence_.tracks()) {
            if (track.kind != veltrix::timeline::TrackKind::Video) continue;
            for (const auto& clip : track.clips)
                if (clip.is_active_at(ticks)) out.push_back(&clip);
        }
        return out;
    }

    // Active audio clips at `ticks`, track order.
    std::vector<const veltrix::timeline::Clip*> active_audio_clips(
        std::int64_t ticks) const {
        std::vector<const veltrix::timeline::Clip*> out;
        for (const auto& track : sequence_.tracks()) {
            if (track.kind != veltrix::timeline::TrackKind::Audio) continue;
            for (const auto& clip : track.clips)
                if (clip.is_active_at(ticks)) out.push_back(&clip);
        }
        return out;
    }

private:
    const veltrix::timeline::Sequence& sequence_;
};

} // namespace veltrix::render
