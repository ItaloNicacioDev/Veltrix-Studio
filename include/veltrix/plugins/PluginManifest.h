// Veltrix Plugins - PluginManifest.h
//
// Manifest parsing + compatibility checking (metadata only; NO dynamic
// loading in this stage). A manifest describes an effects/audio extension:
//   * identity: id (reverse-domain strongly recommended), name, vendor,
//     version (semver-ish major.minor.patch), api version;
//   * compatibility: the Veltrix plugin API version it targets;
//   * failure handling: malformed manifests are REJECTED with explicit
//     errors, never guessed.
//
// SECURITY (rule 4.10): third-party plugins are ARBITRARY NATIVE CODE.
// In-process loading of untrusted plugins is a documented, unacceptable
// risk for the main process; the end state is an out-of-process plugin
// host. No load path exists in this stage. SDK: planned (docs/ROADMAP.md).
//
// Status: manifest JSON parsing + compatibility check implemented and
// tested. Discovery/loading: planned.

#pragma once

#include <cstdint>
#include <string>

#include "veltrix/core/Result.h"

namespace veltrix::plugins {

// Plugin API version of Veltrix itself (bump on breaking changes).
inline constexpr std::uint32_t kCurrentPluginApi = 1;

enum class PluginType : std::uint8_t {
    Unknown = 0,
    Effect,  // visual effect (future: OpenFX-compatible hosting candidate)
    AudioProcessor, // audio plugin (future: VST3 hosting candidate)
};

struct PluginManifest {
    std::string id;          // e.g. "com.vendor.effectname"
    std::string name;
    std::string vendor;
    std::uint32_t version_major = 0;
    std::uint32_t version_minor = 0;
    std::uint32_t version_patch = 0;
    std::uint32_t api_version = 0;   // Veltrix plugin API it targets
    PluginType type = PluginType::Unknown;
    std::string entry_library;        // relative path inside the plugin dir

    // Parses a manifest.json document. Strict: missing/invalid fields are
    // errors; unknown keys are ignored (forwards compatibility).
    static veltrix::core::Result<PluginManifest> from_json(const std::string& text);

    // True when this Veltrix build can host the plugin. Plugins targeting a
    // NEWER api are refused (never "best effort").
    bool is_compatible() const { return api_version <= kCurrentPluginApi; }
};

} // namespace veltrix::plugins
