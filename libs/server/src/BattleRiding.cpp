/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/BattleRiding.h"

#include <algorithm>

namespace openkal::server {
    std::uint8_t battle_riding_grade_rarity(protocol::BattleRidingGrade grade) {
        // ItemRarity: White, Blue, Green, Orange, Red, Purple. The Battle Riding
        // progression uses the semantically matching existing presentation tiers.
        constexpr std::array<std::uint8_t, 4> kRarities{0, 2, 1, 5};
        return kRarities[static_cast<std::size_t>(grade)];
    }

    std::string_view battle_riding_grade_name(protocol::BattleRidingGrade grade) {
        constexpr std::array<std::string_view, 4> kNames{"Normal", "Uncommon", "Rare", "Epic"};
        return kNames[static_cast<std::size_t>(grade)];
    }

    int battle_riding_initial_ability_slots(protocol::BattleRidingGrade grade) {
        constexpr std::array<int, 4> kSlots{1, 1, 2, 2};
        return kSlots[static_cast<std::size_t>(grade)];
    }

    int battle_riding_combine_chance(protocol::BattleRidingGrade grade) {
        constexpr std::array<int, 4> kChance{100, 70, 40, 0};
        return kChance[static_cast<std::size_t>(grade)];
    }

    bool battle_riding_skill_allowed(protocol::BattleRidingGrade grade,
                                     int character_class,
                                     int skill_index) {
        // Epic removes the grade-specific restrictions. Speed-up and shared riding-state
        // restrictions are enforced by their own action handlers.
        if (grade == protocol::BattleRidingGrade::Epic) {
            return true;
        }
        constexpr std::array<int, 1> kKnight{21};
        constexpr std::array<int, 1> kMage{54};
        constexpr std::array<int, 2> kArcher{31, 46};
        constexpr std::array<int, 2> kThief{6, 33};
        const auto blocked = [skill_index](const auto& values) {
            return std::find(values.begin(), values.end(), skill_index) != values.end();
        };
        switch (character_class) {
        case 0:
            return !blocked(kKnight);
        case 1:
            return !blocked(kMage);
        case 2:
            return !blocked(kArcher);
        case 3:
            return !blocked(kThief);
        default:
            return true;
        }
    }

    int battle_riding_satiety_scaled_value(int value, int satiety, int max_satiety) {
        if (value <= 0 || satiety <= 0 || max_satiety <= 0) {
            return 0;
        }
        const int bounded = std::clamp(satiety, 0, max_satiety);
        return static_cast<int>((static_cast<std::int64_t>(value) * bounded) / max_satiety);
    }

    void battle_riding_consume_skill_hit(BattleMountRecord& mount) {
        if (mount.satiety > 0) {
            --mount.satiety;
            ++mount.revision;
        }
    }

}  // namespace openkal::server
