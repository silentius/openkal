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

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace openkal::server {
    /** Accessory families in the inventory's collection drawer. */
    enum class CollectionKind {
        None,
        Mirror,
        Trigram,
        Taegeuk,
        HaninMirror,
        Essence,
        YeoijuAttack,
        YeoijuDefense
    };

    /** Immutable classification of a supported item. */
    struct CollectionDef {
        CollectionKind kind = CollectionKind::None;
        int grade = 0;
        int part = 0;
        bool material = false;
    };

    /** Persistent instance fields already carried by inventory packets and item storage.
     * option/value use random_stat_type/value; absorption uses mix_type.
     */
    struct CollectionItem {
        std::uint32_t iid = 0;
        int index = 0;
        int level = 0;
        int option = 0;
        int value = 0;
        int absorption = 0;
        bool equipped = false;
    };

    /** Number of collection equipment cells (including both mirrors). */
    constexpr int kCollectionSlots = 26;

    /** First of three Yin-Yang Mirror grades. */
    inline constexpr int kYinYangMirrorFirst = 24000;

    /** First of 24 raw trigrams, ordered by grade and position. */
    inline constexpr int kRawTrigramFirst = 24003;

    /** First of 24 enchanted trigrams, ordered by grade and position. */
    inline constexpr int kTrigramFirst = 24027;

    /** First of three Taegeuk grades. */
    inline constexpr int kTaegeukFirst = 24051;

    /** First of three Hanin's Mirror grades. */
    inline constexpr int kHaninMirrorFirst = 24054;

    /** First of three Hanin's Essence grades. */
    inline constexpr int kEssenceFirst = 24057;

    /** Attack Yeoiju item definition index. */
    inline constexpr int kYeoijuAttack = 24060;

    /** Defense Yeoiju item definition index. */
    inline constexpr int kYeoijuDefense = 24061;

    /** Yeoiju fortification material item definition index. */
    inline constexpr int kYeoijuFortification = 24062;

    /** Classifies collection items without relying on translated names.
     * @param index Item definition index.
     * @return Family, grade and trigram position, or None.
     */
    [[nodiscard]] CollectionDef collection_def(int index);

    /** Assigns worn instances to stable slots, sorted by instance id.
     * @param items Inventory instances.
     * @return Instance ids, zero for an empty cell.
     */
    [[nodiscard]] std::array<std::uint32_t, kCollectionSlots>
    collection_slots(const std::vector<CollectionItem>& items);

    /** Validates equip and removal dependencies on the authoritative inventory.
     * @param items Inventory instances.
     * @param item Instance to equip or remove.
     * @param equip True for equip, false for removal.
     * @return Empty on success, otherwise an English explanation.
     */
    [[nodiscard]] std::string collection_wear_error(const std::vector<CollectionItem>& items,
                                                    const CollectionItem& item,
                                                    bool equip);

    /** Checks whether an item belongs in a drawer cell.
     * @param index Item definition index.
     * @param slot Drawer cell index.
     * @return True if the family and position match.
     */
    [[nodiscard]] bool collection_accepts(int index, int slot);

    /** Collection abilities kept separate from ordinary equipment and PvP stats. */
    struct CollectionBonuses {
        /** STR, WIS, INT, HTH, DEX, hit, dodge, defense, resist, absorb,
         * HP%, MP%, speed%, min/max attack%, critical chance/damage%, explosive
         * damage%, min/max magic%, character EXP%, ancient EXP%.
         */
        std::array<double, 22> ability{};
        int hp = 0;
        int mp = 0;
        int attack = 0;
    };

    /** Aggregates active mirror, trigram, Taegeuk and essence abilities.
     * @param items Authoritative inventory instances.
     * @param pvp True to retain only movement speed.
     * @return Aggregated bonuses; Yeoiju PvP attack/defense are deliberately disabled.
     */
    [[nodiscard]] CollectionBonuses collection_bonuses(const std::vector<CollectionItem>& items,
                                                       bool pvp);

    /** Gets the English name of an essence or Taegeuk ability.
     * @param option Persistent random-stat option id (100..121).
     * @return Ability name, or an empty string for invalid ids.
     */
    [[nodiscard]] const char* collection_option_name(int option);

    /** Validates Taegeuk absorption without consuming equipped trigrams.
     * @param items Inventory instances.
     * @param taegeuk Unequipped destination.
     * @return Empty on success, otherwise an explanation.
     */
    [[nodiscard]] std::string collection_absorption_error(const std::vector<CollectionItem>& items,
                                                          const CollectionItem& taegeuk);

    /** Selects a legacy absorption row using a bounded random choice.
     * @param items Inventory instances containing eight worn trigrams.
     * @param taegeuk Destination Taegeuk.
     * @param choice Uniform random integer in [0,7].
     * @return Persistent absorption row (1..184), or zero if validation fails.
     */
    [[nodiscard]] int collection_absorption_roll(const std::vector<CollectionItem>& items,
                                                 const CollectionItem& taegeuk,
                                                 int choice);

    /** Server-owned decimal upgrade probabilities. Missing/invalid files disable upgrades. */
    class CollectionUpgrades {
      public:
        /** Loads all three upgrade tables atomically.
         * @param directory Directory containing trigram.json, hanin.json and yeoiju.json.
         * @return True when every probability is finite and in [0,100].
         */
        [[nodiscard]] bool load(const std::string& directory);

        /** Resolves a destination-level probability.
         * @param kind Accessory family.
         * @param level Destination level, starting at one.
         * @return Percent probability, or zero for an unsupported level.
         */
        [[nodiscard]] double chance(CollectionKind kind, int level) const;

        /** Evaluates a random draw without rounding small chances to integer percent.
         * @param kind Accessory family.
         * @param level Destination level.
         * @param roll Uniform draw in [0,1).
         * @return True if the draw succeeds.
         */
        [[nodiscard]] bool succeeds(CollectionKind kind, int level, double roll) const;

      private:
        std::array<std::vector<double>, 3> m_chances;
    };
}  // namespace openkal::server
