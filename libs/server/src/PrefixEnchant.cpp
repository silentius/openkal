/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/PrefixEnchant.h"

#include <algorithm>
#include <string_view>

namespace openkal::server {
    namespace {

        constexpr int kCharmUpRate[3][24] = {
            {10000000, 10000000, 10000000, 10000000, 9200000, 8200000, 7000000, 5600000,
             4000000,  3200000,  1200000,  600000,   400000,  100000,  50000,   10000,
             1000,     100,      10,       10,       10,      10,      10,      10},
            {10000000, 8900000, 5300000, 3700000, 1900000, 1400000, 800000, 300000,
             50000,    10000,   1000,    100,     10,      10,      10,     10,
             10,       10,      10,      10,      10,      10,      10,     10},
            {10000000, 6700000, 3800000, 1500000, 900000, 50000, 10000, 10, 10, 10, 10, 10,
             10,       10,      10,      10,      10,     10,    10,    10, 10, 10, 10, 10},
        };

        constexpr int kUpgrDefRate[20] = {50, 46, 42, 38, 34, 30, 26, 22, 18, 14,
                                          10, 8,  6,  4,  2,  1,  1,  1,  1,  1};
        constexpr int kUpgrLUpRate[21] = {50, 40, 30, 20, 15, 10, 8, 6, 4, 3, 2,
                                          1,  1,  1,  1,  1,  1,  1, 1, 1, 1};
        constexpr int kUpgrNeedCnt[20] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                                          1, 1, 2, 2, 2, 3, 3, 4, 5, 6};
        constexpr int kUpgrWLevel[21] = {5,  6,  6,  7,  7,  8,  8,  9,  10, 11, 13,
                                         15, 18, 21, 24, 24, 24, 24, 24, 24, 24};
        constexpr int kUpgrWCharm[21] = {0,  0,  0,  1,  1,   2,   2,   3,   4,   5,  6,
                                         10, 18, 29, 74, 100, 100, 100, 100, 100, 100};
        constexpr int kUpgrDSLevel[21] = {0, 0, 0, 0, 2, 2, 2, 2, 3, 3, 3,
                                          3, 4, 4, 4, 4, 4, 4, 4, 4, 4};
        constexpr int kUpgrDCharm[11] = {0, 0, 0, 1, 3, 5, 8, 14, 29, 50, 100};

        [[nodiscard]] int clamp_index(int value, int max_inclusive) {
            if (value < 0) {
                return 0;
            }
            if (value > max_inclusive) {
                return max_inclusive;
            }
            return value;
        }

        [[nodiscard]] int plus_of(CharmKind kind, const CharmState& state) {
            switch (kind) {
            case CharmKind::XAttack:
                return state.x_attack;
            case CharmKind::XMagic:
                return state.x_magic;
            case CharmKind::XHit:
                return state.x_hit;
            case CharmKind::XDefense:
                return state.x_defense;
            case CharmKind::XDodge:
                return state.x_dodge;
            default:
                return 0;
            }
        }

        void set_plus(CharmKind kind, CharmState& state, int value) {
            switch (kind) {
            case CharmKind::XAttack:
                state.x_attack = value;
                break;
            case CharmKind::XMagic:
                state.x_magic = value;
                break;
            case CharmKind::XHit:
                state.x_hit = value;
                break;
            case CharmKind::XDefense:
                state.x_defense = value;
                break;
            case CharmKind::XDodge:
                state.x_dodge = value;
                break;
            default:
                break;
            }
        }

        [[nodiscard]] int plus_cap(CharmKind kind) {
            switch (kind) {
            case CharmKind::XAttack:
            case CharmKind::XMagic:
            case CharmKind::XHit:
                return 20;
            case CharmKind::XDefense:
                return 10;
            case CharmKind::XDodge:
                return 5;
            default:
                return 0;
            }
        }

    }  // namespace

    bool can_apply_polishing_stone(
        bool is_weapon, bool is_defense, bool equipped, int count, bool protected_item) {
        return (is_weapon || is_defense) && !equipped && count == 1 && !protected_item;
    }

    void PrefixCombatDef::apply_primaries(
        int& o_str, int& o_hth, int& o_intl, int& o_wis, int& o_dex) const {
        o_str += str;
        o_hth += hth;
        o_intl += intl;
        o_wis += wis;
        o_dex += dex;
    }

    void PrefixCombatDef::apply_derived(int& o_hit,
                                        int& o_min_attack,
                                        int& o_max_attack,
                                        int& o_min_magic,
                                        int& o_max_magic,
                                        int& o_defense,
                                        int& o_absorb,
                                        int& o_res_fire,
                                        int& o_res_ice,
                                        int& o_res_lightning,
                                        int& o_res_curse,
                                        int& o_res_palsy) const {
        o_hit += hit;
        o_min_attack += min_attack;
        o_max_attack += max_attack;
        o_min_magic += min_magic;
        o_max_magic += max_magic;
        o_defense += defense;
        o_absorb += absorb;
        o_res_fire += res_fire;
        o_res_ice += res_ice;
        o_res_lightning += res_lightning;
        o_res_curse += res_curse;
        o_res_palsy += res_palsy;
    }

    int roll_change_prefix(int current, int rate0, int prefix0, int rate1, int prefix1, int roll) {
        if (roll <= rate0) {
            return prefix0;
        }
        if (roll <= rate1) {
            return prefix1;
        }
        return current;
    }

    bool prefix_target_matches(std::string_view target,
                               bool is_weapon,
                               bool is_defense,
                               bool is_ornament) {
        if (target == "weapon") {
            return is_weapon;
        }
        if (target == "defense") {
            return is_defense;
        }
        if (target == "ornament") {
            return is_ornament;
        }
        return false;
    }

    bool item_uses_endurance(bool is_ornament, int endurance) {
        return !is_ornament && endurance > 0;
    }

    PrefixFailEndurance apply_prefix_fail_endurance(
        bool is_ornament, int cur_end, int max_end, int cut, bool enchant_protected) {
        PrefixFailEndurance out;
        out.cur_end = cur_end;
        if (enchant_protected || !item_uses_endurance(is_ornament, max_end)) {
            return out;
        }
        out.cur_end = cur_end - cut;
        if (out.cur_end < 1) {
            out.cur_end = 0;
            out.destroyed = true;
        }
        return out;
    }

    bool can_apply_revision_hammer(bool is_weapon,
                                   bool is_defense,
                                   bool is_ornament,
                                   std::string_view class_small,
                                   bool equipped,
                                   int count,
                                   int grade) {
        const bool accessory = is_ornament && (class_small == "ring" || class_small == "trinket");
        return (is_weapon || is_defense || accessory) && !equipped && count == 1 &&
               (accessory || grade > 0);
    }

    int revision_hammer_stones(bool is_weapon, bool is_ornament, int grade, int extra) {
        if (is_ornament) {
            return 1;
        }
        if (grade <= 0) {
            return 0;
        }
        return (is_weapon ? 4 : 2) + std::clamp(extra, 0, grade / 10);
    }

    int revision_stones_for_grade(int grade, int extra) {
        if (grade <= 0) {
            return 0;
        }
        const int span = grade / 10;
        const int clamped = std::clamp(extra, 0, span);
        return 1 + clamped;
    }

    RepairState apply_repair(int mode, int amount, int cur_end, int max_end) {
        RepairState out;
        out.cur_end = cur_end;
        out.max_end = max_end;
        if (max_end <= 0 || amount <= 0) {
            return out;
        }
        if (mode == 1) {
            if (cur_end < kDoggebiDestroyBelow) {
                out.outcome = RepairOutcome::Destroyed;
                out.cur_end = 0;
                return out;
            }
            out.max_end = std::max(1, max_end - amount);
            out.cur_end = out.max_end;
            out.outcome = RepairOutcome::Repaired;
            return out;
        }
        if (cur_end >= max_end) {
            return out;
        }
        out.cur_end = std::min(max_end, cur_end + amount);
        out.outcome = RepairOutcome::Repaired;
        return out;
    }

    CharmKind parse_charm_kind(std::string_view token) {
        if (token == "xattack") {
            return CharmKind::XAttack;
        }
        if (token == "xmagic") {
            return CharmKind::XMagic;
        }
        if (token == "xhit") {
            return CharmKind::XHit;
        }
        if (token == "xdefense") {
            return CharmKind::XDefense;
        }
        if (token == "xdodge") {
            return CharmKind::XDodge;
        }
        return CharmKind::None;
    }

    bool charm_target_matches(CharmKind kind, const CharmTarget& target) {
        if (target.equipped || target.grade < 40) {
            return false;
        }
        switch (kind) {
        case CharmKind::XAttack:
            return target.is_weapon && !target.is_wand;
        case CharmKind::XMagic:
            return target.is_weapon && target.is_wand;
        case CharmKind::XHit:
            return target.is_weapon;
        case CharmKind::XDefense:
        case CharmKind::XDodge:
            return target.is_defense;
        default:
            return false;
        }
    }

    int charm_bonus_attack(int x) {
        if (x <= 0) {
            return 0;
        }
        return x * (2 * x + 7) / 9;
    }

    int charm_success_rate(CharmKind kind, int current) {
        if (current < 0) {
            return 0;
        }
        if (kind == CharmKind::XDodge) {
            switch (current) {
            case 0:
                return 5000000;
            case 1:
                return 2500000;
            case 2:
                return 1000000;
            case 3:
                return 500000;
            case 4:
                return 100000;
            default:
                return 0;
            }
        }
        int row = 0;
        if (kind == CharmKind::XDefense) {
            row = 1;
        } else if (kind == CharmKind::XHit) {
            row = 2;
        }
        const int idx = clamp_index(current, 23);
        return kCharmUpRate[row][idx];
    }

    int charm_destroy_weapon_stones(int grade, bool is_wand, int x_attack, int x_magic) {
        int wlevel = grade - 40;
        if (wlevel < 0) {
            wlevel = 0;
        }
        if (wlevel > 20) {
            wlevel = 20;
        }
        int x_value = 20;
        if (is_wand) {
            if (x_magic < 20) {
                x_value = std::max(0, x_magic);
            }
        } else if (x_attack < 20) {
            x_value = std::max(0, x_attack);
        }
        x_value = clamp_index(x_value, 20);
        return kUpgrWLevel[wlevel] + kUpgrWCharm[x_value];
    }

    int charm_destroy_defense_stones(int grade, bool is_shield, int x_defense, int sell) {
        int x_value = 10;
        if (x_defense < 10) {
            x_value = std::max(0, x_defense);
        }
        x_value = clamp_index(x_value, 10);
        if (is_shield) {
            if (grade < 44) {
                return 0;
            }
            int slevel = grade - 40;
            if (slevel > 20) {
                slevel = 20;
            }
            if (slevel < 0) {
                slevel = 0;
            }
            return kUpgrDSLevel[slevel] + kUpgrDCharm[x_value];
        }
        return kUpgrDCharm[x_value] + (sell + 1000) / 10000;
    }

    CharmRollResult apply_charm(CharmKind kind,
                                const CharmState& in,
                                const CharmTarget& target,
                                int success_roll,
                                int destroy_roll,
                                int cut_roll) {
        CharmRollResult out;
        out.state = in;
        if (kind == CharmKind::None || !charm_target_matches(kind, target)) {
            return out;
        }
        const int current = plus_of(kind, in);
        if (current >= plus_cap(kind)) {
            return out;
        }
        out.state.enchant_protected = false;
        const int rate = charm_success_rate(kind, current);
        if (rate >= success_roll) {
            set_plus(kind, out.state, current + 1);
            out.outcome = CharmOutcome::Success;
            return out;
        }

        if (in.enchant_protected) {
            out.outcome = CharmOutcome::FailProtected;
            return out;
        }
        int cur_end = in.cur_end;
        if (destroy_roll > 90) {
            cur_end = 0;
        } else {
            int cut = 1;
            if (cut_roll <= 20) {
                cut = 3;
            } else if (cut_roll <= 50) {
                cut = 2;
            }
            cur_end -= cut;
        }
        out.state.cur_end = cur_end;
        if (cur_end <= 0) {
            out.state.cur_end = 0;
            out.outcome = CharmOutcome::FailDestroyed;
            if (target.is_weapon) {
                out.stones = charm_destroy_weapon_stones(
                    target.grade, target.is_wand, in.x_attack, in.x_magic);
            } else if (target.is_defense) {
                out.stones = charm_destroy_defense_stones(
                    target.grade, target.is_shield, in.x_defense, target.sell);
            }
            return out;
        }
        out.outcome = CharmOutcome::FailDamaged;
        return out;
    }

    int explosive_blow_chance(int upgrade_level, int upgrade_rate) {
        const int idx = clamp_index(upgrade_level, 19);
        const int chance = kUpgrDefRate[idx] + upgrade_rate * kUpgrLUpRate[idx];
        if (chance < 0) {
            return 0;
        }
        if (chance > 100) {
            return 100;
        }
        return chance;
    }

    int explosive_blow_stone_cost(int upgrade_level) {
        return kUpgrNeedCnt[clamp_index(upgrade_level, 19)];
    }

    int explosive_blow_gold_cost(int upgrade_level) {
        const int level = std::max(0, upgrade_level);
        return 1500 * (level + 1);
    }

    int explosive_blow_cap(bool is_wand, int x_attack, int x_magic) {
        return is_wand ? std::max(0, x_magic) : std::max(0, x_attack);
    }

    UpgradeRateOutcome try_upgrade_rate_up(CharmState& in,
                                           const CharmTarget& target,
                                           int stones_owned,
                                           int& stones_consumed) {
        stones_consumed = 0;
        if (target.equipped || target.grade < 40 || !target.is_weapon) {
            return UpgradeRateOutcome::Rejected;
        }
        if (explosive_blow_chance(in.upgrade_level, in.upgrade_rate) >= 100) {
            return UpgradeRateOutcome::Rejected;
        }
        const int need = explosive_blow_stone_cost(in.upgrade_level);
        if (stones_owned < need) {
            return UpgradeRateOutcome::Rejected;
        }
        stones_consumed = need;
        in.upgrade_rate += 1;
        return UpgradeRateOutcome::Applied;
    }

    UpgradeLevelOutcome apply_upgrade_level(CharmState& in,
                                            const CharmTarget& target,
                                            std::int64_t gold_owned,
                                            int success_roll,
                                            int fail_roll,
                                            int& gold_spent) {
        gold_spent = 0;
        if (target.equipped || target.grade < 40 || !target.is_weapon) {
            return UpgradeLevelOutcome::Rejected;
        }
        const int cap = explosive_blow_cap(target.is_wand, in.x_attack, in.x_magic);
        if (in.upgrade_level >= cap) {
            return UpgradeLevelOutcome::Rejected;
        }
        const int cost = explosive_blow_gold_cost(in.upgrade_level);
        if (gold_owned < cost) {
            return UpgradeLevelOutcome::Rejected;
        }
        gold_spent = cost;
        const bool protected_attempt = in.enchant_protected;
        in.enchant_protected = false;
        const int chance = explosive_blow_chance(in.upgrade_level, in.upgrade_rate);
        if (chance >= success_roll) {
            in.upgrade_rate = 0;
            in.upgrade_level += 1;
            return UpgradeLevelOutcome::Success;
        }
        if (protected_attempt) {
            return UpgradeLevelOutcome::FailKeep;
        }
        if (fail_roll > 90) {
            return UpgradeLevelOutcome::Destroyed;
        }
        if (fail_roll > 60) {
            in.upgrade_rate = 0;
            if (in.upgrade_level > 0) {
                in.upgrade_level -= 1;
            }
            return UpgradeLevelOutcome::FailDowngrade;
        }
        return UpgradeLevelOutcome::FailKeep;
    }

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
                              int& defense_range) {
        if (is_weapon) {
            const int atk = charm_bonus_attack(state.x_attack);
            min_attack += atk;
            max_attack += atk;
            const int mag = charm_bonus_attack(state.x_magic);
            min_magic += mag;
            max_magic += mag;
            hit += state.x_hit;
        }
        if (is_defense) {
            defense += state.x_defense;
            defense_range += state.x_defense;
            dodge += state.x_dodge;
        }
    }

}  // namespace openkal::server
