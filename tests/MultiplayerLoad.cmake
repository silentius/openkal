add_executable(openkal_test_host_runtime_metrics HostRuntimeMetricsTest.cpp)
target_link_libraries(openkal_test_host_runtime_metrics PRIVATE openkal::server Threads::Threads)
openkal_target_warnings(openkal_test_host_runtime_metrics)
add_test(NAME host_runtime_metrics COMMAND openkal_test_host_runtime_metrics)
set_tests_properties(host_runtime_metrics PROPERTIES
  LABELS "optimization-coverage" TIMEOUT 15)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_executable(openkal_multiplayer_load_test MultiplayerLoadTest.cpp)
  target_link_libraries(openkal_multiplayer_load_test PRIVATE openkal::server Threads::Threads)
  openkal_target_warnings(openkal_multiplayer_load_test)
  add_test(NAME multiplayer_load_sqlite COMMAND openkal_multiplayer_load_test
    --sqlite --clients 4 --rounds 3 --warmup 1)
  set_tests_properties(multiplayer_load_sqlite PROPERTIES
    LABELS "optimization-coverage" TIMEOUT 120)

  if(OPENKAL_WITH_POSTGRES)
    target_link_libraries(openkal_multiplayer_load_test PRIVATE PostgreSQL::PostgreSQL)
    target_compile_definitions(openkal_multiplayer_load_test PRIVATE OPENKAL_LOAD_POSTGRES=1)
    add_test(NAME multiplayer_load_postgres COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/test_multiplayer_load.py"
      "$<TARGET_FILE:openkal_multiplayer_load_test>"
      --clients 4 --rounds 3 --warmup 1)
    set_tests_properties(multiplayer_load_postgres PROPERTIES
      LABELS "postgres-integration;optimization-coverage"
      SKIP_RETURN_CODE 77 TIMEOUT 180 RUN_SERIAL TRUE)
  endif()
endif()
