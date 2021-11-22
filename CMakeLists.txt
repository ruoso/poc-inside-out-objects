cmake_minimum_required(VERSION 3.21)
project(cppioo LANGUAGES CXX)

enable_testing()
add_subdirectory(googletest)
add_subdirectory(src)
add_subdirectory(examples)
add_subdirectory(t)
