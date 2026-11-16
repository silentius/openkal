/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/RemotePlayers.h"

#include "openkal/client/Skills.h"

#include <algorithm>
#include <cmath>
#include <iterator>

namespace openkal::client {

    void RemotePlayer::apply_network_pose(float wx, float wy, float wz, bool moving) {
        m_motion.set_target({wx, wy, wz}, moving);
    }

    void RemotePlayer::update_motion(float dt) {
        m_motion.update(dt);
    }

    engine::Mat4 RemotePlayer::model_matrix() const {
        auto model = engine::Mat4::translation(wx(), wy(), wz());
        model = engine::Mat4::multiply(
            model,
            engine::Mat4::from_quaternion(
                0.0f, std::sin(m_motion.yaw() * 0.5f), 0.0f, std::cos(m_motion.yaw() * 0.5f)));
        const float scale = m_actor.metamo_scale();
        if (m_actor.metamo_type() >= 0 && std::fabs(scale - 1.0f) > 0.001f) {
            model = engine::Mat4::multiply(model, engine::Mat4::scaling(scale, scale, scale));
        }
        return model;
    }

    void RemotePlayer::spine_world(float& x, float& y, float& z) const {
        x = wx();
        y = wy() + kBodyHeight;
        z = wz();
    }

    void RemotePlayer::face_target(float x, float z) {
        m_motion.face_target(x, z);
    }

    bool RemotePlayer::apply_appearance(
        engine::IEngine& engine,
        const std::string& data_root,
        std::unordered_map<std::string, engine::TextureHandle>& tex_cache,
        const protocol::CreatePlayerFields& fields,
        const ItemDb& catalog) {
        const Class clazz = static_cast<Class>(fields.class_byte & 0x7f);
        if (!m_actor.empty() && m_clazz == clazz && m_appearance.face == fields.face &&
            m_appearance.hair == fields.hair && m_appearance.skins == fields.skins &&
            m_appearance.shield == fields.shield &&
            std::equal(
                std::begin(fields.wear), std::end(fields.wear), std::begin(m_appearance.wear))) {
            return true;
        }
        m_actor.destroy(engine);
        m_clazz = clazz;
        m_appearance = fields;
        m_wear = Wear::from_item_indices(catalog, {std::begin(fields.wear), std::end(fields.wear)});
        if (const auto* shield = catalog.find(fields.shield);
            shield && shield->is_defense() && shield->class_small() == "shield") {
            m_wear.apply_item(*shield);
        }
        for (auto index : fields.skins) {
            if (const auto* def = catalog.find(index); def && def->is_skin()) {
                m_wear.apply_item(*def);
            }
        }
        m_actor.set_appearance(fields.face, fields.hair);
        return m_actor.load(engine, data_root, clazz, tex_cache, &m_wear, &catalog);
    }

    void RemotePlayer::present_skill(engine::IEngine& engine,
                                     const SkillDef* def,
                                     int skill,
                                     bool fire) {
        const int motion = def ? (fire ? def->motion_fire() : def->motion_prep()) : 0;
        if (fire && m_clazz == Class::Knight && (skill == 13 || skill == 41)) {
            m_actor.trigger_attack();
        } else if (motion > 0) {
            m_actor.trigger_skill_motion(motion, fire);
        } else if (fire) {
            m_actor.end_skill();
        }
        // Refresh the selected clip's sockets before emitting projectiles.
        m_actor.update(engine, 0.0f, false, false, false, false, nullptr, wx(), wy(), wz());
    }

    bool RemotePlayers::upsert(engine::IEngine& engine,
                               const std::string& data_root,
                               std::unordered_map<std::string, engine::TextureHandle>& tex_cache,
                               const protocol::CreatePlayerFields& fields,
                               const ItemDb& catalog,
                               float wx,
                               float wy,
                               float wz) {
        const Class clazz = static_cast<Class>(fields.class_byte & 0x7f);
        RemotePlayer* existing = find(fields.id);
        if (existing != nullptr) {
            existing->set_name(fields.name);
            existing->set_pose(wx, wy, wz);
            return existing->apply_appearance(engine, data_root, tex_cache, fields, catalog);
        }
        auto remote = std::make_unique<RemotePlayer>();
        remote->set_id(fields.id);
        remote->set_name(fields.name);
        remote->set_clazz(clazz);
        remote->set_pose(wx, wy, wz);
        if (!remote->apply_appearance(engine, data_root, tex_cache, fields, catalog)) {
            return false;
        }
        m_order.push_back(fields.id);
        m_players[fields.id] = std::move(remote);
        return true;
    }

    void RemotePlayers::remove(engine::IEngine& engine, std::uint32_t id) {
        auto it = m_players.find(id);
        if (it == m_players.end()) {
            return;
        }
        it->second->actor().destroy(engine);
        m_players.erase(it);
        m_rendered_ids.erase(id);
        m_order.erase(std::remove(m_order.begin(), m_order.end(), id), m_order.end());
    }

    void RemotePlayers::destroy(engine::IEngine& engine) {
        for (auto& [id, remote] : m_players) {
            (void) id;
            remote->actor().destroy(engine);
        }
        m_players.clear();
        m_order.clear();
        m_rendered_ids.clear();
        m_render_selection_active = false;
    }

    RemotePlayer* RemotePlayers::find(std::uint32_t id) {
        auto it = m_players.find(id);
        return it == m_players.end() ? nullptr : it->second.get();
    }

    const RemotePlayer* RemotePlayers::find(std::uint32_t id) const {
        auto it = m_players.find(id);
        return it == m_players.end() ? nullptr : it->second.get();
    }

    std::vector<RemotePlayer*> RemotePlayers::all() {
        std::vector<RemotePlayer*> out;
        out.reserve(m_order.size());
        for (const std::uint32_t id : m_order) {
            if (RemotePlayer* remote = find(id)) {
                out.push_back(remote);
            }
        }
        return out;
    }

    std::vector<const RemotePlayer*> RemotePlayers::all() const {
        std::vector<const RemotePlayer*> out;
        out.reserve(m_order.size());
        for (const std::uint32_t id : m_order) {
            if (const RemotePlayer* remote = find(id)) {
                out.push_back(remote);
            }
        }
        return out;
    }

    void RemotePlayers::set_render_selection(std::unordered_set<std::uint32_t> ids) {
        m_rendered_ids = std::move(ids);
        m_render_selection_active = true;
    }

    bool RemotePlayers::is_rendered(std::uint32_t id) const {
        return !m_render_selection_active || m_rendered_ids.count(id) != 0;
    }

    std::uint32_t RemotePlayers::pick(const FollowCamera& cam,
                                      const LocalPlayer& player,
                                      float mx,
                                      float my,
                                      float screen_w,
                                      float screen_h,
                                      float& hit_t) const {
        std::uint32_t hit_id = 0;
        float best = 0.0f;
        for (const RemotePlayer* remote : all()) {
            if (!is_rendered(remote->id())) {
                continue;
            }
            float t = 0.0f;
            if (!WorldPick::pick_monster_box(cam,
                                             player,
                                             mx,
                                             my,
                                             screen_w,
                                             screen_h,
                                             remote->wx(),
                                             remote->wy(),
                                             remote->wz(),
                                             WorldPick::kNpcHalfExtent,
                                             WorldPick::kNpcBaseHeight,
                                             t)) {
                continue;
            }
            if (hit_id == 0 || t < best) {
                best = t;
                hit_id = remote->id();
            }
        }
        hit_t = best;
        return hit_id;
    }

    void RemotePlayers::update(engine::IEngine& engine, float dt, Audio* audio) {
        for (auto& [id, remote] : m_players) {
            (void) id;
            remote->update_motion(dt);
            remote->actor().update(engine,
                                   dt,
                                   remote->moving(),
                                   false,
                                   remote->resting(),
                                   remote->consuming(),
                                   audio,
                                   remote->wx(),
                                   remote->wy(),
                                   remote->wz(),
                                   false,
                                   false,
                                   is_rendered(id));
        }
    }

    void
    RemotePlayers::draw(engine::IEngine& engine, const engine::Mat4& view, bool want_alpha) const {
        for (const RemotePlayer* remote : all()) {
            if (!is_rendered(remote->id())) {
                continue;
            }
            if (remote->actor().empty()) {
                continue;
            }
            remote->actor().draw(engine, remote->model_matrix(), view, want_alpha);
        }
    }

}  // namespace openkal::client
