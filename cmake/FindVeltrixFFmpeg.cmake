# cmake/FindVeltrixFFmpeg.cmake
#
# Locates an FFmpeg development SDK (libavformat/libavcodec/libavutil/
# libswresample/libswscale) for the decode layer (ADR-008).
#
# Search order (first hit wins) — NO machine-specific absolute paths:
#   1. VELTRIX_FFMPEG_DIR  — cache variable or environment variable —
#      root containing include/ and lib/ (Windows SDK layout, e.g.
#      BtbN win64-lgpl-shared).
#   2. Standard system locations via find_path/find_library (Linux distro
#      packages: libavformat-dev etc.).
#
# Result:
#   VELTRIX_FFMPEG_FOUND      — TRUE when headers + all 5 libs were found
#   VELTRIX_FFMPEG_INCLUDE_DIR
#   VELTRIX_FFMPEG_LIBRARIES  — full paths to the 5 libraries
#
# When NOT found, the veltrix-ffmpeg target and its tests are skipped:
# the rest of the project builds identically (documented in BUILDING.md).

set(_veltrix_ffmpeg_components avformat avcodec avutil swresample swscale)

set(_veltrix_ffmpeg_root "")
if(VELTRIX_FFMPEG_DIR)
    set(_veltrix_ffmpeg_root "${VELTRIX_FFMPEG_DIR}")
elseif(DEFINED ENV{VELTRIX_FFMPEG_DIR})
    set(_veltrix_ffmpeg_root "$ENV{VELTRIX_FFMPEG_DIR}")
endif()

# ---- 1. explicit root (preferred) ---------------------------------------------
if(_veltrix_ffmpeg_root)
    find_path(VELTRIX_FFMPEG_INCLUDE_DIR
        NAMES libavformat/avformat.h
        HINTS "${_veltrix_ffmpeg_root}"
        PATH_SUFFIXES include
        NO_DEFAULT_PATH)
    set(VELTRIX_FFMPEG_LIBRARIES "")
    set(_veltrix_all_libs TRUE)
    foreach(_comp ${_veltrix_ffmpeg_components})
        find_library(VELTRIX_FFMPEG_${_comp}_LIB
            NAMES ${_comp}
            HINTS "${_veltrix_ffmpeg_root}"
            PATH_SUFFIXES lib
            NO_DEFAULT_PATH)
        if(NOT VELTRIX_FFMPEG_${_comp}_LIB)
            set(_veltrix_all_libs FALSE)
        else()
            list(APPEND VELTRIX_FFMPEG_LIBRARIES "${VELTRIX_FFMPEG_${_comp}_LIB}")
        endif()
    endforeach()
    if(VELTRIX_FFMPEG_INCLUDE_DIR AND _veltrix_all_libs)
        set(VELTRIX_FFMPEG_FOUND TRUE)
    endif()
endif()

# ---- 2. system search (fallback) ----------------------------------------------
if(NOT VELTRIX_FFMPEG_FOUND)
    find_path(VELTRIX_FFMPEG_INCLUDE_DIR
        NAMES libavformat/avformat.h
        PATH_SUFFIXES include ffmpeg/include)
    set(VELTRIX_FFMPEG_LIBRARIES "")
    set(_veltrix_all_libs TRUE)
    foreach(_comp ${_veltrix_ffmpeg_components})
        find_library(VELTRIX_FFMPEG_${_comp}_LIB
            NAMES ${_comp}
            PATH_SUFFIXES lib ffmpeg/lib)
        if(NOT VELTRIX_FFMPEG_${_comp}_LIB)
            set(_veltrix_all_libs FALSE)
        else()
            list(APPEND VELTRIX_FFMPEG_LIBRARIES "${VELTRIX_FFMPEG_${_comp}_LIB}")
        endif()
    endforeach()
    if(VELTRIX_FFMPEG_INCLUDE_DIR AND _veltrix_all_libs)
        set(VELTRIX_FFMPEG_FOUND TRUE)
    endif()
endif()

if(VELTRIX_FFMPEG_FOUND)
    message(STATUS "Veltrix FFmpeg SDK: ${VELTRIX_FFMPEG_INCLUDE_DIR}")
    message(STATUS "Veltrix FFmpeg libs: ${VELTRIX_FFMPEG_LIBRARIES}")
else()
    set(VELTRIX_FFMPEG_LIBRARIES "")
endif()
