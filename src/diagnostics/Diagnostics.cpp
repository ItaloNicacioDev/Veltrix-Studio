// Veltrix Diagnostics - Diagnostics.cpp

#include "veltrix/diagnostics/Diagnostics.h"

#include <sstream>

#include "veltrix/core/Version.h"
#include "veltrix/jobs/JobManager.h"
#include "veltrix/plugins/PluginManifest.h"
#include "veltrix/project/ProjectStore.h"

namespace veltrix::diagnostics {

std::string build_report() {
    std::ostringstream out;
    out << "=== Veltrix Studio core - diagnostic report ===\n";
    out << "version:        " << veltrix::core::kVersionString << "\n";
    out << "git commit:     " << veltrix::core::kGitCommit << "\n";
    out << "platform:       " << veltrix::core::kPlatform << "\n";
    out << "compiler:       " << veltrix::core::kCompiler << "\n";
    out << "build type:     " << veltrix::core::kBuildType << "\n";
    out << "c++ standard:   " << veltrix::core::kCxxStandard << "\n";
    out << "\n-- formats --\n";
    out << "project schema: " << veltrix::project::ProjectStore::SCHEMA_VERSION << "\n";
    out << "\n-- plugin api --\n";
    out << "current api:    " << veltrix::plugins::kCurrentPluginApi << "\n";
    out << "\n-- vendored dependencies --\n";
    out << "nlohmann/json:  3.11.3 (MIT) - JSON persistence/parsing\n";
    out << "catch2:         2.13.10 (BSL-1.0) - tests only, not shipped\n";
    out << "\n-- runtime discovery --\n";
    out << "ffprobe:        discovered at runtime (PATH / VELTRIX_FFPROBE); "
           "libav* is not linked\n";
    return out.str();
}

} // namespace veltrix::diagnostics
