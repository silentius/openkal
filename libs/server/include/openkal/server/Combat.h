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

#include "openkal/protocol/ArcherSkill.h"

#include <cstdint>
#include <limits>

namespace openkal::server {
    namespace combat {

        /** Knight class index. */
        inline constexpr int kPcKnight = 0;

        /** Mage class index. */
        inline constexpr int kPcMage = 1;

        /** Archer class index. */
        inline constexpr int kPcArcher = 2;

        /** Thief class index. */
        inline constexpr int kPcThief = 3;

        /** Knight Defense (compatibility behavior) skill index. */
        inline constexpr int kKnightDefenseSkill = 6;

        /** Knight Parry skill index (2H block gate). */
        inline constexpr int kKnightParrySkill = 20;

        /** Knight Weapon Defend skill index. */
        inline constexpr int kKnightWeaponDefendSkill = 37;

        /** E_JCAB_2ND_ATTRIB_JOB_1 — 2H parry requires second-job path A. */
        inline constexpr unsigned kSecondJobAttrib1 = 1u << 2;

        /**
         * g_nReviseBlock[type][colorLevel].
         * type 0 shield, 1 two-hand, 2 archer.
         */
        inline constexpr int kReviseBlock[3][7] = {
            {30, 21, 15, 12, 9, 5, 1},
            {30, 21, 15, 12, 9, 5, 1},
            {42, 26, 17, 14, 11, 6, 2},
        };

        /** compatibility behavior — 0 melee, 1 ranged (archer only). */
        inline constexpr int kAttackMelee = 0;

        /** compatibility behavior for archers. */
        inline constexpr int kAttackRange = 1;

        /** compatibility behavior extra vs players: level / g_denoDef[type][class]. */
        inline constexpr int kDenoDef[2][3] = {
            {4, 4, 5},
            {1, 5, 3},
        };

        /** compatibility behavior extra when the attacker is a player. */
        inline constexpr int kPvpAbsorbBonus = 15;

        /** Returns compatibility behavior (1 for archer, else 0).
         * @param clazz Character class index.
         * @return Attack type used to pick target defense.
         */
        [[nodiscard]] inline int player_attack_type(int clazz) {
            return clazz == kPcArcher ? kAttackRange : kAttackMelee;
        }

        /** Clamps a combat result to a non-negative int32 storage value.
         * @param value Signed intermediate.
         * @return 0 when value is negative; INT32_MAX when it overflows; else value.
         */
        [[nodiscard]] inline std::int32_t clamp_nonneg_i32(std::int64_t value) {
            if (value <= 0) {
                return 0;
            }
            constexpr std::int64_t kMax = std::numeric_limits<std::int32_t>::max();
            if (value > kMax) {
                return std::numeric_limits<std::int32_t>::max();
            }
            return static_cast<std::int32_t>(value);
        }

        /** HP/MP spend while godmode is off; zero while godmode is on.
         * @param amount Requested spend.
         * @param godmode True when `/godmode` is active.
         * @return 0 when godmode is on, else max(0, amount).
         */
        [[nodiscard]] inline int resource_cost(int amount, bool godmode) {
            if (godmode || amount <= 0) {
                return 0;
            }
            return amount;
        }

        /** Incoming HP damage while godmode is off.
         * @param damage Resolved damage.
         * @param godmode True when `/godmode` is active.
         * @return 0 when godmode is on, else the clamped damage.
         */
        [[nodiscard]] inline std::int32_t incoming_damage(std::int32_t damage, bool godmode) {
            return godmode ? 0 : clamp_nonneg_i32(damage);
        }

        /** Reduces incoming damage by a 0–100 percent (Knight Perfect Defense).
         * @param damage Unreduced damage.
         * @param percent Reduction percent.
         * @return Remaining damage.
         */
        [[nodiscard]] inline std::int32_t apply_percent_reduction(std::int32_t damage,
                                                                  int percent) {
            if (damage <= 0 || percent <= 0) {
                return clamp_nonneg_i32(damage);
            }
            int p = percent;
            if (p > 100) {
                p = 100;
            }
            return clamp_nonneg_i32(static_cast<std::int64_t>(damage) -
                                    static_cast<std::int64_t>(damage) * p / 100);
        }

        /** Godmode plus Perfect Defense reduction for damage dealt to a player.
         * @param damage Resolved damage.
         * @param godmode True when `/godmode` is active.
         * @param perfect_defense_percent Active Perfect Defense value1 percent.
         * @return Remaining damage.
         */
        [[nodiscard]] inline std::int32_t
        incoming_player_damage(std::int32_t damage, bool godmode, int perfect_defense_percent) {
            return apply_percent_reduction(incoming_damage(damage, godmode),
                                           perfect_defense_percent);
        }

        /** Blessing of Son of the Sky physical/magical power percent.
         * @param skill_level Learned rank.
         * @return Percent bonus, or 0 when unlearned.
         */
        [[nodiscard]] inline int knight_blessing_power_percent(int skill_level) {
            return skill_level > 0 ? 8 + 4 * (skill_level - 1) : 0;
        }

        /** Defense strengthening points based on character level and base defense.
         * @param char_level Character level.
         * @param org_defense Defense before this skill and percent buffs.
         * @return Added defense points.
         */
        [[nodiscard]] inline std::int32_t
        knight_defense_strengthening_points(int char_level, std::int32_t org_defense) {
            const int level = char_level > 0 ? char_level : 0;
            const std::int32_t from_level = (level - 65) * 5;
            return (from_level > 0 ? from_level : 0) + org_defense * 2 / 50;
        }

        /** Provocation of Blow extra hostility (compatibility notes nDmg * 3).
         * @param dealt Damage applied to the monster.
         * @return Hostility added when the monster is aggressive and already has a target.
         */
        [[nodiscard]] inline int knight_provocation_of_blow_hostility(std::int32_t dealt) {
            return static_cast<int>(clamp_nonneg_i32(static_cast<std::int64_t>(dealt) * 3));
        }

        /** Provocation of Blow PvP reduction (compatibility notes KPOBReduce, default 35%).
         * @param raw Skill power before apply-damage.
         * @return Reduced power.
         */
        [[nodiscard]] inline std::int32_t knight_provocation_of_blow_pvp_damage(std::int32_t raw) {
            return clamp_nonneg_i32(static_cast<std::int64_t>(raw) * 35 / 100);
        }

        /** Spin Slash PvP reduction (compatibility notes KSSReduce, default 35%).
         * @param raw Skill power before apply-damage.
         * @return Reduced power.
         */
        [[nodiscard]] inline std::int32_t knight_spin_slash_pvp_damage(std::int32_t raw) {
            return clamp_nonneg_i32(static_cast<std::int64_t>(raw) * 35 / 100);
        }

        /** The Wave of Earth raw PvP scaling from ReadConfig::NWOEReduce.
         * @param raw Skill power before target defense.
         * @return One percent of raw power, with integer truncation.
         */
        [[nodiscard]] inline std::int32_t knight_wave_of_earth_pvp_damage(std::int32_t raw) {
            return clamp_nonneg_i32(raw / 100);
        }

        /** Later Flame Injection/Icicle per-pulse power from the configured compatibility values.
         * @param skill_level Learned rank.
         * @param magic Rolled magic attack.
         * @param grade_roll Deterministic roll in [5, 10], or negative to roll randomly.
         * @return Raw power before PvP scaling and target defense.
         */
        [[nodiscard]] std::int32_t
        mage_channel_power(int skill_level, std::int32_t magic, int grade_roll = -1);

        /** Magic Intensification's additional damage after ordinary target mitigation.
         * @param rank Active buff rank, or zero when inactive.
         * @param magic Rolled magic attack.
         * @param target_is_player Whether to apply the 150% PvP coefficient.
         * @param grade_roll Deterministic roll in [5, 10], or negative for a random roll.
         * @return Additional damage, or zero when the buff is inactive.
         */
        [[nodiscard]] std::int32_t mage_intensification_bonus(int rank,
                                                              std::int32_t magic,
                                                              bool target_is_player,
                                                              int grade_roll = -1);

        /** Character creation base statistics. */
        struct BaseStats {
            std::int32_t str = 0;
            std::int32_t hth = 0;
            std::int32_t intl = 0;
            std::int32_t wis = 0;
            std::int32_t dex = 0;
        };

        /** Returns base statistics plus the initial point bonus.
         * @param clazz Character class index.
         * @return Initial statistics.
         */
        [[nodiscard]] BaseStats class_base_plus_bonus(int clazz);

        /** HP denominator table indexed by class. */
        inline constexpr int kDenoHp[4] = {10, 14, 13, 12};

        /** MP denominator table indexed by class. */
        inline constexpr int kDenoMp[4] = {13, 10, 12, 12};

        /** Computes max HP from HTH, level and class (Char::ReviseProperty).
         * @param hth Health statistic.
         * @param level Character level.
         * @param clazz Character class index.
         * @return Max HP, at least 1.
         */
        [[nodiscard]] std::int32_t
        max_hp_from_stats(std::int32_t hth, std::int32_t level, int clazz);

        /**
         * compatibility behavior max HP: monster definition hp plus compatibility behavior's
         * `52 * level / 3` plus `2 * hth * hth / 10 + 115`.
         * @param hp_base monster definition / JSON `hp` field (nBase[P_HP] before revise).
         * @param hth monster definition HTH (nBase[P_HTH]).
         * @param level Monster level.
         * @return Max HP, at least 1.
         */
        [[nodiscard]] std::int32_t
        monster_max_hp(std::int32_t hp_base, std::int32_t hth, std::int32_t level);

        /** Computes max MP from WIS, level and class (Char::ReviseProperty).
         * @param wis Wisdom statistic.
         * @param level Character level.
         * @param clazz Character class index.
         * @return Max MP, at least 1.
         */
        [[nodiscard]] std::int32_t
        max_mp_from_stats(std::int32_t wis, std::int32_t level, int clazz);

        /** Primary property indices (PROPERTY_TYPE / P_STR..P_DEX). */
        enum class PrimaryStat : int {
            Str = 0,
            Hth = 1,
            Int = 2,
            Wis = 3,
            Dex = 4,
        };

        /** Applies Char::ReviseProperty attack, accuracy, magic and resist
         * contributions.
         * @param level Character level.
         * @param stats Character statistics.
         * @param min_attack Minimum attack value to update.
         * @param max_attack Maximum attack value to update.
         * @param hit Hit value to update.
         * @param dodge Dodge value to update.
         * @param min_magic Minimum magic attack to update.
         * @param max_magic Maximum magic attack to update.
         * @param res_fire Fire resist to update.
         * @param res_ice Ice resist to update.
         * @param res_lightning Lightning resist to update.
         * @param res_curse Curse resist to update.
         * @param res_palsy Non-elemental / palsy resist to update.
         */
        void apply_stat_revise(std::int32_t level,
                               const BaseStats& s,
                               std::int32_t& min_atk,
                               std::int32_t& max_atk,
                               std::int32_t& hit,
                               std::int32_t& dodge,
                               std::int32_t& min_magic,
                               std::int32_t& max_magic,
                               std::int32_t& res_fire,
                               std::int32_t& res_ice,
                               std::int32_t& res_lightning,
                               std::int32_t& res_curse,
                               std::int32_t& res_palsy);

        /**
         * Applies compatibility behavior resist `* 10`, compatibility behavior (STR/HTH/INT/WIS/DEX
         * into attack, hit, dodge, magic and resist) and compatibility behavior HP.
         * Stored resist stays on the `* 10` scale so monster_final_resist / GetPalsyDamage
         * can divide by 10.
         * @param level Monster level.
         * @param stats monster definition primaries (nBase[P_STR..P_DEX]).
         * @param hp_base monster definition hp field.
         * @param min_atk Minimum attack (monster definition, updated in place).
         * @param max_atk Maximum attack (updated in place).
         * @param hit Hit (updated in place).
         * @param dodge Dodge (updated in place).
         * @param min_magic Minimum magic (updated in place).
         * @param max_magic Maximum magic (updated in place).
         * @param res_fire Fire resist as JSON/lisp value; becomes `10 * json + INT/9`.
         * @param res_ice Ice resist (same scale).
         * @param res_lightning Lightning resist (same scale).
         * @param res_curse Curse resist (`10 * json + WIS/9`).
         * @param res_palsy Palsy resist (`10 * json + HTH/9`).
         * @param max_hp Out revised max HP.
         */
        void revise_monster_properties(std::int32_t level,
                                       const BaseStats& stats,
                                       std::int32_t hp_base,
                                       std::int32_t& min_atk,
                                       std::int32_t& max_atk,
                                       std::int32_t& hit,
                                       std::int32_t& dodge,
                                       std::int32_t& min_magic,
                                       std::int32_t& max_magic,
                                       std::int32_t& res_fire,
                                       std::int32_t& res_ice,
                                       std::int32_t& res_lightning,
                                       std::int32_t& res_curse,
                                       std::int32_t& res_palsy,
                                       std::int32_t& max_hp);

        /**
         * Returns PU cost to raise a primary by one (compatibility behavior).
         * @param clazz Character class index.
         * @param property Primary property id (0..4).
         * @param current_value Current base of that property.
         * @return Cost in PU, or 0 when capped / invalid.
         */
        [[nodiscard]] std::int32_t
        get_need_point(int clazz, int property, std::int32_t current_value);

        /**
         * Cumulative PU cost to raise a primary from current by pending steps.
         * @param clazz Character class index.
         * @param property Primary property id (0..4).
         * @param current_value Current base of that property.
         * @param pending Steps to add.
         * @return Total PU cost.
         */
        [[nodiscard]] std::int32_t
        pending_need_points(int clazz, int property, std::int32_t current_value, int pending);

        /** Applies an authoritative batch of primary-stat raises.
         * @param clazz Character class index.
         * @param property Primary property id (0..4).
         * @param requested Requested raise count.
         * @param current_value Current base stat, updated for each affordable raise.
         * @param pu_point Available PU, reduced by server-calculated costs.
         * @return Number of raises applied.
         */
        [[nodiscard]] int spend_primary_points(int clazz,
                                               int property,
                                               int requested,
                                               std::int32_t& current_value,
                                               std::int32_t& pu_point);

        /** Produces an inclusive pseudo-random integer.
         * @param lo Minimum value.
         * @param hi Maximum value.
         * @return Random value in the requested range.
         */
        [[nodiscard]] int irand(int lo, int hi);

        /** Looks up the damage level adjustment. */
        [[nodiscard]] int revise_damage(int abs_level_diff);

        /** Looks up the hit level adjustment. */
        [[nodiscard]] int revise_hit(int abs_level_diff);

        /** Looks up the hit percentage. */
        [[nodiscard]] int hit_rate_pct(int clamped_rate);

        /** Magic resist indices (RESIST_TYPE). */
        enum class MagicResist : int {
            Fire = 0,
            Ice = 1,
            Lightning = 2,
            Curse = 3,
            Palsy = 4,
        };

        /** Physical-combat properties for an attacker or target. */
        struct Combatant {
            std::int32_t level = 1;
            std::int32_t min_attack = 1;
            std::int32_t max_attack = 1;
            std::int32_t min_magic = 0;
            std::int32_t max_magic = 0;
            /** Active later Mage damage bonus and level-limit override. */
            int magic_intensification_rank = 0;
            /** Armor Breaker increases resolved damage above 100 by five percent. */
            bool armor_broken = false;
            std::int32_t hit = 0;
            std::int32_t dodge = 0;
            std::int32_t defense = 0;        // melee GetDefense(0)
            std::int32_t defense_range = 0;  // GetDefense(1) vs ranged attack type
            std::int32_t absorb = 0;         // percent
            std::int32_t fatal_rate = 0;     // runtime state.nFatal (weapon EB %)
            std::int32_t fatal_pt = 0;       // runtime state.nFatal (fatal damage coefficient)
            /** Additional PvE critical damage, in percent of the critical hit. */
            double critical_damage_percent = 0;
            /** Additional PvE explosive damage, in percent of the explosive component. */
            double explosive_damage_percent = 0;

            /** Additional talent explosive power in both PvE and PvP. */
            int talent_explosive_damage_percent = 0;

            /** Base Archer perfect-evasion chance in percent. */
            std::int32_t perfect_evasion_base = 0;

            /** Advanced Archer perfect-evasion chance in percent. */
            std::int32_t perfect_evasion_advanced = 0;

            std::int32_t res_fire = 0;
            std::int32_t res_ice = 0;
            std::int32_t res_lightning = 0;
            std::int32_t res_curse = 0;
            std::int32_t res_palsy = 0;
            int attack_range = 16;
            int attack_speed_ms = 700;
            /** True when this combatant is a player (GetPalsyDamage tables). */
            bool is_player = false;
            /** Class index (0 knight / 1 mage / 2 archer / 3 thief). */
            int clazz = 0;
            /** compatibility behavior / monster definition attack type (0 melee, 1 range). */
            int attack_type = 0;
            /** Equipped Explosive Blow level. */
            int upgrade_level = 0;
            /** Character attack after revise, before gear (runtime state). */
            int char_attack = 0;
            /** Character magic after revise, before gear (runtime state). */
            int char_magic = 0;
            /** Worn weapon min physical attack including charm. */
            int weapon_min_attack = 0;
            /** Worn weapon max physical attack including charm. */
            int weapon_max_attack = 0;
            /** Worn weapon min magic including charm. */
            int weapon_min_magic = 0;
            /** Worn weapon max magic including charm. */
            int weapon_max_magic = 0;
            /** Active pay-scroll compatibility behavior bitmask (compatibility behavior*). */
            std::uint64_t bstate = 0;

            /** runtime state.nBlock — shield / weapon-defend / archer evade percent. */
            std::int32_t block_percent = 0;

            /** True when a shield is equipped (WS_SHIELD). */
            bool wearing_shield = false;

            /** True when a two-hand sword is equipped (WS_2HANDWEAPON). */
            bool wearing_two_hand = false;

            /** True when Knight Parry is learned. */
            bool has_parry = false;

            /** True when specialty has E_JCAB_2ND_ATTRIB_JOB_1. */
            bool has_second_job_attrib = false;
        };

        /** Rolls a combatant's raw attack.
         * @param attacker Attacking combatant.
         * @return Raw attack value.
         */
        [[nodiscard]] std::int32_t roll_attack(const Combatant& attacker);

        /** Resolves Char::CheckHit.
         * @param attacker Attacking combatant.
         * @param target Target combatant.
         * @param add_hit Additional hit modifier.
         * @return True when the attack hits.
         */
        [[nodiscard]] bool
        check_hit(const Combatant& attacker, const Combatant& target, int add_hit = 0);

        /**
         * GetColorLevel from g_nReviseExpA/B (0 gray .. 6 purple).
         * @param attacker_level Attacker level (monster in CheckBlock).
         * @param defender_level Defender level (player in CheckBlock).
         * @return Color-level column for g_nReviseBlock.
         */
        [[nodiscard]] int color_level(int attacker_level, int defender_level);

        /**
         * compatibility behavior. Rolled before CheckHit on player targets.
         * @param attacker Attacking combatant.
         * @param target Defending player combatant.
         * @param chance_roll Inclusive 1..100, or -1 to roll.
         * @return True when the attack is blocked (no hit roll, no damage).
         */
        [[nodiscard]] bool
        check_block(const Combatant& attacker, const Combatant& target, int chance_roll = -1);

        /**
         * Weapon Defend additional nBlock: 2.5 + (level - 69) / 2.
         * @param char_level Player level.
         * @return Non-negative block percent points.
         */
        [[nodiscard]] int knight_weapon_defend_block(int char_level);

        /**
         * related compatibility states.
         * vs monster: GetDefense(attack type). vs player: plus level / g_denoDef.
         * @param attacker Attacking combatant (attack_type selects melee vs range).
         * @param target Defending combatant.
         * @return Non-negative defense total consumed by mitigate_defense.
         */
        [[nodiscard]] std::int32_t final_defense(const Combatant& attacker,
                                                 const Combatant& target);

        /**
         * related compatibility states.
         * vs monster: GetAbsorb(). vs player: GetAbsorb() + 15.
         * @param attacker Attacking combatant.
         * @param target Defending combatant.
         * @return Non-negative absorb total consumed by mitigate_absorb, including values above
         * 100.
         */
        [[nodiscard]] std::int32_t final_absorb(const Combatant& attacker, const Combatant& target);

        /** Applies flat defense up to 60% of attack, then an asymptotic tail.
         * @param attack Revised attack; non-positive values produce zero damage.
         * @param defense Defense total; negative values are treated as zero.
         * @return Remaining damage before absorb and the final minimum of one.
         */
        [[nodiscard]] std::int64_t mitigate_defense(std::int64_t attack, std::int32_t defense);

        /** Applies linear absorb up to 40 points, then an asymptotic tail.
         * @param damage Damage after defense; non-positive values produce zero damage.
         * @param absorb Absorb total; negative values are treated as zero.
         * @return Remaining damage before bonuses and the final minimum of one.
         */
        [[nodiscard]] std::int64_t mitigate_absorb(std::int64_t damage, std::int32_t absorb);

        /** Perfect-evasion roll policy; low ranks retain their two independent rolls. */
        struct PerfectEvasionRoll {
            /** True when both raw percentages are at most 40. */
            bool legacy_two_rolls = true;

            /** Single-roll miss percent, capped at 75; unused and zero on the legacy path. */
            std::int32_t combined_percent = 0;
        };

        /** Selects legacy rolls or one combined perfect-evasion roll.
         * @param base Raw base percentage; negative values are treated as zero.
         * @param advanced Raw advanced percentage; negative values are treated as zero.
         * @return Roll policy and the combined percentage when not using legacy rolls.
         */
        [[nodiscard]] PerfectEvasionRoll perfect_evasion_roll(std::int32_t base,
                                                              std::int32_t advanced);

        /** Resolves final physical damage.
         * @param attacker Attacking combatant.
         * @param target Target combatant.
         * @param raw_attack Rolled attack value.
         * @return At least one for positive revised attack, otherwise zero.
         */
        [[nodiscard]] std::int32_t
        final_damage(const Combatant& attacker, const Combatant& target, std::int32_t raw_attack);

        /** Result of one physical attack resolution. */
        struct AttackResult {
            bool hit = false;
            bool fatal = false;
            bool explosive = false;

            /** True when compatibility behavior succeeded (S2C_ATTACK bit 2). */
            bool blocked = false;

            std::int32_t damage = 0;

            /** Extra Explosive Blow damage included in `damage` (S2C_ATTACK palsy). */
            std::int32_t palsy = 0;

            /** Ice mastery slow duration in seconds (`PtIce + 2`). */
            int ice_slow_duration_sec = 0;

            /** Ice mastery move-speed percent (`-20 * PtIce`). */
            int ice_slow_percent = 0;
        };

        /**
         * compatibility behavior extra: chance is add_fatal + fatal_rate vs 1..100.
         * Extra = nDamage * (2 * fatal_pt + 50) / 100 when it procs.
         * @param damage GetFinalDamage result.
         * @param fatal_rate Weapon / passive EB percent (runtime state.nFatal).
         * @param fatal_pt Fatal damage coefficient (runtime state.nFatal).
         * @param add_fatal Skill GetFatalRate().
         * @param fatal Out: true when the fatal roll succeeded.
         * @return Extra damage, or 0.
         */
        [[nodiscard]] std::int32_t get_fatal_extra(std::int32_t damage,
                                                   std::int32_t fatal_rate,
                                                   std::int32_t fatal_pt,
                                                   int add_fatal,
                                                   bool& fatal);

        /**
         * GetPalsyDamage proc chance after level-gap revise (0..100+).
         * @param upgrade_level Equipped Explosive Blow level.
         * @param attacker_level Attacker level.
         * @param target_level Target level.
         * @param target_is_player True when the target is a player.
         * @return Chance compared against RANDOM(1, 100).
         */
        [[nodiscard]] int get_palsy_chance(int upgrade_level,
                                           int attacker_level,
                                           int target_level,
                                           bool target_is_player);

        /**
         * Extra Explosive Blow damage (compatibility behavior).
         * @param attacker Attacking combatant.
         * @param target Target combatant.
         * @param chance_roll Inclusive 1..100, or -1 to roll.
         * @param damage_roll Inclusive span roll, or -1 to roll.
         * @return Extra damage, or 0.
         */
        [[nodiscard]] std::int32_t get_palsy_extra(const Combatant& attacker,
                                                   const Combatant& target,
                                                   int chance_roll = -1,
                                                   int damage_roll = -1);

        /**
         * compatibility behavior using `g_nSkillContiSuccessRate`.
         * @param owner_level Caster level.
         * @param target_level Target level.
         * @param chance_roll Inclusive 1..100, or -1 to roll.
         * @return True when a continuation buff may apply.
         */
        [[nodiscard]] bool can_conti_skill(int owner_level, int target_level, int chance_roll = -1);

        /**
         * Fills Ice Magic Mastery slow (`compatibility behavior`).
         * @param result Attack result to update.
         * @param owner_level Caster level.
         * @param target_level Target level.
         * @param ice_mastery Learned skill 13 rank (`runtime state`).
         * @param chance_roll Inclusive 1..100, or -1 to roll.
         */
        void apply_ice_mastery_slow(AttackResult& result,
                                    int owner_level,
                                    int target_level,
                                    int ice_mastery,
                                    int chance_roll = -1);

        /**
         * compatibility behavior fire-mastery reduction.
         * @param delay_ms `delay[0] + delay[1]`.
         * @param delay_pre_ms `delay[0]`.
         * @param fire_mastery Learned skill 14 rank (`runtime state`).
         * @return Delay milliseconds, at least 1.
         */
        [[nodiscard]] int fire_mastery_delay_ms(int delay_ms, int delay_pre_ms, int fire_mastery);

        /**
         * compatibility behavior for archer (and generic physical) combat skills.
         * @param clazz Attacker class (formulas are per-class indices).
         * @param skill_index Skill index.
         * @param skill_level Learned skill level.
         * @param dex Archer DEX (compatibility behavior).
         * @param base_attack Rolled compatibility behavior().
         * @param random_roll Optional deterministic formula roll.
         * @param prerequisite_skill_level Learned Shot rank for Stun Arrow or Flamy Arrow rank for
         * Explosion; a negative value preserves the same-rank fallback.
         * @return Skill attack before ReviseTick scaling.
         */
        [[nodiscard]] std::int32_t skill_physical_power(int clazz,
                                                        int skill_index,
                                                        int skill_level,
                                                        std::int32_t dex,
                                                        std::int32_t base_attack,
                                                        int random_roll = -1,
                                                        int prerequisite_skill_level = -1);

        /** Returns the Poison Arrow or Pain tick after resistance.
         * @param skill_index Poison Arrow (13) or Pain (19).
         * @param skill_level Learned rank.
         * @param owner_level Character level.
         * @param base_attack Rolled physical attack.
         * @param final_resist Final palsy or curse resistance in percent.
         * @return Nonnegative damage per tick.
         */
        [[nodiscard]] std::int32_t archer_status_tick(int skill_index,
                                                      int skill_level,
                                                      int owner_level,
                                                      std::int32_t base_attack,
                                                      int final_resist);

        /** Returns nonlethal Life Absorption damage before applying it.
         * @param skill_level Learned rank.
         * @param base_attack Rolled physical attack.
         * @param target_hp Current target health.
         * @return Damage that leaves at least one HP.
         */
        [[nodiscard]] std::int32_t archer_life_absorption_damage(int skill_level,
                                                                 std::int32_t base_attack,
                                                                 std::int32_t target_hp);

        /** Returns the healing from Life Absorption.
         * @param skill_level Learned rank.
         * @param damage Actually inflicted damage.
         * @return Health restored before the maximum-HP cap.
         */
        [[nodiscard]] std::int32_t archer_life_absorption_heal(int skill_level,
                                                               std::int32_t damage);

        /** Computes Spin Slash including all stored Deathblow points.
         * @param skill_level Learned rank.
         * @param base_attack Rolled physical attack.
         * @param mortal_points Stored points, clamped to 0..5.
         * @param attack_roll Fixed 5..20 roll for tests; negative rolls randomly.
         * @param mortal_roll Fixed 3000..6000 roll for tests; negative rolls randomly.
         * @return Raw damage before defense and PvP reduction.
         */
        [[nodiscard]] std::int32_t knight_spin_slash_power(int skill_level,
                                                           std::int32_t base_attack,
                                                           int mortal_points,
                                                           int attack_roll = -1,
                                                           int mortal_roll = -1);

        /**
         * compatibility behavior with the Knight's current Mortal points.
         * @param skill_level Learned Infernal rank.
         * @param base_attack Rolled compatibility behavior().
         * @param mortal_points Current P_MORTAL point contribution.
         * @return Infernal attack before ReviseTick scaling.
         */
        [[nodiscard]] std::int32_t knight_infernal_power(int skill_level,
                                                         std::int32_t base_attack,
                                                         std::int32_t mortal_points);

        /**
         * compatibility behavior bonus for a physical combat skill.
         * @param clazz Attacker class.
         * @param skill_index Skill index.
         * @param skill_level Learned skill level.
         * @return Added hit passed to CheckHit.
         */
        [[nodiscard]] int skill_physical_hit(int clazz, int skill_index, int skill_level);

        /**
         * compatibility behavior for physical blows that add weapon aspeed.
         * @param clazz Attacker class.
         * @param skill_index Skill index.
         * @param skill_level Learned skill level.
         * @param def_delay_ms skill definition delay[0]+delay[1].
         * @param attack_speed_ms Equipped weapon aspeed.
         * @return Delay milliseconds used by ReviseTick.
         */
        [[nodiscard]] int skill_physical_delay_ms(
            int clazz, int skill_index, int skill_level, int def_delay_ms, int attack_speed_ms);

        /**
         * compatibility behavior percent revision for Mesh of Speed / haste-like aspeed.
         * Negative percent lengthens the delay: `delay * (|pct| + 100) / 100`.
         * @param delay_ms Unrevised attack delay.
         * @param speed_percent runtime state.nASpeed (negative is slower).
         * @return Revised delay in milliseconds.
         */
        [[nodiscard]] int revised_attack_delay_ms(int delay_ms, std::int32_t speed_percent);

        /**
         * compatibility behavior for a physical combat skill.
         * @param clazz Attacker class.
         * @param skill_index Skill index.
         * @param skill_level Learned skill level.
         * @return Added fatal chance.
         */
        [[nodiscard]] int skill_physical_fatal(int clazz, int skill_index, int skill_level);

        /**
         * True for related compatibility states (AddHostility, no GetAttack).
         * @param clazz Attacker class.
         * @param skill_index Skill index.
         * @return True for Knight Mockery (4) and Provocation (8).
         */
        [[nodiscard]] bool is_knight_taunt(int clazz, int skill_index);

        /**
         * Jobsystem compare[1] range for a taunt skill.
         * @param skill_index Knight skill index.
         * @param skill_level Learned rank.
         * @return Range in pixels.
         */
        [[nodiscard]] int skill_taunt_range(int skill_index, int skill_level);

        /**
         * related compatibility states.
         * @param skill_index Knight skill index.
         * @param skill_level Learned rank.
         * @return Hostility added to the monster threat table.
         */
        [[nodiscard]] int skill_taunt_hostility(int skill_index, int skill_level);

        /** Weapon Mastery property-point contributions. */
        struct KnightWeaponMasteryContribution {
            /** Added hit points. */
            std::int32_t hit = 0;

            /** Added minimum attack points. */
            std::int32_t min_attack = 0;

            /** Added maximum attack points. */
            std::int32_t max_attack = 0;
        };

        /**
         * compatibility behavior contributions for Knight Weapon Mastery.
         * @param skill_level Learned Weapon Mastery rank.
         * @return Hit and physical attack point contributions.
         */
        [[nodiscard]] KnightWeaponMasteryContribution
        knight_weapon_mastery_contribution(int skill_level);

        /** Critical Hit property contributions. */
        struct KnightCriticalContribution {
            /** Added fatal damage points. */
            std::int32_t fatal_pt = 0;

            /** Added fatal proc percentage points. */
            std::int32_t fatal_rate = 0;
        };

        /**
         * compatibility behavior contributions for Knight Critical Hit.
         * @param skill_level Learned Critical Hit rank.
         * @return Fatal damage and proc-rate contributions.
         */
        [[nodiscard]] KnightCriticalContribution knight_critical_contribution(int skill_level);

        /**
         * compatibility behavior fixed advanced fatal-point contribution.
         * @return Added fatal damage points.
         */
        [[nodiscard]] std::int32_t knight_advanced_critical_fatal_pt();

        /**
         * compatibility behavior life-extension contribution.
         * @param revised_hth_hp Revised HTH-only HP contribution.
         * @param skill_level Learned Reinforce Vitality rank.
         * @return Added maximum HP.
         */
        [[nodiscard]] std::int32_t knight_life_extension_hp(std::int32_t revised_hth_hp,
                                                            int skill_level);

        /** Archer Bow Mastery property-point contributions. */
        struct ArcherBowMasteryContribution {
            /** Added minimum attack points. */
            std::int32_t min_attack = 0;

            /** Added maximum attack points. */
            std::int32_t max_attack = 0;
        };

        /**
         * compatibility behavior contributions.
         * @param skill_level Learned Bow Mastery rank.
         * @return Minimum and maximum physical-attack additions.
         */
        [[nodiscard]] ArcherBowMasteryContribution archer_bow_mastery_contribution(int skill_level);

        /** Archer perfect-evasion percentage contributions. */
        struct ArcherEvasionContribution {
            /** compatibility behavior contribution (`rank + 7`). */
            std::int32_t base_percent = 0;

            /** compatibility behavior contribution (`rank`). */
            std::int32_t advanced_percent = 0;
        };

        /**
         * Returns Archer perfect-evasion passive contributions.
         * @param base_skill_level Learned Perfect Evasion rank.
         * @param advanced_skill_level Learned advanced Perfect Evasion rank.
         * @return Base and advanced percentage-point additions.
         */
        [[nodiscard]] ArcherEvasionContribution
        archer_evasion_contribution(int base_skill_level, int advanced_skill_level);

        /** Archer Fatal Blow passive contributions. */
        struct ArcherFatalContribution {
            /** Added fatal damage points from Fatal Blow. */
            std::int32_t fatal_pt = 0;

            /** Added fatal chance, capped at ten. */
            std::int32_t fatal_rate = 0;

            /** Fixed advanced Critical Hit fatal points. */
            std::int32_t advanced_fatal_pt = 0;
        };

        /**
         * Returns both Archer fatal passive tiers.
         * @param fatal_skill_level Learned Fatal Blow rank.
         * @param advanced_active True when advanced Critical Hit is learned.
         * @return Fatal point and chance additions.
         */
        [[nodiscard]] ArcherFatalContribution archer_fatal_contribution(int fatal_skill_level,
                                                                        bool advanced_active);

        /**
         * compatibility behavior hit contribution.
         * @param skill_level Learned Concentration rank.
         * @return Added hit points.
         */
        [[nodiscard]] std::int32_t archer_concentration_hit(int skill_level);

        /**
         * compatibility behavior passive contribution.
         * @param skill_level Learned Range Extension rank.
         * @return Added attack range.
         */
        [[nodiscard]] std::int32_t archer_attack_range_bonus(int skill_level);

        /**
         * compatibility behavior Archer HP contribution using source base HTH.
         * @param base_hth Unmodified base HTH (`runtime state.nBase[P_HTH]`).
         * @param skill_level Learned Life Extension rank.
         * @return Added maximum HP.
         */
        [[nodiscard]] std::int32_t archer_life_extension_hp(std::int32_t base_hth, int skill_level);

        /**
         * compatibility behavior.
         * @param skill_level Learned Shot rank.
         * @param dex Archer DEX.
         * @param base_attack Rolled physical attack.
         * @return Shot attack.
         */
        [[nodiscard]] std::int32_t
        archer_shot_power(int skill_level, std::int32_t dex, std::int32_t base_attack);

        /**
         * compatibility behavior.
         * @param skill_level Learned Flame Arrow rank.
         * @param base_attack Rolled physical attack.
         * @return Flame Arrow attack.
         */
        [[nodiscard]] std::int32_t archer_flame_arrow_power(int skill_level,
                                                            std::int32_t base_attack);

        /**
         * compatibility behavior, including the separately ranked Shot prerequisite.
         * @param skill_level Learned Stun Arrow rank.
         * @param shot_skill_level Learned Shot rank; zero means unavailable.
         * @param dex Archer DEX.
         * @param base_attack Rolled physical attack.
         * @return Stun Arrow attack, or zero without Shot.
         */
        [[nodiscard]] std::int32_t archer_stun_arrow_power(int skill_level,
                                                           int shot_skill_level,
                                                           std::int32_t dex,
                                                           std::int32_t base_attack);

        /**
         * compatibility behavior, including the separately ranked Flame Arrow prerequisite.
         * @param skill_level Learned Explosion rank.
         * @param flame_skill_level Learned Flame Arrow rank; zero means unavailable.
         * @param base_attack Rolled physical attack.
         * @return Explosion attack, or zero without Flame Arrow.
         */
        [[nodiscard]] std::int32_t
        archer_explosion_power(int skill_level, int flame_skill_level, std::int32_t base_attack);

        /**
         * compatibility behavior.
         * @param skill_level Learned Mystic Arrow rank.
         * @param base_attack Rolled physical attack.
         * @return Mystic Arrow attack.
         */
        [[nodiscard]] std::int32_t archer_mystic_arrow_power(int skill_level,
                                                             std::int32_t base_attack);

        /**
         * Arrow Rain pulse damage: `GetAttack() * 7 + grade * Rate(1800, 2000)`.
         * @param skill_level Learned Arrow Rain rank.
         * @param base_attack Rolled `GetAttack()`.
         * @param random_roll Optional grade roll; negative draws `irand(1800, 2000)`.
         * @return Monster-target tick damage.
         */
        [[nodiscard]] std::int32_t
        archer_arrow_rain_power(int skill_level, std::int32_t base_attack, int random_roll = -1);

        /**
         * Arrow Rain PvP damage scaling (`AARReduce` percent of monster damage).
         * @param damage Monster-target tick damage.
         * @return Player-target tick damage.
         */
        [[nodiscard]] std::int32_t archer_arrow_rain_pvp_damage(std::int32_t damage);

        /**
         * `compatibility behavior`: `(200 * SL + 50) * GetAttack() / 100 / 10`.
         * Stored as Magic-controller GetMagic() and applied as `BUFF_POISONCLOUD` ticks.
         * @param skill_level Learned Poison Cloud rank.
         * @param base_attack Rolled `GetAttack()` at cast.
         * @return Per-second poison tick after the 2 s first delay.
         */
        [[nodiscard]] std::int32_t archer_poison_cloud_power(int skill_level,
                                                             std::int32_t base_attack);

        /**
         * Poison-cloud HP tick never kills (`nValue = CurHP - 1` at 1 HP).
         * @param cur_hp Current hit points.
         * @param tick_damage Formula tick.
         * @return Applied damage, or zero when the target is already at 1 HP.
         */
        [[nodiscard]] inline std::int32_t
        archer_poison_cloud_applied_damage(std::int32_t cur_hp, std::int32_t tick_damage) {
            if (cur_hp <= 1 || tick_damage <= 0) {
                return 0;
            }
            const std::int32_t leave_one = cur_hp - 1;
            return tick_damage < leave_one ? tick_damage : leave_one;
        }

        /**
         * Converted Shrewdness dodge points: `org_dodge * (compatibility behavior / 2) / 400`.
         * @param org_dodge Dodge before this passive.
         * @param char_level Player level.
         * @return Added dodge points, or zero when unlearned inputs are empty.
         */
        [[nodiscard]] inline std::int32_t archer_shrewdness_dodge_points(std::int32_t org_dodge,
                                                                         int char_level) {
            if (org_dodge <= 0 || char_level <= 0) {
                return 0;
            }
            return clamp_nonneg_i32(static_cast<std::int64_t>(org_dodge) * (char_level / 2) /
                                    protocol::kArcherShrewdnessDodgeDivisor);
        }

        /**
         * Converted Blessing of Son of the Sky power percent (maxLevel 1).
         * @param skill_level Learned rank.
         * @return Percent bonus, or 0 when unlearned.
         */
        [[nodiscard]] inline int archer_blessing_power_percent(int skill_level) {
            return skill_level > 0 ? protocol::kArcherBlessingPowerPercent : 0;
        }

        /**
         * Arrow Explosion splash damage: `GetAttack() * 20 + grade * Rate(1000, 1200)`.
         * @param skill_level Learned Arrow Explosion rank.
         * @param base_attack Rolled `GetAttack()`.
         * @param random_roll Optional grade roll; negative draws `irand(1000, 1200)`.
         * @return Monster-target splash damage.
         */
        [[nodiscard]] std::int32_t archer_arrow_explosion_power(int skill_level,
                                                                std::int32_t base_attack,
                                                                int random_roll = -1);

        /**
         * Arrow Explosion PvP damage scaling (`AAEReduce` percent of monster damage).
         * @param damage Monster-target splash damage.
         * @return Player-target splash damage.
         */
        [[nodiscard]] std::int32_t archer_arrow_explosion_pvp_damage(std::int32_t damage);

        /**
         * True when Arrow Rain's caster left the cast cell.
         * @param start_x Pixel X stored at cast.
         * @param start_y Pixel Y stored at cast.
         * @param now_x Current caster pixel X.
         * @param now_y Current caster pixel Y.
         * @return True when either axis changed.
         */
        [[nodiscard]] constexpr bool
        archer_arrow_rain_canceled_by_move(int start_x, int start_y, int now_x, int now_y) {
            return start_x != now_x || start_y != now_y;
        }

        /**
         * Converts Focus Shot elapsed charge time to source tier.
         * @param focus_ms Elapsed milliseconds since PreExcute.
         * @return Zero before one second, otherwise tier one through five.
         */
        [[nodiscard]] int archer_focus_shot_tier(int focus_ms);

        /**
         * compatibility behavior for one explicit charge tier.
         * @param skill_level Learned Focus Shot rank.
         * @param base_attack Rolled physical attack.
         * @param charge_tier Charge tier one through five.
         * @return Focus Shot attack, or zero for an invalid tier.
         */
        [[nodiscard]] std::int32_t
        archer_focus_shot_power(int skill_level, std::int32_t base_attack, int charge_tier);

        /**
         * compatibility behavior for one explicit charge tier.
         * @param skill_level Learned Focus Shot rank.
         * @param charge_tier Charge tier one through five.
         * @return Added hit, or zero for an invalid tier.
         */
        [[nodiscard]] int archer_focus_shot_hit(int skill_level, int charge_tier);

        /**
         * Archer compatibility behavior hostility override.
         * @param skill_index Archer skill index.
         * @return Hostility percentage adjustment.
         */
        [[nodiscard]] int archer_skill_hostility(int skill_index);

        /**
         * compatibility behavior periodic Spirit Damage payload.
         * @param base_attack Rolled attack used by Mystic Arrow.
         * @return Damage for each of the five source ticks.
         */
        [[nodiscard]] std::int32_t archer_mystic_spirit_tick(std::int32_t base_attack);

        /**
         * compatibility behavior fatal-rate delta.
         * @param target_fatal_rate Target fatal percentage before the curse.
         * @param skill_level Learned Curse of Critical Hit rank.
         * @return Non-positive fatal-rate change.
         */
        [[nodiscard]] std::int32_t archer_chance_curse_delta(std::int32_t target_fatal_rate,
                                                             int skill_level);

        /**
         * Resolves hit, ReviseTick scaling, final damage and fatal/EB.
         * @param attacker Attacking combatant.
         * @param target Target combatant.
         * @param add_hit Skill GetHit() bonus.
         * @param add_fatal Skill GetFatalRate().
         * @param raw_attack Precomputed GetAttack; negative rolls attacker min..max.
         * @param revise_tick_ms compatibility behavior / attack counter; negative uses full
         * delay.
         * @param delay_ms GetDelay / GetASpeed; 0 skips scaling.
         * @param skip_hit_check True to skip compatibility behavior (always-hit skills).
         * @return Hit result with fatal/explosive flags.
         */
        [[nodiscard]] AttackResult resolve_physical(const Combatant& attacker,
                                                    const Combatant& target,
                                                    int add_hit = 0,
                                                    int add_fatal = 0,
                                                    std::int32_t raw_attack = -1,
                                                    int revise_tick_ms = -1,
                                                    int delay_ms = 0,
                                                    bool skip_hit_check = false);

        /** Returns true for mage targeted damage skills (skill definition class 1).
         * @param skill_index Skill index.
         * @return True when the skill deals magic damage.
         */
        [[nodiscard]] bool is_mage_magic_attack(int skill_index);

        /** Returns the elemental resist used by a mage skill.
         * @param skill_index Skill index.
         * @return Resist type.
         */
        [[nodiscard]] MagicResist mage_skill_resist(int skill_index);

        /** Rolls a combatant's base magic (compatibility behavior).
         * @param attacker Attacking combatant.
         * @return Rolled base magic.
         */
        [[nodiscard]] std::int32_t roll_magic(const Combatant& attacker);

        /** Computes compatibility behavior for a mage attack skill.
         * @param skill_index Skill index.
         * @param skill_level Learned skill level.
         * @param char_level Caster level.
         * @param intelligence Caster INT.
         * @param base_magic Rolled GetMagic() from stats/weapon.
         * @param lightning_mastery Lightning mastery points (GetPrtyMagic).
         * @param lightning_skill_level Learned Lightning rank used by Spark; a negative value
         * preserves the same-rank fallback.
         * @param random_roll Optional deterministic Spark range roll.
         * @return Skill magic attack before resist and level revise.
         */
        [[nodiscard]] std::int32_t skill_magic_power(int skill_index,
                                                     int skill_level,
                                                     std::int32_t char_level,
                                                     std::int32_t intelligence,
                                                     std::int32_t base_magic,
                                                     std::int32_t lightning_mastery = 0,
                                                     int lightning_skill_level = -1,
                                                     int random_roll = -1);

        /**
         * Computes compatibility behavior for Mage Heal.
         * @param skill_level Learned Heal rank.
         * @param char_level Caster level.
         * @param wisdom Caster WIS.
         * @return Non-negative HP recovery amount.
         */
        [[nodiscard]] std::int32_t
        mage_self_heal_power(int skill_level, std::int32_t char_level, std::int32_t wisdom);

        /**
         * Computes confirmed Mage heal amounts.
         * @param skill_index Mage heal skill index.
         * @param skill_level Learned rank.
         * @param char_level Caster level.
         * @param wisdom Caster WIS.
         * @param target_max_hp Target maximum HP for percentage heals.
         * @param random_roll Optional deterministic roll for random heals.
         * @return Non-negative HP recovery amount; zero for unresolved skills.
         */
        [[nodiscard]] std::int32_t mage_heal_power(int skill_index,
                                                   int skill_level,
                                                   std::int32_t char_level,
                                                   std::int32_t wisdom,
                                                   std::int32_t target_max_hp,
                                                   int random_roll = -1);

        /** Converts monster definition resist into GetFinalResist (monster: stored / 10).
         * Stored values are `10 * monster definition json + INT/9` (compatibility behavior).
         * @param raw_resist Stored resist from monster definition.
         * @return Final resist used by compatibility behavior.
         */
        [[nodiscard]] std::int32_t monster_final_resist(std::int32_t raw_resist);

        /** Returns the percent of magic that gets through (compatibility behavior).
         * @param attacker_level Caster level.
         * @param target_level Target level.
         * @param final_resist Target GetFinalResist for this element.
         * @return 0..100 through-percent; 0 is a full resist.
         */
        [[nodiscard]] std::int32_t magic_through_percent(std::int32_t attacker_level,
                                                         std::int32_t target_level,
                                                         std::int32_t final_resist);

        /** Resolves compatibility behavior for magic (no defense/absorb, no CheckHit).
         * @param attacker_level Caster level.
         * @param target_level Target level.
         * @param raw_attack Magic attack after resist.
         * @return Signed damage before HP clamp; may be <= 0 from a large level gap.
         */
        [[nodiscard]] std::int32_t final_magic_damage(std::int32_t attacker_level,
                                                      std::int32_t target_level,
                                                      std::int32_t raw_attack);

        /**
         * compatibility behavior: never rejects an early recast; returns the elapsed
         * portion of the delay. Updates next_full_ms to now + delay (initial
         * runtime state).
         * @param delay_ms GetDelay = delay[0] + delay[1].
         * @param now_ms Current monotonic millisecond tick.
         * @param next_full_ms In/out next full-delay time (runtime state).
         * @return Milliseconds of delay to apply (1..delay_ms).
         */
        [[nodiscard]] int
        revise_skill_tick(int delay_ms, std::uint64_t now_ms, std::uint64_t& next_full_ms);

        /**
         * Scales a magic payload by ReviseTick / GetDelay (compatibility behavior).
         * @param value Unscaled resisted magic (nResist * nMagic / 100).
         * @param revise_tick_ms ReviseTick result.
         * @param delay_ms GetDelay.
         * @return Integer-scaled value.
         */
        [[nodiscard]] std::int32_t
        scale_by_skill_delay(std::int32_t value, int revise_tick_ms, int delay_ms);

        /** Resolves one mage skill attack. Magic never uses CheckHit.
         * @param attacker Caster combatant.
         * @param target Target combatant.
         * @param skill_index Skill index.
         * @param skill_level Learned skill level.
         * @param intelligence Caster INT.
         * @param lightning_mastery Lightning mastery points.
         * @param revise_tick_ms Elapsed cast delay; a negative value uses the full delay.
         * @param delay_ms GetDelay; 0 skips delay scaling.
         * @param lightning_skill_level Learned Lightning rank used by Spark.
         * @return Always-hit result; damage may be 0 from level gap or resist.
         */
        [[nodiscard]] AttackResult resolve_magic(const Combatant& attacker,
                                                 const Combatant& target,
                                                 int skill_index,
                                                 int skill_level,
                                                 std::int32_t intelligence,
                                                 std::int32_t lightning_mastery = 0,
                                                 int revise_tick_ms = -1,
                                                 int delay_ms = 0,
                                                 int lightning_skill_level = -1);

        /** Resolves an monster magic skill (compatibility behavior through compatibility
         * behavior).
         * @param attacker Monster combatant.
         * @param target Player combatant.
         * @param skill_index Monster skill index.
         * @param skill_level Configured monster skill rank.
         * @param intelligence Monster INT.
         * @return Always-hit magical result; unsupported skill indices return no damage.
         */
        [[nodiscard]] AttackResult resolve_monster_magic(const Combatant& attacker,
                                                         const Combatant& target,
                                                         int skill_index,
                                                         int skill_level,
                                                         std::int32_t intelligence);

        /** True when a monster skill has an elemental magic-damage implementation.
         * @param skill_index Monster skill catalog index.
         * @return True for a directly resolved magical monster skill.
         */
        [[nodiscard]] bool is_monster_magic_skill(int skill_index);

        /** Tests a squared two-dimensional attack range. */
        [[nodiscard]] bool in_attack_range(int ax, int ay, int tx, int ty, int range);

        /**
         * Query for compatibility behavior(..., TRUE) on persistent-area ticks.
         * Stun/sleep on the caster is skipped (bChildAttack).
         */
        struct PersistentAreaAttackQuery {
            /** Caster player id. */
            std::uint32_t owner_id = 0;

            /** Candidate target id (player or monster). */
            std::uint32_t target_id = 0;

            /** True when the candidate is a player. */
            bool target_is_player = false;

            /** True during an PvP duel (compatibility behavior + matching id). */
            bool pvp_duel = false;

            /** True while the caster is flagged PK (compatibility behavior). */
            bool pk_flag = false;

            /** True when the target tile has the village attribute. */
            bool in_village = false;

            /** True when the target tile has the safety attribute. */
            bool in_safety = false;

            /** True during guild war on a castle map. */
            bool guild_war_castle = false;
        };

        /**
         * True when a persistent-area tick may hit this target.
         * Self is never valid. Monsters are always valid. Players follow initial
         * PvP/PK/war gates when those flags are supplied; until OpenKal stores
         * related compatibility states, other players stay eligible (Poison Cloud).
         * @param query Owner, target, and optional PK/PvP flags.
         * @return True when the tick may apply damage.
         */
        [[nodiscard]] inline bool
        can_persistent_area_attack(const PersistentAreaAttackQuery& query) {
            if (query.owner_id == 0 || query.owner_id == query.target_id) {
                return false;
            }
            if (!query.target_is_player) {
                return true;
            }
            const bool has_original_gate =
                query.pvp_duel || query.pk_flag || query.guild_war_castle;
            if (!has_original_gate) {
                return true;
            }
            if (query.pvp_duel || query.guild_war_castle) {
                return true;
            }
            return query.pk_flag && !query.in_village && !query.in_safety;
        }

    }  // namespace combat

    /** Stateless facade for server combat formulas. */
    class Combat final {
      public:
        Combat() = delete;

        /** Returns initial class statistics. */
        [[nodiscard]] static combat::BaseStats class_base_plus_bonus(int clazz);

        /** Applies attack, accuracy, magic and resist revisions. */
        static void apply_stat_revise(std::int32_t level,
                                      const combat::BaseStats& stats,
                                      std::int32_t& min_attack,
                                      std::int32_t& max_attack,
                                      std::int32_t& hit,
                                      std::int32_t& dodge,
                                      std::int32_t& min_magic,
                                      std::int32_t& max_magic,
                                      std::int32_t& res_fire,
                                      std::int32_t& res_ice,
                                      std::int32_t& res_lightning,
                                      std::int32_t& res_curse,
                                      std::int32_t& res_palsy);

        /** Returns PU cost to raise a primary by one. */
        [[nodiscard]] static std::int32_t
        get_need_point(int clazz, int property, std::int32_t current_value);

        /** Cumulative PU cost for pending primary raises. */
        [[nodiscard]] static std::int32_t
        pending_need_points(int clazz, int property, std::int32_t current_value, int pending);

        /** Produces an inclusive pseudo-random integer. */
        [[nodiscard]] static int irand(int lo, int hi);

        /** Looks up the damage level adjustment. */
        [[nodiscard]] static int revise_damage(int abs_level_diff);

        /** Looks up the hit level adjustment. */
        [[nodiscard]] static int revise_hit(int abs_level_diff);

        /** Looks up the hit percentage. */
        [[nodiscard]] static int hit_rate_pct(int clamped_rate);

        /** Rolls a combatant's raw attack. */
        [[nodiscard]] static std::int32_t roll_attack(const combat::Combatant& attacker);

        /** Resolves whether an attack hits. */
        [[nodiscard]] static bool check_hit(const combat::Combatant& attacker,
                                            const combat::Combatant& target,
                                            int add_hit = 0);

        /**
         * Resolves compatibility behavior.
         * @param attacker Attacking combatant.
         * @param target Defending player combatant.
         * @param chance_roll Inclusive 1..100, or -1 to roll.
         * @return True when the attack is blocked.
         */
        [[nodiscard]] static bool check_block(const combat::Combatant& attacker,
                                              const combat::Combatant& target,
                                              int chance_roll = -1);

        /**
         * GetColorLevel for g_nReviseBlock.
         * @param attacker_level Attacker level.
         * @param defender_level Defender level.
         * @return Color-level column 0..6.
         */
        [[nodiscard]] static int color_level(int attacker_level, int defender_level);

        /**
         * Weapon Defend additional nBlock.
         * @param char_level Player level.
         * @return Non-negative block percent points.
         */
        [[nodiscard]] static int knight_weapon_defend_block(int char_level);

        /** Resolves GetFinalDefense for the target. */
        [[nodiscard]] static std::int32_t final_defense(const combat::Combatant& attacker,
                                                        const combat::Combatant& target);

        /** Resolves GetFinalAbsorb for the target. */
        [[nodiscard]] static std::int32_t final_absorb(const combat::Combatant& attacker,
                                                       const combat::Combatant& target);

        /** Resolves final physical damage. */
        [[nodiscard]] static std::int32_t final_damage(const combat::Combatant& attacker,
                                                       const combat::Combatant& target,
                                                       std::int32_t raw_attack);

        /** Resolves a complete physical attack, including fatal/EB. */
        [[nodiscard]] static combat::AttackResult
        resolve_physical(const combat::Combatant& attacker,
                         const combat::Combatant& target,
                         int add_hit = 0,
                         int add_fatal = 0,
                         std::int32_t raw_attack = -1,
                         int revise_tick_ms = -1,
                         int delay_ms = 0,
                         bool skip_hit_check = false);

        /**
         * compatibility behavior extra.
         * @param damage GetFinalDamage result.
         * @param fatal_rate Weapon / passive EB percent.
         * @param fatal_pt Fatal damage coefficient.
         * @param add_fatal Skill GetFatalRate().
         * @param fatal Out: true when the fatal roll succeeded.
         * @return Extra damage, or 0.
         */
        [[nodiscard]] static std::int32_t get_fatal_extra(std::int32_t damage,
                                                          std::int32_t fatal_rate,
                                                          std::int32_t fatal_pt,
                                                          int add_fatal,
                                                          bool& fatal);

        /**
         * compatibility behavior for physical combat skills.
         * @param clazz Attacker class.
         * @param skill_index Skill index.
         * @param skill_level Learned skill level.
         * @param dex Archer DEX.
         * @param base_attack Rolled GetAttack().
         * @param random_roll Optional deterministic formula roll.
         * @param prerequisite_skill_level Learned prerequisite rank for chained Archer skills.
         * @return Skill attack before ReviseTick scaling.
         */
        [[nodiscard]] static std::int32_t skill_physical_power(int clazz,
                                                               int skill_index,
                                                               int skill_level,
                                                               std::int32_t dex,
                                                               std::int32_t base_attack,
                                                               int random_roll = -1,
                                                               int prerequisite_skill_level = -1);

        /** Computes compatibility behavior with current Mortal points. */
        [[nodiscard]] static std::int32_t knight_infernal_power(int skill_level,
                                                                std::int32_t base_attack,
                                                                std::int32_t mortal_points);

        /**
         * compatibility behavior bonus.
         * @param clazz Attacker class.
         * @param skill_index Skill index.
         * @param skill_level Learned skill level.
         * @return Added hit.
         */
        [[nodiscard]] static int skill_physical_hit(int clazz, int skill_index, int skill_level);

        /**
         * compatibility behavior for physical blows that add weapon aspeed.
         * @param clazz Attacker class.
         * @param skill_index Skill index.
         * @param skill_level Learned skill level.
         * @param def_delay_ms skill definition delay[0]+delay[1].
         * @param attack_speed_ms Equipped weapon aspeed.
         * @return Delay milliseconds used by ReviseTick.
         */
        [[nodiscard]] static int skill_physical_delay_ms(
            int clazz, int skill_index, int skill_level, int def_delay_ms, int attack_speed_ms);

        /**
         * compatibility behavior.
         * @param clazz Attacker class.
         * @param skill_index Skill index.
         * @param skill_level Learned skill level.
         * @return Added fatal chance.
         */
        [[nodiscard]] static int skill_physical_fatal(int clazz, int skill_index, int skill_level);

        /** Returns Knight Weapon Mastery property-point contributions. */
        [[nodiscard]] static combat::KnightWeaponMasteryContribution
        knight_weapon_mastery_contribution(int skill_level);

        /** Returns Knight Critical Hit property contributions. */
        [[nodiscard]] static combat::KnightCriticalContribution
        knight_critical_contribution(int skill_level);

        /** Returns the fixed advanced Knight fatal-point contribution. */
        [[nodiscard]] static std::int32_t knight_advanced_critical_fatal_pt();

        /** Returns Knight life-extension HP from revised HTH HP. */
        [[nodiscard]] static std::int32_t knight_life_extension_hp(std::int32_t revised_hth_hp,
                                                                   int skill_level);

        /** Returns Archer Bow Mastery attack contributions. */
        [[nodiscard]] static combat::ArcherBowMasteryContribution
        archer_bow_mastery_contribution(int skill_level);

        /** Returns Archer perfect-evasion passive contributions. */
        [[nodiscard]] static combat::ArcherEvasionContribution
        archer_evasion_contribution(int base_skill_level, int advanced_skill_level);

        /** Returns Archer fatal passive contributions. */
        [[nodiscard]] static combat::ArcherFatalContribution
        archer_fatal_contribution(int fatal_skill_level, bool advanced_active);

        /** Returns Archer Concentration hit points. */
        [[nodiscard]] static std::int32_t archer_concentration_hit(int skill_level);

        /** Returns Archer Range Extension distance. */
        [[nodiscard]] static std::int32_t archer_attack_range_bonus(int skill_level);

        /** Returns Archer Life Extension HP from source base HTH. */
        [[nodiscard]] static std::int32_t archer_life_extension_hp(std::int32_t base_hth,
                                                                   int skill_level);

        /** Computes Shot attack. */
        [[nodiscard]] static std::int32_t
        archer_shot_power(int skill_level, std::int32_t dex, std::int32_t base_attack);

        /** Computes Flame Arrow attack. */
        [[nodiscard]] static std::int32_t archer_flame_arrow_power(int skill_level,
                                                                   std::int32_t base_attack);

        /** Computes Stun Arrow attack with its separately ranked Shot prerequisite. */
        [[nodiscard]] static std::int32_t archer_stun_arrow_power(int skill_level,
                                                                  int shot_skill_level,
                                                                  std::int32_t dex,
                                                                  std::int32_t base_attack);

        /** Computes Explosion attack with its separately ranked Flame Arrow prerequisite. */
        [[nodiscard]] static std::int32_t
        archer_explosion_power(int skill_level, int flame_skill_level, std::int32_t base_attack);

        /** Computes Mystic Arrow attack. */
        [[nodiscard]] static std::int32_t archer_mystic_arrow_power(int skill_level,
                                                                    std::int32_t base_attack);

        /** Computes one Arrow Rain tick against a monster. */
        [[nodiscard]] static std::int32_t
        archer_arrow_rain_power(int skill_level, std::int32_t base_attack, int random_roll = -1);

        /** Applies Arrow Rain's PvP reduce to a tick. */
        [[nodiscard]] static std::int32_t archer_arrow_rain_pvp_damage(std::int32_t damage);

        /** Computes one Poison Cloud tick from the cast-time GetAttack() roll. */
        [[nodiscard]] static std::int32_t archer_poison_cloud_power(int skill_level,
                                                                    std::int32_t base_attack);

        /** Computes one Arrow Explosion splash against a monster. */
        [[nodiscard]] static std::int32_t archer_arrow_explosion_power(int skill_level,
                                                                       std::int32_t base_attack,
                                                                       int random_roll = -1);

        /** Applies Arrow Explosion's PvP reduce to a splash. */
        [[nodiscard]] static std::int32_t archer_arrow_explosion_pvp_damage(std::int32_t damage);

        /** Converts Focus Shot elapsed time to charge tier. */
        [[nodiscard]] static int archer_focus_shot_tier(int focus_ms);

        /** Computes Focus Shot power for an explicit charge tier. */
        [[nodiscard]] static std::int32_t
        archer_focus_shot_power(int skill_level, std::int32_t base_attack, int charge_tier);

        /** Computes Focus Shot hit for an explicit charge tier. */
        [[nodiscard]] static int archer_focus_shot_hit(int skill_level, int charge_tier);

        /** Returns an Archer skill's hostility percentage adjustment. */
        [[nodiscard]] static int archer_skill_hostility(int skill_index);

        /** Returns Mystic Arrow's per-tick Spirit Damage. */
        [[nodiscard]] static std::int32_t archer_mystic_spirit_tick(std::int32_t base_attack);

        /** Returns Curse of Critical Hit's fatal-rate delta. */
        [[nodiscard]] static std::int32_t archer_chance_curse_delta(std::int32_t target_fatal_rate,
                                                                    int skill_level);

        /** Returns true for mage targeted damage skills. */
        [[nodiscard]] static bool is_mage_magic_attack(int skill_index);

        /** Returns the elemental resist used by a mage skill. */
        [[nodiscard]] static combat::MagicResist mage_skill_resist(int skill_index);

        /** Rolls a combatant's base magic. */
        [[nodiscard]] static std::int32_t roll_magic(const combat::Combatant& attacker);

        /** Computes skill magic attack before resist and level revise. */
        [[nodiscard]] static std::int32_t skill_magic_power(int skill_index,
                                                            int skill_level,
                                                            std::int32_t char_level,
                                                            std::int32_t intelligence,
                                                            std::int32_t base_magic,
                                                            std::int32_t lightning_mastery = 0,
                                                            int lightning_skill_level = -1,
                                                            int random_roll = -1);

        /** Computes compatibility behavior for Mage Heal. */
        [[nodiscard]] static std::int32_t
        mage_self_heal_power(int skill_level, std::int32_t char_level, std::int32_t wisdom);

        /** Computes confirmed Mage heal amounts. */
        [[nodiscard]] static std::int32_t mage_heal_power(int skill_index,
                                                          int skill_level,
                                                          std::int32_t char_level,
                                                          std::int32_t wisdom,
                                                          std::int32_t target_max_hp,
                                                          int random_roll = -1);

        /** Converts monster definition resist into GetFinalResist. */
        [[nodiscard]] static std::int32_t monster_final_resist(std::int32_t raw_resist);

        /** Returns the percent of magic that gets through. */
        [[nodiscard]] static std::int32_t magic_through_percent(std::int32_t attacker_level,
                                                                std::int32_t target_level,
                                                                std::int32_t final_resist);

        /** Resolves magic GetFinalDamage (level gap only; no defense). */
        [[nodiscard]] static std::int32_t final_magic_damage(std::int32_t attacker_level,
                                                             std::int32_t target_level,
                                                             std::int32_t raw_attack);

        /**
         * compatibility behavior: elapsed portion of the skill delay.
         * @param delay_ms GetDelay.
         * @param now_ms Current monotonic millisecond tick.
         * @param next_full_ms In/out next full-delay time.
         * @return Applied delay milliseconds.
         */
        [[nodiscard]] static int
        revise_skill_tick(int delay_ms, std::uint64_t now_ms, std::uint64_t& next_full_ms);

        /**
         * Scales a magic payload by ReviseTick / GetDelay.
         * @param value Unscaled resisted magic.
         * @param revise_tick_ms ReviseTick result.
         * @param delay_ms GetDelay.
         * @return Integer-scaled value.
         */
        [[nodiscard]] static std::int32_t
        scale_by_skill_delay(std::int32_t value, int revise_tick_ms, int delay_ms);

        /**
         * Resolves one mage skill attack (never misses).
         * @param attacker Caster combatant.
         * @param target Target combatant.
         * @param skill_index Skill index.
         * @param skill_level Learned skill level.
         * @param intelligence Caster INT.
         * @param lightning_mastery Lightning mastery points.
         * @param revise_tick_ms Elapsed cast delay; a negative value uses the full delay.
         * @param delay_ms GetDelay; 0 skips delay scaling.
         * @param lightning_skill_level Learned Lightning rank used by Spark.
         * @return Always-hit result; damage may be 0 from level gap or resist.
         */
        [[nodiscard]] static combat::AttackResult resolve_magic(const combat::Combatant& attacker,
                                                                const combat::Combatant& target,
                                                                int skill_index,
                                                                int skill_level,
                                                                std::int32_t intelligence,
                                                                std::int32_t lightning_mastery = 0,
                                                                int revise_tick_ms = -1,
                                                                int delay_ms = 0,
                                                                int lightning_skill_level = -1);

        /** Tests a squared two-dimensional attack range. */
        [[nodiscard]] static bool
        in_attack_range(int attacker_x, int attacker_y, int target_x, int target_y, int range);
    };

}  // namespace openkal::server
