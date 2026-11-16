/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/ArcherFxDispatcher.h"

#include "openkal/client/CombatFx.h"
#include "openkal/client/SkillEffectTable.h"
#include "openkal/client/SpriteFx.h"

#include <algorithm>
#include <cmath>

namespace openkal::client {

    ArcherFxDispatcher::ArcherFxDispatcher(SpriteFx& sprite_fx, CombatFx& combat_fx)
        : m_sprite_fx(&sprite_fx), m_combat_fx(&combat_fx) {}

    void ArcherFxDispatcher::clear_looping(std::uint32_t entity) {
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

    bool ArcherFxDispatcher::dispatch(engine::IEngine& engine,
                                      const SkillFxDispatch& dispatch,
                                      const SkillPresentEndpoints& ends) {
        const bool impact_effect =
            dispatch.attach == SkillFxAttach::Target &&
            (dispatch.effect == SkillEt::ExplosionArrow || dispatch.effect == SkillEt::SilentShot ||
             dispatch.effect == SkillEt::VitalityDrain);
        if (impact_effect) {
            const float dx = ends.hit_x - ends.tip_x;
            const float dy = ends.hit_y - ends.tip_y;
            const float dz = ends.hit_z - ends.tip_z;
            const float speed = dispatch.effect == SkillEt::VitalityDrain ? 200.0f : 500.0f;
            m_pending_impacts.push_back(
                {dispatch, ends, std::sqrt(dx * dx + dy * dy + dz * dz) / speed});
            return true;
        }
        return dispatch_now(engine, dispatch, ends);
    }

    void ArcherFxDispatcher::follow_target(std::uint32_t entity, float x, float y, float z) {
        if (!m_sprite_fx || entity == 0) {
            return;
        }
        for (auto& fuse : m_exploder_fuses) {
            if (fuse.target_entity != entity) {
                continue;
            }
            fuse.x = x;
            fuse.y = y;
            fuse.z = z;
            m_sprite_fx->follow_all(fuse.fx_entity, x, y, z);
        }
    }

    void ArcherFxDispatcher::update(engine::IEngine& engine, float dt) {
        for (auto& impact : m_pending_impacts) {
            impact.remaining_sec -= std::max(0.0f, dt);
            if (impact.remaining_sec <= 0.0f) {
                (void) dispatch_now(engine, impact.dispatch, impact.ends);
            }
        }
        m_pending_impacts.erase(std::remove_if(m_pending_impacts.begin(),
                                               m_pending_impacts.end(),
                                               [](const PendingImpact& impact) {
                                                   return impact.remaining_sec <= 0.0f;
                                               }),
                                m_pending_impacts.end());
        for (auto& fuse : m_exploder_fuses) {
            fuse.remaining_sec -= std::max(0.0f, dt);
            if (fuse.remaining_sec > 0.0f || !m_sprite_fx) {
                continue;
            }
            m_sprite_fx->detach(fuse.fx_entity);
            // OffMStateEx: RemoveFx then AddParticleToTargetBone("davi_ef131_05", bone 1).
            // Play the FX script sound (SE010005) that the particle shader does not carry.
            const auto sounds = m_sprite_fx->fx_sounds(engine, kArcherExploderBurstFx);
            m_sprite_fx->spawn_oneshot_particle(
                engine, kArcherExploderBurstFx, fuse.x, fuse.y, fuse.z, fuse.target_entity, 1);
            if (m_combat_fx) {
                for (const auto& sound : sounds) {
                    m_combat_fx->play_sfx(sound, fuse.x, fuse.y, fuse.z);
                }
            }
        }
        m_exploder_fuses.erase(
            std::remove_if(m_exploder_fuses.begin(),
                           m_exploder_fuses.end(),
                           [](const ExploderFuse& fuse) { return fuse.remaining_sec <= 0.0f; }),
            m_exploder_fuses.end());
    }

    bool ArcherFxDispatcher::dispatch_now(engine::IEngine& engine,
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
        if (dispatch.kind == SkillFxAssetKind::Fx && !dispatch.looping) {
            const auto sounds = m_sprite_fx->spawn_oneshot_fx(engine, dispatch.asset, x, y, z);
            for (const auto& sound : sounds) {
                m_combat_fx->play_sfx(sound, x, y, z);
            }
            if (dispatch.effect == SkillEt::VitalityDrain &&
                dispatch.attach == SkillFxAttach::Target) {
                m_combat_fx->play_sfx("efs014", ends.tip_x, ends.tip_y, ends.tip_z);
            }
            return true;
        }
        if (entity == 0) {
            return false;
        }

        std::uint32_t fx_entity = entity;
        if (dispatch.looping || dispatch.kind == SkillFxAssetKind::Particle) {
            fx_entity = m_next_entity++;
            if (m_next_entity < 0xD0000000u || m_next_entity >= 0xE0000000u) {
                m_next_entity = 0xD0000000u;
            }
            if (dispatch.looping && dispatch.effect != SkillEt::ExploderArrow) {
                m_looping_entities[entity].push_back(fx_entity);
            }
        }

        if (dispatch.kind == SkillFxAssetKind::Fx) {
            m_sprite_fx->attach_fx(engine, fx_entity, dispatch.asset, -1);
            for (const auto& sound : m_sprite_fx->fx_sounds(engine, dispatch.asset)) {
                m_combat_fx->play_sfx(sound, x, y, z);
            }
        } else {
            m_sprite_fx->attach_particle(engine, fx_entity, dispatch.asset, -1, 0.0f, 0.0f, 0.0f);
        }
        m_sprite_fx->follow_all(fx_entity, x, y, z);
        if (!dispatch.looping) {
            m_sprite_fx->rewind(fx_entity);
        }
        if (dispatch.effect == SkillEt::ExploderArrow && dispatch.looping) {
            for (auto& fuse : m_exploder_fuses) {
                if (fuse.caster_entity == ends.caster_entity && m_sprite_fx) {
                    m_sprite_fx->detach(fuse.fx_entity);
                    fuse.remaining_sec = 0.0f;
                }
            }
            m_exploder_fuses.erase(
                std::remove_if(m_exploder_fuses.begin(),
                               m_exploder_fuses.end(),
                               [](const ExploderFuse& fuse) { return fuse.remaining_sec <= 0.0f; }),
                m_exploder_fuses.end());
            ExploderFuse fuse;
            fuse.caster_entity = ends.caster_entity;
            fuse.target_entity = entity;
            fuse.fx_entity = fx_entity;
            fuse.remaining_sec = kArcherExploderFuseSec;
            fuse.x = x;
            fuse.y = y;
            fuse.z = z;
            m_exploder_fuses.push_back(fuse);
        }
        return true;
    }

}  // namespace openkal::client
