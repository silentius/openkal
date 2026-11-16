/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/Qigong.h"
#include "test_util.h"

int main() {
    using openkal::server::Qigong;
    using openkal::server::QigongApplyResult;
    using openkal::server::QigongBonus;
    using openkal::test::expect;

    expect(Qigong::is_talisman(23020) && !Qigong::is_talisman(3056) && !Qigong::is_talisman(3058),
           "only index 23020 is the active Qigong Talisman");
    expect(Qigong::is_perforation_shot(23018) && !Qigong::is_perforation_shot(3057) &&
               !Qigong::is_perforation_shot(3059),
           "only index 23018 is the active Perforation Shot");
    expect(Qigong::can_perforate(true, false, "sword", 40, false, false) == QigongApplyResult::Ok,
           "level-40 weapon can be perforated");
    expect(Qigong::can_perforate(false, true, "upperarmor", 40, false, false) ==
               QigongApplyResult::Ok,
           "level-40 upper armor can be perforated");
    expect(Qigong::can_perforate(false, true, "ring", 60, false, false) ==
               QigongApplyResult::InvalidTarget,
           "ornament cannot be perforated");
    expect(Qigong::can_perforate(true, false, "sword", 39, false, false) ==
               QigongApplyResult::InvalidTarget,
           "items below level 40 are rejected");
    expect(Qigong::can_perforate(true, false, "sword", 40, true, false) ==
               QigongApplyResult::Equipped,
           "equipped items are rejected");
    expect(Qigong::can_perforate(true, false, "sword", 40, false, true) ==
               QigongApplyResult::AlreadyPerforated,
           "second perforation is rejected");
    expect(Qigong::can_apply(true, false, "sword", 40, false, false) ==
               QigongApplyResult::NotPerforated,
           "talisman requires perforation");
    expect(Qigong::can_apply(true, false, "sword", 40, false, true) == QigongApplyResult::Ok,
           "talisman permits a perforated weapon and rerolls it");
    expect(Qigong::perforation_succeeds(1) && Qigong::perforation_succeeds(50) &&
               !Qigong::perforation_succeeds(51),
           "perforation success rate is bounded at 50 percent");
    expect(Qigong::roll_effect(1, 0) == 1 && Qigong::roll_effect(24, 9) == 10,
           "the first tier receives its full 24 percent weight");
    expect(Qigong::roll_effect(25, 0) == 11 && Qigong::roll_effect(95, 9) == 70,
           "tier boundaries select the expected weighted effects");
    expect(Qigong::roll_effect(96, 0) == 71 && Qigong::roll_effect(100, 9) == 80,
           "the highest tier has exactly five percent probability");

    const QigongBonus sword_low = QigongBonus::from_effect(1, true, "sword");
    expect(sword_low.tier() == 1 && sword_low.attack_percent() == 2 &&
               sword_low.magic_percent() == 2 && sword_low.hp() == 30 && sword_low.mp() == 10 &&
               sword_low.hit() == 2 && sword_low.primary_kind() == 0 &&
               sword_low.primary_points() == 2,
           "weapon effect 1 matches Black Imoogi low roll");
    const QigongBonus wand_high = QigongBonus::from_effect(80, true, "wand");
    expect(wand_high.tier() == 8 && wand_high.attack_percent() == 19 &&
               wand_high.magic_percent() == 19 && wand_high.dodge() == 18 && wand_high.hit() == 0 &&
               wand_high.primary_kind() == 4 && wand_high.primary_points() == 8,
           "wand effect 80 uses high Black Insanity values and evasion");
    const QigongBonus upper = QigongBonus::from_effect(80, false, "upperarmor");
    expect(upper.tier() == 8 && upper.defense() == 60 && upper.primary_points() == 7,
           "upper armor uses its Black Insanity stat row");
    const QigongBonus lower = QigongBonus::from_effect(80, false, "lowerarmor");
    expect(lower.tier() == 8 && lower.defense() == 60 && lower.primary_points() == 6,
           "lower armor uses its Black Insanity stat row");
    const QigongBonus helmet = QigongBonus::from_effect(80, false, "helmet");
    expect(helmet.tier() == 8 && helmet.defense() == 60 && helmet.primary_points() == 5,
           "helmet uses its Black Insanity stat row");

    return openkal::test::g_failures == 0 ? 0 : 1;
}
