// Veltrix Project - ProjectStore.h
//
// Persistence of projects as JSON (.veltrix files). Format is versioned
// via a top-level "schema_version" integer (ADR-004): unknown NEWER
// versions are refused, not guessed. The current writer always emits
// SCHEMA_VERSION; migrations are registered per step and run in order.
//
// Invariants:
//   * save writes to a temp file in the same directory and atomically
//     renames on success (never leaves a half-written project file);
//   * load performs full validation before returning the project;
//   * ids are round-tripped exactly (u64 strings), never regenerated.

#pragma once

#include <string>

#include "veltrix/core/Result.h"
#include "veltrix/project/Project.h"

namespace veltrix::project {

class ProjectStore {
public:
    static constexpr int SCHEMA_VERSION = 1;

    // Serializes the project to a JSON document (string). Validates first.
    static veltrix::core::Result<std::string> serialize(const Project& project);

    // Parses and validates a JSON document. Unknown schema versions and
    // structural violations produce errors, never partially-filled objects.
    static veltrix::core::Result<Project> deserialize(const std::string& json_text);

    // save_to_file writes atomically (temp + rename). The extension ".veltrix"
    // is appended when missing. Requires the target directory to exist.
    static veltrix::core::ResultVoid save_to_file(const Project& project,
                                                  const std::string& path);
    // load_from_file requires the exact file to exist; no search.
    static veltrix::core::Result<Project> load_from_file(const std::string& path);
};

} // namespace veltrix::project
