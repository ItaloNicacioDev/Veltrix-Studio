// Veltrix IO - FileUtils.h
//
// Thin, explicit file IO helpers used by persistence. Paths are native
// narrow strings; on Windows they are interpreted in the system code page
// (documented limitation - see docs/ARCHITECTURE.md "Known limitations").

#pragma once

#include <cstdint>
#include <string>

#include "veltrix/core/Result.h"

namespace veltrix::io {

// True when the path exists (file or directory).
bool file_exists(const std::string& path);

// Reads the whole file. Fails with NotFound/IoError, never throws.
veltrix::core::Result<std::string> read_text_file(const std::string& path);

// Writes atomically: content goes to "<path>.tmp" in the same directory,
// then the temp file is renamed over `path`. A crash mid-write never
// corrupts an existing file. The parent directory must exist.
veltrix::core::ResultVoid write_text_file_atomic(const std::string& path,
                                                 const std::string& content);

// Size in bytes; NotFound when missing.
veltrix::core::Result<std::uint64_t> file_size(const std::string& path);

// Weakly canonical, native-separator absolute path (best effort).
std::string normalize_path(const std::string& path);

} // namespace veltrix::io
