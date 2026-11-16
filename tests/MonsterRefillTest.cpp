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
#include "openkal/server/world/GameWorld.h"
#include "test_util.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>

int main() {
    using openkal::test::expect;
    const auto root = openkal::test::temp_dir("monster_refill");
    std::filesystem::create_directories(root + "/monsters");
    std::filesystem::create_directories(root + "/maps");
    expect(openkal::test::write_open_area(root + "/maps/n_000_000.akarea"),
           "write owned walkable refill area");
    std::ofstream(root + "/monsters/1.json") << R"({
        "index":1,"name":"Owned refill monster",
        "stats":{"race":0,"level":1,"hp":10},
        "spawns":[
            {"map":0,"area":1,"max":2,"cycle":1,"rect":[10, 10, 20, 20]},
            {"map":0,"area":2,"max":2,"cycle":1,"rect":[50, 50, 60, 60]},
            {"map":0,"area":3,"max":2,"cycle":1,"rect":[200, 200, 210, 210]}
        ]})";
    constexpr int kNear = 15 * openkal::map::kTileSize;
    constexpr int kFar = 205 * openkal::map::kTileSize;
    openkal::server::GameWorld world;
    expect(world.load_monsters_json_dir(root + "/monsters", kNear, kNear, 0),
           "load owned refill definitions");
    expect(world.load_maps_around(root + "/maps", kNear, kNear, 1), "load owned refill map");
    std::uint32_t next = 1;
    expect(world.ensure_monsters_around(kNear, kNear, 512, next, 2) == 2,
           "fill the nearby gen to its configured population");
    expect(world.ensure_monsters_around(kNear, kNear, 512, next, 2) == 0,
           "repeated refill does not duplicate a full gen");
    if (world.monsters().empty()) {
        return 1;
    }
    world.monsters().front().hp = 0;
    expect(world.ensure_monsters_around(kNear, kNear, 512, next, 2) == 0,
           "a pending corpse still occupies population capacity");
    world.monsters().erase(world.monsters().begin());
    expect(world.ensure_monsters_around(kNear, kNear, 512, next, 2) == 1 &&
               world.monsters().size() == 2,
           "refill replaces exactly the removed occupant");
    expect(world.ensure_monsters_around(kNear, kNear, 10000, next, 4) == 2 &&
               world.monsters().size() == 4,
           "fill another gen without exceeding the global cap");
    expect(world.ensure_monsters_around(kFar, kFar, 10000, next, 4) == 2 &&
               world.monsters().size() == 4,
           "a distant destination replaces far occupants and preserves the cap");
    expect(std::count_if(world.monsters().begin(),
                         world.monsters().end(),
                         [](const auto& monster) {
                             return monster.roam_x0 == 200 * openkal::map::kTileSize;
                         }) == 2,
           "cached gen counts follow culling and placement within one refill");
    expect(world.ensure_monsters_around(kFar, kFar, 10000, next, 4) == 0,
           "a repeated destination retains the settled population");
    for (int trip = 0; trip < 20; ++trip) {
        const int position = trip % 2 == 0 ? kNear : kFar;
        (void) world.ensure_monsters_around(position, position, 10000, next, 4);
        expect(world.monsters().size() == 4,
               "repeated distant teleports preserve the global population after replacements");
        const auto generated = next;
        expect(world.ensure_monsters_around(position, position, 10000, next, 4) == 0 &&
                   next == generated,
               "replacement counts remain consistent for the next refill at the same destination");
    }

    openkal::server::GameWorld multiplayer;
    expect(multiplayer.load_monsters_json_dir(root + "/monsters", kNear, kNear, 0) &&
               multiplayer.load_maps_around(root + "/maps", kNear, kNear, 1),
           "load a separate multiplayer world");
    std::vector<openkal::server::PlayerPose> viewers;
    for (std::uint32_t id = 1; id <= 51; ++id) {
        viewers.push_back({id, kNear + static_cast<int>(id), kNear, 0});
    }
    expect(multiplayer.ensure_monsters_around(kNear, kNear, 10000, next, 2, 0, viewers) == 2 &&
               multiplayer.monsters().size() == 2,
           "fifty-one co-located viewers share one resident population budget");
    viewers.back().x = viewers.back().y = kFar;
    expect(multiplayer.ensure_monsters_around(kFar, kFar, 512, next, 2, 0, viewers) == 2 &&
               multiplayer.monsters().size() == 4,
           "a separated viewer gets a populated hunting ground without evicting the first group");
    const auto resident_ids = [&] {
        std::set<std::uint32_t> ids;
        for (const auto& monster : multiplayer.monsters()) {
            ids.insert(monster.id);
        }
        return ids;
    };
    const auto initial_ids = resident_ids();
    for (int refill = 0; refill < 20; ++refill) {
        const int position = refill % 2 == 0 ? kNear : kFar;
        expect(multiplayer.ensure_monsters_around(position, position, 10000, next, 2, 0, viewers) ==
                       0 &&
                   resident_ids() == initial_ids,
               "alternating distant refills retain both visible populations and entity ids");
    }
    const auto dead_id = multiplayer.monsters().front().id;
    multiplayer.monsters().front().hp = 0;
    expect(multiplayer.ensure_monsters_around(kNear, kNear, 512, next, 2, 0, viewers) == 0 &&
               resident_ids() == initial_ids,
           "a pending corpse does not evict monsters visible to a remote group");
    multiplayer.monsters().erase(multiplayer.monsters().begin());
    expect(multiplayer.ensure_monsters_around(kNear, kNear, 512, next, 2, 0, viewers) == 1 &&
               multiplayer.monsters().size() == 4,
           "local respawn restores capacity while the remote group stays populated");
    auto retained_ids = initial_ids;
    retained_ids.erase(dead_id);
    for (auto id : resident_ids()) {
        retained_ids.erase(id);
    }
    expect(retained_ids.empty(), "respawn preserves every other resident monster entity");
    viewers.back().x = viewers.back().y = kNear;
    (void) multiplayer.ensure_monsters_around(kNear, kNear, 10000, next, 2, 0, viewers);
    expect(multiplayer.monsters().size() == 2 &&
               std::all_of(multiplayer.monsters().begin(),
                           multiplayer.monsters().end(),
                           [](const auto& monster) {
                               return monster.roam_x0 == 10 * openkal::map::kTileSize;
                           }),
           "abandoned distant populations can be reclaimed after players regroup");

    std::filesystem::create_directories(root + "/layered");
    std::ofstream(root + "/layered/1.json") << R"({
        "index":1,"name":"Owned layered refill monster",
        "stats":{"race":0,"level":1,"hp":10},
        "spawns":[
            {"map":0,"area":1,"max":2,"cycle":1,"rect":[10, 10, 20, 20]},
            {"map":1,"area":2,"max":2,"cycle":1,"rect":[10, 10, 20, 20]}
        ]})";
    openkal::server::GameWorld layered;
    expect(layered.load_monsters_json_dir(root + "/layered", kNear, kNear, 0) &&
               layered.load_maps_around(root + "/maps", kNear, kNear, 1),
           "load identical spawn rectangles on separate logical maps");
    const std::vector<openkal::server::PlayerPose> map_viewers{{1, kNear, kNear, 0},
                                                               {2, kNear, kNear, 1}};
    expect(layered.ensure_monsters_around(kNear, kNear, 512, next, 2, 0, map_viewers) == 2 &&
               layered.ensure_monsters_around(kNear, kNear, 512, next, 2, 1, map_viewers) == 2 &&
               layered.monsters().size() == 4,
           "equal coordinates on different maps have independent gen counts and capacity");
    const auto settled_next = next;
    for (int refill = 0; refill < 10; ++refill) {
        expect(layered.ensure_monsters_around(
                   kNear, kNear, 512, next, 2, refill % 2, map_viewers) == 0 &&
                   next == settled_next,
               "map-specific refill counts preserve both resident populations");
    }
    std::filesystem::remove_all(root);
    return openkal::test::g_failures ? 1 : 0;
}
