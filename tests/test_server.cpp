/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "fixture_util.h"
#include "openkal/map/Constants.h"
#include "openkal/map/Coords.h"
#include "openkal/map/MapCatalog.h"
#include "openkal/platform/Path.h"
#include "openkal/platform/Profile.h"
#include "openkal/protocol/MonsterRace.h"
#include "openkal/server/ServerHost.h"
#include "openkal/server/store/DataStore.h"
#include "openkal/server/world/DropRoller.h"
#include "openkal/server/world/GameWorld.h"
#include "openkal/server/world/ItemGroupDb.h"
#include "openkal/server/world/StarterKit.h"
#include "test_util.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <thread>
#include <unistd.h>
#include <unordered_map>

namespace {

    bool write_test_monsters_json(const std::string& dir,
                                  int tile_x0,
                                  int tile_y0,
                                  int tile_x1,
                                  int tile_y1,
                                  bool include_far_spawn = false) {
        openkal::test::mkdir_p(dir);
        std::ofstream out(openkal::platform::join_path(dir, "0001.json"));
        if (!out) {
            return false;
        }
        out << R"({
  "index": 1,
  "name": "TestMob",
  "stats": { "race": 0, "level": 1, "hp": 100, "attack": { "min": 1, "max": 1 } },
  "spawns": [
    { "map": 0, "max": 6, "cycle": 1, "rect": [)"
            << tile_x0 << ", " << tile_y0 << ", " << tile_x1 << ", " << tile_y1 << "]}";
        if (include_far_spawn) {
            out << ", { \"map\": 0, \"max\": 5, \"cycle\": 1, \"rect\": [" << tile_x0 + 360 << ", "
                << tile_y0 + 360 << ", " << tile_x0 + 380 << ", " << tile_y0 + 380 << "]}";
        }
        out << R"(]
})";
        return static_cast<bool>(out);
    }

}  // namespace

int main() {
    using openkal::server::DropRoller;
    using openkal::server::kMonsterDropScatterMax;
    using openkal::server::kMonsterDropScatterMin;
    using openkal::server::kPlayerDropScatterMax;
    using openkal::server::kPlayerDropScatterMin;
    using openkal::test::expect;

    {
        constexpr int kOriginX = 1000;
        constexpr int kOriginY = 2000;
        for (int i = 0; i < 64; ++i) {
            const auto pos = DropRoller::scatter_drop_point(kOriginX, kOriginY);
            const int ox = pos.first - kOriginX;
            const int oy = pos.second - kOriginY;
            expect(std::abs(ox) >= kMonsterDropScatterMin && std::abs(ox) <= kMonsterDropScatterMax,
                   "monster drop |ox| in 8..16");
            expect(std::abs(oy) >= kMonsterDropScatterMin && std::abs(oy) <= kMonsterDropScatterMax,
                   "monster drop |oy| in 8..16");
        }
        for (int i = 0; i < 64; ++i) {
            const auto pos = DropRoller::scatter_drop_point(
                kOriginX, kOriginY, kPlayerDropScatterMax, kPlayerDropScatterMin);
            const int ox = pos.first - kOriginX;
            const int oy = pos.second - kOriginY;
            expect(std::abs(ox) >= kPlayerDropScatterMin && std::abs(ox) <= kPlayerDropScatterMax,
                   "player drop |ox| in 4..8");
            expect(std::abs(oy) >= kPlayerDropScatterMin && std::abs(oy) <= kPlayerDropScatterMax,
                   "player drop |oy| in 4..8");
        }
    }

    {
        using openkal::protocol::MonsterRace;
        using openkal::server::DropRoller;
        using openkal::server::GroupDef;
        using openkal::server::GroupEntry;
        using openkal::server::ItemGroupDb;
        using openkal::server::ItemGroupDef;
        using openkal::server::ItemGroupEntry;
        using openkal::server::kGlobalNormalMonsterItemGroupIndex;

        const auto maguni_only = DropRoller::effective_monster_kill_item_groups(
            {}, static_cast<int>(MonsterRace::Maguni));
        expect(maguni_only.size() == 1 &&
                   maguni_only[0].first == kGlobalNormalMonsterItemGroupIndex &&
                   maguni_only[0].second == 1,
               "maguni gets reserved itemgroup once");

        const auto chest_only = DropRoller::effective_monster_kill_item_groups(
            {{12, 2}}, static_cast<int>(MonsterRace::Item));
        expect(chest_only.size() == 1 && chest_only[0].first == 12 && chest_only[0].second == 2,
               "chest keeps own groups without reserved");

        const auto flower_only = DropRoller::effective_monster_kill_item_groups(
            {{7, 1}}, static_cast<int>(MonsterRace::FireFlower));
        expect(flower_only.size() == 1 && flower_only[0].first == 7,
               "fire flower skips reserved itemgroup");

        const auto stripped = DropRoller::effective_monster_kill_item_groups(
            {{3, 1}, {kGlobalNormalMonsterItemGroupIndex, 5}},
            static_cast<int>(MonsterRace::NotMaguni));
        expect(stripped.size() == 2 && stripped[0].first == 3 && stripped[0].second == 1 &&
                   stripped[1].first == kGlobalNormalMonsterItemGroupIndex &&
                   stripped[1].second == 1,
               "reserved index is stripped then appended once");

        ItemGroupDb db;
        GroupDef group;
        group.index = 9001;
        GroupEntry money;
        money.kind = GroupEntry::Kind::Money;
        money.chance = 100.0f;
        money.amount = 42;
        group.entries.push_back(money);
        db.upsert_group(std::move(group));
        ItemGroupDef ig;
        ig.index = kGlobalNormalMonsterItemGroupIndex;
        ItemGroupEntry row;
        row.chance = 100.0f;
        row.group_index = 9001;
        ig.entries.push_back(row);
        db.upsert_itemgroup(std::move(ig));
        expect(db.next_free_itemgroup_index() == 1, "next free itemgroup ignores reserved index");
        DropRoller roller(db);
        const auto maguni_drops =
            roller.roll_monster_drops(DropRoller::effective_monster_kill_item_groups(
                                          {}, static_cast<int>(MonsterRace::Maguni)),
                                      100,
                                      200);
        expect(maguni_drops.size() == 1 && maguni_drops[0].count == 42,
               "reserved itemgroup rolls on maguni kill");
        const auto chest_drops = roller.roll_monster_drops(
            DropRoller::effective_monster_kill_item_groups({}, static_cast<int>(MonsterRace::Item)),
            100,
            200);
        expect(chest_drops.empty(), "reserved itemgroup does not roll on chest");
    }

    const std::string repo = OPENKAL_REPO_ROOT;

    {
        using openkal::server::DropRoller;
        using openkal::server::ItemGroupDb;
        using openkal::server::kGlobalNormalMonsterItemGroupIndex;
        ItemGroupDb db;
        const std::string groups_dir = openkal::platform::join_path(repo, "config/groups");
        const std::string itemgroups_dir = openkal::platform::join_path(repo, "config/itemgroups");
        expect(db.load_groups_dir(groups_dir), "load config/groups");
        expect(db.load_itemgroups_dir(itemgroups_dir), "load config/itemgroups");
        const auto* global_ig = db.find_itemgroup(kGlobalNormalMonsterItemGroupIndex);
        expect(global_ig != nullptr && !global_ig->entries.empty(),
               "reserved itemgroup 1000000.json is valid and non-empty");
        DropRoller roller(db);
        const auto drops = roller.roll_itemgroup(kGlobalNormalMonsterItemGroupIndex);
        expect(!drops.has_value(), "disabled default event itemgroup does not roll");

        const auto* ig908 = db.find_itemgroup(908);
        expect(ig908 != nullptr && ig908->entries.size() == 1 &&
                   ig908->entries[0].group_index == 908,
               "quest itemgroup 908 wraps group 908 (White Peony)");
        const auto drop908 = roller.roll_itemgroup(908);
        expect(drop908.has_value() && drop908->item_index == 153,
               "itemgroup 908 rolls White Peony");

        const auto* ig932 = db.find_itemgroup(932);
        expect(ig932 != nullptr && ig932->entries.size() == 1 &&
                   ig932->entries[0].group_index == 932,
               "quest itemgroup 932 wraps group 932 (Demon Commander head)");
        const auto drop932 = roller.roll_itemgroup(932);
        expect(drop932.has_value() && drop932->item_index == 181,
               "itemgroup 932 rolls Demon Commander head");

        const auto* ig957 = db.find_itemgroup(957);
        expect(ig957 != nullptr && ig957->entries.size() == 1 &&
                   ig957->entries[0].group_index == 957,
               "itemgroup 957 still wraps Hatred Mask group");
        const auto drop957 = roller.roll_itemgroup(957);
        expect(drop957.has_value() && drop957->item_index == 349,
               "itemgroup 957 rolls Devil Troop of Hatred mask");
    }
    auto profile = openkal::platform::Profile::load(openkal::platform::Profile::default_path(repo));
    expect(profile.has_value(), "load profile");
    if (!profile) {
        return 1;
    }

    // Synthetic maps around a fixed spawn — independent of external asset pack server_map.
    constexpr int kSpawnX = 258576;
    constexpr int kSpawnY = 261408;
    constexpr int kSpawnZ = 16100;
    int spawn_tx = 0;
    int spawn_ty = 0;
    openkal::coords::pixel_to_tile(kSpawnX, kSpawnY, spawn_tx, spawn_ty);
    int cell_x = 0;
    int cell_y = 0;
    int lx = 0;
    int ly = 0;
    openkal::coords::world_tile_to_cell(spawn_tx, spawn_ty, cell_x, cell_y, lx, ly);

    const std::string maps = openkal::test::temp_dir("server_maps");
    for (int dy = -2; dy <= 2; ++dy) {
        for (int dx = -2; dx <= 2; ++dx) {
            char name[64];
            std::snprintf(name, sizeof(name), "n_%03d_%03d.akarea", cell_x + dx, cell_y + dy);
            expect(openkal::test::write_open_area(openkal::platform::join_path(maps, name)),
                   "write open area map");
        }
    }

    // Gen rect a bit away from spawn so "outside city core" still holds.
    const int gen_tx0 = spawn_tx + 40;
    const int gen_ty0 = spawn_ty + 40;
    const int gen_tx1 = spawn_tx + 60;
    const int gen_ty1 = spawn_ty + 60;
    int gen_cx = 0;
    int gen_cy = 0;
    openkal::coords::world_tile_to_cell(gen_tx0, gen_ty0, gen_cx, gen_cy, lx, ly);
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            char name[64];
            std::snprintf(name, sizeof(name), "n_%03d_%03d.akarea", gen_cx + dx, gen_cy + dy);
            openkal::test::write_open_area(openkal::platform::join_path(maps, name));
        }
    }

    const std::string monster_dir = openkal::test::temp_dir("server_monsters_json");
    expect(write_test_monsters_json(monster_dir, gen_tx0, gen_ty0, gen_tx1, gen_ty1),
           "write monster JSON");

    openkal::server::GameWorld world;
    expect(world.load_maps_around(maps, kSpawnX, kSpawnY, 2), "load maps around spawn");
    expect(world.load_monsters_json_dir(monster_dir, kSpawnX, kSpawnY, 0), "load monster JSON");
    expect(world.ensure_maps_for_spawns(maps), "ensure spawn maps");
    std::uint32_t id = 1;
    world.spawn_from_config(id);
    expect(world.monsters().size() >= 4, "spawned monsters");
    int outside_city = 0;
    for (const auto& m : world.monsters()) {
        const int dx = m.x - kSpawnX;
        const int dy = m.y - kSpawnY;
        if (dx * dx + dy * dy > 500 * 500) {
            ++outside_city;
        }
    }
    expect(outside_city >= 4, "monsters spawn outside city core");

    // Far-zone refill: a second gen far from spawn should populate via
    // ensure_monsters_around.
    {
        const int far_tx0 = spawn_tx + 400;
        const int far_ty0 = spawn_ty + 400;
        const int far_tx1 = spawn_tx + 420;
        const int far_ty1 = spawn_ty + 420;
        int far_cx = 0;
        int far_cy = 0;
        openkal::coords::world_tile_to_cell(far_tx0, far_ty0, far_cx, far_cy, lx, ly);
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                char name[64];
                std::snprintf(name, sizeof(name), "n_%03d_%03d.akarea", far_cx + dx, far_cy + dy);
                openkal::test::write_open_area(openkal::platform::join_path(maps, name));
            }
        }
        // Add a far JSON spawn and reload into a fresh world slice via ensure API.
        expect(write_test_monsters_json(monster_dir, gen_tx0, gen_ty0, gen_tx1, gen_ty1, true),
               "append far monster JSON spawn");
        openkal::server::GameWorld far_world;
        expect(far_world.load_maps_around(maps, kSpawnX, kSpawnY, 2), "load maps for far test");
        expect(far_world.load_monsters_json_dir(monster_dir, kSpawnX, kSpawnY, 0),
               "reload JSON with far spawn");
        expect(far_world.ensure_maps_for_spawns(maps), "ensure far maps");
        std::uint32_t fid = 1;
        far_world.spawn_from_config(fid, 8, kSpawnX, kSpawnY);
        const std::size_t before = far_world.monsters().size();
        const int far_px = ((far_tx0 + far_tx1) / 2) * 32;
        const int far_py = ((far_ty0 + far_ty1) / 2) * 32;
        const int added = far_world.ensure_monsters_around(far_px, far_py, 4096, fid, 64);
        expect(added > 0, "ensure_monsters_around spawned in far zone");
        expect(far_world.monsters().size() > before, "live monster count grew after far ensure");
        int near_far = 0;
        for (const auto& m : far_world.monsters()) {
            const long long dx = static_cast<long long>(m.x) - far_px;
            const long long dy = static_cast<long long>(m.y) - far_py;
            if (dx * dx + dy * dy < 4096LL * 4096LL) {
                ++near_far;
            }
        }
        expect(near_far >= 4, "monsters exist near far-zone center");
    }

    {
        const std::string monsters_dir = openkal::test::temp_dir("server_monster_area_json");
        const std::string json_path = openkal::platform::join_path(monsters_dir, "0001.json");
        {
            std::ofstream out(json_path);
            expect(static_cast<bool>(out), "write monster area json");
            out << R"({
  "index": 1,
  "name": "AreaMob",
  "stats": { "race": 0, "level": 1, "hp": 10, "attack": { "min": 1, "max": 1 } },
  "spawns": [
    { "map": 2, "area": 108, "max": 4, "cycle": 1, "rect": [10, 20, 30, 40] }
  ]
})";
        }
        openkal::server::GameWorld area_world;
        expect(area_world.load_monsters_json_dir(monsters_dir, 0, 0, 0), "load area json dir");
        const openkal::server::MonsterSpawn* spawn = area_world.find_spawn_by_area(108);
        expect(spawn != nullptr, "find spawn by area 108");
        if (spawn != nullptr) {
            expect(spawn->map_index == 2, "area 108 map");
            expect(spawn->x0 == 10 * openkal::map::kTileSize, "area 108 x0");
            expect(spawn->y0 == 20 * openkal::map::kTileSize, "area 108 y0");
            expect(spawn->x1 == 30 * openkal::map::kTileSize, "area 108 x1");
            expect(spawn->y1 == 40 * openkal::map::kTileSize, "area 108 y1");
            expect(spawn->cycle_sec == 1, "area 108 cycle seconds");
        }
        expect(area_world.find_spawn_by_area(109) == nullptr, "unknown area is null");
        expect(area_world.find_spawn_by_area(0) == nullptr, "area 0 is null");
    }

    {
        const std::string later_dir = openkal::test::temp_dir("server_monster_map33_json");
        const std::string json_path = openkal::platform::join_path(later_dir, "1131.json");
        {
            std::ofstream out(json_path);
            expect(static_cast<bool>(out), "write map 33 monster json");
            out << R"({
  "index": 1131,
  "name": "Death Harvester",
  "stats": { "race": 0, "level": 1, "hp": 10, "attack": { "min": 1, "max": 1 } },
  "spawns": [
    { "map": 33, "area": 7028, "max": 9, "cycle": 30, "rect": [3183, 608, 3197, 624] }
  ]
})";
        }
        openkal::server::GameWorld later_world;
        expect(later_world.load_monsters_json_dir(later_dir, 0, 0, 0), "load map 33 json dir");
        const openkal::server::MonsterSpawn* spawn = later_world.find_spawn_by_area(7028);
        expect(spawn != nullptr && spawn->map_index == 33, "later-pack map 33 gen is kept");
    }

    {
        const std::string blocked_dir = openkal::test::temp_dir("server_monster_blocked_spawn");
        const std::string json_path = openkal::platform::join_path(blocked_dir, "0814.json");
        {
            std::ofstream out(json_path);
            expect(static_cast<bool>(out), "write blocked spawn json");
            out << R"({
  "index": 814,
  "name": "BlockedConjurator",
  "stats": { "race": 0, "level": 1, "hp": 10, "attack": { "min": 1, "max": 1 } },
  "spawns": [
    { "map": 0, "area": 5317, "max": 1, "cycle": 1, "rect": [10, 10, 12, 12] }
  ]
})";
        }
        const std::string blocked_maps = openkal::test::temp_dir("server_monster_blocked_maps");
        expect(openkal::test::write_blocked_area(
                   openkal::platform::join_path(blocked_maps, "n_000_000.akarea")),
               "write blocked area map");
        openkal::server::GameWorld blocked_world;
        expect(blocked_world.load_monsters_json_dir(blocked_dir, 0, 0, 0),
               "load blocked spawn json");
        const int px = 11 * openkal::map::kTileSize;
        const int py = 11 * openkal::map::kTileSize;
        expect(blocked_world.load_maps_around(blocked_maps, px, py, 1), "load blocked maps");
        std::uint32_t bid = 1;
        expect(blocked_world.ensure_monsters_around(px, py, 4096, bid, 64) == 0,
               "blocked gen does not spawn on unwalkable tiles");
        expect(blocked_world.monsters().empty(), "no center-fallback monster on blocked rect");
    }

    {
        const std::string remap_dir = openkal::test::temp_dir("server_monster_mapinfo_remap");
        const std::string json_path = openkal::platform::join_path(remap_dir, "1131.json");
        {
            std::ofstream out(json_path);
            expect(static_cast<bool>(out), "write remap monster json");
            out << R"({
  "index": 1131,
  "name": "Death Harvester",
  "stats": { "race": 0, "level": 1, "hp": 10, "attack": { "min": 1, "max": 1 } },
  "spawns": [
    { "map": 31, "area": 7028, "max": 9, "cycle": 30, "rect": [3183, 608, 3197, 624] }
  ]
})";
        }
        const std::string info_path = openkal::platform::join_path(remap_dir, "mapinfo.json");
        {
            std::ofstream out(info_path);
            expect(static_cast<bool>(out), "write remap mapinfo");
            out << R"({"maps":[{"index":33,"kind":1,"xy":[12,2],"mapname":"Map\\DG03_01"}]})";
        }
        openkal::server::GameWorld remap_world;
        expect(remap_world.load_monsters_json_dir(remap_dir, 0, 0, 0), "load remap json dir");
        openkal::map::MapCatalog catalog;
        expect(catalog.load(info_path), "load remap mapinfo");
        remap_world.apply_named_spawn_maps(catalog);
        const openkal::server::MonsterSpawn* spawn = remap_world.find_spawn_by_area(7028);
        expect(spawn != nullptr && spawn->map_index == 33,
               "area 7028 map follows map definition DG03 cell");
        if (spawn != nullptr) {
            const int x = (spawn->x0 + spawn->x1) / 2;
            const int y = (spawn->y0 + spawn->y1) / 2;
            expect(catalog.resolve_teleport_map(x, y, spawn->map_index) == 33,
                   "resolve_teleport_map names DG03 cell");
        }
    }

    {
        const std::string field_dir = openkal::test::temp_dir("server_monster_field_map");
        const std::string json_path = openkal::platform::join_path(field_dir, "0814.json");
        {
            std::ofstream out(json_path);
            expect(static_cast<bool>(out), "write field-map monster json");
            out << R"({
  "index": 814,
  "name": "CrazyDemonConjurator",
  "stats": { "race": 0, "level": 1, "hp": 10, "attack": { "min": 1, "max": 1 } },
  "spawns": [
    { "map": 6, "area": 5317, "max": 1, "cycle": 1, "rect": [11174, 5836, 11181, 5840] },
    { "map": 31, "area": 7028, "max": 1, "cycle": 1, "rect": [3183, 608, 3197, 624] }
  ]
})";
        }
        const std::string info_path = openkal::platform::join_path(field_dir, "mapinfo.json");
        {
            std::ofstream out(info_path);
            expect(static_cast<bool>(out), "write field-map mapinfo");
            out << R"({"maps":[
  {"index":0,"kind":1,"xy":[43,22],"filename":"data\\MAPS\\n_043_022.akarea"},
  {"index":33,"kind":1,"xy":[12,2],"mapname":"Map\\DG03_01"}
]})";
        }
        openkal::server::GameWorld field_world;
        expect(field_world.load_monsters_json_dir(field_dir, 0, 0, 0), "load field-map json");
        openkal::map::MapCatalog field_maps;
        expect(field_maps.load(info_path), "load field-map mapinfo");
        field_world.bind_field_spawns_to_overworld(field_maps);
        const openkal::server::MonsterSpawn* field = field_world.find_spawn_by_area(5317);
        expect(field != nullptr && field->map_index == 0, "outdoor map-6 gen binds to overworld");
        const openkal::server::MonsterSpawn* indoor = field_world.find_spawn_by_area(7028);
        expect(indoor != nullptr && indoor->map_index == 31, "indoor gen keeps authored map");
    }

    {
        const std::string d6_dir = openkal::test::temp_dir("server_monster_d6_map");
        const std::string json_path = openkal::platform::join_path(d6_dir, "1038.json");
        {
            std::ofstream out(json_path);
            expect(static_cast<bool>(out), "write D6 map-31 monster json");
            out << R"({
  "index": 1038,
  "name": "DungeonScoutSoldier",
  "stats": { "race": 0, "level": 1, "hp": 10, "attack": { "min": 1, "max": 1 } },
  "spawns": [
    { "map": 31, "area": 6575, "max": 5, "cycle": 1, "rect": [2141, 569, 2161, 590] },
    { "map": 31, "area": 6719, "max": 5, "cycle": 1, "rect": [2655, 570, 2672, 589] },
    { "map": 6, "area": 5317, "max": 1, "cycle": 1, "rect": [11174, 5836, 11181, 5840] }
  ]
})";
        }
        const std::string info_path = openkal::platform::join_path(d6_dir, "mapinfo.json");
        {
            std::ofstream out(info_path);
            expect(static_cast<bool>(out), "write D6 mapinfo");
            out << R"({"maps":[
  {"index":25,"kind":1,"xy":[8,2],"mapname":"Map\\d_tower_01"},
  {"index":32,"kind":1,"xy":[10,2],"mapname":"Map\\d_tower_01"},
  {"index":0,"kind":1,"xy":[43,22],"filename":"data\\MAPS\\n_043_022.akarea"}
]})";
        }
        openkal::server::GameWorld d6_world;
        expect(d6_world.load_monsters_json_dir(d6_dir, 0, 0, 0), "load D6 json");
        openkal::map::MapCatalog d6_maps;
        expect(d6_maps.load(info_path), "load D6 mapinfo");
        d6_world.apply_named_spawn_maps(d6_maps);
        d6_world.bind_field_spawns_to_overworld(d6_maps);
        const openkal::server::MonsterSpawn* floor1 = d6_world.find_spawn_by_area(6575);
        expect(floor1 != nullptr && floor1->map_index == 25,
               "D6 floor-1 gen map 31 binds to map definition 25");
        const openkal::server::MonsterSpawn* floor2 = d6_world.find_spawn_by_area(6719);
        expect(floor2 != nullptr && floor2->map_index == 32,
               "D6 floor-2 gen map 31 binds to map definition 32");
        const openkal::server::MonsterSpawn* field = d6_world.find_spawn_by_area(5317);
        expect(field != nullptr && field->map_index == 0, "field gen still binds to overworld");
    }

    {
        // compatibility behavior cycle is seconds (S_CYCLE * 1000 ms). cycle > 1 must wait
        // before refilling; cycle 1 still tops up immediately.
        const std::string cycle_dir = openkal::test::temp_dir("server_monster_cycle");
        const std::string json_path = openkal::platform::join_path(cycle_dir, "0293.json");
        {
            std::ofstream out(json_path);
            expect(static_cast<bool>(out), "write cycle monster json");
            out << R"({
  "index": 293,
  "name": "Wooden Box",
  "stats": { "race": 4, "level": 1, "hp": 1, "attack": { "min": 1, "max": 1 } },
  "spawns": [
    { "map": 0, "area": 1686, "max": 2, "cycle": 1400, "rect": [10, 10, 20, 20] }
  ]
})";
        }
        openkal::server::GameWorld cycle_world;
        expect(cycle_world.load_monsters_json_dir(cycle_dir, 0, 0, 0), "load cycle json dir");
        const std::string cycle_maps = openkal::test::temp_dir("server_monster_cycle_maps");
        expect(openkal::test::write_open_area(
                   openkal::platform::join_path(cycle_maps, "n_000_000.akarea")),
               "write cycle open area map");
        const openkal::server::MonsterSpawn* box = cycle_world.find_spawn_by_area(1686);
        expect(box != nullptr && box->cycle_sec == 1400, "wooden box cycle is 1400 seconds");
        std::uint32_t cid = 1;
        const int cx = 15 * openkal::map::kTileSize;
        const int cy = 15 * openkal::map::kTileSize;
        expect(cycle_world.load_maps_around(cycle_maps, cx, cy, 1), "load cycle maps");
        const int first = cycle_world.ensure_monsters_around(cx, cy, 4096, cid, 64);
        expect(first > 0, "first fill ignores cycle (m_dwNextGen = now)");
        const std::size_t filled = cycle_world.monsters().size();
        cycle_world.monsters().clear();
        const int instant = cycle_world.ensure_monsters_around(cx, cy, 4096, cid, 64);
        expect(instant == 0, "cycle 1400 does not refill instantly");
        std::vector<openkal::server::MonsterMoveEvent> moves;
        cycle_world.tick(1399.0f, moves);
        const int early = cycle_world.ensure_monsters_around(cx, cy, 4096, cid, 64);
        expect(early == 0, "cycle 1400 still waiting before expiry");
        cycle_world.tick(2.0f, moves);
        const int after = cycle_world.ensure_monsters_around(cx, cy, 4096, cid, 64);
        expect(after > 0, "cycle 1400 refills after the wait");
        expect(cycle_world.monsters().size() == filled, "cycle refill restores the gen");
    }

    {
        const std::string fast_dir = openkal::test::temp_dir("server_monster_cycle_fast");
        const std::string json_path = openkal::platform::join_path(fast_dir, "0001.json");
        {
            std::ofstream out(json_path);
            expect(static_cast<bool>(out), "write cycle-1 monster json");
            out << R"({
  "index": 1,
  "name": "FastMob",
  "stats": { "race": 0, "level": 1, "hp": 10, "attack": { "min": 1, "max": 1 } },
  "spawns": [
    { "map": 0, "area": 1, "max": 3, "cycle": 1, "rect": [10, 10, 20, 20] }
  ]
})";
        }
        openkal::server::GameWorld fast_world;
        expect(fast_world.load_monsters_json_dir(fast_dir, 0, 0, 0), "load cycle-1 json dir");
        const std::string fast_maps = openkal::test::temp_dir("server_monster_cycle_fast_maps");
        expect(openkal::test::write_open_area(
                   openkal::platform::join_path(fast_maps, "n_000_000.akarea")),
               "write cycle-1 open area map");
        std::uint32_t fid = 1;
        const int fx = 15 * openkal::map::kTileSize;
        const int fy = 15 * openkal::map::kTileSize;
        expect(fast_world.load_maps_around(fast_maps, fx, fy, 1), "load cycle-1 maps");
        expect(fast_world.ensure_monsters_around(fx, fy, 4096, fid, 64) > 0, "cycle 1 first fill");
        fast_world.monsters().clear();
        expect(fast_world.ensure_monsters_around(fx, fy, 4096, fid, 64) > 0,
               "cycle 1 refills immediately");
    }

    {
        // File-order fill must not starve the gen the player is standing in: earlier
        // JSON spawn records inside the 16k bubble cannot consume the whole cap.
        const std::string nearest_dir = openkal::test::temp_dir("server_spawn_nearest");
        {
            std::ofstream out(openkal::platform::join_path(nearest_dir, "0001.json"));
            expect(static_cast<bool>(out), "write nearest-first far JSON");
            out << R"({"index":1,"name":"FarMob","stats":{"race":0,"level":1,"hp":100,"attack":{"min":1,"max":1}},"spawns":[)";
            for (int i = 0; i < 40; ++i) {
                if (i != 0) {
                    out << ",";
                }
                const int tx0 = 400 + i * 4;
                out << "{\"map\":0,\"max\":6,\"cycle\":1,\"rect\":[" << tx0 << ",100," << (tx0 + 2)
                    << ",102]}";
            }
            out << "]}";
        }
        {
            std::ofstream out(openkal::platform::join_path(nearest_dir, "0025.json"));
            expect(static_cast<bool>(out), "write nearest-first near JSON");
            out << R"({"index":25,"name":"NearMob","stats":{"race":0,"level":1,"hp":100,"attack":{"min":1,"max":1}},"spawns":[{"map":0,"max":8,"cycle":1,"rect":[90,90,110,110]}]})";
        }
        openkal::server::GameWorld nearest_world;
        expect(nearest_world.load_monsters_json_dir(nearest_dir, 0, 0, 0),
               "load nearest-first JSON");
        const std::string nearest_maps = openkal::test::temp_dir("server_spawn_nearest_maps");
        for (int cx = 0; cx <= 3; ++cx) {
            char name[32];
            std::snprintf(name, sizeof(name), "n_%03d_000.akarea", cx);
            expect(openkal::test::write_open_area(openkal::platform::join_path(nearest_maps, name)),
                   "write nearest-first open area map");
        }
        std::uint32_t nid = 1;
        const int player_x = 100 * openkal::map::kTileSize;
        const int player_y = 100 * openkal::map::kTileSize;
        expect(nearest_world.load_maps_around(nearest_maps, player_x, player_y, 2),
               "load nearest-first maps");
        expect(nearest_world.ensure_maps_for_spawns(nearest_maps), "ensure nearest-first maps");
        const int added = nearest_world.ensure_monsters_around(player_x, player_y, 16384, nid, 12);
        expect(added > 0, "nearest-first ensure spawned");
        int near_live = 0;
        int far_live = 0;
        for (const auto& m : nearest_world.monsters()) {
            if (m.hp <= 0) {
                continue;
            }
            if (m.def_index == 25) {
                ++near_live;
            } else {
                ++far_live;
            }
        }
        expect(near_live > 0, "player-standing gen fills before farther file-order gens");
        expect(near_live + far_live <= 12, "nearest-first respects max_monsters");
    }

    bool moved = false;
    for (int i = 0; i < 400; ++i) {
        std::vector<openkal::server::MonsterMoveEvent> ev;
        world.tick(0.05f, ev);
        if (!ev.empty()) {
            moved = true;
            break;
        }
    }
    expect(moved, "monster AI produced a move");

    const std::string save = openkal::test::temp_dir(
        (std::string("sqlite_store_") + std::to_string(::getpid())).c_str());
    std::error_code cleanup_error;
    std::filesystem::remove_all(save, cleanup_error);
    expect(!cleanup_error, "clear sqlite store fixture");
    openkal::test::mkdir_p(save);
    auto store = openkal::server::create_data_store(openkal::server::StoreBackend::Sqlite);
    expect(store != nullptr, "create sqlite store");
    expect(store->open(save), "open sqlite store");
    openkal::server::CharacterRecord def;
    def.name = "TestHero";
    def.map_x = kSpawnX;
    def.map_y = kSpawnY;
    def.map_z = kSpawnZ;
    def.level = 3;
    def.hp = 80;
    def.max_hp = 120;
    def.mp = 40;
    def.max_mp = 60;
    def.exp = 1234;
    def.pu_point = 7;
    def.rage = 777;
    def.str = 20;
    def.hth = 18;
    def.intl = 10;
    def.wis = 10;
    def.dex = 12;
    def.money = 500;
    expect(store->ensure_default_player("tester", "tester", def), "ensure default player");
    auto acc = store->find_account("tester");
    expect(acc.has_value(), "find account");
    auto chars = store->list_characters(acc->id);
    expect(!chars.empty(), "has character");
    std::int32_t pid = chars.front().id;
    expect(pid > 0, "character id assigned");

    auto loaded = store->load_character(pid);
    expect(loaded.has_value(), "load character");
    if (loaded) {
        expect(loaded->name == "TestHero", "character name");
        expect(loaded->level == 3, "character level");
        expect(loaded->exp == 1234, "character exp");
        expect(loaded->money == 500, "character money");
        expect(loaded->str == 20, "character str");
        expect(loaded->specialty == 1, "starting specialty is wandering");
        expect(loaded->rage == 777, "character rage");
    }

    loaded->map = 2;
    loaded->map_x = 111;
    loaded->map_y = 222;
    loaded->map_z = 333;
    loaded->hp = 0;
    loaded->mp = 0;
    loaded->rage = 5432;
    loaded->specialty = 3;
    expect(store->save_character(*loaded), "save character pose");

    openkal::server::ItemRecord item;
    item.iid = 42;
    item.character_id = pid;
    item.index = 47;
    item.prefix = 1;
    item.count = 5;
    item.slot = 0;
    expect(store->insert_item(item), "insert item");
    item.count = 9;
    item.random_stat_type = 15;
    item.random_stat_value = 5;
    item.transform_level = 4;
    item.transform_exp = 3210;
    expect(store->update_item(item), "update item");

    openkal::server::ItemRecord storage_item = item;
    storage_item.iid = 43;
    storage_item.character_id = 0;
    storage_item.account_id = acc->id;
    storage_item.slot = 215;
    storage_item.count = 2;
    expect(store->insert_item(storage_item), "insert account-storage item");

    openkal::server::SkillRecord skill;
    skill.character_id = pid;
    skill.skill_index = 3;
    skill.level = 2;
    expect(store->upsert_skill(skill), "upsert skill");

    openkal::server::ShortcutRecord shortcuts;
    shortcuts.character_id = pid;
    shortcuts.active_index = 4;
    shortcuts.slots[0] = 4;
    shortcuts.slots[2] = 6;
    expect(store->upsert_shortcuts(shortcuts), "upsert shortcuts");

    openkal::server::QuestFlagRecord quest;
    quest.character_id = pid;
    quest.quest = 12;
    quest.flag = 3;
    quest.clear = 0;
    expect(store->upsert_quest_flag(quest), "insert quest flag");
    quest.flag = 4;
    quest.clear = 1;
    expect(store->upsert_quest_flag(quest), "update quest flag");

    {
        openkal::server::StarterKit kit;
        expect(kit.load_dir(openkal::platform::join_path(repo, "config/starters")),
               "load starter kits");
        expect(!kit.items_for_class(0).empty(), "knight starter rows");
        expect(!kit.items_for_class(1).empty(), "mage starter rows");
        expect(!kit.items_for_class(2).empty(), "archer starter rows");
        expect(!kit.items_for_class(3).empty(), "thief starter rows");
        bool mage_has_common = false;
        for (const auto& row : kit.items_for_class(1)) {
            if (row.item_id == 23000 && row.count == 5 && !row.equipped) {
                mage_has_common = true;
            }
        }
        expect(mage_has_common, "common medicine merged into mage kit");
        expect(kit.has_spawn(), "common spawn loaded");
        expect(kit.spawn_x() == 257285, "common spawn x");
        expect(kit.spawn_y() == 258744, "common spawn y");
        expect(kit.spawn_z() == 16100, "common spawn z");
        openkal::server::CharacterRecord starter;
        starter.account_id = acc->id;
        starter.name = "StarterKnight";
        starter.clazz = 0;
        starter.level = 1;
        expect(store->save_character(starter), "create starter character");
        std::optional<openkal::server::CharacterRecord> granted;
        for (const auto& rec : store->list_characters(acc->id)) {
            if (rec.name == "StarterKnight") {
                granted = rec;
                break;
            }
        }
        expect(granted.has_value(), "find starter character");
        if (granted) {
            granted->map_x = 1;
            granted->map_y = 2;
            granted->map_z = 3;
            std::unordered_map<int, openkal::server::ItemCombatDef> defs;
            expect(kit.grant_if_empty(*store, *granted, defs), "grant knight starters");
            expect(granted->map_x == kit.spawn_x(), "grant applies spawn x");
            expect(granted->map_y == kit.spawn_y(), "grant applies spawn y");
            expect(granted->map_z == kit.spawn_z(), "grant applies spawn z");
            auto rows = store->load_items(granted->id);
            expect(!rows.empty(), "starter items inserted");
            bool equipped_sword = false;
            bool has_medicine = false;
            for (const auto& row : rows) {
                if (row.index == 1 && row.slot == 1 && row.count == 1) {
                    equipped_sword = true;
                }
                if (row.index == 23000 && row.slot == 0 && row.count == 5) {
                    has_medicine = true;
                }
            }
            expect(equipped_sword, "starter sword equipped");
            expect(has_medicine, "starter medicine in bag");
            expect(granted->weapon_index == 1, "character weapon_index set");
            expect(!kit.grant_if_empty(*store, *granted, defs), "starter grant is once-only");
        }
        openkal::server::CharacterRecord mage;
        mage.account_id = acc->id;
        mage.name = "StarterMage";
        mage.clazz = 1;
        mage.level = 1;
        expect(store->save_character(mage), "create starter mage");
        std::optional<openkal::server::CharacterRecord> mage_granted;
        for (const auto& rec : store->list_characters(acc->id)) {
            if (rec.name == "StarterMage") {
                mage_granted = rec;
                break;
            }
        }
        expect(mage_granted.has_value(), "find starter mage");
        if (mage_granted) {
            std::unordered_map<int, openkal::server::ItemCombatDef> defs;
            expect(kit.grant_if_empty(*store, *mage_granted, defs), "grant mage starters");
            auto rows = store->load_items(mage_granted->id);
            expect(!rows.empty(), "mage starter items inserted");
            bool equipped_wand = false;
            for (const auto& row : rows) {
                if (row.index == 90 && row.slot == 1 && row.count == 1) {
                    equipped_wand = true;
                }
            }
            expect(equipped_wand, "starter wand equipped");
            expect(mage_granted->weapon_index == 90, "mage weapon_index set");
            if (granted) {
                auto knight_rows = store->load_items(granted->id);
                for (const auto& mage_row : rows) {
                    for (const auto& knight_row : knight_rows) {
                        expect(mage_row.iid != knight_row.iid, "mage iid is globally unique");
                    }
                    expect(mage_row.iid != 42, "mage iid does not reuse fixture iid");
                }
            }
            expect(store->max_item_iid() >= 42, "max_item_iid tracks highest iid");
        }
    }

    store->close();

    auto store2 = openkal::server::create_data_store(openkal::server::StoreBackend::Sqlite);
    expect(store2 != nullptr, "reopen sqlite store");
    expect(store2->open(save), "reopen sqlite path");
    auto reloaded = store2->load_character(pid);
    expect(reloaded.has_value(), "reload character");
    if (reloaded) {
        expect(reloaded->map == 2, "reloaded map");
        expect(reloaded->map_x == 111, "reloaded x");
        expect(reloaded->map_y == 222, "reloaded y");
        expect(reloaded->map_z == 333, "reloaded z");
        expect(reloaded->hp == 0, "reloaded zero hp");
        expect(reloaded->mp == 0, "reloaded zero mp");
        expect(reloaded->rage == 5432, "reloaded rage");
        expect(reloaded->money == 500, "reloaded money");
        expect(reloaded->specialty == 3, "reloaded first-job specialty");
    }
    auto items = store2->load_items(pid);
    expect(items.size() == 1, "reloaded item count");
    if (!items.empty()) {
        expect(items[0].iid == 42, "reloaded item iid");
        expect(items[0].index == 47, "reloaded item index");
        expect(items[0].count == 9, "reloaded item count");
        expect(items[0].random_stat_type == 15, "reloaded random item stat type");
        expect(items[0].random_stat_value == 5, "reloaded random item stat value");
        expect(items[0].transform_level == 4, "reloaded transform egg level");
        expect(items[0].transform_exp == 3210, "reloaded transform egg exp");
    }
    const auto storage_items = store2->load_storage_items(acc->id);
    expect(storage_items.size() == 1, "reloaded account-storage item count");
    if (!storage_items.empty()) {
        expect(storage_items[0].iid == 43, "reloaded account-storage iid");
        expect(storage_items[0].character_id == 0, "storage item has no character owner");
        expect(storage_items[0].account_id == acc->id, "storage item keeps account owner");
        expect(storage_items[0].slot == 215, "storage item keeps sixth-register slot");
    }
    expect(store2->delete_item(42), "delete item");
    expect(store2->delete_item(43), "delete account-storage item");
    expect(store2->load_items(pid).empty(), "item deleted");
    auto skills = store2->load_skills(pid);
    expect(skills.size() == 1, "reloaded skill count");
    if (!skills.empty()) {
        expect(skills[0].skill_index == 3, "reloaded skill index");
        expect(skills[0].level == 2, "reloaded skill level");
    }
    skill.level = 0;
    expect(store2->upsert_skill(skill), "upsert skill level 0 deletes");
    expect(store2->load_skills(pid).empty(), "skill deleted at level 0");
    skill.level = 2;
    expect(store2->upsert_skill(skill), "restore skill after delete");
    auto shortcuts_reloaded = store2->load_shortcuts(pid);
    expect(shortcuts_reloaded.has_value(), "reloaded shortcuts");
    if (shortcuts_reloaded) {
        expect(shortcuts_reloaded->active_index == 4, "reloaded shortcut active");
        expect(shortcuts_reloaded->slots[0] == 4, "reloaded shortcut slot0");
        expect(shortcuts_reloaded->slots[2] == 6, "reloaded shortcut slot2");
        expect(shortcuts_reloaded->slots[1] == -1, "reloaded shortcut empty slot");
    }
    auto flags = store2->load_quest_flags(pid);
    expect(flags.size() == 1, "reloaded quest count");
    if (!flags.empty()) {
        expect(flags[0].quest == 12, "reloaded quest id");
        expect(flags[0].flag == 4, "reloaded quest flag");
        expect(flags[0].clear == 1, "reloaded quest clear");
    }
    store2->close();

    openkal::server::ServerConfig scfg;
    scfg.maps_dir = maps;
    scfg.save_dir =
        openkal::test::temp_dir((std::string("sqlite_host_") + std::to_string(::getpid())).c_str());
    cleanup_error.clear();
    std::filesystem::remove_all(scfg.save_dir, cleanup_error);
    expect(!cleanup_error, "clear sqlite host fixture");
    openkal::test::mkdir_p(scfg.save_dir);
    scfg.store = openkal::server::StoreBackend::Sqlite;
    scfg.monsters_dir = openkal::platform::join_path(repo, "config/monsters");
    scfg.items_dir = openkal::platform::join_path(repo, "config/items");
    scfg.skills_dir = openkal::platform::join_path(repo, "config/skills");
    scfg.transport = openkal::server::TransportKind::Unix;
    scfg.unix_path = openkal::platform::join_path(scfg.save_dir, "test.sock");
    scfg.starters_dir = openkal::platform::join_path(repo, "config/starters");
    scfg.spawn_x = kSpawnX;
    scfg.spawn_y = kSpawnY;
    scfg.spawn_z = kSpawnZ;
    scfg.map_radius_cells = 2;
    openkal::server::ServerHost host;
    expect(host.start(scfg), "start host");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    expect(host.running(), "host running");
    expect(host.monster_snapshot().size() >= 4, "host has monsters");
    host.stop();
    expect(!host.running(), "host stopped");

    return openkal::test::g_failures == 0 ? 0 : 1;
}
