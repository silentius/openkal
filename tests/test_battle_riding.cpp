/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/BattleRiding.h"

#include <iostream>

int main() {
    int failures = 0;
    const auto expect = [&failures](bool condition, const char* label) {
        std::cout << (condition ? "PASS " : "FAIL ") << label << '\n';
        failures += condition ? 0 : 1;
    };

    using openkal::protocol::BattleRidingGrade;
    using namespace openkal::server;

    expect(battle_riding_grade_rarity(BattleRidingGrade::Normal) == 0 &&
               battle_riding_grade_rarity(BattleRidingGrade::Uncommon) == 2 &&
               battle_riding_grade_rarity(BattleRidingGrade::Rare) == 1 &&
               battle_riding_grade_rarity(BattleRidingGrade::Epic) == 5,
           "Battle grades reuse the established ItemRarity presentation");
    expect(battle_riding_initial_ability_slots(BattleRidingGrade::Normal) == 1 &&
               battle_riding_initial_ability_slots(BattleRidingGrade::Rare) == 2,
           "grade controls initial ability slots");
    expect(battle_riding_combine_chance(BattleRidingGrade::Normal) == 100 &&
               battle_riding_combine_chance(BattleRidingGrade::Uncommon) == 70 &&
               battle_riding_combine_chance(BattleRidingGrade::Rare) == 40 &&
               battle_riding_combine_chance(BattleRidingGrade::Epic) == 0,
           "grade-up probabilities use the accepted defaults");
    expect(!battle_riding_skill_allowed(BattleRidingGrade::Rare, 0, 21) &&
               !battle_riding_skill_allowed(BattleRidingGrade::Rare, 1, 54) &&
               !battle_riding_skill_allowed(BattleRidingGrade::Rare, 2, 46) &&
               !battle_riding_skill_allowed(BattleRidingGrade::Rare, 3, 6),
           "grades below Epic retain class-specific mounted restrictions");
    expect(battle_riding_skill_allowed(BattleRidingGrade::Epic, 0, 21) &&
               battle_riding_skill_allowed(BattleRidingGrade::Epic, 1, 54),
           "Epic removes class-specific mounted restrictions");
    expect(battle_riding_satiety_scaled_value(35, 50, 100) == 17 &&
               battle_riding_satiety_scaled_value(35, 0, 100) == 0,
           "equipment value scales linearly with satiety");
    BattleMountRecord mount;
    mount.satiety = 1;
    battle_riding_consume_skill_hit(mount);
    battle_riding_consume_skill_hit(mount);
    expect(mount.satiety == 0 && mount.revision == 1,
           "successful hits consume bounded satiety and advance revision");

    return failures == 0 ? 0 : 1;
}
