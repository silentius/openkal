if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_executable(openkal_test_mage_scheduler_server MageSchedulerServerTest.cpp)
  target_link_libraries(openkal_test_mage_scheduler_server PRIVATE
    openkal::server Threads::Threads)
  openkal_target_warnings(openkal_test_mage_scheduler_server)
  add_test(NAME mage_scheduler_server COMMAND openkal_test_mage_scheduler_server)
  set_tests_properties(mage_scheduler_server PROPERTIES
    LABELS "optimization-coverage" TIMEOUT 180 RUN_SERIAL TRUE)
endif()
