/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/debug/DebugDummyPlayers.h"

#include "openkal/map/Teleport.h"
#include "openkal/protocol/CreatePlayer.h"
#include "openkal/server/Combat.h"

#include <algorithm>

namespace openkal::server {
    namespace {

        /** Horizontal spacing between dummies in server pixels (~8 m). */
        constexpr int kDummySpacingPx = 80;

        struct DummySpec {
            const char* name;
            int clazz;
            int dx;
            int dy;
        };

        constexpr DummySpec kSpecs[] = {
            {"DummyKnight", combat::kPcKnight, 0, 0},
            {"DummyMage", combat::kPcMage, kDummySpacingPx, 0},
            {"DummyArcher", combat::kPcArcher, 0, kDummySpacingPx},
            {"DummyThief", combat::kPcThief, kDummySpacingPx, kDummySpacingPx},
        };

    }  // namespace

    std::vector<std::uint8_t> DummyPlayer::create_player_frame() const {
        return protocol::CreatePlayer::frame(m_player_id,
                                             m_name,
                                             static_cast<std::uint8_t>(m_clazz),
                                             m_px,
                                             m_py,
                                             m_pz,
                                             0,
                                             m_gstate,
                                             m_wear,
                                             0,
                                             0);
    }

    void DummyPlayer::reset() {
        m_cur_hp = m_max_hp;
        m_cur_mp = m_max_mp;
        m_control_until_ms = 0;
        m_skill_buffs = SkillBuffSet{};
        m_last_mp_drain_unix = 0;
    }

    void DummyPlayer::drain_archer_mp(std::int64_t now_unix) {
        if (now_unix == m_last_mp_drain_unix) {
            return;
        }
        m_last_mp_drain_unix = now_unix;
        const std::int32_t drain = m_skill_buffs.archer_mana_burn_per_tick() +
                                   m_skill_buffs.archer_silence_shot_mp_drain();
        if (drain > 0) {
            m_cur_mp = std::max<std::int32_t>(0, m_cur_mp - drain);
        }
    }

    bool DebugDummyPlayers::spawn(std::uint32_t& next_entity_id) {
        if (!m_players.empty()) {
            return true;
        }
        const map::TeleportLoc* fort = map::teleport_loc(4);
        if (fort == nullptr) {
            return false;
        }
        m_players.clear();
        m_players.reserve(4);
        for (const DummySpec& spec : kSpecs) {
            DummyPlayer dummy;
            dummy.set_player_id(next_entity_id++);
            dummy.set_name(spec.name);
            dummy.set_clazz(spec.clazz);
            dummy.set_map(fort->map);
            dummy.set_px(fort->x + spec.dx);
            dummy.set_py(fort->y + spec.dy);
            dummy.set_pz(fort->z);
            dummy.set_level(1);
            dummy.set_max_hp(100);
            dummy.set_cur_hp(100);
            dummy.set_max_mp(50);
            dummy.set_cur_mp(50);
            m_players.push_back(std::move(dummy));
        }
        return m_players.size() == 4;
    }

    std::vector<std::uint32_t> DebugDummyPlayers::despawn() {
        std::vector<std::uint32_t> ids;
        ids.reserve(m_players.size());
        for (const DummyPlayer& dummy : m_players) {
            ids.push_back(dummy.player_id());
        }
        m_players.clear();
        return ids;
    }

    void DebugDummyPlayers::reset() {
        for (DummyPlayer& dummy : m_players) {
            dummy.reset();
        }
    }

    void DebugDummyPlayers::tick(std::int64_t now_unix) {
        for (DummyPlayer& dummy : m_players) {
            (void) dummy.skill_buffs().tick(now_unix);
            dummy.drain_archer_mp(now_unix);
        }
    }

    DummyPlayer* DebugDummyPlayers::find(std::uint32_t player_id) {
        for (DummyPlayer& dummy : m_players) {
            if (dummy.player_id() == player_id) {
                return &dummy;
            }
        }
        return nullptr;
    }

    const DummyPlayer* DebugDummyPlayers::find(std::uint32_t player_id) const {
        for (const DummyPlayer& dummy : m_players) {
            if (dummy.player_id() == player_id) {
                return &dummy;
            }
        }
        return nullptr;
    }

}  // namespace openkal::server
