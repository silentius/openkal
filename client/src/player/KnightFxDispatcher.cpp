/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/KnightFxDispatcher.h"

#include "openkal/client/CombatFx.h"
#include "openkal/client/Player.h"
#include "openkal/client/SkillEffectTable.h"
#include "openkal/client/SpriteFx.h"

namespace openkal::client {
    namespace {

        /** Spawns every FX `model` stage as a clip GB, including offset and delay. */
        void spawn_fx_models(engine::IEngine& engine,
                             SpriteFx& sprite_fx,
                             CombatFx& combat_fx,
                             const SkillFxDispatch& dispatch,
                             const SkillPresentEndpoints& ends,
                             std::uint32_t follow_entity) {
            float origin_x = 0.0f;
            float origin_y = 0.0f;
            float origin_z = 0.0f;
            skill_fx_world(dispatch.attach, ends, origin_x, origin_y, origin_z);
            for (const auto& stage : sprite_fx.fx_model_spawns(dispatch.asset)) {
                if (stage.stem().empty()) {
                    continue;
                }
                float x = origin_x + stage.ox();
                float y = origin_y + stage.oy();
                float z = origin_z + stage.oz();
                if (stage.effect_bone() >= 0) {
                    if (dispatch.attach == SkillFxAttach::Target ||
                        dispatch.attach == SkillFxAttach::TargetOrigin) {
                        x = ends.hit_x + stage.ox();
                        y = ends.hit_y + stage.oy();
                        z = ends.hit_z + stage.oz();
                    } else {
                        // EFFECTBONE 2 is HEAD in the compatible client. Without a
                        // posed skeleton here, lift from the character origin.
                        const float bone_y =
                            stage.effect_bone() >= 2 ? (kBodyHeight * 2.0f) : kBodyHeight;
                        x = origin_x + stage.ox();
                        y = origin_y + bone_y + stage.oy();
                        z = origin_z + stage.oz();
                    }
                }
                // Oneshot `restart 0` plays the GB at clip length. Passing the FX
                // duration (often the editor default 3 s) holds the last frame.
                const float duration = stage.restart() != 0 ? stage.duration() : 0.0f;
                combat_fx.spawn_clip_model(engine,
                                           stage.stem(),
                                           x,
                                           y,
                                           z,
                                           0.0f,
                                           stage.delay(),
                                           duration,
                                           follow_entity,
                                           false,
                                           false,
                                           stage.effect_bone(),
                                           stage.ox(),
                                           stage.oy(),
                                           stage.oz(),
                                           stage.use_effect_bone_rotation());
            }
        }

    }  // namespace

    KnightFxDispatcher::KnightFxDispatcher(SpriteFx& sprite_fx, CombatFx& combat_fx)
        : m_sprite_fx(&sprite_fx), m_combat_fx(&combat_fx) {}

    void KnightFxDispatcher::clear_looping(std::uint32_t entity) {
        if (!m_sprite_fx || entity == 0) {
            return;
        }
        const auto it = m_looping_entities.find(entity);
        if (it == m_looping_entities.end()) {
            return;
        }
        for (const std::uint32_t fx_entity : it->second) {
            m_sprite_fx->detach(fx_entity);
        }
        m_looping_entities.erase(it);
    }

    bool KnightFxDispatcher::dispatch(engine::IEngine& engine,
                                      const SkillFxDispatch& dispatch,
                                      const SkillPresentEndpoints& ends) {
        if (!m_sprite_fx || !m_combat_fx || dispatch.asset.empty()) {
            return false;
        }
        const std::uint32_t entity = skill_fx_entity(dispatch.attach, ends);
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        skill_fx_world(dispatch.attach, ends, x, y, z);

        if (!dispatch.looping) {
            // Fire oneshots (including Target-only Mage buffs) drop caster prep.
            clear_looping(ends.caster_entity);
        }
        if (dispatch.kind == SkillFxAssetKind::Fx && !dispatch.looping) {
            // Ground Explosive Burst keeps target_entity 0 so the blast stays
            // world-fixed at the hit point instead of following the caster.
            // TargetOrigin (Hail ice, Flame Bomb) must stay at feet: follow_oneshots
            // otherwise yanks every monster-followed oneshot to spine/head.
            const std::uint32_t follow =
                dispatch.attach == SkillFxAttach::TargetOrigin ? 0u : entity;
            const auto sounds =
                m_sprite_fx->spawn_oneshot_fx(engine, dispatch.asset, x, y, z, follow);
            for (const auto& sound : sounds) {
                m_combat_fx->play_sfx(sound, x, y, z);
            }
            spawn_fx_models(engine, *m_sprite_fx, *m_combat_fx, dispatch, ends, follow);
            return true;
        }
        if (entity == 0) {
            return false;
        }
        std::uint32_t fx_entity = entity;
        if (dispatch.looping || dispatch.kind == SkillFxAssetKind::Particle) {
            fx_entity = m_next_entity++;
            if (m_next_entity < 0xE0000000u) {
                m_next_entity = 0xE0000000u;
            }
            if (dispatch.looping) {
                m_looping_entities[entity].push_back(fx_entity);
            }
        }
        if (dispatch.kind == SkillFxAssetKind::Fx) {
            m_sprite_fx->attach_fx(engine, fx_entity, dispatch.asset, -1);
            for (const auto& sound : m_sprite_fx->fx_sounds(engine, dispatch.asset)) {
                m_combat_fx->play_sfx(sound, x, y, z);
            }
            spawn_fx_models(engine, *m_sprite_fx, *m_combat_fx, dispatch, ends, 0u);
        } else {
            m_sprite_fx->attach_particle(engine, fx_entity, dispatch.asset, -1, 0.0f, 0.0f, 0.0f);
        }
        m_sprite_fx->follow_all(fx_entity, x, y, z);
        if (!dispatch.looping) {
            m_sprite_fx->rewind(fx_entity);
        }
        return true;
    }

    void KnightFxDispatcher::set_perfect_defense(engine::IEngine& engine,
                                                 std::uint32_t owner,
                                                 bool active,
                                                 float x,
                                                 float y,
                                                 float z,
                                                 float yaw) {
        if (!m_sprite_fx || !m_combat_fx) {
            return;
        }
        if (owner == 0) {
            if (!active) {
                for (const auto& [id, fx_entity] : m_perfect_defense_entities) {
                    (void) id;
                    m_sprite_fx->detach(fx_entity);
                    m_combat_fx->kill_clip_entity(fx_entity);
                }
                m_perfect_defense_entities.clear();
            }
            return;
        }
        const auto it = m_perfect_defense_entities.find(owner);
        if (!active) {
            if (it == m_perfect_defense_entities.end()) {
                return;
            }
            m_sprite_fx->detach(it->second);
            m_combat_fx->kill_clip_entity(it->second);
            m_perfect_defense_entities.erase(it);
            return;
        }
        if (it != m_perfect_defense_entities.end()) {
            return;
        }
        if (!m_sprite_fx->has_fx(kPerfectDefenseLoopFx)) {
            return;
        }
        std::uint32_t fx_entity = m_next_buff_entity++;
        if (m_next_buff_entity < 0xD1000000u) {
            m_next_buff_entity = 0xD1000000u;
        }
        m_sprite_fx->attach_fx(engine, fx_entity, kPerfectDefenseLoopFx, -1);
        m_sprite_fx->loop_attached(fx_entity);
        m_sprite_fx->follow_all(fx_entity, x, y, z);
        for (const auto& sound : m_sprite_fx->fx_sounds(engine, kPerfectDefenseLoopFx)) {
            m_combat_fx->play_clip_loop_sfx(fx_entity, sound, x, y, z);
        }
        for (const auto& stage : m_sprite_fx->fx_model_spawns(kPerfectDefenseLoopFx)) {
            if (stage.stem().empty()) {
                continue;
            }
            const bool loop = stage.restart() != 0;
            m_combat_fx->spawn_clip_model(engine,
                                          stage.stem(),
                                          x + stage.ox(),
                                          y + stage.oy(),
                                          z + stage.oz(),
                                          yaw,
                                          stage.delay(),
                                          stage.duration(),
                                          fx_entity,
                                          true,
                                          loop);
        }
        m_perfect_defense_entities[owner] = fx_entity;
    }

    void KnightFxDispatcher::follow_perfect_defense(
        std::uint32_t owner, float x, float y, float z, float yaw) {
        if (!m_sprite_fx || !m_combat_fx || owner == 0) {
            return;
        }
        const auto it = m_perfect_defense_entities.find(owner);
        if (it == m_perfect_defense_entities.end()) {
            return;
        }
        m_sprite_fx->follow_all(it->second, x, y, z);
        m_combat_fx->follow_clip_entity(it->second, x, y, z, yaw);
    }

}  // namespace openkal::client
