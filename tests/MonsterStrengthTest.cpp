/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/MonsterStrength.h"

#include "fixture_util.h"
#include "openkal/platform/Json.h"
#include "openkal/server/world/GameWorld.h"
#include "test_util.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>

namespace {
    bool all_zero(const openkal::platform::Json& value) {
        if (value.is_number()) {
            return value.number() == 0.0;
        }
        if (value.is_object()) {
            for (const auto& entry : value.object()) {
                if (!all_zero(entry.second)) {
                    return false;
                }
            }
            return true;
        }
        if (value.is_array()) {
            for (const auto& entry : value.array()) {
                if (!all_zero(entry)) {
                    return false;
                }
            }
            return true;
        }
        return false;
    }

    bool write(const std::filesystem::path& path, const std::string& text) {
        std::ofstream out(path);
        out << text;
        return static_cast<bool>(out);
    }

    void test_definition_lookup(const std::filesystem::path& root) {
        using openkal::test::expect;
        const auto initial = root / "lookup-initial";
        const auto replacement = root / "lookup-replacement";
        const auto invalid = root / "lookup-invalid";
        std::filesystem::create_directories(initial);
        std::filesystem::create_directories(replacement);
        std::filesystem::create_directories(invalid);
        expect(write(initial / "1.json", R"({"index":7,"name":"Duplicate A"})") &&
                   write(initial / "2.json", R"({"index":2147483647,"name":"Sparse ID"})") &&
                   write(initial / "3.json", R"({"index":7,"name":"Duplicate B"})") &&
                   write(initial / "4.json", R"({"index":65000,"name":"Removed on reload"})") &&
                   write(replacement / "1.json", R"({"index":7,"name":"Replacement"})") &&
                   write(invalid / "1.json", R"({"index":0,"name":"Invalid ID"})"),
               "write the initial, replacement and invalid definition catalogs");

        openkal::server::GameWorld world;
        expect(world.find_def(7) == nullptr, "an unloaded catalog has no definition for an ID");
        expect(world.load_monsters_json_dir(initial.string(), 0, 0, 0),
               "load sparse and duplicate definition IDs");
        const auto& definitions = world.defs();
        const auto first = std::find_if(
            definitions.begin(), definitions.end(), [](const auto& def) { return def.index == 7; });
        expect(definitions.size() == 4 &&
                   std::count_if(definitions.begin(),
                                 definitions.end(),
                                 [](const auto& def) { return def.index == 7; }) == 2 &&
                   first != definitions.end() && world.find_def(7) == &*first,
               "duplicate records stay in catalog order and lookup returns the first one");
        const auto* sparse = world.find_def(std::numeric_limits<int>::max());
        expect(sparse && sparse->name == "Sparse ID" && world.find_def(8) == nullptr &&
                   world.find_def(-1) == nullptr,
               "lookup accepts the largest valid ID and returns no match for absent IDs");

        const auto first_name = first == definitions.end() ? std::string{} : first->name;
        expect(!world.load_monsters_json_dir(
                   replacement.string(), 0, 0, 0, (root / "missing-strength.json").string()) &&
                   world.find_def(7) && world.find_def(7)->name == first_name &&
                   world.find_def(65000),
               "failure to load strength settings preserves the previous catalog");
        expect(world.load_monsters_json_dir(replacement.string(), 0, 0, 0) &&
                   definitions.size() == 1 && world.find_def(7) == &definitions.front() &&
                   definitions.front().name == "Replacement" && world.find_def(65000) == nullptr &&
                   world.find_def(std::numeric_limits<int>::max()) == nullptr,
               "reloading replaces retained IDs and removes IDs omitted from the new catalog");
        expect(!world.load_monsters_json_dir((root / "missing-monsters").string(), 0, 0, 0) &&
                   definitions.empty() && world.find_def(7) == nullptr,
               "a missing catalog directory clears the previous definitions");
        expect(world.load_monsters_json_dir(replacement.string(), 0, 0, 0) && world.find_def(7) &&
                   !world.load_monsters_json_dir(invalid.string(), 0, 0, 0) &&
                   definitions.empty() && world.find_def(7) == nullptr &&
                   world.find_def(0) == nullptr,
               "a directory without valid records clears the previous definitions");
    }
}  // namespace

int main() {
    using openkal::server::GameWorld;
    using openkal::server::MonsterStrength;
    using openkal::test::expect;
    const auto dir = std::filesystem::temp_directory_path() /
                     ("openkal_monster_strength_" +
                      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(dir / "monsters");
    const auto path = dir / "strength.json";
    MonsterStrength strength;
    expect(strength.adjust(10, 15, "hth") == 10, "default configuration is neutral");
    expect(
        strength.load_file(std::string(OPENKAL_REPO_ROOT) + "/config/server/monster_strength.json"),
        "load shipped neutral template");
    for (int level = 1; level <= 255; ++level) {
        expect(strength.adjust(123, level, "dex") == 123, "neutral template preserves every level");
    }
    // Also validate the authored template's completeness rather than only its behavior.
    std::ifstream input(std::string(OPENKAL_REPO_ROOT) + "/config/server/monster_strength.json");
    const std::string text((std::istreambuf_iterator<char>(input)),
                           std::istreambuf_iterator<char>());
    const auto neutral = openkal::platform::Json::parse(text);
    expect(neutral.at("levels").object().size() == 255, "template contains levels 1 through 255");
    for (const auto& level : neutral.at("levels").object()) {
        expect(level.second.at("stats").object().size() == 9 &&
                   level.second.at("percent").number() == 0.0 && all_zero(level.second.at("stats")),
               "each level contains all stat options and a neutral percentage");
    }

    const std::string modifiers = R"({"schema":"openkal.monster_strength/v1",
        "globalPercent":10.0,"levels":{"15":{"percent":10.0,"stats":{"hth":-1,"dex":-1}}}})";
    expect(write(path, modifiers) && strength.load_file(path.string()),
           "load percentage and offsets");
    expect(strength.adjust(10, 15, "hth") == 11 && strength.adjust(20, 15, "dex") == 23,
           "requested level-15 example produces hth 11 and dex 23");
    expect(strength.adjust(10, 14, "hth") == 11, "other levels use only the global percentage");
    expect(strength.adjust(4, 14, "str") == 4 && strength.adjust(5, 14, "str") == 6,
           "round .4 down and .5 up");
    expect(strength.adjust(14, 15, "str") == 17, "round each percentage step separately");
    for (const char* stat :
         {"sight", "mspeed", "range", "attack", "magic", "exp", "resist", "aspeed", "defense"}) {
        expect(strength.adjust(100, 15, stat) == 100,
               "global and level percentages leave protected stats unchanged");
        for (const auto& level : neutral.at("levels").object()) {
            expect(level.second.at("stats").get(stat) == nullptr,
                   "neutral template omits protected stats at every level");
        }
    }
    for (const char* stat :
         {"sight", "mspeed", "range", "attack", "magic", "exp", "resist", "aspeed", "defense"}) {
        const std::string bad = std::string(R"({"schema":"openkal.monster_strength/v1",
            "globalPercent":0,"levels":{"15":{"stats":{")") +
                                stat + R"(":0}}}})";
        expect(write(path, bad) && !strength.load_file(path.string()),
               "configuration rejects protected stat overrides");
    }
    expect(write(path,
                 R"({"schema":"openkal.monster_strength/v1","globalPercent":-0.1,"levels":{}})") &&
               strength.load_file(path.string()) && strength.adjust(500, 15, "str") == 500,
           "decimal negative percentage preserves mathematical half-up rounding");
    expect(write(path,
                 R"({"schema":"openkal.monster_strength/v1","globalPercent":0.5,"levels":{}})") &&
               strength.load_file(path.string()) && strength.adjust(100, 15, "str") == 101,
           "fractional percentage rounds exact decimal halves up");
    expect(write(path, R"({"schema":"openkal.monster_strength/v1","globalPercent":-50,"levels":{
        "15":{"stats":{"hp":-1000,"hit":-1,"dodge":2}}}})") &&
               strength.load_file(path.string()),
           "load reductions and stat offsets");
    expect(strength.adjust(10, 15, "hp") == 0 && strength.adjust(10, 15, "hit") == 4 &&
               strength.adjust(10, 15, "dodge") == 7,
           "offsets are applied last with a nonnegative stat floor");
    expect(write(path,
                 R"({"schema":"openkal.monster_strength/v1","globalPercent":1e308,"levels":{}})") &&
               strength.load_file(path.string()) &&
               strength.adjust(100, 15, "str") == std::numeric_limits<int>::max() &&
               strength.adjust(0, 15, "str") == 0,
           "extreme finite percentages saturate without overflowing integer conversion");
    for (
        const char* bad :
        {R"({"globalPercent":0,"levels":{}})",
         R"({"schema":"openkal.monster_strength/v1","globalPercent":-101,"levels":{}})",
         R"({"schema":"openkal.monster_strength/v1","globalPercent":"10","levels":{}})",
         R"({"schema":"openkal.monster_strength/v1","globalPercent":1e999,"levels":{}})",
         R"({"schema":"openkal.monster_strength/v1","globalPercent":0,"levels":{"0":{}}})",
         R"({"schema":"openkal.monster_strength/v1","globalPercent":0,"levels":{"01":{}}})",
         R"({"schema":"openkal.monster_strength/v1","globalPercent":0,"levels":{"15":{"percent":-101}}})",
         R"({"schema":"openkal.monster_strength/v1","globalPercent":0,"levels":{"15":{"stats":{"race":1}}}})",
         R"({"schema":"openkal.monster_strength/v1","globalPercent":0,"levels":{"15":{"stats":{"dex":1.5}}}})",
         R"({"schema":"openkal.monster_strength/v1","globalPercent":0,"levels":{"15":{"stats":{"hp":2147483648}}}})",
         R"({"schema":"openkal.monster_strength/v1","globalPercent":0,"levels":{"15":{"stats":{"hit":[1]}}}})",
         R"({"schema":"openkal.monster_strength/v1","globalPercent":0,"levels":{"15":{"stats":{"attack":{"type":1}}}}})",
         R"({"schema":"openkal.monster_strength/v1","globalPercent":0,"levels":{"15":{"precent":10}}})"}) {
        expect(write(path, bad) && !strength.load_file(path.string()) &&
                   strength.adjust(10, 15, "hth") == 10,
               "reject malformed modifiers and discard previous configuration");
    }
    expect(!strength.load_file((dir / "missing.json").string()), "explicit missing config fails");

    const auto monster = dir / "monsters/1.json";
    expect(write(monster, R"({"index":1,"name":"Strength fixture","stats":{
        "level":15,"race":3,"ai":2,"str":10,"hth":10,"int":10,"wis":10,"dex":20,
        "hp":100,"exp":100,"hit":10,"dodge":10,"absorb":10,"range":100,"aspeed":1000,
        "attack":{"type":1,"min":10,"max":20},"magic":[10,20],"defense":[10,20],
        "resist":[10,20,30,40,50],"sight":[100,200],"mspeed":[1000,500],
        "skill":[[3,2]],"itemGroups":[[7,1]],"quests":[[9,1,7,1]]},
        "spawns":[{"map":0,"area":1,"max":1,"cycle":1,"rect":[10,10,20,20]}]})"),
           "write authored monster fixture");
    GameWorld baseline;
    GameWorld adjusted;
    GameWorld expected;
    expect(baseline.load_monsters_json_dir((dir / "monsters").string(), 0, 0, 0),
           "load unmodified monster fixture");
    expect(write(path, modifiers) &&
               adjusted.load_monsters_json_dir((dir / "monsters").string(), 0, 0, 0, path.string()),
           "load modified monster fixture");
    expect(write(monster, R"({"index":1,"name":"Strength fixture","stats":{
        "level":15,"race":3,"ai":2,"str":12,"hth":11,"int":12,"wis":12,"dex":23,
        "hp":121,"exp":100,"hit":12,"dodge":12,"absorb":12,"range":100,"aspeed":1000,
        "attack":[1,10,20],"magic":[10,20],"defense":[10,20],
        "resist":[10,20,30,40,50],"sight":[100,200],"mspeed":[1000,500]}})") &&
               expected.load_monsters_json_dir((dir / "monsters").string(), 0, 0, 0),
           "load independently precomputed stat fixture");
    const auto* actual = adjusted.find_def(1);
    const auto* want = expected.find_def(1);
    expect(actual && want, "both worlds contain the monster definition");
    if (actual && want) {
        expect(actual->max_hp == want->max_hp && actual->hit == want->hit &&
                   actual->dodge == want->dodge && actual->min_attack == want->min_attack &&
                   actual->max_attack == want->max_attack && actual->min_magic == want->min_magic &&
                   actual->max_magic == want->max_magic && actual->strength == want->strength &&
                   actual->intelligence == want->intelligence,
               "adjust primary stats before deriving HP, attacks, accuracy and evasion");
        expect(actual->exp == 100 && actual->absorb == 12 && actual->defense_melee == 10 &&
                   actual->defense_range == 20 && actual->res_fire == want->res_fire &&
                   actual->res_ice == want->res_ice &&
                   actual->res_lightning == want->res_lightning &&
                   actual->res_curse == want->res_curse && actual->res_palsy == want->res_palsy &&
                   actual->attack_range == 100 && actual->detect_range == 100 &&
                   actual->sight_range == 200 && actual->attack_speed_ms == 1000 &&
                   actual->roam_speed_ms == 1000 && actual->chase_speed_ms == 500 &&
                   actual->speed == want->speed,
               "adjust supported stats while preserving protected authored values");
        expect(actual->index == 1 && actual->level == 15 && actual->race == 3 && actual->ai == 2 &&
                   actual->attack_type == 1 && actual->skills.size() == 1 &&
                   actual->skills.front().index == 3 && actual->skills.front().level == 2 &&
                   actual->item_groups == baseline.find_def(1)->item_groups &&
                   actual->quest_drops.size() == 1 &&
                   actual->quest_drops.front().quest_index == 9 &&
                   actual->quest_drops.front().itemgroup == 7 &&
                   adjusted.find_spawn_by_area(1)->count == 1,
               "preserve identity, level, AI, skill, drop and spawn metadata");
        const auto maps = dir / "maps";
        std::filesystem::create_directories(maps);
        expect(openkal::test::write_open_area((maps / "n_000_000.akarea").string()) &&
                   adjusted.load_maps_around(maps.string(), 480, 480, 0),
               "load synthetic map for modified monster spawns");
        std::uint32_t next_id = 100;
        const auto first_spawn = adjusted.spawn_monster_at(next_id, 1, 0, 480, 480, 0);
        expect(first_spawn.has_value(), "spawn monster from modified definition");
        if (first_spawn) {
            const auto* entity = adjusted.find_monster(*first_spawn);
            expect(entity && entity->hp == actual->max_hp && entity->max_hp == actual->max_hp &&
                       entity->min_attack == actual->min_attack &&
                       entity->max_attack == actual->max_attack && entity->attack_range == 100 &&
                       entity->detect_range == 100,
                   "live monster inherits adjusted HP and derived attack with ranges");
        }
        const auto hp = actual->max_hp;
        std::filesystem::remove(path);
        expect(adjusted.find_def(1)->max_hp == hp,
               "loaded values do not depend on the file at runtime");
        adjusted.monsters().clear();
        const auto later_spawn = adjusted.spawn_monster_at(next_id, 1, 0, 480, 480, 0);
        expect(later_spawn && adjusted.find_monster(*later_spawn)->max_hp == hp,
               "later spawns reuse startup modifiers without applying them again");
    }
    expect(adjusted.load_monsters_json_dir((dir / "monsters").string(),
                                           0,
                                           0,
                                           0,
                                           std::string(OPENKAL_REPO_ROOT) +
                                               "/config/server/monster_strength.json") &&
               adjusted.find_def(1)->max_hp == expected.find_def(1)->max_hp,
           "reload derives from authored stats without compounding previous modifiers");
    expect(write(path, "{}") && !adjusted.load_monsters_json_dir(
                                    (dir / "monsters").string(), 0, 0, 0, path.string()),
           "monster loading propagates configuration failure");
    test_definition_lookup(dir);
    std::filesystem::remove_all(dir);
    return openkal::test::g_failures == 0 ? 0 : 1;
}
