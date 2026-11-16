add_library(openkal_server
  src/ManagementApi.cpp
  src/UserRoles.cpp
  src/AccountPassword.cpp
  src/LobbyProtocol.cpp
  src/LobbyClient.cpp
  src/TestControl.cpp
  src/LobbyService.cpp
  src/CollectionEquipment.cpp
  src/EssenceBox.cpp
  src/Host.cpp
  src/HostRuntimeMetrics.cpp
  src/HarvestPlants.cpp
  src/GuildRules.cpp
  src/GuildService.cpp
  src/CastleWarRules.cpp
  src/CastleWarService.cpp
  src/PartyService.cpp
  src/BattleRiding.cpp
  src/PacketFramer.cpp
  src/PrefixEnchant.cpp
  src/WeaponEnchantment.cpp
  src/RandomItemStat.cpp
  src/InventoryUnlock.cpp
  src/ItemStacking.cpp
  src/PayBuff.cpp
  src/RestFoodBuff.cpp
  src/SkillBuff.cpp
  src/SpeedMedicine.cpp
  src/ConsumableCrafting.cpp
  src/Combat.cpp
  src/MonsterStrength.cpp
  src/PlayerRegen.cpp
  src/PlayerSkill.cpp
  src/chat/ChatCommandRegistry.cpp
  src/chat/ChatService.cpp
  src/debug/DebugDummyPlayers.cpp
  src/store/TradingPostStore.cpp
  src/store/SqliteStore.cpp
  src/world/StarterKit.cpp
  src/net/Transport.cpp
  src/net/HostConnection.cpp
  src/net/TlsSession.cpp
  src/world/GameWorld.cpp
  src/world/WorldInterestIndex.cpp
  src/world/WorldOccupancyIndex.cpp
  src/world/FindPath.cpp
  src/world/ItemGroupDb.cpp
  src/world/DropRoller.cpp
  src/world/QuestDb.cpp
  src/world/QuestEngine.cpp
  src/world/DoggebiRitual.cpp
  src/world/NpcRegistry.cpp
  src/MixCatalog.cpp
  src/BofCatalog.cpp
  src/DragonFusing.cpp
  src/Qigong.cpp
  src/ShopCatalog.cpp
  src/FishingCatalog.cpp
  src/LevelMailRewards.cpp
  src/RidingCacheCatalog.cpp
  src/RidingProgressionCatalog.cpp
  src/ServerEvent.cpp
  src/ClockSchedule.cpp
)
add_library(openkal::server ALIAS openkal_server)

target_include_directories(openkal_server
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

find_package(SQLite3 REQUIRED)
find_package(OpenSSL 3.0 REQUIRED)
option(OPENKAL_WITH_POSTGRES "Build the dedicated PostgreSQL persistence backend" OFF)
if(OPENKAL_WITH_POSTGRES)
  find_package(PostgreSQL REQUIRED)
  target_sources(openkal_server PRIVATE src/store/PostgresStore.cpp)
  target_link_libraries(openkal_server PRIVATE PostgreSQL::PostgreSQL)
  target_compile_definitions(openkal_server PUBLIC OPENKAL_WITH_POSTGRES=1)
endif()
target_link_libraries(openkal_server
  PUBLIC
    openkal::platform
    openkal::terrain
    openkal::map
    openkal::protocol
  PRIVATE
    SQLite::SQLite3
    OpenSSL::SSL
    OpenSSL::Crypto
)

target_compile_features(openkal_server PUBLIC cxx_std_17)
set_target_properties(openkal_server PROPERTIES POSITION_INDEPENDENT_CODE ON)
