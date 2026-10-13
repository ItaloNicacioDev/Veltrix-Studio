# Interface/CMakeLists.txt — veltrix-studio (Qt Quick/QML app).
#
# The INTERFACE files live in this directory (project rule); the app
# target compiles the C++ bridge from Interface/app and loads QML from
# Interface/qml at runtime (path resolved in main.cpp relative to the
# executable, with source-tree fallback for dev builds).
#
# NOTE: this SDK (Qt Design Studio "reduced" kit) lacks Qt6EntryPoint
# import libs, so we use a plain add_executable with the WIN32 flag
# instead of qt_add_executable's entry-point machinery. qt_add_qml_module
# still registers the QML files as resources of the app.

qt_add_qml_module(veltrix_ui_module
    URI Veltrix.Interface
    VERSION 1.0
    QML_FILES
        qml/Main.qml
    NO_PLUGIN
    NO_CREATE_TARGETS)

add_executable(veltrix-studio
    app/main.cpp
    app/PreviewController.cpp
    app/PreviewController.h
    app/FrameImageProvider.h)

# This Qt kit ("reduced" Design Studio) omits Qt6EntryPoint import libs.
# Link as a console-subsystem executable with the standard CRT entry so no
# WinMain/EntryPoint shim is needed; the window is still a normal GUI app.
if(MSVC)
    target_link_options(veltrix-studio PRIVATE "/ENTRY:mainCRTStartup")
endif()

# The qml module resources (Main.qml) are compiled into the binary.
target_link_libraries(veltrix-studio PRIVATE veltrix_ui_module)

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

# Dev convenience: place Main.qml under build/Interface/qml so the
# executable's relative lookup finds it when running from the build dir.
add_custom_command(TARGET veltrix-studio POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:veltrix-studio>/../Interface/qml"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/qml/Main.qml"
        "$<TARGET_FILE_DIR:veltrix-studio>/../Interface/qml/Main.qml"
    COMMENT "Placing Main.qml under build/Interface/qml for dev runs")
