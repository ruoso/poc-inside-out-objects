foreach(
    TEST
    001_version
    )
  add_executable(${TEST}.t ${TEST}.t.cpp)
  target_link_libraries(${TEST}.t cpioo gtest gtest_main)
  add_test(${TEST} ${TEST}.t)
endforeach()
