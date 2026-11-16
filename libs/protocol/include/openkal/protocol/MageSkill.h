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

    /** Authoritative payload/selection shape for a Mage skill. */
    enum class MageTargetShape : std::uint8_t {
        /** Skill is not a usable Mage cast. */
        None = 0,

        /** Caster is the only target. */
        Self,

        /** One hostile monster is supplied. */
        Monster,

        /** One hostile player or monster is supplied. */
        HostileCharacter,

        /** One living friendly player is supplied. */
        Player,

        /** One dead friendly player is supplied. */
        DeadPlayer,

        /** Nearby party members are selected by the server. */
        Party,

        /** Hostile targets are selected around the caster. */
        SelfArea,

        /** Hostile targets are selected around a supplied monster. */
        TargetArea,

        /** Hostile targets are selected around supplied world coordinates. */
        GroundPosition,
    };

    /** Authoritative gameplay family for a Mage skill. */
    enum class MageEffectKind : std::uint8_t {
        /** Skill has no implemented gameplay family. */
        None = 0,

        /** Skill deals magical damage. */
        Damage,

        /** Skill restores HP or MP. */
        Heal,

        /** Skill applies a beneficial timed state. */
        Buff,

        /** Skill applies a harmful timed state. */
        Control,

        /** Skill revives a dead player. */
        Resurrection,

        /** Skill removes eligible harmful states. */
        Cleanse,

        /** Skill clears monster hostility. */
        AggroReset,

        /** Skill creates an owner-bound persistent damage area. */
        PersistentArea,

        /** Skill modifies learned passive behavior. */
        Passive,

        /** Skill belongs to the unavailable siege subsystem. */
        Siege,
    };

    /**
     * Returns the cast shape for an early Mage skill.
     * Later-source skills retain only shapes established by converted data.
     * @param skill_index Mage skill index.
     * @return Cast shape.
     */
    [[nodiscard]] constexpr MageTargetShape mage_target_shape(int skill_index) {
        switch (skill_index) {
        case 0:
        case 8:
        case 11:
        case 15:
        case 17:
        case 18:
        case 19:
        case 20:
        case 21:
        case 62:
        case 63:
        case 64:
            return MageTargetShape::Self;
        case 1:
        case 2:
        case 3:
        case 4:
        case 9:
        case 23:
        case 27:
        case 31:
        case 32:
        case 33:
        case 40:
        case 42:
        case 46:
        case 61:
            return MageTargetShape::Monster;
        case 65:
        case 67:
            return MageTargetShape::HostileCharacter;
        case 5:
        case 10:
        case 22:
        case 26:
        case 29:
        case 35:
        case 38:
        case 39:
        case 49:
        case 50:
        case 51:
        case 52:
        case 53:
        case 54:
        case 55:
        case 59:
        case 69:
            return MageTargetShape::Player;
        case 16:
        case 34:
            return MageTargetShape::DeadPlayer;
        case 28:
        case 57:
        case 58:
            return MageTargetShape::Party;
        case 24:
        case 25:
        case 66:
            return MageTargetShape::SelfArea;
        case 36:
        case 37:
        case 41:
        case 44:
            return MageTargetShape::TargetArea;
        case 43:
        case 45:
        case 47:
        case 48:
            return MageTargetShape::GroundPosition;
        default:
            return MageTargetShape::None;
        }
    }

    /**
     * Returns the gameplay family for a Mage skill.
     * @param skill_index Mage skill index.
     * @return Effect family.
     */
    [[nodiscard]] constexpr MageEffectKind mage_effect_kind(int skill_index) {
        switch (skill_index) {
        case 2:
        case 3:
        case 4:
        case 9:
        case 23:
        case 24:
        case 25:
        case 31:
        case 32:
        case 33:
        case 40:
        case 41:
        case 42:
        case 44:
        case 46:
        case 47:
        case 65:
        case 66:
        case 67:
            return MageEffectKind::Damage;
        case 8:
        case 10:
        case 22:
        case 28:
        case 29:
        case 35:
        case 55:
        case 57:
        case 58:
        case 59:
            return MageEffectKind::Heal;
        case 5:
        case 15:
        case 17:
        case 18:
        case 19:
        case 20:
        case 21:
        case 26:
        case 38:
        case 39:
        case 49:
        case 50:
        case 51:
        case 52:
        case 53:
        case 63:
        case 64:
        case 69:
            return MageEffectKind::Buff;
        case 27:
        case 36:
        case 37:
            return MageEffectKind::Control;
        case 16:
        case 34:
            return MageEffectKind::Resurrection;
        case 54:
            return MageEffectKind::Cleanse;
        case 61:
            return MageEffectKind::AggroReset;
        case 43:
        case 45:
        case 48:
            return MageEffectKind::PersistentArea;
        case 6:
        case 7:
        case 12:
        case 13:
        case 14:
        case 30:
        case 56:
        case 60:
        case 68:
        case 70:
        case 71:
            return MageEffectKind::Passive;
        case 80:
        case 81:
            return MageEffectKind::Siege;
        default:
            return MageEffectKind::None;
        }
    }

    /**
     * Returns the area radius in server tiles.
     * Thunder / Ice / Fire Storm use GetRadiusTile 4 (9×9 tile square). Other
     * converted area skills keep the GroundPosition default of 2.
     * @param skill_index Mage skill index.
     * @return Radius in tiles, or zero for non-area/unknown skills.
     */
    [[nodiscard]] constexpr int mage_area_radius_tiles(int skill_index) {
        switch (skill_index) {
        case 65:
            return 4;
        case 66:
        case 67:
            return 3;
        case 43:
        case 45:
        case 48:
            return 4;
        default:
            break;
        }
        switch (mage_target_shape(skill_index)) {
        case MageTargetShape::SelfArea:
        case MageTargetShape::TargetArea:
        case MageTargetShape::GroundPosition:
            return 2;
        default:
            return 0;
        }
    }

    /**
     * True when the compatible client enters RangeSkillCast instead of sending
     * C2S_SKILL on the first skill use (Thunder / Ice / Fire Storm and
     * Explosive Burst: GameCharacter.cpp skills 43, 45, 47, 48).
     * Ground confirm is left-click (ChkRangeSkillCast WM_LBUTTONDOWN);
     * right-click cancels aiming (WM_RBUTTONDOWN).
     * @param skill_index Mage skill index.
     * @return True for ground-targeted storms and Explosive Burst.
     */
    [[nodiscard]] constexpr bool mage_uses_range_skill_selector(int skill_index) {
        return skill_index == 43 || skill_index == 45 || skill_index == 47 || skill_index == 48;
    }

    /**
     * Returns the successful-target cap.
     * @param skill_index Mage skill index.
     * @return Target cap, or zero when uncapped by the skill.
     */
    [[nodiscard]] constexpr int mage_target_cap(int skill_index) {
        if (skill_index == 37) {
            return 5;
        }
        if (skill_index == 36) {
            return 8;
        }
        return 0;
    }

    /**
     * Returns the persistent-area lifetime.
     * @param skill_index Mage skill index.
     * @return Lifetime in milliseconds, or zero for non-persistent skills.
     */
    [[nodiscard]] constexpr int mage_persistent_duration_ms(int skill_index) {
        return mage_effect_kind(skill_index) == MageEffectKind::PersistentArea ? 16000 : 0;
    }

    /**
     * Returns the persistent-area attack interval.
     * @param skill_index Mage skill index.
     * @return Tick interval in milliseconds, or zero for non-persistent skills.
     */
    [[nodiscard]] constexpr int mage_persistent_tick_ms(int skill_index) {
        return mage_effect_kind(skill_index) == MageEffectKind::PersistentArea ? 2000 : 0;
    }

    /**
     * Delay until the first persistent-area tick.
     * G_monstermagic runs at 1000 ms; CanExcuteM damages on even ticks,
     * so the first hit lands within one second of spawn.
     * @param skill_index Mage skill index.
     * @return First-tick delay in milliseconds, or zero for non-persistent skills.
     */
    [[nodiscard]] constexpr int mage_persistent_first_tick_ms(int skill_index) {
        return mage_effect_kind(skill_index) == MageEffectKind::PersistentArea ? 1000 : 0;
    }

    /**
     * Returns the MP cost for a confirmed early Mage skill.
     * @param skill_index Mage skill index.
     * @param skill_level Learned rank.
     * @param character_level Caster level, used by Meditation.
     * @param fallback_cost Converted fixed JSON cost for unlisted skills.
     * @return Non-negative MP cost.
     */
    [[nodiscard]] constexpr int
    mage_skill_mp_cost(int skill_index, int skill_level, int character_level, int fallback_cost) {
        const int level = skill_level > 0 ? skill_level : 1;
        int cost = fallback_cost;
        switch (skill_index) {
        case 2:
            cost = 7 + level / 4;
            break;
        case 3:
            cost = 6 + level / 4;
            break;
        case 4:
            cost = 2 + level / 2;
            break;
        case 8:
            cost = 6 + 6 * level;
            break;
        case 9:
            cost = 56 + 6 * level;
            break;
        case 10:
            cost = 8 + 3 * level;
            break;
        case 15:
            cost = character_level - 9 * level;
            break;
        case 17:
            cost = 45;
            break;
        case 18:
            cost = 58;
            break;
        case 19:
            cost = 64;
            break;
        case 20:
            cost = 75;
            break;
        case 21:
            cost = 82;
            break;
        case 22:
            cost = 39 + 3 * level;
            break;
        case 23:
            cost = 12 + level;
            break;
        case 24:
            cost = 43 + 6 * level;
            break;
        case 25:
            cost = 32 + 6 * level;
            break;
        case 26:
            cost = 82 + 8 * level;
            break;
        case 27:
            cost = 47;
            break;
        case 28:
            cost = 58 + 4 * level;
            break;
        case 29:
            cost = 112;
            break;
        case 31:
            cost = 22 + 4 * level;
            break;
        case 32:
            cost = 12 + 4 * level;
            break;
        case 33:
            cost = 16 + 4 * level;
            break;
        case 35:
            cost = 39 + 4 * level;
            break;
        case 36:
            cost = 98 + 4 * level;
            break;
        case 37:
            cost = 60 + 6 * level;
            break;
        case 38:
            cost = 50 + 12 * level;
            break;
        case 39:
            cost = 41 + 31 * level;
            break;
        case 40:
            cost = 12 + 8 * level;
            break;
        case 41:
            cost = 174;
            break;
        case 42:
            cost = 30 + 8 * level;
            break;
        case 43:
            cost = 352;
            break;
        case 44:
            cost = 126;
            break;
        case 45:
            cost = 396;
            break;
        case 46:
            cost = 32 + 4 * level;
            break;
        case 47:
            cost = 142;
            break;
        case 48:
            cost = 327;
            break;
        case 49:
            cost = 64 + 32 * level;
            break;
        case 50:
            cost = 60 + 27 * level;
            break;
        case 51:
            cost = 67 + 26 * level;
            break;
        case 52:
            cost = 64 + 36 * level;
            break;
        case 53:
            cost = 72 + 38 * level;
            break;
        case 54:
            cost = 74;
            break;
        case 55:
            cost = 190 + 30 * level;
            break;
        case 57:
            cost = 72 + 15 * level;
            break;
        case 58:
            cost = 400 + 40 * level;
            break;
        case 59:
            cost = 596;
            break;
        case 61:
            cost = 50;
            break;
        case 63:
            cost = 102;
            break;
        case 64:
            cost = 5 * character_level;
            break;
        case 65:
        case 66:
        case 67:
            cost = 120 + 4 * character_level;
            break;
        case 69:
            cost = 400 + 10 * level;
            break;
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
    [[nodiscard]] constexpr int mage_life_extension_percent(int skill_level) {
        return skill_level > 0 ? 14 + 10 * skill_level : 0;
    }

}  // namespace openkal::protocol
