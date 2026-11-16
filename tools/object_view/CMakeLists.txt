find_package(Freetype REQUIRED)

set(OBJECT_VIEW_SOURCES
  main.cpp
  ObjectCatalog.cpp
  ObjectView.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiDocument.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/WidgetTools.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/FreetypeFont.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiBmp.cpp
)

add_executable(object_view
  ${OBJECT_VIEW_SOURCES}
)

target_link_libraries(object_view
  PRIVATE
    openkal::engine
    openkal::terrain
    openkal::platform
    Freetype::Freetype
)

target_include_directories(object_view
  PRIVATE
    ${CMAKE_SOURCE_DIR}/client/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(object_view PRIVATE cxx_std_17)

target_compile_definitions(object_view
  PRIVATE
    OPENKAL_REPO_ROOT="${OPENKAL_RESOURCE_ROOT}"
)
