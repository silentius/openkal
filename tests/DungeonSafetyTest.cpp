/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/Player.h"
#include "openkal/terrain/IndoorCollision.h"
#include "test_util.h"

namespace {
    openkal::client::FloorProp make_deck(float z, float y, float half) {
        openkal::client::FloorProp prop;
        prop.set_patch(32, 32);
        prop.add_vertex(-half, y, z - half);
        prop.add_vertex(-half, y, z + half);
        prop.add_vertex(half, y, z - half);
        prop.add_vertex(half, y, z - half);
        prop.add_vertex(-half, y, z + half);
        prop.add_vertex(half, y, z + half);
        prop.pad_bounds_xz(1.0f);
        return prop;
    }

}  // namespace

int main() {
    using openkal::client::FloorField;
    using openkal::client::LocalPlayer;
    using openkal::test::expect;
    {
        using openkal::terrain::IndoorCollision;
        FloorField dungeon;
        dungeon.add_deck(make_deck(0, 100, 100));
        dungeon.add_deck(make_deck(0, 150, 10));
        float height = 0;
        expect(IndoorCollision::resolve_standing(dungeon, {0, 100, 0}, height) && height == 100,
               "login under an arch resolves the floor, not the lintel");
        expect(IndoorCollision::standing(dungeon, {0, 100, 0}), "grounded dungeon pose accepted");
        expect(!IndoorCollision::standing(dungeon, {0, 90, 0}), "underground pose rejected");
        expect(!IndoorCollision::standing(dungeon, {0, 200, 0}), "floating pose rejected");
        expect(!IndoorCollision::standing(dungeon, {300, 100, 0}), "outside pose rejected");
        expect(IndoorCollision::can_move(dungeon, {0, 100, 0}, {20, 100, 0}),
               "normal movement below an arch remains valid");

        FloorField rounded_floor;
        rounded_floor.add_deck(make_deck(0, 100.09f, 100));
        rounded_floor.add_blocker_deck(make_deck(0, 100.12f, 100));
        expect(IndoorCollision::standing(rounded_floor, {0, 100, 0}),
               "protocol-rounded feet remain grounded below the visible floor skin");
        expect(IndoorCollision::can_move(rounded_floor, {0, 100, 0}, {20, 100, 0}),
               "rounded movement does not mistake the floor skin for a ceiling");
        expect(IndoorCollision::can_move(rounded_floor, {0, 102, 0}, {0, 100, 0}),
               "rounded landing also checks headroom above the resolved floor");
        rounded_floor.add_blocker_deck(make_deck(0, 105, 100));
        expect(!IndoorCollision::can_move(rounded_floor, {0, 100, 0}, {20, 100, 0}),
               "a real low ceiling still rejects rounded movement");

        expect(!IndoorCollision::can_move(dungeon, {0, 100, 0}, {0, 102, 0}),
               "vertical-only flight is rejected");
        expect(!IndoorCollision::can_move(dungeon, {0, 102, 0}, {0, 102.1f, 0}),
               "repeated tiny vertical deltas cannot accumulate flight");
        expect(!IndoorCollision::can_move(dungeon, {0, 100, 0}, {0, 98, 0}),
               "movement below the floor is rejected");
        expect(!IndoorCollision::can_move(dungeon, {0, 100, 0}, {0, 150, 0}),
               "movement cannot climb onto an arch");

        FloorField ledge;
        ledge.add_deck(make_deck(0, 100, 10));
        ledge.add_deck(make_deck(35, 60, 25));
        expect(IndoorCollision::can_move(ledge, {0, 100, 9}, {0, 100, 12}),
               "walking off a ledge is permitted");
        expect(IndoorCollision::can_move(ledge, {0, 100, 12}, {0, 90, 15}),
               "falling toward the lower floor is permitted");
        expect(!IndoorCollision::standing(ledge, {0, 90, 15}),
               "an airborne player cannot attack as grounded");
        expect(IndoorCollision::can_move(ledge, {0, 62, 15}, {0, 60, 15}),
               "landing on the lower floor is permitted");
        expect(!IndoorCollision::can_move(ledge, {0, 60, 15}, {0, 100, 9}),
               "one-way ledges cannot be climbed from below");
        FloorField gap;
        gap.add_deck(make_deck(0, 100, 5));
        gap.add_deck(make_deck(20, 100, 5));
        expect(!IndoorCollision::can_move(gap, {0, 100, 0}, {0, 100, 20}),
               "a packet cannot skip unsupported space between valid endpoints");

        LocalPlayer predicted;
        predicted.set_pos(0, 100, 0);
        predicted.set_walk_speed(200);
        openkal::engine::FrameInput input{};
        input.dt = 0.1f;
        input.key_w = true;
        predicted.update(input, 0, gap, nullptr, 32);
        expect(predicted.pos().z == 0, "local prediction also rejects crossing unsupported space");
        FloorField stairs;
        stairs.add_deck(make_deck(0, 100, 5));
        stairs.add_deck(make_deck(10, 106, 5));
        expect(IndoorCollision::can_move(stairs, {0, 100, 0}, {0, 106, 10}),
               "valid steps remain climbable without allowing arch jumps");

        openkal::client::FloorProp wall;
        for (auto v : {openkal::engine::Vec3{-100, 0, -10},
                       {100, 0, -10},
                       {-100, 300, -10},
                       {100, 0, -10},
                       {100, 300, -10},
                       {-100, 300, -10}}) {
            wall.add_vertex(v.x, v.y, v.z);
        }
        dungeon.add_blocker_deck(wall);
        expect(!IndoorCollision::can_move(dungeon, {0, 100, 0}, {0, 100, -20}),
               "a packet cannot cross a thin wall");
        LocalPlayer player;
        player.set_pos(0, 100, 0);
        openkal::client::FollowCamera camera;
        camera.set_pitch_deg(-5);
        camera.set_distance(156);
        camera.limit_indoor_zoom(player, dungeon);
        expect(camera.eye(player).z > -8 && camera.eye(player).z < 0,
               "wall collision overrides minimum wheel distance");
        camera.start_shake(1, 30);
        camera.tick_shake(0.1f);
        camera.limit_indoor_zoom(player, dungeon);
        expect(camera.eye(player).z > -8, "camera shake cannot push the eye through a wall");
        camera.set_yaw_deg(180);
        camera.limit_indoor_zoom(player, dungeon);
        expect(camera.eye(player).z > 100, "turning away restores the requested zoom distance");
        FloorField ceiling;
        ceiling.add_blocker_deck(make_deck(0, 120, 200));
        camera.set_yaw_deg(0);
        camera.set_pitch_deg(-60);
        camera.limit_indoor_zoom(player, ceiling);
        expect(camera.eye(player).y < 118, "low ceilings block even with reversed winding");
        FloorField low_passage;
        low_passage.add_blocker_deck(make_deck(0, 112, 200));
        camera.limit_indoor_zoom(player, low_passage, 4.0f);
        expect(camera.eye(player).y < 110,
               "camera aim stays below low ceilings even with an ultrawide near plane");
    }

    return openkal::test::g_failures == 0 ? 0 : 1;
}
