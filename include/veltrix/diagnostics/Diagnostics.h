// Veltrix Diagnostics - Diagnostics.h
//
// Aggregates build info, dependency versions and runtime metrics for
// reports (CLI `info`, future crash reports). Never logs secrets or
// private paths; the report is plain text with stable section headers.

#pragma once

#include <string>
#include <vector>

namespace veltrix::diagnostics {

// Assembles the human-readable build/runtime report.
std::string build_report();

} // namespace veltrix::diagnostics
