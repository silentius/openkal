/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include "openkal/protocol/Chatting.h"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace openkal::client {

    /** Other party members' item awards use the ItemGet filter and a darker green. */
    inline constexpr std::array<float, 3> kPartyItemColor{0.20f, 0.62f, 0.29f};

    /** Builds client-side battle / system chat strings from gameplay events. */
    class BattleMsgBuilder final {
      public:
        BattleMsgBuilder() = delete;

        /**
         * Formats damage dealt by the local player.
         * @param damage Damage amount.
         * @return Display line.
         */
        [[nodiscard]] static std::string damage_dealt(int damage);

        /**
         * Formats critical damage dealt by the local player.
         * @param damage Damage amount before any Explosive Blow extra damage.
         * @return Display line.
         */
        [[nodiscard]] static std::string critical_damage_dealt(int damage);

        /**
         * Formats Explosive Blow extra damage.
         * @param damage Extra Explosive Blow amount.
         * @return Display line.
         */
        [[nodiscard]] static std::string explosive_blow(int damage);

        /**
         * Formats damage taken by the local player.
         * @param damage Damage amount.
         * @return Display line.
         */
        [[nodiscard]] static std::string damage_taken(int damage);

        /**
         * Formats critical damage received by the local player.
         * @param damage Damage amount.
         * @return Display line.
         */
        [[nodiscard]] static std::string critical_damage_taken(int damage);

        /**
         * Formats a local evade line.
         * @param text Evade sentence; a fallback is used when empty.
         * @return Display line.
         */
        [[nodiscard]] static std::string evaded_attack(std::string_view text);

        /**
         * Formats an HP / MP / EXP delta line.
         * @param label Property label (e.g. "Health Point").
         * @param delta Signed change.
         * @return Display line, or empty when delta is 0.
         */
        [[nodiscard]] static std::string property_delta(std::string_view label, std::int64_t delta);

        /**
         * Formats an item acquisition line.
         * @param item_name Item display name.
         * @param count Stack count.
         * @return Display line.
         */
        [[nodiscard]] static std::string item_acquired(std::string_view item_name, int count);

        /** Formats an item awarded to another party member.
         * @param recipient Recipient name. @param item_name Item display name.
         * @param count Awarded stack count. @return Display line. */
        [[nodiscard]] static std::string
        party_item_acquired(std::string_view recipient, std::string_view item_name, int count);

        /**
         * Formats an item removal line.
         * @param item_name Item display name.
         * @param count Removed stack count.
         * @return Display line.
         */
        [[nodiscard]] static std::string item_removed(std::string_view item_name, int count);

        /**
         * Formats a Geon acquisition line.
         * @param amount Collected Geon amount.
         * @return Display line.
         */
        [[nodiscard]] static std::string money_acquired(int amount);

        /**
         * Formats a first-time skill learn line.
         * @param skill_name Skill display name.
         * @return Display line.
         */
        [[nodiscard]] static std::string skill_learned(std::string_view skill_name);

        /**
         * Formats a skill rank-up line.
         * @param skill_name Skill display name.
         * @param level New learned level.
         * @return Display line.
         */
        [[nodiscard]] static std::string skill_level_up(std::string_view skill_name, int level);

        /**
         * Maps a system MESSAGE ordinal to a chat string.
         * @param msg_id MESSAGE_TYPE ordinal.
         * @return Display line, or empty when unknown.
         */
        [[nodiscard]] static std::string system_message(std::uint8_t msg_id);
    };

}  // namespace openkal::client
