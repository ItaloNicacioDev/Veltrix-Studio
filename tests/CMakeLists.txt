# tests/CMakeLists.txt — unit + integration tests (Catch2 v2, vendored).

add_library(veltrix_catch2 INTERFACE)
target_include_directories(veltrix_catch2 INTERFACE "${CMAKE_SOURCE_DIR}/third_party/catch2")

# One TU compiles the Catch2 runner; test TUs only include catch.hpp.
add_executable(veltrix-tests
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
veltrix_configure_target(veltrix-tests)
target_link_libraries(veltrix-tests PRIVATE veltrix-core veltrix_catch2)

add_test(NAME veltrix-tests COMMAND veltrix-tests)
set_tests_properties(veltrix-tests PROPERTIES TIMEOUT 120)
