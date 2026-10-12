// Veltrix Color - ColorContract.h
//
// CONTRACT (interfaces + metadata only). Color science is easy to get
// subtly wrong; per rule 4.8 NOTHING is converted here without a verified
// implementation. What exists:
//   * explicit enums for named primaries/transfer functions (no raw
//     strings), matched against their standard definitions;
//   * a ParsedColorMetadata value type with validation;
//   * the conversion function SIGNATURE future backends implement.
// Status: no conversions implemented (docs/MODULES.md marks this module
// "contract only"). When conversions land they will be tested against
// published test vectors per standard.

#pragma once

#include <cstdint>

#include "veltrix/core/Result.h"

namespace veltrix::color {

enum class Primaries : std::uint8_t {
    Unspecified = 0,
    BT709,      // Rec.709 / sRGB primaries (ITU-R BT.709)
    BT601_NTSC, // SMPTE C (legacy NTSC)
    BT601_PAL,  // EBU Tech. 3213 (legacy PAL)
    BT2020,     // ITU-R BT.2020 (UHD/HDR)
};

enum class TransferFunction : std::uint8_t {
    Unspecified = 0,
    Linear,
    SRGB,
    Gamma22,
    Gamma28,
    BT709,  // ITU-R BT.709 OETF
    PQ,     // SMPTE ST 2084 (HDR)
    HLG,    // ARIB STD-B67 (HDR)
};

enum class PixelFormat : std::uint8_t {
    Unknown = 0,
    R8G8B8A8_Unorm,
    B8G8R8A8_Unorm,
    // Planar 4:2:0 8-bit (the first real target for decode -> render)
    YUV420P,
    // More formats are added only when a correct conversion exists.
};

// Metadata that travels with media/frames through the pipeline.
struct ColorMetadata {
    Primaries primaries = Primaries::Unspecified;
    TransferFunction transfer = TransferFunction::Unspecified;
    std::int32_t bit_depth = 8;
    bool full_range = false; // video (limited) vs full (JPEG/RGB)

    static veltrix::core::Result<ColorMetadata> create(Primaries p,
                                                       TransferFunction tf,
                                                       std::int32_t bit_depth,
                                                       bool full_range) {
        using veltrix::core::failure;
        if (p == Primaries::Unspecified || tf == TransferFunction::Unspecified)
            return failure<ColorMetadata>(veltrix::core::ErrorCode::InvalidArgument,
                                          "color metadata must specify primaries "
                                          "and transfer function");
        if (bit_depth != 8 && bit_depth != 10 && bit_depth != 12 && bit_depth != 16)
            return failure<ColorMetadata>(veltrix::core::ErrorCode::InvalidArgument,
                                          "unsupported bit depth");
        ColorMetadata m;
        m.primaries = p;
        m.transfer = tf;
        m.bit_depth = bit_depth;
        m.full_range = full_range;
        return veltrix::core::Result<ColorMetadata>(m);
    }
};

// Signature for future pixel-format conversions. Implementations MUST be
// accompanied by standard test vectors; the core refuses to guess.
// struct IPixelConverter { virtual Result<...> convert(...) = 0; };

} // namespace veltrix::color
