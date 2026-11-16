/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/WeaponEnchantment.h"

#include "openkal/server/Combat.h"

#include <algorithm>

namespace openkal::server {

    int enchantment_talisman_grade(int index) {
        if (index < kEnchantmentTalismanIndex ||
            index >= kEnchantmentTalismanIndex + kEnchantmentTalismanCount) {
            return 0;
        }
        return index - kEnchantmentTalismanIndex + 1;
    }

    int enchantment_grade_for_weapon(int grade) {
        if (grade < 40) {
            return 0;
        }
        return std::min(kEnchantmentTalismanCount, grade / 10 - 3);
    }

    bool can_apply_weapon_enchantment(
        int talisman_index, bool is_weapon, bool equipped, int count, int grade) {
        return is_weapon && !equipped && count == 1 && grade >= 40 &&
               enchantment_talisman_grade(talisman_index) == enchantment_grade_for_weapon(grade);
    }

    WeaponEnchantmentRolls roll_weapon_enchantment() {
        WeaponEnchantmentRolls rolls;
        rolls.grade = combat::irand(1, 100);
        for (int& roll : rolls.otp) {
            roll = combat::irand(1, 100);
        }
        for (int& roll : rolls.attack_magic) {
            roll = combat::irand(1, 100);
        }
        return rolls;
    }

    WeaponEnchantmentResult apply_weapon_enchantment(const CharmState& current,
                                                     bool is_wand,
                                                     const WeaponEnchantmentRolls& rolls) {
        WeaponEnchantmentResult result;
        result.state = current;
        auto& state = result.state;
        state.x_attack = state.x_magic = state.x_hit = 0;
        state.upgrade_level = state.upgrade_rate = 0;
        state.enchant_protected = false;
        result.grade_step = rolls.grade <= 10 ? -1 : (rolls.grade <= 15 ? 1 : 0);

        constexpr std::array<int, 5> otp_chances{50, 40, 20, 10, 5};
        for (std::size_t i = 0; i < otp_chances.size(); ++i) {
            if (rolls.otp[i] <= otp_chances[i]) {
                state.x_hit = static_cast<int>(i) + 1;
            }
        }
        constexpr std::array<int, 17> charm_chances{
            100, 90, 80, 70, 60, 50, 40, 30, 20, 18, 16, 14, 12, 10, 6, 2, 1};
        int plus = 0;
        for (std::size_t i = 0; i < charm_chances.size(); ++i) {
            if (rolls.attack_magic[i] <= charm_chances[i]) {
                plus = std::min(16, static_cast<int>(i) + 1);
                state.upgrade_level = i == 16 ? 16 : std::min(10, plus);
            }
        }
        (is_wand ? state.x_magic : state.x_attack) = plus;
        return result;
    }

    bool is_imperial_weapon_name(std::string_view name) {
        constexpr std::string_view word = "imperial";
        const auto letter = [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); };
        for (std::size_t i = 0; i + word.size() <= name.size(); ++i) {
            if ((i > 0 && letter(name[i - 1])) ||
                (i + word.size() < name.size() && letter(name[i + word.size()]))) {
                continue;
            }
            bool matches = true;
            for (std::size_t j = 0; j < word.size(); ++j) {
                char c = name[i + j];
                if (c >= 'A' && c <= 'Z') {
                    c = static_cast<char>(c - 'A' + 'a');
                }
                matches = matches && c == word[j];
            }
            if (matches) {
                return true;
            }
        }
        return false;
    }

    int adjacent_enchantment_weapon(const EnchantmentWeapon& current,
                                    const std::vector<EnchantmentWeapon>& catalog,
                                    int step) {
        static thread_local std::mt19937 random{std::random_device{}()};
        return adjacent_enchantment_weapon(current, catalog, step, random);
    }

    int adjacent_enchantment_weapon(const EnchantmentWeapon& current,
                                    const std::vector<EnchantmentWeapon>& catalog,
                                    int step,
                                    std::mt19937& random) {
        if (step == 0 || current.grade <= 0 || current.kind.empty()) {
            return current.index;
        }
        int next_grade = 0;
        std::vector<int> candidates;
        for (const auto& candidate : catalog) {
            if (candidate.index <= 0 || candidate.grade <= 0 || candidate.kind != current.kind ||
                candidate.class_limit != current.class_limit ||
                candidate.imperial != current.imperial) {
                continue;
            }
            const bool directed =
                step < 0 ? candidate.grade < current.grade : candidate.grade > current.grade;
            if (!directed) {
                continue;
            }
            if (next_grade == 0 ||
                (step < 0 ? candidate.grade > next_grade : candidate.grade < next_grade)) {
                next_grade = candidate.grade;
                candidates.clear();
            }
            if (candidate.grade == next_grade) {
                candidates.push_back(candidate.index);
            }
        }
        if (candidates.empty()) {
            return current.index;
        }
        std::sort(candidates.begin(), candidates.end());
        candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());
        if (candidates.size() == 1) {
            return candidates.front();
        }
        std::uniform_int_distribution<std::size_t> variant(0, candidates.size() - 1);
        return candidates[variant(random)];
    }

}  // namespace openkal::server
