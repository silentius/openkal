/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "openkal/server/BofCatalog.h"
#include "test_util.h"

#include <string>
#include <vector>

#ifndef OPENKAL_REPO_ROOT
#error OPENKAL_REPO_ROOT required
#endif

namespace {

    [[nodiscard]] openkal::server::BofWornPiece piece(int grade) {
        openkal::server::BofWornPiece p;
        p.set_grade(grade);
        return p;
    }

    [[nodiscard]] std::vector<openkal::server::BofWornPiece> n_of(int grade, int count) {
        std::vector<openkal::server::BofWornPiece> out;
        out.reserve(static_cast<std::size_t>(count));
        for (int i = 0; i < count; ++i) {
            out.push_back(piece(grade));
        }
        return out;
    }

}  // namespace

int main() {
    using openkal::server::BofApplyResult;
    using openkal::server::BofCatalog;
    using openkal::test::expect;

    BofCatalog catalog;
    const std::string root = OPENKAL_REPO_ROOT;
    expect(catalog.load_file(root + "/config/bof.json"), "load config/bof.json");
    expect(catalog.ready(), "catalog ready");
    expect(catalog.min_grade() == 46, "minGrade 46");
    expect(catalog.half_count() == 3 && catalog.full_count() == 5, "half 3 full 5");
    expect(catalog.success_rate() == 100, "successRate 100");
    expect(catalog.half_defense_percent() == 3, "halfDefensePercent 3");
    expect(catalog.full_hp_mp_percent() == 10 && catalog.full_resist_percent() == 10,
           "full percents 10");

    expect(catalog.is_bof_armor_part("helmet") && catalog.is_bof_armor_part("hat") &&
               catalog.is_bof_armor_part("upperarmor") && catalog.is_bof_armor_part("lowerarmor") &&
               catalog.is_bof_armor_part("pants") && catalog.is_bof_armor_part("gauntlet") &&
               catalog.is_bof_armor_part("boots"),
           "armor parts include hat/lowerarmor");
    expect(!catalog.is_bof_armor_part("shield") && !catalog.is_bof_armor_part("face") &&
               !catalog.is_bof_armor_part("mask"),
           "shield/face/mask are not BOF parts");

    const int grades[] = {46, 50, 55, 60, 65, 70, 75};
    const int pieces[] = {1, 2, 3, 4, 5, 6, 7};
    const int halves[] = {5, 6, 7, 8, 9, 10, 11};
    const int fulls[] = {10, 11, 12, 13, 14, 15, 16};
    for (int i = 0; i < 7; ++i) {
        const auto tier = catalog.tier_for_grade(grades[i]);
        expect(tier.piece() == pieces[i] && tier.half_set() == halves[i] &&
                   tier.full_set() == fulls[i],
               "explicit tier");
    }

    const auto fb = catalog.tier_for_grade(80);
    expect(fb.piece() == 8 && fb.half_set() == 12 && fb.full_set() == 17,
           "fallback G80 piece=grade/5-8");
    const auto below = catalog.tier_for_grade(45);
    expect(below.piece() == 0 && below.half_set() == 0, "below minGrade has no stats");

    expect(catalog.fx_level_for_grade(46) == 46, "FX snap G46");
    expect(catalog.fx_level_for_grade(64) == 60, "FX snap G64 to 60");
    expect(catalog.fx_level_for_grade(70) == 65, "FX snap G70 to 65");
    expect(catalog.fx_level_for_grade(45) == 0, "FX snap below min is 0");

    expect(BofCatalog::third_stat_is_intel(1), "mage third stat is INT");
    expect(!BofCatalog::third_stat_is_intel(0) && !BofCatalog::third_stat_is_intel(2) &&
               !BofCatalog::third_stat_is_intel(3),
           "knight/archer/thief third stat is DEX");

    expect(catalog.can_apply(true, "helmet", 46, false, false) == BofApplyResult::Ok,
           "valid G46 helmet");
    expect(catalog.can_apply(true, "boots", 60, false, false) == BofApplyResult::Ok,
           "valid G60 boots");
    expect(catalog.can_apply(true, "shield", 60, false, false) == BofApplyResult::InvalidTarget,
           "shield rejected");
    expect(catalog.can_apply(true, "helmet", 40, false, false) == BofApplyResult::InvalidTarget,
           "G40 rejected");
    expect(catalog.can_apply(false, "helmet", 60, false, false) == BofApplyResult::InvalidTarget,
           "non-defense rejected");
    expect(catalog.can_apply(true, "helmet", 60, false, true) == BofApplyResult::AlreadyApplied,
           "already-bof rejected");
    expect(catalog.can_apply(true, "helmet", 60, true, false) == BofApplyResult::Equipped,
           "equipped rejected");

    expect(catalog.roll_success(1) && catalog.roll_success(100), "rate 100 always succeeds");

    {
        const auto bonus = catalog.compute_bonus(0, n_of(46, 1));
        expect(bonus.str() == 1 && bonus.hth() == 1 && bonus.dex() == 1 && bonus.intl() == 0,
               "one G46 piece STR/HTH/DEX");
        expect(bonus.completeness() == 0 && bonus.defense_percent() == 0 && bonus.fx_level() == 0,
               "one piece is not a set");
    }
    {
        const auto bonus = catalog.compute_bonus(1, n_of(46, 1));
        expect(bonus.str() == 1 && bonus.hth() == 1 && bonus.intl() == 1 && bonus.dex() == 0,
               "mage piece uses INT");
    }
    {
        const auto bonus = catalog.compute_bonus(0, n_of(60, 3));
        expect(bonus.completeness() == 1 && bonus.set_grade() == 60, "3x G60 is half set");
        expect(bonus.str() == 3 * 4 + 8 && bonus.hth() == 20 && bonus.dex() == 20,
               "half set adds halfSet not stacked full");
        expect(bonus.defense_percent() == 3 && bonus.hp_mp_percent() == 0 &&
                   bonus.resist_percent() == 0,
               "half set +3% def only");
        expect(bonus.fx_level() == 60, "half G60 FX level 60");
    }
    {
        const auto bonus = catalog.compute_bonus(3, n_of(60, 5));
        expect(bonus.completeness() == 2, "5x G60 is full set");
        expect(bonus.str() == 5 * 4 + 13 && bonus.dex() == 33 && bonus.intl() == 0,
               "full uses fullSet instead of halfSet");
        expect(bonus.defense_percent() == 3 && bonus.hp_mp_percent() == 10 &&
                   bonus.resist_percent() == 10,
               "full +3% def +10% HP/MP/resist");
        expect(bonus.fx_level() == 60, "full G60 FX level 60");
    }
    {
        std::vector<openkal::server::BofWornPiece> mixed = n_of(60, 3);
        mixed.push_back(piece(46));
        mixed.push_back(piece(46));
        const auto bonus = catalog.compute_bonus(0, mixed);
        expect(bonus.completeness() == 1 && bonus.set_grade() == 60, "3xG60 + 2xG46 is G60 half");
        expect(bonus.str() == 3 * 4 + 2 * 1 + 8, "mixed keeps per-piece stats on all five");
        expect(bonus.defense_percent() == 3 && bonus.hp_mp_percent() == 0,
               "mixed is half not full");
        expect(bonus.fx_level() == 60, "mixed FX follows winning grade");
    }
    {
        const auto bonus = catalog.compute_bonus(0, n_of(70, 5));
        expect(bonus.fx_level() == 65, "G70 full snaps FX to 65");
        expect(bonus.str() == 5 * 6 + 15, "G70 stats still scale");
    }

    if (openkal::test::g_failures != 0) {
        return 1;
    }
    return 0;
}
