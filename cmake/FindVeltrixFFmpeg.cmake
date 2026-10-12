# cmake/FindVeltrixFFmpeg.cmake
#
# Locates an FFmpeg development SDK (libavformat/libavcodec/libavutil/
# libswresample/libswscale) for the decode layer (ADR-008).
#
# Search order (first hit wins) — NO machine-specific absolute paths:
#   1. VELTRIX_FFMPEG_DIR  — root containing include/ and lib/
#      (Windows SDK layout, e.g. BtbN win64-lgpl-shared; also accepts a
#      root containing include/ + a lib/ with .lib import libraries).
#   2. Standard system locations via find_path/find_library (Linux distro
#      packages: libavformat-dev etc. — found through CMAKE_PREFIX_PATH /
#      default system paths).
#
# Result:
#   VELTRIX_FFMPEG_FOUND      — TRUE when headers + all 5 libs were found
#   VELTRIX_FFMPEG_INCLUDE_DIR
#   VELTRIX_FFMPEG_LIBRARIES  — full paths to the 5 libraries
#
# When NOT found, the veltrix-ffmpeg target and its tests are skipped:
# the rest of the project builds identically (documented in BUILDING.md).

set(_veltrix_ffmpeg_components avformat avcodec avutil swresample swscale)

# ---- 1. explicit VELTRIX_FFMPEG_DIR ------------------------------------------
if(VELTRIX_FFMPEG_DIR)
    find_path(VELTRIX_FFMPEG_INCLUDE_DIR
        NAMES libavformat/avformat.h
        PATHS "${VELTRIX_FFMPEG_DIR}"
        PATH_SUFFIXES include
        NO_DEFAULT_PATH)
    foreach(_comp ${_veltrix_ffmpeg_components})
        find_library(VELTRIX_FFMPEG_${_comp}_LIB
            NAMES ${_comp}
            PATHS "${VELTRIX_FFMPEG_DIR}"
            PATH_SUFFIXES lib
            NO_DEFAULT_PATH)
        list(APPEND _veltrix_ffmpeg_lib_candidates "${VELTRIX_FFMPEG_${_comp}_LIB}")
    endforeach()
endif()

# ---- 2. system search ---------------------------------------------------------
if(NOT VELTRIX_FFMPEG_INCLUDE_DIR)
    find_path(VELTRIX_FFMPEG_INCLUDE_DIR
        NAMES libavformat/avformat.h
        PATH_SUFFIXES include
        ffmpeg/include)
endif()

foreach(_comp ${_veltrix_ffmpeg_components})
    if(NOT VELTRIX_FFMPEG_${_comp}_LIB)
        find_library(VELTRIX_FFMPEG_${_comp}_LIB
            NAMES ${_comp}
            PATH_SUFFIXES lib ffmpeg/lib)
    endif()
    list(APPEND _veltrix_ffmpeg_lib_candidates "${VELTRIX_FFMPEG_${_comp}_LIB}")
    list(APPEND VELTRIX_FFMPEG_LIBRARIES "${VELTRIX_FFMPEG_${_comp}_LIB}")
endforeach()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(VeltrixFFmpeg
    REQUIRED_VARS
        VELTRIX_FFMPEG_INCLUDE_DIR
        VELTRIX_FFMPEG_avformat_LIB
        VELTRIX_FFMPEG_avcodec_LIB
        VELTRIX_FFMPEG_avutil_LIB
        VELTRIX_FFMPEG_swresample_LIB
        VELTRIX_FFMPEG_swscale_LIB)

if(VELTRIX_FFMPEG_FOUND)
    message(STATUS "Veltrix FFmpeg SDK: ${VELTRIX_FFMPEG_INCLUDE_DIR}")
    message(STATUS "Veltrix FFmpeg libs: ${VELTRIX_FFMPEG_LIBRARIES}")
endif()
