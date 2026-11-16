find_package(Freetype REQUIRED)

add_executable(three_d_item_viewer
  main.cpp
  ${CMAKE_SOURCE_DIR}/tools/object_view/ObjectCatalog.cpp
  ${CMAKE_SOURCE_DIR}/tools/object_view/ObjectView.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiDocument.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/WidgetTools.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/FreetypeFont.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiBmp.cpp
)

target_link_libraries(three_d_item_viewer
  PRIVATE
    openkal::engine
    openkal::terrain
    openkal::platform
    Freetype::Freetype
)

target_include_directories(three_d_item_viewer
  PRIVATE
    ${CMAKE_SOURCE_DIR}/client/include
    ${CMAKE_SOURCE_DIR}/tools/object_view
)

target_compile_features(three_d_item_viewer PRIVATE cxx_std_17)
target_compile_definitions(three_d_item_viewer
  PRIVATE
    OPENKAL_REPO_ROOT="${OPENKAL_RESOURCE_ROOT}"
)

set_target_properties(three_d_item_viewer PROPERTIES OUTPUT_NAME "3dItemViewer")
