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
#include "openkal/map/Portal.h"
#include "openkal/protocol/MonsterRace.h"
#include "openkal/server/Combat.h"
#include "openkal/server/world/GameWorld.h"
#include "test_util.h"

#include <string>
#include <vector>

#ifndef OPENKAL_TEST_RESOURCE_ROOT
#error OPENKAL_TEST_RESOURCE_ROOT required
#endif

int main() {
    using openkal::map::PortalCatalog;
    using openkal::map::PortalKind;
    using openkal::map::PortalRecord;
    using openkal::server::GameWorld;
    using openkal::server::MonsterAttackEvent;
    using openkal::server::MonsterEntity;
    using openkal::server::MonsterMoveEvent;
    using openkal::server::PlayerPose;
    using openkal::test::expect;

    const std::string root = OPENKAL_TEST_RESOURCE_ROOT;
    PortalCatalog catalog;
    expect(catalog.load_json(root + "/config/portals.json"), "load portals.json");
    expect(catalog.portals().size() == 74, "D1–D6, ToP, Emok, and VoD portal records");

    {
        int dungeon = 0;
        expect(PortalCatalog::parse_dungeon_arg("d1", dungeon) && dungeon == 1, "parse d1");
        expect(PortalCatalog::parse_dungeon_arg(" D4 ", dungeon) && dungeon == 4, "parse D4");
        expect(PortalCatalog::parse_dungeon_arg("d5", dungeon) && dungeon == 5, "parse d5");
        expect(PortalCatalog::parse_dungeon_arg("d6", dungeon) && dungeon == 6, "parse d6");
        expect(!PortalCatalog::parse_dungeon_arg("d7", dungeon), "reject d7");
        expect(!PortalCatalog::parse_dungeon_arg("", dungeon), "reject empty");
        expect(!PortalCatalog::parse_dungeon_arg("narootuh", dungeon), "reject city name");
        std::string place;
        expect(PortalCatalog::parse_place_arg("emok", place) && place == "emok_enter",
               "parse emok");
        expect(PortalCatalog::parse_place_arg(" EMOK ", place) && place == "emok_enter",
               "parse EMOK");
        expect(PortalCatalog::parse_place_arg("vod", place) && place == "vod_back", "parse vod");
        expect(PortalCatalog::parse_place_arg("lord", place) && place == "d3_internal_17",
               "parse lord");
        const PortalRecord* lord = catalog.find_by_id(place);
        expect(lord != nullptr && lord->to_map() == 3 && lord->to_x() == 58406 &&
                   lord->to_y() == 5797 && lord->to_z() == 29999,
               "lord target is the D3 top floor");
        expect(std::string(PortalCatalog::place_teleport_reply("emok_enter")) ==
                   "Teleporting to Emok Island.",
               "emok teleport reply");
        expect(std::string(PortalCatalog::place_teleport_reply("vod_back")) ==
                   "Teleporting to Valley of Devah.",
               "vod teleport reply");
        expect(std::string(PortalCatalog::place_teleport_reply("d3_internal_17")) ==
                   "Teleporting to Doggebi Lord stage.",
               "lord teleport reply");
        expect(!PortalCatalog::parse_place_arg("christmas", place), "reject unknown place");
        expect(!PortalCatalog::parse_place_arg("d5", place), "place parser rejects dungeon");
        int area = 0;
        expect(PortalCatalog::parse_area_arg("area 108", area) && area == 108, "parse area 108");
        expect(PortalCatalog::parse_area_arg(" AREA  1 ", area) && area == 1, "parse AREA 1");
        expect(!PortalCatalog::parse_area_arg("area", area), "reject area without id");
        expect(!PortalCatalog::parse_area_arg("area 0", area), "reject area 0");
        expect(!PortalCatalog::parse_area_arg("area -1", area), "reject negative area");
        expect(!PortalCatalog::parse_area_arg("area 108 extra", area), "reject extra area args");
        expect(!PortalCatalog::parse_area_arg("d1", area), "area parser rejects dungeon");
    }

    for (int d = 1; d <= 4; ++d) {
        const PortalRecord* enter = catalog.entrance_for_dungeon(d);
        expect(enter != nullptr, "enter portal present");
        if (enter == nullptr) {
            continue;
        }
        expect(enter->from_map() == 0, "enter from overworld");
        const int expected_to_map = (d == 1) ? 2 : (d == 2) ? 1 : d;
        expect(enter->to_map() == expected_to_map, "enter to dungeon map");
        expect(enter->kind() == PortalKind::Enter, "enter kind");
        expect(enter->contains_tile(enter->left(), enter->top()), "fromRect includes left/top");
        expect(!enter->contains_tile(enter->right(), enter->top()), "PtInRect exclusive right");
        expect(!enter->contains_tile(enter->left(), enter->bottom()), "PtInRect exclusive bottom");
        expect(enter->entrance_x() != enter->to_x() || enter->entrance_y() != enter->to_y(),
               "/teleport stands in front, not at to");
        const int stand_tx = enter->left() - 6;
        const int stand_ty = (enter->top() + enter->bottom()) / 2;
        expect(!enter->contains_tile(stand_tx, stand_ty), "entrance tile is not on the portal");
        const PortalRecord* hit = catalog.find(0, enter->left(), enter->top());
        expect(hit == enter, "find enter by tile");
    }

    {
        const PortalRecord* d1 = catalog.entrance_for_dungeon(1);
        expect(d1 != nullptr && d1->entrance_x() == 282486 && d1->entrance_y() == 236837,
               "d1 entrance pixels");
        expect(d1 != nullptr && d1->to_x() == 50204 && d1->to_map() == 2, "d1 dest is dungeon");
        const PortalRecord* d1_exit = catalog.find(2, 1565, 1063);
        expect(d1_exit != nullptr && d1_exit->to_map() == 0, "d1 exit returns to map 0");
        expect(d1_exit != nullptr && d1_exit->to_x() == 282486, "d1 exit lands at overworld gate");
    }

    {
        const PortalRecord* d4_enter = catalog.entrance_for_dungeon(4);
        expect(d4_enter != nullptr && d4_enter->to_x() == 35560 && d4_enter->to_y() == 34516,
               "d4 enter dest pixels");
        const int spawn_tx = d4_enter->to_x() / 32;
        const int spawn_ty = d4_enter->to_y() / 32;
        const PortalRecord* overlap = catalog.find(4, spawn_tx, spawn_ty);
        expect(overlap != nullptr && overlap->to_map() == 0,
               "d4 spawn tile sits inside the JSON exit rect");
        const int overlap_area =
            (overlap->right() - overlap->left()) * (overlap->bottom() - overlap->top());
        expect(overlap_area > openkal::map::kMaxUngatedPortalArea,
               "d4 spawn overlap is a lookup rect, not a small gate");
    }

    {
        const PortalRecord* d5 = catalog.entrance_for_dungeon(5);
        expect(d5 != nullptr && d5->to_map() == 11 && d5->to_x() == 70176,
               "d5 enter dest is Tower of Priest map 11");
        expect(d5 != nullptr && d5->entrance_x() == 235891 && d5->entrance_y() == 279816,
               "d5 overworld gate pixels");
        const PortalRecord* d5_exit = catalog.find(11, 2190, 1575);
        expect(d5_exit != nullptr && d5_exit->to_map() == 0 && d5_exit->to_x() == 235891,
               "ToP F1 exit returns to the overworld gate");
        const PortalRecord* d5_up = catalog.find(11, 2191, 1771);
        expect(d5_up != nullptr && d5_up->to_map() == 11 && d5_up->to_x() == 86555, "ToP F1 to F2");
        const PortalRecord* d6 = catalog.entrance_for_dungeon(6);
        expect(d6 != nullptr && d6->to_map() == 25 && d6->to_x() == 69464,
               "d6 enter dest is map 25");
        expect(d6 != nullptr && d6->entrance_x() == 356695 && d6->entrance_y() == 245516,
               "d6 overworld gate pixels");
        const PortalRecord* d6_exit = catalog.find(25, 2160, 524);
        expect(d6_exit != nullptr && d6_exit->to_map() == 0 && d6_exit->to_x() == 356695,
               "d6 lobby exit returns to the VoD gate");
        const PortalRecord* vod_other = catalog.find(0, 11135, 7688);
        expect(vod_other != nullptr && vod_other->id() == "vod_to_other_island" &&
                   vod_other->to_x() == 361978,
               "VoD hop to the other island");
        const PortalRecord* vod_back = catalog.find(0, 11309, 7654);
        expect(vod_back != nullptr && vod_back->id() == "vod_back" && vod_back->to_x() == 356468,
               "VoD hop back to the D6 gate island");
        expect(catalog.find(0, 8036, 8097) == nullptr,
               "disabled Narootuh Emok portal is no longer active");
        const PortalRecord* emok = catalog.find_by_id("emok_enter");
        expect(emok != nullptr && !emok->field_trigger_enabled(),
               "Emok remains available only through /teleport emok");
        expect(emok != nullptr && emok->to_map() == 20 && emok->to_x() == 340679 &&
                   emok->to_y() == 260153,
               "Emok command teleport keeps its island destination");
    }

    const std::string chase_maps = openkal::test::temp_dir("portal_chase_maps");
    expect(openkal::test::write_open_area(
               openkal::platform::join_path(chase_maps, "n_000_000.akarea")),
           "write chase map fixture");

    {
        GameWorld world;
        MonsterEntity m;
        m.id = 1;
        m.x = 100;
        m.y = 100;
        m.map = 1;
        m.hp = 100;
        m.max_hp = 100;
        m.race = static_cast<int>(openkal::protocol::MonsterRace::Maguni);
        m.aggressive = true;
        m.detect_range = 400;
        m.sight_range = 800;
        m.attack_range = 80;
        m.min_attack = 7;
        m.max_attack = 7;
        world.monsters().push_back(m);

        std::vector<MonsterMoveEvent> moves;
        std::vector<MonsterAttackEvent> attacks;
        PlayerPose other_map{1, 100, 100, 0};
        world.tick(0.25f, {other_map}, moves, attacks);
        expect(attacks.empty(), "map 1 mob does not attack map 0 player");
        expect(!world.monsters().empty() && !world.monsters().front().aggro,
               "map 1 mob does not aggro map 0 player");

        attacks.clear();
        moves.clear();
        PlayerPose same_map{1, 100, 100, 1};
        world.tick(0.25f, {same_map}, moves, attacks);
        expect(world.monsters().front().aggro, "same-map player is aggroed");
        expect(!attacks.empty() && attacks.front().damage == 7,
               "same-map player is hit when the swing starts");

        bool extra = false;
        for (int i = 0; i < 6 && !extra; ++i) {
            attacks.clear();
            moves.clear();
            world.tick(0.25f, {same_map}, moves, attacks);
            extra = !attacks.empty();
        }
        expect(!extra, "no second attack during aspeed cooldown");
    }

    {
        GameWorld world;
        MonsterEntity m;
        m.id = 1;
        m.x = 100;
        m.y = 100;
        m.map = 1;
        m.hp = 100;
        m.max_hp = 100;
        m.race = static_cast<int>(openkal::protocol::MonsterRace::Maguni);
        m.aggressive = true;
        m.detect_range = 400;
        m.sight_range = 800;
        m.attack_range = 80;
        m.min_attack = 7;
        m.max_attack = 7;
        world.monsters().push_back(m);

        std::vector<MonsterMoveEvent> moves;
        std::vector<MonsterAttackEvent> attacks;
        PlayerPose protected_login{1, 100, 100, 1};
        protected_login.aggro_immune = true;
        world.tick(0.25f, {protected_login}, moves, attacks);
        expect(!world.monsters().front().aggro, "login-protected player is ignored by ScanSight");
        expect(attacks.empty(), "login-protected player is not attacked");
    }

    {
        GameWorld world;
        MonsterEntity m;
        m.id = 1;
        m.x = 100;
        m.y = 100;
        m.map = 1;
        m.hp = 100;
        m.max_hp = 100;
        m.race = static_cast<int>(openkal::protocol::MonsterRace::Maguni);
        m.aggressive = true;
        m.detect_range = 400;
        m.sight_range = 800;
        m.attack_range = 80;
        m.min_attack = 7;
        m.max_attack = 7;
        world.monsters().push_back(m);

        std::vector<MonsterMoveEvent> moves;
        std::vector<MonsterAttackEvent> attacks;
        PlayerPose visible{1, 100, 100, 1};
        world.tick(0.25f, {visible}, moves, attacks);
        expect(world.monsters().front().aggro, "visible player is aggroed");

        attacks.clear();
        moves.clear();
        PlayerPose hidden{1, 100, 100, 1};
        hidden.aggro_immune = true;
        world.tick(0.25f, {hidden}, moves, attacks);
        expect(!world.monsters().front().aggro, "hiding drops existing monster aggro");
        expect(attacks.empty(), "hidden player is not attacked");
    }

    {
        GameWorld world;
        MonsterEntity m;
        m.id = 1;
        m.hp = 100;
        m.max_hp = 100;
        m.spawn_protection_left = 10.0f;
        world.monsters().push_back(m);
        expect(world.apply_player_attack(99, 1, 20, 100, 100) == 0,
               "spawn protection rejects incoming damage");
        expect(world.find_monster(1)->hp == 100, "spawn protection preserves health");
        std::vector<MonsterMoveEvent> moves;
        world.tick(10.0f, moves);
        expect(world.apply_player_attack(99, 1, 20, 100, 100) == 20,
               "spawn protection expires after ten seconds");
        expect(world.find_monster(1)->hp == 80, "damage applies after spawn protection expires");
    }

    {
        GameWorld world;
        MonsterEntity m;
        m.id = 1;
        m.x = 100;
        m.y = 100;
        m.map = 0;
        m.hp = 100;
        m.max_hp = 100;
        m.race = static_cast<int>(openkal::protocol::MonsterRace::NotMaguni);
        m.aggressive = true;
        m.detect_range = 400;
        m.sight_range = 800;
        expect(world.load_maps_around(chase_maps, 100, 100, 0), "load chase map fixture");
        m.attack_range = 80;
        m.min_attack = 7;
        m.max_attack = 7;
        world.monsters().push_back(m);

        std::vector<MonsterMoveEvent> moves;
        std::vector<MonsterAttackEvent> attacks;
        PlayerPose in_melee{1, 100, 100, 0};
        world.tick(0.05f, {in_melee}, moves, attacks);
        expect(world.monsters().front().aggro && world.monsters().front().attack_swinging,
               "melee start commits a swing");
        expect(!attacks.empty() && attacks.front().damage == 7,
               "swing start resolves damage while the player is in melee");

        bool extra_hit = false;
        PlayerPose kited{1, 400, 400, 0};
        for (int i = 0; i < 8 && !extra_hit; ++i) {
            attacks.clear();
            moves.clear();
            world.tick(0.25f, {kited}, moves, attacks);
            extra_hit = !attacks.empty();
        }
        expect(!extra_hit, "kiting after the swing does not emit another attack during aspeed");

        for (int i = 0; i < 12; ++i) {
            attacks.clear();
            moves.clear();
            world.tick(0.25f, {kited}, moves, attacks);
        }
        expect(world.monsters().front().aggro, "out of melee after the swing keeps aggro");
        expect(world.monsters().front().ai == MonsterEntity::Ai::Chase,
               "AI state is Chase/run after a missed swing in sight");
    }

    {
        GameWorld world;
        MonsterEntity m;
        m.id = 1;
        m.x = 100;
        m.y = 100;
        m.map = 0;
        m.hp = 100;
        m.max_hp = 100;
        m.race = static_cast<int>(openkal::protocol::MonsterRace::NotMaguni);
        m.aggressive = true;
        m.detect_range = 400;
        m.sight_range = 200;
        m.attack_range = 80;
        m.min_attack = 7;
        m.max_attack = 7;
        world.monsters().push_back(m);

        std::vector<MonsterMoveEvent> moves;
        std::vector<MonsterAttackEvent> attacks;
        PlayerPose in_melee{1, 100, 100, 0};
        world.tick(0.05f, {in_melee}, moves, attacks);
        expect(world.monsters().front().attack_swinging, "leash test starts a swing");

        PlayerPose fled{1, 2000, 2000, 0};
        for (int i = 0; i < 16; ++i) {
            attacks.clear();
            moves.clear();
            world.tick(0.25f, {fled}, moves, attacks);
        }
        expect(!world.monsters().front().aggro, "out of sight after the swing returns to roam");
        expect(world.monsters().front().ai == MonsterEntity::Ai::Roam,
               "AI state is Roam after losing sight");
    }

    {
        GameWorld world;
        MonsterEntity m;
        m.id = 1;
        // MoveMonsterOn already snapped the authoritative position to this endpoint;
        // the client is still interpolating toward it while move_busy_left is active.
        m.x = 132;
        m.y = 100;
        m.target_x = 132;
        m.target_y = 100;
        m.map = 0;
        m.hp = 100;
        m.max_hp = 100;
        m.race = static_cast<int>(openkal::protocol::MonsterRace::NotMaguni);
        m.aggressive = true;
        m.aggro = true;
        m.aggro_player_id = 1;
        m.ai = MonsterEntity::Ai::Chase;
        m.attack_range = 20;
        m.sight_range = 200;
        m.chase_speed = 80.0f;
        m.moving = true;
        m.move_is_run = true;
        m.move_busy_left = 0.40f;
        world.monsters().push_back(m);

        std::vector<MonsterMoveEvent> moves;
        std::vector<MonsterAttackEvent> attacks;
        PlayerPose fled{1, 2000, 2000, 0};
        world.tick(0.10f, {fled}, moves, attacks);

        const MonsterEntity& dropped = world.monsters().front();
        expect(!dropped.aggro && dropped.ai == MonsterEntity::Ai::Roam,
               "out-of-sight target is dropped during a committed chase step");
        expect(dropped.moving && dropped.move_busy_left > 0.0f,
               "aggro loss preserves the committed chase step until client arrival");
        expect(moves.empty(), "aggro loss does not replace an in-flight client waypoint");

        world.tick(0.30f, {fled}, moves, attacks);
        expect(!world.monsters().front().moving,
               "committed chase step ends after its previous travel time");
        expect(moves.size() == 1 && moves.front().ended,
               "completed chase step emits one terminal movement event");
    }

    {
        GameWorld world;
        MonsterEntity m;
        m.id = 1;
        m.x = 100;
        m.y = 100;
        m.target_x = 100;
        m.target_y = 132;
        m.map = 0;
        m.hp = 100;
        m.max_hp = 100;
        m.race = static_cast<int>(openkal::protocol::MonsterRace::NotMaguni);
        m.aggressive = true;
        m.aggro = true;
        m.aggro_player_id = 1;
        m.ai = MonsterEntity::Ai::Chase;
        m.attack_range = 80;
        m.sight_range = 800;
        m.chase_speed = 80.0f;
        m.moving = true;
        m.move_is_run = true;
        m.move_busy_left = 0.20f;
        m.attack_cooldown = 0.0f;
        m.attack_swinging = false;
        world.monsters().push_back(m);

        std::vector<MonsterMoveEvent> moves;
        std::vector<MonsterAttackEvent> attacks;
        PlayerPose prey{1, 100, 100, 0};
        world.tick(0.05f, {prey}, moves, attacks);
        expect(world.monsters().front().moving, "chase step is still busy");
        expect(attacks.empty(), "no swing while the chase step is in flight");

        attacks.clear();
        moves.clear();
        world.tick(0.20f, {prey}, moves, attacks);
        expect(!world.monsters().front().moving, "chase step has arrived");
        expect(attacks.empty(), "arrive tick only stops; Attack runs next tick");
        expect(world.monsters().front().ai == MonsterEntity::Ai::Attack,
               "AI state is Attack after arriving in melee");

        attacks.clear();
        moves.clear();
        world.tick(0.05f, {prey}, moves, attacks);
        expect(!attacks.empty(), "standing tick after arrive starts the swing");
        expect(world.monsters().front().attack_swinging, "melee swing is committed while standing");
    }

    expect(openkal::server::combat::resource_cost(12, false) == 12, "mp spend without godmode");
    expect(openkal::server::combat::resource_cost(12, true) == 0, "godmode skips mp spend");
    expect(openkal::server::combat::incoming_damage(40, false) == 40, "damage without godmode");
    expect(openkal::server::combat::incoming_damage(40, true) == 0, "godmode zeroes damage");
    expect(openkal::server::combat::incoming_damage(-3, false) == 0, "negative damage clamps");

    return openkal::test::g_failures == 0 ? 0 : 1;
}
