# Interface/CMakeLists.txt — veltrix-studio (Qt Quick/QML app).
#
# The INTERFACE files live in this directory (project rule); the app
# target compiles the C++ bridge from Interface/app and loads QML from
# Interface/qml at RUNTIME as local files (path resolved in main.cpp).
#
# Why no qt_add_qml_module: this machine's only Qt kit ("Qt Design Studio
# reduced") ships neither Qt6EntryPoint import libs nor the QML plugin
# binaries that qt_add_qml_module's machinery requires (its cmake configs
# reference plugin DLLs that do not exist in the kit). Loading QML from
# Interface/qml as local files avoids that machinery entirely and keeps
# ALL interface files in this directory. On a full Qt kit, switching back
# to qt_add_qml_module is a drop-in change (documented in Interface/README).

add_executable(veltrix-studio
    app/main.cpp
    app/PreviewController.cpp
    app/PreviewController.h
    app/FrameImageProvider.h)

# Q_OBJECT requires moc processing of PreviewController.h.
set_target_properties(veltrix-studio PROPERTIES AUTOMOC ON)

# This kit lacks Qt6EntryPoint import libs: link as a console-subsystem
# executable with the standard CRT entry (no WinMain shim needed); the
# window is still a normal GUI app (QGuiApplication).
if(MSVC)
    target_link_options(veltrix-studio PRIVATE "/ENTRY:mainCRTStartup")
endif()

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

if(WIN32)
    # CrashHandler.h uses MiniDumpWriteDump (diagnostics).
    target_link_libraries(veltrix-studio PRIVATE dbghelp)
endif()

if(TARGET veltrix-ffmpeg)
    target_link_libraries(veltrix-studio PRIVATE veltrix-ffmpeg)
    target_compile_definitions(veltrix-studio PRIVATE VELTRIX_HAS_FFMPEG)
endif()

# MSVC + Qt headers: keep our warning policy but tolerate Qt's.
if(MSVC)
    target_compile_options(veltrix-studio PRIVATE /W4 /permissive- /utf-8 /EHsc
                             /Zc:preprocessor /external:W0 /Zi)
    target_compile_definitions(veltrix-studio PRIVATE _CRT_SECURE_NO_WARNINGS
                             UNICODE _UNICODE)
    target_link_options(veltrix-studio PRIVATE "/DEBUG" "/MAP")
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
