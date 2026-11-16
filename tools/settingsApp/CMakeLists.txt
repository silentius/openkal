cmake_minimum_required(VERSION 3.22 FATAL_ERROR)

if(DEFINED ENV{QTDIR})
  list(PREPEND CMAKE_PREFIX_PATH "$ENV{QTDIR}")
endif()

find_package(Qt6 6.9 REQUIRED COMPONENTS Widgets Gui)
qt_standard_project_setup()

add_executable(openkal_settings
  main.cpp
  MainWindow.cpp
  MainWindow.h
)

target_link_libraries(openkal_settings
  PRIVATE
    Qt6::Widgets
    Qt6::Gui
    openkal::settings
    openkal::platform
)

target_compile_features(openkal_settings PRIVATE cxx_std_17)
target_compile_definitions(openkal_settings PRIVATE OPENKAL_REPO_ROOT="${OPENKAL_RESOURCE_ROOT}")
target_include_directories(openkal_settings PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
openkal_target_warnings(openkal_settings)

set_target_properties(openkal_settings PROPERTIES
  AUTOMOC ON
  WIN32_EXECUTABLE ON
)
