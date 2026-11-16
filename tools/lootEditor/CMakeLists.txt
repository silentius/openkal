cmake_minimum_required(VERSION 3.22 FATAL_ERROR)

if(DEFINED ENV{QTDIR})
  list(PREPEND CMAKE_PREFIX_PATH "$ENV{QTDIR}")
endif()

find_package(Qt6 6.9 REQUIRED COMPONENTS Widgets Gui)
qt_standard_project_setup()

set(LOOT_EDITOR_SOURCES
  main.cpp
  MainWindow.cpp
  ItemGroupsTab.cpp
  GroupsTab.cpp
  MonsterListTab.cpp
  DropGraphTab.cpp
  SimulatorTab.cpp
  model/LootDataStore.cpp
  model/LootMonsterStore.cpp
  model/LootPaths.cpp
  model/ItemIconCache.cpp
  model/ItemPickerListModel.cpp
  graph/DropGraphScene.cpp
  graph/ItemGroupNodeItem.cpp
  graph/GroupNodeItem.cpp
  graph/DropEdgeItem.cpp
  widgets/GroupEntriesWidget.cpp
  widgets/ItemIconOverlay.cpp
  widgets/MatchedItemsBar.cpp
  dialogs/GroupEditorDialog.cpp
  dialogs/ItemGroupEditorDialog.cpp
  dialogs/ItemPickerDialog.cpp
  dialogs/PrefixPickerDialog.cpp
  dialogs/FindItemGroupDialog.cpp
  dialogs/GroupUsageDialog.cpp
  dialogs/LootAuditDialog.cpp
  dialogs/NewIndexDialog.cpp
  dialogs/IdRangeFinderDialog.cpp
  dialogs/SettingsDialog.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/ItemDb.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/PrefixDb.cpp
)

add_executable(loot_editor ${LOOT_EDITOR_SOURCES})

target_link_libraries(loot_editor
  PRIVATE
    Qt6::Widgets
    openkal::platform
    openkal::server
)

target_include_directories(loot_editor
  PRIVATE
    ${CMAKE_SOURCE_DIR}/client/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(loot_editor PRIVATE cxx_std_17)

target_compile_definitions(loot_editor
  PRIVATE
    OPENKAL_REPO_ROOT="${OPENKAL_RESOURCE_ROOT}"
    OPENKAL_RUNTIME_DATA_ROOT="${OPENKAL_RUNTIME_DATA_ROOT}"
)
add_dependencies(loot_editor openkal_runtime_icons)

set_target_properties(loot_editor PROPERTIES
  AUTOMOC ON
  WIN32_EXECUTABLE ON
)
