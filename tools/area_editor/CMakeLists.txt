cmake_minimum_required(VERSION 3.22 FATAL_ERROR)

if(DEFINED ENV{QTDIR})
  list(PREPEND CMAKE_PREFIX_PATH "$ENV{QTDIR}")
endif()

find_package(Qt6 6.9 REQUIRED COMPONENTS Widgets Gui)
qt_standard_project_setup()

add_executable(area_editor
  main.cpp
  AreaCanvas.cpp
  AreaEditorWindow.cpp
  bake.cpp
  layers.cpp
  ObjectMask.cpp
)

target_link_libraries(area_editor
  PRIVATE
    Qt6::Widgets
    openkal::engine
    openkal::terrain
    openkal::map
    openkal::platform
)

target_compile_features(area_editor PRIVATE cxx_std_17)

# GLFW comes transitively from openkal::engine (gl backend) for title/key helpers.

target_compile_definitions(area_editor
  PRIVATE
    OPENKAL_REPO_ROOT="${OPENKAL_RESOURCE_ROOT}"
)

set_target_properties(area_editor PROPERTIES
  AUTOMOC ON
  WIN32_EXECUTABLE ON
)
