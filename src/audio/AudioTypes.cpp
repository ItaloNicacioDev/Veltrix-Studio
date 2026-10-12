// Veltrix Audio - AudioTypes.cpp

#include "veltrix/audio/AudioTypes.h"

namespace veltrix::audio {

veltrix::core::ResultVoid mix_into(AudioBuffer& dst, const AudioBuffer& src,
                                   SampleCount dst_frame_offset) {
    using veltrix::core::ErrorCode;
    using veltrix::core::failure;
    if (dst.channels != src.channels || dst.channels == 0)
        return failure(ErrorCode::InvalidArgument, "channel count mismatch");
    if (dst.sample_rate != src.sample_rate)
        return failure(ErrorCode::Unsupported, "sample rate mismatch (resampling planned)");
    if (dst_frame_offset < 0)
        return failure(ErrorCode::InvalidArgument, "negative destination offset");
    if (!dst.is_valid() || !src.is_valid())
        return failure(ErrorCode::InvalidArgument, "invalid audio buffer");
    if (dst_frame_offset > dst.frames)
        return failure(ErrorCode::OutOfRange, "destination offset beyond buffer");

    const SampleCount copy_frames =
        std::min(src.frames, dst.frames - dst_frame_offset);
    const std::size_t base = static_cast<std::size_t>(dst_frame_offset) * dst.channels;
    for (SampleCount f = 0; f < copy_frames; ++f) {
        for (std::int32_t c = 0; c < dst.channels; ++c) {
            const std::size_t di =
                base + static_cast<std::size_t>(f) * dst.channels + c;
            const std::size_t si = static_cast<std::size_t>(f) * src.channels + c;
            float mixed = dst.data[di] + src.data[si];
            if (mixed > 1.0f) mixed = 1.0f;
            else if (mixed < -1.0f) mixed = -1.0f;
            dst.data[di] = mixed;
        }
    }
    return veltrix::core::success();
}

} // namespace veltrix::audio
