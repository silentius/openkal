find_package(Freetype REQUIRED)

set(TILE_VIEW_SOURCES
  main.cpp
  TileView.cpp
  TileExporter.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiDocument.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/WidgetTools.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/FreetypeFont.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiBmp.cpp
)

add_executable(tile_view
  ${TILE_VIEW_SOURCES}
)

target_link_libraries(tile_view
  PRIVATE
    openkal::engine
    openkal::terrain
    openkal::map
    openkal::platform
    Freetype::Freetype
)

target_include_directories(tile_view
  PRIVATE
    ${CMAKE_SOURCE_DIR}/client/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(tile_view PRIVATE cxx_std_17)

target_compile_definitions(tile_view
  PRIVATE
    OPENKAL_REPO_ROOT="${OPENKAL_RESOURCE_ROOT}"
)
