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

#include "test_util.h"

#include <algorithm>
#include <array>
#include <vector>

namespace {
    using namespace openkal::server;
    using openkal::test::expect;

    WeaponEnchantmentRolls misses() {
        WeaponEnchantmentRolls rolls;
        rolls.otp.fill(100);
        rolls.attack_magic.fill(100);
        return rolls;
    }

    void verify_rolls() {
        CharmState old;
        old.x_attack = old.x_magic = old.x_hit = 20;
        old.upgrade_level = 20;
        old.upgrade_rate = 7;
        old.cur_end = 12;
        old.max_end = 30;
        old.x_defense = 3;
        old.x_dodge = 2;
        old.enchant_protected = true;
        for (bool wand : {false, true}) {
            const auto base = apply_weapon_enchantment(old, wand, misses()).state;
            expect(base.x_hit == 0 && base.x_attack == (wand ? 0 : 1) &&
                       base.x_magic == (wand ? 1 : 0) && base.upgrade_level == 1 &&
                       base.upgrade_rate == 0 && !base.enchant_protected,
                   "reset old OTP, both charm branches, EB and progress before rolling");
            expect(base.cur_end == 12 && base.max_end == 30 && base.x_defense == 3 &&
                       base.x_dodge == 2,
                   "reroll preserves endurance and unrelated charm fields");
            constexpr std::array<int, 17> chances{
                100, 90, 80, 70, 60, 50, 40, 30, 20, 18, 16, 14, 12, 10, 6, 2, 1};
            for (std::size_t stage = 0; stage < chances.size(); ++stage) {
                for (int roll = 1; roll <= 100; ++roll) {
                    auto rolls = misses();
                    rolls.attack_magic[stage] = roll;
                    const auto result = apply_weapon_enchantment(old, wand, rolls).state;
                    const bool success = roll <= chances[stage];
                    const int plus = success ? std::min(16, static_cast<int>(stage) + 1) : 1;
                    const int eb = success && stage == 16 ? 16 : std::min(10, plus);
                    expect(result.x_attack == (wand ? 0 : plus) &&
                               result.x_magic == (wand ? plus : 0) && result.upgrade_level == eb,
                           "each charm stage uses its exact percentage and paired EB value");
                }
            }
        }
        constexpr std::array<int, 5> otp_chances{50, 40, 20, 10, 5};
        for (std::size_t stage = 0; stage < otp_chances.size(); ++stage) {
            for (int roll = 1; roll <= 100; ++roll) {
                auto rolls = misses();
                rolls.otp[stage] = roll;
                const int expected = roll <= otp_chances[stage] ? static_cast<int>(stage) + 1 : 0;
                expect(apply_weapon_enchantment(old, false, rolls).state.x_hit == expected,
                       "each OTP stage uses its exact percentage after earlier misses");
            }
        }
        auto rolls = misses();
        rolls.otp.fill(1);
        rolls.attack_magic.fill(1);
        auto result = apply_weapon_enchantment(old, false, rolls);
        expect(result.state.x_hit == 5 && result.state.x_attack == 16 &&
                   result.state.upgrade_level == 16,
               "last successful stages win when every stage succeeds");
        rolls.attack_magic.back() = 2;
        result = apply_weapon_enchantment(old, false, rolls);
        expect(result.state.x_attack == 16 && result.state.upgrade_level == 10,
               "2 percent stage gives +16/10 EB and 1 percent stage gives +16/16 EB");
        std::array<int, 3> grade_counts{};
        for (int grade = 1; grade <= 100; ++grade) {
            rolls.grade = grade;
            ++grade_counts[static_cast<std::size_t>(
                apply_weapon_enchantment(old, false, rolls).grade_step + 1)];
        }
        expect(grade_counts == std::array<int, 3>{10, 85, 5},
               "grade roll has exactly 10 percent lower, 5 percent higher, 85 percent unchanged");
        rolls = misses();
        rolls.grade = 1;
        rolls.otp[4] = 5;
        rolls.attack_magic[15] = 2;
        result = apply_weapon_enchantment(old, true, rolls);
        expect(result.grade_step == -1 && result.state.x_hit == 5 && result.state.x_magic == 16 &&
                   result.state.upgrade_level == 10,
               "grade, OTP and magic stages remain independent of one another");
    }

    void verify_talisman_grades() {
        const std::vector<std::pair<int, int>> grades{{0, 0},  {37, 0}, {39, 0}, {40, 1}, {43, 1},
                                                      {46, 1}, {49, 1}, {50, 2}, {53, 2}, {56, 2},
                                                      {59, 2}, {60, 3}, {62, 3}, {69, 3}, {70, 4},
                                                      {75, 4}, {79, 4}, {80, 5}, {85, 5}, {100, 5}};
        for (const auto& entry : grades) {
            expect(enchantment_grade_for_weapon(entry.first) == entry.second,
                   "weapon grade selects its exact talisman band without invented weapon grades");
            for (int tier = 1; tier <= kEnchantmentTalismanCount; ++tier) {
                const int index = kEnchantmentTalismanIndex + tier - 1;
                expect(
                    enchantment_talisman_grade(index) == tier &&
                        can_apply_weapon_enchantment(index, true, false, 1, entry.first) ==
                            (tier == entry.second),
                    "only the matching one of five talismans accepts each weapon-grade boundary");
            }
        }
        for (int index : {0,
                          kEnchantmentTalismanIndex - 1,
                          kEnchantmentTalismanIndex + kEnchantmentTalismanCount}) {
            expect(enchantment_talisman_grade(index) == 0 &&
                       !can_apply_weapon_enchantment(index, true, false, 1, 40),
                   "unrelated item IDs cannot act as an enchantment talisman");
        }
    }

    void verify_high_grade_downgrade() {
        const std::vector<EnchantmentWeapon> weapons{{8000, 75, "sword2h", 0, true},
                                                     {8001, 80, "sword2h", 0, true},
                                                     {8002, 90, "sword2h", 0, false},
                                                     {8003, 75, "sword", 0, true}};
        const auto& current = weapons[1];
        expect(can_apply_weapon_enchantment(kEnchantmentTalismanIndex + 4, true, false, 1, 80),
               "grade V talisman accepts the Imperial G80 weapon before rolling");
        int downgrades = 0;
        int unchanged = 0;
        for (int roll = 1; roll <= 100; ++roll) {
            auto rolls = misses();
            rolls.grade = roll;
            rolls.otp[4] = 5;
            rolls.attack_magic.back() = 1;
            const auto result = apply_weapon_enchantment({}, false, rolls);
            const int replacement =
                adjacent_enchantment_weapon(current, weapons, result.grade_step);
            downgrades += replacement == 8000 ? 1 : 0;
            unchanged += replacement == 8001 ? 1 : 0;
            expect(result.state.x_attack == 16 && result.state.x_hit == 5 &&
                       result.state.upgrade_level == 16,
                   "G80 to G75 grade-band change preserves the newly rolled enchantment stats");
        }
        expect(
            downgrades == 10 && unchanged == 90,
            "G80 to G75 downgrade is exactly 10 percent; missing higher Imperial grade stays G80");
        expect(can_apply_weapon_enchantment(kEnchantmentTalismanIndex + 3, true, false, 1, 75) &&
                   !can_apply_weapon_enchantment(kEnchantmentTalismanIndex + 4, true, false, 1, 75),
               "the downgraded G75 weapon requires grade IV on the next application");
    }

    void verify_grades() {
        std::vector<EnchantmentWeapon> weapons{{100, 37, "sword", 0, false},
                                               {101, 40, "sword", 0, false},
                                               {102, 50, "sword", 0, false},
                                               {103, 53, "sword", 0, false},
                                               {200, 38, "sword", 0, true},
                                               {201, 43, "sword", 0, true},
                                               {202, 55, "sword", 0, true},
                                               {300, 41, "sword2h", 0, false},
                                               {400, 41, "sword", 3, false},
                                               {500, 41, "wand", 1, false}};
        expect(adjacent_enchantment_weapon(weapons[1], weapons, -1) == 100 &&
                   adjacent_enchantment_weapon(weapons[1], weapons, 1) == 102 &&
                   adjacent_enchantment_weapon(weapons[3], weapons, -1) == 102,
               "find actual adjacent loaded grades, including G40 to G37 and G53 to G50");
        auto rolls = misses();
        rolls.grade = 1;
        rolls.otp[4] = 5;
        rolls.attack_magic.back() = 1;
        for (bool wand : {false, true}) {
            const auto enchanted = apply_weapon_enchantment({}, wand, rolls);
            const int index =
                adjacent_enchantment_weapon(weapons[1], weapons, enchanted.grade_step);
            expect(index == 100 && enchanted.state.x_hit == 5 &&
                       (wand ? enchanted.state.x_magic : enchanted.state.x_attack) == 16 &&
                       enchanted.state.upgrade_level == 16 &&
                       !can_apply_weapon_enchantment(
                           kEnchantmentTalismanIndex, true, false, 1, weapons[0].grade),
                   "G40 to G37 downgrade retains newly rolled OTP, attack/magic and EB but blocks "
                   "reuse");
        }
        expect(adjacent_enchantment_weapon(weapons[4], weapons, 1) == 201 &&
                   adjacent_enchantment_weapon(weapons[5], weapons, -1) == 200 &&
                   adjacent_enchantment_weapon(weapons[5], weapons, 1) == 202,
               "Imperial changes stay Imperial even when normal grades lie closer");
        expect(adjacent_enchantment_weapon(weapons[0], weapons, -1) == 100 &&
                   adjacent_enchantment_weapon(weapons[3], weapons, 1) == 103 &&
                   adjacent_enchantment_weapon(weapons[1], weapons, 0) == 101,
               "catalog boundaries and no-change rolls preserve the definition");
        weapons.push_back({104, 50, "sword", 0, false});
        const auto original_order = weapons;
        std::reverse(weapons.begin(), weapons.end());
        std::mt19937 forward_random{42};
        std::mt19937 reverse_random{42};
        bool same_choices = true;
        for (int i = 0; i < 100; ++i) {
            for (int step : {-1, 1}) {
                const auto& current = original_order[step > 0 ? 1 : 3];
                const int forward =
                    adjacent_enchantment_weapon(current, original_order, step, forward_random);
                const int reverse =
                    adjacent_enchantment_weapon(current, weapons, step, reverse_random);
                same_choices =
                    same_choices && forward == reverse && (forward == 102 || forward == 104);
            }
        }
        expect(same_choices, "grade resolution does not depend on unordered catalog iteration");
        expect(is_imperial_weapon_name("Imperial Training Sword") &&
                   is_imperial_weapon_name("Sword (IMPERIAL)") &&
                   !is_imperial_weapon_name("Nonimperial Sword") &&
                   !is_imperial_weapon_name("Training Sword"),
               "named Imperial family uses a case-insensitive whole word");
        expect(
            can_apply_weapon_enchantment(kEnchantmentTalismanIndex, true, false, 1, 40) &&
                can_apply_weapon_enchantment(kEnchantmentTalismanIndex + 1, true, false, 1, 53) &&
                !can_apply_weapon_enchantment(kEnchantmentTalismanIndex, true, false, 1, 37) &&
                !can_apply_weapon_enchantment(kEnchantmentTalismanIndex, true, false, 1, 39) &&
                !can_apply_weapon_enchantment(kEnchantmentTalismanIndex, false, false, 1, 40) &&
                !can_apply_weapon_enchantment(kEnchantmentTalismanIndex, true, true, 1, 40) &&
                !can_apply_weapon_enchantment(kEnchantmentTalismanIndex, true, false, 2, 40) &&
                !can_apply_weapon_enchantment(kEnchantmentTalismanIndex, true, false, 1, 0),
            "only single unequipped weapons of grade 40 or higher accept the talisman");
    }

    void verify_duplicate_grade_chances() {
        std::vector<EnchantmentWeapon> weapons{{8100, 37, "sword", 0, false},
                                               {8101, 40, "sword", 0, false},
                                               {8102, 45, "sword", 0, false},
                                               {8103, 45, "sword", 0, false},
                                               {8104, 41, "sword", 0, true},
                                               {8105, 41, "sword2h", 0, false},
                                               {8106, 41, "sword", 3, false}};
        const EnchantmentWeapon current{8101, 40, "sword", 0, false};
        for (int order = 0; order < 2; ++order) {
            int downgrades = 0;
            int upgrades = 0;
            int unchanged = 0;
            for (int roll = 1; roll <= 100; ++roll) {
                auto rolls = misses();
                rolls.grade = roll;
                const auto result = apply_weapon_enchantment({}, false, rolls);
                const int replacement =
                    adjacent_enchantment_weapon(current, weapons, result.grade_step);
                downgrades += replacement == 8100 ? 1 : 0;
                upgrades += replacement == 8102 || replacement == 8103 ? 1 : 0;
                unchanged += replacement == 8101 ? 1 : 0;
            }
            expect(downgrades == 10 && upgrades == 5 && unchanged == 85,
                   "multiple G45 definitions preserve all G40 downgrade and upgrade outcomes");
            std::reverse(weapons.begin(), weapons.end());
        }
    }

    void verify_variant_distribution() {
        const std::vector<EnchantmentWeapon> weapons{{9000, 40, "sword", 0, false},
                                                     {9001, 37, "sword", 0, false},
                                                     {9002, 37, "sword", 0, false},
                                                     {9003, 45, "sword", 0, false},
                                                     {9004, 45, "sword", 0, false},
                                                     {9005, 45, "sword", 0, false},
                                                     {9003, 45, "sword", 0, false},
                                                     {9006, 50, "sword", 0, false},
                                                     {9007, 41, "sword", 0, true},
                                                     {9008, 39, "sword", 0, true},
                                                     {9009, 41, "sword2h", 0, false},
                                                     {9010, 41, "sword", 3, false}};
        for (int step : {-1, 1}) {
            std::mt19937 random{20261024};
            std::array<int, 3> counts{};
            bool eligible_only = true;
            const int first = step < 0 ? 9001 : 9003;
            const int variants = step < 0 ? 2 : 3;
            constexpr int trials = 30000;
            for (int i = 0; i < trials; ++i) {
                const int result = adjacent_enchantment_weapon(weapons[0], weapons, step, random);
                if (result >= first && result < first + variants) {
                    ++counts[static_cast<std::size_t>(result - first)];
                } else {
                    eligible_only = false;
                }
            }
            expect(eligible_only,
                   "variant draws exclude Imperial, wrong subtype/class and more distant grades");
            const int expected = trials / variants;
            bool balanced = true;
            for (int i = 0; i < variants; ++i) {
                const int count = counts[static_cast<std::size_t>(i)];
                balanced = balanced && count > expected * 95 / 100 && count < expected * 105 / 100;
            }
            expect(balanced, "each distinct variant has equal weight for upgrades and downgrades");
        }
    }
}  // namespace

int main() {
    verify_rolls();
    verify_grades();
    verify_duplicate_grade_chances();
    verify_variant_distribution();
    verify_high_grade_downgrade();
    verify_talisman_grades();
    return openkal::test::g_failures == 0 ? 0 : 1;
}
