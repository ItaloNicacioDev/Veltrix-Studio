# benchmarks/CMakeLists.txt — micro-benchmarks (harness próprio).

add_executable(veltrix-benchmarks bench_core.cpp)
veltrix_configure_target(veltrix-benchmarks)
target_link_libraries(veltrix-benchmarks PRIVATE veltrix-core)

if(TARGET veltrix-ffmpeg)
    add_executable(veltrix-bench-decode bench_decode.cpp)
    veltrix_configure_target(veltrix-bench-decode)
    target_link_libraries(veltrix-bench-decode PRIVATE veltrix-core veltrix-ffmpeg)
    target_include_directories(veltrix-bench-decode PRIVATE ${CMAKE_SOURCE_DIR}/src)
endif()
