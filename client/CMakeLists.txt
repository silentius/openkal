set(OPENKAL_CLIENT_HEADERS
  include/openkal/client/AccountTextInput.h
  include/openkal/client/ClientApplication.h
  include/openkal/client/ClientSession.h
  include/openkal/client/ClientCpuProfiler.h
  include/openkal/client/ClientInput.h
  include/openkal/client/ClientRuntimeConfig.h
  include/openkal/client/ClientServerConfig.h
  include/openkal/client/ClientTlsConfig.h
  include/openkal/client/ClientShutdownSignal.h
  include/openkal/client/ClientUiResources.h
  include/openkal/client/ClientUiConfiguration.h
  include/openkal/client/FrameProfiler.h
  include/openkal/client/StartupScreen.h
  include/openkal/client/LoadingScreen.h
  include/openkal/client/AnimatedActor.h
  include/openkal/client/AnimSound.h
  include/openkal/client/ArcherFxDispatcher.h
  include/openkal/client/Audio.h
  include/openkal/client/BgmController.h
  include/openkal/client/BattleMsgBuilder.h
  include/openkal/client/ChatCommandRegistry.h
  include/openkal/client/ChatFilterModel.h
  include/openkal/client/ChatWidget.h
  include/openkal/client/OverheadChat.h
  include/openkal/client/ClickMarker.h
  include/openkal/client/RangeSkillDecal.h
  include/openkal/client/ArrowHitFeedback.h
  include/openkal/client/CombatFx.h
  include/openkal/client/CombatFloatText.h
  include/openkal/client/DebugOverlay.h
  include/openkal/client/DropItemVisual.h
  include/openkal/client/EnchantOverlay.h
  include/openkal/client/SelectMarker.h
  include/openkal/client/SoftCursor.h
  include/openkal/client/Equipment.h
  include/openkal/client/DragonHud.h
  include/openkal/client/FishingBar.h
  include/openkal/client/ExpEventBoard.h
  include/openkal/client/Floor.h
  include/openkal/client/FreetypeFont.h
  include/openkal/client/GameHud.h
  include/openkal/client/GameMenu.h
  include/openkal/client/InGameWindowOrder.h
  include/openkal/client/SettingsWindow.h
  include/openkal/client/PartyWindow.h
  include/openkal/client/PlayerOptionsWindow.h
  include/openkal/client/BootShell.h
  include/openkal/client/CharacterCreationDraft.h
  include/openkal/client/BofFx.h
  include/openkal/client/CharacterInfoWindow.h
  include/openkal/client/IndoorMapView.h
  include/openkal/client/Inventory.h
  include/openkal/client/ItemDb.h
  include/openkal/client/ItemTooltip.h
  include/openkal/client/PrefixDb.h
  include/openkal/client/JobTitles.h
  include/openkal/client/LatinText.h
  include/openkal/client/MenuBarWidget.h
  include/openkal/client/MinimapWidget.h
  include/openkal/client/MonsterCatalog.h
  include/openkal/client/MonsterRender.h
  include/openkal/client/Nameplate.h
  include/openkal/client/NetClient.h
  include/openkal/client/TalentTreeWindow.h
  include/openkal/client/PowerbookCatalog.h
  include/openkal/client/PowerbookWindow.h
  include/openkal/client/MailWindow.h
  include/openkal/client/NpcCatalog.h
  include/openkal/client/NpcTalkWindow.h
  include/openkal/client/BlacksmithWindow.h
  include/openkal/client/MixWindow.h
  include/openkal/client/ShopWindow.h
  include/openkal/client/SkinShopWindow.h
  include/openkal/client/StorageWindow.h
  include/openkal/client/SpeedMedicineWindow.h
  include/openkal/client/ConsumableCraftingWindow.h
  include/openkal/client/MixVisual.h
  include/openkal/client/DialogCatalog.h
  include/openkal/client/TaskCatalog.h
  include/openkal/client/TextWrap.h
  include/openkal/client/LocationMapView.h
  include/openkal/client/QuestSnitch.h
  include/openkal/client/QuestJournalWindow.h
  include/openkal/client/QuestMarker.h
  include/openkal/client/Player.h
  include/openkal/client/PetController.h
  include/openkal/client/PileSkillBar.h
  include/openkal/client/PlayerRenderSelection.h
  include/openkal/client/RemotePlayers.h
  include/openkal/client/RemotePlayerMotion.h
  include/openkal/client/SkillBar.h
  include/openkal/client/SkillWindow.h
  include/openkal/client/TransformWindow.h
  include/openkal/client/ShinsuDb.h
  include/openkal/client/Skills.h
  include/openkal/client/SkillEffectTable.h
  include/openkal/client/SkillPresenter.h
  include/openkal/client/LoginMapStage.h
  include/openkal/client/SkyBox.h
  include/openkal/client/SpriteFx.h
  include/openkal/client/StatusWidget.h
  include/openkal/client/UiLayout.h
  include/openkal/client/UiDocument.h
  include/openkal/client/RidingEquipment.h
  include/openkal/client/RidingCacheOverlay.h
  include/openkal/client/RidingUiHost.h
  include/openkal/client/UiView.h
  include/openkal/client/UiSettings.h
  include/openkal/client/UiWidgets.h
  include/openkal/client/WidgetTools.h
  include/openkal/client/WorldPick.h
  include/openkal/client/WorldObjectCatalog.h
  include/openkal/client/WorldMapWidget.h
  include/openkal/client/WorldRenderCache.h
)

set(OPENKAL_CLIENT_SOURCES
  src/main.cpp
  src/ClientApplication.cpp
  src/ClientSession.cpp
  src/ClientCpuProfiler.cpp
  src/ClientInput.cpp
  src/ClientRuntimeConfig.cpp
  src/ClientShutdownSignal.cpp
  src/RenderSettingsApplier.cpp
  src/ClientUiResources.cpp
  src/ClientUiConfiguration.cpp
  src/FrameProfiler.cpp
  src/StartupScreen.cpp
  src/LoadingScreen.cpp
  src/monster/MonsterCatalog.cpp
  src/monster/MonsterRender.cpp
  src/player/Player.cpp
  src/player/PetController.cpp
  src/player/AnimatedActor.cpp
  src/player/PlayerRenderSelection.cpp
  src/player/RemotePlayers.cpp
  src/player/RemotePlayerMotion.cpp
  src/player/BofFx.cpp
  src/player/Equipment.cpp
  src/player/MixVisual.cpp
  src/player/Skills.cpp
  src/player/SkillEffectTable.cpp
  src/player/SkillPresenter.cpp
  src/player/ArcherFxDispatcher.cpp
  src/player/KnightFxDispatcher.cpp
  src/world/ExpEventBoard.cpp
  src/world/SkyBox.cpp
  src/world/NpcCatalog.cpp
  src/world/WorldObjectCatalog.cpp
  src/world/IndoorMapView.cpp
  src/world/Nameplate.cpp
  src/world/ArrowHitFeedback.cpp
  src/world/CombatFx.cpp
  src/world/CombatFloatText.cpp
  src/world/SpriteFx.cpp
  src/world/ClickMarker.cpp
  src/world/RangeSkillDecal.cpp
  src/world/QuestMarker.cpp
  src/world/SelectMarker.cpp
  src/world/WorldPick.cpp
  src/world/LoginMapStage.cpp
  src/world/WorldRenderCache.cpp
  src/world/TerrainStream.cpp
  src/world/DebugOverlay.cpp
  src/world/DropItemVisual.cpp
  src/ui/WidgetTools.cpp
  src/ui/UiBmp.cpp
  src/ui/UiLayout.cpp
  src/ui/UiDocument.cpp
  src/ui/RidingEquipment.cpp
  src/ui/RidingCacheOverlay.cpp
  src/ui/RidingUiHost.cpp
  src/ui/UiView.cpp
  src/ui/UiSettings.cpp
  src/ui/SoftCursor.cpp
  src/ui/GameHud.cpp
  src/ui/GameMenu.cpp
  src/ui/InGameWindowOrder.cpp
  src/ui/SettingsWindow.cpp
  src/ui/TradingPostWindow.cpp
  src/ui/TradeWindow.cpp
  src/ui/PartyWindow.cpp
  src/ui/PlayerOptionsWindow.cpp
  src/ui/BattleMsgBuilder.cpp
  src/ui/ChatCommandRegistry.cpp
  src/ui/ChatFilterModel.cpp
  src/ui/ChatWidget.cpp
  src/ui/OverheadChat.cpp
  src/ui/EnchantOverlay.cpp
  src/ui/DragonHud.cpp
  src/ui/FishingBar.cpp
  src/ui/FreetypeFont.cpp
  src/ui/StatusWidget.cpp
  src/ui/TextWrap.cpp
  src/ui/BuffSlotBar.cpp
  src/ui/MinimapAreaName.cpp
  src/ui/MinimapWidget.cpp
  src/ui/WorldMapWidget.cpp
  src/ui/MenuBarWidget.cpp
  src/ui/SkillBar.cpp
  src/ui/SkillWindow.cpp
  src/ui/TransformWindow.cpp
  src/ui/CharacterInfoWindow.cpp
  src/ui/Inventory.cpp
  src/ui/NpcTalkWindow.cpp
  src/ui/PileSkillBar.cpp
  src/ui/BlacksmithWindow.cpp
  src/ui/MixWindow.cpp
  src/ui/ShopWindow.cpp
  src/ui/SkinShopWindow.cpp
  src/ui/WeaponThumbnailCamera.cpp
  src/ui/StorageWindow.cpp
  src/ui/SpeedMedicineWindow.cpp
  src/ui/ConsumableCraftingWindow.cpp
  src/ui/LocationMapView.cpp
  src/misc/QuestSnitch.cpp
  src/ui/QuestJournalWindow.cpp
  src/ui/TalentTreeWindow.cpp
  src/ui/PowerbookWindow.cpp
  src/misc/PowerbookCatalog.cpp
  src/ui/GuildWindow.cpp
  src/ui/MailWindow.cpp
  src/ui/ItemTooltip.cpp
  src/misc/AnimSound.cpp
  src/misc/Audio.cpp
  src/misc/BgmController.cpp
  src/misc/NetClient.cpp
  src/misc/ServerClock.cpp
  src/misc/DialogCatalog.cpp
  src/misc/TaskCatalog.cpp
  src/misc/MultiplayerMenu.cpp
  src/misc/AutoLogin.cpp
  src/misc/ClientTlsConfig.cpp
  src/misc/ClientServerConfig.cpp
  src/misc/AccountTextInput.cpp
  src/misc/BootShell.cpp
  src/misc/BootShellCharacter.cpp
  src/misc/CharacterCreationDraft.cpp
  src/misc/JobTitles.cpp
  src/misc/ItemDb.cpp
  src/misc/ShinsuDb.cpp
  src/misc/PrefixDb.cpp
)

# Development trust stays local to this build and is absent from release binaries.
set(OPENKAL_DEVELOPMENT_TLS_CA "")
if(NOT OPENKAL_RELEASE_BUILD AND NOT CMAKE_BUILD_TYPE STREQUAL "Release")
  set(OPENKAL_DEVELOPMENT_TLS_CA "${CMAKE_SOURCE_DIR}/save/multiplayer/tls/server.pem")
endif()
set_source_files_properties(src/misc/ClientTlsConfig.cpp PROPERTIES
  COMPILE_DEFINITIONS "OPENKAL_DEVELOPMENT_TLS_CA=\"${OPENKAL_DEVELOPMENT_TLS_CA}\"")

set(OPENKAL_CLIENT_ENGINE_NAME "${OPENKAL_ENGINE}")
if(OPENKAL_ENGINE STREQUAL "gl")
  set(OPENKAL_CLIENT_ENGINE_NAME "GL")
endif()

add_executable(openkal_client
  ${OPENKAL_CLIENT_SOURCES}
  ${OPENKAL_CLIENT_HEADERS}
)
add_dependencies(openkal_client openkal_config_archive)

find_package(Threads REQUIRED)

target_link_libraries(openkal_client
  PRIVATE
    openkal::engine
    openkal::terrain
    openkal::map
    openkal::platform
    openkal::settings
    openkal::server
    openkal::protocol
    Threads::Threads
    ${CMAKE_DL_LIBS}
)

target_compile_features(openkal_client PRIVATE cxx_std_17)

target_compile_definitions(openkal_client
  PRIVATE
    OPENKAL_REPO_ROOT="${OPENKAL_RESOURCE_ROOT}"
    OPENKAL_VERSION="${PROJECT_VERSION}"
    OPENKAL_ENGINE_NAME="${OPENKAL_CLIENT_ENGINE_NAME}"
    $<$<OR:$<BOOL:${OPENKAL_RELEASE_BUILD}>,$<CONFIG:Release>>:OPENKAL_DISABLE_UI_HOT_RELOAD=1>
)

target_include_directories(openkal_client PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/third_party
)

find_package(Freetype REQUIRED)
target_link_libraries(openkal_client PRIVATE Freetype::Freetype)

install(TARGETS openkal_client RUNTIME DESTINATION lib/openkal)
install(FILES ${OPENKAL_CONFIG_ARCHIVE} DESTINATION share/openkal)
if(UNIX AND NOT APPLE)
  install(PROGRAMS ${CMAKE_SOURCE_DIR}/debian/openkal DESTINATION games)
  install(FILES ${CMAKE_SOURCE_DIR}/debian/openkal.desktop DESTINATION share/applications)
  install(FILES ${CMAKE_SOURCE_DIR}/RuntimeData/OpenKal.png
          DESTINATION share/pixmaps
          RENAME openkal.png)
  install(FILES ${CMAKE_SOURCE_DIR}/debian/openkal.profile.example DESTINATION share/openkal)
  install(FILES ${CMAKE_SOURCE_DIR}/debian/openkal.1 DESTINATION share/man/man1)
  install(FILES ${CMAKE_SOURCE_DIR}/debian/copyright DESTINATION share/doc/openkal-client)
endif()

# Extra CPU-profiled client (libgoogle-perftools-dev). Same sources, links -lprofiler.
#   cmake --build build --target openkal_client_profile
#   ./build/bin/openkal_client_profile   # writes openkal_cpu.prof (or $CPUPROFILE)
#   google-pprof --text ./build/bin/openkal_client_profile openkal_cpu.prof
find_path(GPERFTOOLS_INCLUDE_DIR gperftools/profiler.h)
find_library(GPERFTOOLS_PROFILER_LIBRARY NAMES profiler)
if(OPENKAL_BUILD_PROFILE_CLIENT AND GPERFTOOLS_INCLUDE_DIR AND GPERFTOOLS_PROFILER_LIBRARY)
  add_executable(openkal_client_profile
    ${OPENKAL_CLIENT_SOURCES}
    ${OPENKAL_CLIENT_HEADERS}
  )
  add_dependencies(openkal_client_profile openkal_config_archive)
  target_link_libraries(openkal_client_profile
    PRIVATE
      openkal::engine
      openkal::terrain
      openkal::map
      openkal::platform
      openkal::settings
      openkal::server
      openkal::protocol
      Threads::Threads
      ${CMAKE_DL_LIBS}
      ${GPERFTOOLS_PROFILER_LIBRARY}
      Freetype::Freetype
  )
  target_include_directories(openkal_client_profile
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/include
      ${CMAKE_SOURCE_DIR}/third_party
      ${GPERFTOOLS_INCLUDE_DIR}
  )
  target_compile_features(openkal_client_profile PRIVATE cxx_std_17)
  target_compile_definitions(openkal_client_profile
    PRIVATE
      OPENKAL_REPO_ROOT="${OPENKAL_RESOURCE_ROOT}"
      OPENKAL_VERSION="${PROJECT_VERSION}"
      OPENKAL_ENGINE_NAME="${OPENKAL_CLIENT_ENGINE_NAME}"
      OPENKAL_USE_GPERFTOOLS=1
      $<$<OR:$<BOOL:${OPENKAL_RELEASE_BUILD}>,$<CONFIG:Release>>:OPENKAL_DISABLE_UI_HOT_RELOAD=1>
  )
  message(STATUS "  openkal_client_profile enabled (${GPERFTOOLS_PROFILER_LIBRARY})")
else()
  message(STATUS "  openkal_client_profile skipped (install libgoogle-perftools-dev)")
endif()
