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

#include <algorithm>
#include <cstdint>

namespace openkal::protocol {

    /** Hiding movement modifier used by the compatible client speed formula. */
    inline constexpr int kThiefHidingMoveSpeedPercent = -30;

    /** Authoritative payload/selection shape for a pre-Awaken Thief skill. */
    enum class ThiefTargetShape : std::uint8_t {
        None = 0,
        Self,
        HostileCharacter,
        Party,
        SelfArea,
        TargetArea,
    };

    /** Gameplay family for a pre-Awaken Thief skill. */
    enum class ThiefEffectKind : std::uint8_t {
        None = 0,
        Damage,
        DamageControl,
        Buff,
        Control,
        Passive,
    };

    /** Thief behavior that needs dedicated runtime handling. */
    enum class ThiefSpecialKind : std::uint8_t {
        None = 0,
        Hiding,
        Confusion,
        DamageOverTime,
        Execute,
        SuicidalBlow,
        CounterAttack,
        MultiHit,
        ArmorBreaker,
        RemoveSign,
        FlameDance,
    };

    /** First skill index of the separate Awaken catalog, intentionally out of scope here. */
    inline constexpr int kThiefFirstAwakenSkill = 90;

    /** Tests whether an index belongs to the converted first/second/third-job catalog. */
    [[nodiscard]] constexpr bool thief_is_known_skill(int skill_index) {
        return skill_index >= 0 && skill_index <= 37;
    }

    /** Tests whether an index belongs to the explicitly separate Awaken catalog. */
    [[nodiscard]] constexpr bool thief_is_awaken_skill(int skill_index) {
        return skill_index >= kThiefFirstAwakenSkill;
    }

    /** Returns the explicitly mapped cast shape for a pre-Awaken Thief skill. */
    [[nodiscard]] constexpr ThiefTargetShape thief_target_shape(int skill_index) {
        switch (skill_index) {
        case 6:
        case 13:
        case 20:
            return ThiefTargetShape::Self;
        case 29:
        case 31:
        case 34:
        case 35:
            return ThiefTargetShape::Party;
        case 19:
        case 36:
            return ThiefTargetShape::SelfArea;
        case 7:
        case 8:
        case 9:
        case 10:
        case 12:
        case 14:
        case 15:
        case 16:
        case 17:
        case 18:
        case 21:
        case 22:
        case 23:
        case 28:
        case 33:
            return ThiefTargetShape::HostileCharacter;
        case 32:
            return ThiefTargetShape::TargetArea;
        default:
            return ThiefTargetShape::None;
        }
    }

    /** Returns the gameplay family for a pre-Awaken Thief skill. */
    [[nodiscard]] constexpr ThiefEffectKind thief_effect_kind(int skill_index) {
        switch (skill_index) {
        case 7:
        case 18:
        case 19:
        case 21:
        case 28:
        case 36:
            return ThiefEffectKind::Damage;
        case 8:
        case 10:
        case 12:
        case 14:
        case 16:
        case 17:
        case 23:
        case 32:
        case 33:
            return ThiefEffectKind::DamageControl;
        case 6:
        case 13:
        case 20:
        case 29:
        case 31:
        case 34:
        case 35:
            return ThiefEffectKind::Buff;
        case 9:
        case 15:
        case 22:
            return ThiefEffectKind::Control;
        case 2:
        case 3:
        case 4:
        case 5:
        case 24:
        case 25:
        case 26:
        case 27:
        case 30:
        case 37:
            return ThiefEffectKind::Passive;
        default:
            return ThiefEffectKind::None;
        }
    }

    /** Returns dedicated behavior metadata for a pre-Awaken Thief skill. */
    [[nodiscard]] constexpr ThiefSpecialKind thief_special_kind(int skill_index) {
        switch (skill_index) {
        case 6:
            return ThiefSpecialKind::Hiding;
        case 9:
            return ThiefSpecialKind::Confusion;
        case 14:
        case 17:
        case 23:
            return ThiefSpecialKind::DamageOverTime;
        case 18:
            return ThiefSpecialKind::Execute;
        case 20:
            return ThiefSpecialKind::CounterAttack;
        case 21:
            return ThiefSpecialKind::SuicidalBlow;
        case 28:
        case 36:
            return ThiefSpecialKind::MultiHit;
        case 22:
            return ThiefSpecialKind::ArmorBreaker;
        case 34:
            return ThiefSpecialKind::RemoveSign;
        case 35:
            return ThiefSpecialKind::FlameDance;
        default:
            return ThiefSpecialKind::None;
        }
    }

    /** Returns the converted Energy/MP cost instead of the empty skill definition field. */
    [[nodiscard]] constexpr int
    thief_skill_mp_cost(int skill_index, int skill_level, int character_level, int fallback) {
        const int slv = std::max(1, skill_level);
        const int clv = std::max(1, character_level);
        switch (skill_index) {
        case 6:
            return 10 * slv + 10;
        case 7:
            return 20 * slv + 25;
        case 8:
            return 10 * slv;
        case 9:
            return 20 * slv + 10;
        case 10:
            return 5 * slv + 10;
        case 12:
            return 10 * slv + 10;
        case 13:
            return 20 * slv + 80;
        case 14:
            return 30 * slv + 15;
        case 15:
            return 10 * slv + 45;
        case 16:
        case 17:
        case 22:
            return 30 * slv + 35;
        case 18:
            return 450;
        case 19:
            return 50 * slv + 25;
        case 20:
            return 30 * slv + 45;
        case 21:
            return 40 * slv + 45;
        case 23:
            return 65;
        case 28:
        case 33:
            return (clv + slv) * 3 + 50;
        case 29:
        case 34:
            return 15 + 30 * slv;
        case 31:
        case 35:
            return 35 + 30 * slv;
        case 32:
            return 230 + 63 * (clv + slv) * slv / 100;
        case 36:
            return 180 + 125 * (clv + slv) * slv / 100;
        default:
            return std::max(0, fallback);
        }
    }

    /** Returns the additive hit chance used by the derived server implementation. */
    [[nodiscard]] constexpr int thief_skill_hit(int skill_index) {
        switch (skill_index) {
        case 7:
        case 18:
            return 20;
        case 8:
        case 9:
        case 10:
        case 13:
        case 16:
        case 17:
        case 23:
        case 36:
            return 10;
        case 19:
            return 5;
        case 21:
            return 110;
        case 28:
        case 33:
            return 30;
        default:
            return 0;
        }
    }

    /** Returns physical power before PvP reduction. */
    [[nodiscard]] constexpr std::int32_t
    thief_skill_power(int skill_index, int skill_level, std::int32_t base_attack, int random_roll) {
        const std::int64_t slv = std::max(1, skill_level);
        const std::int64_t base = std::max<std::int32_t>(0, base_attack);
        const int roll_5_10 = std::clamp(random_roll, 5, 10);
        const int roll_15_30 = std::clamp(random_roll, 15, 30);
        switch (skill_index) {
        case 7:
            return static_cast<std::int32_t>(
                std::min<std::int64_t>(INT32_MAX, base + 128 + slv * roll_15_30));
        case 8:
            return static_cast<std::int32_t>(
                std::min<std::int64_t>(INT32_MAX, 3 * base + slv * roll_5_10));
        case 10:
            return static_cast<std::int32_t>(
                std::min<std::int64_t>(INT32_MAX, base + slv * roll_5_10));
        case 12:
            return static_cast<std::int32_t>(std::min<std::int64_t>(INT32_MAX, 2 * base));
        case 13:
        case 14:
        case 16:
            return static_cast<std::int32_t>(std::min<std::int64_t>(
                INT32_MAX, 2 * base + slv * (skill_index == 16 ? roll_15_30 : roll_5_10)));
        case 23:
            return static_cast<std::int32_t>(
                std::min<std::int64_t>(INT32_MAX, 2 * base + roll_5_10));
        case 17:
        case 32:
            return static_cast<std::int32_t>(
                std::min<std::int64_t>(INT32_MAX, 60 * base + slv * roll_5_10));
        case 18:
            return static_cast<std::int32_t>(
                std::min<std::int64_t>(INT32_MAX, 4 * base + slv * roll_5_10));
        case 19:
            return static_cast<std::int32_t>(
                std::min<std::int64_t>(INT32_MAX, 3 * base + slv * roll_5_10));
        case 21:
            return static_cast<std::int32_t>(
                std::min<std::int64_t>(INT32_MAX, 8 * base + slv * roll_5_10));
        case 28:
        case 36:
            return static_cast<std::int32_t>(
                std::min<std::int64_t>(INT32_MAX, 7 * base + slv * roll_5_10));
        case 33:
            return static_cast<std::int32_t>(
                std::min<std::int64_t>(INT32_MAX, 15 * base + slv * roll_5_10));
        default:
            return static_cast<std::int32_t>(base);
        }
    }

    /** Returns the percent of raw skill damage retained against player targets. */
    [[nodiscard]] constexpr int thief_pvp_damage_percent(int skill_index) {
        switch (skill_index) {
        case 17:
        case 32:
            return 5;
        case 23:
            return 100;
        case 28:
        case 36:
            return 35;
        case 14:
            return 50;
        case 33:
            return 60;
        default:
            return 100;
        }
    }

    /** Returns the explicitly mapped area radius in server tiles. */
    [[nodiscard]] constexpr int thief_area_radius_tiles(int skill_index) {
        switch (skill_index) {
        case 19:
        case 36:
            return 3;
        case 14:
        case 17:
        case 32:
            return 5;
        default:
            return 0;
        }
    }

    /** Returns the maximum number of affected targets, or zero for uncapped source behavior. */
    [[nodiscard]] constexpr int thief_target_cap(int skill_index) {
        switch (skill_index) {
        case 14:
        case 17:
            return 5;
        default:
            return 0;
        }
    }

    /** Returns the number of damage applications, including an immediate hit when present. */
    [[nodiscard]] constexpr int thief_hit_count(int skill_index, int skill_level) {
        switch (skill_index) {
        case 14:
            return 8;
        case 17:
            return 9;
        case 23:
            return 5;
        case 28:
        case 36:
            return std::max(1, skill_level) + 1;
        default:
            return 1;
        }
    }

    /** Tests whether casting is allowed to complete without first ending Hiding. */
    [[nodiscard]] constexpr bool thief_cast_allowed_from_hiding(int skill_index) {
        return skill_index == 7 || skill_index == 8 || skill_index == 10 || skill_index == 33;
    }

    /** Tests whether the converted jobsystem requires an active Hiding state. */
    [[nodiscard]] constexpr bool thief_skill_requires_hiding(int skill_index) {
        return skill_index == 7 || skill_index == 8 || skill_index == 10 || skill_index == 33;
    }

    /** Returns Acrobatic physical mitigation for one 1..100 probability roll. */
    [[nodiscard]] constexpr int
    thief_acrobatic_reduction_percent(int acrobatic_level, bool high_grade, int chance_roll) {
        const int roll = std::clamp(chance_roll, 1, 100);
        if (high_grade && roll <= 5) {
            return 25;
        }
        const int chance = acrobatic_level > 0 ? 5 + 2 * (acrobatic_level - 1) : 0;
        return roll <= chance ? 10 : 0;
    }

    /** Tests the derived Counter Attack probability roll. */
    [[nodiscard]] constexpr bool thief_counter_triggers(int chance_roll) {
        return std::clamp(chance_roll, 1, 1000) >= 650;
    }

    /** Returns Counter Attack power before the defender's physical reduction. */
    [[nodiscard]] constexpr std::int32_t
    thief_counter_power(std::int32_t base_attack, int skill_level, int random_roll) {
        const std::int64_t power =
            std::max<std::int32_t>(0, base_attack) +
            static_cast<std::int64_t>(std::max(1, skill_level)) * std::clamp(random_roll, 1, 2);
        return static_cast<std::int32_t>(std::min<std::int64_t>(INT32_MAX, power));
    }

}  // namespace openkal::protocol
