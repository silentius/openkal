/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "OptimizationGameWorldPeer.h"
#include "fixture_util.h"
#include "openkal/protocol/MonsterRace.h"
#include "openkal/server/world/FindPath.h"
#include "openkal/server/world/GameWorld.h"
#include "test_util.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <climits>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <unistd.h>

namespace {
    using namespace openkal;
    using namespace openkal::server;
    using test::expect;

    // Arithmetic floor division independently describes the 8px cell contract,
    // including negative coordinates, without reusing the production helper.
    int cell(int value) {
        return value >= 0 ? value / 8 : (value + 1) / 8 - 1;
    }
    bool oracle(const GameWorld& world,
                std::uint32_t self,
                int x,
                int y,
                int map,
                const std::vector<PlayerPose>* players) {
        const auto matches = [&](int ox, int oy) {
            return cell(x) == cell(ox) && cell(y) == cell(oy);
        };
        for (const auto& monster : world.monsters()) {
            if (monster.id != self && monster.hp > 0 && monster.map == map &&
                monster.race != static_cast<int>(protocol::MonsterRace::Magic)) {
                const bool reserved = monster.dirty_move || monster.moving;
                if (matches(reserved ? monster.target_x : monster.x,
                            reserved ? monster.target_y : monster.y)) {
                    return true;
                }
            }
        }
        if (players) {
            for (const auto& player : *players) {
                if (player.map == map && matches(player.x, player.y)) {
                    return true;
                }
            }
        }
        return false;
    }

    MonsterEntity body(std::uint32_t id, int x, int y, int map = 0) {
        MonsterEntity result;
        result.id = id;
        result.x = result.target_x = x;
        result.y = result.target_y = y;
        result.hp = result.max_hp = 100;
        result.map = map;
        result.race = static_cast<int>(protocol::MonsterRace::Maguni);
        return result;
    }

    void test_occupancy() {
        GameWorld world;
        expect(!GameWorldCoveragePeer::blocked(world, 0, 0, 0, 0, nullptr),
               "empty world has no occupied cells");
        std::vector<PlayerPose> players;
        expect(!GameWorldCoveragePeer::blocked(world, 0, 0, 0, 0, &players),
               "empty pose list has no occupied cells");
        auto stationary = body(1, -8, -8);
        auto dirty = body(2, 8, 8);
        dirty.dirty_move = true;
        dirty.target_x = dirty.target_y = 16;
        auto moving = body(3, 24, 24);
        moving.moving = true;
        moving.target_x = moving.target_y = 32;
        auto both = body(4, 40, 40);
        both.dirty_move = both.moving = true;
        both.target_x = both.target_y = 48;
        auto dead = body(5, 56, 56);
        dead.hp = 0;
        auto wrong_map = body(6, 64, 64, 1);
        auto field = body(7, 72, 72);
        field.race = static_cast<int>(protocol::MonsterRace::Magic);
        world.monsters() = {stationary, dirty, moving, both, dead, wrong_map, field};
        players = {{90, -1, 8, 0}, {91, 80, 80, 1}, {92, 88, 88, 0}};
        bool equal = true;
        for (int map = 0; map <= 2; ++map) {
            for (std::uint32_t self = 0; self <= 7; ++self) {
                for (int coordinate : {-17, -9, -8, -1, 0,  7,  8,  15, 16, 23, 24, 31, 32, 39,
                                       40,  47, 48, 55, 56, 63, 64, 71, 72, 79, 80, 87, 88, 95}) {
                    for (int y : {coordinate, coordinate + 1, 8}) {
                        const bool actual = GameWorldCoveragePeer::blocked(
                            world, self, coordinate, y, map, &players);
                        equal =
                            equal && actual == oracle(world, self, coordinate, y, map, &players);
                        const bool without_players = GameWorldCoveragePeer::blocked(
                            world, self, coordinate, y, map, nullptr);
                        equal = equal &&
                                without_players == oracle(world, self, coordinate, y, map, nullptr);
                    }
                }
            }
        }
        expect(equal,
               "occupancy agrees with independent oracle across map/id/race/liveness and signed "
               "cell boundaries");
        expect(GameWorldCoveragePeer::reservation(stationary) == std::pair<int, int>{-8, -8} &&
                   GameWorldCoveragePeer::reservation(dirty) == std::pair<int, int>{16, 16} &&
                   GameWorldCoveragePeer::reservation(moving) == std::pair<int, int>{32, 32} &&
                   GameWorldCoveragePeer::reservation(both) == std::pair<int, int>{48, 48},
               "dirty and moving bodies reserve target rather than source");
        world.monsters() = {body(1, 512, 512), body(2, 512, 512)};
        expect(GameWorldCoveragePeer::blocked(world, 1, 512, 512, 0, nullptr),
               "self exclusion still detects an overlapping second body");
        world.monsters().pop_back();
        expect(!GameWorldCoveragePeer::blocked(world, 1, 512, 512, 0, nullptr),
               "removal immediately releases an overlapping body");
        world.monsters().front().target_x = 520;
        world.monsters().front().dirty_move = true;
        expect(!GameWorldCoveragePeer::blocked(world, 0, 512, 512, 0, nullptr) &&
                   GameWorldCoveragePeer::blocked(world, 0, 520, 512, 0, nullptr),
               "sequential movement transfers source occupancy to target immediately");
        world.monsters().front().hp = -1;
        expect(!GameWorldCoveragePeer::blocked(world, 0, 520, 512, 0, nullptr),
               "death releases a pending movement reservation immediately");
    }

    void test_moves(const std::string& root) {
        GameWorld world;
        expect(world.load_maps_around(root, 512, 512, 0), "load owned movement area");
        const std::vector<PlayerPose> none;
        const auto move =
            [&](int x, int y, int dx, int dy, const std::vector<PlayerPose>& players) {
                const bool legacy = GameWorldCoveragePeer::move(world, 1, x, y, dx, dy, 0, players);
                WorldOccupancyIndex index;
                index.rebuild(world.monsters(), players);
                const bool indexed =
                    GameWorldCoveragePeer::with_occupancy(world, index, players, [&] {
                        return GameWorldCoveragePeer::move(world, 1, x, y, dx, dy, 0, players);
                    });
                expect(indexed == legacy,
                       "indexed sampled movement matches the full-scan predicate");
                return legacy;
            };
        expect(!move(512, 512, 0, 0, none), "zero displacement is not a movement tile");
        bool clear = true;
        for (int dx : {-64, -40, -32, -17, -8, -1, 0, 1, 7, 8, 16, 31, 32, 40, 64}) {
            for (int dy : {-64, -32, -1, 0, 1, 8, 32, 64}) {
                if (dx || dy) {
                    clear = clear && move(512, 512, dx, dy, none);
                }
            }
        }
        expect(clear, "positive and negative axial/diagonal short and long steps remain open");
        expect(!move(8190, 512, 8, 0, none), "missing terrain at first sample blocks a short step");
        expect(!move(8152, 512, 40, 0, none),
               "missing terrain only at long-step endpoint blocks movement");
        expect(!move(512, 512, 8, 0, {{5, 520, 512, 0}}),
               "player occupancy blocks the sampled destination");
        expect(move(512, 512, 8, 0, {{5, 520, 512, 1}}),
               "identical coordinates on another map remain free");
        expect(!move(512, 512, 40, 0, {{5, 552, 512, 0}}),
               "player occupancy at long-step endpoint blocks movement");
        expect(!move(512, 512, 64, 0, {{5, 528, 512, 0}}),
               "intermediate occupancy blocks a long step");
        expect(move(512, 512, 64, 0, {{5, 552, 512, 0}}),
               "characterize existing 32px sampling cap: intermediate cells beyond it are not "
               "queried");
        world.monsters().push_back(body(1, 520, 512));
        expect(move(512, 512, 8, 0, none), "moving body ignores its own occupied cell");
        world.monsters().push_back(body(2, 536, 512));
        auto& blocker = world.monsters().back();
        blocker.target_x = 520;
        blocker.dirty_move = true;
        expect(!move(512, 512, 8, 0, none), "another body's reserved target blocks immediately");
        blocker.dirty_move = false;
        expect(move(512, 512, 8, 0, none),
               "clearing reservation frees the destination immediately");
        auto* terrain = world.grid().cell(0, 0);
        expect(terrain != nullptr, "movement fixture exposes its owned terrain cell");
        if (terrain) {
            terrain->attr_ref(20, 20) = map::kAttrVillage;
            expect(world.walkable_pixel(640, 640) && !world.monster_spawnable_pixel(640, 640) &&
                       !move(632, 640, 8, 0, none) && !move(600, 640, 40, 0, none),
                   "walkable village terrain rejects both short steps and long-step endpoints");
            terrain->attr_ref(20, 20) = 0;
            terrain->set_not_moveable_mask(20, 20, 0xffffu);
            expect(!world.walkable_pixel(640, 640) && !move(632, 640, 8, 0, none),
                   "not-moveable subcell mask blocks movement before occupancy queries");
            terrain->set_not_moveable_mask(20, 20, 0);
            terrain->set_blocked_mask(20, 20, 0xffffu);
            expect(!world.walkable_pixel(640, 640) && !move(600, 640, 40, 0, none),
                   "blocked subcell mask rejects the long-step endpoint");
        }
    }

    void test_spawnable_terrain() {
        GameWorld world;
        map::AreaCell terrain;
        terrain.assign_attrs(map::kTileCount * map::kTileCount, 0);
        terrain.attr_ref(255, 1) = map::kAttrVillage;
        terrain.attr_ref(255, 2) = map::kAttrPortal | map::kAttrSafety;
        terrain.attr_ref(255, 3) = map::kAttrObject;
        terrain.set_blocked_mask(255, 4, 0x0001u);
        terrain.set_not_moveable_at_movement_cell(2040, 40, true);
        world.grid().set_cell(0, 0, terrain);
        map::AreaCell adjacent;
        adjacent.assign_attrs(map::kTileCount * map::kTileCount, 0);
        world.grid().set_cell(1, 0, adjacent);

        const auto spawnable = [&](int x, int y) { return world.monster_spawnable_pixel(x, y); };
        expect(!spawnable(8160, 32) && spawnable(8192, 32),
               "the village exclusion applies only on its side of an area boundary");
        expect(spawnable(8160, 64) && spawnable(8160, 96),
               "portal, safety and object attributes alone do not exclude monster spawns");
        expect(!spawnable(8160, 128) && spawnable(8168, 128),
               "a blocked subcell leaves its neighbor on the same tile open");
        expect(!spawnable(8160, 160) && spawnable(8164, 160),
               "the finer not-moveable mask leaves the adjacent movement cell open");
        expect(!spawnable(16384, 32), "an unloaded area is not a valid spawn position");
    }

    void test_indexed_occupancy() {
        GameWorld world;
        std::vector<PlayerPose> players{{1, 0, 0, 0}, {90, INT_MIN, INT_MAX, -1}};
        auto foreign_players = players;
        foreign_players.front().x = 128;
        for (int i = 0; i < 20; ++i) {
            auto m = body(static_cast<std::uint32_t>(i % 7 + 1), i * 8 - 80, i * 8 - 80, i % 3 - 1);
            m.dirty_move = i % 4 == 1;
            m.moving = i % 4 == 2;
            m.target_x += 8;
            m.target_y -= 8;
            if (i % 5 == 0) {
                m.hp = 0;
            }
            if (i % 6 == 0) {
                m.race = static_cast<int>(protocol::MonsterRace::Magic);
            }
            world.monsters().push_back(m);
        }
        world.monsters().push_back(body(1, 0, 0));
        world.monsters().push_back(body(1, 0, 0));
        world.monsters().push_back(body(50, INT_MIN, INT_MAX, -1));
        world.monsters().push_back(body(51, INT_MAX, INT_MIN, 1));
        WorldOccupancyIndex index;
        index.rebuild(world.monsters(), players);
        const auto compare = [&](const char* stage) {
            bool equal = true;
            GameWorldCoveragePeer::with_occupancy(world, index, players, [&] {
                for (int map : {-1, 0, 1, 99}) {
                    for (std::uint32_t self : {0u, 1u, 2u, 7u, 50u, 51u}) {
                        for (int x :
                             {INT_MIN, INT_MIN + 7, -81, -80, -79, -17,         -16,    -9, -8,
                              -7,      -1,          0,   1,   7,   8,           9,      15, 16,
                              17,      79,          80,  81,  128, INT_MAX - 7, INT_MAX}) {
                            for (int y : {x, -1, 0, INT_MIN, INT_MAX}) {
                                equal = equal && index.blocked(self, x, y, map, true) ==
                                                     oracle(world, self, x, y, map, &players);
                                for (const auto* poses :
                                     std::array<const std::vector<PlayerPose>*, 3>{
                                         &players, &foreign_players, nullptr}) {
                                    equal = equal && GameWorldCoveragePeer::blocked(
                                                         world, self, x, y, map, poses) ==
                                                         oracle(world, self, x, y, map, poses);
                                }
                            }
                        }
                    }
                }
            });
            expect(equal && GameWorldCoveragePeer::occupancy_inactive(world), stage);
        };
        compare("indexed occupancy and foreign-pose fallback match signed-cell oracle including "
                "integer extrema");
        expect(index.blocked(1, 0, 0, 0, true) && !index.blocked(1, 0, 0, 0, false),
               "duplicate self IDs are all excluded while an identically numbered player still "
               "blocks");
        const auto mutate = [&](std::size_t position, const auto& change) {
            const auto before = WorldOccupancyIndex::reservation(world.monsters()[position]);
            change(world.monsters()[position]);
            index.update(before, world.monsters()[position]);
        };
        mutate(1, [](auto& m) {
            m.map = 0;
            m.target_x = 8;
            m.target_y = -8;
            m.moving = true;
        });
        compare(
            "sequential reservation updates release source cells and reserve changed map/targets");
        mutate(20, [](auto& m) { m.hp = 0; });
        compare("removing one duplicate ID reservation retains the other overlapping body");
        mutate(21, [](auto& m) { m.race = static_cast<int>(protocol::MonsterRace::Magic); });
        compare("death and a nonblocking race transition release only their own reservations");
        auto spawned = body(88, -8, 8);
        world.monsters().push_back(spawned);
        index.update({}, spawned);
        compare("a newly spawned reservation joins an existing numeric snapshot");
        const auto removed = WorldOccupancyIndex::reservation(world.monsters().back());
        spawned.hp = 0;
        world.monsters().pop_back();
        index.update(removed, spawned);
        compare("removal releases a numeric reservation without retaining invalid entity pointers");
        bool threw = false;
        try {
            GameWorldCoveragePeer::with_occupancy(
                world, index, players, [] { throw std::runtime_error("owned query failure"); });
        } catch (const std::runtime_error&) {
            threw = true;
        }
        expect(threw && GameWorldCoveragePeer::occupancy_inactive(world),
               "query scope restores both borrowed occupancy pointers when an operation throws");
        world.monsters().clear();
        players.clear();
        index.rebuild(world.monsters(), players);
        compare("an empty rebuild releases every prior monster and player reservation");
    }

    auto monster_state(const MonsterEntity& m) {
        return std::make_tuple(m.id,
                               m.x,
                               m.y,
                               m.z,
                               m.map,
                               m.hp,
                               m.target_x,
                               m.target_y,
                               m.dir,
                               m.moving,
                               m.move_is_run,
                               m.move_busy_left,
                               m.idle_left,
                               m.dirty_move,
                               m.dirty_stop,
                               m.ai,
                               m.aggro,
                               m.aggro_player_id,
                               m.beacon_pursuit,
                               m.aggro_hostility,
                               m.hostility_by_player,
                               m.reward_damage_by_player,
                               m.attack_cooldown,
                               m.skill_cooldown,
                               m.skill_precast_left,
                               m.pending_skill_slot,
                               m.next_skill_slot,
                               m.spawn_protection_left,
                               m.attack_swinging,
                               m.detect_cooldown,
                               m.control_left,
                               m.control_break_on_damage,
                               m.open_player_id,
                               m.open_left,
                               m.open_remove_left,
                               m.gstate,
                               m.knee_left,
                               m.die_remove_left,
                               m.fire_flower_explode_left,
                               m.fire_flower_skill_fire_left,
                               m.fire_flower_remove_left,
                               m.fire_flower_exploded,
                               m.fire_flower_removed,
                               m.slow_left,
                               m.slow_percent,
                               m.dirty_slow,
                               m.path_waypoints);
    }

    struct TickEvents {
        std::vector<MonsterMoveEvent> moves;
        std::vector<MonsterAttackEvent> attacks;
        std::vector<ChestOpenEvent> chests;
        std::vector<MonsterCorpseEvent> corpses;
    };

    auto event_values(const TickEvents& events) {
        std::vector<std::tuple<std::uint32_t, int, int, int, int, bool, bool>> moves;
        std::vector<
            std::tuple<MonsterAttackEvent::Kind, std::uint32_t, std::uint32_t, int, int, int>>
            attacks;
        std::vector<std::tuple<ChestOpenEvent::Kind, std::uint32_t, std::uint32_t, int, int, int>>
            chests;
        std::vector<std::pair<MonsterCorpseEvent::Kind, std::uint32_t>> corpses;
        for (const auto& e : events.moves) {
            moves.emplace_back(e.id, e.x, e.y, e.dx, e.dy, e.ended, e.run);
        }
        for (const auto& e : events.attacks) {
            attacks.emplace_back(
                e.kind, e.attacker_id, e.target_player_id, e.skill_index, e.skill_level, e.damage);
        }
        for (const auto& e : events.chests) {
            chests.emplace_back(e.kind, e.monster_id, e.player_id, e.def_index, e.x, e.y);
        }
        for (const auto& e : events.corpses) {
            corpses.emplace_back(e.kind, e.monster_id);
        }
        return std::make_tuple(moves, attacks, chests, corpses);
    }

    void test_player_selection(const std::string& root) {
        GameWorld world;
        expect(world.load_maps_around(root, 512, 512, 0), "load owned target selection map");
        TickEvents events;
        const auto tick = [&](const std::vector<PlayerPose>& players) {
            world.tick(0.05f, players, events.moves, events.attacks, events.chests, events.corpses);
        };
        auto monster = body(1, 512, 512);
        monster.aggressive = true;
        world.monsters() = {monster};
        tick({{1, 512, 512, 1}, {2, 512, 512, 0, true}, {3, 560, 512, 0}, {4, 464, 512, 0}});
        expect(world.monsters()[0].aggro_player_id == 3,
               "nearest-player ties preserve input order after map and immunity filtering");

        monster.aggressive = false;
        world.monsters() = {monster};
        tick({{1, 512, 512, 1, false, true},
              {2, 512, 512, 0, true, true},
              {3, 560, 512, 0, false, true},
              {4, 544, 512, 0, false, true}});
        expect(world.monsters()[0].beacon_pursuit && world.monsters()[0].aggro_player_id == 3,
               "beacon uses the first eligible pose, even when a later pose is closer");

        monster.aggro = true;
        monster.aggro_player_id = 6;
        world.monsters() = {monster};
        tick({{6, 544, 512, 1}, {6, 544, 512, 0}});
        expect(!world.monsters()[0].aggro,
               "an existing target resolves the first matching ID before checking its map");

        auto flower = body(2, 512, 512);
        flower.race = static_cast<int>(protocol::MonsterRace::FireFlower);
        flower.fire_flower_exploded = true;
        flower.fire_flower_skill_fire_left = 0.05f;
        flower.fire_flower_remove_left = 1.0f;
        world.monsters() = {flower};
        tick({{5, 512, 512, 1}, {4, 520, 512, 0}, {3, 512, 512, 0, true}, {2, 528, 512, 0}});
        expect(events.attacks.size() == 2 && events.attacks[0].target_player_id == 4 &&
                   events.attacks[1].target_player_id == 2,
               "fire flower recipients retain map filtering, immunity and input order");
    }

    void test_repeated_path_searches() {
        using Probe = std::array<int, 4>;
        const auto search = [](std::vector<Probe>& probes, bool nest) {
            return find_monster_path(0, 0, 192, 0, 0, [&](int x, int y, int dx, int dy) {
                probes.push_back({x, y, dx, dy});
                if (nest && probes.size() == 2) {
                    const auto blocked = find_monster_path(
                        -64, -64, 64, 64, 0, [](int, int, int, int) { return false; });
                    expect(!blocked.found && blocked.waypoints.empty(),
                           "a nested blocked path does not return stale waypoints");
                }
                return !(x < 32 && x + dx >= 32 && y >= -32 && y <= 32 && y + dy >= -32 &&
                         y + dy <= 32);
            });
        };
        std::vector<Probe> first_probes;
        const auto first = search(first_probes, false);
        expect(first.found && first.waypoints.size() > 1 && !first_probes.empty(),
               "the bounded path search finds a detour around the obstacle");
        for (bool nest : {true, false}) {
            std::vector<Probe> probes;
            const auto result = search(probes, nest);
            expect(result.found == first.found && result.waypoints == first.waypoints &&
                       probes == first_probes,
                   "repeated and nested searches preserve probes and waypoint order");
        }
    }

    void test_tick_trajectories(const std::string& root) {
        GameWorld legacy, indexed;
        expect(legacy.load_maps_around(root, 512, 512, 0) &&
                   indexed.load_maps_around(root, 512, 512, 0),
               "load two independently owned maps for full trajectory comparison");
        GameWorldCoveragePeer::enable_occupancy_index(legacy, false);
        std::vector<PlayerPose> players{{90, 576, 512, 0}, {91, 640, 640, 0}, {92, 576, 512, 1}};
        for (int i = 0; i < 16; ++i) {
            auto m = body(static_cast<std::uint32_t>(i + 1), 512 + i % 5 * 16, 512 + i / 5 * 16);
            m.roam_x0 = m.roam_y0 = 400;
            m.roam_x1 = m.roam_y1 = 800;
            m.aggro = i < 6;
            m.aggro_player_id = m.aggro ? 90 : 0;
            m.ai = m.aggro ? MonsterEntity::Ai::Chase : MonsterEntity::Ai::Roam;
            if (m.aggro) {
                m.hostility_by_player[90] = 1;
                m.aggro_hostility = 1;
            }
            legacy.monsters().push_back(m);
        }
        auto& monsters = legacy.monsters();
        monsters[0].target_x = monsters[1].x;
        monsters[0].dirty_move = monsters[0].moving = true;
        monsters[0].move_busy_left = 0.05f;
        monsters[1].target_x = monsters[0].x;
        monsters[1].dirty_move = monsters[1].moving = true;
        monsters[1].move_busy_left = 0.05f;
        monsters[2].x = monsters[2].target_x = 568;
        monsters[2].moving = true;
        monsters[2].move_busy_left = 0.05f;
        monsters[3].dirty_stop = monsters[3].dirty_move = monsters[3].moving = true;
        monsters[3].target_x -= 8;
        monsters[4].control_left = 0.15f;
        monsters[4].dirty_stop = monsters[4].moving = true;
        monsters[4].target_x += 16;
        monsters[6].race = static_cast<int>(protocol::MonsterRace::Item);
        monsters[6].open_player_id = 90;
        monsters[6].open_left = 0.05f;
        monsters[7].hp = 0;
        monsters[7].knee_left = 0.05f;
        monsters[8].hp = 0;
        monsters[8].die_remove_left = 0.05f;
        monsters[9].race = static_cast<int>(protocol::MonsterRace::Magic);
        monsters[9].dirty_stop = monsters[9].moving = true;
        monsters[10].race = static_cast<int>(protocol::MonsterRace::FireFlower);
        monsters[10].fire_flower_explode_left = 0.05f;
        monsters[11].hp = 0;
        monsters[11].open_player_id = 90;
        monsters[11].open_remove_left = 0.05f;
        indexed.monsters() = legacy.monsters();
        bool equal = true, arrival_waited = false, attacked_next = false;
        std::size_t total_moves = 0, total_attacks = 0, total_chests = 0, total_corpses = 0;
        for (int tick = 0; tick < 160; ++tick) {
            if (tick == 20) {
                players.front().x += 32;
            }
            if (tick == 40) {
                players.front().aggro_immune = true;
            }
            if (tick == 50) {
                players.front().aggro_immune = false;
            }
            if (tick == 60) {
                legacy.monsters().push_back(body(99, 560, 560));
                indexed.monsters().push_back(body(99, 560, 560));
            }
            if (tick == 80) {
                legacy.monsters().back().hp = 0;
                indexed.monsters().back().hp = 0;
            }
            TickEvents expected, actual;
            std::srand(1000 + tick);
            legacy.tick(0.05f,
                        players,
                        expected.moves,
                        expected.attacks,
                        expected.chests,
                        expected.corpses);
            std::srand(1000 + tick);
            indexed.tick(
                0.05f, players, actual.moves, actual.attacks, actual.chests, actual.corpses);
            equal = equal && event_values(expected) == event_values(actual) &&
                    legacy.monsters().size() == indexed.monsters().size() &&
                    GameWorldCoveragePeer::occupancy_inactive(indexed);
            for (std::size_t i = 0; i < legacy.monsters().size(); ++i) {
                equal = equal &&
                        monster_state(legacy.monsters()[i]) == monster_state(indexed.monsters()[i]);
            }
            const auto attack = [&](std::uint32_t id) {
                return std::any_of(actual.attacks.begin(),
                                   actual.attacks.end(),
                                   [&](const auto& e) { return e.attacker_id == id; });
            };
            if (tick == 0) {
                arrival_waited = !attack(3);
            }
            if (tick == 1) {
                attacked_next = attack(3);
            }
            total_moves += actual.moves.size();
            total_attacks += actual.attacks.size();
            total_chests += actual.chests.size();
            total_corpses += actual.corpses.size();
        }
        expect(equal,
               "160 indexed and legacy ticks preserve complete trajectories, timers, hostility and "
               "every emitted event");
        expect(arrival_waited && attacked_next,
               "movement arrival waits until the following tick before committing an attack");
        expect(total_moves && total_attacks && total_chests && total_corpses,
               "trajectory oracle exercises movement, attacks, opening death and corpse early "
               "continuations");
        legacy.monsters().clear();
        indexed.monsters().clear();
        expect(!GameWorldCoveragePeer::blocked(indexed, 0, 512, 512, 0, nullptr),
               "public mutable world changes after a tick use the full-scan predicate without "
               "stale reservations");

        // The first occupancy query occurs after dirty_stop changed the first
        // monster's old target. A lazy snapshot must capture that current state.
        players = {{90, 576, 512, 0}, {91, 480, 512, 0}};
        auto interrupted = body(1, 512, 512);
        interrupted.target_x = 640;
        interrupted.dirty_stop = interrupted.dirty_move = interrupted.moving = true;
        interrupted.aggro = true;
        interrupted.aggro_player_id = 90;
        interrupted.ai = MonsterEntity::Ai::Chase;
        auto follower = body(2, 528, 512);
        follower.aggro = true;
        follower.aggro_player_id = 91;
        follower.ai = MonsterEntity::Ai::Chase;
        legacy.monsters() = {interrupted, follower};
        indexed.monsters() = legacy.monsters();
        bool lazy_equal = true;
        for (int tick = 0; tick < 8; ++tick) {
            TickEvents expected, actual;
            std::srand(8000 + tick);
            legacy.tick(0.05f,
                        players,
                        expected.moves,
                        expected.attacks,
                        expected.chests,
                        expected.corpses);
            std::srand(8000 + tick);
            indexed.tick(
                0.05f, players, actual.moves, actual.attacks, actual.chests, actual.corpses);
            lazy_equal =
                lazy_equal && event_values(expected) == event_values(actual) &&
                monster_state(legacy.monsters()[0]) == monster_state(indexed.monsters()[0]) &&
                monster_state(legacy.monsters()[1]) == monster_state(indexed.monsters()[1]) &&
                GameWorldCoveragePeer::occupancy_inactive(indexed);
        }
        expect(lazy_equal,
               "lazy first-query construction resynchronizes dirty-stop changes before publishing "
               "the next reservation");
    }

    void benchmark_occupancy(const std::string& root) {
        using Clock = std::chrono::steady_clock;
        constexpr int samples = 25;
        const auto percentile = [](std::vector<double> values, std::size_t percentage) {
            std::sort(values.begin(), values.end());
            return values[(values.size() * percentage + 99) / 100 - 1];
        };
        std::cout << "# CPU-only bounded path searches; indexed timings include a complete "
                     "occupancy rebuild.\n"
                  << "# Candidate columns are modeled upper bounds from recorded step coordinates, "
                     "not measured scan visits.\n"
                  << "monsters,players,samples,step_callbacks,waypoints,legacy_p50_us,legacy_p95_"
                     "us,legacy_p99_us,"
                     "indexed_p50_us,indexed_p95_us,indexed_p99_us,linear_candidate_upper_bound,"
                     "indexed_candidate_upper_bound\n";
        for (int count : {32, 128, 256, 1024}) {
            for (int player_count : {1, 16}) {
                GameWorld world;
                expect(world.load_maps_around(root, 512, 512, 0), "load owned path benchmark area");
                for (int i = 0; i < count; ++i) {
                    world.monsters().push_back(body(
                        static_cast<std::uint32_t>(i + 1), 512 + i % 32 * 8, 512 + i / 32 * 8));
                }
                std::vector<PlayerPose> players;
                for (int i = 0; i < player_count; ++i) {
                    players.push_back(
                        {static_cast<std::uint32_t>(10000 + i), 1200 + i * 8, 1200, 0});
                }
                const auto search = [&](std::vector<std::array<int, 4>>* queries = nullptr) {
                    return find_monster_path(
                        480, 480, 800, 800, 16 * 16, [&](int x, int y, int dx, int dy) {
                            if (queries) {
                                queries->push_back({x, y, dx, dy});
                            }
                            return GameWorldCoveragePeer::move(world, 0, x, y, dx, dy, 0, players);
                        });
                };
                std::vector<std::array<int, 4>> legacy_queries, indexed_queries;
                const auto expected = search(&legacy_queries);
                WorldOccupancyIndex index;
                index.rebuild(world.monsters(), players);
                const auto actual = GameWorldCoveragePeer::with_occupancy(
                    world, index, players, [&] { return search(&indexed_queries); });
                expect(actual.found == expected.found && actual.waypoints == expected.waypoints &&
                           indexed_queries == legacy_queries && !legacy_queries.empty(),
                       "dense path benchmark preserves every probe, node order and path waypoint");
                std::map<std::pair<int, int>, std::size_t> populations;
                for (const auto& m : world.monsters()) {
                    ++populations[{cell(m.x), cell(m.y)}];
                }
                for (const auto& p : players) {
                    ++populations[{cell(p.x), cell(p.y)}];
                }
                std::size_t sample_positions = 0, indexed_upper_bound = 0;
                for (const auto& query : legacy_queries) {
                    const int length = std::max(std::abs(query[2]), std::abs(query[3]));
                    if (!length) {
                        continue;
                    }
                    const auto add = [&](int x, int y) {
                        ++sample_positions;
                        indexed_upper_bound += populations[{cell(x), cell(y)}];
                    };
                    for (int step = 8; step <= 32; step += 8) {
                        const bool more = step < length;
                        add(query[0] + (more ? query[2] * step / length : query[2]),
                            query[1] + (more ? query[3] * step / length : query[3]));
                        if (!more) {
                            break;
                        }
                    }
                    if (length > 32) {
                        add(query[0] + query[2], query[1] + query[3]);
                    }
                }
                std::array<std::vector<double>, 2> timings;
                bool equal = true;
                for (int sample = 0; sample < samples; ++sample) {
                    for (int pass = 0; pass < 2; ++pass) {
                        const int variant = (sample + pass) % 2;
                        const auto start = Clock::now();
                        FindPathResult result;
                        if (variant == 0) {
                            result = search();
                        } else {
                            WorldOccupancyIndex timed_index;
                            timed_index.rebuild(world.monsters(), players);
                            result = GameWorldCoveragePeer::with_occupancy(
                                world, timed_index, players, [&] { return search(); });
                        }
                        timings[variant].push_back(
                            std::chrono::duration<double, std::micro>(Clock::now() - start)
                                .count());
                        equal = equal && result.found == expected.found &&
                                result.waypoints == expected.waypoints;
                    }
                }
                expect(equal,
                       "every timed path search matches the independently recorded legacy result");
                std::cout << count << ',' << player_count << ',' << samples << ','
                          << legacy_queries.size() << ',' << expected.waypoints.size();
                for (int variant = 0; variant < 2; ++variant) {
                    std::cout << ',' << std::fixed << std::setprecision(3)
                              << percentile(timings[variant], 50) << ','
                              << percentile(timings[variant], 95) << ','
                              << percentile(timings[variant], 99);
                }
                std::cout << ',' << sample_positions * (world.monsters().size() + players.size())
                          << ',' << indexed_upper_bound << '\n';
            }
        }

        GameWorld legacy, indexed;
        expect(legacy.load_maps_around(root, 512, 512, 0) &&
                   indexed.load_maps_around(root, 512, 512, 0),
               "load idle world benchmark maps");
        GameWorldCoveragePeer::enable_occupancy_index(legacy, false);
        for (int i = 0; i < 256; ++i) {
            auto m = body(static_cast<std::uint32_t>(i + 1), 512 + i % 32 * 8, 512 + i / 32 * 8);
            m.idle_left = 100;
            legacy.monsters().push_back(m);
        }
        indexed.monsters() = legacy.monsters();
        const std::vector<PlayerPose> players;
        std::array<std::vector<double>, 2> timings;
        bool equal = true;
        for (int sample = 0; sample < samples; ++sample) {
            for (int pass = 0; pass < 2; ++pass) {
                const int variant = (sample + pass) % 2;
                auto& world = variant == 0 ? legacy : indexed;
                TickEvents events;
                const auto start = Clock::now();
                world.tick(
                    0.05f, players, events.moves, events.attacks, events.chests, events.corpses);
                timings[variant].push_back(
                    std::chrono::duration<double, std::micro>(Clock::now() - start).count());
                equal = equal && events.moves.empty() && events.attacks.empty() &&
                        events.chests.empty() && events.corpses.empty() &&
                        GameWorldCoveragePeer::occupancy_inactive(world);
            }
        }
        for (std::size_t i = 0; i < legacy.monsters().size(); ++i) {
            equal = equal &&
                    monster_state(legacy.monsters()[i]) == monster_state(indexed.monsters()[i]);
        }
        expect(equal,
               "idle 256-monster ticks preserve state and emit no occupancy-dependent output");
        std::cout << "# Full 256-monster idle ticks, including production lazy-scope overhead.\n"
                  << "idle_monsters,samples,legacy_p50_us,legacy_p95_us,legacy_p99_us,indexed_p50_"
                     "us,indexed_p95_us,indexed_p99_us\n"
                  << 256 << ',' << samples;
        for (int variant = 0; variant < 2; ++variant) {
            std::cout << ',' << percentile(timings[variant], 50) << ','
                      << percentile(timings[variant], 95) << ','
                      << percentile(timings[variant], 99);
        }
        std::cout << '\n';
    }
}  // namespace

int main(int argc, char** argv) {
    const auto root =
        openkal::test::temp_dir(("optimization_world_" + std::to_string(getpid())).c_str());
    expect(openkal::test::write_open_area(root + "/n_000_000.akarea"), "write owned movement area");
    if (argc > 1 && std::string_view(argv[1]) == "--benchmark") {
        benchmark_occupancy(root);
        std::filesystem::remove_all(root);
        return openkal::test::g_failures ? 1 : 0;
    }
    test_occupancy();
    test_indexed_occupancy();
    test_moves(root);
    test_spawnable_terrain();
    test_tick_trajectories(root);
    test_player_selection(root);
    test_repeated_path_searches();
    std::filesystem::remove_all(root);
    return openkal::test::g_failures ? 1 : 0;
}
