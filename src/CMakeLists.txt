# src/CMakeLists.txt — veltrix-core static library + veltrix-cli.

# ----------------------------------------------------------------------------
# veltrix-core: the entire engine as one static library. Module boundaries
# are enforced by namespaces and include layout, not by link seams (kept
# simple at foundation scale; can be split per-module later).
# ----------------------------------------------------------------------------
add_library(veltrix-core STATIC
    core/AppConfig.cpp
    diagnostics/Diagnostics.cpp
    timeline/Time.cpp
    timeline/Sequence.cpp
    timeline/TimelineEditor.cpp
    project/ProjectSettings.cpp
    project/Project.cpp
    project/ProjectStore.cpp
    media/MediaRegistry.cpp
    media/MediaProber.cpp
    io/FileUtils.cpp
    jobs/JobManager.cpp
    audio/AudioTypes.cpp
    plugins/PluginManifest.cpp)

target_include_directories(veltrix-core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/generated>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/third_party>)
veltrix_configure_target(veltrix-core)
target_link_libraries(veltrix-core PRIVATE veltrix_vendored)
# Threaded runtime (std::thread / std::mutex) on POSIX.
find_package(Threads REQUIRED)
target_link_libraries(veltrix-core PUBLIC Threads::Threads)

if(WIN32)
    # MediaProber.cpp uses Win32 process/pipe APIs.
    target_link_libraries(veltrix-core PRIVATE kernel32 user32)
endif()

# ----------------------------------------------------------------------------
# veltrix-cli: diagnostic tool (NOT the product UI).
# ----------------------------------------------------------------------------
if(VELTRIX_BUILD_CLI)
    add_executable(veltrix-cli cli/main.cpp)
    veltrix_configure_target(veltrix-cli)
    target_link_libraries(veltrix-cli PRIVATE veltrix-core)
endif()
