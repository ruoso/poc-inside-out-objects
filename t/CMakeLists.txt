add_executable(${PROJECT_NAME}_tests
    001_version.t.cpp
    002_managed_array.t.cpp
    003_deeply_nested.t.cpp
)

target_link_libraries(${PROJECT_NAME}_tests cpioo gtest gtest_main)

include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME}_tests)
