# VeltrixOptions.cmake — user-facing build options for the Veltrix core.
#
# All options are documented in docs/BUILDING.md.

option(VELTRIX_BUILD_CLI "Build the veltrix-cli diagnostic executable" ON)
option(VELTRIX_BUILD_TESTS "Build unit and integration tests" ON)
option(VELTRIX_BUILD_BENCHMARKS "Build benchmarks (hand-rolled timing harness)" OFF)
option(VELTRIX_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(VELTRIX_ENABLE_LTO "Enable link time optimization in Release builds" OFF)

set(VELTRIX_THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party"
    CACHE PATH "Directory containing vendored header-only dependencies")

message(STATUS "Veltrix: version ${PROJECT_VERSION}")
message(STATUS "Veltrix: CLI=${VELTRIX_BUILD_CLI} tests=${VELTRIX_BUILD_TESTS} benchmarks=${VELTRIX_BUILD_BENCHMARKS}")
