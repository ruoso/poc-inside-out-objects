foreach(
    TEST
    001_version
    002_managed_array
    003_deeply_nested
    )
  add_executable(${TEST}.t ${TEST}.t.cpp)
  target_link_libraries(${TEST}.t cpioo gtest gtest_main)
  add_test(${TEST} ${TEST}.t)
endforeach()
