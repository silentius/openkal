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
#include "test_util.h"

int main() {
    using namespace openkal::server;
    using openkal::test::expect;
    expect(can_apply_polishing_stone(true, false, false, 1, false), "weapon accepts stone");
    expect(can_apply_polishing_stone(false, true, false, 1, false), "armor accepts stone");
    expect(!can_apply_polishing_stone(false, false, false, 1, false), "other items reject stone");
    expect(!can_apply_polishing_stone(true, false, true, 1, false), "worn items reject stone");
    expect(!can_apply_polishing_stone(true, false, false, 2, false), "stacks reject stone");
    expect(!can_apply_polishing_stone(true, false, false, 1, true), "protection cannot stack");
    const auto protected_prefix = apply_prefix_fail_endurance(false, 1, 30, 5, true);
    expect(!protected_prefix.destroyed && protected_prefix.cur_end == 1,
           "protection prevents fatal prefix damage without repairing the item");
    expect(apply_prefix_fail_endurance(false, 1, 30, 5).destroyed,
           "ordinary prefix damage still destroys fragile gear");
    for (const bool weapon : {true, false}) {
        CharmTarget target;
        target.is_weapon = weapon;
        target.is_defense = !weapon;
        target.grade = 40;
        CharmState state;
        state.x_attack = state.x_defense = 5;
        state.cur_end = 1;
        state.max_end = 30;
        state.enchant_protected = true;
        const auto kind = weapon ? CharmKind::XAttack : CharmKind::XDefense;
        for (int destruction_roll : {1, 100}) {
            const auto failed = apply_charm(kind, state, target, 10000000, destruction_roll, 1);
            expect(failed.outcome == CharmOutcome::FailProtected && failed.state.cur_end == 1 &&
                       failed.state.max_end == 30 && !failed.state.enchant_protected &&
                       failed.stones == 0 && failed.state.x_attack == state.x_attack &&
                       failed.state.x_defense == state.x_defense,
                   "one stone prevents damage and direct destruction without changing stats");
            const auto second = apply_charm(kind, failed.state, target, 10000000, 100, 1);
            expect(second.outcome == CharmOutcome::FailDestroyed,
                   "the very next accepted attempt is unprotected");
        }
        const auto success = apply_charm(kind, state, target, 1, 100, 1);
        expect(success.outcome == CharmOutcome::Success && !success.state.enchant_protected &&
                   success.state.cur_end == 1,
               "success consumes protection too and preserves endurance");
        target.equipped = true;
        const auto invalid = apply_charm(kind, state, target, 1, 100, 1);
        expect(invalid.outcome == CharmOutcome::Rejected && invalid.state.enchant_protected,
               "rejected attempts retain protection");
        target.equipped = false;
        state.x_attack = state.x_defense = 20;
        const auto capped = apply_charm(kind, state, target, 1, 100, 1);
        expect(capped.outcome == CharmOutcome::Rejected && capped.state.enchant_protected,
               "capped enchantments retain protection");
    }
    CharmTarget sword;
    sword.is_weapon = true;
    sword.grade = 40;
    CharmState state;
    state.x_attack = 5;
    state.upgrade_level = 1;
    state.enchant_protected = true;
    int gold = 0, stones = 0;
    expect(try_upgrade_rate_up(state, sword, 10, stones) == UpgradeRateOutcome::Applied &&
               state.enchant_protected,
           "increasing the success rate does not consume a protected attempt");
    expect(apply_upgrade_level(state, sword, 0, 100, 100, gold) == UpgradeLevelOutcome::Rejected &&
               state.enchant_protected && gold == 0,
           "insufficient gold preserves protection");
    for (int roll : {70, 100}) {
        state.enchant_protected = true;
        expect(apply_upgrade_level(state, sword, 10000, 100, roll, gold) ==
                       UpgradeLevelOutcome::FailKeep &&
                   state.upgrade_level == 1 && !state.enchant_protected && gold == 3000,
               "protected Explosive Blow prevents destruction and downgrade, charging normal gold");
    }
    state.enchant_protected = true;
    expect(apply_upgrade_level(state, sword, 10000, 1, 100, gold) == UpgradeLevelOutcome::Success &&
               !state.enchant_protected && state.upgrade_level == 2,
           "successful Explosive Blow also consumes protection");
    return openkal::test::g_failures == 0 ? 0 : 1;
}
