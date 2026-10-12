# Dependencies.cmake — dependency wiring for the Veltrix core.
#
# Policy (see docs/DEPENDENCIES_AND_LICENSES.md):
#   * Only vendored, header-only libraries are linked at this stage:
#       - nlohmann/json 3.11.3 (MIT)      -> third_party/nlohmann/json.hpp
#       - Catch2     2.13.10 (BSL-1.0)    -> third_party/catch2/catch.hpp
#   * FFmpeg is NOT linked into the core. Media probing is done at runtime by
#     locating ffprobe on PATH (or via VELTRIX_FFPROBE). Linking libav* is a
#     future decision documented in docs/ADR/ADR-006-ffmpeg-integration.md.
#   * No machine-specific absolute paths appear here or in any build file.

if(NOT EXISTS "${VELTRIX_THIRD_PARTY_DIR}/nlohmann/json.hpp")
    message(FATAL_ERROR
        "Vendored dependency missing: third_party/nlohmann/json.hpp "
        "(nlohmann/json 3.11.3, MIT). Re-download it - see scripts/README.md.")
endif()

add_library(veltrix_vendored INTERFACE)
target_include_directories(veltrix_vendored INTERFACE "${VELTRIX_THIRD_PARTY_DIR}")

# Versions reported by diagnostics; keep in sync with the vendored files.
set(VELTRIX_NLOHMANN_JSON_VERSION "3.11.3")
set(VELTRIX_CATCH2_VERSION "2.13.10")
