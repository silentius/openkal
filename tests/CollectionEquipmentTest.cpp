/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/CollectionEquipment.h"

#include "fixture_util.h"
#include "openkal/server/Combat.h"
#include "test_util.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>

namespace {
    using namespace openkal::server;
    using openkal::test::expect;

    std::vector<CollectionItem> trigrams(int grade, int level) {
        std::vector<CollectionItem> items;
        for (int part = 0; part < 8; ++part) {
            items.push_back({static_cast<std::uint32_t>(part + 1),
                             24027 + (grade - 1) * 8 + part,
                             level,
                             0,
                             0,
                             0,
                             true});
        }
        return items;
    }

    void probability_tables() {
        CollectionUpgrades upgrades;
        expect(upgrades.load(std::string(OPENKAL_REPO_ROOT) + "/config/collection"),
               "load checked-in probability tables");
        for (int stage = 1; stage <= 20; ++stage) {
            const double chance = std::ldexp(100.0, -stage);
            expect(upgrades.chance(CollectionKind::YeoijuAttack, stage) == chance,
                   "each Yeoiju chance halves");
            expect(upgrades.succeeds(CollectionKind::YeoijuDefense, stage, chance / 200.0),
                   "fractional probabilities remain reachable");
            expect(!upgrades.succeeds(CollectionKind::YeoijuAttack, stage, chance / 100.0),
                   "upper probability boundary is exclusive");
            if (stage <= 15) {
                expect(upgrades.chance(CollectionKind::Trigram, stage) == chance,
                       "trigram table follows requested sequence");
            }
            if (stage <= 2) {
                expect(upgrades.chance(CollectionKind::Essence, stage) == chance,
                       "essence tier follows requested sequence");
            }
        }
        expect(upgrades.chance(CollectionKind::Mirror, 1) == 0 &&
                   upgrades.chance(CollectionKind::Trigram, 16) == 0,
               "unsupported upgrades cannot succeed");
        expect(!upgrades.succeeds(
                   CollectionKind::Trigram, 1, std::numeric_limits<double>::quiet_NaN()) &&
                   !upgrades.succeeds(CollectionKind::Trigram, 1, -0.1),
               "invalid draws fail closed");
        const auto dir = openkal::test::temp_dir("collection_probabilities");
        for (const auto& file : {"trigram.json", "hanin.json", "yeoiju.json"}) {
            std::filesystem::copy_file(std::string(OPENKAL_REPO_ROOT) + "/config/collection/" +
                                           file,
                                       dir + "/" + file,
                                       std::filesystem::copy_options::overwrite_existing);
        }
        {
            std::ofstream file(dir + "/hanin.json");
            file << R"({"success_percent":[101,25]})";
        }
        expect(!upgrades.load(dir) && upgrades.chance(CollectionKind::Trigram, 1) == 0,
               "invalid table disables the whole configuration atomically");
        {
            std::ofstream file(dir + "/hanin.json");
            file << R"({"success_percent":[100,0]})";
        }
        expect(upgrades.load(dir) && upgrades.succeeds(CollectionKind::Essence, 1, .999999) &&
                   !upgrades.succeeds(CollectionKind::Essence, 2, 0),
               "editable guaranteed and disabled stages");
    }

    void dependencies_and_instances() {
        auto items = trigrams(1, 5);
        CollectionItem mirror{20, 24000};
        expect(collection_wear_error(items, mirror, true).empty(),
               "complete trigrams permit mirror");
        items.pop_back();
        expect(!collection_wear_error(items, mirror, true).empty(),
               "incomplete set rejects mirror");
        items = trigrams(1, 5);
        mirror.equipped = true;
        items.push_back(mirror);
        expect(!collection_wear_error(items, items[0], false).empty(),
               "mirror blocks trigram removal");
        CollectionItem taegeuk{21, 24051};
        expect(collection_absorption_error(items, taegeuk).empty(),
               "eight plus-five trigrams permit absorption");
        expect(collection_absorption_roll(items, taegeuk, 0) == 1 &&
                   collection_absorption_roll(items, taegeuk, 7) == 8,
               "absorption chooses a bounded legacy row");
        taegeuk.absorption = 1;
        expect(!collection_absorption_error(items, taegeuk).empty(),
               "absorption cannot overwrite an existing roll");
        taegeuk.absorption = 0;
        items[0].level = 4;
        expect(!collection_absorption_error(items, taegeuk).empty(),
               "lowest trigram gates absorption");
        items = trigrams(3, 15);
        expect(!collection_wear_error(items, CollectionItem{22, 24000}, true).empty(),
               "low mirror cannot amplify high trigrams");
        expect(!collection_wear_error({}, CollectionItem{23, 24003}, true).empty(),
               "raw materials cannot equip");
        expect(!collection_wear_error({}, CollectionItem{23, 24027}, true).empty(),
               "zero-level trigram cannot equip");
        items = {{30, 24056, 0, 0, 0, 0, true}};
        for (int i = 0; i < 5; ++i) {
            CollectionItem essence{static_cast<std::uint32_t>(40 - i), 24059, 0, 100, 50};
            expect(collection_wear_error(items, essence, true).empty(),
                   "same-index stackable essence instances coexist");
            essence.equipped = true;
            items.push_back(essence);
        }
        const auto cells = collection_slots(items);
        expect(cells[11] == 36 && cells[15] == 40,
               "instance ids determine stable separate essence cells");
        expect(!collection_wear_error(items, CollectionItem{41, 24059, 0, 100, 50}, true).empty(),
               "sixth essence is rejected");
        expect(!collection_wear_error(items, items[0], false).empty(),
               "cannot remove occupied Hanin mirror");
        items.resize(1);
        items.push_back({42, 24059, 0, 112, 30, 0, true});
        expect(!collection_wear_error(items, CollectionItem{43, 24057, 0, 112, 1}, true).empty(),
               "speed cannot stack across essence grades");
        for (int i = 0; i < 5; ++i) {
            items.push_back({static_cast<std::uint32_t>(50 + i), kYeoijuAttack, 20, 0, 0, 0, true});
        }
        expect(!collection_wear_error(items, CollectionItem{60, kYeoijuAttack}, true).empty() &&
                   collection_wear_error(items, CollectionItem{61, kYeoijuDefense}, true).empty(),
               "Yeoiju has separate five-item attack and defense limits");
    }

    void combat_isolation() {
        auto items = trigrams(3, 15);
        items.push_back({20, 24002, 0, 0, 0, 0, true});
        items.push_back({21, 24053, 0, 107, 25, 169, true});
        items.push_back({22, 24056, 0, 0, 0, 0, true});
        items.push_back({23, 24059, 0, 112, 30, 0, true});
        for (int ability = 0; ability < 22; ++ability) {
            auto test = items;
            if (ability != 12) {
                test.push_back({24, 24059, 0, 100 + ability, 50, 0, true});
            }
            const auto pve = collection_bonuses(test, false);
            const auto pvp = collection_bonuses(test, true);
            expect(pve.hp > 0 && pve.attack > 0 && pve.ability[7] >= 25,
                   "legacy set and absorption are active in PvE");
            expect(pvp.hp == 0 && pvp.mp == 0 && pvp.attack == 0,
                   "flat HP MP attack never reach PvP");
            for (int stat = 0; stat < 22; ++stat) {
                expect(std::abs(pvp.ability[stat] - (stat == 12 ? 36.0 : 0.0)) < 1e-9,
                       "only amplified speed remains in PvP for every essence option");
            }
        }
        combat::Combatant attacker;
        attacker.level = 90;
        attacker.is_player = true;
        attacker.fatal_rate = 100;
        attacker.fatal_pt = 0;
        combat::Combatant target;
        target.level = 90;
        const auto normal = combat::resolve_physical(attacker, target, 0, 0, 100, 0, 0, true);
        attacker.critical_damage_percent = 20;
        const auto enhanced = combat::resolve_physical(attacker, target, 0, 0, 100, 0, 0, true);
        expect(enhanced.damage == normal.damage * 120 / 100,
               "critical modifier scales critical damage rather than fatal points");
        target.is_player = true;
        const auto pvp = combat::resolve_physical(attacker, target, 0, 0, 100, 0, 0, true);
        attacker.critical_damage_percent = 0;
        expect(pvp.damage ==
                   combat::resolve_physical(attacker, target, 0, 0, 100, 0, 0, true).damage,
               "critical modifier independently rejects PvP targets");
        attacker.upgrade_level = 10;
        attacker.weapon_min_attack = attacker.weapon_max_attack = 100;
        attacker.char_attack = 20;
        target.is_player = false;
        const auto eb = combat::get_palsy_extra(attacker, target, 1, 150);
        attacker.explosive_damage_percent = 10;
        expect(combat::get_palsy_extra(attacker, target, 1, 150) == eb * 110 / 100,
               "explosive modifier affects only explosive damage");
        target.is_player = true;
        const auto pvp_eb = combat::get_palsy_extra(attacker, target, 1, 150);
        attacker.explosive_damage_percent = 0;
        expect(combat::get_palsy_extra(attacker, target, 1, 150) == pvp_eb,
               "explosive modifier independently rejects PvP targets");
    }
}  // namespace

int main() {
    probability_tables();
    dependencies_and_instances();
    combat_isolation();
    return openkal::test::g_failures ? 1 : 0;
}
