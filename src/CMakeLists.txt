# src/CMakeLists.txt — veltrix-core static library + veltrix-ffmpeg (optional
# decode layer) + veltrix-cli.

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
    playback/FrameRequestor.cpp
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
# veltrix-ffmpeg: OPTIONAL real decode layer (ADR-008). Isolated target so
# AV* types never appear in veltrix-core's public surface. When the FFmpeg
# SDK is not found, this target is skipped and the factory is simply
# absent — the rest of the build is unaffected.
# ----------------------------------------------------------------------------
option(VELTRIX_BUILD_FFMPEG_DECODER "Build the libav* decode layer (requires FFmpeg SDK)" ON)

if(VELTRIX_BUILD_FFMPEG_DECODER)
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
    find_package(VeltrixFFmpeg)
    if(VELTRIX_FFMPEG_FOUND)
        add_library(veltrix-ffmpeg STATIC
            media/ffmpeg/AvVideoDecoder.cpp
            media/ffmpeg/AvAudioDecoder.cpp
            media/ffmpeg/AvDecoderFactory.cpp)
        target_include_directories(veltrix-ffmpeg
            PUBLIC $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
                   $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/generated>
            PRIVATE ${CMAKE_SOURCE_DIR}/src)
        # FFmpeg headers trigger MSVC conversion warnings (C4244 in
        # libavutil/common.h); they are third-party, not ours to fix.
        if(MSVC)
            target_compile_options(veltrix-ffmpeg PRIVATE /W4 /external:W0)
            target_compile_definitions(veltrix-ffmpeg PRIVATE _CRT_SECURE_NO_WARNINGS)
        else()
            target_compile_options(veltrix-ffmpeg PRIVATE -Wall -Wextra
                                   -Wno-deprecated-declarations)
        endif()
        target_include_directories(veltrix-ffmpeg SYSTEM PUBLIC ${VELTRIX_FFMPEG_INCLUDE_DIR})
        set_property(TARGET veltrix-ffmpeg PROPERTY CXX_STANDARD 20)
        set_property(TARGET veltrix-ffmpeg PROPERTY CXX_STANDARD_REQUIRED ON)
        set_property(TARGET veltrix-ffmpeg PROPERTY CXX_EXTENSIONS OFF)
        target_link_libraries(veltrix-ffmpeg PUBLIC ${VELTRIX_FFMPEG_LIBRARIES})
    else()
        message(STATUS "Veltrix: FFmpeg SDK not found -> decode layer skipped "
                       "(set VELTRIX_FFMPEG_DIR to enable)")
    endif()
endif()

# ----------------------------------------------------------------------------
# veltrix-cli: diagnostic tool (NOT the product UI).
# ----------------------------------------------------------------------------
if(VELTRIX_BUILD_CLI)
    add_executable(veltrix-cli cli/main.cpp)
    veltrix_configure_target(veltrix-cli)
    target_link_libraries(veltrix-cli PRIVATE veltrix-core)
    if(TARGET veltrix-ffmpeg)
        target_sources(veltrix-cli PRIVATE cli/DecodeCommands.cpp)
        target_include_directories(veltrix-cli PRIVATE ${CMAKE_SOURCE_DIR}/src)
        target_link_libraries(veltrix-cli PRIVATE veltrix-ffmpeg)
        target_compile_definitions(veltrix-cli PRIVATE VELTRIX_HAS_FFMPEG)
    endif()
endif()
