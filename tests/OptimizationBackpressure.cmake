if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_executable(openkal_test_optimization_backpressure OptimizationBackpressureTest.cpp)
  target_link_libraries(openkal_test_optimization_backpressure PRIVATE openkal::server)
  openkal_target_warnings(openkal_test_optimization_backpressure)
  add_test(NAME optimization_backpressure COMMAND openkal_test_optimization_backpressure)
  set_tests_properties(optimization_backpressure PROPERTIES
    LABELS "optimization-coverage"
    TIMEOUT 15)
endif()
