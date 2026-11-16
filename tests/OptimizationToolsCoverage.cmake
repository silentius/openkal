# Characterization tests for the concrete tool optimization sites.
add_executable(openkal_optimization_selector_test
  OptimizationSelectorCoverageTest.cpp
  ${CMAKE_SOURCE_DIR}/tools/testPlayers/TestPlayerSelectorIndex.cpp)
target_include_directories(openkal_optimization_selector_test PRIVATE
  ${CMAKE_SOURCE_DIR}/tools/testPlayers)
target_link_libraries(openkal_optimization_selector_test PRIVATE openkal::server)
if(UNIX AND NOT APPLE)
  target_compile_definitions(openkal_optimization_selector_test PRIVATE OPENKAL_TEST_WRAP_NEW=1)
  target_link_options(openkal_optimization_selector_test PRIVATE "LINKER:--wrap=_Znwm")
endif()
openkal_target_warnings(openkal_optimization_selector_test)
add_test(NAME optimization_selector COMMAND openkal_optimization_selector_test)
set_tests_properties(optimization_selector PROPERTIES LABELS "optimization-coverage")

if(OPENKAL_BUILD_TEST_PLAYERS OR OPENKAL_BUILD_LOOT_EDITOR)
  find_package(Qt6 6.9 REQUIRED COMPONENTS Core Widgets)
endif()
if(OPENKAL_BUILD_TEST_PLAYERS)
  add_executable(openkal_optimization_test_player_test
    OptimizationTestPlayerTest.cpp
    ${CMAKE_SOURCE_DIR}/client/src/ClientInput.cpp)
  target_include_directories(openkal_optimization_test_player_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/client/include)
  target_link_libraries(openkal_optimization_test_player_test PRIVATE
    Qt6::Core openkal_test_player_agent)
  openkal_target_warnings(openkal_optimization_test_player_test)
  add_test(NAME optimization_test_player COMMAND openkal_optimization_test_player_test)
  set_tests_properties(optimization_test_player PROPERTIES TIMEOUT 120)
endif()

if(OPENKAL_BUILD_LOOT_EDITOR)
  find_package(Qt6 6.9 REQUIRED COMPONENTS Test)
  set(picker_root ${CMAKE_SOURCE_DIR}/tools/lootEditor)
  add_executable(openkal_optimization_item_picker_test
    OptimizationItemPickerTest.cpp
    ${picker_root}/dialogs/ItemPickerDialog.cpp
    ${picker_root}/dialogs/ItemPickerDialog.h
    ${picker_root}/model/LootDataStore.cpp
    ${picker_root}/model/LootDataStore.h
    ${picker_root}/model/LootMonsterStore.cpp
    ${picker_root}/model/ItemIconCache.cpp
    ${picker_root}/model/ItemPickerListModel.cpp
    ${picker_root}/widgets/ItemIconOverlay.cpp
    ${CMAKE_SOURCE_DIR}/client/src/misc/ItemDb.cpp
    ${CMAKE_SOURCE_DIR}/client/src/misc/PrefixDb.cpp)
  target_include_directories(openkal_optimization_item_picker_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR} ${picker_root} ${CMAKE_SOURCE_DIR}/client/include)
  target_link_libraries(openkal_optimization_item_picker_test PRIVATE
    Qt6::Widgets Qt6::Test openkal::platform openkal::server)
  set_target_properties(openkal_optimization_item_picker_test PROPERTIES AUTOMOC ON)
  openkal_target_warnings(openkal_optimization_item_picker_test)
  add_test(NAME optimization_item_picker COMMAND openkal_optimization_item_picker_test)
  add_test(NAME optimization_item_picker_hidpi COMMAND openkal_optimization_item_picker_test)
  set_tests_properties(optimization_item_picker PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60)
  set_tests_properties(optimization_item_picker_hidpi PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_SCALE_FACTOR=2" TIMEOUT 60)

  add_executable(openkal_item_picker_model_test
    ItemPickerListModelTest.cpp
    ${picker_root}/model/ItemPickerListModel.cpp
    ${picker_root}/model/LootDataStore.cpp
    ${picker_root}/model/LootDataStore.h
    ${picker_root}/model/LootMonsterStore.cpp
    ${picker_root}/model/ItemIconCache.cpp
    ${picker_root}/widgets/ItemIconOverlay.cpp
    ${CMAKE_SOURCE_DIR}/client/src/misc/ItemDb.cpp
    ${CMAKE_SOURCE_DIR}/client/src/misc/PrefixDb.cpp)
  target_include_directories(openkal_item_picker_model_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR} ${picker_root} ${CMAKE_SOURCE_DIR}/client/include)
  target_link_libraries(openkal_item_picker_model_test PRIVATE
    Qt6::Widgets Qt6::Test openkal::platform openkal::server)
  set_target_properties(openkal_item_picker_model_test PROPERTIES AUTOMOC ON)
  openkal_target_warnings(openkal_item_picker_model_test)
  add_test(NAME item_picker_model COMMAND openkal_item_picker_model_test)
  add_test(NAME item_picker_model_hidpi COMMAND openkal_item_picker_model_test)
  set_tests_properties(item_picker_model PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60)
  set_tests_properties(item_picker_model_hidpi PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_SCALE_FACTOR=2" TIMEOUT 60)
endif()
