/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/Combat.h"

#include "openkal/protocol/ArcherSkill.h"
#include "openkal/protocol/BuffState.h"
#include "openkal/protocol/Progression.h"
#include "openkal/protocol/ThiefSkill.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>

namespace openkal::server {
    namespace combat {

        BaseStats class_base_plus_bonus(int clazz) {
            // Primary-stat baselines before the common +1; the Thief fallback is DEX-focused.
            static constexpr int kBase[4][5] = {
                {18, 16, 8, 8, 10},   // knight
                {8, 10, 18, 16, 8},   // mage
                {14, 10, 8, 10, 18},  // archer
                {12, 12, 8, 8, 20},   // thief (placeholder)
            };
            const int c = std::clamp(clazz, 0, 3);
            BaseStats s;
            s.str = kBase[c][0] + 1;
            s.hth = kBase[c][1] + 1;
            s.intl = kBase[c][2] + 1;
            s.wis = kBase[c][3] + 1;
            s.dex = kBase[c][4] + 1;
            return s;
        }

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
                               std::int32_t& res_palsy) {
            const std::int64_t lv = level;
            const std::int64_t str = s.str;
            const std::int64_t hth = s.hth;
            const std::int64_t intl = s.intl;
            const std::int64_t wis = s.wis;
            const std::int64_t dex = s.dex;
            auto add = [](std::int32_t& dst, std::int64_t delta) {
                dst = clamp_nonneg_i32(static_cast<std::int64_t>(dst) + delta);
            };
            add(min_atk, (dex - 5) / 11 + (11 * str - 80) / 30 + 7 * lv / 10 + 1);
            add(max_atk, 18 * dex / 77 + (8 * str - 25) / 15 + lv);
            add(hit, dex / 8 + 15 * str / 54);
            add(dodge, dex / 3);
            add(min_magic, wis / 7 + (7 * intl - 20) / 12 - 3);
            add(max_magic, 14 * wis / 45 + 7 * intl / 12 - 3);
            add(res_fire, intl / 9);
            add(res_ice, intl / 9);
            add(res_lightning, intl / 9);
            add(res_curse, wis / 9);
            add(res_palsy, hth / 9);
        }

        std::int32_t max_hp_from_stats(std::int32_t hth, std::int32_t level, int clazz) {
            const std::int64_t deno = std::max<std::int64_t>(1, kDenoHp[std::clamp(clazz, 0, 3)]);
            const std::int64_t h = hth;
            const std::int64_t lv = level;
            const std::int32_t out = clamp_nonneg_i32(2 * h * h / deno + 115 + 52 * lv / 3);
            return out < 1 ? 1 : out;
        }

        std::int32_t monster_max_hp(std::int32_t hp_base, std::int32_t hth, std::int32_t level) {
            const std::int64_t h = hth;
            const std::int64_t lv = level;
            const std::int32_t out = clamp_nonneg_i32(static_cast<std::int64_t>(hp_base) +
                                                      52 * lv / 3 + 2 * h * h / 10 + 115);
            return out < 1 ? 1 : out;
        }

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
                                       std::int32_t& max_hp) {
            auto scale_init_resist = [](std::int32_t& resist) {
                resist = clamp_nonneg_i32(10 * static_cast<std::int64_t>(resist));
            };
            scale_init_resist(res_fire);
            scale_init_resist(res_ice);
            scale_init_resist(res_lightning);
            scale_init_resist(res_curse);
            scale_init_resist(res_palsy);
            apply_stat_revise(level,
                              stats,
                              min_atk,
                              max_atk,
                              hit,
                              dodge,
                              min_magic,
                              max_magic,
                              res_fire,
                              res_ice,
                              res_lightning,
                              res_curse,
                              res_palsy);
            max_hp = monster_max_hp(hp_base, stats.hth, level);
        }

        std::int32_t max_mp_from_stats(std::int32_t wis, std::int32_t level, int clazz) {
            const std::int64_t deno = std::max<std::int64_t>(1, kDenoMp[std::clamp(clazz, 0, 3)]);
            const std::int64_t w = wis;
            const std::int64_t lv = level;
            const std::int32_t out = clamp_nonneg_i32(w + 2 * w * w / deno + 8 * lv + 140);
            return out < 1 ? 1 : out;
        }

        namespace {

            constexpr int kPtUpPrtyA[] = {
                1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2,
                2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
                3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5,
                5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
                6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
                7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
                7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
                8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
            };

            constexpr int kPtUpPrtyB[] = {
                1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
                2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
                3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
                4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
                5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
                6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
                7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
            };

            [[nodiscard]] bool uses_primary_curve(int clazz, int property) {
                // Knight STR, Mage INT, Archer DEX use the cheaper B table (initial
                // GetNeedPoint).
                if (clazz == kPcKnight && property == static_cast<int>(PrimaryStat::Str)) {
                    return true;
                }
                if (clazz == kPcMage && property == static_cast<int>(PrimaryStat::Int)) {
                    return true;
                }
                if (clazz == kPcArcher && property == static_cast<int>(PrimaryStat::Dex)) {
                    return true;
                }
                return false;
            }

        }  // namespace

        std::int32_t get_need_point(int clazz, int property, std::int32_t current_value) {
            if (property < 0 || property > static_cast<int>(PrimaryStat::Dex)) {
                return 0;
            }
            constexpr int kALen = static_cast<int>(sizeof(kPtUpPrtyA) / sizeof(kPtUpPrtyA[0]));
            constexpr int kBLen = static_cast<int>(sizeof(kPtUpPrtyB) / sizeof(kPtUpPrtyB[0]));
            static_assert(kALen == 250 && kBLen == 250, "PU cost tables must match MainSvr length");
            if (current_value < 0 || current_value >= kALen) {
                return 0;
            }
            if (uses_primary_curve(clazz, property)) {
                return kPtUpPrtyB[current_value];
            }
            return kPtUpPrtyA[current_value];
        }

        std::int32_t
        pending_need_points(int clazz, int property, std::int32_t current_value, int pending) {
            std::int64_t total = 0;
            std::int32_t cur = current_value;
            for (int i = 0; i < pending; ++i) {
                const std::int32_t need = get_need_point(clazz, property, cur);
                if (need <= 0) {
                    break;
                }
                total += need;
                ++cur;
            }
            return clamp_nonneg_i32(total);
        }

        int spend_primary_points(int clazz,
                                 int property,
                                 int requested,
                                 std::int32_t& current_value,
                                 std::int32_t& pu_point) {
            int applied = 0;
            const int count = std::clamp(requested, 0, 255);
            while (applied < count) {
                const std::int32_t need = get_need_point(clazz, property, current_value);
                if (need <= 0 || pu_point < need) {
                    break;
                }
                pu_point -= need;
                ++current_value;
                ++applied;
            }
            return applied;
        }

        int irand(int lo, int hi) {
            if (hi <= lo) {
                return lo;
            }
            return lo + (std::rand() % (hi - lo + 1));
        }

        int revise_damage(int abs_level_diff) {
            static constexpr int kTable[] = {
                0,  0,  0,  1,  1,  3,  3,  6,  6,  9,  9,  12, 12, 15, 15, 18, 18, 21, 21, 24,
                24, 27, 27, 30, 30, 33, 33, 36, 36, 39, 39, 42, 42, 45, 45, 48, 48, 51, 51, 54,
                54, 57, 57, 60, 60, 63, 63, 66, 66, 69, 69, 72, 72, 75, 75, 78, 78, 81, 81, 84,
                84, 87, 87, 90, 90, 93, 93, 96, 96, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
                99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99};
            const int i = std::clamp(abs_level_diff, 0, 99);
            return kTable[i];
        }

        int revise_hit(int abs_level_diff) {
            static constexpr int kTable[] = {
                0,  0,  0,  0,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16,
                17, 18, 19, 20, 22, 24, 26, 28, 30, 32, 34, 36, 38, 40, 43, 46, 49, 52, 55, 58, 61,
                64, 67, 70, 74, 78, 82, 86, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90,
                90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90,
                90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 0};
            const int i = std::clamp(abs_level_diff, 0, 101);
            return kTable[i];
        }

        int hit_rate_pct(int clamped_rate) {
            static constexpr int kTable[] = {50, 52, 52, 54, 54, 56, 56, 58, 58, 60, 60, 62, 62,
                                             64, 64, 66, 66, 68, 68, 70, 70, 72, 72, 74, 74, 76,
                                             76, 78, 78, 80, 80, 82, 82, 84, 84, 86, 86, 88, 88,
                                             90, 90, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 0};
            const int i = std::clamp(std::abs(clamped_rate), 0, 51);
            return kTable[i];
        }

        std::int32_t roll_attack(const Combatant& a) {
            return irand(a.min_attack, a.max_attack);
        }

        std::int32_t roll_magic(const Combatant& attacker) {
            return irand(attacker.min_magic, attacker.max_magic);
        }

        std::int32_t mage_channel_power(int skill_level, std::int32_t magic, int grade_roll) {
            const int roll = grade_roll >= 0 ? std::clamp(grade_roll, 5, 10) : irand(5, 10);
            return clamp_nonneg_i32(7LL * std::max(0, magic) +
                                    static_cast<std::int64_t>(std::max(1, skill_level)) * roll);
        }

        std::int32_t mage_intensification_bonus(int rank,
                                                std::int32_t magic,
                                                bool target_is_player,
                                                int grade_roll) {
            if (rank <= 0) {
                return 0;
            }
            const int roll = grade_roll >= 0 ? std::clamp(grade_roll, 5, 10) : irand(5, 10);
            const std::int64_t bonus =
                2LL * std::max(0, magic) + static_cast<std::int64_t>(rank) * roll;
            return clamp_nonneg_i32(target_is_player ? bonus * 150 / 100 : bonus);
        }

        bool is_mage_magic_attack(int skill_index) {
            switch (skill_index) {
            case 2:   // Fire Magic
            case 3:   // Ice Magic
            case 4:   // Lightning Magic
            case 9:   // Shock Wave
            case 23:  // Lightning Blow
            case 24:  // Splashy Ice
            case 25:  // Fire Blow
            case 31:  // Lightning Summons
            case 32:  // Hail
            case 33:  // Flame Bomb
            case 40:  // Soul Destruction
            case 41:  // Chain Lightning
            case 42:  // Thunder
            case 44:  // Ice Restriction
            case 46:  // Explosion
            case 47:  // Explosive Burst
            case 43:  // Thunder Storm tick
            case 45:  // Ice Storm tick
            case 48:  // Fire Storm tick
                return true;
            default:
                return false;
            }
        }

        MagicResist mage_skill_resist(int skill_index) {
            switch (skill_index) {
            case 2:
            case 25:
            case 33:
            case 46:
            case 47:
            case 48:
                return MagicResist::Fire;
            case 3:
            case 24:
            case 27:
            case 32:
            case 44:
            case 45:
                return MagicResist::Ice;
            case 4:
            case 9:
            case 23:
            case 31:
            case 41:
            case 42:
            case 43:
                return MagicResist::Lightning;
            default:
                return MagicResist::Palsy;
            }
        }

        namespace {

            [[nodiscard]] std::int32_t clamp_skill_level(int skill_level) {
                return std::max(1, skill_level);
            }

            [[nodiscard]] std::int32_t cap_magic(std::int64_t value, std::int64_t cap) {
                return clamp_nonneg_i32((value >= cap) ? cap : value);
            }

            [[nodiscard]] int target_resist_for(const Combatant& target, MagicResist resist) {
                switch (resist) {
                case MagicResist::Fire:
                    return target.res_fire;
                case MagicResist::Ice:
                    return target.res_ice;
                case MagicResist::Lightning:
                    return target.res_lightning;
                case MagicResist::Curse:
                    return target.res_curse;
                case MagicResist::Palsy:
                default:
                    return target.res_palsy;
                }
            }

            struct MagicInputs {
                std::int64_t rank = 1;
                std::int64_t actorLevel = 0;
                std::int64_t intellect = 0;
                std::int64_t baseline = 0;
                std::int64_t masteryBonus = 0;
            };

            struct FixedMagicRule {
                int abilityId = 0;
                int levelNumerator = 0;
                int levelDenominator = 1;
                int intellectNumerator = 0;
                int intellectDenominator = 1;
                int flatPower = 0;
                int capPerRank = 0;
                int flatCap = 0;
                int bonusPerRank = 0;
                int outputPercent = 100;
                bool addBaseline = true;
            };

            constexpr std::array<FixedMagicRule, 12> kFixedMagicRules = {{
                {2, 3, 2, 3, 10, 0, 21, 30, 2, 100, true},
                {3, 3, 2, 3, 13, 0, 16, 30, 0, 100, true},
                {24, 5, 3, 5, 7, 0, 40, 120, 0, 100, true},
                {25, 5, 3, 6, 5, 0, 70, 150, 8, 100, true},
                {32, 3, 2, 10, 4, 240, 150, 400, 0, 100, true},
                {33, 3, 2, 10, 3, 380, 150, 500, 0, 100, true},
                {40, 9, 2, 5, 1, 0, 200, 600, 0, 100, true},
                {44, 3, 2, 2, 6, 480, 0, 500, 0, 100, true},
                {46, 3, 2, 14, 3, 580, 300, 600, 0, 100, true},
                {47, 6, 2, 4, 5, 580, 0, 900, 0, 100, true},
                {45, 3, 2, 10, 4, 240, 0, 0, 0, 60, false},
                {48, 3, 2, 14, 3, 380, 0, 0, 0, 60, false},
            }};

            [[nodiscard]] std::int64_t evaluate_fixed_magic(const FixedMagicRule& rule,
                                                            const MagicInputs& input) {
                const std::int64_t levelPart =
                    rule.levelNumerator * input.actorLevel / rule.levelDenominator;
                const std::int64_t intellectPart = rule.intellectNumerator * input.rank *
                                                   input.intellect / rule.intellectDenominator;
                std::int64_t power = (levelPart + intellectPart) / 2 + rule.flatPower;
                if (rule.capPerRank != 0 || rule.flatCap != 0) {
                    power = std::min(power, rule.capPerRank * input.rank + rule.flatCap);
                }
                power = power * rule.outputPercent / 100;
                if (rule.addBaseline) {
                    power += input.baseline + rule.bonusPerRank * input.rank;
                }
                return power;
            }

            struct BandFormula {
                int levelNumerator = 0;
                int levelDenominator = 1;
                int intellectNumerator = 0;
                int intellectDenominator = 1;
                int flatPower = 0;
                bool halveCombinedTerms = false;
                bool halveIntellectTerm = false;
                bool amplifyIntellect = false;
                bool includeMastery = false;
            };

            struct RandomMagicRule {
                int abilityId = 0;
                BandFormula lower;
                BandFormula upper;
                int lowerCapPerRank = 0;
                int lowerFlatCap = 0;
                int upperCapPerRank = 0;
                int upperFlatCap = 0;
            };

            constexpr std::array<RandomMagicRule, 6> kRandomMagicRules = {{
                {4,
                 {3, 2, 2, 16, 0, true, false, false, false},
                 {3, 2, 2, 16, 0, true, false, false, true},
                 12,
                 30,
                 12,
                 180},
                {9,
                 {5, 3, 6, 4, 50, true, false, false, false},
                 {5, 3, 11, 3, 150, true, false, false, true},
                 60,
                 200,
                 200,
                 200},
                {31,
                 {3, 2, 2, 7, 320, false, true, false, true},
                 {3, 2, 10, 2, 320, false, true, false, true},
                 1,
                 450,
                 200,
                 700},
                {41,
                 {3, 2, 7, 3, 360, false, true, false, true},
                 {3, 2, 10, 4, 700, false, true, false, true},
                 0,
                 700,
                 0,
                 1000},
                {42,
                 {3, 2, 5, 3, 360, false, true, true, true},
                 {3, 2, 10, 2, 700, false, true, true, true},
                 154,
                 456,
                 300,
                 900},
                {43,
                 {3, 2, 5, 3, 360, false, true, true, true},
                 {3, 2, 10, 2, 700, false, true, true, true},
                 0,
                 0,
                 0,
                 0},
            }};

            [[nodiscard]] std::int64_t evaluate_band(const BandFormula& formula,
                                                     const MagicInputs& input) {
                const std::int64_t effectiveIntellect =
                    formula.amplifyIntellect ? 3 * input.intellect / 2 : input.intellect;
                const std::int64_t levelPart =
                    formula.levelNumerator * input.actorLevel / formula.levelDenominator;
                std::int64_t intellectPart = formula.intellectNumerator * input.rank *
                                             effectiveIntellect / formula.intellectDenominator;
                if (formula.halveIntellectTerm) {
                    intellectPart /= 2;
                }
                std::int64_t power = levelPart + intellectPart;
                if (formula.halveCombinedTerms) {
                    power /= 2;
                }
                if (formula.includeMastery) {
                    power += input.masteryBonus;
                }
                return power + formula.flatPower;
            }

            [[nodiscard]] std::pair<std::int64_t, std::int64_t>
            evaluate_random_magic(const RandomMagicRule& rule, const MagicInputs& input) {
                std::int64_t lower = evaluate_band(rule.lower, input);
                std::int64_t upper = evaluate_band(rule.upper, input);
                if (rule.lowerCapPerRank != 0 || rule.lowerFlatCap != 0) {
                    lower = std::min(lower, rule.lowerCapPerRank * input.rank + rule.lowerFlatCap);
                }
                if (rule.upperCapPerRank != 0 || rule.upperFlatCap != 0) {
                    upper = std::min(upper, rule.upperCapPerRank * input.rank + rule.upperFlatCap);
                }
                return {lower, upper};
            }

        }  // namespace

        std::int32_t skill_magic_power(int ability_id,
                                       int ability_rank,
                                       std::int32_t actor_level,
                                       std::int32_t intellect,
                                       std::int32_t baseline_power,
                                       std::int32_t storm_mastery,
                                       int prerequisite_rank,
                                       int random_roll) {
            const MagicInputs input = {
                clamp_skill_level(ability_rank),
                actor_level,
                intellect,
                baseline_power,
                20 * std::max<std::int64_t>(0, storm_mastery),
            };

            const auto fixedRule = std::find_if(
                kFixedMagicRules.begin(), kFixedMagicRules.end(), [ability_id](const auto& rule) {
                    return rule.abilityId == ability_id;
                });
            if (fixedRule != kFixedMagicRules.end()) {
                return clamp_nonneg_i32(evaluate_fixed_magic(*fixedRule, input));
            }

            const auto randomRule = std::find_if(
                kRandomMagicRules.begin(), kRandomMagicRules.end(), [ability_id](const auto& rule) {
                    return rule.abilityId == ability_id;
                });
            if (randomRule != kRandomMagicRules.end()) {
                const auto [lower, upper] = evaluate_random_magic(*randomRule, input);
                return clamp_nonneg_i32(input.baseline +
                                        irand(static_cast<int>(lower), static_cast<int>(upper)));
            }

            if (ability_id == 23) {
                const int requiredRank =
                    prerequisite_rank >= 0 ? prerequisite_rank : static_cast<int>(input.rank);
                if (requiredRank <= 0) {
                    return 0;
                }
                const std::int64_t prerequisitePower =
                    skill_magic_power(4, requiredRank, actor_level, intellect, 0, storm_mastery);
                std::int64_t lower =
                    3 * input.actorLevel / 2 + 3 * input.rank * input.intellect / 10 / 2 + 10;
                std::int64_t upper = prerequisitePower + 3 * input.actorLevel / 2 +
                                     5 * input.rank * input.intellect / 8 / 2 + 30 +
                                     input.masteryBonus;
                lower = std::min(lower, 30 * input.rank + 150);
                upper = std::min(upper, 40 * input.rank + 200);
                const int selected =
                    random_roll >= 0
                        ? std::clamp(random_roll, static_cast<int>(lower), static_cast<int>(upper))
                        : irand(static_cast<int>(lower), static_cast<int>(upper));
                return clamp_nonneg_i32(input.baseline + selected);
            }

            return clamp_nonneg_i32(input.baseline);
        }

        std::int32_t
        mage_self_heal_power(int skill_level, std::int32_t char_level, std::int32_t wisdom) {
            const std::int64_t slv = clamp_skill_level(skill_level);
            const std::int64_t level_term = 15 * static_cast<std::int64_t>(char_level) / 10;
            const std::int64_t wisdom_term = static_cast<std::int64_t>(wisdom) * (5 * slv) / 10;
            return clamp_nonneg_i32(100 + (level_term + wisdom_term) / 2);
        }

        std::int32_t mage_heal_power(int skill_index,
                                     int skill_level,
                                     std::int32_t char_level,
                                     std::int32_t wisdom,
                                     std::int32_t target_max_hp,
                                     int random_roll) {
            const std::int64_t slv = clamp_skill_level(skill_level);
            const std::int64_t lv = char_level;
            const std::int64_t wis = wisdom;
            const std::int64_t max_hp = std::max<std::int64_t>(0, target_max_hp);
            switch (skill_index) {
            case 8:
                return mage_self_heal_power(skill_level, char_level, wisdom);
            case 10:
                return clamp_nonneg_i32(30 * slv + 70 + (7 * lv / 11 + 3 * slv * wis / 11) / 2);
            case 22:
                return clamp_nonneg_i32(50 * slv + 500 + (8 * lv / 11 + 5 * slv * wis / 9) / 2);
            case 28: {
                const int max_roll = static_cast<int>(10 * slv);
                const int roll =
                    random_roll >= 0 ? std::clamp(random_roll, 0, max_roll) : irand(0, max_roll);
                const std::int64_t amount = 60 * slv + (lv / 2 + 8 * slv * wis / 7) + roll + 100;
                return cap_magic(amount, 180 * slv);
            }
            case 29:
                return clamp_nonneg_i32(max_hp / 4);
            case 35: {
                const int roll = random_roll >= 0 ? std::clamp(random_roll, 0, 100) : irand(0, 100);
                return cap_magic(roll + 100 * slv + 11 * wis / 3 + 750, 1600);
            }
            case 55:
                return clamp_nonneg_i32(max_hp * (20 * slv + 15) / 100);
            case 57: {
                const int roll = random_roll >= 0 ? std::clamp(random_roll, 0, 100) : irand(0, 100);
                return clamp_nonneg_i32(8 * lv / 11 + roll + 550);
            }
            case 58:  // compatibility behavior uses a percentage above 100; max-HP clamping makes
                      // it a full heal.
            case 59:
                return clamp_nonneg_i32(max_hp);
            default:
                return 0;
            }
        }

        std::int32_t monster_final_resist(std::int32_t raw_resist) {
            return clamp_nonneg_i32(static_cast<std::int64_t>(raw_resist) / 10);
        }

        std::int32_t magic_through_percent(std::int32_t attacker_level,
                                           std::int32_t target_level,
                                           std::int32_t final_resist) {
            const std::int64_t diff = static_cast<std::int64_t>(attacker_level) - target_level;
            std::int64_t calc = static_cast<std::int64_t>(final_resist) - diff * std::abs(diff) / 6;
            if (calc < 0) {
                calc = 0;
            }
            if (calc >= 100) {
                return 0;
            }
            return static_cast<std::int32_t>(100 - calc);
        }

        std::int32_t final_magic_damage(std::int32_t attacker_level,
                                        std::int32_t target_level,
                                        std::int32_t raw_attack) {
            const std::int64_t diff = static_cast<std::int64_t>(attacker_level) - target_level;
            return clamp_nonneg_i32(static_cast<std::int64_t>(raw_attack) +
                                    diff * std::abs(diff) / 4);
        }

        int revise_skill_tick(int delay_ms, std::uint64_t now_ms, std::uint64_t& next_full_ms) {
            const int delay = std::max(1, delay_ms);
            const std::uint64_t prev = next_full_ms;
            next_full_ms = now_ms + static_cast<std::uint64_t>(delay);
            if (now_ms >= prev) {
                return delay;
            }
            const std::uint64_t remaining = prev - now_ms;
            return remaining >= static_cast<std::uint64_t>(delay)
                       ? 0
                       : delay - static_cast<int>(remaining);
        }

        std::int32_t scale_by_skill_delay(std::int32_t value, int revise_tick_ms, int delay_ms) {
            if (delay_ms <= 0) {
                return clamp_nonneg_i32(value);
            }
            const std::int64_t revise = std::clamp(revise_tick_ms, 0, delay_ms);
            return clamp_nonneg_i32(revise * static_cast<std::int64_t>(value) / delay_ms);
        }

        AttackResult resolve_magic(const Combatant& attacker,
                                   const Combatant& target,
                                   int skill_index,
                                   int skill_level,
                                   std::int32_t intelligence,
                                   std::int32_t lightning_mastery,
                                   int revise_tick_ms,
                                   int delay_ms,
                                   int lightning_skill_level) {
            AttackResult r;
            r.hit = true;
            if (!is_mage_magic_attack(skill_index)) {
                return r;
            }
            if (delay_ms > 0 && revise_tick_ms == 0) {
                return r;
            }
            const std::int32_t base_magic = roll_magic(attacker);
            const std::int32_t n_magic = skill_magic_power(skill_index,
                                                           skill_level,
                                                           attacker.level,
                                                           intelligence,
                                                           base_magic,
                                                           lightning_mastery,
                                                           lightning_skill_level);
            const MagicResist element = mage_skill_resist(skill_index);
            const std::int32_t through =
                magic_through_percent(attacker.level,
                                      target.level,
                                      monster_final_resist(target_resist_for(target, element)));
            std::int32_t resisted =
                clamp_nonneg_i32(static_cast<std::int64_t>(through) * n_magic / 100);
            if (delay_ms > 0) {
                const int revise = revise_tick_ms < 0 ? delay_ms : revise_tick_ms;
                resisted = scale_by_skill_delay(resisted, revise, delay_ms);
            }
            r.damage = final_magic_damage(attacker.level, target.level, resisted);
            if (attacker.magic_intensification_rank > 0 && skill_index != 24 && skill_index != 43 &&
                skill_index != 44 && skill_index != 45 && skill_index != 48) {
                // GetFinalDamage first replaces a non-positive result, then adds a second roll.
                if (r.damage <= 0) {
                    r.damage = mage_intensification_bonus(attacker.magic_intensification_rank,
                                                          roll_magic(attacker),
                                                          target.is_player);
                }
                r.damage =
                    clamp_nonneg_i32(static_cast<std::int64_t>(r.damage) +
                                     mage_intensification_bonus(attacker.magic_intensification_rank,
                                                                roll_magic(attacker),
                                                                target.is_player));
            }
            if (target.armor_broken && r.damage > 100) {
                r.damage = clamp_nonneg_i32(static_cast<std::int64_t>(r.damage) + r.damage / 20);
            }
            if ((attacker.bstate & protocol::kCbsPayUpAttack) != 0) {
                r.damage = clamp_nonneg_i32(static_cast<std::int64_t>(r.damage) + r.damage / 10);
            }
            r.palsy = get_palsy_extra(attacker, target);
            if (r.palsy > 0) {
                r.explosive = true;
                r.damage = clamp_nonneg_i32(static_cast<std::int64_t>(r.damage) + r.palsy);
            }
            return r;
        }

        AttackResult resolve_monster_magic(const Combatant& attacker,
                                           const Combatant& target,
                                           int skill_index,
                                           int skill_level,
                                           std::int32_t intelligence) {
            AttackResult result;
            result.hit = true;
            const std::int64_t level = std::max(1, attacker.level);
            const std::int64_t rank = std::max(1, skill_level);
            const std::int64_t intl = std::max(0, intelligence);
            std::int64_t raw = 0;
            MagicResist resist = MagicResist::Palsy;

            switch (skill_index) {
            case 29: {  // compatibility behavior
                const std::int64_t value = (3 * level / 2 + 20 * intl / 16) / 2;
                const int low = static_cast<int>(std::min<std::int64_t>(value, 150));
                const int high = static_cast<int>(std::min<std::int64_t>(value, 300));
                raw = irand(std::min(low, high), std::max(low, high));
                resist = MagicResist::Lightning;
                break;
            }
            case 30: {  // compatibility behavior
                const std::int64_t low =
                    std::min<std::int64_t>(300, 3 * level / 2 + 3 * rank * intl / 10 / 2 + 10);
                const std::int64_t high =
                    std::min<std::int64_t>(400, 3 * level / 2 + 5 * rank * intl / 8 / 2 + 30);
                raw = irand(static_cast<int>(std::min(low, high)),
                            static_cast<int>(std::max(low, high)));
                resist = MagicResist::Lightning;
                break;
            }
            case 31:  // compatibility behavior
                raw = 3 * level / 2 + 60 * intl / 10;
                resist = MagicResist::Fire;
                break;
            case 32: {  // compatibility behavior
                const std::int64_t low = std::min<std::int64_t>(
                    60 * rank + 200, (5 * level / 3 + 6 * rank * intl / 4) / 2 + 50);
                const std::int64_t high = std::min<std::int64_t>(
                    200 * rank + 200, (5 * level / 3 + 11 * rank * intl / 3) / 2 + 150);
                raw = irand(static_cast<int>(std::min(low, high)),
                            static_cast<int>(std::max(low, high)));
                resist = MagicResist::Lightning;
                break;
            }
            case 33:  // compatibility behavior
                raw = std::min<std::int64_t>(70 * rank + 150,
                                             (5 * level / 3 + 6 * rank * intl / 5) / 2) +
                      8 * rank;
                resist = MagicResist::Fire;
                break;
            case 34:  // compatibility behavior
                raw = roll_attack(attacker);
                resist = MagicResist::Fire;
                break;
            default:
                return result;
            }

            const std::int32_t through =
                magic_through_percent(attacker.level,
                                      target.level,
                                      monster_final_resist(target_resist_for(target, resist)));
            const std::int32_t resisted =
                clamp_nonneg_i32(static_cast<std::int64_t>(through) * raw / 100);
            result.damage = final_magic_damage(attacker.level, target.level, resisted);
            return result;
        }

        bool is_monster_magic_skill(int skill_index) {
            switch (skill_index) {
            case 29:
            case 30:
            case 31:
            case 32:
            case 33:
            case 34:
                return true;
            default:
                return false;
            }
        }

        int color_level(int attacker_level, int defender_level) {
            const int pl = std::clamp(defender_level, 1, protocol::kMaxPlayerLevel);
            const int al = std::max(1, attacker_level);
            const int band = std::clamp((pl - 1) / 10, 0, 9);
            const int diff = al - pl;
            if (diff < 0) {
                const int idx = std::min(std::abs(diff), 20);
                const int value = protocol::kReviseExpB[band][idx];
                if (value == 100) {
                    return 0;
                }
                if (value == 50) {
                    return 1;
                }
                if (value == 25) {
                    return 2;
                }
                return 3;
            }
            const int idx = std::min(diff, 20);
            const int value = protocol::kReviseExpA[band][idx];
            if (value == 100) {
                return 6;
            }
            if (value == 50) {
                return 5;
            }
            if (value == 25) {
                return 4;
            }
            return 3;
        }

        int knight_weapon_defend_block(int char_level) {
            const int level = std::max(1, char_level);
            return std::max(0, (5 + (level - 69)) / 2);
        }

        bool check_block(const Combatant& attacker, const Combatant& target, int chance_roll) {
            if (!target.is_player) {
                return false;
            }
            int n_block = target.block_percent;
            if (n_block <= 0 || target.clazz == kPcMage) {
                return false;
            }
            const int roll = (chance_roll >= 1) ? chance_roll : irand(1, 100);
            const int color = std::clamp(color_level(attacker.level, target.level), 0, 6);
            if (target.clazz == kPcArcher) {
                if (!attacker.is_player) {
                    return kReviseBlock[kPcArcher][color] + n_block >= roll;
                }
                return n_block >= roll;
            }
            if (!target.wearing_shield) {
                if (!target.has_second_job_attrib) {
                    return false;
                }
                if (!target.wearing_two_hand || !target.has_parry) {
                    return false;
                }
            }
            if (attacker.is_player) {
                return n_block + target.level / 3 > roll;
            }
            const int block_type = target.wearing_two_hand ? 1 : 0;
            if (attacker.attack_type == kAttackRange) {
                if (block_type != 0) {
                    n_block = (n_block + 3) / 3;
                } else {
                    n_block += 3;
                }
            }
            return kReviseBlock[block_type][color] + n_block >= roll;
        }

        bool check_hit(const Combatant& attacker, const Combatant& target, int add_hit) {
            // related compatibility states: Rage always hits.
            if ((attacker.bstate & protocol::kCbsSkillAnimalRage) != 0) {
                return true;
            }
            int effective_add = add_hit;
            // compatibility behavior: +10 vs monsters. compatibility behavior: CheckHit(..., -5).
            if (!target.is_player && (attacker.bstate & protocol::kCbsPayUpHit) != 0) {
                effective_add += 10;
            }
            if (target.is_player && (target.bstate & protocol::kCbsPayUpDodge) != 0) {
                effective_add -= 5;
            }
            const std::int64_t diff = static_cast<std::int64_t>(attacker.level) - target.level;
            const std::int64_t point = (diff < 0) ? -revise_hit(static_cast<int>(-diff))
                                                  : revise_hit(static_cast<int>(diff));
            std::int64_t rate =
                static_cast<std::int64_t>(effective_add) + point + attacker.hit - target.dodge;
            rate = std::clamp(rate, std::int64_t{-41}, std::int64_t{41});
            const int pct = hit_rate_pct(static_cast<int>(rate));
            if (rate < 0) {
                return (100 - pct) >= irand(1, 100);
            }
            return pct >= irand(1, 100);
        }

        std::int32_t final_defense(const Combatant& attacker, const Combatant& target) {
            const int type = (attacker.attack_type != 0) ? kAttackRange : kAttackMelee;
            std::int64_t defense = (type == kAttackRange) ? target.defense_range : target.defense;
            if (target.is_player && attacker.is_player) {
                const int clazz = std::clamp(target.clazz, 0, 2);
                const int deno = std::max(1, kDenoDef[type][clazz]);
                defense += static_cast<std::int64_t>(target.level) / deno;
            }
            return clamp_nonneg_i32(defense);
        }

        std::int32_t final_absorb(const Combatant& attacker, const Combatant& target) {
            std::int64_t absorb = target.absorb;
            if (target.is_player && attacker.is_player) {
                absorb += kPvpAbsorbBonus;
            }
            return clamp_nonneg_i32(absorb);
        }

        std::int64_t mitigate_defense(std::int64_t attack, std::int32_t defense) {
            if (attack <= 0) {
                return 0;
            }
            const std::int64_t total = std::max<std::int64_t>(0, defense);
            if (attack >= (5 * total + 2) / 3) {
                return attack - total;
            }
            const auto denominator = static_cast<std::uint64_t>(5 * (5 * total - attack));
            // The tail bounds attack below 5 * INT32_MAX / 3, so its square fits uint64.
            const auto square =
                static_cast<std::uint64_t>(attack) * static_cast<std::uint64_t>(attack);
            return static_cast<std::int64_t>(4 * (square / denominator) +
                                             4 * (square % denominator) / denominator);
        }

        std::int64_t mitigate_absorb(std::int64_t damage, std::int32_t absorb) {
            if (damage <= 0) {
                return 0;
            }
            if (absorb <= 0) {
                return damage;
            }
            // Split products without discarding the remainder before the final division.
            if (absorb <= 40) {
                return damage - (damage / 100) * absorb - (damage % 100) * absorb / 100;
            }
            const std::int64_t divisor = static_cast<std::int64_t>(absorb) + 20;
            return 36 * (damage / divisor) + 36 * (damage % divisor) / divisor;
        }

        PerfectEvasionRoll perfect_evasion_roll(std::int32_t base, std::int32_t advanced) {
            base = std::max(0, base);
            advanced = std::max(0, advanced);
            if (base <= 40 && advanced <= 40) {
                return {};
            }
            const auto convert = [](std::int32_t percent) {
                return percent <= 40 ? percent
                                     : static_cast<std::int32_t>(
                                           100 - 3600 / (static_cast<std::int64_t>(percent) + 20));
            };
            const auto p1 = convert(base);
            const auto p2 = convert(advanced);
            return {false, std::min(75, p1 + p2 - p1 * p2 / 100)};
        }

        std::int32_t
        final_damage(const Combatant& attacker, const Combatant& target, std::int32_t raw_attack) {
            const std::int64_t diff = static_cast<std::int64_t>(attacker.level) - target.level;
            std::int64_t atk = raw_attack;
            // compatibility behavior applies g_nReviseDamage only when a monster is involved.
            if (!attacker.is_player || !target.is_player) {
                if (diff < 0) {
                    atk += -revise_damage(static_cast<int>(-diff));
                } else {
                    atk += revise_damage(static_cast<int>(diff));
                }
            }
            if (atk <= 0) {
                return 0;
            }
            std::int64_t damage = mitigate_defense(atk, final_defense(attacker, target));
            damage = mitigate_absorb(damage, final_absorb(attacker, target));
            if (target.armor_broken && damage > 100) {
                damage += damage / 20;
            }
            // compatibility behavior: +10% final damage (physical path).
            if ((attacker.bstate & protocol::kCbsPayUpAttack) != 0) {
                damage += damage / 10;
            }
            return clamp_nonneg_i32(std::max<std::int64_t>(1, damage));
        }

        AttackResult resolve_physical(const Combatant& attacker,
                                      const Combatant& target,
                                      int add_hit,
                                      int add_fatal,
                                      std::int32_t raw_attack,
                                      int revise_tick_ms,
                                      int delay_ms,
                                      bool skip_hit_check) {
            AttackResult r;
            const auto evades = [](std::int32_t percent) {
                return percent > 0 && irand(1, 100) <= std::clamp(percent, 0, 100);
            };
            const auto evasion =
                perfect_evasion_roll(target.perfect_evasion_base, target.perfect_evasion_advanced);
            const bool evaded =
                evasion.legacy_two_rolls
                    ? evades(target.perfect_evasion_base) || evades(target.perfect_evasion_advanced)
                    : evades(evasion.combined_percent);
            if (evaded) {
                return r;
            }
            if (target.is_player && check_block(attacker, target)) {
                r.blocked = true;
                return r;
            }
            if (!skip_hit_check && !check_hit(attacker, target, add_hit)) {
                return r;
            }
            std::int32_t raw = (raw_attack >= 0) ? raw_attack : roll_attack(attacker);
            if (delay_ms > 0) {
                const int revise = (revise_tick_ms >= 0) ? revise_tick_ms : delay_ms;
                raw = scale_by_skill_delay(raw, revise, delay_ms);
            }
            r.hit = true;
            const std::int32_t base_damage = final_damage(attacker, target, raw);
            bool fatal = false;
            const std::int32_t extra = get_fatal_extra(
                base_damage, attacker.fatal_rate, attacker.fatal_pt, add_fatal, fatal);
            r.fatal = fatal;
            r.damage = clamp_nonneg_i32(static_cast<std::int64_t>(base_damage) + extra);
            if (fatal && !target.is_player) {
                r.damage = clamp_nonneg_i32(
                    static_cast<std::int64_t>(r.damage) +
                    static_cast<std::int64_t>(r.damage * attacker.critical_damage_percent / 100.0));
            }
            // compatibility behavior: −10% damage taken from monsters.
            if (!attacker.is_player && target.is_player &&
                (target.bstate & protocol::kCbsPayCutDamage) != 0) {
                r.damage -= r.damage / 10;
            }
            r.palsy = get_palsy_extra(attacker, target);
            if (r.palsy > 0) {
                r.explosive = true;
                r.damage = clamp_nonneg_i32(static_cast<std::int64_t>(r.damage) + r.palsy);
            }
            return r;
        }

        std::int32_t get_fatal_extra(std::int32_t damage,
                                     std::int32_t fatal_rate,
                                     std::int32_t fatal_pt,
                                     int add_fatal,
                                     bool& fatal) {
            fatal = false;
            if (damage <= 0) {
                return 0;
            }
            const int chance = add_fatal + fatal_rate;
            if (chance < irand(1, 100)) {
                return 0;
            }
            fatal = true;
            return clamp_nonneg_i32(static_cast<std::int64_t>(damage) *
                                    (2 * static_cast<std::int64_t>(fatal_pt) + 50) / 100);
        }

        int get_palsy_chance(int upgrade_level,
                             int attacker_level,
                             int target_level,
                             bool target_is_player) {
            if (upgrade_level <= 0) {
                return 0;
            }
            static constexpr int kUpgrDamageRate[21] = {0,  10, 11, 11, 12, 12, 13, 13, 14, 14, 15,
                                                        15, 16, 16, 17, 17, 18, 18, 19, 19, 20};
            static constexpr int kPlayerA[8] = {100, 115, 130, 145, 160, 175, 190, 200};
            static constexpr int kPlayerB[8] = {100, 85, 70, 55, 40, 25, 10, 0};
            static constexpr int kMonsterA[8] = {100, 115, 130, 145, 160, 175, 190, 200};
            static constexpr int kMonsterB[31] = {100, 100, 100, 100, 100, 100, 85, 85, 85, 85, 85,
                                                  85,  85,  85,  85,  85,  70,  70, 70, 70, 70, 55,
                                                  55,  55,  40,  40,  40,  25,  25, 25, 10};
            const int level = std::clamp(upgrade_level, 0, 20);
            const int gap = attacker_level - target_level;
            int revise = 100;
            if (target_is_player) {
                if (gap < 0) {
                    const int idx = std::min(std::abs(gap), 7);
                    revise = kPlayerB[idx];
                } else {
                    revise = kPlayerA[std::min(gap, 7)];
                }
            } else if (gap < 0) {
                const int idx = std::min(std::abs(gap), 30);
                revise = kMonsterB[idx];
            } else {
                revise = kMonsterA[std::min(gap, 7)];
            }
            int chance = revise * kUpgrDamageRate[level] / 100;
            // compatibility behavior / UPXBLOW5 — only vs monsters (GetPalsyDamage).
            if (!target_is_player) {
                // Caller must pass attacker bstate via a separate path; chance is adjusted
                // in get_palsy_extra using attacker.bstate.
            }
            return chance;
        }

        std::int32_t get_palsy_extra(const Combatant& attacker,
                                     const Combatant& target,
                                     int chance_roll,
                                     int damage_roll) {
            if (attacker.upgrade_level <= 0) {
                return 0;
            }
            int chance = get_palsy_chance(
                attacker.upgrade_level, attacker.level, target.level, target.is_player);
            if (!target.is_player) {
                if ((attacker.bstate & protocol::kCbsPayUpXBlow5) != 0) {
                    chance += 5;
                } else if ((attacker.bstate & protocol::kCbsPayUpXBlow3) != 0) {
                    chance += 3;
                }
            }
            const int roll = chance_roll > 0 ? chance_roll : irand(1, 100);
            if (chance < roll) {
                return 0;
            }
            int resist = target.res_palsy;
            if (!target.is_player) {
                resist /= 10;
            }
            if (resist < 0) {
                resist = 0;
            }
            if (resist >= 100) {
                resist = 0;
            } else {
                resist = 100 - resist;
            }
            const int eb = std::max(0, attacker.upgrade_level);
            const bool mage = attacker.clazz == kPcMage;
            const int wmin = mage ? attacker.weapon_min_magic : attacker.weapon_min_attack;
            const int wmax = mage ? attacker.weapon_max_magic : attacker.weapon_max_attack;
            const int cstat = mage ? attacker.char_magic : attacker.char_attack;
            const int span_lo = wmin + eb * wmin / 20;
            const int span_hi = wmax + eb * wmax / 10;
            int span = 0;
            if (damage_roll >= 0) {
                span =
                    std::clamp(damage_roll, std::min(span_lo, span_hi), std::max(span_lo, span_hi));
            } else {
                span = irand(span_lo, span_hi);
            }
            const auto damage =
                clamp_nonneg_i32(static_cast<std::int64_t>(resist) *
                                 (3 * static_cast<std::int64_t>(cstat) + span) / 100);
            const double percent = attacker.talent_explosive_damage_percent +
                                   (target.is_player ? 0.0 : attacker.explosive_damage_percent);
            return clamp_nonneg_i32(static_cast<std::int64_t>(damage) +
                                    static_cast<std::int64_t>(damage * percent / 100.0));
        }

        bool can_conti_skill(int owner_level, int target_level, int chance_roll) {
            int gab = owner_level - target_level;
            if (gab <= 0) {
                if (gab < -9) {
                    gab = -9;
                }
                gab = std::abs(gab);
            } else {
                if (gab > 3) {
                    gab = 3;
                }
                gab += 10;
            }
            static constexpr int kRate[] = {89, 83, 74, 65, 56, 48, 32, 19, 5, 3, 0, 91, 93, 95};
            const int idx = std::clamp(gab, 0, 13);
            const int roll = chance_roll > 0 ? chance_roll : irand(1, 100);
            return kRate[idx] >= roll;
        }

        void apply_ice_mastery_slow(AttackResult& result,
                                    int owner_level,
                                    int target_level,
                                    int ice_mastery,
                                    int chance_roll) {
            result.ice_slow_duration_sec = 0;
            result.ice_slow_percent = 0;
            if (ice_mastery <= 0) {
                return;
            }
            if (!can_conti_skill(owner_level, target_level, chance_roll)) {
                return;
            }
            result.ice_slow_duration_sec = ice_mastery + 2;
            result.ice_slow_percent = -20 * ice_mastery;
        }

        int fire_mastery_delay_ms(int delay_ms, int delay_pre_ms, int fire_mastery) {
            if (fire_mastery > 0) {
                delay_ms -= 5 * fire_mastery * std::max(0, delay_pre_ms) / 100;
            }
            return std::max(1, delay_ms);
        }

        std::int32_t skill_physical_power(int clazz,
                                          int skill_index,
                                          int skill_level,
                                          std::int32_t dex,
                                          std::int32_t base_attack,
                                          int random_roll,
                                          int prerequisite_skill_level) {
            const std::int64_t slv = std::max(1, skill_level);
            const std::int64_t base = base_attack;
            if (clazz == kPcKnight) {
                switch (skill_index) {
                case 3:  // compatibility behavior
                    return clamp_nonneg_i32(base + 8 * slv);
                case 5:  // compatibility behavior
                    return clamp_nonneg_i32(base + 50 * slv + 150);
                case 7: {  // compatibility behavior
                    const int roll = random_roll >= 0
                                         ? std::clamp(random_roll, 0, static_cast<int>(slv))
                                         : irand(0, static_cast<int>(slv));
                    return clamp_nonneg_i32(base + 20 * slv + 60 + roll);
                }
                case 9:  // compatibility behavior
                    return clamp_nonneg_i32(base + 10 * slv + 50);
                case 13: {  // compatibility behavior
                    const int roll = random_roll >= 0
                                         ? std::clamp(random_roll, 0, static_cast<int>(slv))
                                         : irand(0, static_cast<int>(slv));
                    return clamp_nonneg_i32(base + 24 * slv + 80 + roll);
                }
                case 16:  // compatibility behavior
                    return clamp_nonneg_i32(base + base * (10 * slv + 100) / 80);
                case 41: {  // PowerfulWideningWound; ReadConfig default coefficients.
                    const int roll =
                        random_roll >= 0 ? std::clamp(random_roll, 5, 10) : irand(5, 10);
                    return clamp_nonneg_i32(8 * base + 320 + 196 * slv + slv * roll);
                }
                case 22: {  // compatibility behavior
                    const std::int64_t extra = base * (10 * slv + 60) / 100;
                    return clamp_nonneg_i32(((extra >= 200) ? 200 : extra) + base);
                }
                case 25:  // compatibility behavior Excute nAttack + lv * 10 * nAttack / 100
                    return clamp_nonneg_i32(base + slv * 10 * base / 100);
                case 42: {  // Provocation of Blow — compatibility notes GetAttack * 7 + rank *
                            // Rate(5, 10)
                    const int roll =
                        random_roll >= 0 ? std::clamp(random_roll, 5, 10) : irand(5, 10);
                    return clamp_nonneg_i32(static_cast<std::int64_t>(base) * 7 +
                                            static_cast<std::int64_t>(slv) * roll);
                }
                case 38: {  // Spin Slash — compatibility notes GetAttack * 7 + rank * Rate(5, 20)
                    const int roll =
                        random_roll >= 0 ? std::clamp(random_roll, 5, 20) : irand(5, 20);
                    return clamp_nonneg_i32(static_cast<std::int64_t>(base) * 7 +
                                            static_cast<std::int64_t>(slv) * roll);
                }
                case 91:  // TheBoomOfEarth is pure control.
                    return 0;
                case 93:  // TheWaveOfEarth: NWOEMul/Min/Max are all 1 in ReadConfig.
                    return clamp_nonneg_i32(base + slv);
                default:
                    return clamp_nonneg_i32(base);
                }
            }
            if (clazz == kPcThief) {
                const bool wide_roll = skill_index == 7 || skill_index == 16;
                const int roll =
                    random_roll >= 0 ? random_roll : irand(wide_roll ? 15 : 5, wide_roll ? 30 : 10);
                return protocol::thief_skill_power(skill_index, skill_level, base_attack, roll);
            }
            if (clazz != kPcArcher) {
                return clamp_nonneg_i32(base);
            }
            switch (skill_index) {
            case 3:  // compatibility behavior
                return archer_shot_power(skill_level, dex, base_attack);
            case 4:  // compatibility behavior
                return clamp_nonneg_i32(base);
            case 6:  // compatibility behavior
                return archer_flame_arrow_power(skill_level, base_attack);
            case 9:  // compatibility behavior
                return archer_stun_arrow_power(
                    skill_level,
                    prerequisite_skill_level >= 0 ? prerequisite_skill_level : skill_level,
                    dex,
                    base_attack);
            case 14:  // compatibility behavior
                return archer_explosion_power(
                    skill_level,
                    prerequisite_skill_level >= 0 ? prerequisite_skill_level : skill_level,
                    base_attack);
            case 16: {  // compatibility behavior uncharged (case 1)
                return archer_focus_shot_power(skill_level, base_attack, 1);
            }
            case 21:  // compatibility behavior
                return archer_mystic_arrow_power(skill_level, base_attack);
            default:
                return clamp_nonneg_i32(base);
            }
        }

        std::int32_t archer_status_tick(int skill_index,
                                        int skill_level,
                                        int owner_level,
                                        std::int32_t base_attack,
                                        int final_resist) {
            const std::int64_t rank = std::max(1, skill_level);
            std::int64_t damage =
                skill_index == 13 ? owner_level + 10 * rank : (150 * rank + 100) * base_attack / 20;
            damage -= damage * std::clamp(final_resist, 0, 100) / 100;
            return clamp_nonneg_i32(skill_index == 19 ? std::min<std::int64_t>(4500, damage) / 30
                                                      : damage);
        }

        std::int32_t archer_life_absorption_damage(int skill_level,
                                                   std::int32_t base_attack,
                                                   std::int32_t target_hp) {
            return clamp_nonneg_i32(
                std::min<std::int64_t>(std::max(0, target_hp - 1),
                                       (40LL * std::max(1, skill_level) + 50) * base_attack / 50));
        }

        std::int32_t archer_life_absorption_heal(int skill_level, std::int32_t damage) {
            return clamp_nonneg_i32(static_cast<std::int64_t>(damage) *
                                    (10LL * std::max(1, skill_level) + 20) / 50);
        }

        std::int32_t knight_spin_slash_power(int skill_level,
                                             std::int32_t base_attack,
                                             int mortal_points,
                                             int attack_roll,
                                             int mortal_roll) {
            const int roll = attack_roll >= 0 ? std::clamp(attack_roll, 5, 20) : irand(5, 20);
            const int deathblow =
                mortal_roll >= 0 ? std::clamp(mortal_roll, 3000, 6000) : irand(3000, 6000);
            return clamp_nonneg_i32(
                7LL * base_attack + static_cast<std::int64_t>(std::max(1, skill_level)) * roll +
                static_cast<std::int64_t>(std::clamp(mortal_points, 0, 5)) * deathblow);
        }

        std::int32_t knight_infernal_power(int skill_level,
                                           std::int32_t base_attack,
                                           std::int32_t mortal_points) {
            const std::int64_t slv = std::max(1, skill_level);
            const std::int64_t base = base_attack;
            const std::int64_t mortal = std::max<std::int64_t>(0, mortal_points);
            return clamp_nonneg_i32(slv * (5 * mortal + 25) + base + 5 * mortal + 45 +
                                    base * (20 * mortal + 2 * slv + 8) / 50);
        }

        int skill_physical_hit(int clazz, int skill_index, int skill_level) {
            const int slv = std::max(1, skill_level);
            if (clazz == kPcKnight) {
                switch (skill_index) {
                case 3:  // compatibility behavior
                    return slv / 2;
                case 5:  // compatibility behavior
                    return 2 * slv;
                case 7:  // compatibility behavior
                    return slv + slv / 2;
                case 9:  // compatibility behavior
                    return slv + 9;
                case 13:  // compatibility behavior
                    return 2 * slv;
                case 16:  // compatibility behavior
                    return 10;
                case 41:  // Character-level bonus is added by the cast handler.
                    return 10 + 3 * slv;
                case 17:  // compatibility behavior
                    return 10;
                case 25:  // compatibility behavior
                    return 5;
                default:
                    return 0;
                }
            }
            if (clazz == kPcThief) {
                return protocol::thief_skill_hit(skill_index);
            }
            if (clazz != kPcArcher) {
                return 0;
            }
            switch (skill_index) {
            case 3:   // compatibility behavior
            case 6:   // compatibility behavior
            case 9:   // compatibility behavior
            case 14:  // compatibility behavior
                return 2 * slv;
            default:
                return 0;
            }
        }

        int skill_physical_delay_ms(
            int clazz, int skill_index, int skill_level, int def_delay_ms, int attack_speed_ms) {
            const int delay = std::max(0, def_delay_ms);
            const int aspeed = std::max(1, attack_speed_ms);
            const int slv = std::max(1, skill_level);
            if (clazz == kPcKnight) {
                switch (skill_index) {
                case 3:   // compatibility behavior
                case 7:   // compatibility behavior
                case 13:  // compatibility behavior
                case 16:  // compatibility behavior
                case 41:  // Powerful Widening Wound (same GetDelay family as compatibility
                          // behavior)
                    return std::max(1, delay + aspeed);
                case 5:  // compatibility behavior
                case 9:  // compatibility behavior
                    return std::max(1, delay + aspeed - 100 * ((slv + 1) / 2));
                default:
                    break;
                }
            }
            if (clazz == kPcArcher && (skill_index == 3 || skill_index == 9)) {
                return std::max(1, delay - 100 * slv);
            }
            return std::max(1, delay);
        }

        int revised_attack_delay_ms(int delay_ms, std::int32_t speed_percent) {
            const int delay = std::max(0, delay_ms);
            if (speed_percent == 0) {
                return std::max(1, delay);
            }
            if (speed_percent <= 0) {
                return std::max(1, delay * (std::abs(speed_percent) + 100) / 100);
            }
            return std::max(1, 100 * delay / (speed_percent + 100));
        }

        int skill_physical_fatal(int clazz, int skill_index, int skill_level) {
            const int slv = std::max(1, skill_level);
            if (clazz == kPcKnight) {
                return skill_index == 5 ? 5 : 0;
            }
            if (clazz == kPcThief) {
                return 0;
            }
            if (clazz != kPcArcher) {
                return 0;
            }
            switch (skill_index) {
            case 3:  // compatibility behavior
                return 10;
            case 16:  // compatibility behavior
                return slv + 5;
            default:
                return 0;
            }
        }

        KnightWeaponMasteryContribution knight_weapon_mastery_contribution(int skill_level) {
            const std::int32_t slv = std::max(0, skill_level);
            const std::int32_t attack = slv / 2 + 1;
            return {.hit = slv / 2, .min_attack = attack, .max_attack = attack};
        }

        KnightCriticalContribution knight_critical_contribution(int skill_level) {
            const std::int32_t slv = std::max(0, skill_level);
            return {.fatal_pt = slv, .fatal_rate = std::min<std::int32_t>(slv, 10)};
        }

        std::int32_t knight_advanced_critical_fatal_pt() {
            return 20;
        }

        std::int32_t knight_life_extension_hp(std::int32_t revised_hth_hp, int skill_level) {
            const std::int64_t hth_hp = std::max<std::int64_t>(0, revised_hth_hp);
            const std::int64_t slv = std::max(0, skill_level);
            return clamp_nonneg_i32(hth_hp * (10 * slv + 14) / 100);
        }

        ArcherBowMasteryContribution archer_bow_mastery_contribution(int skill_level) {
            const std::int32_t slv = std::max(0, skill_level);
            ArcherBowMasteryContribution contribution;
            contribution.min_attack = slv / 2;
            contribution.max_attack = slv;
            return contribution;
        }

        ArcherEvasionContribution archer_evasion_contribution(int base_skill_level,
                                                              int advanced_skill_level) {
            ArcherEvasionContribution contribution;
            const std::int32_t base = std::max(0, base_skill_level);
            contribution.base_percent = base > 0 ? base + 7 : 0;
            contribution.advanced_percent = std::max(0, advanced_skill_level);
            return contribution;
        }

        ArcherFatalContribution archer_fatal_contribution(int fatal_skill_level,
                                                          bool advanced_active) {
            ArcherFatalContribution contribution;
            const std::int32_t slv = std::max(0, fatal_skill_level);
            contribution.fatal_pt = slv;
            contribution.fatal_rate = std::min<std::int32_t>(slv, 10);
            contribution.advanced_fatal_pt = advanced_active ? 80 : 0;
            return contribution;
        }

        std::int32_t archer_concentration_hit(int skill_level) {
            return clamp_nonneg_i32(2 * static_cast<std::int64_t>(std::max(0, skill_level)));
        }

        std::int32_t archer_attack_range_bonus(int skill_level) {
            return clamp_nonneg_i32(20 * static_cast<std::int64_t>(std::max(0, skill_level)));
        }

        std::int32_t archer_life_extension_hp(std::int32_t base_hth, int skill_level) {
            const std::int64_t hth = std::max<std::int64_t>(0, base_hth);
            const std::int64_t slv = std::max(0, skill_level);
            const std::int64_t revised_hth_hp = 2 * hth * hth / kDenoHp[kPcArcher];
            return clamp_nonneg_i32(revised_hth_hp * (10 * slv + 14) / 100);
        }

        std::int32_t
        archer_shot_power(int skill_level, std::int32_t dex, std::int32_t base_attack) {
            const std::int64_t slv = std::max(1, skill_level);
            const std::int64_t d = std::max<std::int64_t>(0, dex);
            const std::int64_t base = std::max<std::int64_t>(0, base_attack);
            const std::int64_t extra = 7 * slv * d / 4 / 2 + 50;
            const std::int64_t cap = 70 * slv + 200;
            return clamp_nonneg_i32(std::min(extra, cap) + base);
        }

        std::int32_t archer_flame_arrow_power(int skill_level, std::int32_t base_attack) {
            const std::int64_t slv = std::max(1, skill_level);
            const std::int64_t base = std::max<std::int64_t>(0, base_attack);
            return clamp_nonneg_i32(base + 12 * slv);
        }

        std::int32_t archer_stun_arrow_power(int skill_level,
                                             int shot_skill_level,
                                             std::int32_t dex,
                                             std::int32_t base_attack) {
            if (shot_skill_level <= 0) {
                return 0;
            }
            const std::int64_t slv = std::max(1, skill_level);
            const std::int64_t base = std::max<std::int64_t>(0, base_attack);
            const std::int64_t shot = archer_shot_power(shot_skill_level, dex, 0);
            return clamp_nonneg_i32(12 * slv + base + shot);
        }

        std::int32_t
        archer_explosion_power(int skill_level, int flame_skill_level, std::int32_t base_attack) {
            if (flame_skill_level <= 0) {
                return 0;
            }
            const std::int64_t slv = std::max(1, skill_level);
            const std::int64_t base = std::max<std::int64_t>(0, base_attack);
            const std::int64_t flame = archer_flame_arrow_power(flame_skill_level, 0);
            return clamp_nonneg_i32(19 * slv + base + flame);
        }

        std::int32_t archer_mystic_arrow_power(int skill_level, std::int32_t base_attack) {
            const std::int64_t slv = std::max(1, skill_level);
            const std::int64_t base = std::max<std::int64_t>(0, base_attack);
            return clamp_nonneg_i32((slv + 2) * (50 * base / 100));
        }

        std::int32_t
        archer_arrow_rain_power(int skill_level, std::int32_t base_attack, int random_roll) {
            using protocol::kArcherArrowRainAttackMul;
            using protocol::kArcherArrowRainGradeMax;
            using protocol::kArcherArrowRainGradeMin;
            const std::int64_t slv = std::max(1, skill_level);
            const std::int64_t base = std::max<std::int64_t>(0, base_attack);
            const int grade_roll =
                random_roll >= 0
                    ? std::clamp(random_roll, kArcherArrowRainGradeMin, kArcherArrowRainGradeMax)
                    : irand(kArcherArrowRainGradeMin, kArcherArrowRainGradeMax);
            return clamp_nonneg_i32(base * kArcherArrowRainAttackMul + slv * grade_roll);
        }

        std::int32_t archer_arrow_rain_pvp_damage(std::int32_t damage) {
            return clamp_nonneg_i32(static_cast<std::int64_t>(std::max(0, damage)) *
                                    protocol::kArcherArrowRainPvpPercent / 100);
        }

        std::int32_t archer_poison_cloud_power(int skill_level, std::int32_t base_attack) {
            const std::int64_t slv = std::max(1, skill_level);
            const std::int64_t attack = std::max<std::int64_t>(0, base_attack);
            return clamp_nonneg_i32((200 * slv + 50) * attack / 100 / 10);
        }

        std::int32_t
        archer_arrow_explosion_power(int skill_level, std::int32_t base_attack, int random_roll) {
            using protocol::kArcherArrowExplosionAttackMul;
            using protocol::kArcherArrowExplosionGradeMax;
            using protocol::kArcherArrowExplosionGradeMin;
            const std::int64_t slv = std::max(1, skill_level);
            const std::int64_t base = std::max<std::int64_t>(0, base_attack);
            const int grade_roll =
                random_roll >= 0
                    ? std::clamp(
                          random_roll, kArcherArrowExplosionGradeMin, kArcherArrowExplosionGradeMax)
                    : irand(kArcherArrowExplosionGradeMin, kArcherArrowExplosionGradeMax);
            return clamp_nonneg_i32(base * kArcherArrowExplosionAttackMul + slv * grade_roll);
        }

        std::int32_t archer_arrow_explosion_pvp_damage(std::int32_t damage) {
            return clamp_nonneg_i32(static_cast<std::int64_t>(std::max(0, damage)) *
                                    protocol::kArcherArrowExplosionPvpPercent / 100);
        }

        int archer_focus_shot_tier(int focus_ms) {
            if (focus_ms < 1000) {
                return 0;
            }
            if (focus_ms >= 2900) {
                return 5;
            }
            if (focus_ms >= 2600) {
                return 4;
            }
            if (focus_ms >= 2300) {
                return 3;
            }
            if (focus_ms >= 1700) {
                return 2;
            }
            return 1;
        }

        std::int32_t
        archer_focus_shot_power(int skill_level, std::int32_t base_attack, int charge_tier) {
            if (charge_tier < 1 || charge_tier > 5) {
                return 0;
            }
            static constexpr int kSkillCoefficients[5] = {25, 32, 40, 47, 55};
            const std::int64_t slv = std::max(1, skill_level);
            const std::int64_t base = std::max<std::int64_t>(0, base_attack);
            const std::int64_t coefficient =
                50 * charge_tier + kSkillCoefficients[charge_tier - 1] * slv + 50;
            const std::int64_t extra = std::min<std::int64_t>(2000, base * coefficient / 90);
            return clamp_nonneg_i32(base + extra);
        }

        int archer_focus_shot_hit(int skill_level, int charge_tier) {
            if (charge_tier < 1 || charge_tier > 5) {
                return 0;
            }
            const int slv = std::max(1, skill_level);
            return (slv + 1) / 2 + 2 * charge_tier - 1;
        }

        int archer_skill_hostility(int skill_index) {
            switch (skill_index) {
            case 3:  // compatibility behavior
                return 100;
            case 9:  // compatibility behavior
                return 50;
            case 14:  // compatibility behavior
                return 75;
            default:
                return 0;
            }
        }

        std::int32_t archer_mystic_spirit_tick(std::int32_t base_attack) {
            const std::int64_t base = std::max<std::int64_t>(0, base_attack);
            return clamp_nonneg_i32((50 * base / 100) / 5);
        }

        std::int32_t archer_chance_curse_delta(std::int32_t target_fatal_rate, int skill_level) {
            const std::int64_t fatal = std::max<std::int64_t>(0, target_fatal_rate);
            const std::int64_t slv = std::max(1, skill_level);
            return static_cast<std::int32_t>(fatal * (25 * slv + 25) / -100);
        }

        bool is_knight_taunt(int clazz, int skill_index) {
            return clazz == kPcKnight && (skill_index == 4 || skill_index == 8);
        }

        int skill_taunt_range(int skill_index, int skill_level) {
            const int lv = std::max(1, skill_level);
            if (skill_index == 4) {
                return 115 + 7 * lv;
            }
            if (skill_index == 8) {
                return 136 + 8 * lv;
            }
            return 0;
        }

        int skill_taunt_hostility(int skill_index, int skill_level) {
            const int lv = std::max(1, skill_level);
            if (skill_index == 4) {
                // compatibility behavior
                return 150 * lv + 50;
            }
            if (skill_index == 8) {
                // compatibility behavior
                return 200 * lv + 800;
            }
            return 0;
        }

        bool in_attack_range(int ax, int ay, int tx, int ty, int range) {
            const int dx = tx - ax;
            const int dy = ty - ay;
            const int r = std::max(1, range);
            return static_cast<long long>(dx) * dx + static_cast<long long>(dy) * dy <=
                   static_cast<long long>(r) * r;
        }

    }  // namespace combat

    combat::BaseStats Combat::class_base_plus_bonus(int clazz) {
        return combat::class_base_plus_bonus(clazz);
    }

    void Combat::apply_stat_revise(std::int32_t level,
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
                                   std::int32_t& res_palsy) {
        combat::apply_stat_revise(level,
                                  stats,
                                  min_attack,
                                  max_attack,
                                  hit,
                                  dodge,
                                  min_magic,
                                  max_magic,
                                  res_fire,
                                  res_ice,
                                  res_lightning,
                                  res_curse,
                                  res_palsy);
    }

    std::int32_t Combat::get_need_point(int clazz, int property, std::int32_t current_value) {
        return combat::get_need_point(clazz, property, current_value);
    }

    std::int32_t
    Combat::pending_need_points(int clazz, int property, std::int32_t current_value, int pending) {
        return combat::pending_need_points(clazz, property, current_value, pending);
    }

    int Combat::irand(int lo, int hi) {
        return combat::irand(lo, hi);
    }

    int Combat::revise_damage(int abs_level_diff) {
        return combat::revise_damage(abs_level_diff);
    }

    int Combat::revise_hit(int abs_level_diff) {
        return combat::revise_hit(abs_level_diff);
    }

    int Combat::hit_rate_pct(int clamped_rate) {
        return combat::hit_rate_pct(clamped_rate);
    }

    std::int32_t Combat::roll_attack(const combat::Combatant& attacker) {
        return combat::roll_attack(attacker);
    }

    bool Combat::check_hit(const combat::Combatant& attacker,
                           const combat::Combatant& target,
                           int add_hit) {
        return combat::check_hit(attacker, target, add_hit);
    }

    bool Combat::check_block(const combat::Combatant& attacker,
                             const combat::Combatant& target,
                             int chance_roll) {
        return combat::check_block(attacker, target, chance_roll);
    }

    int Combat::color_level(int attacker_level, int defender_level) {
        return combat::color_level(attacker_level, defender_level);
    }

    int Combat::knight_weapon_defend_block(int char_level) {
        return combat::knight_weapon_defend_block(char_level);
    }

    std::int32_t Combat::final_defense(const combat::Combatant& attacker,
                                       const combat::Combatant& target) {
        return combat::final_defense(attacker, target);
    }

    std::int32_t Combat::final_absorb(const combat::Combatant& attacker,
                                      const combat::Combatant& target) {
        return combat::final_absorb(attacker, target);
    }

    std::int32_t Combat::final_damage(const combat::Combatant& attacker,
                                      const combat::Combatant& target,
                                      std::int32_t raw_attack) {
        return combat::final_damage(attacker, target, raw_attack);
    }

    combat::AttackResult Combat::resolve_physical(const combat::Combatant& attacker,
                                                  const combat::Combatant& target,
                                                  int add_hit,
                                                  int add_fatal,
                                                  std::int32_t raw_attack,
                                                  int revise_tick_ms,
                                                  int delay_ms,
                                                  bool skip_hit_check) {
        return combat::resolve_physical(attacker,
                                        target,
                                        add_hit,
                                        add_fatal,
                                        raw_attack,
                                        revise_tick_ms,
                                        delay_ms,
                                        skip_hit_check);
    }

    std::int32_t Combat::get_fatal_extra(std::int32_t damage,
                                         std::int32_t fatal_rate,
                                         std::int32_t fatal_pt,
                                         int add_fatal,
                                         bool& fatal) {
        return combat::get_fatal_extra(damage, fatal_rate, fatal_pt, add_fatal, fatal);
    }

    std::int32_t Combat::skill_physical_power(int clazz,
                                              int skill_index,
                                              int skill_level,
                                              std::int32_t dex,
                                              std::int32_t base_attack,
                                              int random_roll,
                                              int prerequisite_skill_level) {
        return combat::skill_physical_power(clazz,
                                            skill_index,
                                            skill_level,
                                            dex,
                                            base_attack,
                                            random_roll,
                                            prerequisite_skill_level);
    }

    std::int32_t Combat::knight_infernal_power(int skill_level,
                                               std::int32_t base_attack,
                                               std::int32_t mortal_points) {
        return combat::knight_infernal_power(skill_level, base_attack, mortal_points);
    }

    int Combat::skill_physical_hit(int clazz, int skill_index, int skill_level) {
        return combat::skill_physical_hit(clazz, skill_index, skill_level);
    }

    int Combat::skill_physical_delay_ms(
        int clazz, int skill_index, int skill_level, int def_delay_ms, int attack_speed_ms) {
        return combat::skill_physical_delay_ms(
            clazz, skill_index, skill_level, def_delay_ms, attack_speed_ms);
    }

    int Combat::skill_physical_fatal(int clazz, int skill_index, int skill_level) {
        return combat::skill_physical_fatal(clazz, skill_index, skill_level);
    }

    combat::KnightWeaponMasteryContribution
    Combat::knight_weapon_mastery_contribution(int skill_level) {
        return combat::knight_weapon_mastery_contribution(skill_level);
    }

    combat::KnightCriticalContribution Combat::knight_critical_contribution(int skill_level) {
        return combat::knight_critical_contribution(skill_level);
    }

    std::int32_t Combat::knight_advanced_critical_fatal_pt() {
        return combat::knight_advanced_critical_fatal_pt();
    }

    std::int32_t Combat::knight_life_extension_hp(std::int32_t revised_hth_hp, int skill_level) {
        return combat::knight_life_extension_hp(revised_hth_hp, skill_level);
    }

    combat::ArcherBowMasteryContribution Combat::archer_bow_mastery_contribution(int skill_level) {
        return combat::archer_bow_mastery_contribution(skill_level);
    }

    combat::ArcherEvasionContribution
    Combat::archer_evasion_contribution(int base_skill_level, int advanced_skill_level) {
        return combat::archer_evasion_contribution(base_skill_level, advanced_skill_level);
    }

    combat::ArcherFatalContribution Combat::archer_fatal_contribution(int fatal_skill_level,
                                                                      bool advanced_active) {
        return combat::archer_fatal_contribution(fatal_skill_level, advanced_active);
    }

    std::int32_t Combat::archer_concentration_hit(int skill_level) {
        return combat::archer_concentration_hit(skill_level);
    }

    std::int32_t Combat::archer_attack_range_bonus(int skill_level) {
        return combat::archer_attack_range_bonus(skill_level);
    }

    std::int32_t Combat::archer_life_extension_hp(std::int32_t base_hth, int skill_level) {
        return combat::archer_life_extension_hp(base_hth, skill_level);
    }

    std::int32_t
    Combat::archer_shot_power(int skill_level, std::int32_t dex, std::int32_t base_attack) {
        return combat::archer_shot_power(skill_level, dex, base_attack);
    }

    std::int32_t Combat::archer_flame_arrow_power(int skill_level, std::int32_t base_attack) {
        return combat::archer_flame_arrow_power(skill_level, base_attack);
    }

    std::int32_t Combat::archer_stun_arrow_power(int skill_level,
                                                 int shot_skill_level,
                                                 std::int32_t dex,
                                                 std::int32_t base_attack) {
        return combat::archer_stun_arrow_power(skill_level, shot_skill_level, dex, base_attack);
    }

    std::int32_t Combat::archer_explosion_power(int skill_level,
                                                int flame_skill_level,
                                                std::int32_t base_attack) {
        return combat::archer_explosion_power(skill_level, flame_skill_level, base_attack);
    }

    std::int32_t Combat::archer_mystic_arrow_power(int skill_level, std::int32_t base_attack) {
        return combat::archer_mystic_arrow_power(skill_level, base_attack);
    }

    std::int32_t
    Combat::archer_arrow_rain_power(int skill_level, std::int32_t base_attack, int random_roll) {
        return combat::archer_arrow_rain_power(skill_level, base_attack, random_roll);
    }

    std::int32_t Combat::archer_arrow_rain_pvp_damage(std::int32_t damage) {
        return combat::archer_arrow_rain_pvp_damage(damage);
    }

    std::int32_t Combat::archer_poison_cloud_power(int skill_level, std::int32_t base_attack) {
        return combat::archer_poison_cloud_power(skill_level, base_attack);
    }

    std::int32_t Combat::archer_arrow_explosion_power(int skill_level,
                                                      std::int32_t base_attack,
                                                      int random_roll) {
        return combat::archer_arrow_explosion_power(skill_level, base_attack, random_roll);
    }

    std::int32_t Combat::archer_arrow_explosion_pvp_damage(std::int32_t damage) {
        return combat::archer_arrow_explosion_pvp_damage(damage);
    }

    int Combat::archer_focus_shot_tier(int focus_ms) {
        return combat::archer_focus_shot_tier(focus_ms);
    }

    std::int32_t
    Combat::archer_focus_shot_power(int skill_level, std::int32_t base_attack, int charge_tier) {
        return combat::archer_focus_shot_power(skill_level, base_attack, charge_tier);
    }

    int Combat::archer_focus_shot_hit(int skill_level, int charge_tier) {
        return combat::archer_focus_shot_hit(skill_level, charge_tier);
    }

    int Combat::archer_skill_hostility(int skill_index) {
        return combat::archer_skill_hostility(skill_index);
    }

    std::int32_t Combat::archer_mystic_spirit_tick(std::int32_t base_attack) {
        return combat::archer_mystic_spirit_tick(base_attack);
    }

    std::int32_t Combat::archer_chance_curse_delta(std::int32_t target_fatal_rate,
                                                   int skill_level) {
        return combat::archer_chance_curse_delta(target_fatal_rate, skill_level);
    }

    bool Combat::is_mage_magic_attack(int skill_index) {
        return combat::is_mage_magic_attack(skill_index);
    }

    combat::MagicResist Combat::mage_skill_resist(int skill_index) {
        return combat::mage_skill_resist(skill_index);
    }

    std::int32_t Combat::roll_magic(const combat::Combatant& attacker) {
        return combat::roll_magic(attacker);
    }

    std::int32_t Combat::skill_magic_power(int skill_index,
                                           int skill_level,
                                           std::int32_t char_level,
                                           std::int32_t intelligence,
                                           std::int32_t base_magic,
                                           std::int32_t lightning_mastery,
                                           int lightning_skill_level,
                                           int random_roll) {
        return combat::skill_magic_power(skill_index,
                                         skill_level,
                                         char_level,
                                         intelligence,
                                         base_magic,
                                         lightning_mastery,
                                         lightning_skill_level,
                                         random_roll);
    }

    std::int32_t
    Combat::mage_self_heal_power(int skill_level, std::int32_t char_level, std::int32_t wisdom) {
        return combat::mage_self_heal_power(skill_level, char_level, wisdom);
    }

    std::int32_t Combat::mage_heal_power(int skill_index,
                                         int skill_level,
                                         std::int32_t char_level,
                                         std::int32_t wisdom,
                                         std::int32_t target_max_hp,
                                         int random_roll) {
        return combat::mage_heal_power(
            skill_index, skill_level, char_level, wisdom, target_max_hp, random_roll);
    }

    std::int32_t Combat::monster_final_resist(std::int32_t raw_resist) {
        return combat::monster_final_resist(raw_resist);
    }

    std::int32_t Combat::magic_through_percent(std::int32_t attacker_level,
                                               std::int32_t target_level,
                                               std::int32_t final_resist) {
        return combat::magic_through_percent(attacker_level, target_level, final_resist);
    }

    std::int32_t Combat::final_magic_damage(std::int32_t attacker_level,
                                            std::int32_t target_level,
                                            std::int32_t raw_attack) {
        return combat::final_magic_damage(attacker_level, target_level, raw_attack);
    }

    int Combat::revise_skill_tick(int delay_ms, std::uint64_t now_ms, std::uint64_t& next_full_ms) {
        return combat::revise_skill_tick(delay_ms, now_ms, next_full_ms);
    }

    std::int32_t
    Combat::scale_by_skill_delay(std::int32_t value, int revise_tick_ms, int delay_ms) {
        return combat::scale_by_skill_delay(value, revise_tick_ms, delay_ms);
    }

    combat::AttackResult Combat::resolve_magic(const combat::Combatant& attacker,
                                               const combat::Combatant& target,
                                               int skill_index,
                                               int skill_level,
                                               std::int32_t intelligence,
                                               std::int32_t lightning_mastery,
                                               int revise_tick_ms,
                                               int delay_ms,
                                               int lightning_skill_level) {
        return combat::resolve_magic(attacker,
                                     target,
                                     skill_index,
                                     skill_level,
                                     intelligence,
                                     lightning_mastery,
                                     revise_tick_ms,
                                     delay_ms,
                                     lightning_skill_level);
    }

    bool
    Combat::in_attack_range(int attacker_x, int attacker_y, int target_x, int target_y, int range) {
        return combat::in_attack_range(attacker_x, attacker_y, target_x, target_y, range);
    }

}  // namespace openkal::server
