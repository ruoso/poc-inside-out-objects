add_library(
  cpioo
  cpioo/version.cpp
  cpioo/managed_entity.cpp
  )
target_include_directories(
  cpioo
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  )

# Add benchmark subdirectory if Google Benchmark is available
option(BUILD_BENCHMARKS "Build benchmarks" ON)
if(BUILD_BENCHMARKS)
    find_package(benchmark QUIET)
    if(benchmark_FOUND)
        add_subdirectory(benchmark)
    else()
        message(STATUS "Google Benchmark not found, skipping benchmarks")
    endif()
endif()

