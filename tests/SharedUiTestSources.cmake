# Share only explicitly verified groups with matching compile commands.
# Variant-specific sources and test entry points stay on their targets.
set(_openkal_shared_ui_core_sources
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiDocument.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiLayout.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiView.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/WidgetTools.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiBmp.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/FreetypeFont.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/TextWrap.cpp)
set(_openkal_shared_hud_sources ${OPENKAL_HUD_TEST_SOURCES})
list(REMOVE_ITEM _openkal_shared_hud_sources ${_openkal_shared_ui_core_sources})

function(openkal_share_ui_test_sources support_target exemplar source_list_name)
  set(shared_sources ${${source_list_name}})

  add_library(${support_target} OBJECT ${shared_sources})
  foreach(property IN ITEMS
      COMPILE_DEFINITIONS COMPILE_OPTIONS COMPILE_FLAGS INCLUDE_DIRECTORIES
      COMPILE_FEATURES CXX_STANDARD CXX_STANDARD_REQUIRED CXX_EXTENSIONS
      POSITION_INDEPENDENT_CODE CXX_VISIBILITY_PRESET VISIBILITY_INLINES_HIDDEN)
    get_property(property_set TARGET ${exemplar} PROPERTY ${property} SET)
    if(property_set)
      get_target_property(property_value ${exemplar} ${property})
      set_property(TARGET ${support_target} PROPERTY ${property} "${property_value}")
    endif()
  endforeach()
  get_target_property(link_libraries ${exemplar} LINK_LIBRARIES)
  target_link_libraries(${support_target} PRIVATE ${link_libraries})
  add_dependencies(${support_target} openkal_runtime_icons)

  foreach(consumer IN LISTS ARGN)
    get_target_property(consumer_sources ${consumer} SOURCES)
    foreach(source IN LISTS shared_sources)
      list(FIND consumer_sources "${source}" source_index)
      if(source_index EQUAL -1)
        message(FATAL_ERROR "${consumer} no longer contains shared UI source ${source}")
      endif()
    endforeach()
    list(REMOVE_ITEM consumer_sources ${shared_sources})
    set_property(TARGET ${consumer} PROPERTY SOURCES "${consumer_sources}")
    # Inject objects without adding usage requirements to the consumer or its test main.
    target_sources(${consumer} PRIVATE $<TARGET_OBJECTS:${support_target}>)
  endforeach()
endfunction()

openkal_share_ui_test_sources(openkal_test_ui_server_sources openkal_test_clock_hud
  _openkal_shared_ui_core_sources
  openkal_test_clock_hud
  openkal_test_character_info_hud
  openkal_test_quest_journal_hud
  openkal_test_companion_ui
  openkal_test_blacksmith_ui
  openkal_test_consumable_crafting_ui
  openkal_test_talent_tree
  openkal_test_powerbook
  openkal_test_trade_ui
  openkal_test_trading_post_ui)

openkal_share_ui_test_sources(openkal_test_ui_engine_sources openkal_test_minimap_hud
  _openkal_shared_ui_core_sources
  openkal_test_minimap_hud
  openkal_test_menu_bar_hud
  openkal_test_player_options
  openkal_test_mail_hud
  openkal_test_multiplayer_chat_hud
  openkal_test_npc_talk_ui)

openkal_share_ui_test_sources(openkal_test_hud_server_sources openkal_test_clock_hud
  _openkal_shared_hud_sources
  openkal_test_clock_hud
  openkal_test_character_info_hud
  openkal_test_quest_journal_hud
  openkal_test_companion_ui
  openkal_test_trade_ui
  openkal_test_trading_post_ui)

openkal_share_ui_test_sources(openkal_test_hud_engine_sources openkal_test_minimap_hud
  _openkal_shared_hud_sources
  openkal_test_minimap_hud
  openkal_test_menu_bar_hud
  openkal_test_player_options
  openkal_test_mail_hud
  openkal_test_multiplayer_chat_hud)

unset(_openkal_shared_ui_core_sources)
unset(_openkal_shared_hud_sources)
