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

# ----------------------------------------------------------------------------
# veltrix-studio: OPTIONAL Qt Quick/QML graphical app. All interface files
# live in Interface/ (project rule). Built only when a Qt6 SDK with Quick
# is found: VELTRIX_QT_DIR, CMAKE_PREFIX_PATH, or standard locations.
# Without Qt, this target is skipped — core/CLI/tests are unaffected.
# ----------------------------------------------------------------------------
option(VELTRIX_BUILD_UI "Build the Qt Quick UI app (requires Qt6 SDK)" ON)

if(VELTRIX_BUILD_UI)
    set(_veltrix_qt_root "")
    if(VELTRIX_QT_DIR)
        set(_veltrix_qt_root "${VELTRIX_QT_DIR}")
    elseif(DEFINED ENV{VELTRIX_QT_DIR})
        set(_veltrix_qt_root "$ENV{VELTRIX_QT_DIR}")
    endif()
    if(_veltrix_qt_root)
        list(PREPEND CMAKE_PREFIX_PATH "${_veltrix_qt_root}")
    endif()

    find_package(Qt6 QUIET COMPONENTS Quick QuickControls2 Gui Qml)
    if(Qt6_FOUND)
        message(STATUS "Veltrix UI: Qt ${Qt6_VERSION} at ${Qt6_DIR}")
        add_subdirectory(Interface)
    else()
        message(STATUS "Veltrix UI: Qt6 not found -> UI app skipped "
                       "(set VELTRIX_QT_DIR to enable)")
    endif()
endif()

if(VELTRIX_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(VELTRIX_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()
