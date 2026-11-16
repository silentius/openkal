/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "openkal/server/MixCatalog.h"
#include "test_util.h"

#include <string>
#include <vector>

#ifndef OPENKAL_REPO_ROOT
#error OPENKAL_REPO_ROOT required
#endif

namespace {

    openkal::server::MixItemOffer
    make_weapon(std::uint32_t iid, int index, int grade, const char* kind) {
        openkal::server::MixItemOffer offer;
        offer.set_iid(iid);
        offer.set_index(index);
        offer.set_grade(grade);
        offer.set_wearable(true);
        offer.set_weapon(true);
        offer.set_class_small(kind);
        return offer;
    }

    openkal::server::MixItemOffer
    make_armor(std::uint32_t iid, int index, int grade, const char* kind) {
        openkal::server::MixItemOffer offer;
        offer.set_iid(iid);
        offer.set_index(index);
        offer.set_grade(grade);
        offer.set_wearable(true);
        offer.set_defense(true);
        offer.set_class_small(kind);
        return offer;
    }

    openkal::server::MixItemOffer make_stone(std::uint32_t iid, int index) {
        openkal::server::MixItemOffer offer;
        offer.set_iid(iid);
        offer.set_index(index);
        return offer;
    }

}  // namespace

int main() {
    using openkal::server::MixCatalog;
    using openkal::test::expect;

    MixCatalog catalog;
    const std::string root = OPENKAL_REPO_ROOT;
    expect(catalog.load_dir(root + "/config/mixing"), "load config/mixing");
    expect(catalog.stone_count() >= 13, "stones loaded");
    expect(catalog.recipe_count() >= 11, "upgrade + G3 + 9 making recipes");
    expect(catalog.is_mixing_stone(895) && catalog.stone_type(895) == 1, "shadow type 1");
    expect(catalog.stone_require_mix_level(904) == 2, "mystery requires mix level 2");
    expect(!catalog.is_mixing_stone(31), "gold is not a mix stone");

    expect(catalog.is_imperial_weapon(778) && catalog.is_imperial_weapon(779) &&
               !catalog.is_imperial_weapon(244),
           "making recipes identify Imperial definitions without item-name assumptions");

    const auto center50 = make_weapon(1, 244, 50, "sword");
    const auto extra50 = make_weapon(2, 245, 50, "sword");
    const auto chest46 = make_armor(3, 251, 46, "upperarmor");
    const auto helm46 = make_armor(4, 250, 46, "helmet");
    const auto armor50 = make_armor(5, 260, 50, "upperarmor");
    const auto stone_a = make_stone(6, 895);
    const auto stone_b = make_stone(7, 895);
    const auto stone_holy = make_stone(8, 896);
    const auto stone_mystery_a = make_stone(9, 904);
    const auto stone_mystery_b = make_stone(10, 904);
    const auto stone_blood = make_stone(11, 905);

    {
        std::vector<openkal::server::MixItemOffer> outer{extra50, chest46, stone_a, stone_b};
        const auto match = catalog.match(1, center50, outer, 0);
        expect(match.valid(), "g50 making with g46 chest");
        expect(match.recipe_id() == "make_g50", "matched make_g50");
        expect(match.extra_weapon_iid() == 2, "extra weapon iid");
        expect(match.replacement_index() == 778, "knight sword imperial g50");
    }
    {
        const auto two_hand = make_weapon(1, 442, 50, "sword2h");
        std::vector<openkal::server::MixItemOffer> outer{extra50, chest46, stone_a, stone_b};
        const auto match = catalog.match(1, two_hand, outer, 0);
        expect(match.valid() && match.replacement_index() == 779, "knight 2h imperial g50");
    }
    {
        std::vector<openkal::server::MixItemOffer> outer{extra50, helm46, stone_a, stone_b};
        const auto match = catalog.match(1, center50, outer, 0);
        expect(!match.valid(), "g50 making rejects g46 helmet");
    }
    {
        std::vector<openkal::server::MixItemOffer> outer{extra50, armor50, stone_a, stone_b};
        const auto match = catalog.match(1, center50, outer, 0);
        expect(!match.valid(), "g50 making rejects g50 armor");
    }
    {
        std::vector<openkal::server::MixItemOffer> outer{extra50, chest46, stone_a, stone_holy};
        const auto match = catalog.match(1, center50, outer, 0);
        expect(!match.valid(), "g50 making needs identical stones");
    }
    {
        std::vector<openkal::server::MixItemOffer> outer{chest46, stone_a};
        const auto up = catalog.match(0, center50, outer, 0);
        expect(!up.valid(), "upgrade needs the extra weapon");
    }
    {
        std::vector<openkal::server::MixItemOffer> outer{extra50, chest46, stone_a};
        const auto up = catalog.match(0, center50, outer, 0);
        expect(up.valid() && up.recipe_id() == "upgrade",
               "upgrade weapon + extra + armor + 1 stone");
        expect(up.extra_weapon_iid() == 2, "upgrade consumes extra weapon");
        expect(up.replacement_index() == 0, "upgrade keeps center item");
    }
    {
        const auto armor8 = make_armor(20, 80, 8, "upperarmor");
        std::vector<openkal::server::MixItemOffer> outer{extra50, armor8, stone_a};
        expect(!catalog.match(0, center50, outer, 0).valid(), "G50 mix rejects G8 armor");
    }
    {
        const auto extra8 = make_weapon(21, 90, 8, "sword");
        std::vector<openkal::server::MixItemOffer> outer{extra8, chest46, stone_a};
        expect(!catalog.match(0, center50, outer, 0).valid(), "G50 mix rejects G8 extra weapon");
    }
    {
        const auto center21 = make_weapon(22, 100, 21, "sword");
        std::vector<openkal::server::MixItemOffer> outer{extra50, chest46, stone_a};
        expect(!catalog.match(0, center21, outer, 0).valid(), "only mix-list weapon grades mix");
    }
    {
        std::vector<openkal::server::MixItemOffer> outer{extra50, armor50, stone_a};
        expect(!catalog.match(0, center50, outer, 0).valid(),
               "G50 mix needs G46 chest not G50 armor");
    }
    {
        std::vector<openkal::server::MixItemOffer> outer{extra50, chest46, stone_a, stone_b};
        const auto up = catalog.match(0, center50, outer, 0);
        expect(!up.valid(), "upgrade G1/G2 rejects two elemental stones");
    }
    {
        auto g2 = make_weapon(1, 244, 50, "sword");
        g2.set_mix_type(static_cast<std::uint8_t>((2 << 4) | 1));
        std::vector<openkal::server::MixItemOffer> one{extra50, chest46, stone_mystery_a};
        expect(!catalog.match(0, g2, one, 0).valid(), "G3 refuses a single mystery stone");
        std::vector<openkal::server::MixItemOffer> two{
            extra50, chest46, stone_mystery_a, stone_mystery_b};
        const auto g3 = catalog.match(0, g2, two, 0);
        expect(g3.valid() && g3.recipe_id() == "upgrade_g3" && g3.stone2_iid() == 10 &&
                   g3.extra_weapon_iid() == 2,
               "G2 + extra + two mystery stones is G3");
        std::vector<openkal::server::MixItemOffer> no_extra{
            chest46, stone_mystery_a, stone_mystery_b};
        expect(!catalog.match(0, g2, no_extra, 0).valid(), "G3 needs the extra weapon");
        std::vector<openkal::server::MixItemOffer> mixed_types{
            extra50, chest46, stone_mystery_a, stone_blood};
        expect(!catalog.match(0, g2, mixed_types, 0).valid(), "G3 needs the same stone type");
        auto g1 = make_weapon(1, 244, 50, "sword");
        g1.set_mix_type(static_cast<std::uint8_t>((1 << 4) | 1));
        expect(!catalog.match(0, g1, two, 0).valid(), "G3 refused on G1");
    }
    {
        const auto center40 = make_weapon(10, 101, 40, "sword");
        std::vector<openkal::server::MixItemOffer> outer{chest46, stone_a, stone_b};
        const auto make = catalog.match(1, center40, outer, 0);
        expect(!make.valid(), "g40 making still needs extra g40 weapon + g42 armor");
    }
    {
        const auto g1_shadow = static_cast<std::uint8_t>((1 << 4) | 1);
        const auto g2_shadow = static_cast<std::uint8_t>((2 << 4) | 1);
        auto plan = catalog.plan_roll(0, 895);
        expect(plan.allowed() && plan.target_level() == 1 && plan.new_mix_type() == g1_shadow,
               "new mix goes to level 1");
        plan = catalog.plan_roll(g1_shadow, 895);
        expect(plan.allowed() && plan.target_level() == 2 && plan.new_mix_type() == g2_shadow,
               "same stone level 2");
        plan = catalog.plan_roll(g1_shadow, 896);
        expect(plan.allowed() && plan.target_level() == 2 &&
                   plan.new_mix_type() == static_cast<int>((2 << 4) | 2),
               "G1 shadow + holy goes to G2 holy");
        plan = catalog.plan_roll(g2_shadow, 895);
        expect(!plan.allowed(), "block same stone at max");
        plan = catalog.plan_roll(0, 904);
        expect(!plan.allowed(), "mystery requires existing level 2");
        plan = catalog.plan_roll(g1_shadow, 904);
        expect(!plan.allowed(), "mystery refused on G1");
        plan = catalog.plan_roll(g2_shadow, 904);
        expect(plan.allowed() && plan.target_level() == 2 &&
                   plan.new_mix_type() == static_cast<int>((2 << 4) | 10),
               "mystery on G2 shadow");
        plan = catalog.plan_roll(g2_shadow, 905);
        expect(plan.allowed() && plan.target_level() == 2 &&
                   plan.new_mix_type() == static_cast<int>((2 << 4) | 11),
               "demon blood on G2 shadow");
    }
    {
        auto mixed = make_weapon(1, 244, 50, "sword");
        mixed.set_mix_type(static_cast<std::uint8_t>((1 << 4) | 1));
        std::vector<openkal::server::MixItemOffer> outer{extra50, chest46, stone_a, stone_b};
        const auto match = catalog.match(1, mixed, outer, 0);
        expect(match.valid() && match.recipe_id() == "make_g50",
               "making from a G1 center still matches");
        expect(catalog.plan_roll(0, 895).target_level() == 1, "making roll stays G1");
    }
    for (std::uint8_t mode : {0, 1}) {
        const auto rows = catalog.material_rows(mode);
        expect(rows.size() == (mode == 0 ? 18u : 9u),
               "material lists include all configured recipes");
        for (const auto& row : rows) {
            auto center = make_weapon(1, 244, row.center_grade, "sword");
            center.set_mix_type(row.grade3 ? 0x21 : 0);
            std::vector<openkal::server::MixItemOffer> outer{
                make_weapon(2, 245, row.extra_grade, "sword"),
                make_armor(3, 251, row.armor_grade, row.upper_only ? "upperarmor" : "helmet")};
            for (int i = 0; i < row.stone_count; ++i) {
                outer.push_back(
                    make_stone(static_cast<std::uint32_t>(4 + i), row.grade3 ? 904 : 895));
            }
            const auto match = catalog.match(mode, center, outer, 0);
            expect(match.valid() && match.recipe_id() == row.recipe_id && match.gold() == row.gold,
                   "every displayed material row passes the actual recipe matcher");
            if (row.center_grade == 50) {
                expect(row.extra_grade == 50 && row.armor_grade == 46 && row.upper_only,
                       "Grade 50 list requires an extra Grade 50 weapon and Grade 46 chest");
            }
        }
    }
    {
        MixCatalog custom;
        expect(custom.load_json_text(R"({
            "grades":[{"center":41,"extra":39,"armor":37,"slot":"upperarmor"}],
            "stones":[{"index":895,"type":1}],
            "recipes":[{"id":"custom_upgrade","mode":0,"gold":1234,
                "center":{"grade":41},"extra_weapon":{"required":true},
                "armor":{"required":true},"stones":{"count":1}}]
        })"),
               "load custom material rules");
        const auto rows = custom.material_rows(0);
        expect(rows.size() == 1 && rows[0].center_grade == 41 && rows[0].extra_grade == 39 &&
                   rows[0].armor_grade == 37 && rows[0].upper_only && rows[0].gold == 1234,
               "material lists reflect configured requirements rather than fixed UI tables");
        expect(custom.material_rows(1).empty(), "missing creation recipes produce an empty list");
    }
    return openkal::test::g_failures == 0 ? 0 : 1;
}
