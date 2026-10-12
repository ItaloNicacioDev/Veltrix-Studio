// Veltrix Audio - AudioTypes.h
//
// Unambiguous audio vocabulary (rule 4.7):
//   * AudioSample  : one sample value for ONE channel (float, deinterleaved
//                    representation inside the engine);
//   * AudioFrame   : one sample for ALL channels at one time instant
//                    (N channels -> N samples);
//   * SampleRate   : AudioFrames per second;
//   * SampleCount  : count of AudioFrames (per all channels).
// Editing positions still use timeline EditTicks; SampleCount is used only
// inside the audio pipeline where the sample rate is fixed.
//
// Status: types + a tested linear gain + mixdown of non-overlapping
// streams implemented. Fades/keyframes/effects are planned.

#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

#include "veltrix/core/Result.h"

namespace veltrix::audio {

using SampleRate = std::int32_t;
using SampleCount = std::int64_t;

// Deinterleaved planar buffer: channel c = data[c * frames + f].
struct AudioBuffer {
    std::int32_t channels = 0;
    SampleRate sample_rate = 0;
    SampleCount frames = 0;
    std::vector<float> data; // frames * channels

    bool is_valid() const {
        return channels > 0 && sample_rate > 0 && frames >= 0 &&
               data.size() == static_cast<std::size_t>(frames) * channels;
    }
};

// Applies linear gain (0.0..inf) with hard clipping to [-1, 1].
inline void apply_gain(AudioBuffer& buffer, float gain) {
    if (gain == 1.0f) return;
    for (float& s : buffer.data) {
        s *= gain;
        if (s > 1.0f) s = 1.0f;
        else if (s < -1.0f) s = -1.0f;
    }
}

// Mixes `src` into `dst` starting at `dst_frame` (sum, then clamp).
// Channel counts must match; sample rates must match (resampling is a
// planned module). Fails cleanly on mismatch or out-of-range offsets.
veltrix::core::ResultVoid mix_into(AudioBuffer& dst, const AudioBuffer& src,
                                   SampleCount dst_frame_offset);

} // namespace veltrix::audio
