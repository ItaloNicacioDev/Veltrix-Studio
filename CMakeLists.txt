cmake_minimum_required(VERSION 3.24)

project(VeltrixStudio
    VERSION 0.1.0
    DESCRIPTION "Veltrix Studio - open source video editor core engine"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(VeltrixOptions)
include(CompilerOptions)
include(Dependencies)

# Optional git commit identifier (tolerated when the tree is not a repository).
execute_process(
    COMMAND git rev-parse --short=12 HEAD
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    OUTPUT_VARIABLE VELTRIX_GIT_COMMIT
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _git_result)
if(NOT _git_result EQUAL 0 OR NOT VELTRIX_GIT_COMMIT)
    set(VELTRIX_GIT_COMMIT "unknown")
endif()

# Generated version/build header.
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/veltrix/core/Version.h"
    @ONLY)

add_subdirectory(src)

if(VELTRIX_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(VELTRIX_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()
