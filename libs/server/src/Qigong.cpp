/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/Qigong.h"

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

        // Values transcribed from https://gameagit.com/kalonline/Qigong.
        constexpr std::array<WeaponTier, 8> kWeaponTiers = {{{2, 3, 2, 2, 30, 10},
                                                             {4, 5, 2, 4, 50, 30},
                                                             {6, 7, 3, 6, 70, 50},
                                                             {8, 9, 3, 8, 90, 70},
                                                             {10, 11, 5, 10, 110, 90},
                                                             {12, 13, 5, 12, 130, 110},
                                                             {14, 15, 6, 15, 200, 150},
                                                             {18, 19, 8, 18, 250, 200}}};
        constexpr std::array<ArmorTier, 8> kArmorTiers = {{{6, 8, 1},
                                                           {10, 12, 1},
                                                           {14, 16, 2},
                                                           {18, 21, 2},
                                                           {24, 28, 3},
                                                           {32, 36, 3},
                                                           {40, 45, 4},
                                                           {50, 60, 5}}};
        constexpr std::array<int, 8> kUpperPrimary = {{1, 1, 2, 2, 3, 3, 5, 7}};
        constexpr std::array<int, 8> kLowerPrimary = {{1, 1, 1, 2, 2, 3, 5, 6}};
        // Tier probabilities: 24%, 20%, 16%, 13%, 9%, 7%, 6%, and 5%.
        constexpr std::array<int, 8> kTierCumulativeWeights = {{24, 44, 60, 73, 82, 89, 95, 100}};
        constexpr std::array<const char*, 8> kTierNames = {{"Black Imoogi",
                                                            "Black Shadow Dragon",
                                                            "Black Sky Dragon",
                                                            "Black Ancient Dragon",
                                                            "Black Storm Dragon",
                                                            "Black Violent Dragon",
                                                            "Black Thunder Dragon",
                                                            "Black Insanity Dragon"}};

        [[nodiscard]] bool is_upper(std::string_view value) {
            return value == "upperarmor";
        }

        [[nodiscard]] bool is_lower(std::string_view value) {
            return value == "lowerarmor" || value == "pants";
        }

    }  // namespace

    const char* QigongBonus::tier_name() const {
        return m_tier >= 1 && m_tier <= static_cast<int>(kTierNames.size())
                   ? kTierNames[static_cast<std::size_t>(m_tier - 1)]
                   : "";
    }

    QigongBonus
    QigongBonus::from_effect(int effect_id, bool is_weapon, std::string_view class_small) {
        QigongBonus out;
        if (effect_id < 1 || effect_id > 80) {
            return out;
        }
        const int zero_based = effect_id - 1;
        const int tier_index = zero_based / 10;
        const bool high_value = ((zero_based / 5) % 2) != 0;
        out.m_tier = tier_index + 1;
        out.m_primary_kind = zero_based % 5;
        if (is_weapon) {
            const WeaponTier& tier = kWeaponTiers[static_cast<std::size_t>(tier_index)];
            out.m_primary_points = tier.primary;
            out.m_attack_percent = high_value ? tier.high_percent : tier.low_percent;
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
        if (!Qigong::is_armor_part(class_small)) {
            return {};
        }
        const ArmorTier& tier = kArmorTiers[static_cast<std::size_t>(tier_index)];
        out.m_defense = high_value ? tier.high_defense : tier.low_defense;
        out.m_primary_points =
            is_upper(class_small)
                ? kUpperPrimary[static_cast<std::size_t>(tier_index)]
                : (is_lower(class_small) ? kLowerPrimary[static_cast<std::size_t>(tier_index)]
                                         : tier.primary);
        return out;
    }

    bool Qigong::is_armor_part(std::string_view class_small) {
        return class_small == "shield" || class_small == "helmet" || class_small == "hat" ||
               class_small == "upperarmor" || class_small == "lowerarmor" ||
               class_small == "pants" || class_small == "gauntlet" || class_small == "boots";
    }

    QigongApplyResult Qigong::can_perforate(bool is_weapon,
                                            bool is_defense,
                                            std::string_view class_small,
                                            int grade,
                                            bool equipped,
                                            bool perforated) {
        if (equipped) {
            return QigongApplyResult::Equipped;
        }
        if ((!is_weapon && !(is_defense && is_armor_part(class_small))) || grade < 40) {
            return QigongApplyResult::InvalidTarget;
        }
        return perforated ? QigongApplyResult::AlreadyPerforated : QigongApplyResult::Ok;
    }

    QigongApplyResult Qigong::can_apply(bool is_weapon,
                                        bool is_defense,
                                        std::string_view class_small,
                                        int grade,
                                        bool equipped,
                                        bool perforated) {
        const QigongApplyResult result =
            can_perforate(is_weapon, is_defense, class_small, grade, equipped, false);
        if (result != QigongApplyResult::Ok) {
            return result;
        }
        return perforated ? QigongApplyResult::Ok : QigongApplyResult::NotPerforated;
    }

    bool Qigong::perforation_succeeds(int roll) {
        return roll >= 1 && roll <= perforation_success_rate();
    }

    int Qigong::roll_effect(int tier_roll, int variant_roll) {
        if (tier_roll < 1 || tier_roll > 100 || variant_roll < 0 || variant_roll > 9) {
            return 0;
        }
        for (std::size_t tier = 0; tier < kTierCumulativeWeights.size(); ++tier) {
            if (tier_roll <= kTierCumulativeWeights[tier]) {
                return static_cast<int>(tier) * 10 + variant_roll + 1;
            }
        }
        return 0;
    }

}  // namespace openkal::server
