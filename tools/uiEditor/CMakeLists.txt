cmake_minimum_required(VERSION 3.22 FATAL_ERROR)

if(DEFINED ENV{QTDIR})
  list(PREPEND CMAKE_PREFIX_PATH "$ENV{QTDIR}")
endif()

find_package(Qt6 6.9 REQUIRED COMPONENTS Widgets Gui)
find_package(Freetype REQUIRED)
qt_standard_project_setup()

set(UI_EDITOR_SOURCES
  main.cpp
  MainWindow.cpp
  UiCanvas.cpp
  BmpStore.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiDocument.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/TextWrap.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiBmp.cpp
)

add_executable(ui_editor ${UI_EDITOR_SOURCES})

target_link_libraries(ui_editor
  PRIVATE
    Qt6::Widgets
    Qt6::Gui
    openkal::platform
    openkal::terrain
    Freetype::Freetype
)

target_include_directories(ui_editor
  PRIVATE
    ${CMAKE_SOURCE_DIR}/client/include
    ${CMAKE_SOURCE_DIR}/engines/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(ui_editor PRIVATE cxx_std_17)

target_compile_definitions(ui_editor
  PRIVATE
    OPENKAL_REPO_ROOT="${OPENKAL_RESOURCE_ROOT}"
    OPENKAL_RUNTIME_DATA_ROOT="${OPENKAL_RUNTIME_DATA_ROOT}"
)
add_dependencies(ui_editor openkal_runtime_icons)

set_target_properties(ui_editor PROPERTIES
  AUTOMOC ON
  WIN32_EXECUTABLE ON
)
