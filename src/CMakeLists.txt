add_library(
  cpioo
  cpioo/version.cpp
  )
target_include_directories(
  cpioo
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  )

