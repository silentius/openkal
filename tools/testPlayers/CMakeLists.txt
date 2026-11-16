find_package(Qt6 6.9 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()
add_library(openkal_test_player_agent STATIC
  TestPlayerAgent.cpp TestPlayerAgent.h TestPlayerNavigation.cpp TestPlayerNavigation.h
  TestPlayerSelectorIndex.cpp TestPlayerSelectorIndex.h
  ${CMAKE_SOURCE_DIR}/client/src/misc/DialogCatalog.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/TaskCatalog.cpp
  ${CMAKE_SOURCE_DIR}/client/src/player/Skills.cpp)
target_include_directories(openkal_test_player_agent PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE ${CMAKE_SOURCE_DIR}/client/include)
target_link_libraries(openkal_test_player_agent PUBLIC openkal::server openkal::terrain)
openkal_target_warnings(openkal_test_player_agent)
add_library(openkal_test_player_worker STATIC TestPlayerWorker.cpp TestPlayerWorker.h)
target_link_libraries(openkal_test_player_worker PUBLIC Qt6::Core openkal_test_player_agent)
openkal_target_warnings(openkal_test_player_worker)
add_executable(test_players main.cpp TestPlayerWindow.cpp TestPlayerWindow.h
  TestPlayerBackend.cpp TestPlayerBackend.h)
target_link_libraries(test_players PRIVATE Qt6::Widgets openkal_test_player_worker)
target_compile_definitions(test_players PRIVATE OPENKAL_SOURCE_ROOT="${CMAKE_SOURCE_DIR}"
  OPENKAL_REPO_ROOT="${OPENKAL_RESOURCE_ROOT}")
add_dependencies(test_players openkal_config_archive)
set_target_properties(test_players PROPERTIES AUTOMOC ON)
openkal_target_warnings(test_players)
if(OPENKAL_BUILD_TESTS)
  add_executable(openkal_test_player_agent_test
    ${CMAKE_SOURCE_DIR}/tests/TestPlayerAgentTest.cpp
    ${CMAKE_SOURCE_DIR}/client/src/ClientInput.cpp)
  target_include_directories(openkal_test_player_agent_test PRIVATE
    ${CMAKE_SOURCE_DIR}/tests ${CMAKE_SOURCE_DIR}/client/include)
  target_link_libraries(openkal_test_player_agent_test PRIVATE openkal_test_player_agent)
  set_target_properties(openkal_test_player_agent_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
  foreach(config IN ITEMS DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(openkal_test_player_agent_test PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY_${config} "${CMAKE_BINARY_DIR}/tests")
  endforeach()
  openkal_target_warnings(openkal_test_player_agent_test)
  add_test(NAME test_player_agent COMMAND openkal_test_player_agent_test)
  set_tests_properties(test_player_agent PROPERTIES TIMEOUT 60)
  add_executable(openkal_test_player_worker_test ${CMAKE_SOURCE_DIR}/tests/TestPlayerWorkerTest.cpp)
  target_include_directories(openkal_test_player_worker_test PRIVATE ${CMAKE_SOURCE_DIR}/tests)
  target_link_libraries(openkal_test_player_worker_test PRIVATE openkal_test_player_worker)
  set_target_properties(openkal_test_player_worker_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
  foreach(config IN ITEMS DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(openkal_test_player_worker_test PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY_${config} "${CMAKE_BINARY_DIR}/tests")
  endforeach()
  openkal_target_warnings(openkal_test_player_worker_test)
  add_test(NAME test_player_worker COMMAND openkal_test_player_worker_test)
  set_tests_properties(test_player_worker PROPERTIES TIMEOUT 30)
  add_test(NAME test_players_ui COMMAND test_players --smoke-test)
  set_tests_properties(test_players_ui PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-settings" TIMEOUT 15)
endif()

if(OPENKAL_BUILD_TESTS AND OPENKAL_WITH_POSTGRES)
  find_package(PostgreSQL REQUIRED)
  openkal_set_runtime_output("${CMAKE_BINARY_DIR}/tests")
  add_executable(openkal_test_player_integration
    ${CMAKE_SOURCE_DIR}/tests/TestPlayerIntegrationTest.cpp
    TestPlayerBackend.cpp TestPlayerBackend.h)
  target_include_directories(openkal_test_player_integration PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/tests)
  target_link_libraries(openkal_test_player_integration PRIVATE
    Qt6::Core openkal_test_player_worker PostgreSQL::PostgreSQL)
  target_compile_definitions(openkal_test_player_integration PRIVATE QT_NO_KEYWORDS
    OPENKAL_REPO_ROOT="${OPENKAL_RESOURCE_ROOT}")
  set_target_properties(openkal_test_player_integration PROPERTIES AUTOMOC ON)
  openkal_target_warnings(openkal_test_player_integration)
  add_test(NAME test_player_integration COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_SOURCE_DIR}/tests/test_postgres_multiplayer.py"
    "$<TARGET_FILE:openkal_test_player_integration>")
  set_tests_properties(test_player_integration PROPERTIES
    SKIP_RETURN_CODE 77 TIMEOUT 360 LABELS "postgres-integration" RUN_SERIAL TRUE)
endif()
