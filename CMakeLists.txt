cmake_minimum_required(VERSION 3.21)
project(cppioo LANGUAGES CXX)

# Sanitizer options
option(ENABLE_ASAN "Enable Address Sanitizer" OFF)
option(ENABLE_TSAN "Enable Thread Sanitizer" OFF)

# Cannot enable both sanitizers at the same time
if(ENABLE_ASAN AND ENABLE_TSAN)
  message(FATAL_ERROR "Cannot enable both Address Sanitizer and Thread Sanitizer at the same time")
endif()

# Configure sanitizers
if(ENABLE_ASAN)
  message(STATUS "Address Sanitizer enabled")
  add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address)
elseif(ENABLE_TSAN)
  message(STATUS "Thread Sanitizer enabled")
  add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
  add_link_options(-fsanitize=thread)
endif()

enable_testing()
add_subdirectory(googletest)
add_subdirectory(src)
#add_subdirectory(examples)
add_subdirectory(t)
