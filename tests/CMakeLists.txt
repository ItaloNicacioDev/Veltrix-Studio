# tests/CMakeLists.txt — unit + integration tests (Catch2 v2, vendored).
#
# The FFmpeg decode tests live in the SAME executable but are compiled ONLY
# when veltrix-ffmpeg exists; otherwise those TUs are excluded (and their
# cases simply absent — no silent passes).

add_library(veltrix_catch2 INTERFACE)
target_include_directories(veltrix_catch2 INTERFACE "${CMAKE_SOURCE_DIR}/third_party/catch2")

set(VELTRIX_TEST_SOURCES
    catch_runner.cpp
    unit/TestTime.cpp
    unit/TestTimeline.cpp
    unit/TestProjectStore.cpp
    unit/TestJobs.cpp
    unit/TestCache.cpp
    unit/TestMediaRegistry.cpp
    unit/TestPlugins.cpp
    unit/TestConfigAndDiagnostics.cpp
    integration/TestCliWorkflow.cpp)

if(TARGET veltrix-ffmpeg)
    list(APPEND VELTRIX_TEST_SOURCES
        unit/TestFfmpegDecode.cpp
        integration/TestFrameRequestor.cpp)
endif()

add_executable(veltrix-tests ${VELTRIX_TEST_SOURCES})
veltrix_configure_target(veltrix-tests)
target_link_libraries(veltrix-tests PRIVATE veltrix-core veltrix_catch2)

if(TARGET veltrix-ffmpeg)
    target_link_libraries(veltrix-tests PRIVATE veltrix-ffmpeg)
    target_include_directories(veltrix-tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
    # The decode tests need the ffmpeg BINARY at runtime for fixture
    # generation; failure to find it SKIPS those cases (WARN in output).
    # Nothing is downloaded at test time.
endif()

add_test(NAME veltrix-tests COMMAND veltrix-tests)
set_tests_properties(veltrix-tests PROPERTIES TIMEOUT 300)
