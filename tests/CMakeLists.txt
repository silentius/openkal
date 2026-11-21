add_executable(openkal_test_platform test_platform.cpp)
target_link_libraries(openkal_test_platform PRIVATE openkal::platform openkal::map)
target_compile_definitions(openkal_test_platform PRIVATE
  OPENKAL_REPO_ROOT="${CMAKE_SOURCE_DIR}")
openkal_target_warnings(openkal_test_platform)
add_test(NAME platform COMMAND openkal_test_platform)

add_executable(openkal_test_config_archive test_config_archive.cpp)
target_link_libraries(openkal_test_config_archive PRIVATE openkal::platform openkal::map)
openkal_target_warnings(openkal_test_config_archive)
add_test(NAME config_archive COMMAND openkal_test_config_archive)

add_executable(openkal_test_harvest_plant_asset HarvestPlantAssetTest.cpp)
target_link_libraries(openkal_test_harvest_plant_asset PRIVATE openkal::terrain)
target_compile_definitions(openkal_test_harvest_plant_asset PRIVATE
  OPENKAL_TEST_CONFIG_ARCHIVE="${OPENKAL_CONFIG_ARCHIVE}")
add_dependencies(openkal_test_harvest_plant_asset openkal_config_archive)
openkal_target_warnings(openkal_test_harvest_plant_asset)
add_test(NAME harvest_plant_asset COMMAND openkal_test_harvest_plant_asset)

add_executable(openkal_test_harvest_plants HarvestPlantsTest.cpp)
target_link_libraries(openkal_test_harvest_plants PRIVATE openkal::server)
target_compile_definitions(openkal_test_harvest_plants PRIVATE OPENKAL_REPO_ROOT="${CMAKE_SOURCE_DIR}")
openkal_target_warnings(openkal_test_harvest_plants)
add_test(NAME harvest_plants COMMAND openkal_test_harvest_plants)

add_test(NAME runtime_icon_conversion
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_runtime_icons.py")

foreach(category IN ITEMS item pet quest)
  if(category STREQUAL "quest")
    set(converter_test "test_convert_dialogs.py")
  elseif(category STREQUAL "item")
    set(converter_test "test_convert_items.py")
  else()
    set(converter_test "test_convert_pets.py")
  endif()
  add_test(NAME config_converter_${category}
    COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_SOURCE_DIR}/tools/${category}_config_convert/${converter_test}")
endforeach()

add_executable(openkal_test_config_converters
  ConfigConverterCompatibilityTest.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/ItemDb.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/PrefixDb.cpp
  ${CMAKE_SOURCE_DIR}/client/src/world/NpcCatalog.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/TaskCatalog.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/DialogCatalog.cpp)
target_include_directories(openkal_test_config_converters PRIVATE ${CMAKE_SOURCE_DIR}/client/include)
target_link_libraries(openkal_test_config_converters PRIVATE openkal::server openkal::platform)
openkal_target_warnings(openkal_test_config_converters)
add_test(NAME config_converter_compatibility
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_config_converters.py"
    $<TARGET_FILE:openkal_test_config_converters>)

add_test(NAME skin_catalog_audit
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_audit_skins.py")
if(TARGET skin_asset_probe)
  set_tests_properties(skin_catalog_audit PROPERTIES
    ENVIRONMENT "OPENKAL_SKIN_PROBE=$<TARGET_FILE:skin_asset_probe>")
endif()

add_executable(openkal_test_skin_asset_inspection SkinAssetInspectionTest.cpp
  ${CMAKE_SOURCE_DIR}/tools/skin_audit/SkinAssetInspection.cpp)
target_include_directories(openkal_test_skin_asset_inspection PRIVATE
  ${CMAKE_SOURCE_DIR}/tools/skin_audit)
target_link_libraries(openkal_test_skin_asset_inspection PRIVATE openkal::terrain openkal::platform)
openkal_target_warnings(openkal_test_skin_asset_inspection)
add_test(NAME skin_asset_inspection COMMAND openkal_test_skin_asset_inspection)

add_executable(openkal_test_texture_formats TextureFormatsTest.cpp)
target_link_libraries(openkal_test_texture_formats PRIVATE openkal::terrain PNG::PNG)
openkal_target_warnings(openkal_test_texture_formats)
add_test(NAME texture_formats COMMAND openkal_test_texture_formats)

add_executable(openkal_test_gb_material GbMaterialTest.cpp)
target_link_libraries(openkal_test_gb_material PRIVATE openkal::terrain openkal::platform)
openkal_target_warnings(openkal_test_gb_material)
add_test(NAME gb_material COMMAND openkal_test_gb_material)

add_executable(openkal_test_transparent_mesh TransparentMeshTest.cpp
  ${CMAKE_SOURCE_DIR}/engines/TransparentMesh.cpp)
target_include_directories(openkal_test_transparent_mesh PRIVATE ${CMAKE_SOURCE_DIR}/engines/include)
openkal_target_warnings(openkal_test_transparent_mesh)
add_test(NAME transparent_mesh COMMAND openkal_test_transparent_mesh)

add_test(NAME skin_preview_selection
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_skin_previews.py")

add_executable(openkal_test_runtime_icons
  RuntimeIconsTest.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/FreetypeFont.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/WidgetTools.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiBmp.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiDocument.cpp
  ${CMAKE_SOURCE_DIR}/engines/null/null_engine.cpp)
target_include_directories(openkal_test_runtime_icons PRIVATE
  ${CMAKE_SOURCE_DIR}/client/include
  ${CMAKE_SOURCE_DIR}/engines/include)
target_link_libraries(openkal_test_runtime_icons PRIVATE
  openkal::terrain openkal::platform openkal::protocol Freetype::Freetype)
target_compile_definitions(openkal_test_runtime_icons PRIVATE
  OPENKAL_TEST_CONFIG_ARCHIVE="${OPENKAL_CONFIG_ARCHIVE}"
  OPENKAL_TEST_GENERATED_RUNTIME_ROOT="${OPENKAL_GENERATED_RUNTIME_ROOT}")
add_dependencies(openkal_test_runtime_icons openkal_config_archive)
openkal_target_warnings(openkal_test_runtime_icons)
add_test(NAME runtime_icons COMMAND openkal_test_runtime_icons)

add_executable(openkal_test_client_settings test_client_settings.cpp)
target_link_libraries(openkal_test_client_settings PRIVATE openkal::settings openkal::platform)
openkal_target_warnings(openkal_test_client_settings)
add_test(NAME client_settings COMMAND openkal_test_client_settings)

add_executable(openkal_test_client_runtime_config
  ClientRuntimeConfigTest.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ClientRuntimeConfig.cpp)
target_include_directories(openkal_test_client_runtime_config PRIVATE ${CMAKE_SOURCE_DIR}/client/include)
target_link_libraries(openkal_test_client_runtime_config PRIVATE openkal::settings openkal::platform)
target_compile_definitions(openkal_test_client_runtime_config PRIVATE OPENKAL_REPO_ROOT="${CMAKE_BINARY_DIR}")
add_dependencies(openkal_test_client_runtime_config openkal_config_archive)
openkal_target_warnings(openkal_test_client_runtime_config)
add_test(NAME client_runtime_config COMMAND openkal_test_client_runtime_config)

add_executable(openkal_test_multiplayer_presentation
  MultiplayerPresentationTest.cpp
  ${CMAKE_SOURCE_DIR}/client/src/player/Player.cpp
  ${CMAKE_SOURCE_DIR}/client/src/player/Equipment.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/NetClient.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/ServerClock.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/JobTitles.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/ItemDb.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/PrefixDb.cpp
  ${CMAKE_SOURCE_DIR}/client/src/player/Skills.cpp
  ${CMAKE_SOURCE_DIR}/client/src/player/SkillEffectTable.cpp
  ${CMAKE_SOURCE_DIR}/client/src/world/DebugOverlay.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/BuffSlotBar.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/WidgetTools.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiBmp.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiDocument.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/BattleMsgBuilder.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/OverheadChat.cpp
  ${CMAKE_SOURCE_DIR}/client/src/player/RemotePlayerMotion.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/TextWrap.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/FreetypeFont.cpp)
target_include_directories(openkal_test_multiplayer_presentation PRIVATE ${CMAKE_SOURCE_DIR}/client/include)
target_link_libraries(openkal_test_multiplayer_presentation PRIVATE openkal::server openkal::terrain openkal::engine Freetype::Freetype)
openkal_target_warnings(openkal_test_multiplayer_presentation)
add_test(NAME multiplayer_presentation COMMAND openkal_test_multiplayer_presentation)

add_executable(openkal_test_remote_player_motion
  RemotePlayerMotionTest.cpp
  ${CMAKE_SOURCE_DIR}/client/src/player/RemotePlayerMotion.cpp)
target_include_directories(openkal_test_remote_player_motion PRIVATE
  ${CMAKE_SOURCE_DIR}/client/include ${CMAKE_SOURCE_DIR}/engines/include)
openkal_target_warnings(openkal_test_remote_player_motion)
add_test(NAME remote_player_motion COMMAND openkal_test_remote_player_motion)

if(OPENKAL_ENGINE STREQUAL "gl")
  add_executable(openkal_test_mesh_specular MeshSpecularTest.cpp)
  target_link_libraries(openkal_test_mesh_specular PRIVATE openkal::engine)
  openkal_target_warnings(openkal_test_mesh_specular)
  add_test(NAME mesh_specular COMMAND openkal_test_mesh_specular)
  set_tests_properties(mesh_specular PROPERTIES SKIP_RETURN_CODE 77)

  add_executable(openkal_test_text_edit_input TextEditInputTest.cpp)
  target_link_libraries(openkal_test_text_edit_input PRIVATE openkal::engine)
  openkal_target_warnings(openkal_test_text_edit_input)
  add_test(NAME text_edit_input COMMAND openkal_test_text_edit_input)
  set_tests_properties(text_edit_input PROPERTIES SKIP_RETURN_CODE 77)

  add_executable(openkal_test_hit_feedback HitFeedbackTest.cpp)
  target_link_libraries(openkal_test_hit_feedback PRIVATE openkal::engine)
  openkal_target_warnings(openkal_test_hit_feedback)
  add_test(NAME hit_feedback COMMAND openkal_test_hit_feedback)
  set_tests_properties(hit_feedback PROPERTIES SKIP_RETURN_CODE 77)

  add_executable(openkal_test_world_label_depth WorldLabelDepthTest.cpp)
  target_link_libraries(openkal_test_world_label_depth PRIVATE openkal::engine)
  openkal_target_warnings(openkal_test_world_label_depth)
  add_test(NAME world_label_depth COMMAND openkal_test_world_label_depth)
  set_tests_properties(world_label_depth PROPERTIES SKIP_RETURN_CODE 77)

  add_executable(openkal_test_shadow_mask ShadowMaskTest.cpp)
  target_link_libraries(openkal_test_shadow_mask PRIVATE openkal::engine)
  openkal_target_warnings(openkal_test_shadow_mask)
  add_test(NAME shadow_mask COMMAND openkal_test_shadow_mask)
  set_tests_properties(shadow_mask PROPERTIES SKIP_RETURN_CODE 77)
endif()

add_executable(openkal_test_in_game_window_order
  test_in_game_window_order.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/InGameWindowOrder.cpp)
target_include_directories(openkal_test_in_game_window_order PRIVATE
  ${CMAKE_SOURCE_DIR}/client/include)
openkal_target_warnings(openkal_test_in_game_window_order)
add_test(NAME in_game_window_order COMMAND openkal_test_in_game_window_order)

add_executable(openkal_test_pet_controller
  test_pet_controller.cpp
  ${CMAKE_SOURCE_DIR}/client/src/player/PetController.cpp)
target_include_directories(openkal_test_pet_controller PRIVATE
  ${CMAKE_SOURCE_DIR}/client/include)
openkal_target_warnings(openkal_test_pet_controller)
add_test(NAME pet_controller COMMAND openkal_test_pet_controller)

add_executable(openkal_test_map
  test_map.cpp
  ${CMAKE_SOURCE_DIR}/tools/area_import/LegacyAreaImporter.cpp)
target_include_directories(openkal_test_map PRIVATE
  ${CMAKE_SOURCE_DIR}/tools/area_import)
target_link_libraries(openkal_test_map PRIVATE openkal::map openkal::platform)
target_compile_definitions(openkal_test_map PRIVATE
  OPENKAL_REPO_ROOT="${CMAKE_SOURCE_DIR}")
openkal_target_warnings(openkal_test_map)
add_test(NAME map COMMAND openkal_test_map)

add_executable(openkal_generate_area_fixture EXCLUDE_FROM_ALL map/GenerateAreaFixture.cpp)
target_link_libraries(openkal_generate_area_fixture PRIVATE openkal::map)
openkal_target_warnings(openkal_generate_area_fixture)

add_executable(openkal_test_area_map_fixture AreaMapFixtureTest.cpp)
target_link_libraries(openkal_test_area_map_fixture PRIVATE openkal::map)
target_compile_definitions(openkal_test_area_map_fixture PRIVATE
  OPENKAL_AREA_FIXTURE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/map/fixtures/n_000_000.akarea")
openkal_target_warnings(openkal_test_area_map_fixture)
add_test(NAME area_map_fixture COMMAND openkal_test_area_map_fixture)

if(TARGET area_import)
  add_test(NAME area_import_player_collision
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/map/test_area_import.py"
      $<TARGET_FILE:area_import>)
endif()

add_executable(openkal_test_protocol test_protocol.cpp)
target_link_libraries(openkal_test_protocol PRIVATE openkal::protocol)
openkal_target_warnings(openkal_test_protocol)
add_test(NAME protocol COMMAND openkal_test_protocol)

add_executable(openkal_test_speed_medicine test_speed_medicine.cpp)
target_link_libraries(openkal_test_speed_medicine PRIVATE openkal::server)
openkal_target_warnings(openkal_test_speed_medicine)
add_test(NAME speed_medicine COMMAND openkal_test_speed_medicine)

add_executable(openkal_test_assets test_assets.cpp)
target_link_libraries(openkal_test_assets PRIVATE openkal::map openkal::terrain openkal::platform)
target_compile_definitions(openkal_test_assets PRIVATE
  OPENKAL_REPO_ROOT="${CMAKE_SOURCE_DIR}")
openkal_target_warnings(openkal_test_assets)
add_test(NAME assets COMMAND openkal_test_assets)

add_executable(openkal_test_opl test_opl.cpp)
target_link_libraries(openkal_test_opl PRIVATE openkal::terrain openkal::platform)
target_compile_definitions(openkal_test_opl PRIVATE
  OPENKAL_REPO_ROOT="${CMAKE_SOURCE_DIR}")
openkal_target_warnings(openkal_test_opl)
add_test(NAME opl COMMAND openkal_test_opl)

add_executable(openkal_test_gb test_gb.cpp)
target_link_libraries(openkal_test_gb PRIVATE openkal::terrain openkal::platform)
target_compile_definitions(openkal_test_gb PRIVATE
  OPENKAL_REPO_ROOT="${CMAKE_SOURCE_DIR}")
openkal_target_warnings(openkal_test_gb)
add_test(NAME gb COMMAND openkal_test_gb)

add_executable(openkal_test_height_field test_height_field.cpp)
target_link_libraries(openkal_test_height_field PRIVATE openkal::terrain)
openkal_target_warnings(openkal_test_height_field)
add_test(NAME height_field COMMAND openkal_test_height_field)

add_executable(openkal_test_area_editor_object_mask
  test_area_editor_object_mask.cpp
  ${CMAKE_SOURCE_DIR}/tools/area_editor/ObjectMask.cpp
  ${CMAKE_SOURCE_DIR}/tools/area_editor/layers.cpp)
target_include_directories(openkal_test_area_editor_object_mask PRIVATE
  ${CMAKE_SOURCE_DIR}/tools/area_editor)
target_link_libraries(openkal_test_area_editor_object_mask PRIVATE openkal::engine openkal::terrain)
target_compile_definitions(openkal_test_area_editor_object_mask PRIVATE
  OPENKAL_REPO_ROOT="${CMAKE_SOURCE_DIR}")
openkal_target_warnings(openkal_test_area_editor_object_mask)
add_test(NAME area_editor_object_mask COMMAND openkal_test_area_editor_object_mask)
add_test(NAME area_editor_user_data COMMAND openkal_test_area_editor_object_mask --user-data)

add_executable(openkal_test_nameplate
  test_nameplate.cpp
  ${CMAKE_SOURCE_DIR}/client/src/world/Nameplate.cpp)
target_include_directories(openkal_test_nameplate PRIVATE
  ${CMAKE_SOURCE_DIR}/client/include
  ${CMAKE_SOURCE_DIR}/engines/include)
target_link_libraries(openkal_test_nameplate PRIVATE openkal::terrain openkal::protocol)
openkal_target_warnings(openkal_test_nameplate)
add_test(NAME nameplate COMMAND openkal_test_nameplate)

add_executable(openkal_test_combat_float_text
  test_combat_float_text.cpp
  ${CMAKE_SOURCE_DIR}/client/src/world/CombatFloatText.cpp)
target_include_directories(openkal_test_combat_float_text PRIVATE
  ${CMAKE_SOURCE_DIR}/client/include
  ${CMAKE_SOURCE_DIR}/engines/include)
target_link_libraries(openkal_test_combat_float_text PRIVATE openkal::terrain openkal::protocol)
openkal_target_warnings(openkal_test_combat_float_text)
add_test(NAME combat_float_text COMMAND openkal_test_combat_float_text)

add_executable(openkal_test_floor
  test_floor.cpp)
target_include_directories(openkal_test_floor PRIVATE
  ${CMAKE_SOURCE_DIR}/client/include
  ${CMAKE_SOURCE_DIR}/engines/include)
target_link_libraries(openkal_test_floor PRIVATE openkal::terrain openkal::map)
openkal_target_warnings(openkal_test_floor)
add_test(NAME floor COMMAND openkal_test_floor)

add_executable(openkal_test_player_movement
  test_player_movement.cpp
  ${CMAKE_SOURCE_DIR}/client/src/player/Player.cpp
  ${CMAKE_SOURCE_DIR}/client/src/player/Equipment.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/ItemDb.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/PrefixDb.cpp)
target_include_directories(openkal_test_player_movement PRIVATE
  ${CMAKE_SOURCE_DIR}/client/include
  ${CMAKE_SOURCE_DIR}/engines/include)
target_link_libraries(openkal_test_player_movement PRIVATE
  openkal::terrain openkal::map openkal::platform openkal::protocol openkal::server)
target_compile_definitions(openkal_test_player_movement PRIVATE
  OPENKAL_REPO_ROOT="${CMAKE_SOURCE_DIR}")
openkal_target_warnings(openkal_test_player_movement)
add_test(NAME player_movement COMMAND openkal_test_player_movement)

add_executable(openkal_test_player_bridge_movement
  PlayerBridgeMovementTest.cpp
  ${CMAKE_SOURCE_DIR}/client/src/player/Player.cpp
  ${CMAKE_SOURCE_DIR}/client/src/player/Equipment.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/ItemDb.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/PrefixDb.cpp)
target_include_directories(openkal_test_player_bridge_movement PRIVATE ${CMAKE_SOURCE_DIR}/client/include)
target_link_libraries(openkal_test_player_bridge_movement PRIVATE openkal::terrain openkal::server)
openkal_target_warnings(openkal_test_player_bridge_movement)
add_test(NAME player_bridge_movement COMMAND openkal_test_player_bridge_movement)

add_executable(openkal_test_dungeon_safety
  DungeonSafetyTest.cpp
  ${CMAKE_SOURCE_DIR}/client/src/player/Player.cpp
  ${CMAKE_SOURCE_DIR}/client/src/player/Equipment.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/ItemDb.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/PrefixDb.cpp)
target_include_directories(openkal_test_dungeon_safety PRIVATE ${CMAKE_SOURCE_DIR}/client/include)
target_link_libraries(openkal_test_dungeon_safety PRIVATE openkal::terrain openkal::server)
openkal_target_warnings(openkal_test_dungeon_safety)
add_test(NAME dungeon_safety COMMAND openkal_test_dungeon_safety)

add_executable(openkal_test_dungeon_server DungeonServerTest.cpp)
target_link_libraries(openkal_test_dungeon_server PRIVATE openkal::server)
openkal_target_warnings(openkal_test_dungeon_server)
add_test(NAME dungeon_server COMMAND openkal_test_dungeon_server)

add_executable(openkal_test_server test_server_fast.cpp)
target_link_libraries(openkal_test_server PRIVATE openkal::server openkal::platform)
target_compile_definitions(openkal_test_server PRIVATE
  OPENKAL_REPO_ROOT="${CMAKE_SOURCE_DIR}"
  OPENKAL_TEST_RESOURCE_ROOT="${CMAKE_CURRENT_BINARY_DIR}/resources")
openkal_target_warnings(openkal_test_server)
add_test(NAME server COMMAND openkal_test_server)

add_executable(openkal_test_monster_strength MonsterStrengthTest.cpp)
target_link_libraries(openkal_test_monster_strength PRIVATE openkal::server openkal::platform)
target_compile_definitions(openkal_test_monster_strength PRIVATE
  OPENKAL_REPO_ROOT="${CMAKE_SOURCE_DIR}")
openkal_target_warnings(openkal_test_monster_strength)
add_test(NAME monster_strength COMMAND openkal_test_monster_strength)

add_executable(openkal_test_server_event test_server_event.cpp)
target_link_libraries(openkal_test_server_event PRIVATE openkal::server openkal::platform)
target_compile_definitions(openkal_test_server_event PRIVATE
  OPENKAL_REPO_ROOT="${CMAKE_SOURCE_DIR}")
openkal_target_warnings(openkal_test_server_event)
add_test(NAME server_event COMMAND openkal_test_server_event)

add_executable(openkal_test_battle_riding test_battle_riding.cpp)
target_link_libraries(openkal_test_battle_riding PRIVATE openkal::server)
openkal_target_warnings(openkal_test_battle_riding)
add_test(NAME battle_riding COMMAND openkal_test_battle_riding)

add_executable(openkal_test_shop_catalog test_shop_catalog.cpp)
target_link_libraries(openkal_test_shop_catalog PRIVATE openkal::server openkal::platform)
target_compile_definitions(openkal_test_shop_catalog PRIVATE
  OPENKAL_REPO_ROOT="${CMAKE_SOURCE_DIR}")
openkal_target_warnings(openkal_test_shop_catalog)
add_test(NAME shop_catalog COMMAND openkal_test_shop_catalog)

add_executable(openkal_test_fishing_catalog test_fishing_catalog.cpp)
target_link_libraries(openkal_test_fishing_catalog PRIVATE openkal::server openkal::platform)
target_compile_definitions(openkal_test_fishing_catalog PRIVATE
  OPENKAL_REPO_ROOT="${CMAKE_SOURCE_DIR}")
openkal_target_warnings(openkal_test_fishing_catalog)
add_test(NAME fishing_catalog COMMAND openkal_test_fishing_catalog)

add_executable(openkal_test_stone_buffs test_stone_buffs.cpp)
target_link_libraries(openkal_test_stone_buffs PRIVATE openkal::server)
openkal_target_warnings(openkal_test_stone_buffs)
add_test(NAME stone_buffs COMMAND openkal_test_stone_buffs)

add_executable(openkal_test_riding_cache_catalog test_riding_cache_catalog.cpp)
target_link_libraries(openkal_test_riding_cache_catalog PRIVATE openkal::server openkal::platform)
target_compile_definitions(openkal_test_riding_cache_catalog PRIVATE
  OPENKAL_REPO_ROOT="${CMAKE_SOURCE_DIR}")
openkal_target_warnings(openkal_test_riding_cache_catalog)
add_test(NAME riding_cache_catalog COMMAND openkal_test_riding_cache_catalog)

add_executable(openkal_test_level_mail_rewards test_level_mail_rewards.cpp)
target_link_libraries(openkal_test_level_mail_rewards PRIVATE openkal::server openkal::platform)
target_compile_definitions(openkal_test_level_mail_rewards PRIVATE
  OPENKAL_REPO_ROOT="${CMAKE_SOURCE_DIR}")
openkal_target_warnings(openkal_test_level_mail_rewards)
add_test(NAME level_mail_rewards COMMAND openkal_test_level_mail_rewards)

option(OPENKAL_BUILD_SERVER_CONFIG_TESTS
  "Build server JSON configuration integration tests" OFF)
if(OPENKAL_BUILD_SERVER_CONFIG_TESTS)
  add_executable(openkal_test_server_config test_server.cpp)
  target_link_libraries(openkal_test_server_config PRIVATE openkal::server openkal::platform)
  target_compile_definitions(openkal_test_server_config PRIVATE
    OPENKAL_REPO_ROOT="${CMAKE_SOURCE_DIR}")
  openkal_target_warnings(openkal_test_server_config)
  add_test(NAME server_config COMMAND openkal_test_server_config)
  set_tests_properties(server_config PROPERTIES LABELS "server-config")
endif()

add_executable(openkal_test_monster_occupancy test_monster_occupancy.cpp)
target_link_libraries(openkal_test_monster_occupancy PRIVATE openkal::server openkal::platform)
target_compile_definitions(openkal_test_monster_occupancy PRIVATE
  OPENKAL_REPO_ROOT="${CMAKE_SOURCE_DIR}")
openkal_target_warnings(openkal_test_monster_occupancy)
add_test(NAME monster_occupancy COMMAND openkal_test_monster_occupancy)

add_executable(openkal_test_monster_refill MonsterRefillTest.cpp)
target_link_libraries(openkal_test_monster_refill PRIVATE openkal::server openkal::platform)
openkal_target_warnings(openkal_test_monster_refill)
add_test(NAME monster_refill COMMAND openkal_test_monster_refill)

add_executable(openkal_test_monster_fx
  test_monster_fx.cpp
  ${CMAKE_SOURCE_DIR}/client/src/monster/MonsterCatalog.cpp)
target_include_directories(openkal_test_monster_fx PRIVATE
  ${CMAKE_SOURCE_DIR}/client/include
  ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(openkal_test_monster_fx PRIVATE openkal::platform)
target_compile_definitions(openkal_test_monster_fx PRIVATE
  OPENKAL_REPO_ROOT="${CMAKE_SOURCE_DIR}")
openkal_target_warnings(openkal_test_monster_fx)
add_test(NAME monster_fx COMMAND openkal_test_monster_fx)

add_executable(openkal_test_world_object_catalog
  test_world_object_catalog.cpp
  ${CMAKE_SOURCE_DIR}/client/src/world/WorldObjectCatalog.cpp)
target_include_directories(openkal_test_world_object_catalog PRIVATE
  ${CMAKE_SOURCE_DIR}/client/include
  ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(openkal_test_world_object_catalog PRIVATE openkal::platform)
target_compile_definitions(openkal_test_world_object_catalog PRIVATE
  OPENKAL_REPO_ROOT="${CMAKE_SOURCE_DIR}")
openkal_target_warnings(openkal_test_world_object_catalog)
add_test(NAME world_object_catalog COMMAND openkal_test_world_object_catalog)

add_executable(openkal_test_monster_action_damage test_monster_action_damage.cpp)
target_include_directories(openkal_test_monster_action_damage PRIVATE
  ${CMAKE_SOURCE_DIR}/client/include
  ${CMAKE_SOURCE_DIR}/engines/include)
target_link_libraries(openkal_test_monster_action_damage PRIVATE
  openkal::terrain
  openkal::protocol)
openkal_target_warnings(openkal_test_monster_action_damage)
add_test(NAME monster_action_damage COMMAND openkal_test_monster_action_damage)

add_executable(openkal_test_player_regen test_player_regen.cpp)
target_link_libraries(openkal_test_player_regen PRIVATE openkal::server)
openkal_target_warnings(openkal_test_player_regen)
add_test(NAME player_regen COMMAND openkal_test_player_regen)

add_executable(openkal_test_player_skill test_player_skill.cpp)
target_link_libraries(openkal_test_player_skill PRIVATE openkal::server)
openkal_target_warnings(openkal_test_player_skill)
add_test(NAME player_skill COMMAND openkal_test_player_skill)

add_executable(openkal_test_skill_ability_tooltip
  test_skill_ability_tooltip.cpp
  ${CMAKE_SOURCE_DIR}/client/src/player/Skills.cpp
  ${CMAKE_SOURCE_DIR}/client/src/player/SkillEffectTable.cpp)
target_include_directories(openkal_test_skill_ability_tooltip PRIVATE
  ${CMAKE_SOURCE_DIR}/client/include)
target_link_libraries(openkal_test_skill_ability_tooltip PRIVATE
  openkal::platform
  openkal::protocol)
target_compile_definitions(openkal_test_skill_ability_tooltip PRIVATE
  OPENKAL_REPO_ROOT="${CMAKE_SOURCE_DIR}")
openkal_target_warnings(openkal_test_skill_ability_tooltip)
add_test(NAME skill_ability_tooltip COMMAND openkal_test_skill_ability_tooltip)

add_executable(openkal_test_sprite_fx_parse
  test_sprite_fx_parse.cpp
  ${CMAKE_SOURCE_DIR}/client/src/world/SpriteFx.cpp)
target_include_directories(openkal_test_sprite_fx_parse PRIVATE
  ${CMAKE_SOURCE_DIR}/client/include
  ${CMAKE_SOURCE_DIR}/engines/include)
target_link_libraries(openkal_test_sprite_fx_parse PRIVATE
  openkal::platform
  openkal::terrain)
openkal_target_warnings(openkal_test_sprite_fx_parse)
add_test(NAME sprite_fx_parse COMMAND openkal_test_sprite_fx_parse)

add_executable(openkal_test_skill_buff test_skill_buff.cpp)
target_link_libraries(openkal_test_skill_buff PRIVATE openkal::server)
openkal_target_warnings(openkal_test_skill_buff)
add_test(NAME skill_buff COMMAND openkal_test_skill_buff)

add_executable(openkal_test_combat test_combat.cpp)
target_link_libraries(openkal_test_combat PRIVATE openkal::server)
target_compile_definitions(openkal_test_combat PRIVATE
  OPENKAL_REPO_ROOT="${CMAKE_SOURCE_DIR}")
openkal_target_warnings(openkal_test_combat)
add_test(NAME combat COMMAND openkal_test_combat)

add_executable(openkal_test_chat
  test_chat.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/ChatFilterModel.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/BattleMsgBuilder.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/ChatCommandRegistry.cpp)
target_include_directories(openkal_test_chat PRIVATE
  ${CMAKE_SOURCE_DIR}/client/include
  ${CMAKE_SOURCE_DIR}/engines/include)
target_link_libraries(openkal_test_chat PRIVATE openkal::server openkal::protocol openkal::terrain)
openkal_target_warnings(openkal_test_chat)
add_test(NAME chat COMMAND openkal_test_chat)

add_executable(openkal_test_minimap
  test_minimap.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/MinimapAreaName.cpp)
target_include_directories(openkal_test_minimap PRIVATE
  ${CMAKE_SOURCE_DIR}/client/include
  ${CMAKE_SOURCE_DIR}/engines/include)
target_link_libraries(openkal_test_minimap PRIVATE openkal::map openkal::terrain)
target_compile_definitions(openkal_test_minimap PRIVATE
  OPENKAL_REPO_ROOT="${CMAKE_SOURCE_DIR}")
openkal_target_warnings(openkal_test_minimap)
add_test(NAME minimap COMMAND openkal_test_minimap)

find_package(Freetype REQUIRED)
set(OPENKAL_HUD_TEST_SOURCES
  ${CMAKE_SOURCE_DIR}/client/src/misc/ServerClock.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/GameHud.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/MinimapWidget.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/WorldMapWidget.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiSettings.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiDocument.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiLayout.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiView.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/WidgetTools.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiBmp.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/FreetypeFont.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/StatusWidget.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/DragonHud.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/MenuBarWidget.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/ChatWidget.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/ChatCommandRegistry.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/ChatFilterModel.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/TextWrap.cpp
)
add_executable(openkal_test_clock_hud ClockHudTest.cpp ${OPENKAL_HUD_TEST_SOURCES})
target_link_libraries(openkal_test_clock_hud PRIVATE openkal::server)
add_test(NAME clock_hud COMMAND openkal_test_clock_hud)
add_executable(openkal_test_minimap_hud MinimapHudTest.cpp ${OPENKAL_HUD_TEST_SOURCES})
add_executable(openkal_test_menu_bar_hud MenuBarHudTest.cpp ${OPENKAL_HUD_TEST_SOURCES})
add_executable(openkal_test_guild_window GuildWindowTest.cpp ${OPENKAL_HUD_TEST_SOURCES}
  ${CMAKE_SOURCE_DIR}/client/src/ui/GuildWindow.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ClientInput.cpp)
target_compile_definitions(openkal_test_guild_window PRIVATE
  OPENKAL_TEST_CONFIG_ARCHIVE="${OPENKAL_CONFIG_ARCHIVE}")
add_dependencies(openkal_test_guild_window openkal_config_archive)
add_executable(openkal_test_player_options PlayerOptionsTest.cpp ${OPENKAL_HUD_TEST_SOURCES}
  ${CMAKE_SOURCE_DIR}/client/src/ui/PlayerOptionsWindow.cpp)
add_test(NAME player_options_protocol COMMAND openkal_test_player_options protocol)
add_test(NAME player_options_window COMMAND openkal_test_player_options)
add_executable(openkal_test_mail_hud MailHudTest.cpp ${OPENKAL_HUD_TEST_SOURCES}
  ${CMAKE_SOURCE_DIR}/client/src/ui/MailWindow.cpp)
add_executable(openkal_test_multiplayer_chat_hud MultiplayerChatHudTest.cpp ${OPENKAL_HUD_TEST_SOURCES})
add_test(NAME multiplayer_chat_hud COMMAND openkal_test_multiplayer_chat_hud)
add_executable(openkal_test_battle_feed_hud BattleFeedHudTest.cpp ${OPENKAL_HUD_TEST_SOURCES}
  ${CMAKE_SOURCE_DIR}/client/src/ui/SettingsWindow.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/PartyWindow.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/BattleMsgBuilder.cpp)
target_link_libraries(openkal_test_battle_feed_hud PRIVATE openkal::settings)
add_executable(openkal_test_character_info_hud CharacterInfoHudTest.cpp ${OPENKAL_HUD_TEST_SOURCES}
  ${CMAKE_SOURCE_DIR}/client/src/ui/CharacterInfoWindow.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/BuffSlotBar.cpp)
target_link_libraries(openkal_test_character_info_hud PRIVATE openkal::server)
add_executable(openkal_test_quest_journal_hud QuestJournalHudTest.cpp ${OPENKAL_HUD_TEST_SOURCES}
  ${CMAKE_SOURCE_DIR}/client/src/ui/QuestJournalWindow.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/LocationMapView.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/QuestSnitch.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/PowerbookCatalog.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/TaskCatalog.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/ItemDb.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/PrefixDb.cpp)
target_link_libraries(openkal_test_quest_journal_hud PRIVATE openkal::server)
add_executable(openkal_test_inventory_hud InventoryHudTest.cpp ${OPENKAL_HUD_TEST_SOURCES}
  ${CMAKE_SOURCE_DIR}/client/src/ui/SkillBar.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/SkillWindow.cpp
  ${CMAKE_SOURCE_DIR}/client/src/player/Skills.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/Inventory.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/ItemTooltip.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/RidingEquipment.cpp
  ${CMAKE_SOURCE_DIR}/client/src/player/Equipment.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/BootShellCharacter.cpp
  ${CMAKE_SOURCE_DIR}/client/src/player/Player.cpp
  ${CMAKE_SOURCE_DIR}/client/src/player/MixVisual.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/ItemDb.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/PrefixDb.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/ShinsuDb.cpp)
target_link_libraries(openkal_test_inventory_hud PRIVATE openkal::server)
add_executable(openkal_test_companion_ui CompanionUiTest.cpp ${OPENKAL_HUD_TEST_SOURCES}
  ${CMAKE_SOURCE_DIR}/client/src/ui/TransformWindow.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/RidingUiHost.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/RidingEquipment.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/ItemTooltip.cpp
  ${CMAKE_SOURCE_DIR}/client/src/player/MixVisual.cpp
  ${CMAKE_SOURCE_DIR}/client/src/player/Skills.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/ItemDb.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/PrefixDb.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/ShinsuDb.cpp)
target_link_libraries(openkal_test_companion_ui PRIVATE openkal::server)
add_executable(openkal_test_skill_ui SkillUiTest.cpp ${OPENKAL_HUD_TEST_SOURCES}
  ${CMAKE_SOURCE_DIR}/client/src/ui/SkillWindow.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/ItemTooltip.cpp
  ${CMAKE_SOURCE_DIR}/client/src/player/Skills.cpp
  ${CMAKE_SOURCE_DIR}/client/src/player/MixVisual.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/ItemDb.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/PrefixDb.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/ShinsuDb.cpp)
target_link_libraries(openkal_test_skill_ui PRIVATE openkal::server)
add_executable(openkal_test_blacksmith_ui BlacksmithUiTest.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/BlacksmithWindow.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiDocument.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiLayout.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/TextWrap.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiView.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/WidgetTools.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiBmp.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/FreetypeFont.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/ItemDb.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/PrefixDb.cpp)
target_link_libraries(openkal_test_blacksmith_ui PRIVATE openkal::server)
add_test(NAME blacksmith_ui COMMAND openkal_test_blacksmith_ui)
add_executable(openkal_test_npc_talk_ui NpcTalkUiTest.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/NpcTalkWindow.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/TextWrap.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/DialogCatalog.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiDocument.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiLayout.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiView.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/WidgetTools.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiBmp.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/FreetypeFont.cpp)
add_test(NAME npc_talk_ui COMMAND openkal_test_npc_talk_ui)
add_executable(openkal_test_mix_ui MixUiTest.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/NpcTalkWindow.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/TextWrap.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/DialogCatalog.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/MixWindow.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiDocument.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiLayout.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiView.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/WidgetTools.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiBmp.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/FreetypeFont.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/ItemDb.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/PrefixDb.cpp)
target_link_libraries(openkal_test_mix_ui PRIVATE openkal::server)
add_test(NAME mix_ui COMMAND openkal_test_mix_ui)
add_executable(openkal_test_consumable_crafting_ui ConsumableCraftingUiTest.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/ConsumableCraftingWindow.cpp
  ${CMAKE_SOURCE_DIR}/client/src/world/NpcCatalog.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/NpcTalkWindow.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/DialogCatalog.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/TextWrap.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiDocument.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiLayout.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiView.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/WidgetTools.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiBmp.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/FreetypeFont.cpp)
target_link_libraries(openkal_test_consumable_crafting_ui PRIVATE openkal::server)
add_test(NAME consumable_crafting_ui COMMAND openkal_test_consumable_crafting_ui)
foreach(target
    openkal_test_consumable_crafting_ui
    openkal_test_blacksmith_ui
    openkal_test_npc_talk_ui
    openkal_test_mix_ui
    openkal_test_skill_ui
    openkal_test_clock_hud
    openkal_test_minimap_hud
    openkal_test_menu_bar_hud
    openkal_test_mail_hud
    openkal_test_guild_window
    openkal_test_player_options
    openkal_test_battle_feed_hud
    openkal_test_multiplayer_chat_hud
    openkal_test_character_info_hud
    openkal_test_quest_journal_hud
    openkal_test_companion_ui
    openkal_test_inventory_hud)
  target_include_directories(${target} PRIVATE ${CMAKE_SOURCE_DIR}/client/include)
  target_link_libraries(${target} PRIVATE
    openkal::engine openkal::map openkal::terrain openkal::protocol openkal::platform Freetype::Freetype)
  target_compile_definitions(${target} PRIVATE
    OPENKAL_REPO_ROOT="${CMAKE_SOURCE_DIR}"
    OPENKAL_TEST_RUNTIME_ROOT="${OPENKAL_GENERATED_RUNTIME_ROOT}")
  add_dependencies(${target} openkal_runtime_icons)
  openkal_target_warnings(${target})
endforeach()
add_test(NAME minimap_hud COMMAND openkal_test_minimap_hud)
add_test(NAME menu_bar_hud COMMAND openkal_test_menu_bar_hud)
add_test(NAME mail_hud COMMAND openkal_test_mail_hud)
add_test(NAME battle_feed_hud COMMAND openkal_test_battle_feed_hud)
add_test(NAME character_info_hud COMMAND openkal_test_character_info_hud)
add_test(NAME quest_journal_hud COMMAND openkal_test_quest_journal_hud)
add_test(NAME inventory_hud COMMAND openkal_test_inventory_hud)
add_test(NAME emote_controls COMMAND openkal_test_inventory_hud --emotes-only)
add_test(NAME inventory_scroll COMMAND openkal_test_inventory_hud --compact-inventory-only)
add_test(NAME revision_hammer_drag COMMAND openkal_test_inventory_hud --revision-hammer-only)
add_test(NAME companion_ui COMMAND openkal_test_companion_ui)
add_test(NAME skill_ui COMMAND openkal_test_skill_ui)

add_executable(openkal_test_mix_catalog test_mix_catalog.cpp)
target_link_libraries(openkal_test_mix_catalog PRIVATE openkal::server openkal::platform)
target_compile_definitions(openkal_test_mix_catalog PRIVATE
  OPENKAL_REPO_ROOT="${CMAKE_SOURCE_DIR}")
openkal_target_warnings(openkal_test_mix_catalog)
add_test(NAME mix_catalog COMMAND openkal_test_mix_catalog)

add_executable(openkal_test_shinsu_db
  test_shinsu_db.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/ShinsuDb.cpp)
target_include_directories(openkal_test_shinsu_db PRIVATE
  ${CMAKE_SOURCE_DIR}/client/include)
target_link_libraries(openkal_test_shinsu_db PRIVATE openkal::platform)
target_compile_definitions(openkal_test_shinsu_db PRIVATE
  OPENKAL_REPO_ROOT="${CMAKE_SOURCE_DIR}")
openkal_target_warnings(openkal_test_shinsu_db)
add_test(NAME shinsu_db COMMAND openkal_test_shinsu_db)

add_executable(openkal_test_bof_catalog test_bof_catalog.cpp)
target_link_libraries(openkal_test_bof_catalog PRIVATE openkal::server openkal::platform)
target_compile_definitions(openkal_test_bof_catalog PRIVATE
  OPENKAL_REPO_ROOT="${CMAKE_SOURCE_DIR}")
openkal_target_warnings(openkal_test_bof_catalog)
add_test(NAME bof_catalog COMMAND openkal_test_bof_catalog)

add_test(NAME weapon_enchantment_inventory
  COMMAND openkal_test_inventory_hud --weapon-enchantment-only)

add_executable(openkal_test_weapon_enchantment WeaponEnchantmentTest.cpp)
target_link_libraries(openkal_test_weapon_enchantment PRIVATE openkal::server)
openkal_target_warnings(openkal_test_weapon_enchantment)
add_test(NAME weapon_enchantment COMMAND openkal_test_weapon_enchantment)

add_executable(openkal_test_weapon_enchantment_server WeaponEnchantmentServerTest.cpp)
target_link_libraries(openkal_test_weapon_enchantment_server PRIVATE openkal::server openkal::platform)
openkal_target_warnings(openkal_test_weapon_enchantment_server)
add_test(NAME weapon_enchantment_server COMMAND openkal_test_weapon_enchantment_server)

add_executable(openkal_test_revision_hammer_server RevisionHammerServerTest.cpp)
target_link_libraries(openkal_test_revision_hammer_server PRIVATE openkal::server openkal::platform)
openkal_target_warnings(openkal_test_revision_hammer_server)
add_test(NAME revision_hammer_server COMMAND openkal_test_revision_hammer_server)

add_executable(openkal_test_polishing_stone_server PolishingStoneServerTest.cpp)
target_link_libraries(openkal_test_polishing_stone_server PRIVATE openkal::server openkal::platform)
openkal_target_warnings(openkal_test_polishing_stone_server)
add_test(NAME polishing_stone_server COMMAND openkal_test_polishing_stone_server)

add_executable(openkal_test_polishing_stone PolishingStoneTest.cpp)
target_link_libraries(openkal_test_polishing_stone PRIVATE openkal::server)
openkal_target_warnings(openkal_test_polishing_stone)
add_test(NAME polishing_stone COMMAND openkal_test_polishing_stone)

add_executable(openkal_test_qigong test_qigong.cpp)
target_link_libraries(openkal_test_qigong PRIVATE openkal::server openkal::platform)
openkal_target_warnings(openkal_test_qigong)
add_test(NAME qigong COMMAND openkal_test_qigong)

add_executable(openkal_test_dragon_fusing test_dragon_fusing.cpp)
target_link_libraries(openkal_test_dragon_fusing PRIVATE openkal::server openkal::platform)
openkal_target_warnings(openkal_test_dragon_fusing)
add_test(NAME dragon_fusing COMMAND openkal_test_dragon_fusing)

add_executable(openkal_test_portal test_portal.cpp)
target_link_libraries(openkal_test_portal PRIVATE openkal::map openkal::server openkal::protocol)
target_compile_definitions(openkal_test_portal PRIVATE
  OPENKAL_REPO_ROOT="${CMAKE_SOURCE_DIR}")
openkal_target_warnings(openkal_test_portal)
add_test(NAME portal COMMAND openkal_test_portal)

add_executable(openkal_test_doggebi_ritual test_doggebi_ritual.cpp)
target_link_libraries(openkal_test_doggebi_ritual PRIVATE openkal::server)
openkal_target_warnings(openkal_test_doggebi_ritual)
add_test(NAME doggebi_ritual COMMAND openkal_test_doggebi_ritual)

add_executable(openkal_test_quest test_quest.cpp)
target_link_libraries(openkal_test_quest PRIVATE openkal::server openkal::protocol openkal::platform)
target_compile_definitions(openkal_test_quest PRIVATE
  OPENKAL_TEST_CONFIG_ARCHIVE="${OPENKAL_CONFIG_ARCHIVE}")
add_dependencies(openkal_test_quest openkal_config_archive)
openkal_target_warnings(openkal_test_quest)
add_test(NAME quest COMMAND openkal_test_quest)

add_executable(openkal_test_quest_marker
  test_quest_marker.cpp
  ${CMAKE_SOURCE_DIR}/client/src/world/QuestMarker.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/DialogCatalog.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/TaskCatalog.cpp)
target_include_directories(openkal_test_quest_marker PRIVATE
  ${CMAKE_SOURCE_DIR}/client/include
  ${CMAKE_SOURCE_DIR}/engines/include)
target_link_libraries(openkal_test_quest_marker PRIVATE
  openkal::server
  openkal::protocol
  openkal::platform)
target_compile_definitions(openkal_test_quest_marker PRIVATE
  OPENKAL_REPO_ROOT="${CMAKE_SOURCE_DIR}")
openkal_target_warnings(openkal_test_quest_marker)
add_test(NAME quest_marker COMMAND openkal_test_quest_marker)

add_executable(openkal_test_quest_journal
  test_quest_journal.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/TextWrap.cpp)
target_include_directories(openkal_test_quest_journal PRIVATE
  ${CMAKE_SOURCE_DIR}/client/include
  ${CMAKE_SOURCE_DIR}/engines/include)
target_link_libraries(openkal_test_quest_journal PRIVATE openkal::terrain openkal::protocol)
openkal_target_warnings(openkal_test_quest_journal)
add_test(NAME quest_journal COMMAND openkal_test_quest_journal)

add_executable(openkal_test_latin_text test_latin_text.cpp)
target_include_directories(openkal_test_latin_text PRIVATE
  ${CMAKE_SOURCE_DIR}/client/include)
openkal_target_warnings(openkal_test_latin_text)
add_test(NAME latin_text COMMAND openkal_test_latin_text)

add_executable(openkal_test_ui_document
  test_ui_document.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/RidingEquipment.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiDocument.cpp)
target_include_directories(openkal_test_ui_document PRIVATE
  ${CMAKE_SOURCE_DIR}/client/include)
target_link_libraries(openkal_test_ui_document PRIVATE openkal::platform openkal::protocol)
target_compile_definitions(openkal_test_ui_document PRIVATE
  OPENKAL_REPO_ROOT="${CMAKE_SOURCE_DIR}")
openkal_target_warnings(openkal_test_ui_document)
add_test(NAME ui_document COMMAND openkal_test_ui_document)

foreach(mode IN ITEMS development release)
  add_executable(openkal_test_ui_layout_${mode}
    UiLayoutReloadTest.cpp
    ${CMAKE_SOURCE_DIR}/client/src/ui/UiLayout.cpp)
  target_include_directories(openkal_test_ui_layout_${mode} PRIVATE
    ${CMAKE_SOURCE_DIR}/client/include)
  target_link_libraries(openkal_test_ui_layout_${mode} PRIVATE openkal::platform)
  if(mode STREQUAL "release")
    target_compile_definitions(openkal_test_ui_layout_${mode} PRIVATE
      OPENKAL_DISABLE_UI_HOT_RELOAD=1)
  endif()
  openkal_target_warnings(openkal_test_ui_layout_${mode})
  add_test(NAME ui_layout_${mode} COMMAND openkal_test_ui_layout_${mode})
endforeach()

add_executable(openkal_test_arrow_hit_feedback
  ArrowHitFeedbackTest.cpp
  ${CMAKE_SOURCE_DIR}/client/src/world/ArrowHitFeedback.cpp)
target_include_directories(openkal_test_arrow_hit_feedback PRIVATE
  ${CMAKE_SOURCE_DIR}/client/include
  ${CMAKE_SOURCE_DIR}/engines/include)
target_link_libraries(openkal_test_arrow_hit_feedback PRIVATE openkal::protocol)
openkal_target_warnings(openkal_test_arrow_hit_feedback)
add_test(NAME arrow_hit_feedback COMMAND openkal_test_arrow_hit_feedback)

add_executable(openkal_test_clip_effect_cues test_clip_effect_cues.cpp)
target_include_directories(openkal_test_clip_effect_cues PRIVATE
  ${CMAKE_SOURCE_DIR}/client/include
  ${CMAKE_SOURCE_DIR}/engines/include)
target_link_libraries(openkal_test_clip_effect_cues PRIVATE
  openkal::terrain
  openkal::protocol
  openkal::server)
openkal_target_warnings(openkal_test_clip_effect_cues)
add_test(NAME clip_effect_cues COMMAND openkal_test_clip_effect_cues)

find_package(Threads REQUIRED)
add_executable(openkal_test_monster_pose
  MonsterPoseTest.cpp
  ${CMAKE_SOURCE_DIR}/client/src/monster/MonsterRender.cpp
  ${CMAKE_SOURCE_DIR}/client/src/monster/MonsterCatalog.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/Audio.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/AnimSound.cpp
  ${CMAKE_SOURCE_DIR}/engines/null/null_engine.cpp)
target_include_directories(openkal_test_monster_pose PRIVATE
  ${CMAKE_SOURCE_DIR}/client/include
  ${CMAKE_SOURCE_DIR}/engines/include
  ${CMAKE_SOURCE_DIR}/third_party)
target_link_libraries(openkal_test_monster_pose PRIVATE
  openkal::terrain openkal::protocol Threads::Threads ${CMAKE_DL_LIBS})
openkal_target_warnings(openkal_test_monster_pose)
add_test(NAME monster_pose COMMAND openkal_test_monster_pose)

add_executable(openkal_test_player_chest_pose
  PlayerChestPoseTest.cpp
  ${CMAKE_SOURCE_DIR}/engines/TransparentMesh.cpp
  ${CMAKE_SOURCE_DIR}/client/src/player/AnimatedActor.cpp
  ${CMAKE_SOURCE_DIR}/client/src/player/Player.cpp
  ${CMAKE_SOURCE_DIR}/client/src/player/Equipment.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/ItemDb.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/PrefixDb.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/Audio.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/AnimSound.cpp
  ${CMAKE_SOURCE_DIR}/engines/null/null_engine.cpp)
target_include_directories(openkal_test_player_chest_pose PRIVATE
  ${CMAKE_SOURCE_DIR}/client/include
  ${CMAKE_SOURCE_DIR}/engines/include
  ${CMAKE_SOURCE_DIR}/third_party)
target_link_libraries(openkal_test_player_chest_pose PRIVATE
  openkal::terrain openkal::protocol openkal::server Threads::Threads ${CMAKE_DL_LIBS})
openkal_target_warnings(openkal_test_player_chest_pose)
add_test(NAME player_chest_pose COMMAND openkal_test_player_chest_pose)

add_executable(openkal_test_audio
  test_audio.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/Audio.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/BgmController.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/AnimSound.cpp)
target_include_directories(openkal_test_audio PRIVATE
  ${CMAKE_SOURCE_DIR}/client/include
  ${CMAKE_SOURCE_DIR}/third_party)
target_link_libraries(openkal_test_audio PRIVATE
  openkal::platform
  openkal::map
  openkal::terrain
  Threads::Threads
  ${CMAKE_DL_LIBS})
target_compile_definitions(openkal_test_audio PRIVATE
  OPENKAL_TEST_CONFIG_ARCHIVE="${OPENKAL_CONFIG_ARCHIVE}")
add_dependencies(openkal_test_audio openkal_config_archive)
openkal_target_warnings(openkal_test_audio)
add_test(NAME audio COMMAND openkal_test_audio)

find_package(Freetype REQUIRED)
add_executable(openkal_test_item_tooltip
  test_item_tooltip.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/ItemTooltip.cpp
  ${CMAKE_SOURCE_DIR}/client/src/player/MixVisual.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/WidgetTools.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiDocument.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiBmp.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/ItemDb.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/PrefixDb.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/FreetypeFont.cpp)
target_include_directories(openkal_test_item_tooltip PRIVATE
  ${CMAKE_SOURCE_DIR}/client/include
  ${CMAKE_SOURCE_DIR}/engines/include
  ${CMAKE_SOURCE_DIR}/third_party)
target_link_libraries(openkal_test_item_tooltip PRIVATE
  openkal::platform
  openkal::engine
  openkal::terrain
  openkal::server
  Freetype::Freetype)
target_compile_definitions(openkal_test_item_tooltip PRIVATE
  OPENKAL_REPO_ROOT="${CMAKE_SOURCE_DIR}")
openkal_target_warnings(openkal_test_item_tooltip)
add_test(NAME item_tooltip COMMAND openkal_test_item_tooltip)

add_executable(openkal_test_debug_dummy_players test_debug_dummy_players.cpp)
target_link_libraries(openkal_test_debug_dummy_players PRIVATE openkal::server)
openkal_target_warnings(openkal_test_debug_dummy_players)
add_test(NAME debug_dummy_players COMMAND openkal_test_debug_dummy_players)

add_executable(openkal_test_character_creation
  test_character_creation.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/CharacterCreationDraft.cpp)
target_include_directories(openkal_test_character_creation PRIVATE
  ${CMAKE_SOURCE_DIR}/client/include)
target_link_libraries(openkal_test_character_creation PRIVATE openkal::protocol)
openkal_target_warnings(openkal_test_character_creation)
add_test(NAME character_creation COMMAND openkal_test_character_creation)

add_executable(openkal_test_soft_cursor
  SoftCursorTest.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/SoftCursor.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/FreetypeFont.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/WidgetTools.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiBmp.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiDocument.cpp)
target_include_directories(openkal_test_soft_cursor PRIVATE
  ${CMAKE_SOURCE_DIR}/client/include
  ${CMAKE_SOURCE_DIR}/engines/include)
target_link_libraries(openkal_test_soft_cursor PRIVATE
  openkal::terrain openkal::platform openkal::protocol Freetype::Freetype)
target_compile_definitions(openkal_test_soft_cursor PRIVATE
  OPENKAL_TEST_CONFIG_ARCHIVE="${OPENKAL_CONFIG_ARCHIVE}")
add_dependencies(openkal_test_soft_cursor openkal_config_archive)
openkal_target_warnings(openkal_test_soft_cursor)
add_test(NAME soft_cursor COMMAND openkal_test_soft_cursor)

# Exercise the same combined resources shipped in config.pak. Extract them for
# tests that also use standard file I/O, keeping both source checkouts untouched.
set(OPENKAL_TEST_RESOURCE_ROOT "${CMAKE_CURRENT_BINARY_DIR}/resources")
set(OPENKAL_TEST_RESOURCE_STAMP "${CMAKE_CURRENT_BINARY_DIR}/resources.stamp")
add_custom_command(
  OUTPUT "${OPENKAL_TEST_RESOURCE_STAMP}"
  COMMAND "${CMAKE_COMMAND}" -E rm -rf "${OPENKAL_TEST_RESOURCE_ROOT}"
  COMMAND "${Python3_EXECUTABLE}" -m zipfile -e
          "${OPENKAL_CONFIG_ARCHIVE}" "${OPENKAL_TEST_RESOURCE_ROOT}"
  COMMAND "${CMAKE_COMMAND}" -E touch "${OPENKAL_TEST_RESOURCE_STAMP}"
  DEPENDS "${OPENKAL_CONFIG_ARCHIVE}"
  COMMENT "Preparing combined test resources"
  VERBATIM)
add_custom_target(openkal_test_resources DEPENDS "${OPENKAL_TEST_RESOURCE_STAMP}")
add_dependencies(openkal_test_resources openkal_config_archive)
foreach(target
    openkal_test_map
    openkal_test_area_editor_object_mask
    openkal_test_player_movement
    openkal_test_shop_catalog
    openkal_test_monster_occupancy
    openkal_test_monster_fx
    openkal_test_skill_ability_tooltip
    openkal_test_combat
    openkal_test_minimap
    openkal_test_inventory_hud
    openkal_test_mix_ui
    openkal_test_skill_ui
    openkal_test_portal
    openkal_test_quest_marker
    openkal_test_item_tooltip)
  target_compile_definitions(${target} PRIVATE
    OPENKAL_TEST_RESOURCE_ROOT="${OPENKAL_TEST_RESOURCE_ROOT}")
  add_dependencies(${target} openkal_test_resources)
endforeach()

add_executable(openkal_test_collection CollectionEquipmentTest.cpp)
target_link_libraries(openkal_test_collection PRIVATE openkal::server)
target_compile_definitions(openkal_test_collection PRIVATE OPENKAL_REPO_ROOT="${CMAKE_SOURCE_DIR}")
openkal_target_warnings(openkal_test_collection)
add_test(NAME collection_equipment COMMAND openkal_test_collection)

add_executable(openkal_test_inventory_server InventoryServerTest.cpp)
target_link_libraries(openkal_test_inventory_server PRIVATE openkal::server)
openkal_target_warnings(openkal_test_inventory_server)
add_test(NAME inventory_server COMMAND openkal_test_inventory_server)

add_executable(openkal_test_skin_server SkinServerTest.cpp)
target_link_libraries(openkal_test_skin_server PRIVATE openkal::server)
openkal_target_warnings(openkal_test_skin_server)
add_test(NAME skin_server COMMAND openkal_test_skin_server)

add_executable(openkal_test_starter_kit StarterKitTest.cpp)
target_link_libraries(openkal_test_starter_kit PRIVATE openkal::server)
openkal_target_warnings(openkal_test_starter_kit)
add_test(NAME starter_kit COMMAND openkal_test_starter_kit)

add_executable(openkal_test_teleport_item_server TeleportItemServerTest.cpp)
target_link_libraries(openkal_test_teleport_item_server PRIVATE openkal::server)
openkal_target_warnings(openkal_test_teleport_item_server)
add_test(NAME teleport_item_server COMMAND openkal_test_teleport_item_server)

add_executable(openkal_test_collection_server CollectionServerTest.cpp)
target_link_libraries(openkal_test_collection_server PRIVATE openkal::server)
target_compile_definitions(openkal_test_collection_server PRIVATE OPENKAL_REPO_ROOT="${CMAKE_SOURCE_DIR}")
openkal_target_warnings(openkal_test_collection_server)
add_test(NAME collection_server COMMAND openkal_test_collection_server)

add_executable(openkal_test_essence_box EssenceBoxTest.cpp)
target_link_libraries(openkal_test_essence_box PRIVATE openkal::server)
target_compile_definitions(openkal_test_essence_box PRIVATE OPENKAL_REPO_ROOT="${CMAKE_SOURCE_DIR}")
openkal_target_warnings(openkal_test_essence_box)
add_test(NAME essence_box COMMAND openkal_test_essence_box)

add_executable(openkal_test_auto_hp_server AutoHpServerTest.cpp)
target_link_libraries(openkal_test_auto_hp_server PRIVATE openkal::server)
target_compile_definitions(openkal_test_auto_hp_server PRIVATE OPENKAL_REPO_ROOT="${CMAKE_SOURCE_DIR}")
openkal_target_warnings(openkal_test_auto_hp_server)
add_test(NAME auto_hp_server COMMAND openkal_test_auto_hp_server)

# These legacy integration checks inspect a complete user-maintained data
# catalog. Keep them out of the project-owned unit-test run until each one has a
# minimal synthetic fixture. They remain available to developers who explicitly
# configure a compatible optional data directory.
set(OPENKAL_USER_DATA_TESTS
    map
    area_editor_user_data
    player_movement
    server
    shop_catalog
    monster_occupancy
    monster_fx
    skill_ability_tooltip
    combat
    minimap
    mix_ui
    inventory_hud
    skill_ui
    portal
    quest
    quest_marker
    item_tooltip)
set(_openkal_user_data_tests_default OFF)
if(OPENKAL_DATA_ROOT)
  set(_openkal_user_data_tests_default ON)
endif()
option(OPENKAL_RUN_USER_DATA_TESTS
       "Run integration tests against OPENKAL_DATA_ROOT"
       ${_openkal_user_data_tests_default})
set_tests_properties(${OPENKAL_USER_DATA_TESTS} PROPERTIES LABELS "optional-user-data")
if(NOT OPENKAL_RUN_USER_DATA_TESTS)
  set_tests_properties(${OPENKAL_USER_DATA_TESTS} PROPERTIES DISABLED TRUE)
endif()
unset(_openkal_user_data_tests_default)

add_executable(openkal_test_talent_tree TalentTreeTest.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/BuffSlotBar.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/TextWrap.cpp
  ${CMAKE_SOURCE_DIR}/client/src/player/Skills.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/TalentTreeWindow.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiDocument.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiLayout.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiView.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/WidgetTools.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiBmp.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/FreetypeFont.cpp)
target_include_directories(openkal_test_talent_tree PRIVATE ${CMAKE_SOURCE_DIR}/client/include)
target_link_libraries(openkal_test_talent_tree PRIVATE openkal::server openkal::engine openkal::terrain Freetype::Freetype)
target_compile_definitions(openkal_test_talent_tree PRIVATE
  OPENKAL_REPO_ROOT="${CMAKE_SOURCE_DIR}"
  OPENKAL_TEST_RUNTIME_ROOT="${OPENKAL_GENERATED_RUNTIME_ROOT}")
add_dependencies(openkal_test_talent_tree openkal_runtime_icons)
openkal_target_warnings(openkal_test_talent_tree)
add_test(NAME talent_tree COMMAND openkal_test_talent_tree)

add_executable(openkal_test_talent_tree_server TalentTreeServerTest.cpp)
target_link_libraries(openkal_test_talent_tree_server PRIVATE openkal::server)
openkal_target_warnings(openkal_test_talent_tree_server)
add_test(NAME talent_tree_server COMMAND openkal_test_talent_tree_server)

find_program(OPENKAL_OPENSSL_EXECUTABLE openssl REQUIRED)
add_executable(openkal_test_tls_transport TlsTransportTest.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/ClientTlsConfig.cpp)
target_include_directories(openkal_test_tls_transport PRIVATE ${CMAKE_SOURCE_DIR}/client/include)
target_link_libraries(openkal_test_tls_transport PRIVATE openkal::server Threads::Threads)
openkal_target_warnings(openkal_test_tls_transport)
add_test(NAME tls_transport COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/test_tls_transport.py"
  "$<TARGET_FILE:openkal_test_tls_transport>" "${OPENKAL_OPENSSL_EXECUTABLE}")
set_tests_properties(tls_transport PROPERTIES TIMEOUT 60)

if(OPENKAL_WITH_POSTGRES)
  find_package(PostgreSQL REQUIRED)
  add_executable(openkal_test_guild_postgres GuildPostgresTest.cpp)
  target_link_libraries(openkal_test_guild_postgres PRIVATE openkal::server PostgreSQL::PostgreSQL)
  openkal_target_warnings(openkal_test_guild_postgres)
  add_test(NAME guild_postgres COMMAND openkal_test_guild_postgres)
  set_tests_properties(guild_postgres PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 60 LABELS "postgres-integration")

  add_executable(openkal_test_postgres_multiplayer PostgresMultiplayerTest.cpp)
  target_link_libraries(openkal_test_postgres_multiplayer PRIVATE openkal::server PostgreSQL::PostgreSQL)
  openkal_target_warnings(openkal_test_postgres_multiplayer)
  add_test(NAME postgres_multiplayer COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/test_postgres_multiplayer.py" "$<TARGET_FILE:openkal_test_postgres_multiplayer>")
  set_tests_properties(postgres_multiplayer PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 120 LABELS "postgres-integration")
  add_test(NAME guild_server COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/test_postgres_multiplayer.py" "$<TARGET_FILE:openkal_test_postgres_multiplayer>")
  set_tests_properties(guild_server PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 120 LABELS "postgres-integration" RUN_SERIAL TRUE)

  add_executable(openkal_test_castle_war_server CastleWarServerTest.cpp)
  target_link_libraries(openkal_test_castle_war_server PRIVATE openkal::server PostgreSQL::PostgreSQL)
  openkal_target_warnings(openkal_test_castle_war_server)
  add_test(NAME castle_war_server COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/test_postgres_multiplayer.py" "$<TARGET_FILE:openkal_test_castle_war_server>")
  set_tests_properties(castle_war_server PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 90 LABELS "postgres-integration" RUN_SERIAL TRUE)
  add_test(NAME pvp_server COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/test_postgres_multiplayer.py" "$<TARGET_FILE:openkal_test_castle_war_server>" pvp)
  set_tests_properties(pvp_server PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 90 LABELS "postgres-integration" RUN_SERIAL TRUE)


endif()

add_executable(openkal_test_account_password AccountPasswordTest.cpp)
target_link_libraries(openkal_test_account_password PRIVATE openkal::server)
openkal_target_warnings(openkal_test_account_password)
add_test(NAME account_password COMMAND openkal_test_account_password)

add_executable(openkal_test_account_text_input
  AccountTextInputTest.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/AccountTextInput.cpp)
target_include_directories(openkal_test_account_text_input PRIVATE ${CMAKE_SOURCE_DIR}/client/include)
target_link_libraries(openkal_test_account_text_input PRIVATE openkal::server)
openkal_target_warnings(openkal_test_account_text_input)
add_test(NAME account_text_input COMMAND openkal_test_account_text_input)

add_executable(openkal_test_auto_login
  AutoLoginTest.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/AutoLogin.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/AccountTextInput.cpp)
target_include_directories(openkal_test_auto_login PRIVATE ${CMAKE_SOURCE_DIR}/client/include)
target_link_libraries(openkal_test_auto_login PRIVATE openkal::server)
openkal_target_warnings(openkal_test_auto_login)
add_test(NAME auto_login COMMAND openkal_test_auto_login)

add_executable(openkal_test_client_connection_config
  ClientConnectionConfigTest.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/ClientServerConfig.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/ClientTlsConfig.cpp)
target_include_directories(openkal_test_client_connection_config PRIVATE ${CMAKE_SOURCE_DIR}/client/include)
target_link_libraries(openkal_test_client_connection_config PRIVATE openkal::server)
target_compile_definitions(openkal_test_client_connection_config PRIVATE
  OPENKAL_TEST_CONFIG_ARCHIVE="${OPENKAL_CONFIG_ARCHIVE}")
add_dependencies(openkal_test_client_connection_config openkal_config_archive)
openkal_target_warnings(openkal_test_client_connection_config)
add_test(NAME client_connection_config COMMAND openkal_test_client_connection_config)

add_test(NAME multiplayer_client_launcher COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/test_multiplayer_client_launcher.py")

find_package(OpenSSL 3.0 REQUIRED)
add_executable(openkal_test_server_startup
  ServerStartupTest.cpp
  ${CMAKE_SOURCE_DIR}/server/ServerStartupConfig.cpp
  ${CMAKE_SOURCE_DIR}/server/DevelopmentCertificate.cpp)
target_include_directories(openkal_test_server_startup PRIVATE ${CMAKE_SOURCE_DIR}/server)
target_link_libraries(openkal_test_server_startup PRIVATE openkal::server OpenSSL::Crypto)
openkal_target_warnings(openkal_test_server_startup)
add_test(NAME server_startup COMMAND openkal_test_server_startup)
add_test(NAME server_development_tls COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/test_server_development_tls.py"
  "$<TARGET_FILE:openkal_test_server_startup>" "$<TARGET_FILE:openkal_test_tls_transport>")
set_tests_properties(server_development_tls PROPERTIES TIMEOUT 40)
add_test(NAME multiplayer_server_launcher COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/test_multiplayer_server_launcher.py"
  "$<TARGET_FILE:openkal_mainsvr>" "$<BOOL:${OPENKAL_WITH_POSTGRES}>")

add_executable(openkal_test_party_service PartyServiceTest.cpp)
target_link_libraries(openkal_test_party_service PRIVATE openkal::server)
openkal_target_warnings(openkal_test_party_service)
add_test(NAME party_service COMMAND openkal_test_party_service)

add_executable(openkal_test_party_server PartyServerTest.cpp)
target_link_libraries(openkal_test_party_server PRIVATE openkal::server)
openkal_target_warnings(openkal_test_party_server)
add_test(NAME party_server COMMAND openkal_test_party_server)
set_tests_properties(party_server PROPERTIES TIMEOUT 35)

add_executable(openkal_test_powerbook PowerbookTest.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/ItemTooltip.cpp
  ${CMAKE_SOURCE_DIR}/client/src/player/MixVisual.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/PowerbookCatalog.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/ItemDb.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/PrefixDb.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ClientInput.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/PowerbookWindow.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/LocationMapView.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiDocument.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiLayout.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/TextWrap.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiView.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/WidgetTools.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiBmp.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/FreetypeFont.cpp)
target_include_directories(openkal_test_powerbook PRIVATE ${CMAKE_SOURCE_DIR}/client/include)
target_link_libraries(openkal_test_powerbook PRIVATE openkal::server openkal::engine openkal::terrain Freetype::Freetype)
target_compile_definitions(openkal_test_powerbook PRIVATE
  OPENKAL_REPO_ROOT="${CMAKE_SOURCE_DIR}"
  OPENKAL_TEST_RUNTIME_ROOT="${OPENKAL_GENERATED_RUNTIME_ROOT}")
add_dependencies(openkal_test_powerbook openkal_runtime_icons)
openkal_target_warnings(openkal_test_powerbook)
add_test(NAME powerbook COMMAND openkal_test_powerbook)

add_executable(openkal_test_guild GuildTest.cpp)
target_link_libraries(openkal_test_guild PRIVATE openkal::server)
target_compile_definitions(openkal_test_guild PRIVATE OPENKAL_REPO_ROOT="${CMAKE_SOURCE_DIR}")
openkal_target_warnings(openkal_test_guild)
foreach(part rules protocol service)
  add_test(NAME guild_${part} COMMAND openkal_test_guild ${part})
endforeach()

add_executable(openkal_test_castle_war CastleWarTest.cpp)
target_link_libraries(openkal_test_castle_war PRIVATE openkal::server)
target_compile_definitions(openkal_test_castle_war PRIVATE OPENKAL_REPO_ROOT="${CMAKE_SOURCE_DIR}")
openkal_target_warnings(openkal_test_castle_war)
add_test(NAME castle_war COMMAND openkal_test_castle_war)

add_executable(openkal_test_castle_war_rules CastleWarRulesTest.cpp)
target_link_libraries(openkal_test_castle_war_rules PRIVATE openkal::server)
target_compile_definitions(openkal_test_castle_war_rules PRIVATE OPENKAL_REPO_ROOT="${CMAKE_SOURCE_DIR}")
openkal_target_warnings(openkal_test_castle_war_rules)
add_test(NAME castle_war_rules COMMAND openkal_test_castle_war_rules)

add_test(NAME guild_window COMMAND openkal_test_guild_window)
add_test(NAME guild_packaged_ui COMMAND openkal_test_guild_window --packaged)

add_executable(openkal_test_consumable_crafting_server ConsumableCraftingServerTest.cpp)
target_link_libraries(openkal_test_consumable_crafting_server PRIVATE openkal::server)
target_compile_definitions(openkal_test_consumable_crafting_server PRIVATE OPENKAL_REPO_ROOT="${CMAKE_SOURCE_DIR}")
openkal_target_warnings(openkal_test_consumable_crafting_server)
add_test(NAME consumable_crafting_server COMMAND openkal_test_consumable_crafting_server)

add_executable(openkal_test_skin_shop_server SkinShopServerTest.cpp)
target_link_libraries(openkal_test_skin_shop_server PRIVATE openkal::server)
openkal_target_warnings(openkal_test_skin_shop_server)
add_test(NAME skin_shop_server COMMAND openkal_test_skin_shop_server)

add_executable(openkal_test_skin_shop_ui SkinShopUiTest.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/SkinShopWindow.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/WeaponThumbnailCamera.cpp
  ${CMAKE_SOURCE_DIR}/client/src/player/AnimatedActor.cpp
  ${CMAKE_SOURCE_DIR}/client/src/player/Player.cpp
  ${CMAKE_SOURCE_DIR}/client/src/player/Equipment.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/ItemDb.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/PrefixDb.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/Audio.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/AnimSound.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/TextWrap.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiDocument.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiLayout.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiView.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/WidgetTools.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiBmp.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/FreetypeFont.cpp)
target_include_directories(openkal_test_skin_shop_ui PRIVATE
  ${CMAKE_SOURCE_DIR}/client/include ${CMAKE_SOURCE_DIR}/third_party)
target_link_libraries(openkal_test_skin_shop_ui PRIVATE openkal::engine openkal::terrain
  openkal::server Freetype::Freetype Threads::Threads ${CMAKE_DL_LIBS})
target_compile_definitions(openkal_test_skin_shop_ui PRIVATE
  OPENKAL_REPO_ROOT="${CMAKE_SOURCE_DIR}" OPENKAL_RUNTIME_ROOT="${OPENKAL_GENERATED_RUNTIME_ROOT}")
add_dependencies(openkal_test_skin_shop_ui openkal_runtime_icons)
openkal_target_warnings(openkal_test_skin_shop_ui)
add_test(NAME skin_shop_ui COMMAND openkal_test_skin_shop_ui)

add_executable(openkal_test_trade TradeTest.cpp)
target_link_libraries(openkal_test_trade PRIVATE openkal::server)
openkal_target_warnings(openkal_test_trade)
add_test(NAME trade COMMAND openkal_test_trade)

add_executable(openkal_test_trade_ui TradeUiTest.cpp ${OPENKAL_HUD_TEST_SOURCES}
  ${CMAKE_SOURCE_DIR}/client/src/ui/ItemTooltip.cpp
  ${CMAKE_SOURCE_DIR}/client/src/player/MixVisual.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/TradeWindow.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/ItemDb.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/PrefixDb.cpp)
target_include_directories(openkal_test_trade_ui PRIVATE ${CMAKE_SOURCE_DIR}/client/include)
target_link_libraries(openkal_test_trade_ui PRIVATE openkal::server openkal::engine
  openkal::map openkal::terrain Freetype::Freetype)
target_compile_definitions(openkal_test_trade_ui PRIVATE
  OPENKAL_REPO_ROOT="${CMAKE_SOURCE_DIR}" OPENKAL_TEST_RUNTIME_ROOT="${OPENKAL_GENERATED_RUNTIME_ROOT}")
add_dependencies(openkal_test_trade_ui openkal_runtime_icons)
openkal_target_warnings(openkal_test_trade_ui)
add_test(NAME trade_ui COMMAND openkal_test_trade_ui)

add_executable(openkal_test_population TestPopulationTest.cpp)
target_link_libraries(openkal_test_population PRIVATE openkal::server)
openkal_target_warnings(openkal_test_population)
add_test(NAME test_population COMMAND openkal_test_population)

add_executable(openkal_test_player_render_selection
  PlayerRenderSelectionTest.cpp
  ${CMAKE_SOURCE_DIR}/client/src/player/PlayerRenderSelection.cpp)
target_include_directories(openkal_test_player_render_selection PRIVATE ${CMAKE_SOURCE_DIR}/client/include)
openkal_target_warnings(openkal_test_player_render_selection)
add_test(NAME player_render_selection COMMAND openkal_test_player_render_selection)

add_executable(openkal_test_trading_post TradingPostTest.cpp)
target_link_libraries(openkal_test_trading_post PRIVATE openkal::server sqlite3)
openkal_target_warnings(openkal_test_trading_post)
add_test(NAME trading_post COMMAND openkal_test_trading_post)

add_executable(openkal_test_trading_post_ui TradingPostUiTest.cpp ${OPENKAL_HUD_TEST_SOURCES}
  ${CMAKE_SOURCE_DIR}/client/src/world/Nameplate.cpp
  ${CMAKE_SOURCE_DIR}/client/src/world/NpcCatalog.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/ItemTooltip.cpp
  ${CMAKE_SOURCE_DIR}/client/src/player/MixVisual.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/TradingPostWindow.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/ItemDb.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/PrefixDb.cpp)
target_include_directories(openkal_test_trading_post_ui PRIVATE ${CMAKE_SOURCE_DIR}/client/include)
target_link_libraries(openkal_test_trading_post_ui PRIVATE openkal::server openkal::engine
  openkal::map openkal::terrain Freetype::Freetype)
target_compile_definitions(openkal_test_trading_post_ui PRIVATE
  OPENKAL_REPO_ROOT="${CMAKE_SOURCE_DIR}" OPENKAL_TEST_RUNTIME_ROOT="${OPENKAL_GENERATED_RUNTIME_ROOT}")
add_dependencies(openkal_test_trading_post_ui openkal_runtime_icons)
openkal_target_warnings(openkal_test_trading_post_ui)
add_test(NAME trading_post_ui COMMAND openkal_test_trading_post_ui)

add_executable(openkal_test_trading_post_server TradingPostServerTest.cpp)
target_link_libraries(openkal_test_trading_post_server PRIVATE openkal::server)
openkal_target_warnings(openkal_test_trading_post_server)
add_test(NAME trading_post_server COMMAND openkal_test_trading_post_server)
set_tests_properties(trading_post_server PROPERTIES TIMEOUT 45)

add_executable(openkal_test_item_stacking_server ItemStackingServerTest.cpp)
target_link_libraries(openkal_test_item_stacking_server PRIVATE openkal::server sqlite3)
openkal_target_warnings(openkal_test_item_stacking_server)
add_test(NAME item_stacking_server COMMAND openkal_test_item_stacking_server)
set_tests_properties(item_stacking_server PROPERTIES TIMEOUT 45)

add_executable(openkal_test_item_ownership ItemOwnershipTest.cpp)
target_link_libraries(openkal_test_item_ownership PRIVATE openkal::server sqlite3)
openkal_target_warnings(openkal_test_item_ownership)
add_test(NAME item_ownership COMMAND openkal_test_item_ownership)

add_executable(openkal_test_user_roles UserRolesTest.cpp)
target_link_libraries(openkal_test_user_roles PRIVATE openkal::server sqlite3)
target_compile_definitions(openkal_test_user_roles PRIVATE OPENKAL_REPO_ROOT="${CMAKE_SOURCE_DIR}")
openkal_target_warnings(openkal_test_user_roles)
add_test(NAME user_roles COMMAND openkal_test_user_roles)

add_executable(openkal_test_management_api ManagementApiTest.cpp)
target_link_libraries(openkal_test_management_api PRIVATE openkal::server)
openkal_target_warnings(openkal_test_management_api)
add_test(NAME management_api COMMAND openkal_test_management_api)

# Characterization targets stay enabled without optional game data.
include(OptimizationServerCoverage.cmake)
include(DoggebiLordServer.cmake)
include(OptimizationClientCoverage.cmake)
include(OptimizationToolsCoverage.cmake)
include(OptimizationCraftingCoverage.cmake)
include(OptimizationMultilineCoverage.cmake)
include(OptimizationBackpressure.cmake)
include(MultiplayerLoad.cmake)
include(StorageServer.cmake)

add_executable(openkal_test_optimization_output_queue OptimizationOutputQueueTest.cpp)
target_link_libraries(openkal_test_optimization_output_queue PRIVATE openkal::server)
openkal_target_warnings(openkal_test_optimization_output_queue)
add_test(NAME optimization_output_queue COMMAND openkal_test_optimization_output_queue)
set_tests_properties(optimization_output_queue PROPERTIES LABELS "optimization-coverage")

add_executable(openkal_test_optimization_protocol OptimizationProtocolCoverageTest.cpp)
target_link_libraries(openkal_test_optimization_protocol PRIVATE openkal::protocol)
openkal_target_warnings(openkal_test_optimization_protocol)
add_test(NAME optimization_protocol_coverage COMMAND openkal_test_optimization_protocol)
set_tests_properties(optimization_protocol_coverage PROPERTIES LABELS "optimization-coverage")

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND UNIX)
  add_executable(openkal_test_optimization_tls_coverage OptimizationTlsCoverageTest.cpp)
  add_executable(openkal_test_optimization_tls_public OptimizationTlsCoverageTest.cpp)
  target_compile_definitions(openkal_test_optimization_tls_public PRIVATE OPENKAL_TLS_PUBLIC_ONLY=1)
  foreach(tls_target IN ITEMS openkal_test_optimization_tls_coverage openkal_test_optimization_tls_public)
    target_link_libraries(${tls_target} PRIVATE openkal::server)
    target_link_options(${tls_target} PRIVATE
      -Wl,--wrap=poll -Wl,--wrap=SSL_write_ex -Wl,--wrap=SSL_read_ex
      -Wl,--wrap=SSL_get_error -Wl,--wrap=send -Wl,--wrap=recv)
    openkal_target_warnings(${tls_target})
  endforeach()
  add_test(NAME optimization_tls_coverage COMMAND openkal_test_optimization_tls_coverage)
  add_test(NAME optimization_tls_public COMMAND openkal_test_optimization_tls_public)
  set_tests_properties(optimization_tls_coverage optimization_tls_public PROPERTIES
    LABELS "optimization-coverage" TIMEOUT 15)
endif()

add_test(NAME coverage_reporter
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/test_coverage_report.py")

if(OPENKAL_ENABLE_COVERAGE)
  add_custom_target(openkal_coverage_report
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/coverage_report.py"
      --root "${CMAKE_SOURCE_DIR}" --build-dir "${CMAKE_BINARY_DIR}"
      --manifest "${CMAKE_SOURCE_DIR}/tests/OptimizationCoverageScope.json"
      --gcov "${OPENKAL_GCOV_EXECUTABLE}"
      --output-dir "${CMAKE_BINARY_DIR}/coverage"
      --report-only
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Reporting raw line and branch coverage for the planned change sites"
    VERBATIM)
endif()

# Apply sharing after source-cloning characterization targets have been configured.
include(SharedUiTestSources.cmake)

include(ArcherSchedulerServer.cmake)

include(MageSchedulerServer.cmake)
include(WorldEventServer.cmake)

add_test(NAME source_packaging
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/test_package_source.py")
add_test(NAME image_inventory
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/check_image_inventory.py")
add_test(NAME installed_launcher
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/test_installed_launcher.py")
