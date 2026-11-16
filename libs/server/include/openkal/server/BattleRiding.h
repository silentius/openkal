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

#include "openkal/protocol/Riding.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace openkal::server {

    /** Number of persistent Battle Riding equipment slots. */
    inline constexpr std::size_t kBattleRidingEquipmentSlots = 5;

    /** Maximum number of rolled Battle Riding abilities. */
    inline constexpr std::size_t kBattleRidingAbilitySlots = 6;

    /** Equipment positions that belong to a Battle Riding Pet rather than the player. */
    enum class BattleRidingEquipmentSlot : std::uint8_t {
        Helmet = 0,
        Armor = 1,
        Saddle = 2,
        SaddleCloth = 3,
        Magnet = 4,
    };

    /** Persistent, per-item-instance Battle Riding state. */
    struct BattleMountRecord {
        std::int32_t mount_iid = 0;
        protocol::BattleRidingGrade grade = protocol::BattleRidingGrade::Normal;
        int satiety = 100;
        int max_satiety = 100;
        std::uint32_t revision = 0;
        std::array<int, kBattleRidingAbilitySlots> abilities{};
        std::array<std::int32_t, kBattleRidingEquipmentSlots> equipment_iids{};
    };

    /** Maps a Battle Riding grade to the existing ItemRarity value.
     * @param grade Battle Riding grade.
     * @return Existing ItemRarity numeric value.
     */
    [[nodiscard]] std::uint8_t battle_riding_grade_rarity(protocol::BattleRidingGrade grade);

    /** Returns the English display name for a Battle Riding grade.
     * @param grade Battle Riding grade.
     * @return Stable English display name.
     */
    [[nodiscard]] std::string_view battle_riding_grade_name(protocol::BattleRidingGrade grade);

    /** Returns the number of initially available ability slots for a grade.
     * @param grade Battle Riding grade.
     * @return Initial slot count.
     */
    [[nodiscard]] int battle_riding_initial_ability_slots(protocol::BattleRidingGrade grade);

    /** Returns the configured grade-combination success percentage.
     * @param grade Current Battle Riding grade.
     * @return Success percentage, or zero when no higher grade exists.
     */
    [[nodiscard]] int battle_riding_combine_chance(protocol::BattleRidingGrade grade);

    /** Returns whether a class skill is available while mounted.
     * @param grade Current Battle Riding grade.
     * @param character_class OpenKal character class index.
     * @param skill_index Skill index.
     * @return True when the skill may be used while mounted.
     */
    [[nodiscard]] bool battle_riding_skill_allowed(protocol::BattleRidingGrade grade,
                                                   int character_class,
                                                   int skill_index);

    /** Scales a mounted equipment value linearly with current satiety.
     * @param value Full-satiety equipment value.
     * @param satiety Current satiety.
     * @param max_satiety Maximum satiety.
     * @return Scaled mounted value.
     */
    [[nodiscard]] int battle_riding_satiety_scaled_value(int value, int satiety, int max_satiety);

    /** Applies one successful skill hit to satiety and advances the record revision.
     * @param mount Mutable per-IID Battle Riding record.
     */
    void battle_riding_consume_skill_hit(BattleMountRecord& mount);

}  // namespace openkal::server
