if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_executable(openkal_test_world_event_server WorldEventServerTest.cpp)
  target_link_libraries(openkal_test_world_event_server PRIVATE
    openkal::server Threads::Threads)
  openkal_target_warnings(openkal_test_world_event_server)
  add_test(NAME world_event_server COMMAND openkal_test_world_event_server)
  set_tests_properties(world_event_server PROPERTIES
    LABELS "optimization-coverage" TIMEOUT 180 RUN_SERIAL TRUE)
endif()
