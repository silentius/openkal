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

#include <cstdint>

namespace openkal::protocol {

    /** Authoritative payload/selection shape for a Knight skill. */
    enum class KnightTargetShape : std::uint8_t {
        /** Skill has no cast target. */
        None = 0,

        /** Caster is the only target. */
        Self,

        /** One hostile monster is supplied. */
        Monster,

        /** One hostile monster or player is supplied. */
        HostileCharacter,

        /** One living friendly player is supplied. */
        Player,

        /** One dead friendly player is supplied. */
        DeadPlayer,

        /** Nearby party members are selected by the server. */
        Party,

        /** Hostile targets are selected around the caster. */
        SelfArea,

        /** Hostile targets are selected around or toward a supplied target. */
        TargetArea,
    };

    /** Authoritative gameplay family for a Knight skill. */
    enum class KnightEffectKind : std::uint8_t {
        /** Skill has no implemented gameplay family. */
        None = 0,

        /** Skill deals physical damage. */
        Damage,

        /** Skill deals damage and applies control. */
        DamageControl,

        /** Skill applies a beneficial state. */
        Buff,

        /** Skill applies non-damage control. */
        Control,

        /** Skill restores HP. */
        Heal,

        /** Skill revives a dead player. */
        Resurrection,

        /** Skill changes monster hostility. */
        Aggro,

        /** Skill executes the special Beheading behavior. */
        Execute,

        /** Skill provides movement or another non-combat utility. */
        Utility,

        /** Skill modifies learned passive behavior. */
        Passive,

        /** Skill belongs to the unavailable siege subsystem. */
        Siege,
    };

    /** Special behavior that needs class-specific handling. */
    enum class KnightSpecialKind : std::uint8_t {
        /** Skill uses its ordinary effect-family behavior. */
        None = 0,

        /** Run changes the caster's movement state. */
        Run,

        /** Beheading executes its demon-finisher behavior. */
        Beheading,

        /** Mockery or Provocation changes hostility. */
        Taunt,

        /** Skill participates in an ordered melee combo. */
        Combo,

        /** Rest toggles the caster's resting state. */
        Rest,

        /** Protect applies mitigation to one friendly player. */
        Protect,

        /** Shield Attack applies its player stun. */
        ShieldStun,

        /** Rush closes distance and applies stiffness. */
        Rush,

        /** Half Swing attacks the forward half-area. */
        FrontalSweep,

        /** Sacrifice spends caster HP to heal a player. */
        Sacrifice,

        /** Skill creates or replaces the active party chant. */
        Chant,

        /** Berserk applies its attack/defense tradeoff. */
        Berserk,

        /** Survival Instinct converts remaining MP into HP. */
        SurvivalInstinct,

        /** Elastic Shield spends HP while applying a party buff. */
        HealthCostBuff,

        /** Revival resurrects a dead friendly player. */
        Revival,

        /** Commonback returns the caster to a configured point. */
        Return,

        /** Skill requires the unavailable siege subsystem. */
        Siege,

        /** Walk on the Air is a faster run toggle that cannot stack with Run. */
        WalkOnAir,
    };

    /**
     * Tests whether an index belongs to the converted Knight catalog.
     * @param skill_index Knight skill index.
     * @return True for a known Knight skill index.
     */
    [[nodiscard]] constexpr bool knight_is_known_skill(int skill_index) {
        return (skill_index >= 0 && skill_index <= 42) || skill_index == 45 || skill_index == 62 ||
               skill_index == 70 || skill_index == 71 || skill_index == 80 || skill_index == 81 ||
               skill_index == 88 || skill_index == 91 || skill_index == 93;
    }

    /**
     * Returns the explicitly mapped cast shape for a Knight skill.
     * Later skills retain only shapes established by converted JSON data.
     * @param skill_index Knight skill index.
     * @return Cast shape.
     */
    [[nodiscard]] constexpr KnightTargetShape knight_target_shape(int skill_index) {
        switch (skill_index) {
        case 0:
        case 10:
        case 11:
        case 18:
        case 24:
        case 33:
        case 62:
        case 88:
            return KnightTargetShape::Self;
        case 4:
        case 8:
            return KnightTargetShape::Monster;
        case 3:
        case 5:
        case 7:
        case 9:
        case 13:
        case 15:
        case 16:
        case 17:
        case 21:
        case 22:
        case 41:
        case 42:
            return KnightTargetShape::HostileCharacter;
        case 1:
            return KnightTargetShape::Monster;
        case 14:
        case 26:
            return KnightTargetShape::Player;
        case 35:
            return KnightTargetShape::DeadPlayer;
        case 19:
        case 23:
        case 27:
        case 28:
        case 29:
        case 31:
        case 32:
        case 34:
        case 40:
            return KnightTargetShape::Party;
        case 38:
        case 91:
        case 93:
            return KnightTargetShape::SelfArea;
        case 25:
            return KnightTargetShape::TargetArea;
        default:
            return KnightTargetShape::None;
        }
    }

    /**
     * Returns the gameplay family for a Knight skill.
     * @param skill_index Knight skill index.
     * @return Effect family.
     */
    [[nodiscard]] constexpr KnightEffectKind knight_effect_kind(int skill_index) {
        switch (skill_index) {
        case 3:
        case 5:
        case 7:
        case 13:
        case 16:
        case 17:
        case 22:
        case 25:
        case 38:
        case 41:
        case 42:
        case 93:
            return KnightEffectKind::Damage;
        case 9:
            return KnightEffectKind::DamageControl;
        case 10:
        case 14:
        case 18:
        case 19:
        case 23:
        case 24:
        case 27:
        case 28:
        case 29:
        case 31:
        case 32:
        case 34:
        case 40:
            return KnightEffectKind::Buff;
        case 15:
        case 21:
        case 91:
            return KnightEffectKind::Control;
        case 26:
        case 33:
            return KnightEffectKind::Heal;
        case 35:
            return KnightEffectKind::Resurrection;
        case 4:
        case 8:
            return KnightEffectKind::Aggro;
        case 1:
            return KnightEffectKind::Execute;
        case 0:
        case 11:
        case 62:
        case 88:
            return KnightEffectKind::Utility;
        case 2:
        case 6:
        case 12:
        case 20:
        case 30:
        case 36:
        case 37:
        case 39:
        case 45:
        case 70:
        case 71:
            return KnightEffectKind::Passive;
        case 80:
        case 81:
            return KnightEffectKind::Siege;
        default:
            return KnightEffectKind::None;
        }
    }

    /** Returns the implemented Knight-specific handling for a skill index.
     * Converted later skills remain unclassified when their behavior is unavailable.
     * @param skill_index Knight skill index.
     * @return Special behavior classification.
     */
    [[nodiscard]] constexpr KnightSpecialKind knight_special_kind(int skill_index) {
        switch (skill_index) {
        case 0:
            return KnightSpecialKind::Run;
        case 1:
            return KnightSpecialKind::Beheading;
        case 4:
        case 8:
        case 42:
            return KnightSpecialKind::Taunt;
        case 7:
        case 9:
        case 13:
        case 41:
            return KnightSpecialKind::Combo;
        case 11:
            return KnightSpecialKind::Rest;
        case 14:
            return KnightSpecialKind::Protect;
        case 15:
            return KnightSpecialKind::ShieldStun;
        case 21:
            return KnightSpecialKind::Rush;
        case 25:
            return KnightSpecialKind::FrontalSweep;
        case 26:
            return KnightSpecialKind::Sacrifice;
        case 19:
        case 23:
        case 27:
        case 28:
        case 29:
        case 31:
        case 32:
            return KnightSpecialKind::Chant;
        case 24:
            return KnightSpecialKind::Berserk;
        case 33:
            return KnightSpecialKind::SurvivalInstinct;
        case 34:
            return KnightSpecialKind::HealthCostBuff;
        case 35:
            return KnightSpecialKind::Revival;
        case 62:
            return KnightSpecialKind::Return;
        case 88:
            return KnightSpecialKind::WalkOnAir;
        case 80:
        case 81:
            return KnightSpecialKind::Siege;
        default:
            return KnightSpecialKind::None;
        }
    }

    /**
     * Returns the confirmed area enumeration radius in server tiles.
     * @param skill_index Knight skill index.
     * @return Radius in tiles, or zero for non-area or unavailable behavior.
     */
    [[nodiscard]] constexpr int knight_area_radius_tiles(int skill_index) {
        if (skill_index == 25) {
            return 1;
        }
        if (skill_index == 42) {
            // Enumerates a 400px caster circle (ceil(400 / kTileSize)).
            return 13;
        }
        if (skill_index == 38 || skill_index == 91 || skill_index == 93) {
            return 3;
        }
        return 0;
    }

    /**
     * Returns the confirmed successful-target cap.
     * @param skill_index Knight skill index.
     * @return Target cap, or zero when uncapped or unavailable.
     */
    [[nodiscard]] constexpr int knight_target_cap(int skill_index) {
        static_cast<void>(skill_index);
        return 0;
    }

    /**
     * Returns the converted JSON energy cost for a Knight skill.
     * @param skill_index Knight skill index.
     * @param skill_level Learned rank.
     * @param character_level Caster level used by later converted formulas.
     * @param fallback_cost Cost for an unknown skill index.
     * @return Non-negative cost.
     */
    [[nodiscard]] constexpr int
    knight_skill_mp_cost(int skill_index, int skill_level, int character_level, int fallback_cost) {
        const int level = skill_level > 0 ? skill_level : 1;
        int cost = fallback_cost;
        switch (skill_index) {
        case 0:
            cost = 6;
            break;
        case 1:
        case 2:
        case 6:
        case 11:
        case 12:
        case 19:
        case 20:
        case 23:
        case 26:
        case 27:
        case 28:
        case 29:
        case 30:
        case 31:
        case 32:
        case 33:
        case 36:
        case 37:
        case 39:
        case 70:
        case 71:
        case 80:
        case 81:
            cost = 0;
            break;
        case 3:
        case 4:
            cost = 10 + level;
            break;
        case 5:
            cost = 28 + 3 * level;
            break;
        case 7:
        case 13:
            cost = 15 + level;
            break;
        case 8:
            cost = 24 + 4 * level;
            break;
        case 9:
            cost = 19 + 2 * level;
            break;
        case 10:
            cost = 76;
            break;
        case 14:
            cost = 25 + 25 * level;
            break;
        case 15:
            cost = 58;
            break;
        case 16:
            cost = 24 + 2 * level;
            break;
        case 17:
            cost = 12 + 3 * level;
            break;
        case 18:
            cost = 300 + 20 * level;
            break;
        case 21:
            cost = 46 + 4 * level;
            break;
        case 22:
            cost = 72 + 16 * level;
            break;
        case 24:
            cost = 200 + 15 * level;
            break;
        case 25:
            cost = 49 + 9 * level;
            break;
        case 34:
            cost = 475;
            break;
        case 35:
            cost = 240 + 80 * level;
            break;
        case 38:
            cost = level == 1 ? 209 : 200 + 20 * level;
            break;
        case 40: {
            const int base = 20 + 4 * character_level;
            cost = base > 0 ? base + level * base / 10 : 0;
            break;
        }
        case 41:
            cost = 30 + level;
            break;
        case 42:
        case 91:
            cost = 200 + level * (20 + (level - 1));
            break;
        case 45:
            cost = 0;
            break;
        case 62:
            cost = 30;
            break;
        case 88:
            cost = 6;
            break;
        case 93: {
            const int scaled = (5 * level * (character_level + level)) / 4;
            cost = 180 + scaled;
            break;
        }
        default:
            break;
        }
        return cost > 0 ? cost : 0;
    }

    /**
     * Returns the Life Extension max-HP percentage.
     * @param skill_level Learned rank.
     * @return Percentage bonus, or zero when unlearned.
     */
    [[nodiscard]] constexpr int knight_life_extension_percent(int skill_level) {
        return skill_level > 0 ? 14 + 10 * skill_level : 0;
    }

}  // namespace openkal::protocol
