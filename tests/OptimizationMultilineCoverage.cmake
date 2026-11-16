# Exercise the public multiline UiView caller with owned universal-UI documents.
add_executable(openkal_test_optimization_multiline
  OptimizationMultilineCoverageTest.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiDocument.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiLayout.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiView.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/WidgetTools.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiBmp.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/FreetypeFont.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/TextWrap.cpp)
target_include_directories(openkal_test_optimization_multiline PRIVATE
  ${CMAKE_SOURCE_DIR}/client/include)
target_link_libraries(openkal_test_optimization_multiline PRIVATE
  openkal::server openkal::terrain openkal::engine Freetype::Freetype)
openkal_target_warnings(openkal_test_optimization_multiline)
add_test(NAME optimization_multiline_coverage COMMAND openkal_test_optimization_multiline)
set_tests_properties(optimization_multiline_coverage PROPERTIES LABELS "optimization-coverage")
