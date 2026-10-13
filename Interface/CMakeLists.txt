# Interface/CMakeLists.txt — veltrix-studio (Qt Quick/QML app).
#
# The INTERFACE files live in this directory (project rule); the app
# target compiles the C++ bridge from Interface/app and loads QML from
# Interface/qml at runtime (path resolved in main.cpp relative to the
# executable, with source-tree fallback for dev builds).

qt_add_executable(veltrix-studio
    app/main.cpp
    app/PreviewController.cpp
    app/PreviewController.h
    app/FrameImageProvider.h)

set_target_properties(veltrix-studio PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE)

qt_add_qml_module(veltrix-studio
    URI Veltrix.Interface
    VERSION 1.0
    QML_FILES
        qml/Main.qml)

target_include_directories(veltrix-studio PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/app
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/generated
    ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(veltrix-studio PRIVATE
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Gui
    Qt6::Qml
    veltrix-core)

if(TARGET veltrix-ffmpeg)
    target_link_libraries(veltrix-studio PRIVATE veltrix-ffmpeg)
    target_compile_definitions(veltrix-studio PRIVATE VELTRIX_HAS_FFMPEG)
endif()

# MSVC + Qt headers: keep our warning policy but tolerate Qt's.
if(MSVC)
    target_compile_options(veltrix-studio PRIVATE /W4 /permissive- /utf-8 /EHsc
                             /Zc:preprocessor /external:W0)
    target_compile_definitions(veltrix-studio PRIVATE _CRT_SECURE_NO_WARNINGS
                             UNICODE _UNICODE)
    set_property(TARGET veltrix-studio PROPERTY CXX_STANDARD 20)
    set_property(TARGET veltrix-studio PROPERTY CXX_STANDARD_REQUIRED ON)
    set_property(TARGET veltrix-studio PROPERTY CXX_EXTENSIONS OFF)
endif()

# Development run helper: alias for the app; runtime Qt/libav DLLs must be
# reachable (windeployqt or PATH) — documented in Interface/README.md.
