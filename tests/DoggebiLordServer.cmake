if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_executable(openkal_test_doggebi_lord_server DoggebiLordServerTest.cpp)
  target_link_libraries(openkal_test_doggebi_lord_server PRIVATE
    openkal::server Threads::Threads)
  openkal_target_warnings(openkal_test_doggebi_lord_server)
  add_test(NAME doggebi_lord_server COMMAND openkal_test_doggebi_lord_server)
  set_tests_properties(doggebi_lord_server PROPERTIES
    LABELS "optimization-coverage" TIMEOUT 90 RUN_SERIAL TRUE)
endif()
