/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/protocol/Pvp.h"

#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketReader.h"
#include "openkal/protocol/PacketWriter.h"

#include <utility>

namespace openkal::protocol {

    namespace {
        constexpr std::uint8_t kAttackable = 1;
        constexpr std::uint8_t kDefeated = 2;
        constexpr std::uint8_t kMasked = 4;
        constexpr std::uint8_t kDueling = 8;
    }

    std::vector<std::uint8_t> frame_assassin_list(const AssassinList& list) {
        if (list.npc_id == 0 || list.entries.size() > kAssassinListLimit) {
            return {};
        }
        PacketWriter writer;
        writer.write_u32(list.npc_id);
        writer.write_u8(static_cast<std::uint8_t>(list.entries.size()));
        for (const auto& entry : list.entries) {
            writer.write_string(entry.assassin);
            writer.write_string(entry.last_hitter);
        }
        return writer.frame(static_cast<std::uint8_t>(S2C::AssassinList));
    }

    bool parse_assassin_list(const std::uint8_t* data, std::size_t size, AssassinList& list) {
        PacketReader reader(data, size);
        AssassinList parsed;
        std::uint8_t count = 0;
        if (!reader.read_u32(parsed.npc_id) || parsed.npc_id == 0 || !reader.read_u8(count) ||
            count > kAssassinListLimit) {
            return false;
        }
        parsed.entries.resize(count);
        for (auto& entry : parsed.entries) {
            if (!reader.read_string(entry.assassin) || entry.assassin.empty() ||
                !reader.read_string(entry.last_hitter) || entry.last_hitter.empty()) {
                return false;
            }
        }
        if (reader.remaining() != 0) {
            return false;
        }
        list = std::move(parsed);
        return true;
    }

    std::vector<std::uint8_t> frame_pvp_status(const PvpStatus& status) {
        PacketWriter writer;
        writer.write_u32(status.player_id);
        writer.write_u8(static_cast<std::uint8_t>(status.relation));
        writer.write_u8((status.attackable ? kAttackable : 0) | (status.defeated ? kDefeated : 0) |
                        (status.masked ? kMasked : 0) | (status.dueling ? kDueling : 0));
        return writer.frame(static_cast<std::uint8_t>(S2C::PvpStatus));
    }

    bool parse_pvp_status(const std::uint8_t* data, std::size_t size, PvpStatus& status) {
        PacketReader reader(data, size);
        std::uint8_t relation = 0, flags = 0;
        if (!reader.read_u32(status.player_id) || !reader.read_u8(relation) ||
            !reader.read_u8(flags) || reader.remaining() != 0 || status.player_id == 0 ||
            relation > static_cast<std::uint8_t>(PvpRelation::Enemy) ||
            (flags & ~(kAttackable | kDefeated | kMasked | kDueling)) != 0) {
            return false;
        }
        status.relation = static_cast<PvpRelation>(relation);
        status.attackable = (flags & kAttackable) != 0;
        status.defeated = (flags & kDefeated) != 0;
        status.masked = (flags & kMasked) != 0;
        status.dueling = (flags & kDueling) != 0;
        return true;
    }

    std::vector<std::uint8_t>
    frame_pvp_target_health_request(const PvpTargetHealthRequest& request) {
        PacketWriter writer;
        writer.write_u32(request.player_id);
        writer.write_u32(request.serial);
        return writer.frame(static_cast<std::uint8_t>(C2S::PvpTargetHealth));
    }

    bool parse_pvp_target_health_request(const std::uint8_t* data,
                                         std::size_t size,
                                         PvpTargetHealthRequest& request) {
        PacketReader reader(data, size);
        PvpTargetHealthRequest parsed;
        if (!reader.read_u32(parsed.player_id) || !reader.read_u32(parsed.serial) ||
            parsed.serial == 0 || reader.remaining() != 0) {
            return false;
        }
        request = parsed;
        return true;
    }

    std::vector<std::uint8_t> frame_pvp_target_health(const PvpTargetHealth& health) {
        PacketWriter writer;
        writer.write_u32(health.target.player_id);
        writer.write_u32(health.target.serial);
        writer.write_i32(health.hp);
        writer.write_i32(health.max_hp);
        return writer.frame(static_cast<std::uint8_t>(S2C::PvpTargetHealth));
    }

    bool
    parse_pvp_target_health(const std::uint8_t* data, std::size_t size, PvpTargetHealth& health) {
        PacketReader reader(data, size);
        PvpTargetHealth parsed;
        if (!reader.read_u32(parsed.target.player_id) || !reader.read_u32(parsed.target.serial) ||
            !reader.read_i32(parsed.hp) || !reader.read_i32(parsed.max_hp) ||
            reader.remaining() != 0 || parsed.hp < 0 || parsed.max_hp < parsed.hp ||
            (parsed.target.player_id == 0 ? parsed.target.serial != 0 || parsed.max_hp != 0
                                          : parsed.target.serial == 0)) {
            return false;
        }
        health = parsed;
        return true;
    }

}  // namespace openkal::protocol
