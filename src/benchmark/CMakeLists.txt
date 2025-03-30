# Find Google Benchmark package
find_package(benchmark REQUIRED)

# Create benchmark executable
add_executable(cpioo_benchmark benchmark.cpp)

# Link against the benchmark library and your cpioo library
target_link_libraries(cpioo_benchmark
    PRIVATE
    benchmark::benchmark
    benchmark::benchmark_main
    cpioo
)

# Enable optimizations for benchmarking
target_compile_options(cpioo_benchmark PRIVATE -O3)

# Add pthread support needed for multithreaded benchmarks
find_package(Threads REQUIRED)
target_link_libraries(cpioo_benchmark PRIVATE Threads::Threads)
