/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/NetClient.h"

#include "openkal/client/BattleMsgBuilder.h"
#include "openkal/client/DebugOverlay.h"
#include "openkal/client/ItemDb.h"
#include "openkal/client/JobTitles.h"
#include "openkal/client/SkillEffectTable.h"
#include "openkal/client/Skills.h"
#include "openkal/map/Coords.h"
#include "openkal/platform/Log.h"
#include "openkal/protocol/Attack.h"
#include "openkal/protocol/BuffState.h"
#include "openkal/protocol/Chatting.h"
#include "openkal/protocol/Cooking.h"
#include "openkal/protocol/DropItem.h"
#include "openkal/protocol/Effect.h"
#include "openkal/protocol/EnchantItem.h"
#include "openkal/protocol/FrameDrain.h"
#include "openkal/protocol/GameExit.h"
#include "openkal/protocol/Handshake.h"
#include "openkal/protocol/ItemInfo.h"
#include "openkal/protocol/ItemMonster.h"
#include "openkal/protocol/ItemWear.h"
#include "openkal/protocol/MState.h"
#include "openkal/protocol/Mail.h"
#include "openkal/protocol/MapState.h"
#include "openkal/protocol/MixingItem.h"
#include "openkal/protocol/MonsterRace.h"
#include "openkal/protocol/Move.h"
#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketReader.h"
#include "openkal/protocol/PacketWriter.h"
#include "openkal/protocol/Progression.h"
#include "openkal/protocol/Property.h"
#include "openkal/protocol/Quest.h"
#include "openkal/protocol/Riding.h"
#include "openkal/protocol/Shop.h"
#include "openkal/protocol/Shortcut.h"
#include "openkal/protocol/Skill.h"
#include "openkal/protocol/Teleport.h"
#include "openkal/protocol/Transform.h"
#include "openkal/protocol/TrashItem.h"
#include "openkal/protocol/UpgradeItem.h"
#include "openkal/protocol/UseItem.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <string>
#include <unordered_map>
#include <utility>

namespace openkal::client {
    namespace {

        /** Squared 2D distance in client XZ. */
        [[nodiscard]] float dist2_xz(float ax, float az, float bx, float bz) {
            const float dx = ax - bx;
            const float dz = az - bz;
            return dx * dx + dz * dz;
        }

        /**
         * Picks the fall origin for a newly created ground item.
         * Prefer a nearby monster (corpse / chest) so loot flies out of the body;
         * otherwise the local player (inventory drop).
         */
        void assign_drop_origin(NetGroundItem& g,
                                const std::unordered_map<std::uint32_t, NetMonster>& monsters,
                                float local_wx,
                                float local_wz) {
            g.origin_wx = g.wx;
            g.origin_wz = g.wz;
            constexpr float kMonsterOriginMax = 32.0f;
            constexpr float kPlayerOriginMax = 48.0f;
            const float monster_max2 = kMonsterOriginMax * kMonsterOriginMax;
            float best_mob = monster_max2;
            bool found_mob = false;
            float mob_wx = g.wx;
            float mob_wz = g.wz;
            for (const auto& [id, m] : monsters) {
                (void) id;
                const float d = dist2_xz(m.wx(), m.wz(), g.wx, g.wz);
                if (d <= monster_max2 && (!found_mob || d < best_mob)) {
                    best_mob = d;
                    mob_wx = m.wx();
                    mob_wz = m.wz();
                    found_mob = true;
                }
            }
            const float d_player = dist2_xz(local_wx, local_wz, g.wx, g.wz);
            if (found_mob && best_mob <= d_player) {
                g.origin_wx = mob_wx;
                g.origin_wz = mob_wz;
                return;
            }
            if (d_player <= kPlayerOriginMax * kPlayerOriginMax) {
                g.origin_wx = local_wx;
                g.origin_wz = local_wz;
            }
        }

    }  // namespace

    void NetMonster::tick_motion(float dt) {
        if (m_hp <= 0 || m_pending_die || m_pending_knee || m_pending_behead) {
            m_moving = false;
            return;
        }
        if (!m_moving || !m_has_abs_target) {
            return;
        }
        const float dx = m_tx - m_wx;
        const float dz = m_tz - m_wz;
        const float dist = std::sqrt(dx * dx + dz * dz);
        // Idle as soon as the visual stop point is reached. Waiting for MoveEnd left
        // them moonwalking in place until the server finished the same step.
        const float step = m_move_speed * dt;
        if (dist < 1.5f || step >= dist) {
            m_wx = m_tx;
            m_wz = m_tz;
            m_has_abs_target = false;
            if (m_stop_on_arrive || m_attack_after_move) {
                m_moving = false;
                m_stop_on_arrive = false;
            }
            if (m_attack_after_move) {
                m_attack_after_move = false;
                m_pending_attack = true;
            }
            // Chase segments chain with MoveOn (bEndPos=FALSE). Stay in the run clip
            // at the waypoint until the next On/End so the gait does not idle every
            // ~32px step.
        } else {
            m_wx += dx / dist * step;
            m_wz += dz / dist * step;
            m_yaw = std::atan2(-dx, -dz);
        }
    }

    void NetMonster::apply_mspeed_from_ticks(float walk_tick, float run_tick) {
        m_walk_speed = 32000.0f / std::max(1.0f, walk_tick);
        m_run_speed = 32000.0f / std::max(1.0f, run_tick);
        refresh_move_speed();
    }

    float NetMonster::speed_factor(std::int16_t speed_value) {
        return protocol::move_speed_factor(speed_value);
    }

    void NetMonster::refresh_move_speed() {
        const float base = m_running ? m_run_speed : m_walk_speed;
        m_move_speed = base * speed_factor(m_speed_value);
    }

    void NetMonster::apply_speed_value(std::int16_t speed_value) {
        m_speed_value = speed_value;
        refresh_move_speed();
    }

    float NetMonster::anim_speed_scale() const {
        return speed_factor(m_speed_value);
    }

    bool NetClient::connect_local_unix(const std::string& path) {
        clear_online_views();
        m_session = server::connect_unix(path);
        return m_session != nullptr;
    }

    bool NetClient::connect_tcp(const std::string& host, std::uint16_t port) {
        clear_online_views();
        m_session = server::connect_tcp(host, port);
        return m_session != nullptr;
    }

    void NetClient::adopt_session(std::unique_ptr<server::ISession> session) {
        close();
        m_session = std::move(session);
    }

    bool NetClient::handshake() {
        if (!m_session) {
            return false;
        }
        if (!send_packet(protocol::Handshake::build_connect(0))) {
            log::warn("NetClient: failed to send C2S_CONNECT");
            return false;
        }
        // Wait for S2C_CODE then reply ANS_CODE.
        for (int attempt = 0; attempt < 400; ++attempt) {
            pump_rx();
            if (!m_session || !m_session->alive()) {
                log::warn("NetClient: disconnected during handshake");
                return false;
            }
            // Must parse frames here ? pump_rx alone only fills the buffer.
            process_frames(/*origin_patch=*/32);
            if (m_got_code) {
                const auto ans = protocol::Handshake::build_answer(m_protocol_test);
                if (!send_packet(ans)) {
                    log::warn("NetClient: failed to send C2S_ANS_CODE");
                    return false;
                }
                m_handshook = true;
                log::info("NetClient: handshake complete");
                return true;
            }
            timespec ts{0, 5 * 1000 * 1000};  // 5ms
            nanosleep(&ts, nullptr);
        }
        log::warn("NetClient: handshake timeout (rx=" + std::to_string(m_rx.size()) + " bytes)");
        return false;
    }

    void NetClient::update(float dt, int origin_patch) {
        if (!m_session) {
            return;
        }
        pump_rx();
        process_frames(origin_patch);
        if (m_read_only) {
            m_observed_motion.update(dt);
        }
        if (!m_session || !m_session->alive()) {
            clear_online_views();
        }
        for (auto& [id, m] : m_monsters) {
            (void) id;
            m.tick_motion(dt);
        }
        for (auto& [id, g] : m_ground_items) {
            (void) id;
            g.age_sec += dt;
        }
    }

    bool NetClient::take_monster_attack(std::uint32_t id) {
        auto it = m_monsters.find(id);
        if (it == m_monsters.end() || !it->second.pending_attack()) {
            return false;
        }
        it->second.set_pending_attack(false);
        return true;
    }

    bool NetClient::take_monster_action_damage(std::uint32_t id, PendingMonsterActionDamage& out) {
        auto it = m_monsters.find(id);
        if (it == m_monsters.end()) {
            return false;
        }
        return it->second.take_pending_action_damage(out);
    }

    bool NetClient::take_monster_die(std::uint32_t id) {
        auto it = m_monsters.find(id);
        if (it == m_monsters.end() || !it->second.pending_die()) {
            return false;
        }
        it->second.set_pending_die(false);
        return true;
    }

    bool NetClient::take_monster_knee(std::uint32_t id) {
        auto it = m_monsters.find(id);
        if (it == m_monsters.end() || !it->second.pending_knee()) {
            return false;
        }
        it->second.set_pending_knee(false);
        return true;
    }

    bool NetClient::take_monster_behead(std::uint32_t id) {
        auto it = m_monsters.find(id);
        if (it == m_monsters.end() || !it->second.pending_behead()) {
            return false;
        }
        it->second.set_pending_behead(false);
        return true;
    }

    void NetClient::forget_monster(std::uint32_t id) {
        m_monsters.erase(id);
    }

    bool NetClient::send_get_item_monster(std::uint32_t monster_id) {
        if (!m_session || !m_handshook || monster_id == 0) {
            return false;
        }
        return send_packet(protocol::ItemMonster::frame_get(monster_id));
    }

    bool NetClient::send_cancel_item_monster(std::uint32_t monster_id) {
        if (!m_session || !m_handshook) {
            return false;
        }
        return send_packet(protocol::ItemMonster::frame_cancel(monster_id));
    }

    bool NetClient::take_chest_open_start(std::uint8_t& open_sec) {
        if (!m_have_chest_open_start) {
            return false;
        }
        open_sec = m_chest_open_sec;
        m_have_chest_open_start = false;
        return true;
    }

    bool NetClient::take_chest_open_stop() {
        if (!m_have_chest_open_stop) {
            return false;
        }
        m_have_chest_open_stop = false;
        return true;
    }

    void NetClient::set_monster_mspeed(std::uint32_t id, float walk_tick, float run_tick) {
        auto it = m_monsters.find(id);
        if (it == m_monsters.end()) {
            return;
        }
        it->second.apply_mspeed_from_ticks(walk_tick, run_tick);
    }

    bool NetClient::take_local_skill_cast() {
        if (!m_pending_local_skill) {
            return false;
        }
        m_pending_local_skill = false;
        return true;
    }

    bool NetClient::take_skill_action(PendingSkillAction& out) {
        if (m_pending_skill_actions.empty()) {
            return false;
        }
        out = m_pending_skill_actions.front();
        m_pending_skill_actions.pop_front();
        return true;
    }

    bool NetClient::take_transform_action(PendingTransformAction& out) {
        if (m_pending_transform_actions.empty()) {
            return false;
        }
        out = m_pending_transform_actions.front();
        m_pending_transform_actions.pop_front();
        return true;
    }

    bool NetClient::take_player_attack(PendingPlayerAttack& out) {
        if (m_pending_player_attacks.empty()) {
            return false;
        }
        out = m_pending_player_attacks.front();
        m_pending_player_attacks.pop_front();
        return true;
    }

    bool NetClient::take_hit_fx(PendingHitFx& out) {
        if (m_pending_hit_fx.empty()) {
            return false;
        }
        out = m_pending_hit_fx.front();
        m_pending_hit_fx.pop_front();
        return true;
    }

    bool NetClient::take_local_evade() {
        if (!m_pending_evade) {
            return false;
        }
        m_pending_evade = false;
        return true;
    }

    bool NetClient::take_combat_float(PendingCombatFloat& out) {
        if (m_pending_combat_floats.empty()) {
            return false;
        }
        out = std::move(m_pending_combat_floats.front());
        m_pending_combat_floats.pop_front();
        return true;
    }

    bool NetClient::take_delay_damage(PendingDelayDamage& out) {
        if (m_pending_delay_damage.empty()) {
            return false;
        }
        out = m_pending_delay_damage.front();
        m_pending_delay_damage.pop_front();
        return true;
    }

    void NetClient::enqueue_combat_float(CombatFloatKind kind,
                                         std::string text,
                                         std::uint32_t anchor_id,
                                         bool wait_for_bolt) {
        if (text.empty()) {
            return;
        }
        PendingCombatFloat line;
        line.kind = kind;
        line.text = std::move(text);
        line.anchor_id = anchor_id;
        line.wait_for_bolt = wait_for_bolt;
        m_pending_combat_floats.push_back(std::move(line));
    }

    bool NetClient::take_hit_from(std::uint32_t& attacker_id) {
        if (!m_have_hit_from) {
            return false;
        }
        attacker_id = m_hit_from_id;
        m_have_hit_from = false;
        return true;
    }

    bool NetClient::take_cur_hp(std::int32_t& hp) {
        if (!m_have_hp) {
            return false;
        }
        hp = m_cur_hp;
        m_have_hp = false;
        return true;
    }

    bool NetClient::take_cur_mp(std::int32_t& mp) {
        if (!m_have_mp) {
            return false;
        }
        mp = m_cur_mp;
        m_have_mp = false;
        return true;
    }

    bool NetClient::take_max_hp(std::int32_t& max_hp) {
        if (!m_have_max_hp) {
            return false;
        }
        max_hp = m_max_hp_prop;
        m_have_max_hp = false;
        return true;
    }

    bool NetClient::take_max_mp(std::int32_t& max_mp) {
        if (!m_have_max_mp) {
            return false;
        }
        max_mp = m_max_mp_prop;
        m_have_max_mp = false;
        return true;
    }

    bool NetClient::take_exp(std::int64_t& current, std::int64_t& total) {
        if (!m_have_exp) {
            return false;
        }
        current = m_exp_current;
        total = m_exp_total;
        m_have_exp = false;
        return true;
    }

    bool NetClient::take_level(std::int32_t& level) {
        if (!m_have_level) {
            return false;
        }
        level = m_level;
        m_have_level = false;
        return true;
    }

    bool NetClient::take_points(std::int32_t& pu, std::int32_t& su) {
        if (!m_have_points) {
            return false;
        }
        pu = m_pu_point;
        su = m_su_point;
        m_have_points = false;
        return true;
    }

    bool NetClient::take_level_up_effect(std::uint32_t& entity_id) {
        if (m_pending_level_up_fx.empty()) {
            return false;
        }
        entity_id = m_pending_level_up_fx.front();
        m_pending_level_up_fx.pop_front();
        return true;
    }

    bool NetClient::take_charm_effect(std::uint32_t& entity_id, std::uint8_t& effect) {
        if (m_pending_charm_fx.empty()) {
            return false;
        }
        entity_id = m_pending_charm_fx.front().first;
        effect = m_pending_charm_fx.front().second;
        m_pending_charm_fx.pop_front();
        return true;
    }

    bool NetClient::take_damaged_effect(std::uint32_t& entity_id, std::uint16_t& damage) {
        if (m_pending_damaged_fx.empty()) {
            return false;
        }
        entity_id = m_pending_damaged_fx.front().first;
        damage = m_pending_damaged_fx.front().second;
        m_pending_damaged_fx.pop_front();
        return true;
    }

    bool NetClient::take_skill_info(std::unordered_map<int, int>& learned) {
        if (!m_have_skill_info) {
            return false;
        }
        learned = m_skill_info;
        m_have_skill_info = false;
        return true;
    }

    bool NetClient::take_skill_up(int& index, int& level) {
        if (m_pending_skill_ups.empty()) {
            return false;
        }
        index = m_pending_skill_ups.front().first;
        level = m_pending_skill_ups.front().second;
        m_pending_skill_ups.pop_front();
        return true;
    }

    bool NetClient::take_shortcut(bool& present, protocol::ShortcutBar& bar) {
        if (!m_have_shortcut) {
            return false;
        }
        present = m_shortcut_present;
        bar = m_shortcut;
        m_have_shortcut = false;
        m_shortcut_present = false;
        m_shortcut = protocol::ShortcutBar{};
        return true;
    }

    bool NetClient::send_shortcut(const protocol::ShortcutBar& bar) {
        if (!m_session || !m_handshook) {
            return false;
        }
        return send_packet(protocol::Shortcut::frame_client_save(bar));
    }

    bool NetClient::send_revival() {
        if (!m_session || !m_handshook) {
            return false;
        }
        return send_packet(protocol::Skill::frame_client_revival());
    }

    bool NetClient::send_save_revival_point(std::uint8_t point, std::uint32_t npc_id) {
        if (!m_session || !m_handshook || npc_id == 0) {
            return false;
        }
        return send_packet(protocol::Skill::frame_client_save_revival_point(point, npc_id));
    }

    bool NetClient::send_update_property(std::uint8_t property, std::uint8_t count) {
        if (!m_session || !m_handshook) {
            return false;
        }
        if (property > static_cast<std::uint8_t>(protocol::PropertyType::Dex) || count == 0) {
            return false;
        }
        return send_packet(protocol::Property::frame_c2s_update(property, count));
    }

    void NetClient::set_char_identity(std::string name, int clazz, std::string job) {
        m_char_name = std::move(name);
        m_char_clazz = clazz;
        m_char_job = std::move(job);
        if (m_char_job.empty()) {
            m_char_job = JobTitles::class_name(clazz);
        }
        m_specialty_index = 0;
        m_specialty = protocol::kStartingSpecialty;
        m_char_gov_pos = JobTitles::title(clazz, m_specialty_index);
    }

    void NetClient::seed_char_primaries(std::int32_t str,
                                        std::int32_t hth,
                                        std::int32_t intl,
                                        std::int32_t wis,
                                        std::int32_t dex,
                                        std::int32_t pu) {
        m_str = str;
        m_hth = hth;
        m_intl = intl;
        m_wis = wis;
        m_dex = dex;
        m_pu_point = pu;
    }

    NetClient::CharInfoStats NetClient::char_info_stats() const {
        CharInfoStats s;
        s.name = m_char_name;
        s.clazz = m_char_clazz;
        s.job = m_char_job.empty() ? JobTitles::class_name(m_char_clazz) : m_char_job;
        s.guild = m_char_guild;
        s.guild_pos = m_char_guild_pos;
        s.specialty_index = m_specialty_index;
        s.gov_pos = m_char_gov_pos.empty() ? JobTitles::title(m_char_clazz, m_specialty_index)
                                           : m_char_gov_pos;
        s.awaken = m_char_awaken;
        s.level = m_level;
        s.ctb = m_char_ctb;
        s.hp = m_cur_hp;
        s.max_hp = m_max_hp_prop;
        s.mp = m_cur_mp;
        s.max_mp = m_max_mp_prop;
        s.str = m_str;
        s.hth = m_hth;
        s.intl = m_intl;
        s.wis = m_wis;
        s.dex = m_dex;
        s.pu_point = m_pu_point;
        s.min_attack = m_min_attack;
        s.max_attack = m_max_attack;
        s.min_magic = m_min_magic;
        s.max_magic = m_max_magic;
        s.hit = m_hit;
        s.dodge = m_dodge;
        s.defense = m_defense;
        s.absorb = m_absorb;
        s.res_fire = m_res_fire;
        s.res_ice = m_res_ice;
        s.res_lightning = m_res_lightning;
        s.res_curse = m_res_curse;
        s.res_palsy = m_res_palsy;
        return s;
    }

    bool NetClient::take_move_before(int& sx, int& sy, int& sz) {
        if (!m_have_move_before) {
            return false;
        }
        sx = m_move_before_sx;
        sy = m_move_before_sy;
        sz = m_move_before_sz;
        m_have_move_before = false;
        return true;
    }

    bool NetClient::send_attack_monster(std::uint32_t monster_id) {
        if (!m_session || !m_handshook || monster_id == 0) {
            return false;
        }
        return send_packet(protocol::Attack::frame_client(protocol::kCkMonster, monster_id));
    }

    bool NetClient::send_attack_player(std::uint32_t player) {
        if (!m_session || !m_handshook || !pvp_can_attack(player)) {
            return false;
        }
        return send_packet(protocol::Attack::frame_client(protocol::kCkPlayer, player));
    }

    bool NetClient::send_skill_target_monster(std::uint8_t skill, std::uint32_t monster_id) {
        if (!m_session || !m_handshook || monster_id == 0) {
            return false;
        }
        m_expect_skill_index = skill;
        m_expect_skill_target = monster_id;
        return send_packet(
            protocol::Skill::frame_client_target(skill, protocol::kCkMonster, monster_id));
    }

    bool NetClient::send_skill_target_monster_item(std::uint8_t skill,
                                                   std::uint32_t monster_id,
                                                   std::uint32_t item_iid) {
        return send_skill_target_item(skill, protocol::kCkMonster, monster_id, item_iid);
    }

    bool NetClient::send_skill_target_item(std::uint8_t skill,
                                           std::uint8_t kind,
                                           std::uint32_t target_id,
                                           std::uint32_t item_iid) {
        if (!m_session || !m_handshook || target_id == 0 || item_iid == 0) {
            return false;
        }
        protocol::PacketWriter writer;
        writer.write_u8(skill);
        writer.write_u8(kind);
        writer.write_u32(target_id);
        writer.write_u32(item_iid);
        m_expect_skill_index = skill;
        m_expect_skill_target = target_id;
        return send_packet(writer.frame(static_cast<std::uint8_t>(protocol::C2S::Skill)));
    }

    bool NetClient::send_skill_target_player(std::uint8_t skill, std::uint32_t player_id) {
        if (!m_session || !m_handshook || player_id == 0) {
            return false;
        }
        m_expect_skill_index = skill;
        m_expect_skill_target = player_id;
        return send_packet(
            protocol::Skill::frame_client_target(skill, protocol::kCkPlayer, player_id));
    }

    bool NetClient::send_skill_ground(std::uint8_t skill, std::int32_t x, std::int32_t y) {
        if (!m_session || !m_handshook) {
            return false;
        }
        m_expect_skill_index = skill;
        m_expect_skill_target = 0;
        return send_packet(protocol::Skill::frame_client_ground(skill, x, y));
    }

    bool NetClient::send_preskill(std::uint8_t skill, std::uint32_t target) {
        if (!m_session || !m_handshook || target == 0) {
            return false;
        }
        return send_packet(protocol::Skill::frame_preskill(skill, target));
    }

    bool NetClient::send_skill_self(std::uint8_t skill) {
        if (!m_session || !m_handshook) {
            return false;
        }
        return send_packet(protocol::Skill::frame_client_self(skill));
    }

    bool NetClient::send_transform(std::uint8_t flag, std::uint8_t stage) {
        if (!m_session || !m_handshook) {
            return false;
        }
        return send_packet(protocol::Transform::frame_c2s(flag, stage));
    }

    bool NetClient::send_transform_skill(std::uint8_t skill) {
        if (!m_session || !m_handshook) {
            return false;
        }
        return send_packet(protocol::Transform::frame_c2s_skill(skill));
    }

    bool NetClient::send_skill_boost(bool enable) {
        if (!m_session || !m_handshook) {
            return false;
        }
        return send_packet(protocol::Skill::frame_client_boost(enable ? 1 : 0));
    }

    bool NetClient::send_learn_skill(std::uint8_t skill) {
        if (!m_session || !m_handshook) {
            return false;
        }
        return send_packet(protocol::Skill::frame_client_learn_skill(skill));
    }

    bool NetClient::send_skill_up(std::uint8_t skill) {
        if (!m_session || !m_handshook) {
            return false;
        }
        return send_packet(protocol::Skill::frame_client_skill_up(skill));
    }

    bool NetClient::send_emote(std::uint8_t motion) {
        const auto frame = protocol::Emote::frame_client(motion);
        return m_session && !frame.empty() && send_packet(frame);
    }

    bool NetClient::take_emote(protocol::PlayerEmote& out) {
        if (m_pending_emotes.empty()) {
            return false;
        }
        out = m_pending_emotes.front();
        m_pending_emotes.pop_front();
        return true;
    }

    bool NetClient::send_rest(bool want_rest) {
        if (!m_session || !m_handshook) {
            return false;
        }
        return send_packet(protocol::Skill::frame_rest(want_rest ? 1 : 0));
    }

    bool NetClient::send_quest_job_end() {
        if (!m_session || !m_handshook) {
            return false;
        }
        protocol::PacketWriter writer;
        return send_packet(writer.frame(static_cast<std::uint8_t>(protocol::C2S::QuestJobEnd)));
    }

    bool NetClient::send_cooking(const protocol::CookingOrder& order) {
        if (!m_session || !m_handshook) {
            return false;
        }
        return send_packet(protocol::Cooking::frame_c2s(protocol::kCookingPotions, order));
    }

    bool NetClient::take_rest_state(bool& resting) {
        if (!m_have_rest) {
            return false;
        }
        resting = m_resting;
        m_have_rest = false;
        return true;
    }

    bool NetClient::take_fishing_progress_start() {
        if (!m_fishing_progress_start) {
            return false;
        }
        m_fishing_progress_start = false;
        return true;
    }

    bool NetClient::entity_resting(std::uint32_t entity_id) const {
        if (entity_id != 0 && entity_id == m_local_player_id) {
            return m_resting;
        }
        const auto it = m_entity_resting.find(entity_id);
        return it != m_entity_resting.end() && it->second;
    }

    bool NetClient::entity_consuming(std::uint32_t entity_id) const {
        const auto it = m_entity_mstate.find(entity_id);
        if (it == m_entity_mstate.end()) {
            return false;
        }
        return (it->second & (protocol::kCmsHp | protocol::kCmsMp)) != 0;
    }

    bool NetClient::send_puton_item(std::uint32_t iid) {
        if (!m_session || !m_handshook || iid == 0) {
            return false;
        }
        return send_packet(protocol::ItemWear::frame_client_put_on(iid));
    }

    bool NetClient::send_putoff_item(std::uint32_t iid) {
        if (!m_session || !m_handshook || iid == 0) {
            return false;
        }
        return send_packet(protocol::ItemWear::frame_client_put_off(iid));
    }

    bool NetClient::send_riding_toggle() {
        if (!m_session || !m_handshook) {
            return false;
        }
        return send_packet(protocol::Riding::frame_client_toggle());
    }

    bool NetClient::send_battle_riding_request(const protocol::BattleRidingRequest& request) {
        if (!m_session || !m_handshook || request.operation_id == 0 ||
            (protocol::battle_riding_operation_requires_mount(request.operation) &&
             request.mount_iid == 0)) {
            return false;
        }
        return send_packet(protocol::Riding::frame_client_battle_request(request));
    }

    bool NetClient::send_enchant_item(std::uint32_t from_iid, std::uint32_t to_iid) {
        if (!m_session || !m_handshook || from_iid == 0 || to_iid == 0 || from_iid == to_iid) {
            return false;
        }
        return send_packet(protocol::EnchantItem::frame_c2s(from_iid, to_iid));
    }

    bool NetClient::send_upgrade_item(protocol::UpgradeItemType type, std::uint32_t iid) {
        if (!m_session || !m_handshook || iid == 0) {
            return false;
        }
        return send_packet(protocol::UpgradeItem::frame_c2s(type, iid));
    }

    bool NetClient::send_mixing(std::uint8_t mode,
                                std::uint32_t weapon_iid,
                                std::uint32_t extra_weapon_iid,
                                std::uint32_t armor_iid,
                                std::uint32_t stone1_iid,
                                std::uint32_t stone2_iid) {
        if (!m_session || !m_handshook || weapon_iid == 0 || extra_weapon_iid == 0 ||
            armor_iid == 0 || stone1_iid == 0 || (mode != 0 && mode != 1)) {
            return false;
        }
        if (mode == 1 && stone2_iid == 0) {
            return false;
        }
        return send_packet(protocol::MixingItem::frame_c2s(
            mode, weapon_iid, extra_weapon_iid, armor_iid, stone1_iid, stone2_iid));
    }

    bool NetClient::take_enchant_failed() {
        if (!m_enchant_failed) {
            return false;
        }
        m_enchant_failed = false;
        return true;
    }

    bool NetClient::send_pickup_item(std::uint32_t item_id, int tile_x, int tile_y) {
        if (!m_session || !m_handshook || item_id == 0) {
            return false;
        }
        protocol::PacketWriter w;
        w.write_u32(item_id);
        w.write_u32(static_cast<std::uint32_t>(tile_x));
        w.write_u32(static_cast<std::uint32_t>(tile_y));
        return send_packet(w.frame(static_cast<std::uint8_t>(protocol::C2S::PickupItem)));
    }

    bool NetClient::send_drop_item(std::uint32_t iid, std::uint32_t count) {
        if (!m_session || !m_handshook || iid == 0 || count == 0) {
            return false;
        }
        return send_packet(protocol::DropItem::frame_c2s(iid, count));
    }

    bool NetClient::send_trash_item(std::uint32_t iid) {
        if (!m_session || !m_handshook || iid == 0) {
            return false;
        }
        return send_packet(protocol::TrashItem::frame_c2s(iid));
    }

    bool NetClient::send_use_item(std::uint32_t iid, std::uint8_t auto_hp_percent) {
        if (!m_session || !m_handshook || iid == 0) {
            return false;
        }
        return send_packet(protocol::UseItem::frame_c2s(iid, auto_hp_percent));
    }

    bool NetClient::send_ask_npc(std::uint32_t npc_id) {
        if (!m_session || !m_handshook || npc_id == 0) {
            return false;
        }
        const bool sent = send_packet(protocol::Quest::frame_ask_npc(npc_id));
        if (sent) {
            m_last_asked_npc_id = npc_id;
        }
        return sent;
    }

    bool NetClient::send_quest(std::uint16_t quest_index, std::uint8_t quest_flag) {
        if (!m_session || !m_handshook) {
            return false;
        }
        return send_packet(
            protocol::Quest::frame_quest(protocol::Quest::make_key(quest_index, quest_flag)));
    }

    bool NetClient::take_send_html(std::int32_t& out, std::uint32_t* npc_id) {
        if (m_pending_html.empty()) {
            return false;
        }
        out = m_pending_html.front().first;
        if (npc_id) {
            *npc_id = m_pending_html.front().second;
        }
        m_pending_html.pop_front();
        return true;
    }

    std::optional<protocol::AssassinList> NetClient::take_assassin_list() {
        return std::exchange(m_pending_assassin_list, std::nullopt);
    }

    bool NetClient::send_shop_item_list(std::uint32_t npc_id) {
        return m_session && m_handshook && npc_id != 0 &&
               send_packet(protocol::Shop::frame_item_list_c2s(npc_id));
    }

    bool
    NetClient::send_buy_item(std::uint32_t npc_id, std::uint16_t index, std::uint16_t quantity) {
        return m_session && m_handshook && npc_id != 0 && index != 0 && quantity != 0 &&
               send_packet(protocol::Shop::frame_buy_c2s(npc_id, index, quantity));
    }

    bool
    NetClient::send_sell_item(std::uint32_t npc_id, std::uint32_t iid, std::uint16_t quantity) {
        return m_session && m_handshook && npc_id != 0 && iid != 0 && quantity != 0 &&
               send_packet(protocol::Shop::frame_sell_c2s(npc_id, iid, quantity));
    }

    bool NetClient::take_shop_item_list(protocol::ShopItemList& out) {
        if (m_pending_shop_lists.empty()) {
            return false;
        }
        out = std::move(m_pending_shop_lists.front());
        m_pending_shop_lists.pop_front();
        return true;
    }

    bool NetClient::send_storage_info(std::uint32_t npc_id) {
        return m_session && m_handshook && npc_id != 0 &&
               send_packet(protocol::Storage::frame_info_c2s(npc_id));
    }

    bool
    NetClient::send_storage_deposit(std::uint32_t npc_id, std::uint32_t iid, std::uint32_t count) {
        return m_session && m_handshook && npc_id != 0 && iid != 0 && count != 0 &&
               send_packet(protocol::Storage::frame_transfer_c2s(true, npc_id, iid, count));
    }

    bool
    NetClient::send_storage_withdraw(std::uint32_t npc_id, std::uint32_t iid, std::uint32_t count) {
        return m_session && m_handshook && npc_id != 0 && iid != 0 && count != 0 &&
               send_packet(protocol::Storage::frame_transfer_c2s(false, npc_id, iid, count));
    }

    bool NetClient::take_storage_info(protocol::StorageInfo& out) {
        if (m_pending_storage_info.empty()) {
            return false;
        }
        out = std::move(m_pending_storage_info.front());
        m_pending_storage_info.pop_front();
        return true;
    }

    bool NetClient::send_storage_money(const protocol::StorageMoneyTransfer& transfer) {
        return m_session && m_handshook &&
               send_packet(protocol::Storage::frame_money_c2s(transfer));
    }

    bool NetClient::take_storage_balance(protocol::StorageBalance& out) {
        if (m_pending_storage_balances.empty()) {
            return false;
        }
        out = m_pending_storage_balances.front();
        m_pending_storage_balances.pop_front();
        return true;
    }

    bool NetClient::send_talent_request(protocol::TalentCommand command,
                                        std::uint64_t expected,
                                        std::uint64_t desired) {
        return m_session && m_handshook &&
               send_packet(protocol::frame_talent_request(command, expected, desired));
    }
    bool NetClient::take_talent_state(protocol::TalentState& out) {
        if (!m_talent_dirty) {
            return false;
        }
        out = m_talent_state;
        m_talent_dirty = false;
        return true;
    }

    bool NetClient::send_mail_list() {
        return m_session && m_handshook && send_packet(protocol::Mail::frame_list_c2s());
    }

    bool NetClient::send_mail_claim(std::uint32_t id) {
        return m_session && m_handshook && id != 0 &&
               send_packet(protocol::Mail::frame_claim_c2s(id));
    }

    bool NetClient::send_mail_action(protocol::MailCommand command, std::uint32_t id) {
        const auto packet = protocol::Mail::frame_action_c2s(command, id);
        return !packet.empty() && m_session && m_handshook && send_packet(packet);
    }

    bool NetClient::take_mail_claim_result(protocol::MailClaimResult& out) {
        if (m_pending_mail_claims.empty()) {
            return false;
        }
        out = m_pending_mail_claims.front();
        m_pending_mail_claims.pop_front();
        return true;
    }

    bool NetClient::take_insert_item(PendingInsertItem& out) {
        if (m_pending_inserts.empty()) {
            return false;
        }
        out = m_pending_inserts.front();
        m_pending_inserts.pop_front();
        return true;
    }

    bool NetClient::take_item_wear(PendingItemWear& out) {
        if (m_pending_item_wear.empty()) {
            return false;
        }
        out = m_pending_item_wear.front();
        m_pending_item_wear.pop_front();
        return true;
    }

    bool NetClient::take_riding_state(protocol::RidingState& out) {
        if (m_pending_riding_states.empty()) {
            return false;
        }
        out = m_pending_riding_states.front();
        m_pending_riding_states.pop_front();
        return true;
    }

    bool NetClient::take_riding_cache_result(protocol::RidingCacheResult& out) {
        if (m_pending_riding_cache_results.empty()) {
            return false;
        }
        out = m_pending_riding_cache_results.front();
        m_pending_riding_cache_results.pop_front();
        return true;
    }

    bool NetClient::take_update_item_info(protocol::UpdateItemInfoFields& out) {
        if (m_pending_item_info.empty()) {
            return false;
        }
        out = m_pending_item_info.front();
        m_pending_item_info.pop_front();
        return true;
    }

    bool NetClient::take_trash_item(std::uint32_t& iid) {
        if (m_pending_trash.empty()) {
            return false;
        }
        iid = m_pending_trash.front();
        m_pending_trash.pop_front();
        return true;
    }

    bool NetClient::send_guild(protocol::GuildRequest request) {
        if (!request.request_id) {
            const auto stamp = std::chrono::duration_cast<std::chrono::microseconds>(
                                   std::chrono::system_clock::now().time_since_epoch())
                                   .count();
            m_guild_request = std::max(m_guild_request + 1, static_cast<std::uint64_t>(stamp));
            request.request_id = m_guild_request;
        }
        m_guild_request = std::max(m_guild_request, request.request_id);
        return m_session && m_handshook && send_packet(protocol::frame_guild_request(request));
    }
    bool NetClient::take_guild_view(platform::Json& out) {
        if (m_pending_guild_views.empty()) {
            return false;
        }
        out = std::move(m_pending_guild_views.front());
        m_pending_guild_views.pop_front();
        return true;
    }
    std::uint64_t NetClient::send_player_options(protocol::PlayerOptionsRequest request) {
        const auto stamp = std::chrono::duration_cast<std::chrono::microseconds>(
                               std::chrono::system_clock::now().time_since_epoch())
                               .count();
        m_guild_request = std::max(m_guild_request + 1, static_cast<std::uint64_t>(stamp));
        request.request_id = m_guild_request;
        return m_session && m_handshook &&
                       send_packet(protocol::frame_player_options_request(request))
                   ? request.request_id
                   : 0;
    }
    bool NetClient::take_player_options(protocol::PlayerOptionsState& out) {
        if (m_pending_player_options.empty()) {
            return false;
        }
        out = std::move(m_pending_player_options.front());
        m_pending_player_options.pop_front();
        return true;
    }
    std::string NetClient::guild_name(std::uint32_t player) const {
        const auto found = m_guild_displays.find(player);
        return found != m_guild_displays.end() && found->second.at("visible").boolean()
                   ? found->second.at("name").as_string()
                   : std::string{};
    }
    std::string NetClient::guild_title(std::uint32_t player) const {
        const auto found = m_guild_displays.find(player);
        return found != m_guild_displays.end() && found->second.at("visible").boolean()
                   ? found->second.at("title").as_string()
                   : std::string{};
    }

    int NetClient::castle_side(std::uint32_t player) const {
        const auto found = m_guild_displays.find(player);
        if (found == m_guild_displays.end()) {
            return 0;
        }
        const auto& side = found->second.at("castle_side");
        return side.is_number() && (side.number() == 1 || side.number() == 2) ? side.as_int() : 0;
    }

    bool NetClient::castle_enemy(std::uint32_t player) const {
        const int own = castle_side(m_local_player_id);
        const int other = castle_side(player);
        return own != 0 && other != 0 && own != other;
    }

    bool NetClient::castle_defeated(std::uint32_t player) const {
        const auto found = m_guild_displays.find(player);
        if (found == m_guild_displays.end() || castle_side(player) == 0) {
            return false;
        }
        const auto& alive = found->second.at("castle_alive");
        return alive.type() == platform::Json::Type::Bool && !alive.boolean();
    }

    protocol::PvpStatus NetClient::pvp_status(std::uint32_t player) const {
        const auto found = m_pvp_status.find(player);
        if (found != m_pvp_status.end()) {
            return found->second;
        }
        using Relation = protocol::PvpRelation;
        const int own = castle_side(m_local_player_id);
        const int other = castle_side(player);
        const bool enemy = castle_enemy(player);
        const bool defeated = castle_defeated(player);
        const auto relation = enemy
                                  ? Relation::Enemy
                                  : (own != 0 && own == other ? Relation::Ally : Relation::Neutral);
        return {player, relation, enemy && !defeated, defeated, false};
    }

    bool NetClient::take_duel_defeat(std::uint32_t player) {
        return m_pending_duel_defeats.erase(player) != 0;
    }

    bool NetClient::pvp_can_attack(std::uint32_t player) const {
        return pvp_status(player).attackable;
    }

    void NetClient::select_pvp_health_target(std::uint32_t player) {
        if (!m_pvp_health_supported || m_read_only) {
            return;
        }
        if (player == m_local_player_id || !pvp_can_attack(player)) {
            player = 0;
        }
        if (player == m_pvp_health_request.player_id) {
            return;
        }
        m_pvp_target_health.reset();
        auto serial = m_pvp_health_request.serial + 1;
        if (serial == 0) {
            serial = 1;
        }
        const protocol::PvpTargetHealthRequest request{player, serial};
        if (send_packet(protocol::frame_pvp_target_health_request(request))) {
            m_pvp_health_request = request;
        }
    }

    bool NetClient::send_packet(const std::vector<std::uint8_t>& packet) {
        if (!m_session || packet.empty()) {
            return false;
        }
        if (m_read_only) {
            std::size_t consumed = 0;
            std::uint8_t type = 0;
            std::vector<std::uint8_t> payload;
            if (protocol::FrameParser::pop(packet.data(), packet.size(), consumed, type, payload) !=
                    protocol::FrameParseStatus::Complete ||
                consumed != packet.size()) {
                return false;
            }
            std::uint8_t loaded = 0;
            int height = 0;
            const bool teleport_ack = type == static_cast<std::uint8_t>(protocol::C2S::Teleport) &&
                                      protocol::Teleport::parse_client_ack(
                                          payload.data(), payload.size(), loaded, height);
            if (type != static_cast<std::uint8_t>(protocol::C2S::Connect) &&
                type != static_cast<std::uint8_t>(protocol::C2S::AnsCode) && !teleport_ack) {
                return false;
            }
        }
        if (m_trade.locked()) {
            std::size_t consumed = 0;
            std::uint8_t type = 0;
            std::vector<std::uint8_t> payload;
            if (protocol::FrameParser::pop(packet.data(), packet.size(), consumed, type, payload) !=
                    protocol::FrameParseStatus::Complete ||
                consumed != packet.size() || !protocol::is_trade_packet(type)) {
                return false;
            }
        }
        return m_session->send_all(packet);
    }

    bool NetClient::send_inventory_sort(bool quest) {
        if (!m_handshook || m_inventory_sort_pending >= 0 || m_inventory_sort_ready >= 0) {
            return false;
        }
        protocol::PacketWriter request;
        request.write_u8(quest ? 1 : 0);
        if (!send_packet(request.frame(static_cast<std::uint8_t>(protocol::C2S::SortInventory)))) {
            return false;
        }
        m_inventory_sort_pending = quest ? 1 : 0;
        return true;
    }

    bool NetClient::take_inventory_sort(bool& quest) {
        if (m_inventory_sort_ready < 0) {
            return false;
        }
        quest = m_inventory_sort_ready != 0;
        m_inventory_sort_ready = -1;
        return true;
    }

    bool NetClient::send_trading_post(const protocol::TradingPostRequest& request) {
        return m_handshook && send_packet(protocol::frame_trading_post_request(request));
    }

    bool NetClient::send_trade(const protocol::TradeRequest& request) {
        return m_handshook && send_packet(protocol::frame_trade_request(request));
    }

    bool NetClient::send_party(const protocol::PartyRequest& request) {
        return m_session && m_handshook && send_packet(protocol::frame_party_request(request));
    }

    bool NetClient::send_chat(std::string_view text) {
        if (!m_session || !m_handshook || text.empty()) {
            return false;
        }
        return send_packet(protocol::Chatting::frame_c2s(text));
    }

    bool NetClient::take_chat(PendingChat& out) {
        if (m_pending_chats.empty()) {
            return false;
        }
        out = std::move(m_pending_chats.front());
        m_pending_chats.pop_front();
        return true;
    }

    bool NetClient::take_map_state(PendingMapState& out) {
        if (m_pending_map_states.empty()) {
            return false;
        }
        out = m_pending_map_states.front();
        m_pending_map_states.pop_front();
        return true;
    }

    bool NetClient::take_party_item(protocol::PartyItemNotice& out) {
        if (m_pending_party_items.empty()) {
            return false;
        }
        out = std::move(m_pending_party_items.front());
        m_pending_party_items.pop_front();
        return true;
    }

    void NetClient::queue_item_received(int index, int count) {
        std::string name = "item #" + std::to_string(index);
        if (m_item_catalog) {
            if (const auto* definition = m_item_catalog->find(index)) {
                if (!definition->name().empty()) {
                    name = definition->name();
                }
            }
        }
        std::string text = "You got ";
        if (count > 1) {
            text += std::to_string(count) + "x ";
        }
        text += name;
        m_pending_infos.push_back({std::move(text),
                                   protocol::BattleMsgKind::ItemGet,
                                   std::array<float, 3>{0.40f, 0.95f, 0.48f}});
    }

    void NetClient::queue_money_received(std::int64_t geons) {
        if (geons > 0) {
            m_pending_infos.push_back({"You got " + std::to_string(geons) + " geons",
                                       protocol::BattleMsgKind::MoneyGet,
                                       std::array<float, 3>{1.0f, 1.0f, 1.0f}});
        }
    }

    bool NetClient::take_info_message(PendingInfoMessage& out) {
        if (m_pending_infos.empty()) {
            return false;
        }
        out = std::move(m_pending_infos.front());
        m_pending_infos.pop_front();
        return true;
    }

    bool NetClient::send_teleport_request(std::uint8_t loc_index) {
        if (!m_session || !m_handshook) {
            return false;
        }
        return send_packet(protocol::Teleport::frame_client_request(loc_index));
    }

    bool NetClient::take_teleport(int& map, int& sx, int& sy, int& sz) {
        if (!m_have_teleport) {
            return false;
        }
        map = m_teleport_map;
        sx = m_teleport_sx;
        sy = m_teleport_sy;
        sz = m_teleport_sz;
        m_have_teleport = false;
        return true;
    }

    bool NetClient::send_teleport_ack(int sx, int sy, int sz) {
        if (!m_session || !m_handshook) {
            return false;
        }
        // Seed move baseline to the teleported server pose so the next MoveOn is
        // small.
        m_server_px = sx;
        m_server_py = sy;
        m_server_pz = sz;
        m_have_move_baseline = true;
        m_was_moving = false;
        m_move_sync_elapsed = 0.0f;
        return send_packet(protocol::Teleport::frame_client_ack(1, sz));
    }

    void NetClient::seed_move_baseline(int sx, int sy, int sz) {
        m_server_px = sx;
        m_server_py = sy;
        m_server_pz = sz;
        m_have_move_baseline = true;
        m_was_moving = false;
        m_move_sync_elapsed = 0.0f;
    }

    bool NetClient::take_create_player(protocol::CreatePlayerFields& out) {
        if (!m_have_create_player) {
            return false;
        }
        out = m_create_player;
        m_have_create_player = false;
        return true;
    }

    bool NetClient::take_create_other_player(protocol::CreatePlayerFields& out) {
        if (m_pending_other_players.empty()) {
            return false;
        }
        out = m_pending_other_players.front();
        m_pending_other_players.erase(m_pending_other_players.begin());
        return true;
    }

    bool NetClient::take_remove_other_player(std::uint32_t& out_id) {
        if (m_pending_remove_others.empty()) {
            return false;
        }
        out_id = m_pending_remove_others.front();
        m_pending_remove_others.erase(m_pending_remove_others.begin());
        return true;
    }

    bool NetClient::send_move_towards(int sx, int sy, int sz, bool end) {
        const int x = sx - m_server_px;
        const int y = sy - m_server_py;
        const int z = sz - m_server_pz;
        const int chunks = std::max(
            {1, (std::abs(x) + 63) / 64, (std::abs(y) + 63) / 64, (std::abs(z) + 126) / 127});
        const auto dx = static_cast<std::int8_t>(x / chunks);
        const auto dy = static_cast<std::int8_t>(y / chunks);
        const auto dz = static_cast<std::int8_t>(z / chunks);
        const bool drained = chunks == 1;
        const auto packet = end && drained ? protocol::Move::frame_client_end(dx, dy, dz)
                                           : protocol::Move::frame_client_on(dx, dy, dz);
        if (!send_packet(packet)) {
            return false;
        }
        m_server_px += dx;
        m_server_py += dy;
        m_server_pz += dz;
        m_was_moving = !end || !drained;
        return true;
    }

    void NetClient::flush_local_move(float wx, float wy, float wz, int origin_patch) {
        if (!m_session || !m_handshook) {
            return;
        }
        m_local_wx = wx;
        m_local_wz = wz;
        const int sx = coords::to_server(wx, origin_patch);
        const int sy = coords::to_server(wz, origin_patch);
        const int sz = coords::to_server_z(wy);
        if (!m_have_move_baseline) {
            seed_move_baseline(sx, sy, sz);
            return;
        }
        for (int guard = 0; guard < 64; ++guard) {
            if (sx == m_server_px && sy == m_server_py && sz == m_server_pz && !m_was_moving) {
                break;
            }
            if (!send_move_towards(sx, sy, sz, true)) {
                break;
            }
        }
        m_move_sync_elapsed = 0.0f;
    }

    void NetClient::sync_local_move(
        float wx, float wy, float wz, bool moving, float dt, int origin_patch, bool hold_position) {
        if (!m_session || !m_handshook) {
            return;
        }
        m_local_wx = wx;
        m_local_wz = wz;
        if (m_read_only) {
            return;
        }
        // Idle height corrections also cancel chest opening on the server.
        if (hold_position) {
            return;
        }
        const int sx = coords::to_server(wx, origin_patch);
        const int sy = coords::to_server(wz, origin_patch);
        const int sz = coords::to_server_z(wy);
        if (!m_have_move_baseline) {
            seed_move_baseline(sx, sy, sz);
            return;
        }
        m_move_sync_elapsed += std::clamp(dt, 0.0f, 0.1f);
        if (moving && m_move_sync_elapsed < 1.0f / 20.0f) {
            return;
        }
        if (moving || m_was_moving || sx != m_server_px || sy != m_server_py || sz != m_server_pz) {
            (void) send_move_towards(sx, sy, sz, !moving);
            m_move_sync_elapsed = 0.0f;
        }
    }

    void NetClient::close() {
        if (m_session && m_handshook && m_session->alive() && !m_trade.locked()) {
            m_game_exit_answered = false;
            m_game_exit_saved = false;
            if (send_packet(protocol::GameExit::frame_client_request())) {
                constexpr int kExitWaitAttempts = 200;
                for (int attempt = 0; attempt < kExitWaitAttempts && !m_game_exit_answered;
                     ++attempt) {
                    pump_rx();
                    process_frames(/*origin_patch=*/32);
                    if (!m_session || !m_session->alive()) {
                        break;
                    }
                    if (!m_game_exit_answered) {
                        timespec ts{0, 5 * 1000 * 1000};
                        nanosleep(&ts, nullptr);
                    }
                }
                if (!m_game_exit_answered) {
                    log::warn("NetClient: game-exit persistence acknowledgement timed out");
                } else if (!m_game_exit_saved) {
                    log::error("NetClient: server rejected game-exit persistence");
                }
            } else {
                log::warn("NetClient: failed to send C2S_GAMEEXIT");
            }
        }
        if (m_session) {
            m_session->close();
            m_session.reset();
        }
        clear_online_views();
        m_pending_storage_balances.clear();
        m_monsters.clear();
        m_npcs.clear();
        m_pending_html.clear();
        m_pending_assassin_list.reset();
        m_last_asked_npc_id = 0;
        m_quest_flags.clear();
        m_quest_flags_dirty = false;
        m_quest_state_changed = false;
        m_quest_accepted = false;
        m_ground_items.clear();
        m_pending_inserts.clear();
        m_pending_item_wear.clear();
        m_pending_riding_states.clear();
        m_pending_riding_cache_results.clear();
        m_pending_item_info.clear();
        m_mail.clear();
        m_talent_state = {};
        m_talent_dirty = false;
        m_mail_dirty = false;
        m_pending_mail_claims.clear();
        m_handshook = false;
        m_got_code = false;
        m_game_exit_answered = false;
        m_game_exit_saved = false;
        m_have_move_baseline = false;
        m_was_moving = false;
        m_move_sync_elapsed = 0.0f;
        m_local_player_id = 0;
        m_expect_skill_index = 0;
        m_expect_skill_target = 0;
        m_have_create_player = false;
        m_pending_other_players.clear();
        m_remote_player_positions.clear();
        m_remote_player_classes.clear();
        m_pending_remove_others.clear();
        m_have_move_before = false;
        m_have_teleport = false;
        m_entity_resting.clear();
        m_entity_mstate.clear();
        m_buff_slots.clear();
        m_pending_charm_fx.clear();
        m_pending_damaged_fx.clear();
        m_pending_level_up_fx.clear();
        m_pending_player_attacks.clear();
        m_pending_skill_actions.clear();
        m_pending_emotes.clear();
        m_pending_hit_fx.clear();
    }

    void NetClient::clear_online_views() {
        m_trading_post = {};
        m_inventory_sort_pending = m_inventory_sort_ready = -1;
        m_trading_post_collection_pending = false;
        ++m_trading_post_serial;
        m_trade = {};
        ++m_trade_serial;
        m_party = {};
        m_pending_guild_views.clear();
        m_pending_player_options.clear();
        m_guild_view = {};
        m_guild_displays.clear();
        m_pvp_status.clear();
        m_pvp_health_supported = false;
        m_pvp_health_request = {};
        m_pvp_target_health.reset();
        m_pending_duel_defeats.clear();
        m_char_guild.clear();
        m_char_guild_pos.clear();
        ++m_guild_serial;
        m_pending_party_items.clear();
        m_exp_event_state.reset();
        m_honor_status.reset();
        m_server_clock.reset();
    }

    void NetClient::pump_rx() {
        if (!m_session || !m_session->recv_into(m_rx)) {
            clear_online_views();
            if (m_session) {
                m_session->close();
            }
        }
    }

    void NetClient::face_monster_to_target(std::uint32_t attacker,
                                           std::uint32_t target,
                                           int origin_patch) {
        const auto attacker_it = m_monsters.find(attacker);
        if (attacker_it == m_monsters.end()) {
            return;
        }
        float target_wx = 0.0f;
        float target_wz = 0.0f;
        if (m_local_player_id != 0 && target == m_local_player_id) {
            target_wx = m_local_wx;
            target_wz = m_local_wz;
        } else if (const auto remote = m_remote_player_positions.find(target);
                   remote != m_remote_player_positions.end()) {
            target_wx = coords::to_client(remote->second.x, origin_patch);
            target_wz = coords::to_client(remote->second.y, origin_patch);
        } else {
            const auto target_it = m_monsters.find(target);
            if (target_it == m_monsters.end()) {
                return;
            }
            target_wx = target_it->second.wx();
            target_wz = target_it->second.wz();
        }
        const float dx = target_wx - attacker_it->second.wx();
        const float dz = target_wz - attacker_it->second.wz();
        if (dx * dx + dz * dz > 1e-4f) {
            attacker_it->second.set_yaw(std::atan2(-dx, -dz));
        }
    }

    bool NetClient::handle_online_packet(std::uint8_t type,
                                         const std::vector<std::uint8_t>& payload) {
        if (type == static_cast<std::uint8_t>(protocol::S2C::SortInventory)) {
            if (payload.size() == 2 && payload[0] <= 1 && payload[1] <= 1 &&
                m_inventory_sort_pending == payload[0]) {
                m_inventory_sort_ready = payload[1] ? payload[0] : -1;
                m_inventory_sort_pending = -1;
            }
            return true;
        }
        if (type == static_cast<std::uint8_t>(protocol::S2C::TradingPostPickup)) {
            protocol::TradingPostPickup pickup;
            if (protocol::parse_trading_post_pickup(payload.data(), payload.size(), pickup)) {
                if (pickup.item_index) {
                    queue_item_received(pickup.item_index, pickup.quantity);
                }
                queue_money_received(pickup.geons);
            } else if (m_session) {
                m_session->close();
            }
            return true;
        }
        if (type == static_cast<std::uint8_t>(protocol::S2C::TradingPostCollection)) {
            if (!protocol::parse_trading_post_collection(
                    payload.data(), payload.size(), m_trading_post_collection_pending) &&
                m_session) {
                m_session->close();
            }
            return true;
        }
        if (type == static_cast<std::uint8_t>(protocol::S2C::TradingPost)) {
            protocol::TradingPostState state;
            if (protocol::parse_trading_post_state(payload.data(), payload.size(), state)) {
                m_trading_post = std::move(state);
                ++m_trading_post_serial;
            } else if (m_session) {
                m_session->close();
            }
            return true;
        }
        if (type == static_cast<std::uint8_t>(protocol::S2C::OpenTrade)) {
            protocol::TradeState state;
            if (protocol::parse_trade_state(payload.data(), payload.size(), state)) {
                if (state.phase == protocol::TradePhase::Completed) {
                    for (const auto& item : state.other) {
                        queue_item_received(item.index, item.count);
                    }
                    queue_money_received(state.partner_money);
                }
                if (!state.locked() && !state.status.empty()) {
                    m_pending_infos.push_back({state.status, std::nullopt, std::nullopt});
                }
                m_trade = std::move(state);
                ++m_trade_serial;
            } else if (m_session) {
                m_session->close();
            }
            return true;
        }
        if (type == static_cast<std::uint8_t>(protocol::S2C::UpdateParty)) {
            protocol::PartyItemNotice notice;
            if (protocol::parse_party_item(payload.data(), payload.size(), notice) &&
                notice.party == m_party.id && notice.recipient != m_local_player_id) {
                m_pending_party_items.push_back(std::move(notice));
            }
            return true;
        }
        if (type == static_cast<std::uint8_t>(protocol::S2C::PvpStatus)) {
            protocol::PvpStatus status;
            if (protocol::parse_pvp_status(payload.data(), payload.size(), status)) {
                auto& previous = m_pvp_status[status.player_id];
                if (status.dueling && status.defeated && !(previous.dueling && previous.defeated)) {
                    m_pending_duel_defeats.insert(status.player_id);
                }
                previous = status;
                if (!status.attackable && status.player_id == m_pvp_health_request.player_id) {
                    m_pvp_target_health.reset();
                }
            }
            return true;
        }
        if (type == static_cast<std::uint8_t>(protocol::S2C::PvpTargetHealth)) {
            protocol::PvpTargetHealth health;
            if (protocol::parse_pvp_target_health(payload.data(), payload.size(), health)) {
                if (health.target.player_id == 0) {
                    m_pvp_health_supported = true;
                } else if (health.target.player_id == m_pvp_health_request.player_id &&
                           health.target.serial == m_pvp_health_request.serial) {
                    if (health.max_hp > 0 && pvp_can_attack(health.target.player_id)) {
                        m_pvp_target_health = health;
                    } else {
                        m_pvp_target_health.reset();
                    }
                }
            }
            return true;
        }
        if (type == static_cast<std::uint8_t>(protocol::S2C::Guild)) {
            protocol::GuildResponse response;
            if (protocol::parse_guild_response(payload.data(), payload.size(), response)) {
                if (response.kind == protocol::GuildMessage::Display) {
                    const auto player =
                        static_cast<std::uint32_t>(response.data.at("player").number());
                    if (player) {
                        m_guild_displays[player] = std::move(response.data);
                    }
                } else {
                    auto& request_id = response.data.object()["_request_id"];
                    request_id.set_type(platform::Json::Type::String);
                    request_id.string() = std::to_string(response.request_id);
                    m_char_guild = response.data.at("own").at("name").as_string();
                    m_char_guild_pos = response.data.at("own").at("title").as_string();
                    m_guild_view = response.data;
                    if (m_pending_guild_views.size() >= 128) {
                        m_pending_guild_views.pop_front();
                    }
                    m_pending_guild_views.push_back(std::move(response.data));
                    ++m_guild_serial;
                }
            }
            return true;
        }
        if (type == static_cast<std::uint8_t>(protocol::S2C::PartyInfo)) {
            protocol::PartyState state;
            if (protocol::parse_party_state(payload.data(), payload.size(), state)) {
                m_party = std::move(state);
            }
            return true;
        }
        if (type == static_cast<std::uint8_t>(protocol::S2C::PlayerOptions)) {
            protocol::PlayerOptionsState state;
            if (protocol::parse_player_options_state(payload.data(), payload.size(), state)) {
                if (m_pending_player_options.size() >= 32) {
                    m_pending_player_options.pop_front();
                }
                m_pending_player_options.push_back(std::move(state));
            }
            return true;
        }

        if (type == static_cast<std::uint8_t>(protocol::S2C::Chatting)) {
            PendingChat chat;
            if (!protocol::Chatting::parse_s2c(
                    payload.data(), payload.size(), chat.speaker, chat.text)) {
                return true;
            }
            m_pending_chats.push_back(std::move(chat));
            return true;
        }
        return false;
    }

    bool NetClient::handle_inventory_packet(std::uint8_t type,
                                            const std::vector<std::uint8_t>& payload) {
        if (type == static_cast<std::uint8_t>(protocol::S2C::Mail)) {
            if (!payload.empty() &&
                payload[0] == static_cast<std::uint8_t>(protocol::MailCommand::List)) {
                std::vector<protocol::MailEntry> entries;
                if (protocol::Mail::parse_list_s2c(payload.data(), payload.size(), entries)) {
                    m_mail = std::move(entries);
                    m_mail_dirty = true;
                } else {
                    log::warn("NetClient: S2C_MAIL inbox parse failed");
                }
            } else {
                protocol::MailClaimResult result;
                if (protocol::Mail::parse_claim_result_s2c(
                        payload.data(), payload.size(), result)) {
                    m_pending_mail_claims.push_back(result);
                } else {
                    log::warn("NetClient: S2C_MAIL claim parse failed");
                }
            }
            return true;
        }
        if (type == static_cast<std::uint8_t>(protocol::S2C::InsertItem)) {
            protocol::PacketReader r(payload);
            std::uint32_t iid = 0, info = 0, num = 0;
            std::uint16_t index = 0;
            std::uint8_t prefix = 0;
            if (!r.read_u32(iid) || !r.read_u16(index) || !r.read_u8(prefix) || !r.read_u32(info) ||
                !r.read_u32(num)) {
                return true;
            }
            PendingInsertItem ins;
            ins.iid = iid;
            ins.index = static_cast<int>(index);
            ins.prefix = static_cast<int>(prefix);
            ins.count = static_cast<int>(num);
            ins.info = info;
            if (r.remaining() >= 8) {
                std::uint8_t charm[protocol::kInsertCharmBytes];
                const std::size_t charm_size =
                    std::min<std::size_t>(r.remaining(), protocol::kInsertCharmBytes);
                if (r.read_bytes(charm, charm_size)) {
                    ins.charm = protocol::unpack_insert_charm(charm, charm_size);
                }
            }
            if (r.remaining() >= sizeof(std::uint64_t) && r.read_u64(ins.expire_at)) {
                ins.has_expire_at = true;
            }
            m_pending_inserts.push_back(ins);
            return true;
        }
        if (type == static_cast<std::uint8_t>(protocol::S2C::PutOnItem) ||
            type == static_cast<std::uint8_t>(protocol::S2C::PutOffItem)) {
            PendingItemWear wear;
            if (protocol::parse_s2c_item_wear(payload.data(),
                                              payload.size(),
                                              wear.player_id,
                                              wear.iid,
                                              wear.item_index,
                                              &wear.expire_at)) {
                wear.equipped = type == static_cast<std::uint8_t>(protocol::S2C::PutOnItem);
                m_pending_item_wear.push_back(wear);
            }
            return true;
        }
        if (type == static_cast<std::uint8_t>(protocol::S2C::Riding)) {
            protocol::RidingState state;
            if (protocol::Riding::parse_server_state(payload.data(), payload.size(), state)) {
                m_pending_riding_states.push_back(state);
            }
            return true;
        }
        if (type == static_cast<std::uint8_t>(protocol::S2C::RidingCache)) {
            protocol::RidingCacheResult result;
            if (protocol::Riding::parse_server_cache_result(
                    payload.data(), payload.size(), result)) {
                m_pending_riding_cache_results.push_back(result);
            }
            return true;
        }
        if (type == static_cast<std::uint8_t>(protocol::S2C::TrashItem)) {
            std::uint32_t iid = 0;
            std::uint8_t log_type = 0;
            if (protocol::TrashItem::parse_s2c(payload.data(), payload.size(), iid, log_type) &&
                iid != 0) {
                (void) log_type;
                m_pending_trash.push_back(iid);
            }
            return true;
        }
        if (type == static_cast<std::uint8_t>(protocol::S2C::ItemList)) {
            protocol::ShopItemList list;
            if (protocol::Shop::parse_item_list_s2c(payload.data(), payload.size(), list)) {
                m_pending_shop_lists.push_back(std::move(list));
            }
            return true;
        }
        if (type == static_cast<std::uint8_t>(protocol::S2C::StorageInfo)) {
            protocol::StorageInfo info;
            if (protocol::Storage::parse_info_s2c(payload.data(), payload.size(), info)) {
                m_pending_storage_info.push_back(std::move(info));
            }
            return true;
        }
        if (type == static_cast<std::uint8_t>(protocol::S2C::StorageMoney)) {
            protocol::StorageBalance balance;
            if (protocol::Storage::parse_money_s2c(payload.data(), payload.size(), balance)) {
                m_pending_storage_balances.push_back(balance);
            }
            return true;
        }
        if (type == static_cast<std::uint8_t>(protocol::S2C::UpdateItemInfo)) {
            protocol::UpdateItemInfoFields fields;
            if (!protocol::ItemInfo::parse_update(payload.data(), payload.size(), fields)) {
                return true;
            }
            m_transform_egg_index = static_cast<int>(fields.index);
            m_transform_egg_level = std::max(1, static_cast<int>(fields.level));
            m_transform_egg_exp = static_cast<int>(fields.exp);
            m_pending_item_info.push_back(fields);
            if (fields.added != 0) {
                PendingInfoMessage info;
                info.text = BattleMsgBuilder::property_delta("Animal EXP", fields.added);
                info.kind = protocol::BattleMsgKind::TransformExp;
                if (!info.text.empty()) {
                    m_pending_infos.push_back(std::move(info));
                }
            }
            return true;
        }
        return false;
    }

    bool NetClient::handle_quest_packet(std::uint8_t type,
                                        const std::vector<std::uint8_t>& payload) {
        if (type == static_cast<std::uint8_t>(protocol::S2C::AssassinList)) {
            protocol::AssassinList list;
            if (protocol::parse_assassin_list(payload.data(), payload.size(), list)) {
                m_pending_assassin_list = std::move(list);
            }
            return true;
        }
        if (type == static_cast<std::uint8_t>(protocol::S2C::SendHtml)) {
            std::int32_t html_id = 0;
            std::uint32_t npc_id = 0;
            if (protocol::Quest::parse_send_html(
                    payload.data(), payload.size(), html_id, &npc_id)) {
                m_pending_html.emplace_back(html_id, npc_id != 0 ? npc_id : m_last_asked_npc_id);
            }
            return true;
        }
        if (type == static_cast<std::uint8_t>(protocol::S2C::SaveQuest)) {
            protocol::QuestFlagEntry entry;
            if (protocol::Quest::parse_save_quest(payload.data(), payload.size(), entry)) {
                bool found = false;
                bool was_active = false;
                for (auto& q : m_quest_flags) {
                    if (q.quest == entry.quest) {
                        was_active = q.flag != 0 && q.clear == 0;
                        q = entry;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    m_quest_flags.push_back(entry);
                }
                if (!was_active && entry.flag != 0 && entry.clear == 0) {
                    m_quest_accepted = true;
                }
                m_quest_flags_dirty = true;
                m_quest_state_changed = true;
            }
            return true;
        }
        if (type == static_cast<std::uint8_t>(protocol::S2C::QuestInfo)) {
            std::vector<protocol::QuestFlagEntry> entries;
            if (protocol::Quest::parse_quest_info(payload.data(), payload.size(), entries)) {
                m_quest_flags = std::move(entries);
                m_quest_flags_dirty = true;
            }
            return true;
        }
        if (type == static_cast<std::uint8_t>(protocol::S2C::SkillInfo)) {
            std::vector<protocol::SkillRankEntry> entries;
            if (protocol::Skill::parse_skill_info(payload.data(), payload.size(), entries)) {
                m_skill_info.clear();
                for (const auto& e : entries) {
                    m_skill_info[static_cast<int>(e.index)] = static_cast<int>(e.level);
                }
                m_have_skill_info = true;
            }
            return true;
        }
        if (type == static_cast<std::uint8_t>(protocol::S2C::SkillUp)) {
            std::uint8_t index = 0;
            std::uint8_t level = 0;
            if (protocol::Skill::parse_skill_up(payload.data(), payload.size(), index, level)) {
                m_pending_skill_ups.emplace_back(static_cast<int>(index), static_cast<int>(level));
            }
            return true;
        }
        if (type == static_cast<std::uint8_t>(protocol::S2C::Shortcut)) {
            bool present = false;
            protocol::ShortcutBar bar;
            if (protocol::Shortcut::parse_server(payload.data(), payload.size(), present, bar)) {
                m_shortcut_present = present;
                m_shortcut = bar;
                m_have_shortcut = true;
            }
            return true;
        }
        return false;
    }

    bool NetClient::handle_snapshot_packet(std::uint8_t type,
                                           const std::vector<std::uint8_t>& payload) {
        if (type == static_cast<std::uint8_t>(protocol::S2C::Code)) {
            protocol::S2CCodeFields fields;
            if (protocol::Handshake::parse_code(payload.data(), payload.size(), fields)) {
                m_protocol_test = fields.protocol_test_value;
                m_got_code = true;
            } else {
                log::warn("NetClient: S2C_CODE parse failed (payload=" +
                          std::to_string(payload.size()) + ")");
            }
            return true;
        }

        if (type == static_cast<std::uint8_t>(protocol::S2C::AnsGameExit)) {
            bool saved = false;
            if (protocol::GameExit::parse_server_answer(payload.data(), payload.size(), saved)) {
                m_game_exit_saved = saved;
                m_game_exit_answered = true;
            } else {
                log::warn("NetClient: S2C_ANS_GAMEEXIT parse failed");
            }
            return true;
        }

        if (type == static_cast<std::uint8_t>(protocol::S2C::ClockSync)) {
            protocol::ClockSyncFields snapshot;
            if (protocol::ClockSync::parse(payload.data(), payload.size(), snapshot)) {
                m_server_clock.synchronize(std::move(snapshot));
            } else if (m_session) {
                m_session->close();
            }
            return true;
        }

        if (type == static_cast<std::uint8_t>(protocol::S2C::HonorStatus)) {
            protocol::HonorStatus status;
            if (protocol::parse_honor_status(payload.data(), payload.size(), status)) {
                m_honor_status = status;
            }
            return true;
        }

        if (type == static_cast<std::uint8_t>(protocol::S2C::ExpEventState)) {
            protocol::ExpEventStateFields state;
            if (protocol::ExpEventState::parse(payload.data(), payload.size(), state)) {
                m_exp_event_state = state;
            }
            return true;
        }

        if (type == static_cast<std::uint8_t>(protocol::S2C::MapState)) {
            protocol::MapStateFields fields;
            if (!protocol::MapState::parse(payload.data(), payload.size(), fields)) {
                return true;
            }
            PendingMapState state;
            state.map = fields.map;
            state.phase = fields.phase;
            state.active_mask = fields.active_mask;
            state.timer = fields.timer;
            state.timer_seconds = fields.timer_seconds;
            m_pending_map_states.push_back(state);
            return true;
        }

        if (type == static_cast<std::uint8_t>(protocol::S2C::UpdateProperty)) {
            protocol::UpdatePropertyFields fields;
            if (!protocol::Property::parse_update_any(payload.data(), payload.size(), fields)) {
                return true;
            }
            if (fields.kind == protocol::UpdatePropertyKind::Exp) {
                m_exp_current = fields.exp_current;
                m_exp_total = std::max<std::int64_t>(1, protocol::need_exp_for_level(m_level));
                m_have_exp = true;
                if (fields.exp_delta != 0) {
                    PendingInfoMessage info;
                    info.text = BattleMsgBuilder::property_delta("Exp", fields.exp_delta);
                    info.kind = protocol::BattleMsgKind::Exp;
                    if (!info.text.empty()) {
                        m_pending_infos.push_back(std::move(info));
                    }
                }
            } else if (fields.kind == protocol::UpdatePropertyKind::Level) {
                m_level = std::max(1, static_cast<int>(fields.level_value));
                m_exp_total = std::max<std::int64_t>(1, protocol::need_exp_for_level(m_level));
                m_have_level = true;
                m_have_exp = true;
            } else if (fields.kind == protocol::UpdatePropertyKind::Vital) {
                if (fields.property == static_cast<std::uint8_t>(protocol::PropertyType::Hp)) {
                    if (m_chat_prev_hp >= 0) {
                        const std::int64_t delta = static_cast<std::int64_t>(fields.vital_cur) -
                                                   static_cast<std::int64_t>(m_chat_prev_hp);
                        if (delta > 0) {
                            const std::string line =
                                BattleMsgBuilder::property_delta("Health Point", delta);
                            if (!line.empty()) {
                                PendingInfoMessage info;
                                info.text = line;
                                info.kind = protocol::BattleMsgKind::Hp;
                                m_pending_infos.push_back(std::move(info));
                            }
                            enqueue_combat_float(
                                CombatFloatKind::Heal, std::to_string(static_cast<int>(delta)), 0);
                        }
                    }
                    m_cur_hp = fields.vital_cur;
                    m_max_hp_prop = fields.vital_max;
                    m_chat_prev_hp = m_cur_hp;
                    m_have_hp = true;
                    m_have_max_hp = true;
                } else if (fields.property ==
                           static_cast<std::uint8_t>(protocol::PropertyType::Mp)) {
                    if (m_chat_prev_mp >= 0) {
                        const std::int64_t delta = static_cast<std::int64_t>(fields.vital_cur) -
                                                   static_cast<std::int64_t>(m_chat_prev_mp);
                        const std::string line =
                            BattleMsgBuilder::property_delta("Magic Point", delta);
                        if (!line.empty()) {
                            PendingInfoMessage info;
                            info.text = line;
                            info.kind = protocol::BattleMsgKind::Mp;
                            m_pending_infos.push_back(std::move(info));
                        }
                    }
                    m_cur_mp = fields.vital_cur;
                    m_max_mp_prop = fields.vital_max;
                    m_chat_prev_mp = m_cur_mp;
                    m_have_mp = true;
                    m_have_max_mp = true;
                }
            } else if (fields.property ==
                       static_cast<std::uint8_t>(protocol::PropertyType::CurHp)) {
                if (m_chat_prev_hp >= 0) {
                    const std::int64_t delta = static_cast<std::int64_t>(fields.word_value) -
                                               static_cast<std::int64_t>(m_chat_prev_hp);
                    if (delta > 0) {
                        const std::string line =
                            BattleMsgBuilder::property_delta("Health Point", delta);
                        if (!line.empty()) {
                            PendingInfoMessage info;
                            info.text = line;
                            info.kind = protocol::BattleMsgKind::Hp;
                            m_pending_infos.push_back(std::move(info));
                        }
                        enqueue_combat_float(
                            CombatFloatKind::Heal, std::to_string(static_cast<int>(delta)), 0);
                    }
                }
                m_cur_hp = fields.word_value;
                m_chat_prev_hp = m_cur_hp;
                m_have_hp = true;
            } else if (fields.property ==
                       static_cast<std::uint8_t>(protocol::PropertyType::CurMp)) {
                if (m_chat_prev_mp >= 0) {
                    const std::int64_t delta = static_cast<std::int64_t>(fields.word_value) -
                                               static_cast<std::int64_t>(m_chat_prev_mp);
                    const std::string line = BattleMsgBuilder::property_delta("Magic Point", delta);
                    if (!line.empty()) {
                        PendingInfoMessage info;
                        info.text = line;
                        info.kind = protocol::BattleMsgKind::Mp;
                        m_pending_infos.push_back(std::move(info));
                    }
                }
                m_cur_mp = fields.word_value;
                m_chat_prev_mp = m_cur_mp;
                m_have_mp = true;
            } else if (fields.property ==
                       static_cast<std::uint8_t>(protocol::PropertyType::PuPoint)) {
                m_pu_point = fields.word_value;
                m_have_points = true;
            } else if (fields.property ==
                       static_cast<std::uint8_t>(protocol::PropertyType::Contribute)) {
                m_char_ctb = fields.word_value;
            } else if (fields.property ==
                       static_cast<std::uint8_t>(protocol::PropertyType::SuPoint)) {
                m_su_point = fields.word_value;
                m_have_points = true;
            } else if (fields.property == static_cast<std::uint8_t>(protocol::PropertyType::Str)) {
                m_str = fields.word_value;
            } else if (fields.property == static_cast<std::uint8_t>(protocol::PropertyType::Hth)) {
                m_hth = fields.word_value;
            } else if (fields.property == static_cast<std::uint8_t>(protocol::PropertyType::Int)) {
                m_intl = fields.word_value;
            } else if (fields.property == static_cast<std::uint8_t>(protocol::PropertyType::Wis)) {
                m_wis = fields.word_value;
            } else if (fields.property == static_cast<std::uint8_t>(protocol::PropertyType::Dex)) {
                m_dex = fields.word_value;
            } else if (fields.property == static_cast<std::uint8_t>(protocol::PropertyType::Hit)) {
                m_hit = fields.word_value;
            } else if (fields.property ==
                       static_cast<std::uint8_t>(protocol::PropertyType::Dodge)) {
                m_dodge = fields.word_value;
            } else if (fields.property ==
                       static_cast<std::uint8_t>(protocol::PropertyType::MinAttack)) {
                m_min_attack = fields.word_value;
            } else if (fields.property ==
                       static_cast<std::uint8_t>(protocol::PropertyType::MaxAttack)) {
                m_max_attack = fields.word_value;
            } else if (fields.property ==
                       static_cast<std::uint8_t>(protocol::PropertyType::MinMagic)) {
                m_min_magic = fields.word_value;
            } else if (fields.property ==
                       static_cast<std::uint8_t>(protocol::PropertyType::MaxMagic)) {
                m_max_magic = fields.word_value;
            } else if (fields.property ==
                       static_cast<std::uint8_t>(protocol::PropertyType::Defense)) {
                m_defense = fields.word_value;
            } else if (fields.property ==
                       static_cast<std::uint8_t>(protocol::PropertyType::Absorb)) {
                m_absorb = fields.word_value;
            } else if (fields.property ==
                       static_cast<std::uint8_t>(protocol::PropertyType::ResFire)) {
                m_res_fire = fields.word_value;
            } else if (fields.property ==
                       static_cast<std::uint8_t>(protocol::PropertyType::ResIce)) {
                m_res_ice = fields.word_value;
            } else if (fields.property ==
                       static_cast<std::uint8_t>(protocol::PropertyType::ResLitning)) {
                m_res_lightning = fields.word_value;
            } else if (fields.property ==
                       static_cast<std::uint8_t>(protocol::PropertyType::ResCurse)) {
                m_res_curse = fields.word_value;
            } else if (fields.property ==
                       static_cast<std::uint8_t>(protocol::PropertyType::ResPalsy)) {
                m_res_palsy = fields.word_value;
            } else if (fields.property == protocol::kPropertyRage) {
                m_rage = fields.word_value;
            } else if (fields.property == protocol::kPropertyMortal) {
                m_mortal = fields.word_value;
            }
            return true;
        }

        return false;
    }

    bool NetClient::handle_world_packet(std::uint8_t type,
                                        const std::vector<std::uint8_t>& payload,
                                        int origin_patch) {
        if (type == static_cast<std::uint8_t>(protocol::S2C::CreatePlayer)) {
            protocol::CreatePlayerFields fields;
            if (protocol::CreatePlayer::parse(payload.data(), payload.size(), fields)) {
                m_player_gstate[fields.id] = fields.gstate;
                if ((fields.class_byte & 0x80) != 0) {
                    m_create_player = fields;
                    seed_move_baseline(fields.x, fields.y, fields.z);
                    if (m_read_only) {
                        m_observed_motion.reset({coords::to_client(fields.x, origin_patch),
                                                 coords::to_client_y(fields.z),
                                                 coords::to_client(fields.y, origin_patch)});
                    }
                    m_have_create_player = true;
                    m_local_player_id = fields.id;
                } else {
                    m_pending_remove_others.erase(std::remove(m_pending_remove_others.begin(),
                                                              m_pending_remove_others.end(),
                                                              fields.id),
                                                  m_pending_remove_others.end());
                    m_pending_other_players.push_back(fields);
                    m_remote_player_positions[fields.id] = {fields.x, fields.y, false, fields.z};
                    m_remote_player_classes[fields.id] = fields.class_byte & 0x7f;
                }
                log::info("NetClient: CreatePlayer id=" + std::to_string(fields.id) +
                          " name=" + fields.name +
                          ((fields.class_byte & 0x80) != 0 ? " (local)" : " (other)"));
            }
            return true;
        }

        if (type == static_cast<std::uint8_t>(protocol::S2C::RemovePlayer)) {
            std::uint32_t id = 0;
            if (protocol::CreatePlayer::parse_remove(payload.data(), payload.size(), id)) {
                m_guild_displays.erase(id);
                m_pvp_status.erase(id);
                if (id == m_pvp_health_request.player_id) {
                    m_pvp_target_health.reset();
                }
                m_pending_duel_defeats.erase(id);
                m_pending_remove_others.push_back(id);
                m_pending_other_players.erase(
                    std::remove_if(m_pending_other_players.begin(),
                                   m_pending_other_players.end(),
                                   [id](const auto& fields) { return fields.id == id; }),
                    m_pending_other_players.end());
                m_remote_player_positions.erase(id);
                m_remote_player_classes.erase(id);
                m_player_gstate.erase(id);
            }
            return true;
        }

        if (type == static_cast<std::uint8_t>(protocol::S2C::GState)) {
            protocol::PacketReader r(payload);
            std::uint32_t id = 0;
            std::uint32_t gstate = 0;
            if (!r.read_u32(id) || !r.read_u32(gstate)) {
                return true;
            }
            auto mit = m_monsters.find(id);
            if (mit != m_monsters.end()) {
                mit->second.set_gstate(gstate);
                if ((gstate & protocol::kCgsKnee) != 0) {
                    mit->second.set_hp(0);
                    mit->second.set_pending_knee(true);
                    mit->second.set_moving(false);
                }
                return true;
            }
            auto nit = m_npcs.find(id);
            if (nit != m_npcs.end()) {
                nit->second.set_gstate(gstate);
                return true;
            }
            m_player_gstate[id] = gstate;
            if (id == m_local_player_id) {
                m_create_player.gstate = gstate;
                if ((gstate & protocol::kCgsFish) != 0) {
                    m_fishing_progress_start = true;
                }
            }
            return true;
        }

        if (type == static_cast<std::uint8_t>(protocol::S2C::CreateMonster)) {
            protocol::PacketReader r(payload);
            std::uint16_t index = 0;
            std::uint32_t id = 0, x = 0, y = 0, gstate = 0, mstate = 0, gid = 0;
            std::int32_t hp = 0, max_hp = 0;
            std::uint16_t dir = 0;
            std::uint64_t guild_i = 0;
            std::string guild, name;
            std::uint8_t race = 0;
            if (!r.read_u16(index) || !r.read_u32(id) || !r.read_u32(x) || !r.read_u32(y) ||
                !r.read_u16(dir) || !r.read_i32(hp) || !r.read_i32(max_hp) || !r.read_u32(gstate) ||
                !r.read_u32(mstate) || !r.read_u64(guild_i) || !r.read_string(guild) ||
                !r.read_u8(race) || !r.read_u32(gid) || !r.read_string(name)) {
                return true;
            }
            (void) mstate;
            (void) guild_i;
            (void) guild;
            (void) gid;

            const float abs_wx = coords::to_client(static_cast<int>(x), origin_patch);
            const float abs_wz = coords::to_client(static_cast<int>(y), origin_patch);
            auto existing = m_monsters.find(id);
            if (existing != m_monsters.end()) {
                // Snapshot refresh (sight / rare correction). Do not yank a walking
                // mob.
                auto& m = existing->second;
                m.set_x(static_cast<int>(x));
                m.set_y(static_cast<int>(y));
                m.set_dir(dir);
                m.set_hp(hp);
                m.set_max_hp(max_hp);
                m.set_gstate(gstate);
                if (race == static_cast<std::uint8_t>(protocol::MonsterRace::CastleStandard)) {
                    m.set_name(name.empty() ? ("Mob" + std::to_string(index)) : name);
                }
                if ((gstate & protocol::kCgsKnee) != 0) {
                    m.set_hp(0);
                    m.set_pending_knee(true);
                    m.set_moving(false);
                }
                if (!m.moving()) {
                    const float ddx = abs_wx - m.wx();
                    const float ddz = abs_wz - m.wz();
                    if (ddx * ddx + ddz * ddz > 128.0f * 128.0f) {
                        m.set_tx(abs_wx);
                        m.set_tz(abs_wz);
                        m.set_has_abs_target(true);
                        m.set_stop_on_arrive(true);
                        m.set_moving(true);
                    } else {
                        m.set_wx(abs_wx);
                        m.set_wz(abs_wz);
                        m.set_tx(abs_wx);
                        m.set_tz(abs_wz);
                        m.set_has_abs_target(false);
                        m.set_stop_on_arrive(false);
                    }
                }
                if (dir != 0) {
                    m.set_yaw(static_cast<float>(dir) * (3.14159265f / 128.0f));
                }
                return true;
            }

            NetMonster m;
            m.set_id(id);
            m.set_index(index);
            m.set_race(race);
            m.set_x(static_cast<int>(x));
            m.set_y(static_cast<int>(y));
            m.set_dir(dir);
            m.set_hp(hp);
            m.set_max_hp(max_hp);
            m.set_gstate(gstate);
            m.set_name(name.empty() ? ("Mob" + std::to_string(index)) : name);
            m.set_wx(abs_wx);
            m.set_wz(abs_wz);
            m.set_tx(abs_wx);
            m.set_tz(abs_wz);
            m.set_moving(false);
            m.set_has_abs_target(false);
            m.set_stop_on_arrive(false);
            m.set_move_speed(20.0f);
            if (dir != 0) {
                m.set_yaw(static_cast<float>(dir) * (3.14159265f / 128.0f));
            }
            if ((gstate & protocol::kCgsKnee) != 0) {
                m.set_hp(0);
                m.set_pending_knee(true);
                m.set_moving(false);
            }
            m_monsters[id] = m;
            return true;
        }

        if (type == static_cast<std::uint8_t>(protocol::S2C::CreateItem)) {
            protocol::PacketReader r(payload);
            std::uint16_t index = 0;
            std::uint32_t id = 0, x = 0, y = 0, num = 0;
            if (!r.read_u16(index) || !r.read_u32(id) || !r.read_u32(x) || !r.read_u32(y) ||
                !r.read_u32(num)) {
                return true;
            }
            NetGroundItem g;
            g.id = id;
            g.index = static_cast<int>(index);
            g.count = static_cast<int>(num);
            g.wx = coords::to_client(static_cast<int>(x), origin_patch);
            g.wz = coords::to_client(static_cast<int>(y), origin_patch);
            g.age_sec = 0.0f;
            assign_drop_origin(g, m_monsters, m_local_wx, m_local_wz);
            m_ground_items[id] = g;
            return true;
        }

        if (type == static_cast<std::uint8_t>(protocol::S2C::RemoveItem)) {
            protocol::PacketReader r(payload);
            std::uint32_t id = 0;
            if (r.read_u32(id)) {
                m_ground_items.erase(id);
            }
            return true;
        }

        if (type == static_cast<std::uint8_t>(protocol::S2C::CreateNpc)) {
            protocol::CreateNpcInfo info;
            if (!protocol::Quest::parse_create_npc(payload.data(), payload.size(), info)) {
                return true;
            }
            NetNpc npc;
            npc.set_id(info.id);
            npc.set_index(static_cast<int>(info.index));
            npc.set_shape(static_cast<int>(info.shape));
            npc.set_x(info.x);
            npc.set_y(info.y);
            npc.set_z(info.z);
            npc.set_wx(coords::to_client(info.x, origin_patch));
            npc.set_wz(coords::to_client(info.y, origin_patch));
            npc.set_wy(coords::to_client_y(info.z));
            npc.set_gstate(info.gstate);
            m_npcs[info.id] = std::move(npc);
            return true;
        }

        if (type == static_cast<std::uint8_t>(protocol::S2C::RemoveNpc)) {
            std::uint32_t id = 0;
            if (protocol::Quest::parse_remove_npc(payload.data(), payload.size(), id)) {
                m_npcs.erase(id);
            }
            return true;
        }

        if (type == static_cast<std::uint8_t>(protocol::S2C::MoveMonsterOn) ||
            type == static_cast<std::uint8_t>(protocol::S2C::MoveMonsterEnd)) {
            protocol::PacketReader r(payload);
            std::uint32_t id = 0;
            std::uint8_t dx = 0, dy = 0, run = 0;
            if (!r.read_u32(id) || !r.read_u8(dx) || !r.read_u8(dy) || !r.read_u8(run)) {
                return true;
            }
            (void) run;
            auto it = m_monsters.find(id);
            if (it == m_monsters.end()) {
                return true;
            }
            auto& m = it->second;
            // Match KalOnline: accumulate pixel deltas into server pt, walk to abs
            // dest.
            const auto sdx = static_cast<std::int8_t>(dx);
            const auto sdy = static_cast<std::int8_t>(dy);
            m.set_x(m.x() + static_cast<int>(sdx));
            m.set_y(m.y() + static_cast<int>(sdy));
            if (m.pending_attack()) {
                // Attack clip already latched: do not restart interpolation under
                // the swing (a later chase MoveOn in the same buffer).
                m.set_wx(coords::to_client(m.x(), origin_patch));
                m.set_wz(coords::to_client(m.y(), origin_patch));
                m.halt_motion();
                m.set_pending_attack(true);
                return true;
            }
            m.set_tx(coords::to_client(m.x(), origin_patch));
            m.set_tz(coords::to_client(m.y(), origin_patch));
            const bool is_end = (type == static_cast<std::uint8_t>(protocol::S2C::MoveMonsterEnd));
            m.set_running(run != 0);
            m.set_move_speed(m.running() ? m.run_speed() : m.walk_speed());
            m.set_has_abs_target(true);
            m.set_moving(true);
            m.set_stop_on_arrive(is_end);
            const float fdx = m.tx() - m.wx();
            const float fdz = m.tz() - m.wz();
            if (fdx * fdx + fdz * fdz > 1e-4f) {
                m.set_yaw(std::atan2(-fdx, -fdz));
            }
            return true;
        }

        if (type == static_cast<std::uint8_t>(protocol::S2C::RemoveMonster)) {
            protocol::PacketReader r(payload);
            std::uint32_t id = 0;
            if (r.read_u32(id)) {
                auto it = m_monsters.find(id);
                if (it != m_monsters.end()) {
                    auto& m = it->second;
                    if (m.finishing_death() || m.pending_die() || m.pending_behead()) {
                        m.set_server_removed(true);
                        m.set_moving(false);
                    } else {
                        m_monsters.erase(it);
                    }
                }
            }
            return true;
        }

        if (type == static_cast<std::uint8_t>(protocol::S2C::MovePlayerOn) ||
            type == static_cast<std::uint8_t>(protocol::S2C::MovePlayerEnd)) {
            std::uint32_t id = 0;
            std::int8_t dx = 0, dy = 0, dz = 0;
            if (!protocol::Move::parse_player(payload.data(), payload.size(), id, dx, dy, dz)) {
                return true;
            }
            (void) dz;
            if (id == m_local_player_id) {
                if (m_read_only) {
                    m_server_px += dx;
                    m_server_py += dy;
                    m_server_pz += dz;
                    m_observed_motion.set_target(
                        {coords::to_client(m_server_px, origin_patch),
                         coords::to_client_y(m_server_pz),
                         coords::to_client(m_server_py, origin_patch)},
                        type == static_cast<std::uint8_t>(protocol::S2C::MovePlayerOn));
                }
                // Self echo: pose already advanced when we sent; keep id/baseline only.
                return true;
            }
            auto remote = m_remote_player_positions.find(id);
            if (remote != m_remote_player_positions.end()) {
                remote->second.x += static_cast<int>(dx);
                remote->second.y += static_cast<int>(dy);
                remote->second.z += static_cast<int>(dz);
                remote->second.moving =
                    type == static_cast<std::uint8_t>(protocol::S2C::MovePlayerOn);
            }
            return true;
        }

        if (type == static_cast<std::uint8_t>(protocol::S2C::MoveBefore)) {
            std::uint32_t id = 0;
            int x = 0, y = 0, z = 0;
            std::uint16_t dir = 0;
            if (!protocol::Move::parse_before(payload.data(), payload.size(), id, x, y, z, dir)) {
                return true;
            }
            (void) z;
            (void) dir;
            auto remote = m_remote_player_positions.find(id);
            if (remote != m_remote_player_positions.end()) {
                remote->second = {x, y, false, z};
            } else if (m_local_player_id == 0 || id == m_local_player_id) {
                m_local_player_id = id;
                seed_move_baseline(x, y, z);
                m_move_before_sx = x;
                m_move_before_sy = y;
                m_move_before_sz = z;
                m_have_move_before = true;
                if (m_read_only) {
                    m_observed_motion.reset({coords::to_client(x, origin_patch),
                                             coords::to_client_y(z),
                                             coords::to_client(y, origin_patch)});
                }
            }
            return true;
        }

        if (type == static_cast<std::uint8_t>(protocol::S2C::Teleport)) {
            std::uint8_t map = 0, cheat = 0;
            int x = 0, y = 0, z = 0;
            if (!protocol::Teleport::parse_server(
                    payload.data(), payload.size(), map, x, y, z, cheat)) {
                return true;
            }
            (void) cheat;
            m_teleport_map = static_cast<int>(map);
            m_teleport_sx = x;
            m_teleport_sy = y;
            m_teleport_sz = z;
            m_have_teleport = true;
            if (m_read_only) {
                seed_move_baseline(x, y, z);
                m_observed_motion.reset({coords::to_client(x, origin_patch),
                                         coords::to_client_y(z),
                                         coords::to_client(y, origin_patch)});
            }
            // Clear remote mobs/items; server will re-Create in the new sight.
            m_monsters.clear();
            m_npcs.clear();
            m_pending_html.clear();
            m_pending_assassin_list.reset();
            m_last_asked_npc_id = 0;
            m_ground_items.clear();
            return true;
        }
        return false;
    }

    bool NetClient::handle_status_packet(std::uint8_t type,
                                         const std::vector<std::uint8_t>& payload) {
        if (type == static_cast<std::uint8_t>(protocol::S2C::TalentTree)) {
            protocol::TalentState state;
            if (protocol::parse_talent_state(payload.data(), payload.size(), state)) {
                m_talent_state = state;
                m_talent_dirty = true;
                const auto now_ms = static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now().time_since_epoch())
                        .count());
                sync_talent_beacon_buff(m_buff_slots, state, now_ms);
            }
            return true;
        }

        if (type == static_cast<std::uint8_t>(protocol::S2C::MState)) {
            std::uint32_t entity_id = 0;
            std::uint64_t mstate = 0;
            if (!protocol::MState::parse(payload.data(), payload.size(), entity_id, mstate)) {
                return true;
            }
            m_entity_mstate[entity_id] = mstate;
            auto mit = m_monsters.find(entity_id);
            if (mit != m_monsters.end()) {
                mit->second.set_ice_slowed((mstate & protocol::kCmsMoveSpeed) != 0 ||
                                           (mstate & protocol::kCmsMoveStop) != 0);
                mit->second.set_move_stopped((mstate & protocol::kCmsMoveStop) != 0);
                if ((mstate & protocol::kCmsMoveSpeed) == 0 && mit->second.speed_value() != 0) {
                    mit->second.apply_speed_value(0);
                }
            }
            if (entity_id == m_local_player_id || m_local_player_id == 0) {
                auto upsert_rest_food = [&](std::uint64_t bit) {
                    const std::string_view icon = protocol::mstate_to_rest_food_icon(bit);
                    if (icon.empty()) {
                        return;
                    }
                    if ((mstate & bit) == 0) {
                        m_buff_slots.erase(std::remove_if(m_buff_slots.begin(),
                                                          m_buff_slots.end(),
                                                          [bit](const BuffSlotEntry& e) {
                                                              return e.bstate == bit;
                                                          }),
                                           m_buff_slots.end());
                        return;
                    }
                    BuffSlotEntry entry;
                    entry.bstate = bit;
                    entry.icon_file = std::string(icon);
                    entry.sys_name_id = protocol::mstate_to_rest_food_sys(bit);
                    entry.expire_ms = 0;
                    entry.always_blink = true;
                    for (auto& slot : m_buff_slots) {
                        if (slot.bstate == bit) {
                            slot = entry;
                            return;
                        }
                    }
                    m_buff_slots.push_back(entry);
                };
                upsert_rest_food(protocol::kCmsHp);
                upsert_rest_food(protocol::kCmsMp);
            }
            return true;
        }

        if (type == static_cast<std::uint8_t>(protocol::S2C::UpdateMProperty)) {
            std::uint32_t entity_id = 0;
            std::uint8_t prop = 0;
            std::int16_t value = 0;
            if (!protocol::Property::parse_update_mproperty(
                    payload.data(), payload.size(), entity_id, prop, value)) {
                return true;
            }
            if (prop != protocol::kPropertyMSpeed) {
                return true;
            }
            if (entity_id == m_local_player_id && m_local_player_id != 0) {
                m_local_mspeed = value;
            }
            auto mit = m_monsters.find(entity_id);
            if (mit != m_monsters.end()) {
                mit->second.apply_speed_value(value);
                mit->second.set_ice_slowed(value < 0);
            }
            return true;
        }

        if (type == static_cast<std::uint8_t>(protocol::S2C::Message) ||
            type == static_cast<std::uint8_t>(protocol::S2C::MessageV)) {
            protocol::PacketReader r(payload);
            std::uint8_t msg = 0;
            if (!r.read_u8(msg)) {
                return true;
            }
            if (msg == static_cast<std::uint8_t>(protocol::BStateMsg::AddBState)) {
                std::uint64_t bstate = 0;
                std::int32_t remain = 0;
                std::int32_t skill_index = 0;
                std::int32_t item_index = 0;
                if (!r.read_u64(bstate) || !r.read_i32(remain)) {
                    return true;
                }
                if (r.remaining() >= 4) {
                    (void) r.read_i32(skill_index);
                }
                if (r.remaining() >= 4) {
                    (void) r.read_i32(item_index);
                }
                const auto now = std::chrono::steady_clock::now().time_since_epoch();
                const std::uint64_t now_ms = static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
                for (int i = 0; i < 64; ++i) {
                    const std::uint64_t bit = std::uint64_t{1} << i;
                    if ((bstate & bit) == 0) {
                        continue;
                    }
                    const int inventory_register = protocol::inventory_register_index(bit);
                    if (inventory_register >= 0) {
                        m_inventory_register_expire_ms[static_cast<std::size_t>(
                            inventory_register)] =
                            remain <= 0 ? 0 : now_ms + static_cast<std::uint64_t>(remain) * 1000ull;
                        continue;
                    }
                    std::string icon;
                    std::string effect_name;
                    const bool exp_event = bit == protocol::kCbsExpEvent;
                    const bool haste = bit == protocol::kCbsSkillHaste;
                    if (haste) {
                        icon = std::string(protocol::bstate_to_buff_icon(bit));
                    }
                    if (m_item_catalog != nullptr && item_index > 0 && !haste && !exp_event) {
                        const ItemDef* item = m_item_catalog->find(item_index);
                        if (item != nullptr) {
                            icon = m_item_catalog->icon_filename(item_index);
                            effect_name = item->name();
                        }
                    }
                    if (icon.empty() && m_skill_catalog != nullptr && skill_index > 0) {
                        icon = m_skill_catalog->buff_icon_for_skill(m_char_clazz, skill_index);
                        if (icon.empty()) {
                            icon = m_skill_catalog->buff_icon_for_skill(1, skill_index);
                        }
                    }
                    if (icon.empty() && m_skill_catalog != nullptr) {
                        icon = std::string(m_skill_catalog->buff_icon_for(bit));
                    }
                    if (icon.empty()) {
                        icon = std::string(protocol::bstate_to_buff_icon(bit));
                    }
                    if (icon.empty()) {
                        continue;
                    }
                    const bool is_count =
                        (bit & (protocol::kCbsPayPerfectDef | protocol::kCbsPayDrainHp)) != 0;
                    BuffSlotEntry entry;
                    entry.bstate = bit;
                    entry.icon_file = std::move(icon);
                    entry.source_item_index = exp_event ? 0 : item_index;
                    entry.sys_name_id = protocol::bstate_to_sys_name(bit);
                    entry.effect_name = std::move(effect_name);
                    if (m_skill_catalog != nullptr) {
                        if (skill_index > 0) {
                            entry.effect_name = std::string(
                                m_skill_catalog->buff_name_for_skill(m_char_clazz, skill_index));
                            if (entry.effect_name.empty()) {
                                entry.effect_name = std::string(
                                    m_skill_catalog->buff_name_for_skill(1, skill_index));
                            }
                        }
                        if (entry.effect_name.empty()) {
                            entry.effect_name = std::string(m_skill_catalog->buff_name_for(bit));
                        }
                    }
                    if (exp_event) {
                        entry.effect_name = "EXP Event: +" + std::to_string(item_index) + "% EXP";
                    }
                    entry.is_count = is_count;
                    if (is_count) {
                        entry.charges = remain;
                        entry.expire_ms = 0;
                    } else {
                        entry.charges = 0;
                        entry.expire_ms =
                            remain <= 0 ? 0 : now_ms + static_cast<std::uint64_t>(remain) * 1000ull;
                    }
                    bool replaced = false;
                    for (auto& slot : m_buff_slots) {
                        if (slot.bstate == bit) {
                            slot = entry;
                            replaced = true;
                            break;
                        }
                    }
                    if (!replaced) {
                        m_buff_slots.push_back(entry);
                    }
                }
                return true;
            }
            if (msg == static_cast<std::uint8_t>(protocol::BStateMsg::SubBState)) {
                std::uint64_t bstate = 0;
                if (!r.read_u64(bstate)) {
                    return true;
                }
                for (int i = 0; i < 4; ++i) {
                    if ((bstate & protocol::inventory_register_bstate(i)) != 0) {
                        m_inventory_register_expire_ms[static_cast<std::size_t>(i)] = 0;
                    }
                }
                m_buff_slots.erase(std::remove_if(m_buff_slots.begin(),
                                                  m_buff_slots.end(),
                                                  [bstate](const BuffSlotEntry& e) {
                                                      return (e.bstate & bstate) != 0 ||
                                                             e.bstate == bstate;
                                                  }),
                                   m_buff_slots.end());
                return true;
            }
            if (msg == static_cast<std::uint8_t>(protocol::InfoMsg::SpecialtyUp)) {
                std::uint8_t specialty = 0;
                if (r.read_u8(specialty)) {
                    m_specialty = specialty;
                    m_specialty_index = protocol::specialty_title_index(specialty);
                    m_char_gov_pos = JobTitles::title(m_char_clazz, m_specialty_index);
                }
                return true;
            }
            if (msg == static_cast<std::uint8_t>(protocol::ItemMonsterMsg::Open) ||
                msg == static_cast<std::uint8_t>(protocol::ItemMonsterMsg::Opened) ||
                msg == static_cast<std::uint8_t>(protocol::ItemMonsterMsg::Canceled) ||
                msg == static_cast<std::uint8_t>(protocol::ItemMonsterMsg::Used)) {
                m_last_chest_feedback_msg = msg;
                if (DebugOverlay::enabled()) {
                    const char* name = "unknown";
                    if (msg == static_cast<std::uint8_t>(protocol::ItemMonsterMsg::Open)) {
                        name = "Open";
                    } else if (msg == static_cast<std::uint8_t>(protocol::ItemMonsterMsg::Opened)) {
                        name = "Opened";
                    } else if (msg ==
                               static_cast<std::uint8_t>(protocol::ItemMonsterMsg::Canceled)) {
                        name = "Canceled";
                    } else if (msg == static_cast<std::uint8_t>(protocol::ItemMonsterMsg::Used)) {
                        name = "Used";
                    }
                    log::info(std::string("chest_feedback: msg=") + name + " (" +
                              std::to_string(msg) + ")");
                }
            } else {
                if (msg == static_cast<std::uint8_t>(protocol::SystemMessageId::EnchantFailed)) {
                    m_enchant_failed = true;
                }
                const std::string line = BattleMsgBuilder::system_message(msg);
                if (!line.empty()) {
                    PendingInfoMessage info;
                    info.text = line;
                    info.kind = std::nullopt;
                    m_pending_infos.push_back(std::move(info));
                }
            }
            if (msg == static_cast<std::uint8_t>(protocol::ItemMonsterMsg::Open)) {
                std::uint8_t open_sec = 6;
                if (type == static_cast<std::uint8_t>(protocol::S2C::MessageV)) {
                    (void) r.read_u8(open_sec);
                }
                m_chest_open_sec = open_sec;
                m_have_chest_open_start = true;
            } else if (msg == static_cast<std::uint8_t>(protocol::ItemMonsterMsg::Opened) ||
                       msg == static_cast<std::uint8_t>(protocol::ItemMonsterMsg::Canceled) ||
                       msg == static_cast<std::uint8_t>(protocol::ItemMonsterMsg::Used)) {
                m_have_chest_open_stop = true;
            }
            return true;
        }
        return false;
    }

    bool NetClient::handle_combat_packet(std::uint8_t type,
                                         const std::vector<std::uint8_t>& payload,
                                         int origin_patch) {
        if (type == static_cast<std::uint8_t>(protocol::S2C::Attack)) {
            std::uint32_t attacker = 0, target = 0;
            std::int32_t damage = 0, palsy = 0;
            std::uint8_t flags = 0;
            if (!protocol::Attack::parse_server(
                    payload.data(), payload.size(), attacker, target, damage, palsy, flags)) {
                return true;
            }
            auto attacker_it = m_monsters.find(attacker);
            if (attacker_it != m_monsters.end()) {
                // Face the target (player or other mob) — server dir is not rebroadcast
                // each swing.
                face_monster_to_target(attacker, target, origin_patch);
                // ModelAttack: if runtime state, queue runtime state and
                // finish the current walk before GO_ATTACK. Halting mid-run played
                // the 2nd swing while the chase interpolation was still live.
                if (attacker_it->second.has_abs_target()) {
                    attacker_it->second.set_attack_after_move(true);
                } else {
                    attacker_it->second.halt_motion();
                    attacker_it->second.set_pending_attack(true);
                }
            }
            const bool player_target =
                target != 0 &&
                (target == m_local_player_id || m_remote_player_positions.count(target) != 0);
            const auto remote_attacker = m_remote_player_positions.find(attacker);
            const bool local_attacker = attacker != 0 && attacker == m_local_player_id;
            if (remote_attacker != m_remote_player_positions.end() && player_target) {
                // A confirmed PvP swing follows the attacker's final approach. A later
                // movement packet can still replace this snapshot.
                remote_attacker->second.moving = false;
            }
            if (remote_attacker != m_remote_player_positions.end() ||
                (local_attacker && (m_read_only || player_target))) {
                m_pending_player_attacks.push_back({attacker, target, player_target});
            }
            if ((flags & protocol::kAtkAnim) != 0) {
                return true;
            }
            if (palsy > 0) {
                flags |= protocol::kAtkExplosive;
            }
            if (attacker_it != m_monsters.end() && m_monsters.find(target) == m_monsters.end()) {
                PendingMonsterActionDamage ad;
                ad.damage = damage;
                ad.flags = flags;
                ad.target_id = target;
                attacker_it->second.set_pending_action_damage(ad);
            }
            // Every observer faces the victim toward the actual player attacker.
            if (local_attacker || remote_attacker != m_remote_player_positions.end()) {
                face_monster_to_target(target, attacker, origin_patch);
            }
            auto it = m_monsters.find(target);
            if (it != m_monsters.end()) {
                it->second.set_hp(std::max(0, it->second.hp() - static_cast<int>(damage)));
            }
            if (m_local_player_id != 0 && target == m_local_player_id) {
                m_hit_from_id = attacker;
                m_have_hit_from = true;
                if (damage > 0) {
                    const bool critical = (flags & protocol::kAtkFatal) != 0;
                    PendingInfoMessage info;
                    info.text =
                        critical ? BattleMsgBuilder::critical_damage_taken(static_cast<int>(damage))
                                 : BattleMsgBuilder::damage_taken(static_cast<int>(damage));
                    info.kind = protocol::BattleMsgKind::Damage;
                    if (critical) {
                        info.color = std::array<float, 3>{1.0f, 0.1f, 0.1f};
                    }
                    m_pending_infos.push_back(std::move(info));
                }
            }
            if (m_local_player_id != 0 && attacker == m_local_player_id && damage > 0) {
                const int eb = std::max(0, static_cast<int>(palsy));
                const int base = std::max(0, static_cast<int>(damage) - eb);
                const bool critical = (flags & protocol::kAtkFatal) != 0;
                const bool float_on_monster = m_monsters.find(target) != m_monsters.end();
                if (base > 0) {
                    PendingInfoMessage info;
                    info.text = critical ? BattleMsgBuilder::critical_damage_dealt(base)
                                         : BattleMsgBuilder::damage_dealt(base);
                    info.kind = protocol::BattleMsgKind::Damage;
                    if (critical) {
                        info.color = std::array<float, 3>{1.0f, 0.1f, 0.1f};
                    }
                    m_pending_infos.push_back(std::move(info));
                    if (float_on_monster) {
                        enqueue_combat_float(
                            CombatFloatKind::DamageDealt, std::to_string(base), target);
                    }
                }
                if (eb > 0) {
                    PendingInfoMessage info;
                    info.text = BattleMsgBuilder::explosive_blow(eb);
                    info.kind = protocol::BattleMsgKind::Damage;
                    m_pending_infos.push_back(std::move(info));
                    if (float_on_monster) {
                        enqueue_combat_float(
                            CombatFloatKind::DamageDealt, std::to_string(eb), target);
                    }
                }
            }
            const auto remote_class = m_remote_player_classes.find(attacker);
            const bool archer = attacker == m_local_player_id
                                    ? m_char_clazz == static_cast<int>(Class::Archer)
                                    : remote_class != m_remote_player_classes.end() &&
                                          remote_class->second == static_cast<int>(Class::Archer);
            bool pvp_impact = false;
            if (player_target &&
                (local_attacker || remote_attacker != m_remote_player_positions.end())) {
                const auto source_pvp = pvp_status(attacker);
                const auto target_pvp = pvp_status(target);
                pvp_impact = source_pvp.attackable || target_pvp.attackable || source_pvp.masked ||
                             target_pvp.masked || (source_pvp.dueling && target_pvp.dueling) ||
                             (castle_side(attacker) != 0 && castle_side(target) != 0);
            }
            // Misses consume their own arrow, so a later hit cannot claim its impact.
            if (((flags & protocol::kAtkHit) != 0 || archer) && target != 0 &&
                (m_monsters.find(target) != m_monsters.end() || pvp_impact)) {
                PendingHitFx fx;
                fx.attacker_id = attacker;
                fx.target_id = target;
                fx.flags = flags;
                m_pending_hit_fx.push_back(fx);
            }
            return true;
        }

        if (type == static_cast<std::uint8_t>(protocol::S2C::PlayerAnimation)) {
            protocol::PlayerEmote event;
            if (protocol::Emote::parse_server(payload.data(), payload.size(), event)) {
                m_pending_emotes.push_back(event);
            }
            return true;
        }

        if (type == static_cast<std::uint8_t>(protocol::S2C::Skill)) {
            protocol::S2CSkillFields sk{};
            if (!protocol::Skill::parse_server(payload.data(), payload.size(), sk)) {
                return true;
            }
            const bool expected_cast =
                m_expect_skill_index != 0 && sk.skill == m_expect_skill_index &&
                sk.caster == m_local_player_id && m_local_player_id != 0 &&
                ((m_expect_skill_target != 0 && sk.target == m_expect_skill_target) ||
                 (m_expect_skill_target == 0 && sk.caster == m_local_player_id &&
                  m_local_player_id != 0));
            if (expected_cast && sk.has_damage) {
                m_expect_skill_target = 0;
                m_expect_skill_index = 0;
            }
            const bool local_cast =
                (sk.caster == m_local_player_id && m_local_player_id != 0) || expected_cast;
            const auto remote_class = m_remote_player_classes.find(sk.caster);
            const int caster_class = remote_class != m_remote_player_classes.end()
                                         ? static_cast<int>(remote_class->second)
                                     : local_cast ? m_char_clazz
                                                  : -2;
            const bool explosion_splash =
                caster_class == 2 &&
                protocol::archer_special_kind(static_cast<int>(sk.skill)) ==
                    protocol::ArcherSpecialKind::ArrowExplosion &&
                sk.has_damage && sk.damage > 0;
            if (local_cast) {
                m_pending_local_skill = true;
            }
            if ((!local_cast || m_read_only) && sk.caster != 0 && !explosion_splash) {
                PendingSkillAction act;
                act.caster_id = sk.caster;
                act.skill = sk.skill;
                act.target_id = sk.target;
                act.fire = true;
                m_pending_skill_actions.push_back(act);
                face_monster_to_target(sk.caster, sk.target, origin_patch);
            }
            if (sk.has_damage && sk.target != 0) {
                if (local_cast || remote_class != m_remote_player_classes.end()) {
                    face_monster_to_target(sk.target, sk.caster, origin_patch);
                }
                auto it = m_monsters.find(sk.target);
                if (it != m_monsters.end()) {
                    it->second.set_hp(std::max(0, it->second.hp() - static_cast<int>(sk.damage)));
                }
                if (sk.damage > 0 && is_fire_delay_damage_skill(static_cast<int>(sk.skill))) {
                    PendingDelayDamage delay;
                    delay.target_id = sk.target;
                    delay.damage = static_cast<int>(sk.damage);
                    m_pending_delay_damage.push_back(delay);
                }
            }
            if (m_local_player_id != 0 && sk.target == m_local_player_id && sk.has_damage &&
                sk.damage > 0) {
                m_hit_from_id = sk.caster;
                m_have_hit_from = true;
                const bool critical = (sk.flags & protocol::kAtkFatal) != 0;
                PendingInfoMessage info;
                info.text =
                    critical ? BattleMsgBuilder::critical_damage_taken(static_cast<int>(sk.damage))
                             : BattleMsgBuilder::damage_taken(static_cast<int>(sk.damage));
                info.kind = protocol::BattleMsgKind::Damage;
                if (critical) {
                    info.color = std::array<float, 3>{1.0f, 0.1f, 0.1f};
                }
                m_pending_infos.push_back(std::move(info));
                if (m_monsters.find(sk.caster) != m_monsters.end()) {
                    enqueue_combat_float(CombatFloatKind::DamageTaken,
                                         std::to_string(static_cast<int>(sk.damage)),
                                         0);
                }
            }
            if (local_cast && sk.has_damage && sk.damage > 0 && sk.target != 0 &&
                sk.target != m_local_player_id) {
                const int eb = std::max(0, static_cast<int>(sk.palsy));
                const int base = std::max(0, static_cast<int>(sk.damage) - eb);
                const bool critical = (sk.flags & protocol::kAtkFatal) != 0;
                bool wait_for_bolt = false;
                if (m_char_clazz == 1) {
                    const SkillEffectEntry* se =
                        find_skill_effect(1, 1, static_cast<int>(sk.skill));
                    wait_for_bolt = se != nullptr && is_mage_bolt_et(se->effect);
                }
                if (base > 0) {
                    PendingInfoMessage info;
                    info.text = critical ? BattleMsgBuilder::critical_damage_dealt(base)
                                         : BattleMsgBuilder::damage_dealt(base);
                    info.kind = protocol::BattleMsgKind::Damage;
                    if (critical) {
                        info.color = std::array<float, 3>{1.0f, 0.1f, 0.1f};
                    }
                    m_pending_infos.push_back(std::move(info));
                    enqueue_combat_float(CombatFloatKind::DamageDealt,
                                         std::to_string(base),
                                         sk.target,
                                         wait_for_bolt);
                }
                if (eb > 0) {
                    PendingInfoMessage info;
                    info.text = BattleMsgBuilder::explosive_blow(eb);
                    info.kind = protocol::BattleMsgKind::Damage;
                    m_pending_infos.push_back(std::move(info));
                    enqueue_combat_float(
                        CombatFloatKind::DamageDealt, std::to_string(eb), sk.target, wait_for_bolt);
                }
            }
            if (sk.has_damage && (sk.flags & protocol::kAtkHit) != 0 && sk.target != 0) {
                PendingHitFx fx;
                fx.skill_index = static_cast<int>(sk.skill);
                fx.attacker_id = sk.caster;
                fx.target_id = sk.target;
                fx.flags = sk.flags;
                if (sk.palsy > 0) {
                    fx.flags |= protocol::kAtkExplosive;
                }
                m_pending_hit_fx.push_back(fx);
            }
            return true;
        }

        if (type == static_cast<std::uint8_t>(protocol::S2C::Effect)) {
            std::uint32_t entity_id = 0;
            std::uint8_t effect = 0;
            if (!protocol::Effect::parse(payload.data(), payload.size(), entity_id, effect)) {
                return true;
            }
            if (effect == static_cast<std::uint8_t>(protocol::EffectType::LevelUp)) {
                m_pending_level_up_fx.push_back(entity_id);
            } else if (effect == static_cast<std::uint8_t>(protocol::EffectType::Damaged)) {
                std::uint16_t damage = 0;
                if (protocol::Effect::parse_damaged(
                        payload.data(), payload.size(), entity_id, damage)) {
                    m_pending_damaged_fx.emplace_back(entity_id, damage);
                }
            } else if (effect == static_cast<std::uint8_t>(protocol::EffectType::Hp) ||
                       effect == static_cast<std::uint8_t>(protocol::EffectType::Mp) ||
                       (effect >= static_cast<std::uint8_t>(protocol::EffectType::Charm1) &&
                        effect <= static_cast<std::uint8_t>(protocol::EffectType::Charm8))) {
                m_pending_charm_fx.emplace_back(entity_id, effect);
            }
            return true;
        }

        if (type == static_cast<std::uint8_t>(protocol::S2C::Action)) {
            std::uint32_t skill_caster = 0;
            std::uint8_t skill_idx = 0;
            std::uint32_t skill_target = 0;
            if (protocol::Skill::parse_action_skill(
                    payload.data(), payload.size(), skill_caster, skill_idx, skill_target)) {
                PendingSkillAction act;
                act.caster_id = skill_caster;
                act.skill = skill_idx;
                act.target_id = skill_target;
                m_pending_skill_actions.push_back(act);
                face_monster_to_target(skill_caster, skill_target, origin_patch);
                return true;
            }
            std::uint32_t id = 0;
            std::uint8_t action = 0;
            std::uint8_t arg = 0;
            if (!protocol::Skill::parse_action(payload.data(), payload.size(), id, action, arg)) {
                return true;
            }
            if (action == protocol::kAtRest) {
                m_entity_resting[id] = arg != 0;
                if (id == m_local_player_id || m_local_player_id == 0) {
                    m_resting = arg != 0;
                    m_have_rest = true;
                }
            } else if (action == protocol::kAtDie) {
                auto it = m_monsters.find(id);
                if (it != m_monsters.end()) {
                    it->second.set_pending_die(true);
                    it->second.set_pending_knee(false);
                    it->second.set_hp(0);
                    it->second.set_gstate(it->second.gstate() & ~protocol::kCgsKnee);
                    it->second.set_moving(false);
                    it->second.set_finishing_death(true);
                }
            } else if (action == protocol::kAtKnee) {
                auto it = m_monsters.find(id);
                if (it != m_monsters.end()) {
                    it->second.set_pending_knee(true);
                    it->second.set_hp(0);
                    it->second.set_gstate(it->second.gstate() | protocol::kCgsKnee);
                    it->second.set_moving(false);
                }
            } else if (action == protocol::kAtBeheaded) {
                auto it = m_monsters.find(id);
                if (it != m_monsters.end()) {
                    it->second.set_pending_behead(true);
                    it->second.set_pending_knee(false);
                    it->second.set_hp(0);
                    it->second.set_gstate(it->second.gstate() & ~protocol::kCgsKnee);
                    it->second.set_moving(false);
                    it->second.set_finishing_death(true);
                }
            } else if (action == protocol::kAtPreTransform || action == protocol::kAtTransform) {
                PendingTransformAction tf;
                tf.entity_id = id;
                tf.pre = action == protocol::kAtPreTransform;
                tf.arg = arg;
                m_pending_transform_actions.push_back(tf);
                if (!tf.pre && (id == m_local_player_id || m_local_player_id == 0)) {
                    m_transform_tflag = static_cast<int>(arg);
                    if (protocol::transform_tflag_stage(m_transform_tflag) <= 0) {
                        m_local_mspeed = 0;
                    }
                }
            }
            return true;
        }
        return false;
    }

    void NetClient::process_frames(int origin_patch) {
        protocol::FrameDrain frames(m_rx);
        std::vector<std::uint8_t> payload;
        for (;;) {
            std::uint8_t type = 0;
            const protocol::FrameParseStatus parse_status = frames.pop(type, payload);
            if (parse_status == protocol::FrameParseStatus::Incomplete) {
                break;
            }
            if (parse_status == protocol::FrameParseStatus::Invalid) {
                log::warn("NetClient: server sent an invalid packet length");
                if (m_session) {
                    m_session->close();
                }
                frames.discard();
                break;
            }

            if (handle_online_packet(type, payload)) {
                continue;
            }
            if (handle_snapshot_packet(type, payload)) {
                continue;
            }

            if (handle_status_packet(type, payload)) {
                continue;
            }
            if (handle_inventory_packet(type, payload)) {
                continue;
            }

            if (handle_world_packet(type, payload, origin_patch)) {
                continue;
            }

            if (handle_quest_packet(type, payload)) {
                continue;
            }

            if (handle_combat_packet(type, payload, origin_patch)) {
                continue;
            }
        }
    }

}  // namespace openkal::client
