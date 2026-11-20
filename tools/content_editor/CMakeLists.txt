if(DEFINED ENV{QTDIR})
  list(PREPEND CMAKE_PREFIX_PATH "$ENV{QTDIR}")
endif()

find_package(Qt6 6.9 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

add_library(openkal_content_editor STATIC
  Catalog.cpp JsonEditor.cpp JsonEditor.h MapView.cpp MapView.h
  PlacementPreview.cpp PlacementPreview.h ContentAudit.cpp QuestEditor.cpp QuestEditor.h
  ${CMAKE_SOURCE_DIR}/client/src/monster/MonsterCatalog.cpp
  ${CMAKE_SOURCE_DIR}/client/src/world/NpcCatalog.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/MinimapAreaName.cpp)
target_link_libraries(openkal_content_editor PUBLIC Qt6::Widgets openkal::terrain openkal_world_viewport)
target_include_directories(openkal_content_editor PRIVATE
  ${CMAKE_SOURCE_DIR}/client/include ${CMAKE_SOURCE_DIR}/libs/server/include)
openkal_target_warnings(openkal_content_editor)

add_executable(content_editor Main.cpp ContentWindow.cpp)
target_link_libraries(content_editor PRIVATE openkal_content_editor)
target_compile_definitions(content_editor PRIVATE
  OPENKAL_RESOURCE_ROOT="${OPENKAL_RESOURCE_ROOT}")
if(OPENKAL_RELEASE_BUILD OR CMAKE_BUILD_TYPE STREQUAL "Release")
  target_compile_definitions(content_editor PRIVATE
    OPENKAL_SOURCE_CONFIG="" OPENKAL_EXTERNAL_DATA_ROOT="")
else()
  target_compile_definitions(content_editor PRIVATE
    OPENKAL_SOURCE_CONFIG="${CMAKE_SOURCE_DIR}/config"
    OPENKAL_EXTERNAL_DATA_ROOT="${OPENKAL_DATA_ROOT}")
endif()
add_dependencies(content_editor openkal_config_archive)
set_target_properties(content_editor PROPERTIES WIN32_EXECUTABLE ON)
openkal_target_warnings(content_editor)

if(OPENKAL_BUILD_TESTS)
  openkal_set_runtime_output("${CMAKE_BINARY_DIR}/tests")
  add_executable(content_editor_test ContentEditorTest.cpp ContentWindow.cpp)
  target_include_directories(content_editor_test PRIVATE ${CMAKE_SOURCE_DIR}/tests)
  target_compile_definitions(content_editor_test PRIVATE OPENKAL_RESOURCE_ROOT="${CMAKE_BINARY_DIR}")
  target_link_libraries(content_editor_test PRIVATE openkal_content_editor)
  openkal_target_warnings(content_editor_test)
  add_test(NAME content_editor_test COMMAND content_editor_test)
  set_tests_properties(content_editor_test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
