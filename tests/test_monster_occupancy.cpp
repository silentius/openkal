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
#include "openkal/map/Coords.h"
#include "openkal/platform/Path.h"
#include "openkal/protocol/ItemMonster.h"
#include "openkal/protocol/MonsterRace.h"
#include "openkal/server/world/GameWorld.h"
#include "test_util.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <set>
#include <string>
#include <utility>
#include <vector>

int main() {
    using openkal::server::GameWorld;
    using openkal::server::MonsterAttackEvent;
    using openkal::server::MonsterCorpseEvent;
    using openkal::server::MonsterEntity;
    using openkal::server::MonsterMoveEvent;
    using openkal::server::PlayerPose;
    using openkal::test::expect;

    constexpr int kPlayerX = 258576;
    constexpr int kPlayerY = 261408;
    int spawn_tx = 0;
    int spawn_ty = 0;
    openkal::coords::pixel_to_tile(kPlayerX, kPlayerY, spawn_tx, spawn_ty);
    int cell_x = 0;
    int cell_y = 0;
    int lx = 0;
    int ly = 0;
    openkal::coords::world_tile_to_cell(spawn_tx, spawn_ty, cell_x, cell_y, lx, ly);

    const std::string maps = openkal::test::temp_dir("monster_occupancy_maps");
    for (int dy = -2; dy <= 2; ++dy) {
        for (int dx = -2; dx <= 2; ++dx) {
            char name[64];
            std::snprintf(name, sizeof(name), "n_%03d_%03d.akarea", cell_x + dx, cell_y + dy);
            expect(openkal::test::write_open_area(openkal::platform::join_path(maps, name)),
                   "write open area map");
        }
    }

    GameWorld world;
    expect(world.load_maps_around(maps, kPlayerX, kPlayerY, 2), "load occupancy maps");

    constexpr int kPack = 8;
    constexpr int kStartX = kPlayerX - 192;
    constexpr int kStartY = kPlayerY;
    for (int i = 0; i < kPack; ++i) {
        MonsterEntity m;
        m.id = static_cast<std::uint32_t>(i + 1);
        m.x = kStartX;
        m.y = kStartY;
        m.target_x = kStartX;
        m.target_y = kStartY;
        m.map = 0;
        m.hp = 100;
        m.max_hp = 100;
        m.race = static_cast<int>(openkal::protocol::MonsterRace::Maguni);
        m.aggressive = true;
        m.detect_range = 400;
        m.sight_range = 800;
        m.attack_range = 20;
        m.chase_speed = 80.0f;
        m.roam_speed = 20.0f;
        m.roam_x0 = kStartX - 256;
        m.roam_y0 = kStartY - 256;
        m.roam_x1 = kPlayerX + 256;
        m.roam_y1 = kStartY + 256;
        m.idle_left = 0.0f;
        world.monsters().push_back(m);
    }

    PlayerPose prey{1, kPlayerX, kPlayerY, 0};
    bool any_move = false;
    for (int i = 0; i < 240; ++i) {
        std::vector<MonsterMoveEvent> moves;
        std::vector<MonsterAttackEvent> attacks;
        world.tick(0.05f, {prey}, moves, attacks);
        if (!moves.empty()) {
            any_move = true;
        }
    }
    expect(any_move, "packed chase produced movement");

    std::set<std::pair<int, int>> cells;
    int near_player = 0;
    std::set<std::pair<int, int>> pixels;
    for (const auto& m : world.monsters()) {
        if (m.hp <= 0) {
            continue;
        }
        int ox = m.x;
        int oy = m.y;
        if (m.dirty_move || m.moving) {
            ox = m.target_x;
            oy = m.target_y;
        }
        cells.emplace(ox >> 3, oy >> 3);
        pixels.emplace(ox, oy);
        const long long dx = static_cast<long long>(ox) - kPlayerX;
        const long long dy = static_cast<long long>(oy) - kPlayerY;
        if (dx * dx + dy * dy <= 96LL * 96LL) {
            ++near_player;
        }
    }
    const int unique_pixels = static_cast<int>(pixels.size());
    expect(static_cast<int>(cells.size()) == kPack, "chasing monsters occupy distinct 8px cells");
    expect(unique_pixels == kPack, "chasing monsters do not share a pixel");
    expect(near_player >= 4, "several monsters closed to melee around the player");

    {
        GameWorld blocked_world;
        expect(blocked_world.load_maps_around(maps, kPlayerX, kPlayerY, 2),
               "load temporarily blocked chase maps");
        MonsterEntity chaser = world.monsters().front();
        chaser.id = 100;
        chaser.x = kPlayerX - 96;
        chaser.y = kPlayerY;
        chaser.hp = chaser.max_hp = 100;
        chaser.moving = false;
        chaser.move_busy_left = 0;
        chaser.attack_swinging = false;
        chaser.path_waypoints.clear();
        chaser.hostility_by_player.clear();
        chaser.reward_damage_by_player.clear();
        blocked_world.monsters().push_back(chaser);
        expect(blocked_world.apply_player_attack(prey.id, chaser.id, 60, prey.x, prey.y) == 60,
               "initial area hit earns reward damage");
        expect(blocked_world.apply_monster_slow(chaser.id, 5.0f, -50),
               "ice slow remains active during a blocked chase");
        std::vector<PlayerPose> crowded{prey};
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (dx != 0 || dy != 0) {
                    crowded.push_back({static_cast<std::uint32_t>(crowded.size() + 1),
                                       chaser.x + dx * 8,
                                       chaser.y + dy * 8,
                                       0});
                }
            }
        }
        std::vector<MonsterMoveEvent> moves;
        std::vector<MonsterAttackEvent> attacks;
        blocked_world.tick(0.05f, crowded, moves, attacks);
        const auto& waiting = blocked_world.monsters().front();
        expect(!waiting.moving && waiting.aggro && waiting.aggro_player_id == prey.id,
               "occupied routes make a monster wait without forgetting its target");
        expect(waiting.reward_damage_by_player.count(prey.id) == 1 &&
                   waiting.reward_damage_by_player.at(prey.id) == 60,
               "temporarily failed chase preserves the first area hit's reward claim");
        blocked_world.tick(0.3f, {prey}, moves, attacks);
        expect(blocked_world.monsters().front().moving && blocked_world.monsters().front().aggro,
               "chase resumes after the blocking characters leave");
        expect(blocked_world.apply_player_attack(prey.id, chaser.id, 40, prey.x, prey.y) == 40,
               "follow-up area hit kills the waiting monster");
        const auto coverage = openkal::server::calculate_monster_reward_coverage(
            blocked_world.monsters().front(), 100);
        expect(coverage.credited_damage == 100 &&
                   coverage.loot_access == openkal::server::MonsterLootAccess::HostilityOwner,
               "both hits count toward owned loot after a blocked chase");
    }

    {
        const std::string safe_maps = openkal::test::temp_dir("monster_safe_maps");
        for (int dy = -2; dy <= 2; ++dy) {
            for (int dx = -2; dx <= 2; ++dx) {
                char name[64];
                std::snprintf(name, sizeof(name), "n_%03d_%03d.akarea", cell_x + dx, cell_y + dy);
                expect(
                    openkal::test::write_open_area(openkal::platform::join_path(safe_maps, name)),
                    "write safe-zone test area map");
            }
        }

        openkal::map::AreaCell safe_cell;
        safe_cell.set_version(openkal::map::kVersion);
        safe_cell.assign_attrs(
            static_cast<std::size_t>(openkal::map::kTileCount) * openkal::map::kTileCount, 0);
        for (int safe_y = ly - 2; safe_y <= ly + 2; ++safe_y) {
            for (int safe_x = lx - 2; safe_x <= lx + 2; ++safe_x) {
                safe_cell.attr_ref(safe_x, safe_y) = openkal::map::kAttrVillage;
            }
        }
        char safe_name[64];
        std::snprintf(safe_name, sizeof(safe_name), "n_%03d_%03d.akarea", cell_x, cell_y);
        safe_cell.ensure_blocked_mask(false);
        safe_cell.ensure_not_moveable_mask(false);
        expect(openkal::map::AreaMapCodec::save(
                   openkal::test::area_fixture_path(
                       openkal::platform::join_path(safe_maps, safe_name)),
                   safe_cell),
               "write village safe-zone tile");

        GameWorld safe_world;
        expect(safe_world.load_maps_around(safe_maps, kPlayerX, kPlayerY, 2),
               "load safe-zone maps");
        expect(safe_world.walkable_pixel(kPlayerX, kPlayerY), "safe-zone tile remains walkable");
        expect(!safe_world.monster_spawnable_pixel(kPlayerX, kPlayerY),
               "safe-zone tile blocks monsters");

        MonsterEntity chaser;
        chaser.id = 100;
        chaser.x = kPlayerX - 96;
        chaser.y = kPlayerY;
        chaser.target_x = kPlayerX;
        chaser.target_y = kPlayerY;
        chaser.map = 0;
        chaser.hp = 100;
        chaser.max_hp = 100;
        chaser.race = static_cast<int>(openkal::protocol::MonsterRace::Maguni);
        chaser.aggressive = true;
        chaser.detect_range = 400;
        chaser.sight_range = 800;
        chaser.attack_range = 20;
        chaser.aggro = true;
        chaser.aggro_player_id = prey.id;
        chaser.aggro_hostility = 40;
        chaser.hostility_by_player.emplace(prey.id, 40);
        chaser.reward_damage_by_player.emplace(prey.id, 40);
        chaser.ai = MonsterEntity::Ai::Chase;
        chaser.roam_x0 = kPlayerX - 256;
        chaser.roam_y0 = kPlayerY - 128;
        chaser.roam_x1 = kPlayerX - 96;
        chaser.roam_y1 = kPlayerY + 128;
        safe_world.monsters().push_back(chaser);

        std::vector<MonsterMoveEvent> safe_moves;
        std::vector<MonsterAttackEvent> safe_attacks;
        bool chase_stopped = false;
        for (int i = 0; i < 10 && safe_world.monsters().front().aggro; ++i) {
            safe_world.tick(1.0f, {prey}, safe_moves, safe_attacks);
            chase_stopped = chase_stopped ||
                            std::any_of(safe_moves.begin(), safe_moves.end(), [](const auto& move) {
                                return move.ended;
                            });
        }
        const MonsterEntity& idle = safe_world.monsters().front();
        expect(!idle.aggro && idle.aggro_player_id == 0,
               "monster drops a target inside a safe zone");
        expect(idle.aggro_hostility == 0 && idle.hostility_by_player.empty() &&
                   idle.reward_damage_by_player.empty(),
               "losing safe-zone aggro clears all reward hostility");
        expect(idle.ai == MonsterEntity::Ai::Roam && idle.moving && !idle.move_is_run,
               "monster immediately starts a roaming walk after losing aggro");
        expect(chase_stopped, "safe-zone aggro drop stops the active chase");
        expect(std::any_of(safe_moves.begin(),
                           safe_moves.end(),
                           [](const auto& move) { return !move.ended && !move.run; }),
               "aggro drop emits a walking roam step");
        expect(safe_attacks.empty(), "monster does not attack a player inside a safe zone");

        safe_moves.clear();
        safe_world.tick(4.0f, {prey}, safe_moves, safe_attacks);
        expect(!safe_world.monsters().front().aggro,
               "failed reacquired chase returns to roaming again");
        expect(safe_world.monsters().front().detect_cooldown > 0.0f,
               "safe-zone player presence retriggers aggro after the roaming pause");
        expect(std::any_of(safe_moves.begin(),
                           safe_moves.end(),
                           [](const auto& move) { return move.ended; }),
               "retriggered unreachable chase is stopped again");
        expect(safe_world.apply_player_attack(prey.id, chaser.id, 100, kPlayerX, kPlayerY) == 100,
               "safe-zone attack may still deal lethal damage");
        expect(safe_world.monsters().front().hp == 0 &&
                   safe_world.monsters().front().hostility_by_player.empty() &&
                   safe_world.monsters().front().reward_damage_by_player.empty(),
               "safe-zone lethal damage after aggro loss creates no reward claim");
    }

    const std::string monster_defs = openkal::test::temp_dir("monster_skill_defs");
    {
        std::ofstream out(openkal::platform::join_path(monster_defs, "9000_test_mage.json"));
        expect(static_cast<bool>(out), "create monster skill fixture");
        out << R"({
  "index": 9000,
  "name": "Test Demon Worker",
  "stats": {
    "race": 0,
    "level": 50,
    "ai": 4,
    "range": 80,
    "int": 30,
    "hp": 1400,
    "aspeed": 1600,
    "sight": [150, 340],
    "mspeed": [1600, 500],
    "attack": { "type": 0, "min": 310, "max": 380 },
    "skill": [29, 1]
  }
})";
    }
    {
        std::ofstream out(openkal::platform::join_path(monster_defs, "9001_test_basic.json"));
        expect(static_cast<bool>(out), "create no-skill monster fixture");
        out << R"({
  "index": 9001,
  "name": "Test Basic Monster",
  "stats": {
    "race": 0,
    "level": 50,
    "ai": 3,
    "range": 80,
    "hp": 1400,
    "aspeed": 1600,
    "sight": [150, 340],
    "mspeed": [1600, 500],
    "attack": { "type": 0, "min": 310, "max": 380 }
  }
})";
    }
    {
        std::ofstream out(openkal::platform::join_path(monster_defs, "9002_test_skill_zero.json"));
        expect(static_cast<bool>(out), "create skill-zero monster fixture");
        out << R"({
  "index": 9002,
  "name": "Test Rage Monster",
  "stats": {
    "race": 0,
    "level": 50,
    "ai": 3,
    "range": 80,
    "hp": 1400,
    "aspeed": 1600,
    "sight": [150, 340],
    "mspeed": [1600, 500],
    "attack": { "type": 0, "min": 310, "max": 380 },
    "skill": [0, 1]
  }
})";
    }
    {
        std::ofstream out(openkal::platform::join_path(monster_defs, "9003_test_flame_plant.json"));
        expect(static_cast<bool>(out), "create flame-plant monster fixture");
        out << R"({
  "index": 9003,
  "name": "Test Flame Plant",
  "stats": {
    "race": 9,
    "level": 70,
    "hp": 1500,
    "attack": { "type": 0, "min": 1100, "max": 1700 },
    "skill": [34, 1]
  }
})";
    }

    GameWorld skill_world;
    expect(skill_world.load_maps_around(maps, kPlayerX, kPlayerY, 2),
           "load maps for monster skill fixture");
    expect(skill_world.load_skills_json_dir(std::string(OPENKAL_TEST_RESOURCE_ROOT) +
                                            "/config/skills"),
           "load monster skill catalog");
    expect(skill_world.load_monsters_json_dir(monster_defs, kPlayerX, kPlayerY, 0),
           "load Demon Worker skill fixture");
    std::uint32_t monster_id = 100;
    expect(
        skill_world.spawn_monster_at(monster_id, 9000, 0, kPlayerX - 64, kPlayerY, 0).has_value(),
        "spawn Demon Worker skill fixture");

    std::vector<MonsterMoveEvent> skill_moves;
    std::vector<MonsterAttackEvent> skill_events;
    skill_world.tick(0.05f, {prey}, skill_moves, skill_events);
    expect(std::any_of(skill_events.begin(),
                       skill_events.end(),
                       [](const MonsterAttackEvent& event) {
                           return event.kind == MonsterAttackEvent::Kind::SkillPrepare &&
                                  event.skill_index == 29 && event.skill_level == 1;
                       }),
           "AI 4 Demon Worker prepares its configured Thunder Strike");

    skill_world.tick(1.0f, {prey}, skill_moves, skill_events);
    expect(std::any_of(skill_events.begin(),
                       skill_events.end(),
                       [](const MonsterAttackEvent& event) {
                           return event.kind == MonsterAttackEvent::Kind::SkillFire &&
                                  event.skill_index == 29 && event.skill_level == 1;
                       }),
           "AI 4 Demon Worker executes Thunder Strike after its precast");

    GameWorld zero_skill_world;
    expect(zero_skill_world.load_maps_around(maps, kPlayerX, kPlayerY, 2),
           "load maps for skill-zero monster fixture");
    expect(zero_skill_world.load_skills_json_dir(std::string(OPENKAL_TEST_RESOURCE_ROOT) +
                                                 "/config/skills"),
           "load skill catalog for skill-zero monster fixture");
    expect(zero_skill_world.load_monsters_json_dir(monster_defs, kPlayerX, kPlayerY, 0),
           "load skill-zero monster fixture");
    std::uint32_t zero_skill_id = 150;
    expect(zero_skill_world.spawn_monster_at(zero_skill_id, 9002, 0, kPlayerX - 64, kPlayerY, 0)
               .has_value(),
           "spawn skill-zero monster fixture");
    zero_skill_world.tick(0.05f, {prey}, skill_moves, skill_events);
    expect(std::any_of(skill_events.begin(),
                       skill_events.end(),
                       [](const MonsterAttackEvent& event) {
                           return event.kind == MonsterAttackEvent::Kind::SkillPrepare &&
                                  event.skill_index == 0 && event.skill_level == 1;
                       }),
           "valid monster skill 0 is not discarded");

    GameWorld flame_plant_world;
    expect(flame_plant_world.load_maps_around(maps, kPlayerX, kPlayerY, 2),
           "load maps for flame-plant fixture");
    expect(flame_plant_world.load_skills_json_dir(std::string(OPENKAL_TEST_RESOURCE_ROOT) +
                                                  "/config/skills"),
           "load skill catalog for flame-plant fixture");
    expect(flame_plant_world.load_monsters_json_dir(monster_defs, kPlayerX, kPlayerY, 0),
           "load flame-plant fixture");
    std::uint32_t flame_plant_id_gen = 175;
    const auto spawned_flame_plant = flame_plant_world.spawn_monster_at(
        flame_plant_id_gen, 9003, 0, kPlayerX + 128, kPlayerY, 0);
    expect(spawned_flame_plant.has_value(), "spawn flame-plant fixture");
    const std::uint32_t flame_plant_id = spawned_flame_plant.value_or(0);
    const MonsterEntity* flame_plant = flame_plant_world.find_monster(flame_plant_id);
    expect(flame_plant != nullptr && (flame_plant->gstate & openkal::protocol::kCgsInvincible) != 0,
           "flame plant is created invincible");

    std::vector<openkal::server::ChestOpenEvent> flower_chests;
    std::vector<MonsterCorpseEvent> flower_corpses;
    flame_plant_world.tick(11.9f, {prey}, skill_moves, skill_events, flower_chests, flower_corpses);
    expect(skill_events.empty(), "flame plant waits twelve seconds before exploding");
    flame_plant_world.tick(0.2f, {prey}, skill_moves, skill_events, flower_chests, flower_corpses);
    expect(std::any_of(skill_events.begin(),
                       skill_events.end(),
                       [](const MonsterAttackEvent& event) {
                           return event.kind == MonsterAttackEvent::Kind::SkillPrepare &&
                                  event.skill_index == 34 && event.target_player_id == 0;
                       }),
           "flame plant prepares its self-centered explosion");
    flame_plant_world.tick(4.0f, {prey}, skill_moves, skill_events, flower_chests, flower_corpses);
    expect(std::any_of(skill_events.begin(),
                       skill_events.end(),
                       [id = prey.id](const MonsterAttackEvent& event) {
                           return event.kind == MonsterAttackEvent::Kind::SkillFire &&
                                  event.skill_index == 34 && event.target_player_id == id;
                       }),
           "flame plant damages players in its five-tile explosion radius");
    flame_plant_world.tick(3.0f, {prey}, skill_moves, skill_events, flower_chests, flower_corpses);
    expect(std::any_of(flower_corpses.begin(),
                       flower_corpses.end(),
                       [id = flame_plant_id](const MonsterCorpseEvent& event) {
                           return event.kind == MonsterCorpseEvent::Kind::Die &&
                                  event.monster_id == id;
                       }),
           "flame plant dies seven seconds after starting its explosion");

    GameWorld basic_world;
    expect(basic_world.load_maps_around(maps, kPlayerX, kPlayerY, 2),
           "load maps for no-skill monster fixture");
    expect(basic_world.load_monsters_json_dir(monster_defs, kPlayerX, kPlayerY, 0),
           "load no-skill monster fixture");
    std::uint32_t basic_id = 200;
    expect(basic_world.spawn_monster_at(basic_id, 9001, 0, kPlayerX + 64, kPlayerY, 0).has_value(),
           "spawn no-skill monster fixture");
    basic_world.tick(0.05f, {prey}, skill_moves, skill_events);
    expect(std::any_of(skill_events.begin(),
                       skill_events.end(),
                       [](const MonsterAttackEvent& event) {
                           return event.kind == MonsterAttackEvent::Kind::Basic;
                       }),
           "AI 3 without configured skills keeps its basic attack");

    return openkal::test::g_failures == 0 ? 0 : 1;
}
