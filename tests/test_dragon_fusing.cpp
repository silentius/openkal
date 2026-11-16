/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/DragonFusing.h"
#include "test_util.h"

int main() {
    using openkal::server::DragonFusing;
    using openkal::server::DragonFusingApplyResult;
    using openkal::server::DragonFusingBonus;
    using openkal::test::expect;

    expect(DragonFusing::is_scroll(23019) && !DragonFusing::is_scroll(2102) &&
               !DragonFusing::is_scroll(2365),
           "only index 23019 is the active Dragon Spirit Scroll");
    expect(DragonFusing::can_apply(true, false, "sword", false) == DragonFusingApplyResult::Ok,
           "weapons can receive Dragon Fusing");
    expect(DragonFusing::can_apply(false, true, "upperarmor", false) == DragonFusingApplyResult::Ok,
           "armor can receive Dragon Fusing");
    expect(DragonFusing::can_apply(false, true, "ring", false) ==
               DragonFusingApplyResult::InvalidTarget,
           "ornaments are not covered by the Dragon Fusing stat table");
    expect(DragonFusing::can_apply(true, false, "sword", true) == DragonFusingApplyResult::Equipped,
           "equipped targets are rejected");
    expect(DragonFusing::can_apply(true, false, "sword", false) == DragonFusingApplyResult::Ok,
           "an existing Dragon Fusing effect can be rerolled");

    expect(DragonFusing::roll_effect(1, 0, 0, true, "sword") == 1 &&
               DragonFusing::roll_effect(100, 4, 2, true, "sword") == 237,
           "rolls preserve tier, primary stat, and exact table value");

    const DragonFusingBonus sword = DragonFusingBonus::from_effect(1, true, "sword");
    expect(sword.tier() == 1 && sword.attack_percent() == 1 && sword.magic_percent() == 1 &&
               sword.hp() == 30 && sword.mp() == 10 && sword.hit() == 2 &&
               sword.primary_kind() == 0 && sword.primary_points() == 1,
           "weapon effect 1 matches Imoogi");
    const DragonFusingBonus wand = DragonFusingBonus::from_effect(237, true, "wand");
    expect(wand.tier() == 8 && wand.attack_percent() == 17 && wand.dodge() == 18 &&
               wand.hit() == 0 && wand.primary_kind() == 4 && wand.primary_points() == 8,
           "wand effect uses the Insanity Dragon high roll and evasion");
    const DragonFusingBonus upper = DragonFusingBonus::from_effect(240, false, "upperarmor");
    expect(upper.tier() == 8 && upper.defense() == 40 && upper.primary_points() == 7,
           "upper armor uses its Insanity Dragon stat row");
    const DragonFusingBonus lower = DragonFusingBonus::from_effect(240, false, "lowerarmor");
    expect(lower.tier() == 8 && lower.defense() == 40 && lower.primary_points() == 6,
           "lower armor uses its Insanity Dragon stat row");

    return openkal::test::g_failures == 0 ? 0 : 1;
}
