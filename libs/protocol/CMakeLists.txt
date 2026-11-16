# SPDX-License-Identifier: GPL-3.0-or-later

add_library(openkal_protocol
  src/Attack.cpp
  src/BuffState.cpp
  src/ChatAmount.cpp
  src/Chatting.cpp
  src/ClockSync.cpp
  src/Cipher.cpp
  src/Cooking.cpp
  src/CreatePlayer.cpp
  src/DropItem.cpp
  src/Effect.cpp
  src/Emote.cpp
  src/ExpEventState.cpp
  src/FrameDrain.cpp
  src/EnchantItem.cpp
  src/GetItemId.cpp
  src/GameExit.cpp
  src/Handshake.cpp
  src/ItemInfo.cpp
  src/ItemWear.cpp
  src/ItemMonster.cpp
  src/LevelUp.cpp
  src/TalentTree.cpp
  src/Mail.cpp
  src/MapState.cpp
  src/Move.cpp
  src/MState.cpp
  src/MonsterRace.cpp
  src/PacketReader.cpp
  src/PacketWriter.cpp
  src/Guild.cpp
  src/PlayerOptions.cpp
  src/TradingPost.cpp
  src/Trade.cpp
  src/Party.cpp
  src/PlayerInfo.cpp
  src/Progression.cpp
  src/Property.cpp
  src/Quest.cpp
  src/Riding.cpp
  src/Shortcut.cpp
  src/Skill.cpp
  src/Shop.cpp
  src/Storage.cpp
  src/Teleport.cpp
  src/Transform.cpp
  src/TrashItem.cpp
  src/UseItem.cpp
  src/UpgradeItem.cpp
  src/MixingItem.cpp
)
add_library(openkal::protocol ALIAS openkal_protocol)

target_include_directories(openkal_protocol
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(openkal_protocol PUBLIC cxx_std_17)
# Position-independent for future shared linking from servers.
set_target_properties(openkal_protocol PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_link_libraries(openkal_protocol PUBLIC openkal::platform)
