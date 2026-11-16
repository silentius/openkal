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

#include "openkal/server/PrefixEnchant.h"

#include <array>
#include <random>
#include <string_view>
#include <vector>

namespace openkal::server {

    /** First definition ID for the five Talismans of Enchantment, ordered I through V. */
    inline constexpr int kEnchantmentTalismanIndex = 23022;

    /** Number of weapon-grade-specific enchantment talismans. */
    inline constexpr int kEnchantmentTalismanCount = 5;

    /** Resolves an enchantment talisman's grade from its definition ID.
     * @param index Item definition ID.
     * @return Grade 1..5, or zero for unrelated items.
     */
    [[nodiscard]] int enchantment_talisman_grade(int index);

    /** Resolves the talisman grade required by a weapon before rerolling.
     * @param grade Current weapon grade.
     * @return 1 for G40..49, 2 for G50..59, 3 for G60..69, 4 for G70..79, 5 for G80+;
     * zero below G40.
     */
    [[nodiscard]] int enchantment_grade_for_weapon(int grade);

    /** Independent inclusive 1..100 rolls, evaluated in the listed stage order. */
    struct WeaponEnchantmentRolls {
        /** Grade roll: 1..10 lowers, 11..15 raises, 16..100 retains the grade. */
        int grade = 100;

        /** Rolls for OTP +1 through +5. */
        std::array<int, 5> otp{};

        /** Rolls for +1 through +16/10 EB, followed by +16/16 EB. */
        std::array<int, 17> attack_magic{};
    };

    /** Result of resetting and rerolling one weapon's enchantments. */
    struct WeaponEnchantmentResult {
        /** Updated charm fields, preserving endurance and unrelated bonuses. */
        CharmState state;

        /** Requested adjacent grade direction: -1, 0 or 1. */
        int grade_step = 0;
    };

    /** Minimal catalog entry used to find adjacent weapon grades. */
    struct EnchantmentWeapon {
        /** Item definition ID. */
        int index = 0;

        /** Item grade, from the definition's level. */
        int grade = 0;

        /** Weapon subtype, including sword2h as a separate family. */
        std::string_view kind;

        /** Class restriction; -1 means unrestricted. */
        int class_limit = -1;

        /** True for the Imperial weapon family. */
        bool imperial = false;
    };

    /** Checks whether this target accepts a Talisman of Enchantment.
     * @param talisman_index Source talisman definition ID.
     * @param is_weapon True for weapon definitions.
     * @param equipped True while the target is worn.
     * @param count Target stack size.
     * @param grade Item grade; targets below grade 40 are rejected.
     * @return True for a matching talisman and a single unequipped weapon of grade 40 or higher.
     */
    [[nodiscard]] bool can_apply_weapon_enchantment(
        int talisman_index, bool is_weapon, bool equipped, int count, int grade);

    /** Generates separate random draws for every enchantment stage.
     * @return Inclusive 1..100 rolls for grade, all OTP stages and all charm stages.
     */
    [[nodiscard]] WeaponEnchantmentRolls roll_weapon_enchantment();

    /** Resets OTP, attack, magic, EB and revision progress before independent stage rolls.
     * Later successful stages replace earlier successes, even after a failed stage.
     * @param current Existing charm fields; unrelated fields are preserved.
     * @param is_wand True to apply magic; false to apply physical attack.
     * @param rolls Independent inclusive 1..100 rolls in stage order.
     * @return Rerolled charm fields and requested grade direction.
     */
    [[nodiscard]] WeaponEnchantmentResult apply_weapon_enchantment(
        const CharmState& current, bool is_wand, const WeaponEnchantmentRolls& rolls);

    /** Recognizes the whole word Imperial in a case-insensitive definition name.
     * @param name Item definition name.
     * @return True when the name identifies an Imperial weapon.
     */
    [[nodiscard]] bool is_imperial_weapon_name(std::string_view name);

    /** Finds the nearest loaded grade in the same subtype, class and Imperial family.
     * Distinct matching definitions at that grade have equal selection chances.
     * @param current Source weapon catalog entry.
     * @param catalog Loaded weapon definitions.
     * @param step Negative for lower, positive for higher, zero for no change.
     * @return Replacement ID; current ID when no matching grade exists in the requested direction.
     */
    [[nodiscard]] int adjacent_enchantment_weapon(const EnchantmentWeapon& current,
                                                  const std::vector<EnchantmentWeapon>& catalog,
                                                  int step);

    /** Selects an adjacent weapon using a caller-owned random engine.
     * @param current Source weapon catalog entry.
     * @param catalog Loaded weapon definitions.
     * @param step Negative for lower, positive for higher, zero for no change.
     * @param random Random engine used to choose uniformly among matching variants.
     * @return Replacement ID; current ID when no matching grade exists in the requested direction.
     */
    [[nodiscard]] int adjacent_enchantment_weapon(const EnchantmentWeapon& current,
                                                  const std::vector<EnchantmentWeapon>& catalog,
                                                  int step,
                                                  std::mt19937& random);

}  // namespace openkal::server
