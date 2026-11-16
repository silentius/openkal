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
#include "openkal/map/MapStream.h"
#include "openkal/map/Portal.h"
#include "openkal/map/WorldGrid.h"
#include "openkal/terrain/HeightPatch.h"
#include "test_util.h"

#include <cmath>
#include <string>
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
    using openkal::test::expect;

    openkal::client::FloorField floor;
    floor.add_patch(make_flat_patch());

    openkal::map::AreaCell cell;
    cell.set_cell_x(32);
    cell.set_cell_y(32);
    cell.assign_attrs(256u * 256u, 0u);
    cell.ensure_blocked_mask();
    cell.ensure_not_moveable_mask();
    cell.set_blocked_at_subcell(0, 0, true);

    openkal::map::WorldGrid walk;
    walk.set_cell(32, 32, cell);

    expect(openkal::client::LocalPlayer::may_stand_at(0.0f, 0.0f, 2000.0f, floor, &walk, 32),
           "player uses geometry across a blocked navigation subcell");
    expect(openkal::client::LocalPlayer::may_stand_at(8.0f, 0.0f, 2000.0f, floor, &walk, 32),
           "player accepts an open neighboring eight-pixel area subcell");
    cell.set_blocked_at_subcell(0, 0, false);
    cell.set_not_moveable_at_subcell(0, 0, true);
    walk.set_cell(32, 32, cell);
    expect(!openkal::client::LocalPlayer::may_stand_at(0.0f, 0.0f, 2000.0f, floor, &walk, 32),
           "player cannot enter a not-moveable area subcell");
    expect(openkal::client::LocalPlayer::may_stand_at(0.0f, 0.0f, 2000.0f, floor, nullptr, 32),
           "null area grid preserves terrain-only movement");
    using openkal::client::FloorField;
    using openkal::client::LocalPlayer;
    openkal::engine::FrameInput input{};
    input.dt = 0.1f;
    input.key_w = true;

    // Blocked navigation masks must not turn a one-way dungeon ledge into a wall.
    cell.ensure_blocked_mask();
    cell.set_blocked_masks(std::vector<openkal::map::AreaCell::SubcellMask>(256u * 256u, 0xffffu));
    cell.set_not_moveable_masks(std::vector<openkal::map::AreaCell::SubcellMask>(256u * 256u, 0u));
    walk.set_cell(32, 32, cell);
    for (bool click : {false, true}) {
        FloorField indoor;
        indoor.add_deck(make_deck(0.0f, 2100.0f, 20.0f));
        indoor.add_deck(make_deck(260.0f, 1900.0f, 240.0f));
        LocalPlayer player;
        player.set_pos(0.0f, 2100.0f, 19.0f);
        player.set_walk_speed(100.0f);
        player.snap_to_ground(indoor);
        player.set_click_target(0.0f, 400.0f);
        input.key_w = !click;
        player.update(input, 0.0f, indoor, &walk, 32);
        expect(player.pos().z > 20.0f, "keyboard and click movement can leave a dungeon ledge");
        player.update(input, 0.0f, indoor, &walk, 32);
        expect(player.pos().y < 2100.0f && player.pos().y > 1900.0f,
               "ledge descent applies gravity instead of snapping to the bottom");
        for (int frame = 0; frame < 25; ++frame) {
            player.update(input, 0.0f, indoor, &walk, 32);
        }
        expect(std::fabs(player.pos().y - 1900.0f) < 0.01f,
               "fall lands on the lower dungeon floor despite navigation masks");
        expect(!LocalPlayer::may_stand_at(0.0f, 0.0f, 1900.0f, indoor, &walk, 32),
               "the same ledge remains unclimbable from below");
        expect(!LocalPlayer::may_stand_at(1000.0f, 1000.0f, 1900.0f, indoor, &walk, 32),
               "empty indoor space remains unreachable");
    }

    {
        auto patch = make_flat_patch();
        for (int z = 1; z < openkal::map::kHeightSamples; ++z) {
            for (int x = 0; x < openkal::map::kHeightSamples; ++x) {
                patch.heights[static_cast<std::size_t>(z) * openkal::map::kHeightSamples + x] =
                    1900u;
            }
        }
        FloorField hillside;
        hillside.add_patch(std::move(patch));
        LocalPlayer player;
        player.set_pos(10.0f, 2000.0f, 0.0f);
        player.set_walk_speed(100.0f);
        player.snap_to_ground(hillside);
        input.key_w = true;
        for (int frame = 0; frame < 30; ++frame) {
            player.update(input, 0.0f, hillside, &walk, 32);
        }
        expect(player.pos().z > 32.0f && std::fabs(player.pos().y - 1900.0f) < 0.01f,
               "outdoor hillside descent crosses navigation masks and reaches lower terrain");
    }

    {
        openkal::client::FloorProp wall;
        wall.set_patch(32, 32);
        wall.add_vertex(-20.0f, 1900.0f, 40.0f);
        wall.add_vertex(20.0f, 1900.0f, 40.0f);
        wall.add_vertex(-20.0f, 2200.0f, 40.0f);
        wall.add_vertex(20.0f, 1900.0f, 40.0f);
        wall.add_vertex(20.0f, 2200.0f, 40.0f);
        wall.add_vertex(-20.0f, 2200.0f, 40.0f);
        wall.pad_bounds_xz(1.0f);
        floor.add_blocker_deck(std::move(wall));
        LocalPlayer player;
        player.set_pos(0.0f, 2000.0f, 30.0f);
        player.set_walk_speed(100.0f);
        player.update(input, 0.0f, floor, &walk, 32);
        expect(player.pos().z == 30.0f, "actual collision walls still stop player movement");
    }

    {
        openkal::map::WorldGrid missing;
        expect(LocalPlayer::may_stand_at(0.0f, 0.0f, 2000.0f, floor, &missing, 32),
               "missing area data does not block a valid geometric floor");
        expect(!missing.walkable_world_pixel(0, 0),
               "missing area data remains blocked for server navigation");
    }

    // Exercise shipped entrance masks, including opaque KSM imports, without
    // requiring the proprietary terrain/compatibility behavior assets in the test environment.
    openkal::map::PortalCatalog portals;
    const std::string root = OPENKAL_TEST_RESOURCE_ROOT;
    expect(portals.load_json(root + "/config/portals.json"), "load shipped portals");
    openkal::map::WorldGrid shipped;
    for (int dungeon = 1; dungeon <= 6; ++dungeon) {
        const auto* portal = portals.entrance_for_dungeon(dungeon);
        expect(portal != nullptr, "dungeon entrance exists");
        if (portal == nullptr) {
            continue;
        }
        const int px = portal->left() * 32 + 16;
        const int py = portal->top() * 32 + 16;
        expect(openkal::map::MapStream::ensure_cell(
                   shipped, root + "/RuntimeData/areas", px / 8192, py / 8192),
               "load shipped dungeon entrance area");
        auto patch = make_flat_patch();
        patch.patch_x = px / 8192;
        patch.patch_y = py / 8192;
        FloorField entrance;
        entrance.add_patch(std::move(patch));
        const float x = openkal::coords::to_client(px);
        const float z = openkal::coords::to_client(py);
        const bool not_moveable = shipped.not_moveable_world_pixel(px, py);
        expect(LocalPlayer::may_stand_at(x, z, 2000.0f, entrance, &shipped, 32) == !not_moveable,
               "shipped not-moveable masks control movement at dungeon portals");
        expect(portals.find(0, px / 32, py / 32) == portal,
               "reachable entrance position resolves to the dungeon portal");
    }
    return openkal::test::g_failures == 0 ? 0 : 1;
}
