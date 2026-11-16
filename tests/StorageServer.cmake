if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_executable(openkal_test_storage_server StorageServerTest.cpp)
  target_link_libraries(openkal_test_storage_server PRIVATE openkal::server Threads::Threads)
  openkal_target_warnings(openkal_test_storage_server)
  add_test(NAME storage_server COMMAND openkal_test_storage_server)
  set_tests_properties(storage_server PROPERTIES
    LABELS "optimization-coverage" TIMEOUT 90)

  if(OPENKAL_WITH_POSTGRES)
    target_link_libraries(openkal_test_storage_server PRIVATE PostgreSQL::PostgreSQL)
    target_compile_definitions(openkal_test_storage_server PRIVATE OPENKAL_STORAGE_POSTGRES=1)
    add_test(NAME storage_server_postgres COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/test_multiplayer_load.py"
      "$<TARGET_FILE:openkal_test_storage_server>")
    set_tests_properties(storage_server_postgres PROPERTIES
      LABELS "postgres-integration;optimization-coverage"
      SKIP_RETURN_CODE 77 TIMEOUT 120 RUN_SERIAL TRUE)
  endif()
endif()
