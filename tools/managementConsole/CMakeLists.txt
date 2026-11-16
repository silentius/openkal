find_package(Qt6 6.9 REQUIRED COMPONENTS Widgets)
find_package(PostgreSQL REQUIRED)
qt_standard_project_setup()
add_executable(management_console Main.cpp ManagementWindow.cpp ManagementWindow.h
  ManagementBackend.cpp ManagementBackend.h)
target_link_libraries(management_console PRIVATE Qt6::Widgets openkal::server PostgreSQL::PostgreSQL)
set_target_properties(management_console PROPERTIES AUTOMOC ON)
openkal_target_warnings(management_console)
if(OPENKAL_BUILD_TESTS)
  add_test(NAME management_console_ui COMMAND management_console --smoke-test)
  set_tests_properties(management_console_ui PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-settings"
    TIMEOUT 15)
endif()

if(OPENKAL_BUILD_TESTS)
  openkal_set_runtime_output("${CMAKE_BINARY_DIR}/tests")
  add_executable(openkal_test_management_backend ${CMAKE_SOURCE_DIR}/tests/ManagementBackendTest.cpp
    ManagementBackend.cpp ManagementBackend.h)
  target_include_directories(openkal_test_management_backend PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/tests)
  target_link_libraries(openkal_test_management_backend PRIVATE Qt6::Core openkal::server PostgreSQL::PostgreSQL)
  set_target_properties(openkal_test_management_backend PROPERTIES AUTOMOC ON)
  openkal_target_warnings(openkal_test_management_backend)
  add_test(NAME management_backend COMMAND openkal_test_management_backend)
  set_tests_properties(management_backend PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 60 LABELS "postgres-integration")
endif()
