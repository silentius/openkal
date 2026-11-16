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
#include <string>
#include <string_view>

namespace openkal::server {

    /** Definition ID reserved for the OpenKal Polishing Stone. */
    inline constexpr int kPolishingStoneIndex = 23017;

    /** Definition ID reserved for the consumable Revision Hammer. */
    inline constexpr int kRevisionHammerIndex = 23021;

    /** Calculates the Revision Hammer reward: weapons 4 + roll, armor 2 + roll, accessories 1.
     * @param is_weapon True for a weapon; false for armor or accessories.
     * @param is_ornament True for a ring or trinket, which always yields one stone.
     * @param grade Item grade; ungraded weapons and armor yield no stones.
     * @param extra Inclusive 0..grade/10 roll, clamped to that range.
     * @return Number of Stones of Revision awarded by a valid conversion.
     */
    [[nodiscard]] int
    revision_hammer_stones(bool is_weapon, bool is_ornament, int grade, int extra);

    /** Checks whether a single unequipped item can be converted into Revision Stones.
     * @param is_weapon True for weapons.
     * @param is_defense True for armor and shields.
     * @param is_ornament True for accessories.
     * @param class_small Equipment subtype; only rings and trinkets accept accessory conversion.
     * @param equipped True while worn.
     * @param count Target stack size.
     * @param grade Item grade for weapon and armor rewards; accessories always give one stone.
     * @return True when consuming one hammer and the target is allowed.
     */
    [[nodiscard]] bool can_apply_revision_hammer(bool is_weapon,
                                                 bool is_defense,
                                                 bool is_ornament,
                                                 std::string_view class_small,
                                                 bool equipped,
                                                 int count,
                                                 int grade);

    /** Checks whether a stone can protect this single, unequipped weapon or armor.
     * @param is_weapon True for weapons.
     * @param is_defense True for armor and shields.
     * @param equipped True while worn.
     * @param count Target stack size.
     * @param protected_item True if protection already exists.
     * @return True when applying one stone is allowed.
     */
    [[nodiscard]] bool can_apply_polishing_stone(
        bool is_weapon, bool is_defense, bool equipped, int count, bool protected_item);

    /** Combat bonuses loaded from a prefix JSON definition. */
    struct PrefixCombatDef {
        /** Prefix index. */
        int index = 0;

        /** Strength point bonus. */
        int str = 0;

        /** Health point bonus. */
        int hth = 0;

        /** Intelligence point bonus. */
        int intl = 0;

        /** Wisdom point bonus. */
        int wis = 0;

        /** Dexterity point bonus. */
        int dex = 0;

        /** Flat HP bonus applied after the HP formula. */
        int hp = 0;

        /** Flat MP bonus applied after the MP formula. */
        int mp = 0;

        /** Hit bonus. */
        int hit = 0;

        /** Minimum physical attack bonus. */
        int min_attack = 0;

        /** Maximum physical attack bonus. */
        int max_attack = 0;

        /** Minimum magical attack bonus. */
        int min_magic = 0;

        /** Maximum magical attack bonus. */
        int max_magic = 0;

        /** Defense bonus. */
        int defense = 0;

        /** Absorb bonus. */
        int absorb = 0;

        /** Fire resist bonus. */
        int res_fire = 0;

        /** Ice resist bonus. */
        int res_ice = 0;

        /** Lightning resist bonus. */
        int res_lightning = 0;

        /** Curse resist bonus. */
        int res_curse = 0;

        /** Palsy resist bonus. */
        int res_palsy = 0;

        /**
         * Adds primary-stat bonuses before apply_stat_revise.
         * @param str Strength accumulator.
         * @param hth Health accumulator.
         * @param intl Intelligence accumulator.
         * @param wis Wisdom accumulator.
         * @param dex Dexterity accumulator.
         */
        void apply_primaries(int& str, int& hth, int& intl, int& wis, int& dex) const;

        /**
         * Adds derived combat bonuses after revise and gear.
         * @param hit Hit accumulator.
         * @param min_attack Minimum physical attack.
         * @param max_attack Maximum physical attack.
         * @param min_magic Minimum magical attack.
         * @param max_magic Maximum magical attack.
         * @param defense Defense accumulator.
         * @param absorb Absorb accumulator.
         * @param res_fire Fire resist.
         * @param res_ice Ice resist.
         * @param res_lightning Lightning resist.
         * @param res_curse Curse resist.
         * @param res_palsy Palsy resist.
         */
        void apply_derived(int& hit,
                           int& min_attack,
                           int& max_attack,
                           int& min_magic,
                           int& max_magic,
                           int& defense,
                           int& absorb,
                           int& res_fire,
                           int& res_ice,
                           int& res_lightning,
                           int& res_curse,
                           int& res_palsy) const;
    };

    /**
     * Rolls compatibility behavior (RANDOM 1..100, cumulative rates).
     * @param current Prefix currently on the item, or 0.
     * @param rate0 First cumulative success ceiling.
     * @param prefix0 Prefix applied when roll <= rate0.
     * @param rate1 Second cumulative success ceiling.
     * @param prefix1 Prefix applied when rate0 < roll <= rate1.
     * @param roll Inclusive 1..100 roll.
     * @return New prefix index (unchanged when the roll misses both rates).
     */
    [[nodiscard]] int
    roll_change_prefix(int current, int rate0, int prefix0, int rate1, int prefix1, int roll);

    /**
     * Matches a changeprefix JSON target class against an item.
     * @param target Lowercase class string (weapon / defense / ornament).
     * @param is_weapon True when the destination is a weapon.
     * @param is_defense True when the destination is armor or a shield.
     * @param is_ornament True when the destination is a ring, trinket, or necklace.
     * @return True when the talisman may enchant that item class.
     */
    [[nodiscard]] bool prefix_target_matches(std::string_view target,
                                             bool is_weapon,
                                             bool is_defense,
                                             bool is_ornament);

    /**
     * True when the item has a destroyable endurance pool.
     * Compatibility behavior never copies item definition endurance, so rings, belts,
     * and necklaces cannot be damaged or destroyed by failed prefix.
     * @param is_ornament True when the destination class is ornament.
     * @param endurance item definition endurance, or leftover instance max.
     * @return False for every ornament, even when JSON still lists endurance.
     */
    [[nodiscard]] bool item_uses_endurance(bool is_ornament, int endurance);

    /** Endurance after a failed ChangePrefix roll. */
    struct PrefixFailEndurance {
        /** True when current endurance fell below 1 and the item is deleted. */
        bool destroyed = false;

        /** Current endurance after the cut, or 0 when destroyed. */
        int cur_end = 0;
    };

    /**
     * Applies failure damage for a prefix-enchantment attempt.
     * compatibility behavior only sends MSG_ENCHANTFAILED.
     * @param is_ornament True when the destination is a ring, trinket, or necklace.
     * @param cur_end Current endurance before the fail.
     * @param max_end Max endurance; 0 means the item has no pool.
     * @param enchant_protected True to prevent all failure damage.
     * @param cut Amount subtracted from current endurance (OpenKal uses 0..5).
     * @return Unchanged endurance for ornaments; otherwise the cut result.
     */
    [[nodiscard]] PrefixFailEndurance apply_prefix_fail_endurance(
        bool is_ornament, int cur_end, int max_end, int cut, bool enchant_protected = false);

    /** item definition index of Stone of Revision given when an enchanted item is destroyed. */
    inline constexpr int kRevisionStoneIndex = 517;

    /** Doggebi fixing tool destroys the target when current endurance is below this. */
    inline constexpr int kDoggebiDestroyBelow = 4;

    /**
     * Stones of Revision granted when a graded item is destroyed by a failed
     * enchant. Formula is 1 + roll, with roll in [0, grade/10].
     * @param grade Item grade (item definition level). G40 yields 1..5.
     * @param extra Inclusive 0..grade/10 roll.
     * @return Stone count, or 0 when grade is 0.
     */
    [[nodiscard]] int revision_stones_for_grade(int grade, int extra);

    /** Outcome of dropping a repair tool onto gear. */
    enum class RepairOutcome : int {
        Rejected = 0,
        Repaired = 1,
        Destroyed = 2,
    };

    /** Current and max endurance after a repair attempt. */
    struct RepairState {
        /** What the tool did. */
        RepairOutcome outcome = RepairOutcome::Rejected;

        /** Current endurance after the attempt. */
        int cur_end = 0;

        /** Max endurance after the attempt. */
        int max_end = 0;
    };

    /**
     * Applies specialty.repair [mode, amount] from item JSON.
     * Mode 0 adds amount to current endurance (capped at max). Mode 1 is Doggebi:
     * destroy when cur < 4, otherwise reduce max by amount (floor 1) and fully
     * restore current endurance.
     * @param mode Repair JSON first value.
     * @param amount Repair JSON second value.
     * @param cur_end Current endurance.
     * @param max_end Max endurance.
     * @return Resulting endurance and whether the item survived.
     */
    [[nodiscard]] RepairState apply_repair(int mode, int amount, int cur_end, int max_end);

    /** specialty.charming target. */
    enum class CharmKind : int {
        None = 0,
        XAttack,
        XMagic,
        XHit,
        XDefense,
        XDodge,
    };

    /** Per-instance talisman and Explosive Blow values. */
    struct CharmState {
        /** Talisman of Attack plus. */
        int x_attack = 0;

        /** Talisman of Magic plus. */
        int x_magic = 0;

        /** Talisman of Accuracy plus. */
        int x_hit = 0;

        /** Talisman of Defense plus. */
        int x_defense = 0;

        /** Talisman of Evasion plus. */
        int x_dodge = 0;

        /** Explosive Blow level. */
        int upgrade_level = 0;

        /** Stone of Revision count applied at the current EB level. */
        int upgrade_rate = 0;

        /** Current endurance. */
        int cur_end = 0;

        /** Max endurance. */
        int max_end = 0;

        /** One protected attempt remains; accepted rolls consume it. */
        bool enchant_protected = false;
    };

    /** Outcome of dropping a charming talisman onto gear. */
    enum class CharmOutcome : int {
        Rejected = 0,
        Success = 1,
        FailDamaged = 2,
        FailDestroyed = 3,
        FailProtected = 4,
    };

    /** Result of one charming attempt. */
    struct CharmRollResult {
        /** What happened. */
        CharmOutcome outcome = CharmOutcome::Rejected;

        /** Item fields after the attempt. */
        CharmState state;

        /** Stones of Revision granted when the item is destroyed. */
        int stones = 0;
    };

    /** Gear flags used to validate a talisman target. */
    struct CharmTarget {
        /** True when the destination is a weapon. */
        bool is_weapon = false;

        /** True when class_small is wand (stick). */
        bool is_wand = false;

        /** True when the destination is armor or a shield. */
        bool is_defense = false;

        /** True when class_small is shield. */
        bool is_shield = false;

        /** True while ITEM_PUTON. */
        bool equipped = false;

        /** item definition level / grade. */
        int grade = 0;

        /** item definition sell price (armor destroy stones). */
        int sell = 0;
    };

    /**
     * Parses specialty.charming JSON tokens.
     * @param token Lowercase charming name.
     * @return Kind, or None.
     */
    [[nodiscard]] CharmKind parse_charm_kind(std::string_view token);

    /**
     * True when the talisman may enchant that destination.
     * @param kind Charm type.
     * @param target Destination flags.
     * @return True when class and grade allow the attempt.
     */
    [[nodiscard]] bool charm_target_matches(CharmKind kind, const CharmTarget& target);

    /**
     * Attack/magic plus converted to combat points (ApplySpec).
     * @param x Plus value 0..20.
     * @return x * (2*x + 7) / 9.
     */
    [[nodiscard]] int charm_bonus_attack(int x);

    /**
     * Success ceiling in 1..10000000 for the current plus.
     * @param kind Charm type.
     * @param current Plus before the attempt.
     * @return Rate compared against RANDOM(1, 10000000).
     */
    [[nodiscard]] int charm_success_rate(CharmKind kind, int current);

    /**
     * Applies one prefix-charm roll.
     * @param kind Charm type.
     * @param in Current item fields.
     * @param target Destination flags.
     * @param success_roll Inclusive 1..10000000.
     * @param destroy_roll Inclusive 1..100 (fail branch: >90 destroys endurance).
     * @param cut_roll Inclusive 1..100 (fail branch endurance cut).
     * @return Updated fields and stones when destroyed.
     */
    [[nodiscard]] CharmRollResult apply_charm(CharmKind kind,
                                              const CharmState& in,
                                              const CharmTarget& target,
                                              int success_roll,
                                              int destroy_roll,
                                              int cut_roll);

    /**
     * Explosive Blow success percent after stones (capped at 100).
     * @param upgrade_level Current EB level.
     * @param upgrade_rate Stones applied at this level.
     * @return 0..100.
     */
    [[nodiscard]] int explosive_blow_chance(int upgrade_level, int upgrade_rate);

    /**
     * Stones of Revision consumed by one RateUp click.
     * @param upgrade_level Current EB level.
     * @return Stack count.
     */
    [[nodiscard]] int explosive_blow_stone_cost(int upgrade_level);

    /**
     * Gold charged for one Explosive Blow click.
     * @param upgrade_level Current EB level.
     * @return 1500 * (level + 1).
     */
    [[nodiscard]] int explosive_blow_gold_cost(int upgrade_level);

    /**
     * Max Explosive Blow for a weapon (fixed operator-precedence bug).
     * @param is_wand True for sticks.
     * @param x_attack Attack plus.
     * @param x_magic Magic plus.
     * @return Cap.
     */
    [[nodiscard]] int explosive_blow_cap(bool is_wand, int x_attack, int x_magic);

    /** Result of UI_RATEUP. */
    enum class UpgradeRateOutcome : int {
        Rejected = 0,
        Applied = 1,
    };

    /**
     * Applies Stone of Revision (UpgradeRateUp). Chance must be below 100.
     * @param in Current fields.
     * @param target Weapon flags.
     * @param stones_owned Stones of Revision in the bag.
     * @param stones_consumed Out stones removed on Applied.
     * @return Applied when stones were available.
     */
    [[nodiscard]] UpgradeRateOutcome try_upgrade_rate_up(CharmState& in,
                                                         const CharmTarget& target,
                                                         int stones_owned,
                                                         int& stones_consumed);

    /** Result of UI_LEVEL after the success/fail rolls. */
    enum class UpgradeLevelOutcome : int {
        Rejected = 0,
        Success = 1,
        FailKeep = 2,
        FailDowngrade = 3,
        Destroyed = 4,
    };

    /**
     * Resolves Explosive Blow (UpgradeLevel) with fail bands.
     * Uses upgrade_rate (not upgrade_level) in the chance formula.
     * @param in Current fields.
     * @param target Weapon flags.
     * @param gold_owned Player gold.
     * @param success_roll Inclusive 1..100 compared against chance.
     * @param fail_roll Inclusive 1..100 used when the attempt misses.
     * @param gold_spent Out gold to deduct (0 when rejected).
     * @return Outcome; Destroyed leaves in unchanged aside from the caller's delete.
     */
    [[nodiscard]] UpgradeLevelOutcome apply_upgrade_level(CharmState& in,
                                                          const CharmTarget& target,
                                                          std::int64_t gold_owned,
                                                          int success_roll,
                                                          int fail_roll,
                                                          int& gold_spent);

    /**
     * Stones granted when a charmed weapon is destroyed.
     * @param grade Item grade.
     * @param is_wand True for sticks.
     * @param x_attack Attack plus.
     * @param x_magic Magic plus.
     * @return Stone count.
     */
    [[nodiscard]] int
    charm_destroy_weapon_stones(int grade, bool is_wand, int x_attack, int x_magic);

    /**
     * Stones granted when charmed armor is destroyed.
     * @param grade Item grade.
     * @param is_shield True for shields.
     * @param x_defense Defense plus.
     * @param sell item definition sell price.
     * @return Stone count.
     */
    [[nodiscard]] int
    charm_destroy_defense_stones(int grade, bool is_shield, int x_defense, int sell);

    /**
     * Applies equipped charm pluses to combat accumulators.
     * @param state Item charm fields.
     * @param is_weapon True for weapons.
     * @param is_defense True for armor.
     * @param min_attack Physical min.
     * @param max_attack Physical max.
     * @param min_magic Magical min.
     * @param max_magic Magical max.
     * @param hit Hit.
     * @param dodge Dodge.
     * @param defense Melee defense.
     * @param defense_range Ranged defense.
     */
    void apply_equipped_charm(const CharmState& state,
                              bool is_weapon,
                              bool is_defense,
                              int& min_attack,
                              int& max_attack,
                              int& min_magic,
                              int& max_magic,
                              int& hit,
                              int& dodge,
                              int& defense,
                              int& defense_range);

}  // namespace openkal::server
