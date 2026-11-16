/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/SkillPresenter.h"

#include "openkal/client/Audio.h"

#include <cctype>
#include <string_view>

namespace openkal::client {
    namespace {

        /** True when a JSON skill sound is a cast-channel loop (owned by main skill_loop). */
        [[nodiscard]] bool is_cast_loop_sound(std::string_view name) {
            return Audio::parse_name(name).looping();
        }

        /** True for configured skill-animation GB stems (`Skill_M_6_0`). */
        [[nodiscard]] bool is_skill_gb_stem(std::string_view name) {
            constexpr std::string_view prefix = "skill_";
            if (name.size() < prefix.size()) {
                return false;
            }
            for (std::size_t i = 0; i < prefix.size(); ++i) {
                const char c = static_cast<char>(std::tolower(static_cast<unsigned char>(name[i])));
                if (c != prefix[i]) {
                    return false;
                }
            }
            return true;
        }

        /** True for ET values rendered by the compatibility behavior projectile. */
        [[nodiscard]] bool is_archer_arrow(SkillEt et) {
            switch (et) {
            case SkillEt::NegativeArrow:
            case SkillEt::ImmolationArrow:
            case SkillEt::FireArrow:
            case SkillEt::StunningArrow:
            case SkillEt::PoisonArrow:
            case SkillEt::ExplosionArrow:
            case SkillEt::ExploderArrow:
            case SkillEt::ChargedArrow:
            case SkillEt::MysticArrow:
            case SkillEt::SilentShot:
                return true;
            default:
                return false;
            }
        }

        /** Recognizes Archer effects that suppress the generic efs010 sound fallback. */
        [[nodiscard]] bool is_archer_et(SkillEt et) {
            switch (et) {
            case SkillEt::ChargedArrow:
            case SkillEt::ChargedArrowPrep:
            case SkillEt::FireArrow:
            case SkillEt::PoisonArrow:
            case SkillEt::ExplosionArrow:
            case SkillEt::ExploderArrow:
            case SkillEt::StunningArrow:
            case SkillEt::NegativeArrow:
            case SkillEt::ImmolationArrow:
            case SkillEt::CriticalChance:
            case SkillEt::WildStrength:
            case SkillEt::VitalityDrain:
            case SkillEt::InflectSuffering:
            case SkillEt::InflectSufferingPrep:
            case SkillEt::MeshOfSpeed:
            case SkillEt::DarknessVision:
            case SkillEt::DarknessVisionPrep:
            case SkillEt::ResistLightning:
            case SkillEt::ResistIce:
            case SkillEt::ResistFire:
            case SkillEt::SilentShot:
            case SkillEt::SilentShotPrep:
            case SkillEt::ResistCurse:
            case SkillEt::ResistSpirit:
            case SkillEt::EmergencyEscape:
            case SkillEt::CloudSight:
            case SkillEt::ManaBurn:
            case SkillEt::ManaBurnPrep:
            case SkillEt::MeshOfChance:
            case SkillEt::MeshOfForce:
            case SkillEt::MeshOfHealth:
            case SkillEt::MeshOfAgility:
            case SkillEt::ConstrictMind:
            case SkillEt::ConstrictWisdom:
            case SkillEt::VanishWall:
            case SkillEt::VanishConfidence:
            case SkillEt::DestroySight:
            case SkillEt::DestroySightPrep:
            case SkillEt::BenefitAblation:
            case SkillEt::BenefitAblationPrep:
            case SkillEt::PoisonCloudPrep:
                return true;
            default:
                return false;
            }
        }

        /** True for compatibility behavior fire ETs (Heal / Cure / Restore / Group Cure). */
        [[nodiscard]] bool is_chealing_et(SkillEt et) {
            switch (et) {
            case SkillEt::Healing:
            case SkillEt::LightRemedy:
            case SkillEt::LightReflection:
                return true;
            default:
                return false;
            }
        }

    }  // namespace

    void skill_fx_world(
        SkillFxAttach attach, const SkillPresentEndpoints& ends, float& x, float& y, float& z) {
        switch (attach) {
        case SkillFxAttach::Caster:
            x = ends.tip_x;
            y = ends.tip_y;
            z = ends.tip_z;
            return;
        case SkillFxAttach::CasterOrigin:
            x = ends.caster_origin_x;
            y = ends.caster_origin_y;
            z = ends.caster_origin_z;
            return;
        case SkillFxAttach::TargetOrigin:
            x = ends.origin_x;
            y = ends.origin_y;
            z = ends.origin_z;
            return;
        case SkillFxAttach::Target:
            x = ends.hit_x;
            y = ends.hit_y;
            z = ends.hit_z;
            return;
        }
    }

    std::uint32_t skill_fx_entity(SkillFxAttach attach, const SkillPresentEndpoints& ends) {
        return attach == SkillFxAttach::Caster || attach == SkillFxAttach::CasterOrigin
                   ? ends.caster_entity
                   : ends.target_entity;
    }

    bool SkillPresenter::present(engine::IEngine& engine,
                                 int job,
                                 int skill_index,
                                 SkillPresentPhase phase,
                                 const SkillDef* def,
                                 const SkillPresentEndpoints& ends) {
        if (!m_fx || !ends.valid) {
            return false;
        }
        if (phase == SkillPresentPhase::Fire) {
            m_fx->clear_cast_fx(ends.caster_entity);
        }
        // Persistent fields use magic-monster idle `(*EFFECT_FX …)` cues.
        // Skip a second fire dispatch at the caster or ground. Explosive Burst shares
        // RangeSkillCast targeting but still plays effect_ef091 at the ground point.
        if (job == 1 && phase == SkillPresentPhase::Fire &&
            protocol::mage_persistent_duration_ms(skill_index) > 0) {
            return true;
        }
        if (job == 2 && skill_index == 21 && phase == SkillPresentPhase::Fire) {
            if (m_archer_fx_dispatcher) {
                m_archer_fx_dispatcher->clear_looping(ends.caster_entity);
            }
            for (int i = 0; i < kArcherMysticArrowCount; ++i) {
                m_fx->spawn_arrow(SkillEt::MysticArrow,
                                  ends.tip_x,
                                  ends.tip_y,
                                  ends.tip_z,
                                  ends.hit_x,
                                  ends.hit_y,
                                  ends.hit_z,
                                  kArcherMysticExtras[i],
                                  ends.target_entity,
                                  1,
                                  ends.impact_scale);
            }
            return true;
        }
        const int phase_i = (phase == SkillPresentPhase::Prep) ? 0 : 1;
        const SkillEffectEntry* se = find_skill_effect(job, phase_i, skill_index);
        bool handled = false;
        if (se) {
            if (se->effect != SkillEt::None) {
                handled = present_et(engine, se->effect, ends) || handled;
            }
            if (se->effect2 != SkillEt::None && se->effect2 != se->effect) {
                handled = present_et(engine, se->effect2, ends) || handled;
            }
        }

        // JSON stems: only oneshot SFX here. `_L` cast loops are started/stopped by
        // main via play_skill_cast_sfx / stop_skill_cast_sfx (Lightning Blow efs019_L).
        if (def) {
            const std::string& snd = def->sound(phase_i);
            if (!snd.empty() && !is_cast_loop_sound(snd)) {
                m_fx->play_sfx(snd, ends.tip_x, ends.tip_y, ends.tip_z);
                handled = true;
            }
            const std::string& eff = def->effect(phase_i);
            if (phase == SkillPresentPhase::Prep && is_skill_gb_stem(eff)) {
                m_fx->spawn_cast_fx(
                    engine, ends.tip_x, ends.tip_y, ends.tip_z, 0.0f, eff, ends.caster_entity);
                handled = true;
            } else if (!handled && !eff.empty() && phase == SkillPresentPhase::Fire) {
                m_fx->play_sfx("efs010", ends.tip_x, ends.tip_y, ends.tip_z);
                handled = true;
            }
        }
        return handled;
    }

    bool SkillPresenter::present_et(engine::IEngine& engine,
                                    SkillEt et,
                                    const SkillPresentEndpoints& ends) {
        if (!m_fx) {
            return false;
        }
        const auto knight_dispatches = knight_fx_dispatches(et);
        if (!knight_dispatches.empty()) {
            if (m_knight_fx_dispatcher) {
                m_knight_fx_dispatcher->clear_looping(ends.caster_entity);
                for (const auto& dispatch : knight_dispatches) {
                    (void) m_knight_fx_dispatcher->dispatch(engine, dispatch, ends);
                }
            }
            // A mapped Knight ET is claimed even before its entity-aware SpriteFx
            // dispatcher is wired, so it never aliases to the generic efs010 SFX.
            return true;
        }

        const auto mage_dispatches = mage_fx_dispatches(et);
        if (!mage_dispatches.empty()) {
            if (m_knight_fx_dispatcher) {
                m_knight_fx_dispatcher->clear_looping(ends.caster_entity);
                for (const auto& dispatch : mage_dispatches) {
                    (void) m_knight_fx_dispatcher->dispatch(engine, dispatch, ends);
                }
            }
            if (is_chealing_et(et)) {
                // compatibility behavior plays efs022 and DrawDonut at the target origin;
                // the particle shader has no sound of its own.
                m_fx->play_sfx("efs022", ends.origin_x, ends.origin_y, ends.origin_z);
                m_fx->spawn_heal_donut(
                    ends.origin_x, ends.origin_y, ends.origin_z, ends.target_entity);
            }
            return true;
        }

        const auto archer_dispatches = archer_fx_dispatches(et);
        if (!archer_dispatches.empty()) {
            if (m_archer_fx_dispatcher) {
                m_archer_fx_dispatcher->clear_looping(ends.caster_entity);
                for (const auto& dispatch : archer_dispatches) {
                    (void) m_archer_fx_dispatcher->dispatch(engine, dispatch, ends);
                }
            }
            if (is_archer_arrow(et)) {
                m_fx->spawn_arrow(et,
                                  ends.tip_x,
                                  ends.tip_y,
                                  ends.tip_z,
                                  ends.hit_x,
                                  ends.hit_y,
                                  ends.hit_z,
                                  0.0f,
                                  ends.target_entity,
                                  1,
                                  ends.impact_scale);
            }
            if (et == SkillEt::VitalityDrain) {
                m_fx->spawn_vitality_drain(
                    ends.hit_x, ends.hit_y, ends.hit_z, ends.tip_x, ends.tip_y, ends.tip_z);
            }
            return true;
        }

        if (is_archer_arrow(et)) {
            if (m_archer_fx_dispatcher) {
                m_archer_fx_dispatcher->clear_looping(ends.caster_entity);
            }
            m_fx->spawn_arrow(et,
                              ends.tip_x,
                              ends.tip_y,
                              ends.tip_z,
                              ends.hit_x,
                              ends.hit_y,
                              ends.hit_z,
                              0.0f,
                              ends.target_entity,
                              1,
                              ends.impact_scale);
            return true;
        }

        if (is_archer_et(et) && m_archer_fx_dispatcher) {
            m_archer_fx_dispatcher->clear_looping(ends.caster_entity);
        }

        switch (et) {
        case SkillEt::FireBolt:
        case SkillEt::Fireball:
            if (m_knight_fx_dispatcher) {
                m_knight_fx_dispatcher->clear_looping(ends.caster_entity);
            }
            m_fx->spawn_fire_bolt(engine,
                                  ends.tip_x,
                                  ends.tip_y,
                                  ends.tip_z,
                                  ends.hit_x,
                                  ends.hit_y,
                                  ends.hit_z,
                                  ends.target_entity,
                                  ends.impact_scale);
            return true;
        case SkillEt::IceBolt:
        case SkillEt::FrostBlast:
            if (m_knight_fx_dispatcher) {
                m_knight_fx_dispatcher->clear_looping(ends.caster_entity);
            }
            m_fx->spawn_ice_bolt(engine,
                                 ends.tip_x,
                                 ends.tip_y,
                                 ends.tip_z,
                                 ends.hit_x,
                                 ends.hit_y,
                                 ends.hit_z,
                                 ends.target_entity,
                                 ends.impact_scale);
            return true;
        case SkillEt::IceNova:
            if (m_knight_fx_dispatcher) {
                m_knight_fx_dispatcher->clear_looping(ends.caster_entity);
            }
            m_fx->spawn_ice_nova(ends.origin_x, ends.origin_y, ends.origin_z, ends.target_entity);
            return true;
        case SkillEt::Lightning:
        case SkillEt::LightningImpact:
        case SkillEt::Spark:
        case SkillEt::LightningRed:
        case SkillEt::ManaBurn:
        case SkillEt::SpiritBlast:
            // Chain Lightning prep is compatibility behavior (effect_ef077); fire is
            // compatibility behavior and must drop the looping charge FX.
            if (m_knight_fx_dispatcher) {
                m_knight_fx_dispatcher->clear_looping(ends.caster_entity);
            }
            if (m_archer_fx_dispatcher && et == SkillEt::ManaBurn) {
                m_archer_fx_dispatcher->clear_looping(ends.caster_entity);
            }
            m_fx->spawn_lightning(engine,
                                  ends.tip_x,
                                  ends.tip_y,
                                  ends.tip_z,
                                  ends.hit_x,
                                  ends.hit_y,
                                  ends.hit_z,
                                  ends.impact_scale,
                                  lightning_beam_count(et),
                                  et == SkillEt::SpiritBlast
                                      ? CombatFx::LightningKind::Spirit
                                      : (is_red_lightning_et(et)
                                             ? CombatFx::LightningKind::Red
                                             : CombatFx::LightningKind::Yellow));
            return true;
        case SkillEt::HealingPrep:
            // Healing preparation uses character animation and optional JSON clip cues.
            return true;
        case SkillEt::StoneshieldPrep:
            // The preparation follows the caster spine rather than the feet.
            m_fx->spawn_stone_shield_prep(ends.hit_x, ends.hit_y, ends.hit_z, ends.caster_entity);
            return true;
        case SkillEt::Stoneshield:
            // The fire phase has no separate looping shield mesh.
            return true;
        case SkillEt::Haste:
            // ET_HASTE: pG_EffectGroup.AddTarget("haste", target, target matrix, Effect/).
            m_fx->spawn_clip_model(engine,
                                   "haste",
                                   ends.origin_x,
                                   ends.origin_y,
                                   ends.origin_z,
                                   0.0f,
                                   0.0f,
                                   0.0f,
                                   ends.target_entity);
            return true;
        case SkillEt::Meditation:
            // Two GenericGlow polylines orbit the caster spine
            // for 10 s. Skill_M_3_0 is archer compatibility behavior (Staggering Blow) prep.
            m_fx->spawn_meditation(ends.hit_x, ends.hit_y, ends.hit_z, ends.caster_entity);
            return true;
        case SkillEt::Taunt:
        case SkillEt::LightningImpactPrep:
        case SkillEt::ChargedArrowPrep:
            m_fx->spawn_cast_fx(
                engine, ends.tip_x, ends.tip_y, ends.tip_z, 0.0f, {}, ends.caster_entity);
            m_fx->play_sfx("efs010", ends.tip_x, ends.tip_y, ends.tip_z);
            return true;
        case SkillEt::BeheadingCharm:
            m_fx->spawn_behead(engine, ends.hit_x, ends.hit_y, ends.hit_z);
            return true;
        default:
            if (is_archer_et(et)) {
                return true;
            }
            m_fx->play_sfx("efs010", ends.tip_x, ends.tip_y, ends.tip_z);
            return true;
        }
    }

}  // namespace openkal::client
