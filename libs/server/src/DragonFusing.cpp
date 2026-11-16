/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/DragonFusing.h"

#include <array>

namespace openkal::server {
    namespace {

        struct WeaponTier {
            int low_percent;
            int high_percent;
            int primary;
            int hit;
            int hp;
            int mp;
        };

        struct ArmorTier {
            int low_defense;
            int high_defense;
            int primary;
        };

        // Values transcribed from https://gameagit.com/kalonline/YonShin.
        constexpr std::array<WeaponTier, 8> kWeaponTiers = {{{1, 2, 1, 2, 30, 10},
                                                             {3, 4, 1, 4, 50, 30},
                                                             {5, 6, 2, 6, 70, 50},
                                                             {7, 8, 2, 8, 90, 70},
                                                             {9, 10, 3, 10, 110, 90},
                                                             {11, 12, 3, 12, 130, 110},
                                                             {13, 14, 5, 15, 200, 150},
                                                             {15, 17, 8, 18, 250, 200}}};
        constexpr std::array<ArmorTier, 8> kArmorTiers = {{{1, 2, 1},
                                                           {3, 4, 1},
                                                           {5, 6, 2},
                                                           {7, 9, 2},
                                                           {11, 14, 3},
                                                           {17, 21, 3},
                                                           {25, 30, 5},
                                                           {35, 40, 5}}};
        constexpr std::array<int, 8> kUpperPrimary = {{1, 1, 2, 2, 3, 3, 5, 7}};
        constexpr std::array<int, 8> kLowerPrimary = {{1, 1, 1, 2, 2, 3, 5, 6}};
        constexpr std::array<int, 8> kTierCumulativeWeights = {{24, 44, 60, 73, 82, 89, 95, 100}};
        constexpr std::array<const char*, 8> kTierNames = {{"Imoogi",
                                                            "Shadow Dragon",
                                                            "Sky Dragon",
                                                            "Ancient Dragon",
                                                            "Storm Dragon",
                                                            "Violent Dragon",
                                                            "Thunder Dragon",
                                                            "Insanity Dragon"}};
        constexpr int kEffectVariantsPerPrimary = 6;
        constexpr int kEffectVariantsPerTier = 5 * kEffectVariantsPerPrimary;

        [[nodiscard]] bool is_upper(std::string_view value) {
            return value == "upperarmor";
        }

        [[nodiscard]] bool is_lower(std::string_view value) {
            return value == "lowerarmor" || value == "pants";
        }

        [[nodiscard]] int
        value_count(int tier_index, bool is_weapon, std::string_view class_small) {
            if (is_weapon) {
                const auto& tier = kWeaponTiers[static_cast<std::size_t>(tier_index)];
                return tier.high_percent - tier.low_percent + 1;
            }
            if (!DragonFusing::is_armor_part(class_small)) {
                return 0;
            }
            const auto& tier = kArmorTiers[static_cast<std::size_t>(tier_index)];
            return tier.high_defense - tier.low_defense + 1;
        }

    }  // namespace

    const char* DragonFusingBonus::tier_name() const {
        return m_tier >= 1 && m_tier <= static_cast<int>(kTierNames.size())
                   ? kTierNames[static_cast<std::size_t>(m_tier - 1)]
                   : "";
    }

    DragonFusingBonus
    DragonFusingBonus::from_effect(int effect_id, bool is_weapon, std::string_view class_small) {
        DragonFusingBonus out;
        if (effect_id < 1 ||
            effect_id > static_cast<int>(kTierNames.size()) * kEffectVariantsPerTier) {
            return out;
        }
        const int zero_based = effect_id - 1;
        const int tier_index = zero_based / kEffectVariantsPerTier;
        const int variant = zero_based % kEffectVariantsPerTier;
        const int value_offset = variant % kEffectVariantsPerPrimary;
        const int count = value_count(tier_index, is_weapon, class_small);
        if (value_offset >= count) {
            return out;
        }
        out.m_tier = tier_index + 1;
        out.m_primary_kind = variant / kEffectVariantsPerPrimary;
        if (is_weapon) {
            const auto& tier = kWeaponTiers[static_cast<std::size_t>(tier_index)];
            out.m_primary_points = tier.primary;
            out.m_attack_percent = tier.low_percent + value_offset;
            out.m_magic_percent = out.m_attack_percent;
            out.m_hp = tier.hp;
            out.m_mp = tier.mp;
            if (class_small == "wand") {
                out.m_dodge = tier.hit;
            } else {
                out.m_hit = tier.hit;
            }
            return out;
        }
        const auto& tier = kArmorTiers[static_cast<std::size_t>(tier_index)];
        out.m_defense = tier.low_defense + value_offset;
        out.m_primary_points =
            is_upper(class_small)
                ? kUpperPrimary[static_cast<std::size_t>(tier_index)]
                : (is_lower(class_small) ? kLowerPrimary[static_cast<std::size_t>(tier_index)]
                                         : tier.primary);
        return out;
    }

    bool DragonFusing::is_armor_part(std::string_view class_small) {
        return class_small == "shield" || class_small == "helmet" || class_small == "hat" ||
               class_small == "upperarmor" || class_small == "lowerarmor" ||
               class_small == "pants" || class_small == "gauntlet" || class_small == "boots";
    }

    DragonFusingApplyResult DragonFusing::can_apply(bool is_weapon,
                                                    bool is_defense,
                                                    std::string_view class_small,
                                                    bool equipped) {
        if (equipped) {
            return DragonFusingApplyResult::Equipped;
        }
        if ((!is_weapon && !(is_defense && is_armor_part(class_small)))) {
            return DragonFusingApplyResult::InvalidTarget;
        }
        return DragonFusingApplyResult::Ok;
    }

    int DragonFusing::roll_effect(int tier_roll,
                                  int primary_roll,
                                  int value_roll,
                                  bool is_weapon,
                                  std::string_view class_small) {
        if (tier_roll < 1 || tier_roll > 100 || primary_roll < 0 || primary_roll > 4 ||
            value_roll < 0 || (!is_weapon && !is_armor_part(class_small))) {
            return 0;
        }
        for (std::size_t tier = 0; tier < kTierCumulativeWeights.size(); ++tier) {
            if (tier_roll > kTierCumulativeWeights[tier]) {
                continue;
            }
            const int count = value_count(static_cast<int>(tier), is_weapon, class_small);
            const int value_offset = value_roll % count;
            return static_cast<int>(tier) * kEffectVariantsPerTier +
                   primary_roll * kEffectVariantsPerPrimary + value_offset + 1;
        }
        return 0;
    }

}  // namespace openkal::server
