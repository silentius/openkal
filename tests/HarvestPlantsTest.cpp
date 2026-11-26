/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/HarvestPlants.h"

#include "fixture_util.h"
#include "openkal/map/Attributes.h"
#include "openkal/map/MapCatalog.h"
#include "openkal/protocol/HarvestPlant.h"
#include "openkal/server/CastleWarRules.h"
#include "openkal/server/world/GameWorld.h"
#include "test_util.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <random>
#include <tuple>
#include <unistd.h>

namespace {
    using namespace openkal;
    using test::expect;

    void test_area_discovery(const map::AreaCell& outdoor, server::HarvestConfig config) {
        server::GameWorld world;
        expect(world.load_monsters_json_dir(
                   std::string(OPENKAL_REPO_ROOT) + "/config/monsters", 0, 0, 0),
               "load plant definition for discovery");
        config.types.front().plants_per_area = 1;
        for (const auto& [x, y] : {std::pair{0, 0},
                                   {63, 63},
                                   {30, 30},
                                   {31, 30},
                                   {32, 30},
                                   {33, 30},
                                   {34, 30},
                                   {63, 0},
                                   {0, 1},
                                   {38, 30},
                                   {39, 30}}) {
            world.grid().set_cell(x, y, outdoor);
        }
        map::MapCatalog catalog;
        expect(catalog.load_json_text(R"({"maps":[
            {"index":0,"kind":1,"xy":[0, 0]},
            {"index":255,"kind":1,"xy":[63, 63]},
            {"index":4,"kind":1,"xy":[30, 30]},
            {"index":1,"kind":1,"xy":[31, 30]},
            {"index":2,"kind":2,"xy":[31, 30],"mapname":"owned_indoor"},
            {"index":3,"kind":2,"xy":[32, 30]},
            {"index":3,"kind":1,"xy":[39, 30],"mapname":"owned_named_outdoor"},
            {"index":-1,"kind":1,"xy":[33, 30]},
            {"index":256,"kind":1,"xy":[34, 30]},
            {"index":6,"kind":1,"xy":[-1, 1]},
            {"index":7,"kind":1,"xy":[64, 0]},
            {"index":7,"kind":1,"xy":[0, -1]},
            {"index":7,"kind":1,"xy":[0, 64]},
            {"index":2,"kind":2,"xy":[-1, 64],"mapname":"owned_out_of_bounds"},
            {"index":5,"kind":1,"xy":[30, 30]},
            {"index":8,"kind":1,"xy":[38, 30]},
            {"index":9,"kind":1,"xy":[36, 30]},
            {"index":10,"kind":1,"xy":[37, 30]}
        ]})"),
               "load duplicate, named, invalid and streamed area entries");

        using Area = std::tuple<int, int, int>;
        constexpr std::uint32_t seed = 1234;
        std::mt19937 random(seed);
        std::vector<Area> candidates{
            {0, 0, 0}, {255, 63, 63}, {4, 30, 30}, {5, 30, 30}, {8, 38, 30}};
        std::shuffle(candidates.begin(), candidates.end(), random);
        std::vector<Area> expected;
        for (const auto& [index, x, y] : candidates) {
            if (std::none_of(expected.begin(), expected.end(), [&](const auto& area) {
                    return std::get<1>(area) == x && std::get<2>(area) == y;
                })) {
                expected.emplace_back(index, x, y);
            }
        }
        server::HarvestPlants plants(seed);
        std::uint32_t next = 1;
        const auto check_order = [&](const char* message) {
            std::vector<Area> actual;
            std::uint32_t id = 1;
            for (const auto& plant : world.monsters()) {
                expect(plant.id == id++, "the ordered spawns consume consecutive entity IDs");
                actual.emplace_back(
                    plant.map, plant.x / map::kWorldSize, plant.y / map::kWorldSize);
            }
            expect(actual == expected, message);
        };
        plants.update(world, catalog, config, next, 0);
        check_order(
            "shuffle receives eligible loaded records in catalog order, including duplicates");

        world.grid().set_cell(36, 30, outdoor);
        expected.emplace_back(9, 36, 30);
        plants.update(world, catalog, config, next, 0);
        check_order("a newly loaded cell spawns once after the initially loaded cells");

        catalog.clear();
        world.grid().set_cell(37, 30, outdoor);
        plants.update(world, catalog, config, next, 0);
        check_order("clearing the catalog discards its undiscovered areas");
        expect(catalog.load_json_text(R"({"maps":[{"index":10,"kind":1,"xy":[37, 30]}]})"),
               "reload an explicit outdoor area");
        expect(!catalog.load_json_text("{"), "reject malformed replacement catalog");
        plants.update(world, catalog, config, next, 0);
        check_order("a failed catalog load discards the previous entries");
        expect(catalog.load_json_text(R"({"maps":[{"index":10,"kind":1,"xy":[37, 30]}]})"),
               "restore outdoor area before a missing file load");
        const auto root = test::temp_dir(("plant_catalog_" + std::to_string(getpid())).c_str());
        expect(!catalog.load_json(root + "/missing.json"), "reject a missing replacement catalog");
        plants.update(world, catalog, config, next, 0);
        check_order("a missing catalog file discards the previous entries");
        std::filesystem::remove_all(root);
        expect(catalog.load_json_text(R"({"maps":[{"index":11,"kind":1,"xy":[37, 30]}]})"),
               "load a replacement catalog in place");
        expected.emplace_back(11, 37, 30);
        plants.update(world, catalog, config, next, 0);
        check_order("in-place catalog reload discovers the current record rather than the old one");
    }

    void test_respawn_retry(const map::AreaCell& outdoor, server::HarvestConfig config) {
        server::GameWorld world;
        expect(world.load_monsters_json_dir(
                   std::string(OPENKAL_REPO_ROOT) + "/config/monsters", 0, 0, 0),
               "load plant definition for the respawn test");
        config.types.front().plants_per_area = 1;
        config.types.front().respawn_seconds = 5;
        world.grid().set_cell(30, 30, outdoor);
        map::MapCatalog catalog;
        expect(catalog.load_json_text(R"({"maps":[{"index":0,"kind":1,"xy":[30, 30]},
                                                {"index":0,"kind":1,"xy":[31, 30]}]})"),
               "load the initial and streamed retry areas");
        server::HarvestPlants plants(1234);
        std::uint32_t next = 1;
        plants.update(world, catalog, config, next, 0);
        expect(next == 2 && world.monsters().size() == 1, "one plant is initially spawned");
        if (world.monsters().empty()) {
            return;
        }
        world.monsters().front().hp = 0;
        plants.update(world, catalog, config, next, 6);
        plants.update(world, catalog, config, next, 4.75f);
        expect(next == 2, "the removal tick does not count toward the respawn delay");

        map::AreaCell blocked = outdoor;
        blocked.ensure_blocked_mask(true);
        world.grid().set_cell(30, 30, std::move(blocked));
        plants.update(world, catalog, config, next, 0.25f);
        expect(next == 2, "placement fails at the exact respawn deadline when the cell is blocked");
        world.grid().set_cell(30, 30, outdoor);
        plants.update(world, catalog, config, next, 9.75f);
        expect(next == 2, "the failed placement waits for the full retry delay");

        world.grid().set_cell(31, 30, outdoor);
        plants.update(world, catalog, config, next, 0.25f);
        const auto* regrown = world.find_monster(2);
        const auto* added = world.find_monster(3);
        expect(next == 4 && regrown && added && regrown->x / map::kWorldSize == 30 &&
                   added->x / map::kWorldSize == 31,
               "the exact retry deadline regrows the old slot before populating a new cell");
        plants.update(world, catalog, config, next, 10);
        expect(next == 4, "further ticks do not duplicate the regrown or newly spawned plant");
    }

    void test_castle_exclusion(const map::AreaCell& outdoor, server::HarvestConfig config) {
        server::GameWorld world;
        expect(world.load_monsters_json_dir(
                   std::string(OPENKAL_REPO_ROOT) + "/config/monsters", 0, 0, 0),
               "load the plant definition for the castle test");
        config.types.front().plants_per_area = 1;
        config.types.front().respawn_seconds = 5;
        for (int x = 30; x <= 34; ++x) {
            world.grid().set_cell(x, 30, outdoor);
        }
        world.grid().cell(30, 30)->assign_attrs(map::kTileCount * map::kTileCount,
                                                map::kAttrAttackCastle);
        world.grid().cell(31, 30)->assign_attrs(map::kTileCount * map::kTileCount,
                                                map::kAttrDefenceCastle);
        map::MapCatalog catalog;
        expect(catalog.load_json_text(R"({"maps":[
            {"index":7,"kind":1,"xy":[30, 30]}, {"index":7,"kind":1,"xy":[31, 30]},
            {"index":7,"kind":1,"xy":[32, 30]}, {"index":8,"kind":1,"xy":[33, 30]},
            {"index":7,"kind":1,"xy":[34, 30]}
        ]})"),
               "load castle and nearby outdoor cells");
        server::CastleWarRules castle;
        castle.enabled = true;
        castle.attack_camp.map = 7;
        castle.bounds = {32 * map::kWorldSize,
                         30 * map::kWorldSize,
                         34 * map::kWorldSize - 1,
                         31 * map::kWorldSize - 1};
        server::HarvestPlants plants(1234);
        std::uint32_t next = 1;
        plants.update(world, catalog, config, next, 0, &castle);
        expect(next == 3, "only the two cells outside the castle can spawn plants");
        auto& monsters = world.monsters();
        auto outside = monsters.end();
        for (auto it = monsters.begin(); it != monsters.end(); ++it) {
            const int cell_x = it->x / map::kWorldSize;
            expect(cell_x != 30 && cell_x != 31, "neither castle terrain flag permits a spawn");
            expect(!castle.contains(it->map, it->x, it->y),
                   "untagged terrain inside the castle bounds cannot spawn a plant");
            if (cell_x == 34 && it->map == 7) {
                outside = it;
            }
        }
        expect(std::any_of(monsters.begin(),
                           monsters.end(),
                           [](const auto& plant) {
                               return plant.map == 8 && plant.x / map::kWorldSize == 33;
                           }),
               "castle bounds permit matching coordinates on another map");
        expect(outside != monsters.end(), "a cell outside the bounds on the castle map can spawn");
        if (outside == monsters.end()) {
            return;
        }
        monsters.erase(outside);
        plants.update(world, catalog, config, next, 0, &castle);
        const auto before = next;
        castle.bounds[2] = 35 * map::kWorldSize - 1;
        plants.update(world, catalog, config, next, 5, &castle);
        expect(next == before, "a due regrowth cannot spawn inside the configured castle bounds");
        castle.bounds[2] = 34 * map::kWorldSize - 1;
        plants.update(world, catalog, config, next, 10, &castle);
        const auto* regrown = world.find_monster(before);
        expect(next == before + 1 && regrown && regrown->map == 7 &&
                   regrown->x / map::kWorldSize == 34,
               "regrowth can retry once its cell is outside the castle bounds");
    }
}  // namespace

int main() {
    using namespace openkal;
    using test::expect;
    const auto settings =
        server::load_harvest_config(std::string(OPENKAL_REPO_ROOT) + "/config/server/plants.json");
    expect(settings.has_value(), "owned server settings are valid");
    expect(settings && settings->types.size() == 3, "all three owned ingredients are configured");
    if (settings) {
        for (const auto& identity :
             {std::pair{protocol::kAloePlantIndex, protocol::kAloeItemIndex},
              std::pair{protocol::kBlueLotusPlantIndex, protocol::kBlueLotusItemIndex},
              std::pair{protocol::kGinsengPlantIndex, protocol::kGinsengItemIndex}}) {
            const auto* type = settings->find_type(identity.first);
            const bool isGinseng = identity.first == protocol::kGinsengPlantIndex;
            expect(type && type->item_index == identity.second && type->enabled &&
                       type->items_per_pick == (isGinseng ? 1 : 4) && type->max_plants == 256 &&
                       type->plants_per_area == (isGinseng ? 4 : 8) && type->respawn_seconds == 600,
                   "owned herbs retain their configured harvest amounts, population and regrowth");
        }
    }
    const auto parse = [](const std::string& entry) {
        return server::parse_harvest_config(platform::Json::parse(
            R"({"types":{"aloe_vera":{"entity_index":65000,"item_index":23027,"enabled":true)" +
            entry + "}}}"));
    };
    const auto defaults = parse("");
    expect(defaults && defaults->types.size() == 1 && defaults->types.front().max_plants == 256 &&
               defaults->types.front().plants_per_area == 8 &&
               defaults->types.front().respawn_seconds == 600 &&
               defaults->types.front().items_per_pick == 1,
           "each type retains the rare population, ten-minute regrowth and one-item defaults");
    expect(!parse(R"(,"max_plants":-1)"), "reject negative population");
    expect(!parse(R"(,"plants_per_area":2.5)"), "reject fractional population");
    expect(!parse(R"(,"respawn_seconds":0)"), "reject immediate regrowth");
    for (const auto& amount : {"0", "-1", "1.5", "1001", "true"}) {
        expect(!parse(std::string(",\"items_per_pick\":") + amount),
               "reject invalid harvest amounts");
    }
    expect(
        !server::parse_harvest_config(platform::Json::parse(
            R"({"types":{"first":{"entity_index":65000,"item_index":23027,"enabled":true},"second":{"entity_index":65000,"item_index":23027,"enabled":true}}})")),
        "two plant types cannot share an entity definition");
    expect(!server::parse_harvest_config(platform::Json::parse(
               R"({"types":{"missing_entity":{"enabled":true,"item_index":23027}}})")),
           "plant identity must be explicit");
    expect(!server::parse_harvest_config(platform::Json::parse(R"({"enabled":true})")),
           "flat settings must be migrated into plant types");
    server::GameWorld world;
    expect(
        world.load_monsters_json_dir(std::string(OPENKAL_REPO_ROOT) + "/config/monsters", 0, 0, 0),
        "load owned plant definition");
    map::AreaCell outdoor;
    // Outdoor hunting ground can carry Safety without being a Village.
    outdoor.assign_attrs(map::kTileCount * map::kTileCount, map::kAttrSafety);
    if (defaults) {
        test_area_discovery(outdoor, *defaults);
        test_respawn_retry(outdoor, *defaults);
        test_castle_exclusion(outdoor, *defaults);
    }
    world.grid().set_cell(30, 30, outdoor);
    world.grid().set_cell(31, 30, outdoor);
    world.grid().set_cell(32, 30, outdoor);
    world.grid().set_cell(33, 30, outdoor);
    auto* city = world.grid().cell(31, 30);
    city->assign_attrs(map::kTileCount * map::kTileCount, map::kAttrVillage);
    auto* blocked = world.grid().cell(32, 30);
    blocked->ensure_blocked_mask(true);
    map::MapCatalog catalog;
    expect(catalog.load_json_text(R"({"maps":[{"index":0,"kind":1,"xy":[30, 30]},
        {"index":0,"kind":1,"xy":[31, 30]}, {"index":0,"kind":1,"xy":[32, 30]},
        {"index":1,"kind":2,"xy":[33, 30],"mapname":"synthetic_dungeon"}]})"),
           "explicit map kinds");
    auto* ground = world.grid().cell(30, 30);
    if (settings) {
        server::GameWorld herbs;
        expect(herbs.load_monsters_json_dir(
                   std::string(OPENKAL_REPO_ROOT) + "/config/monsters", 0, 0, 0),
               "load every shipped gathering definition");
        herbs.set_harvest_plant_definitions({protocol::kAloePlantIndex,
                                             protocol::kBlueLotusPlantIndex,
                                             protocol::kGinsengPlantIndex});
        herbs.grid().set_cell(30, 30, outdoor);
        server::HarvestPlants population(5678);
        std::uint32_t next_id = 1;
        for (int tick = 0; tick < 12; ++tick) {
            population.update(herbs, catalog, *settings, next_id, 0);
        }
        for (const auto& type : settings->types) {
            expect(std::count_if(herbs.monsters().begin(),
                                 herbs.monsters().end(),
                                 [&](const auto& node) {
                                     return node.def_index == type.entity_index;
                                 }) == type.plants_per_area,
                   "each shipped herb populates the same eligible area independently");
        }
    }
    const int sample_x = 30 * map::kWorldSize + 100 * map::kTileSize + 16;
    const int sample_y = 30 * map::kWorldSize + 100 * map::kTileSize + 16;
    for (const auto flag : {map::kAttrVillage,
                            map::kAttrPortal,
                            map::kAttrObject,
                            map::kAttrAttackCastle,
                            map::kAttrDefenceCastle}) {
        ground->attr_ref(100, 100) = flag;
        expect(!server::HarvestPlants::valid_position(world, sample_x, sample_y),
               "exclude protected terrain attributes");
    }
    ground->attr_ref(100, 100) = map::kAttrSafety;
    expect(server::HarvestPlants::valid_position(world, sample_x, sample_y),
           "Safety outdoor ground remains eligible outside a Village");
    ground->set_not_moveable_at_subcell(402, 402, true);
    expect(!server::HarvestPlants::valid_position(world, sample_x, sample_y),
           "exclude not-moveable subcells");
    ground->set_not_moveable_at_subcell(402, 402, false);
    ground->set_blocked_at_subcell(401, 401, true);
    expect(!server::HarvestPlants::valid_position(world, sample_x, sample_y),
           "check blocked subcells throughout the footprint");
    ground->set_blocked_at_subcell(401, 401, false);
    server::HarvestPlants plants(1234);
    server::HarvestConfig config;
    config.types = defaults->types;
    config.types.front().enabled = false;
    world.set_harvest_plant_definitions({protocol::kAloePlantIndex});
    config.types.front().max_plants = 3;
    config.types.front().plants_per_area = 8;
    std::uint32_t next = 1;
    plants.update(world, catalog, config, next, 1);
    expect(world.monsters().empty(), "disabled gathering spawns nothing");
    config.types.front().enabled = true;
    for (int i = 0; i < 8; ++i) {
        plants.update(world, catalog, config, next, 0);
    }
    expect(world.monsters().size() == 3, "world cap takes precedence over area limit");
    for (const auto& plant : world.monsters()) {
        expect(plant.def_index == protocol::kAloePlantIndex && plant.map == 0,
               "owned outdoor gathering entity");
        expect(plant.x / map::kWorldSize == 30 && plant.y / map::kWorldSize == 30,
               "no city, blocked or dungeon plants");
        expect(server::HarvestPlants::valid_position(world, plant.x, plant.y),
               "walkable gathering footprint");
    }
    if (world.monsters().empty()) {
        return 1;
    }
    const auto plant = world.monsters().front();
    expect(world.begin_open_chest(10, plant.id, protocol::kHarvestSeconds) == 0, "claim plant");
    expect(world.begin_open_chest(11, plant.id, protocol::kHarvestSeconds) == 2,
           "second player cannot claim it");
    expect(world.cancel_open_chest(10, plant.id), "movement can cancel harvest");
    expect(world.begin_open_chest(11, plant.id, protocol::kHarvestSeconds) == 0,
           "canceled plant remains available");
    std::vector<server::MonsterMoveEvent> moves;
    std::vector<server::MonsterAttackEvent> attacks;
    std::vector<server::ChestOpenEvent> opened;
    world.tick(2, {}, moves, attacks, opened);
    expect(opened.size() == 1 && opened.front().player_id == 11, "one completed harvest event");
    expect(world.find_monster(plant.id)->hp > 0,
           "plant stays alive until the inventory reward is committed");
    auto& entities = world.monsters();
    entities.erase(std::remove_if(entities.begin(),
                                  entities.end(),
                                  [&](const auto& entity) { return entity.id == plant.id; }),
                   entities.end());
    plants.update(world, catalog, config, next, 0);
    const auto before = next;
    plants.update(world, catalog, config, next, 599);
    expect(next == before, "no regrowth before ten minutes");
    plants.update(world, catalog, config, next, 1);
    expect(next == before + 1, "one plant regrows after ten minutes");
    plants.update(world, catalog, config, next, 1);
    expect(next == before + 1, "regrowth cannot duplicate a living plant");
    {
        const auto root = test::temp_dir(("plant_types_" + std::to_string(getpid())).c_str());
        for (const int index : {65000, 65001}) {
            std::ofstream(root + "/" + std::to_string(index) + ".json")
                << "{\"index\":" << index
                << R"(,"name":"Owned test herb","stats":{"race":4,"hp":1},"spawns":[]})";
        }
        server::GameWorld mixed_world;
        expect(mixed_world.load_monsters_json_dir(root, 0, 0, 0), "load two owned test herbs");
        mixed_world.set_harvest_plant_definitions({65000, 65001});
        mixed_world.grid().set_cell(30, 30, outdoor);
        map::MapCatalog mixed_catalog;
        expect(mixed_catalog.load_json_text(R"({"maps":[{"index":0,"kind":1,"xy":[30, 30]}]})"),
               "shared outdoor area for multiple types");
        const auto mixed_config = server::parse_harvest_config(platform::Json::parse(
            R"({"types":{
                "aloe_vera":{"entity_index":65000,"item_index":23027,"items_per_pick":3,"enabled":true,"max_plants":2,"plants_per_area":2,"respawn_seconds":1},
                "test_herb":{"entity_index":65001,"item_index":9991,"items_per_pick":5,"enabled":true,"max_plants":3,"plants_per_area":3,"respawn_seconds":10},
                "disabled_herb":{"entity_index":65002,"item_index":9992,"enabled":false}
            }})"));
        expect(mixed_config && mixed_config->find_type(65001) &&
                   mixed_config->find_type(65001)->items_per_pick == 5,
               "type identity resolves its own ingredient amount");
        if (!mixed_config) {
            return 1;
        }
        server::HarvestPlants mixed_plants(1234);
        std::uint32_t mixed_next = 1;
        for (int i = 0; i < 4; ++i) {
            mixed_plants.update(mixed_world, mixed_catalog, *mixed_config, mixed_next, 0);
        }
        const auto count = [&](int index) {
            return std::count_if(mixed_world.monsters().begin(),
                                 mixed_world.monsters().end(),
                                 [&](const auto& node) { return node.def_index == index; });
        };
        expect(count(65000) == 2 && count(65001) == 3 && count(65002) == 0,
               "types have independent population limits and enable switches");
        mixed_world.monsters().clear();
        mixed_plants.update(mixed_world, mixed_catalog, *mixed_config, mixed_next, 0);
        mixed_plants.update(mixed_world, mixed_catalog, *mixed_config, mixed_next, 1);
        expect(count(65000) == 2 && count(65001) == 0,
               "only the type with the short timer regrows after one second");
        mixed_plants.update(mixed_world, mixed_catalog, *mixed_config, mixed_next, 8);
        expect(count(65001) == 0, "second type retains its longer timer");
        mixed_plants.update(mixed_world, mixed_catalog, *mixed_config, mixed_next, 1);
        expect(count(65000) == 2 && count(65001) == 3,
               "second type regrows without duplicating the first type");
        std::filesystem::remove_all(root);
    }
    {
        server::GameWorld large_world;
        expect(large_world.load_monsters_json_dir(
                   std::string(OPENKAL_REPO_ROOT) + "/config/monsters", 0, 0, 0),
               "load large gathering world's plant definition");
        std::string maps = R"({"maps":[)";
        for (int cy = 10; cy < 16; ++cy) {
            for (int cx = 10; cx < 16; ++cx) {
                if (cx != 10 || cy != 10) {
                    maps += ',';
                }
                maps += R"({"index":0,"kind":1,"xy":[)" + std::to_string(cx) + ", " +
                        std::to_string(cy) + "]}";
                large_world.grid().set_cell(cx, cy, outdoor);
            }
        }
        maps += "]}";
        map::MapCatalog large_catalog;
        expect(large_catalog.load_json_text(maps), "load many outdoor gathering areas");
        server::HarvestConfig large_config;
        large_config.types = defaults->types;
        large_config.types.front().enabled = true;
        large_config.types.front().max_plants = 2560;
        large_config.types.front().plants_per_area = 80;
        large_config.types.front().respawn_seconds = 1;
        server::HarvestPlants population(1234);
        std::uint32_t large_next = 1;
        population.update(large_world, large_catalog, large_config, large_next, 0);
        expect(!large_world.monsters().empty() && large_world.monsters().size() <= 8,
               "large initial population leaves work for later network ticks");
        for (int tick = 0; tick < 400; ++tick) {
            const auto count = large_world.monsters().size();
            population.update(large_world, large_catalog, large_config, large_next, 0);
            expect(large_world.monsters().size() <= count + 8,
                   "each population update has bounded placement work");
        }
        expect(large_world.monsters().size() == 2560,
               "incremental generation reaches the configured world cap");
        for (int cy = 10; cy < 16; ++cy) {
            for (int cx = 10; cx < 16; ++cx) {
                expect(std::count_if(large_world.monsters().begin(),
                                     large_world.monsters().end(),
                                     [&](const auto& node) {
                                         return node.x / map::kWorldSize == cx &&
                                                node.y / map::kWorldSize == cy;
                                     }) <= 80,
                       "pending areas are populated only once across updates");
            }
        }
        large_world.monsters().clear();
        population.update(large_world, large_catalog, large_config, large_next, 0);
        expect(large_world.monsters().empty(), "mass harvest starts the regrowth delay");
        population.update(large_world, large_catalog, large_config, large_next, 1);
        expect(!large_world.monsters().empty() && large_world.monsters().size() <= 8,
               "simultaneous regrowth also leaves time for network processing");
        for (int tick = 0; tick < 400; ++tick) {
            population.update(large_world, large_catalog, large_config, large_next, 0);
        }
        expect(large_world.monsters().size() == 2560,
               "deferred regrowth restores every maintained slot without duplicates");
    }
    return test::g_failures ? 1 : 0;
}
