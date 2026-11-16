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

    /** Authoritative payload/selection shape for an Archer skill. */
    enum class ArcherTargetShape : std::uint8_t {
        /** Skill has no cast target. */
        None = 0,

        /** Caster is the only target. */
        Self,

        /** One hostile monster is supplied. */
        Monster,

        /** One hostile monster or player is supplied. */
        HostileCharacter,

        /** Nearby party members are selected by the server. */
        Party,

        /** Hostile targets are selected around a supplied target. */
        TargetArea,

        /** Hostile targets are selected around supplied world coordinates. */
        GroundPosition,
    };

    /** Gameplay family for an Archer skill. */
    enum class ArcherEffectKind : std::uint8_t {
        /** Skill has no classified gameplay family. */
        None = 0,

        /** Skill deals physical damage. */
        Damage,

        /** Skill deals damage and applies a harmful state. */
        DamageControl,

        /** Skill applies a beneficial state. */
        Buff,

        /** Skill applies a harmful state without direct damage. */
        Debuff,

        /** Skill damages a target and restores caster HP. */
        Drain,

        /** Skill removes beneficial states from a hostile target. */
        Dispel,

        /** Skill creates a persistent hostile area. */
        PersistentArea,

        /** Skill performs a non-combat action. */
        Utility,

        /** Skill modifies learned passive behavior. */
        Passive,

        /** Skill belongs to the unavailable siege subsystem. */
        Siege,
    };

    /** Archer-specific behavior that needs dedicated handling. */
    enum class ArcherSpecialKind : std::uint8_t {
        /** Skill uses ordinary effect-family behavior. */
        None = 0,

        /** Run changes the caster's movement state. */
        Run,

        /** Beheading executes its demon-finisher behavior. */
        Beheading,

        /** Rest toggles the caster's resting state. */
        Rest,

        /** Negative Arrow reduces target hostility. */
        AggroReduction,

        /** Heart Breaker applies its stopping effect. */
        HeartBreaker,

        /** Poison Arrow consumes a Poison Pouch. */
        PoisonArrow,

        /** Focus Shot uses charged-shot behavior. */
        FocusShot,

        /** Skill applies an elemental or control resistance. */
        Resistance,

        /** Emergency Escape applies a nearby party movement buff. */
        EmergencyEscape,

        /** Poison Cloud creates a ground-position poison field. */
        PoisonCloud,

        /** Arrow Rain channels a ground-position arrow storm. */
        ArrowRain,

        /** Arrow Explosion plants a delayed splash on one hostile. */
        ArrowExplosion,

        /** Commonback returns the caster to a configured point. */
        Return,

        /** The configured skill requires behavior outside the current implementation. */
        LaterSource,

        /** Skill requires the unavailable siege subsystem. */
        Siege,
    };

    /** Implementation availability classification for an Archer skill. */
    enum class ArcherSourceKind : std::uint8_t {
        /** Target selection and gameplay rules are implemented. */
        Known = 0,

        /** The configured skill is recognized, but its gameplay rules are unavailable. */
        LaterSource,

        /** Skill belongs to the unavailable siege subsystem. */
        Siege,

        /** Index is not part of the converted Archer catalog. */
        Unknown,
    };

    /** Equipped or consumable item requirement for an Archer skill. */
    enum class ArcherItemRequirement : std::uint8_t {
        /** Skill has no confirmed item requirement. */
        None = 0,

        /** Skill requires an equipped bow. */
        Bow,

        /** Skill requires a bow and consumes one Poison Pouch. */
        BowAndPoisonPouch,
    };

    /** Converted item definition index of the Poison Pouch. */
    inline constexpr int kArcherPoisonPouchItemIndex = 360;

    /** Maximum Focus Shot charge-bar fill time in milliseconds. */
    inline constexpr int kArcherFocusShotBarMs = 3300;

    /** Emergency Escape movement-buff duration in seconds. */
    inline constexpr int kArcherEscapeDurationSec = 38;

    /** Emergency Escape movement-speed bonus in percent. */
    inline constexpr int kArcherEscapeMoveSpeedPercent = 200;

    /** Sharpness movement-effect duration in seconds. */
    inline constexpr int kArcherSharpnessDurationSec = 30;

    /** Sharpness movement-speed percentage. */
    inline constexpr int kArcherSharpnessMoveSpeedPercent = 90;

    /** Monster definition used to represent a Poison Cloud field. */
    inline constexpr int kArcherPoisonCloudControllerDef = 231;

    /** Poison Cloud radius in server tiles. */
    inline constexpr int kArcherPoisonCloudRadiusTiles = 2;

    /** Interval between poison HP ticks after the first delay, in milliseconds. */
    inline constexpr int kArcherPoisonCloudTickMs = 1000;

    /** Delay before the first poison HP tick, in milliseconds. */
    inline constexpr int kArcherPoisonCloudFirstTickMs = 2000;

    /** Poison duration applied on entering the field, in milliseconds. */
    inline constexpr int kArcherPoisonCloudBuffMs = 60000;

    /** Interval between field applications to nearby targets, in milliseconds. */
    inline constexpr int kArcherPoisonCloudPulseMs = 200;

    /** Poison Cloud field lifetime in milliseconds. */
    inline constexpr int kArcherPoisonCloudDurationMs = 12000;

    /** Monster definition used to represent an Arrow Rain field. */
    inline constexpr int kArcherArrowRainControllerDef = 567;

    /** Arrow Rain selection radius in server tiles. */
    inline constexpr int kArcherArrowRainRadiusTiles = 3;

    /** Interval between Arrow Rain damage pulses, in milliseconds. */
    inline constexpr int kArcherArrowRainTickMs = 2000;

    /** Number of damage pulses in an Arrow Rain channel. */
    inline constexpr int kArcherArrowRainTicks = 5;

    /** Arrow Rain controller lifetime in milliseconds. */
    inline constexpr int kArcherArrowRainDurationMs = 10000;

    /** Base-attack multiplier for each Arrow Rain damage pulse. */
    inline constexpr int kArcherArrowRainAttackMul = 7;

    /** Lower per-rank random damage contribution for Arrow Rain. */
    inline constexpr int kArcherArrowRainGradeMin = 1800;

    /** Upper per-rank random damage contribution for Arrow Rain. */
    inline constexpr int kArcherArrowRainGradeMax = 2000;

    /** Player-target Arrow Rain damage as a percentage of monster-target damage. */
    inline constexpr int kArcherArrowRainPvpPercent = 35;

    /** Delay from Arrow Explosion cast to splash damage, in milliseconds. */
    inline constexpr int kArcherArrowExplosionDelayMs = 3000;

    /** Arrow Explosion splash radius in server tiles. */
    inline constexpr int kArcherArrowExplosionRadiusTiles = 3;

    /** Base-attack multiplier for Arrow Explosion splash damage. */
    inline constexpr int kArcherArrowExplosionAttackMul = 20;

    /** Lower per-rank random damage contribution for Arrow Explosion. */
    inline constexpr int kArcherArrowExplosionGradeMin = 1000;

    /** Upper per-rank random damage contribution for Arrow Explosion. */
    inline constexpr int kArcherArrowExplosionGradeMax = 1200;

    /** Player-target Arrow Explosion damage as a percentage of monster-target damage. */
    inline constexpr int kArcherArrowExplosionPvpPercent = 30;

    /** Divisor applied to Shrewdness dodge scaling. */
    inline constexpr int kArcherShrewdnessDodgeDivisor = 400;

    /** Converted Blessing of Son of the Sky physical/magical power percent. */
    inline constexpr int kArcherBlessingPowerPercent = 8;

    /**
     * Tests whether an index belongs to the converted Archer catalog.
     * @param skill_index Archer skill index.
     * @return True for one of the 55 converted runtime indices.
     */
    [[nodiscard]] constexpr bool archer_is_known_skill(int skill_index) {
        return (skill_index >= 0 && skill_index <= 49) || skill_index == 62 || skill_index == 70 ||
               skill_index == 71 || skill_index == 80 || skill_index == 81;
    }

    /**
     * Returns source-confidence and blocking metadata.
     * @param skill_index Archer skill index.
     * @return Source classification.
     */
    [[nodiscard]] constexpr ArcherSourceKind archer_source_kind(int skill_index) {
        if (skill_index == 70 || skill_index == 71) {
            return ArcherSourceKind::LaterSource;
        }
        if (skill_index == 80 || skill_index == 81) {
            return ArcherSourceKind::Siege;
        }
        return archer_is_known_skill(skill_index) ? ArcherSourceKind::Known
                                                  : ArcherSourceKind::Unknown;
    }

    /**
     * Tests whether a skill must remain unavailable to runtime casting.
     * @param skill_index Archer skill index.
     * @return True for later-source, siege, and unknown indices.
     */
    [[nodiscard]] constexpr bool archer_is_blocked_skill(int skill_index) {
        return archer_source_kind(skill_index) != ArcherSourceKind::Known;
    }

    /**
     * Returns the explicitly mapped cast shape for an Archer skill.
     * @param skill_index Archer skill index.
     * @return Cast shape.
     */
    [[nodiscard]] constexpr ArcherTargetShape archer_target_shape(int skill_index) {
        switch (skill_index) {
        case 0:
        case 7:
        case 10:
        case 11:
        case 24:
        case 25:
        case 26:
        case 28:
        case 29:
        case 46:
        case 62:
            return ArcherTargetShape::Self;
        case 1:
        case 3:
        case 4:
        case 6:
        case 21:
            return ArcherTargetShape::Monster;
        case 9:
        case 13:
        case 14:
        case 15:
        case 16:
        case 18:
        case 19:
        case 22:
        case 23:
        case 27:
        case 33:
        case 34:
        case 35:
        case 36:
        case 37:
        case 38:
        case 39:
        case 40:
        case 41:
        case 43:
        case 44:
        case 45:
        case 49:
            return ArcherTargetShape::HostileCharacter;
        case 31:
            return ArcherTargetShape::Party;
        case 42:
        case 47:
            return ArcherTargetShape::GroundPosition;
        default:
            return ArcherTargetShape::None;
        }
    }

    /**
     * Returns the gameplay family for an Archer skill.
     * @param skill_index Archer skill index.
     * @return Effect family.
     */
    [[nodiscard]] constexpr ArcherEffectKind archer_effect_kind(int skill_index) {
        switch (skill_index) {
        case 3:
        case 4:
        case 6:
        case 14:
        case 16:
        case 49:
            return ArcherEffectKind::Damage;
        case 9:
        case 13:
        case 21:
            return ArcherEffectKind::DamageControl;
        case 7:
        case 10:
        case 24:
        case 25:
        case 26:
        case 28:
        case 29:
        case 31:
        case 46:
            return ArcherEffectKind::Buff;
        case 15:
        case 19:
        case 22:
        case 23:
        case 27:
        case 33:
        case 34:
        case 35:
        case 36:
        case 37:
        case 38:
        case 39:
        case 40:
        case 41:
        case 43:
        case 44:
            return ArcherEffectKind::Debuff;
        case 18:
            return ArcherEffectKind::Drain;
        case 45:
            return ArcherEffectKind::Dispel;
        case 42:
        case 47:
            return ArcherEffectKind::PersistentArea;
        case 0:
        case 1:
        case 11:
        case 62:
            return ArcherEffectKind::Utility;
        case 2:
        case 5:
        case 8:
        case 12:
        case 17:
        case 20:
        case 30:
        case 32:
        case 48:
        case 70:
        case 71:
            return ArcherEffectKind::Passive;
        case 80:
        case 81:
            return ArcherEffectKind::Siege;
        default:
            return ArcherEffectKind::None;
        }
    }

    /** Returns the implemented Archer-specific handling for a skill index.
     * @param skill_index Archer skill index.
     * @return Special behavior classification.
     */
    [[nodiscard]] constexpr ArcherSpecialKind archer_special_kind(int skill_index) {
        switch (skill_index) {
        case 0:
            return ArcherSpecialKind::Run;
        case 1:
            return ArcherSpecialKind::Beheading;
        case 4:
            return ArcherSpecialKind::AggroReduction;
        case 9:
            return ArcherSpecialKind::HeartBreaker;
        case 11:
            return ArcherSpecialKind::Rest;
        case 13:
            return ArcherSpecialKind::PoisonArrow;
        case 16:
            return ArcherSpecialKind::FocusShot;
        case 24:
        case 25:
        case 26:
        case 28:
        case 29:
            return ArcherSpecialKind::Resistance;
        case 31:
            return ArcherSpecialKind::EmergencyEscape;
        case 42:
            return ArcherSpecialKind::PoisonCloud;
        case 47:
            return ArcherSpecialKind::ArrowRain;
        case 49:
            return ArcherSpecialKind::ArrowExplosion;
        case 70:
        case 71:
            return ArcherSpecialKind::LaterSource;
        case 62:
            return ArcherSpecialKind::Return;
        case 80:
        case 81:
            return ArcherSpecialKind::Siege;
        default:
            return ArcherSpecialKind::None;
        }
    }

    /**
     * True when the compatible client enters RangeSkillCast instead of sending
     * C2S_SKILL on the first skill use (Poison Cloud 42 and Arrow Rain 47:
     * GameCharacter.cpp `PC_ARCHER`, `DEF_GRADE3_SKILL_HW_080800`).
     * Ground confirm is left-click (`ChkRangeSkillCast` WM_LBUTTONDOWN);
     * right-click cancels aiming (WM_RBUTTONDOWN).
     * @param skill_index Archer skill index.
     * @return True for ground-targeted Poison Cloud and Arrow Rain.
     */
    [[nodiscard]] constexpr bool archer_uses_range_skill_selector(int skill_index) {
        return skill_index == 42 || skill_index == 47;
    }

    /**
     * compatibility behavior pip for Focus Shot (non-Korean `DrawPileSkillBar`).
     * @param focus_ms Elapsed charge time in milliseconds.
     * @return 0 until 1000 ms, then 1 through 5.
     */
    [[nodiscard]] constexpr int archer_focus_shot_bar_count(int focus_ms) {
        if (focus_ms < 1000) {
            return 0;
        }
        if (focus_ms >= kArcherFocusShotBarMs) {
            return 5;
        }
        if (focus_ms >= 2800) {
            return 4;
        }
        if (focus_ms >= 2300) {
            return 3;
        }
        if (focus_ms >= 1700) {
            return 2;
        }
        return 1;
    }

    /**
     * Returns the confirmed equipment/consumable requirement.
     * @param skill_index Archer skill index.
     * @return Item requirement.
     */
    [[nodiscard]] constexpr ArcherItemRequirement archer_item_requirement(int skill_index) {
        switch (skill_index) {
        case 3:
        case 4:
        case 6:
        case 9:
        case 14:
        case 15:
        case 16:
        case 21:
        case 27:
        case 47:
        case 49:
            return ArcherItemRequirement::Bow;
        case 13:
            return ArcherItemRequirement::BowAndPoisonPouch;
        default:
            return ArcherItemRequirement::None;
        }
    }

    /**
     * Returns a required consumable item definition index.
     * @param skill_index Archer skill index.
     * @return Item index, or zero when no consumable is required.
     */
    [[nodiscard]] constexpr int archer_required_item_index(int skill_index) {
        return skill_index == 13 ? kArcherPoisonPouchItemIndex : 0;
    }

    /**
     * Returns the confirmed area enumeration radius in server tiles.
     * Poison Cloud is `compatibility behavior` (2). Arrow Rain and
     * Arrow Explosion use their configured Chebyshev radii.
     * @param skill_index Archer skill index.
     * @return Radius in tiles, or zero when non-area or unavailable.
     */
    [[nodiscard]] constexpr int archer_area_radius_tiles(int skill_index) {
        if (skill_index == 42) {
            return kArcherPoisonCloudRadiusTiles;
        }
        if (skill_index == 47) {
            return kArcherArrowRainRadiusTiles;
        }
        if (skill_index == 49) {
            return kArcherArrowExplosionRadiusTiles;
        }
        static_cast<void>(skill_index);
        return 0;
    }

    /**
     * Returns the confirmed successful-target cap.
     * Current converted Archer data does not establish an authoritative cap.
     * @param skill_index Archer skill index.
     * @return Target cap, or zero when uncapped or unavailable.
     */
    [[nodiscard]] constexpr int archer_target_cap(int skill_index) {
        static_cast<void>(skill_index);
        return 0;
    }

    /**
     * Returns the MP cost for a supported Archer skill.
     * @param skill_index Archer skill index.
     * @param skill_level Learned rank.
     * @param character_level Caster level used by later converted formulas.
     * @param fallback_cost Cost for an unknown skill index.
     * @return Non-negative MP cost.
     */
    [[nodiscard]] constexpr int
    archer_skill_mp_cost(int skill_index, int skill_level, int character_level, int fallback_cost) {
        const int level = skill_level > 0 ? skill_level : 1;
        int cost = fallback_cost;
        switch (skill_index) {
        case 0:
            cost = 6;
            break;
        case 1:
        case 2:
        case 5:
        case 8:
        case 11:
        case 12:
        case 17:
        case 20:
        case 30:
        case 32:
        case 48:
        case 70:
        case 71:
        case 80:
        case 81:
            cost = 0;
            break;
        case 3:
            cost = 16 + 3 * level;
            break;
        case 4:
            cost = 2 * level;
            break;
        case 6:
            cost = 4 + level;
            break;
        case 7:
            cost = 56;
            break;
        case 9:
            cost = 50 + level;
            break;
        case 10:
            cost = 92;
            break;
        case 13:
            cost = 15;
            break;
        case 14:
            cost = 8 + 3 * level / 2;
            break;
        case 15:
            cost = 72;
            break;
        case 16:
            cost = 25 + 5 * level;
            break;
        case 18:
            cost = 20 + 6 * level;
            break;
        case 19:
            cost = 40 + 8 * level;
            break;
        case 21:
            cost = 31 + 11 * level;
            break;
        case 22:
            cost = 20;
            break;
        case 23:
            cost = 57 + 11 * level;
            break;
        case 24:
        case 25:
        case 26:
            cost = 42 + 11 * level;
            break;
        case 27:
            cost = 80 + 20 * level;
            break;
        case 28:
        case 29:
            cost = 22 + 14 * level;
            break;
        case 31:
            cost = 70;
            break;
        case 33:
            cost = 50;
            break;
        case 34:
            cost = 40 + 16 * level;
            break;
        case 35:
            cost = 26;
            break;
        case 36:
        case 37:
        case 40:
            cost = 38;
            break;
        case 38:
        case 39:
            cost = 42;
            break;
        case 41:
            cost = 83;
            break;
        case 42:
            cost = 100 + 16 * level;
            break;
        case 43:
            cost = 98;
            break;
        case 44:
            cost = 100;
            break;
        case 45:
            cost = 68;
            break;
        case 46:
            cost = 50;
            break;
        case 47:
        case 49:
            cost = 20 + 4 * character_level;
            break;
        case 62:
            cost = 30;
            break;
        default:
            break;
        }
        return cost > 0 ? cost : 0;
    }

}  // namespace openkal::protocol
