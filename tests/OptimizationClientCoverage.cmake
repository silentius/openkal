# Characterization tests for the client changes in CodebaseReviewPlan.md.
# Reuse the established test source/link sets so these targets stay hermetic.
get_target_property(optimization_client_sources openkal_test_multiplayer_presentation SOURCES)
list(FILTER optimization_client_sources EXCLUDE REGEX "MultiplayerPresentationTest\\.cpp$")
add_executable(openkal_test_optimization_client OptimizationClientCoverageTest.cpp
  ${optimization_client_sources})
target_include_directories(openkal_test_optimization_client PRIVATE ${CMAKE_SOURCE_DIR}/client/include)
target_link_libraries(openkal_test_optimization_client PRIVATE
  openkal::server openkal::terrain openkal::engine Freetype::Freetype)
if(UNIX AND NOT APPLE)
  target_compile_definitions(openkal_test_optimization_client PRIVATE OPENKAL_TEST_WRAP_NEW=1)
  target_link_options(openkal_test_optimization_client PRIVATE "LINKER:--wrap=_Znwm")
endif()
openkal_target_warnings(openkal_test_optimization_client)
add_test(NAME optimization_client_coverage COMMAND openkal_test_optimization_client)

get_target_property(optimization_ui_sources openkal_test_inventory_hud SOURCES)
list(FILTER optimization_ui_sources EXCLUDE REGEX "InventoryHudTest\\.cpp$|/Inventory\\.cpp$")
add_executable(openkal_test_optimization_ui OptimizationUiCoverageTest.cpp ${optimization_ui_sources})
target_include_directories(openkal_test_optimization_ui PRIVATE ${CMAKE_SOURCE_DIR}/client/include)
target_link_libraries(openkal_test_optimization_ui PRIVATE
  openkal::server openkal::engine openkal::map openkal::terrain openkal::protocol
  openkal::platform Freetype::Freetype)
target_compile_definitions(openkal_test_optimization_ui PRIVATE
  OPENKAL_REPO_ROOT="${CMAKE_SOURCE_DIR}")
if(UNIX AND NOT APPLE)
  set_property(SOURCE OptimizationUiCoverageTest.cpp APPEND PROPERTY
    COMPILE_DEFINITIONS OPENKAL_TEST_WRAP_NEW=1)
  target_link_options(openkal_test_optimization_ui PRIVATE "LINKER:--wrap=_Znwm")
endif()
openkal_target_warnings(openkal_test_optimization_ui)
add_test(NAME optimization_ui_coverage COMMAND openkal_test_optimization_ui)
