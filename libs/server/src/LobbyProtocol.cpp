/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/LobbyProtocol.h"

namespace openkal::server {
    void write_lobby_character(protocol::PacketWriter& w, const CharacterRecord& c) {
        w.write_u64(static_cast<std::uint64_t>(c.id));
        w.write_u64(static_cast<std::uint64_t>(c.account_id));
        w.write_string(c.name);
        w.write_u64(static_cast<std::uint64_t>(c.clazz));
        w.write_u64(static_cast<std::uint64_t>(c.face));
        w.write_u64(static_cast<std::uint64_t>(c.hair));
        w.write_u64(static_cast<std::uint64_t>(c.map));
        w.write_u64(static_cast<std::uint64_t>(c.map_x));
        w.write_u64(static_cast<std::uint64_t>(c.map_y));
        w.write_u64(static_cast<std::uint64_t>(c.map_z));
        w.write_u64(static_cast<std::uint64_t>(c.revival_point));
        w.write_u64(static_cast<std::uint64_t>(c.level));
        w.write_u64(static_cast<std::uint64_t>(c.exp));
        w.write_u64(static_cast<std::uint64_t>(c.hp));
        w.write_u64(static_cast<std::uint64_t>(c.max_hp));
        w.write_u64(static_cast<std::uint64_t>(c.mp));
        w.write_u64(static_cast<std::uint64_t>(c.max_mp));
        w.write_u64(static_cast<std::uint64_t>(c.pu_point));
        w.write_u64(static_cast<std::uint64_t>(c.su_point));
        w.write_u64(static_cast<std::uint64_t>(c.contribution));
        w.write_u64(static_cast<std::uint64_t>(c.rage));
        w.write_u64(static_cast<std::uint64_t>(c.str));
        w.write_u64(static_cast<std::uint64_t>(c.hth));
        w.write_u64(static_cast<std::uint64_t>(c.intl));
        w.write_u64(static_cast<std::uint64_t>(c.wis));
        w.write_u64(static_cast<std::uint64_t>(c.dex));
        w.write_u64(static_cast<std::uint64_t>(c.money));
        w.write_u64(static_cast<std::uint64_t>(c.next_iid));
        w.write_u64(static_cast<std::uint64_t>(c.weapon_index));
        w.write_u64(static_cast<std::uint64_t>(c.armor_index));
        w.write_u64(static_cast<std::uint64_t>(c.transform_index));
        w.write_u64(static_cast<std::uint64_t>(c.transform_level));
        w.write_u64(static_cast<std::uint64_t>(c.transform_exp));
        w.write_u64(static_cast<std::uint64_t>(c.transform_put_on_ticks));
        w.write_u64(static_cast<std::uint64_t>(c.talent_mask));
        w.write_u64(static_cast<std::uint64_t>(c.beacon_ready_at));
        w.write_u64(static_cast<std::uint64_t>(c.specialty));
        w.write_u64(static_cast<std::uint64_t>(c.deletion_state));
        w.write_u64(static_cast<std::uint64_t>(c.delete_after));
    }
    bool read_lobby_character(protocol::PacketReader& r, CharacterRecord& c) {
        std::uint64_t n = 0;
        if (!r.read_u64(n)) {
            return false;
        }
        c.id = static_cast<std::int32_t>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.account_id = static_cast<std::int32_t>(n);
        if (!r.read_string(c.name) || c.name.size() > 64) {
            return false;
        }
        if (!r.read_u64(n)) {
            return false;
        }
        c.clazz = static_cast<std::int32_t>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.face = static_cast<std::int32_t>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.hair = static_cast<std::int32_t>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.map = static_cast<int>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.map_x = static_cast<int>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.map_y = static_cast<int>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.map_z = static_cast<int>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.revival_point = static_cast<std::int32_t>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.level = static_cast<std::int32_t>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.exp = static_cast<std::int64_t>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.hp = static_cast<std::int32_t>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.max_hp = static_cast<std::int32_t>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.mp = static_cast<std::int32_t>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.max_mp = static_cast<std::int32_t>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.pu_point = static_cast<std::int32_t>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.su_point = static_cast<std::int32_t>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.contribution = static_cast<std::int32_t>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.rage = static_cast<std::int32_t>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.str = static_cast<std::int32_t>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.hth = static_cast<std::int32_t>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.intl = static_cast<std::int32_t>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.wis = static_cast<std::int32_t>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.dex = static_cast<std::int32_t>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.money = static_cast<std::int64_t>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.next_iid = static_cast<std::uint32_t>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.weapon_index = static_cast<int>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.armor_index = static_cast<int>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.transform_index = static_cast<int>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.transform_level = static_cast<int>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.transform_exp = static_cast<int>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.transform_put_on_ticks = static_cast<int>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.talent_mask = static_cast<std::uint64_t>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.beacon_ready_at = static_cast<std::int64_t>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.specialty = static_cast<std::int32_t>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.deletion_state = static_cast<CharacterDeletionState>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.delete_after = static_cast<std::int64_t>(n);
        return r.ok();
    }
    void write_lobby_item(protocol::PacketWriter& w, const ItemRecord& c) {
        w.write_u64(static_cast<std::uint64_t>(c.iid));
        w.write_u64(static_cast<std::uint64_t>(c.character_id));
        w.write_u64(static_cast<std::uint64_t>(c.account_id));
        w.write_u64(static_cast<std::uint64_t>(c.index));
        w.write_u64(static_cast<std::uint64_t>(c.prefix));
        w.write_u64(static_cast<std::uint64_t>(c.count));
        w.write_u64(static_cast<std::uint64_t>(c.slot));
        w.write_u64(static_cast<std::uint64_t>(c.cur_end));
        w.write_u64(static_cast<std::uint64_t>(c.max_end));
        w.write_u64(static_cast<std::uint64_t>(c.x_attack));
        w.write_u64(static_cast<std::uint64_t>(c.x_magic));
        w.write_u64(static_cast<std::uint64_t>(c.x_hit));
        w.write_u64(static_cast<std::uint64_t>(c.x_defense));
        w.write_u64(static_cast<std::uint64_t>(c.x_dodge));
        w.write_u64(static_cast<std::uint64_t>(c.upgrade_level));
        w.write_u64(static_cast<std::uint64_t>(c.upgrade_rate));
        w.write_u64(static_cast<std::uint64_t>(c.mix_type));
        w.write_u64(static_cast<std::uint64_t>(c.bof));
        w.write_u64(static_cast<std::uint64_t>(c.qigong_perforated));
        w.write_u64(static_cast<std::uint64_t>(c.qigong_effect));
        w.write_u64(static_cast<std::uint64_t>(c.dragon_effect));
        w.write_u64(static_cast<std::uint64_t>(c.random_stat_type));
        w.write_u64(static_cast<std::uint64_t>(c.random_stat_value));
        w.write_u64(static_cast<std::uint64_t>(c.transform_level));
        w.write_u64(static_cast<std::uint64_t>(c.transform_exp));
        w.write_u64(static_cast<std::uint64_t>(c.pet_expire_at));
        w.write_u64(static_cast<std::uint64_t>(c.ride_expire_at));
        w.write_u64(static_cast<std::uint64_t>(c.account_bound) |
                    (static_cast<std::uint64_t>(c.character_bound) << 1));
        w.write_u64(static_cast<std::uint64_t>(c.enchant_protected));
    }
    bool read_lobby_item(protocol::PacketReader& r, ItemRecord& c) {
        std::uint64_t n = 0;
        if (!r.read_u64(n)) {
            return false;
        }
        c.iid = static_cast<std::int32_t>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.character_id = static_cast<std::int32_t>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.account_id = static_cast<std::int32_t>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.index = static_cast<int>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.prefix = static_cast<int>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.count = static_cast<int>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.slot = static_cast<int>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.cur_end = static_cast<int>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.max_end = static_cast<int>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.x_attack = static_cast<int>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.x_magic = static_cast<int>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.x_hit = static_cast<int>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.x_defense = static_cast<int>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.x_dodge = static_cast<int>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.upgrade_level = static_cast<int>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.upgrade_rate = static_cast<int>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.mix_type = static_cast<int>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.bof = static_cast<int>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.qigong_perforated = static_cast<int>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.qigong_effect = static_cast<int>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.dragon_effect = static_cast<int>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.random_stat_type = static_cast<int>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.random_stat_value = static_cast<int>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.transform_level = static_cast<int>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.transform_exp = static_cast<int>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.pet_expire_at = static_cast<std::int64_t>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.ride_expire_at = static_cast<std::int64_t>(n);
        if (!r.read_u64(n)) {
            return false;
        }
        c.account_bound = (n & 1u) != 0;
        c.character_bound = (n & 2u) != 0;
        if (!r.read_u64(n)) {
            return false;
        }
        c.enchant_protected = static_cast<bool>(n);
        return r.ok();
    }
}  // namespace openkal::server
