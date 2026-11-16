cmake_minimum_required(VERSION 3.22 FATAL_ERROR)

if(DEFINED ENV{QTDIR})
  list(PREPEND CMAKE_PREFIX_PATH "$ENV{QTDIR}")
endif()

find_package(Qt6 6.9 REQUIRED COMPONENTS Widgets Gui)
qt_standard_project_setup()

add_executable(item_viewer
  main.cpp
  ItemViewerWindow.cpp
  ${CMAKE_SOURCE_DIR}/tools/lootEditor/model/ItemIconCache.cpp
  ${CMAKE_SOURCE_DIR}/tools/lootEditor/model/LootPaths.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/ItemDb.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/PrefixDb.cpp
)

target_link_libraries(item_viewer
  PRIVATE
    Qt6::Widgets
    openkal::platform
    openkal::server
)

target_include_directories(item_viewer
  PRIVATE
    ${CMAKE_SOURCE_DIR}/client/include
    ${CMAKE_SOURCE_DIR}/tools/lootEditor
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(item_viewer PRIVATE cxx_std_17)
target_compile_definitions(item_viewer
  PRIVATE
    OPENKAL_REPO_ROOT="${OPENKAL_RESOURCE_ROOT}"
)

set_target_properties(item_viewer PROPERTIES
  WIN32_EXECUTABLE ON
)
