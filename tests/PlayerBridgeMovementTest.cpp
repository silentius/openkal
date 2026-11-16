/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/Floor.h"
#include "openkal/client/Player.h"
#include "openkal/map/AreaCell.h"
#include "openkal/map/Coords.h"
#include "openkal/map/WorldGrid.h"
#include "openkal/terrain/HeightPatch.h"
#include "test_util.h"

#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

namespace {

    openkal::terrain::HeightPatch make_flat_patch() {
        openkal::terrain::HeightPatch patch;
        patch.patch_x = 32;
        patch.patch_y = 32;
        patch.heights.assign(static_cast<std::size_t>(openkal::map::kHeightSamples) *
                                 openkal::map::kHeightSamples,
                             2000u);
        return patch;
    }

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

    openkal::map::AreaCell cell;
    cell.set_cell_x(32);
    cell.set_cell_y(32);
    cell.assign_attrs(256u * 256u, 0u);
    cell.ensure_not_moveable_mask();
    openkal::engine::FrameInput input{};
    input.dt = 0.1f;

    {
        auto bridge_cell = cell;
        bridge_cell.set_not_moveable_masks(
            std::vector<openkal::map::AreaCell::SubcellMask>(256u * 256u, 0xffffu));
        for (int sy = 0; sy < 4; ++sy) {
            bridge_cell.set_not_moveable_at_subcell(2, sy, false);
        }
        openkal::map::WorldGrid bridge_walk;
        bridge_walk.set_cell(32, 32, bridge_cell);

        // River and high gorge decks must cross the terrain mask, including when
        // movement starts halfway across the bridge.
        for (const auto& heights : {std::pair<float, float>{1300.0f, 1590.0f},
                                    std::pair<float, float>{140.0f, 2325.0f}}) {
            auto patch = make_flat_patch();
            patch.heights.assign(patch.heights.size(), static_cast<std::uint16_t>(heights.first));
            FloorField bridge;
            bridge.add_patch(std::move(patch));
            bridge.add_deck(make_deck(80.0f, heights.second, 80.0f));
            expect(bridge_walk.not_moveable_world_pixel(openkal::coords::to_server(16.0f, 32),
                                                        openkal::coords::to_server(40.0f, 32)),
                   "bridge crosses a not-moveable terrain subcell");
            expect(
                LocalPlayer::may_stand_at(16.0f, 40.0f, heights.second, bridge, &bridge_walk, 32),
                "reachable bridge deck overrides the terrain mask");
            expect(
                !LocalPlayer::may_stand_at(160.0f, 80.0f, heights.second, bridge, &bridge_walk, 32),
                "terrain beside the bridge remains not-moveable");
            for (bool click : {false, true}) {
                for (float start_z : {24.0f, 80.0f}) {
                    LocalPlayer player;
                    player.set_pos(16.0f, heights.second, start_z);
                    player.set_walk_speed(100.0f);
                    player.snap_to_ground(bridge);
                    player.set_click_target(16.0f, start_z + 40.0f);
                    input.key_w = !click;
                    for (int frame = 0; frame < 4; ++frame) {
                        player.update(input, 0.0f, bridge, &bridge_walk, 32);
                    }
                    expect(player.pos().z >= start_z + 39.0f,
                           "keyboard and click movement cross masked bridge cells");
                    expect(std::fabs(player.pos().y - heights.second) < 0.01f,
                           "bridge traversal keeps the deck height");
                }
            }
        }

        FloorField roof;
        roof.add_patch(make_flat_patch());
        roof.add_deck(make_deck(80.0f, 2200.0f, 80.0f));
        expect(!LocalPlayer::may_stand_at(16.0f, 80.0f, 2000.0f, roof, &bridge_walk, 32),
               "an unreachable roof cannot bypass the terrain mask");
        FloorField indoor;
        indoor.add_deck(make_deck(80.0f, 2000.0f, 80.0f));
        expect(!LocalPlayer::may_stand_at(16.0f, 80.0f, 2000.0f, indoor, &bridge_walk, 32),
               "indoor not-moveable masks still block their floor");

        FloorField bridge;
        auto patch = make_flat_patch();
        patch.heights.assign(patch.heights.size(), 1300u);
        bridge.add_patch(std::move(patch));
        bridge.add_deck(make_deck(80.0f, 1590.0f, 80.0f));
        openkal::client::FloorProp wall;
        wall.add_vertex(-80.0f, 1590.0f, 40.0f);
        wall.add_vertex(80.0f, 1590.0f, 40.0f);
        wall.add_vertex(-80.0f, 1690.0f, 40.0f);
        wall.add_vertex(80.0f, 1590.0f, 40.0f);
        wall.add_vertex(80.0f, 1690.0f, 40.0f);
        wall.add_vertex(-80.0f, 1690.0f, 40.0f);
        wall.pad_bounds_xz(1.0f);
        bridge.add_blocker_deck(std::move(wall));
        LocalPlayer player;
        player.set_pos(16.0f, 1590.0f, 30.0f);
        player.set_walk_speed(100.0f);
        input.key_w = true;
        player.update(input, 0.0f, bridge, &bridge_walk, 32);
        expect(player.pos().z == 30.0f, "collision walls still stop movement on a masked bridge");
    }

    {
        constexpr float ground = 2000.0f;
        constexpr float front = 40.0f;
        struct Platform {
            float rise;
            float run;
            bool climbable;
        };
        const Platform scenarios[] = {
            {6.0f, 0.0f, true},
            {9.0f, 0.0f, false},
            {7.17f, 7.17f, false},
            {9.0f, 9.0f, false},
            {9.0f, 18.0f, true},
        };
        for (const auto& scenario : scenarios) {
            const float top = ground + scenario.rise;
            const float base = front - scenario.run;
            FloorField floor;
            floor.add_patch(make_flat_patch());
            floor.add_deck(make_deck(60.0f, top, 20.0f));

            openkal::client::FloorProp riser;
            riser.add_vertex(-20.0f, ground, base);
            riser.add_vertex(-20.0f, top, front);
            riser.add_vertex(20.0f, ground, base);
            riser.add_vertex(20.0f, ground, base);
            riser.add_vertex(-20.0f, top, front);
            riser.add_vertex(20.0f, top, front);
            floor.add_deck(riser);
            floor.add_blocker_deck(std::move(riser));

            for (const bool click : {false, true}) {
                LocalPlayer player;
                player.set_pos(16.0f, ground, 18.0f);
                if (click) {
                    player.set_click_target(16.0f, 59.0f);
                }
                openkal::engine::FrameInput step_input{};
                step_input.dt = 0.05f;
                step_input.key_w = !click;
                for (int frame = 0; frame < 21; ++frame) {
                    player.update(step_input, 0.0f, floor);
                }
                if (scenario.climbable) {
                    expect(player.pos().z > front && std::fabs(player.pos().y - top) < 0.01f,
                           "movement climbs a low step or walkable slope");
                } else {
                    expect(
                        player.pos().z < front && std::fabs(player.pos().y - ground) < 0.01f,
                        "movement cannot pass through a vertical or beveled riser above the step limit");
                }

                if (!scenario.climbable && scenario.run > 0.0f) {
                    player.set_pos(16.0f, ground, front + 2.0f);
                    if (click) {
                        player.set_click_target(16.0f, 18.0f);
                    }
                    step_input.key_w = false;
                    step_input.key_s = !click;
                    for (int frame = 0; frame < 14; ++frame) {
                        player.update(step_input, 0.0f, floor);
                    }
                    expect(
                        player.pos().z < base && std::fabs(player.pos().y - ground) < 0.01f,
                        "a character saved inside a beveled riser can move downhill to open ground");
                }
            }
        }
    }

    {
        constexpr float lower = 2005.0f;
        constexpr float upper = lower + 24.0f;
        constexpr float front = 40.0f;
        constexpr float base = front - 5.0f;
        for (const bool has_underpass : {false, true}) {
            FloorField floor;
            floor.add_patch(make_flat_patch());
            floor.add_deck(make_deck(48.0f, lower, 48.0f));
            floor.add_deck(make_deck(60.0f, upper, 20.0f));

            openkal::client::FloorProp edge;
            edge.add_vertex(-20.0f, lower, base);
            edge.add_vertex(-20.0f, upper, front);
            edge.add_vertex(20.0f, lower, base);
            edge.add_vertex(20.0f, lower, base);
            edge.add_vertex(-20.0f, upper, front);
            edge.add_vertex(20.0f, upper, front);
            if (has_underpass) {
                constexpr float underside = upper - 2.0f;
                edge.add_vertex(-20.0f, underside, front);
                edge.add_vertex(20.0f, underside, front);
                edge.add_vertex(-20.0f, underside, 80.0f);
                edge.add_vertex(20.0f, underside, front);
                edge.add_vertex(20.0f, underside, 80.0f);
                edge.add_vertex(-20.0f, underside, 80.0f);
            }
            floor.add_blocker_deck(std::move(edge));

            LocalPlayer player;
            player.set_pos(16.0f, lower, front - 0.5f);
            openkal::engine::FrameInput step_input{};
            step_input.dt = 0.05f;
            step_input.key_w = true;
            for (int frame = 0; frame < 8; ++frame) {
                player.update(step_input, 0.0f, floor);
            }
            if (has_underpass) {
                expect(player.pos().z > front && std::fabs(player.pos().y - lower) < 0.01f,
                       "a bridge underside leaves room to traverse the lower deck");
            } else {
                expect(player.pos().z < front && std::fabs(player.pos().y - lower) < 0.01f,
                       "movement from the lower deck cannot enter a solid tapered platform");
                player.set_pos(16.0f, upper, front + 2.0f);
                player.snap_to_ground(floor);
                step_input.key_w = false;
                step_input.key_s = true;
                for (int frame = 0; frame < 19; ++frame) {
                    player.update(step_input, 0.0f, floor);
                }
                expect(player.pos().z < base && std::fabs(player.pos().y - lower) < 0.01f,
                       "walking off a solid platform still drops onto the lower deck");
            }
        }
    }

    {
        constexpr float ground = 2000.0f;
        constexpr float start = 40.0f;
        constexpr float rise = 4.0f;
        constexpr float bevel = 1.25f;
        constexpr float tread = 3.5f;
        constexpr int step_count = 5;
        constexpr float top = ground + step_count * rise;
        constexpr float landing = start + step_count * (bevel + tread);
        FloorField floor;
        floor.add_patch(make_flat_patch());
        openkal::client::FloorProp stairs;
        const auto add_surface = [&](float lower_z, float lower_y, float upper_z, float upper_y) {
            stairs.add_vertex(0.0f, lower_y, lower_z);
            stairs.add_vertex(0.0f, upper_y, upper_z);
            stairs.add_vertex(32.0f, lower_y, lower_z);
            stairs.add_vertex(32.0f, lower_y, lower_z);
            stairs.add_vertex(0.0f, upper_y, upper_z);
            stairs.add_vertex(32.0f, upper_y, upper_z);
        };
        for (int i = 0; i < step_count; ++i) {
            const float foot = start + i * (bevel + tread);
            const float previous = ground + i * rise;
            add_surface(foot, previous, foot + bevel, previous + rise);
            add_surface(foot + bevel, previous + rise, foot + bevel + tread, previous + rise);
        }
        add_surface(landing, top, landing + 32.0f, top);
        floor.add_deck(stairs);
        floor.add_blocker_deck(std::move(stairs));

        for (const int frequency : {144, 60, 30, 10}) {
            for (const bool click : {false, true}) {
                LocalPlayer player;
                player.set_pos(16.0f, ground, start - 4.0f);
                if (click) {
                    player.set_click_target(16.0f, landing + 4.0f);
                }
                openkal::engine::FrameInput step_input{};
                step_input.dt = 1.0f / frequency;
                step_input.key_w = !click;
                float highest = ground;
                bool kept_support = true;
                for (int frame = 0; frame < frequency * 2 && player.pos().z < landing + 2.0f;
                     ++frame) {
                    player.update(step_input, 0.0f, floor);
                    kept_support = kept_support && player.pos().y > highest - rise * 0.5f;
                    if (player.pos().y > highest) {
                        highest = player.pos().y;
                    }
                }
                expect(player.pos().z >= landing + 2.0f && std::fabs(player.pos().y - top) < 0.01f,
                       "movement traverses five narrow beveled stairs at varied frame rates");
                expect(kept_support, "movement retains support between successive stair treads");
            }
        }
        LocalPlayer runner;
        runner.set_pos(16.0f, ground, start - 4.0f);
        openkal::engine::FrameInput run_input{};
        run_input.dt = 0.1f;
        run_input.key_w = true;
        run_input.key_shift = true;
        for (int frame = 0; frame < 15 && runner.pos().z < landing + 2.0f; ++frame) {
            runner.update(run_input, 0.0f, floor);
        }
        expect(runner.pos().z >= landing + 2.0f && std::fabs(runner.pos().y - top) < 0.01f,
               "running with long frame intervals still resolves successive stair treads");
    }

    {
        FloorField floor;
        floor.add_patch(make_flat_patch());
        LocalPlayer player;
        player.set_pos(16.0f, 2000.0f, 30.0f);
        player.set_click_target(100.0f, 100.0f);
        auto locked_input = input;
        locked_input.key_w = true;
        locked_input.key_shift = true;
        player.update(locked_input, 0.0f, floor, nullptr, 32, true);
        expect(player.pos().x == 16.0f && player.pos().z == 30.0f && !player.moving() &&
                   !player.running() && !player.has_click_target(),
               "a locked action blocks WASD and discards an existing click path");
        player.set_click_target(100.0f, 100.0f);
        player.update(input, 0.0f, floor, nullptr, 32, true);
        expect(player.pos().x == 16.0f && player.pos().z == 30.0f && !player.has_click_target(),
               "new mouse movement cannot slide the player during a locked animation");
        player.update(locked_input, 0.0f, floor);
        expect(player.pos().z > 30.0f && player.moving(),
               "held movement resumes after the animation releases its lock");
    }

    return openkal::test::g_failures == 0 ? 0 : 1;
}
