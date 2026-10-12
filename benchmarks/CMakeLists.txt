# benchmarks/CMakeLists.txt — micro-benchmarks (harness próprio).

add_executable(veltrix-benchmarks bench_core.cpp)
veltrix_configure_target(veltrix-benchmarks)
target_link_libraries(veltrix-benchmarks PRIVATE veltrix-core)
