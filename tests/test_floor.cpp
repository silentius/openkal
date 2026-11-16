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
#include "openkal/client/Floor.h"
#include "openkal/map/Constants.h"
#include "openkal/platform/Path.h"
#include "openkal/terrain/HeightPatch.h"
#include "test_util.h"

#include <array>
#include <cmath>
#include <cstdint>

namespace {

    openkal::terrain::HeightPatch make_flat_patch(int px, int py, std::uint16_t h) {
        openkal::terrain::HeightPatch p;
        p.patch_x = px;
        p.patch_y = py;
        p.heights.assign(static_cast<std::size_t>(openkal::map::kHeightSamples) *
                             openkal::map::kHeightSamples,
                         h);
        return p;
    }

    /** Horizontal quad, compatibility behavior from above, centered on (cx, cz). */
    openkal::client::FloorProp make_deck(float cx, float y, float cz, float half) {
        openkal::client::FloorProp prop;
        prop.set_patch(32, 32);
        prop.add_vertex(cx - half, y, cz - half);
        prop.add_vertex(cx - half, y, cz + half);
        prop.add_vertex(cx + half, y, cz - half);
        prop.add_vertex(cx + half, y, cz - half);
        prop.add_vertex(cx - half, y, cz + half);
        prop.add_vertex(cx + half, y, cz + half);
        prop.pad_bounds_xz(1.0f);
        return prop;
    }

    bool near(float a, float b) {
        return std::fabs(a - b) < 0.5f;
    }

}  // namespace

int main() {
    using openkal::client::FloorField;
    using openkal::client::FloorProp;
    using openkal::map::kWaterPosY;
    using openkal::test::expect;

    constexpr float kX = 16.0f;
    constexpr float kZ = 16.0f;
    constexpr float kHalf = 40.0f;

    {
        FloorField floor;
        floor.add_patch(make_flat_patch(32, 32, 1300));
        expect(near(floor.sample(kX, kZ, -1.0f), 1300.0f), "riverbed without props");
    }

    {
        FloorField floor;
        floor.add_patch(make_flat_patch(32, 32, 1300));
        floor.add_deck(make_deck(kX, 1590.0f, kZ, kHalf));
        expect(near(floor.sample(kX, kZ, -1.0f), 1590.0f), "deep river uses bridge deck");
        expect(floor.sample(kX, kZ, -1.0f) - floor.sample_terrain(kX, kZ, -1.0f) >
                   FloorField::kStandableDeckEpsilon,
               "bridge counts as standable deck");
        float standing = 1300;
        expect(floor.resolve_standing(kX, kZ, standing, standing) && near(standing, 1590),
               "combined occupancy and height query resolves the ordinary water bridge");
    }

    {
        FloorField floor;
        floor.add_patch(make_flat_patch(32, 32, 1300));
        floor.add_deck(make_deck(kX, 1590.0f, kZ, kHalf));
        floor.add_deck(make_deck(kX, 2200.0f, kZ, kHalf));
        expect(near(floor.sample(kX, kZ, -1.0f), 1590.0f), "roof over river is not preferred");
    }

    {
        FloorField floor;
        floor.add_patch(make_flat_patch(32, 32, 1300));
        floor.add_deck(make_deck(kX, 1400.0f, kZ, kHalf));
        floor.add_deck(make_deck(kX, 1590.0f, kZ, kHalf));
        expect(near(floor.sample(kX, kZ, -1.0f), 1590.0f),
               "submerged rock does not hide the bridge");
    }

    {
        FloorField floor;
        floor.add_patch(make_flat_patch(32, 32, 2000));
        floor.add_deck(make_deck(kX, 2020.0f, kZ, kHalf));
        expect(near(floor.get_floor(kX, kZ), 2020.0f), "town platform over land");
        expect(near(floor.sample(kX, kZ, 2000.0f), 2000.0f),
               "MovePlayer does not snap 20 units onto a platform");
    }

    {
        FloorField floor;
        floor.add_patch(make_flat_patch(32, 32, 2000));
        floor.add_deck(make_deck(kX, 2200.0f, kZ, kHalf));
        expect(near(floor.get_floor(kX, kZ), 2200.0f),
               "GetFloor stands on a deck within 100*METER of terrain");
        expect(near(floor.sample(kX, kZ, 2000.0f), 2000.0f),
               "MovePlayer walks under a high outdoor deck");
        expect(floor.can_occupy(kX, kZ, 2000.0f),
               "passage under a high outdoor deck is occupyable");
        float standing = 2000;
        expect(floor.resolve_standing(kX, kZ, standing, standing) && near(standing, 2000),
               "combined standing query keeps a passage below an unreachable roof");
    }

    {
        FloorField floor;
        floor.add_patch(make_flat_patch(32, 32, 1530));
        floor.add_deck(make_deck(kX, 1746.0f, kZ, kHalf));
        expect(near(floor.sample(kX, kZ, -1.0f), 1746.0f), "arched stone river bridge (bridge_01)");
    }

    {
        FloorField floor;
        floor.add_patch(make_flat_patch(32, 32, 1606));
        floor.add_deck(make_deck(kX, 1600.0f, kZ, kHalf));
        expect(near(floor.sample(kX, kZ, 1606.0f), 1606.0f),
               "bridge CLS below the bank does not pull the player into the soil");
    }

    {
        FloorField floor;
        floor.add_patch(make_flat_patch(32, 32, 140));
        floor.add_deck(make_deck(kX, 2325.0f, kZ, kHalf));
        expect(near(floor.sample(kX, kZ, 140.0f), 140.0f),
               "gorge floor cannot jump onto m_bridge06");
        expect(near(floor.sample(kX, kZ, 2325.0f), 2325.0f),
               "MovePlayer probe from the deck keeps m_bridge06");
    }

    {
        const std::string cls_gb =
            openkal::platform::join_path(openkal::test::temp_dir("floor"), "plank.gb");
        expect(openkal::test::write_cls_only_gb_v11(cls_gb), "write cls plank");
        FloorField floor;
        floor.add_patch(make_flat_patch(32, 32, 1300));
        floor.add_prop(cls_gb, openkal::engine::Mat4::translation(kX, 1590.0f, kZ), 32, 32);
        expect(near(floor.sample(kX + 2.0f, kZ + 2.0f, -1.0f), 1590.0f),
               "cls-only plank is standable over river");
    }

    {
        FloorField floor;
        floor.add_deck(make_deck(0.0f, 2100.0f, kZ, 20.0f));
        floor.add_deck(make_deck(80.0f, 1900.0f, kZ, 30.0f));
        expect(near(floor.sample(0.0f, kZ, 2100.0f), 2100.0f),
               "indoor stands on the upper ledge without KCM");
        expect(near(floor.sample(80.0f, kZ, 2100.0f), 1900.0f),
               "indoor drops to the lower floor past the ledge");
        expect(floor.can_occupy(80.0f, kZ, 2100.0f), "lower floor is occupyable from the ledge");
    }

    {
        FloorField floor;
        floor.add_deck(make_deck(kX, 1900.0f, kZ, kHalf));
        floor.add_deck(make_deck(kX, 2200.0f, kZ, kHalf));
        expect(near(floor.sample(kX, kZ, 1900.0f), 1900.0f),
               "indoor arch lintel is not the standing floor");
        expect(floor.can_occupy(kX, kZ, 1900.0f), "passage under an arch is occupyable");
        expect(near(floor.sample(kX, kZ, 2200.0f), 2200.0f),
               "standing on the lintel keeps the lintel");
    }

    {
        FloorField floor;
        floor.add_deck(make_deck(kX, 2100.0f, kZ, kHalf));
        expect(near(floor.sample(kX, kZ, 1900.0f), 1900.0f),
               "high ledge does not snap the player up");
        expect(!floor.can_occupy(kX, kZ, 1900.0f), "cannot climb a dungeon ledge from below");
        expect(near(floor.sample(kX, kZ, 2100.0f), 2100.0f), "ledge top stays standable");
        expect(floor.can_occupy(kX, kZ, 2100.0f), "ledge top is occupyable from itself");
        expect(near(floor.get_floor(kX, kZ), 2100.0f),
               "GetFloor from the sky stands on the upper deck");
        expect(near(floor.sample(kX, kZ, 1900.0f), 1900.0f),
               "MovePlayer from the pit does not snap onto the upper deck");
    }

    {
        FloorField floor;
        floor.add_deck(make_deck(kX, 1900.0f, kZ, kHalf));
        expect(floor.can_occupy(kX, kZ, 1900.0f), "indoor deck stays occupyable");
        expect(!floor.can_occupy(kX + 400.0f, kZ, 1900.0f), "indoor empty XZ is not occupyable");
        expect(near(floor.sample(kX + 400.0f, kZ, 1900.0f), 1900.0f),
               "sample keeps fallback off the indoor deck");
    }

    {
        FloorField floor;
        floor.add_patch(make_flat_patch(32, 32, 2000));
        floor.add_deck(make_deck(kX, 2200.0f, kZ, kHalf));
        expect(near(floor.sample(kX, kZ, 2000.0f), 2000.0f),
               "outdoor arch lintel is not the standing floor");
        expect(floor.can_occupy(kX, kZ, 2000.0f), "passage under an outdoor arch is occupyable");
        expect(near(floor.get_floor(kX, kZ), 2200.0f),
               "GetFloor still sees the outdoor lintel from the sky");
        expect(near(floor.sample(kX, kZ, 2200.0f), 2200.0f),
               "standing on the outdoor lintel keeps the lintel");
    }

    {
        FloorField floor;
        floor.add_patch(make_flat_patch(32, 32, 2000));
        FloorProp col;
        col.set_patch(32, 32);
        const float x0 = kX - 4.0f;
        const float x1 = kX + 4.0f;
        const float z0 = kZ - 4.0f;
        const float z1 = kZ + 4.0f;
        auto face = [&](float ax,
                        float ay,
                        float az,
                        float bx,
                        float by,
                        float bz,
                        float cxv,
                        float cy,
                        float czv,
                        float dx,
                        float dy,
                        float dz) {
            col.add_vertex(ax, ay, az);
            col.add_vertex(bx, by, bz);
            col.add_vertex(cxv, cy, czv);
            col.add_vertex(bx, by, bz);
            col.add_vertex(dx, dy, dz);
            col.add_vertex(cxv, cy, czv);
        };
        face(x0, 2000.0f, z1, x1, 2000.0f, z1, x0, 2040.0f, z1, x1, 2040.0f, z1);
        face(x1, 2000.0f, z0, x0, 2000.0f, z0, x1, 2040.0f, z0, x0, 2040.0f, z0);
        face(x1, 2000.0f, z1, x1, 2000.0f, z0, x1, 2040.0f, z1, x1, 2040.0f, z0);
        face(x0, 2000.0f, z0, x0, 2000.0f, z1, x0, 2040.0f, z0, x0, 2040.0f, z1);
        col.pad_bounds_xz(1.0f);
        floor.add_blocker_deck(std::move(col));
        float t = 0.0f;
        expect(floor.raycast(kX - 20.0f,
                             2011.5f,
                             kZ,
                             1.0f,
                             0.0f,
                             0.0f,
                             40.0f,
                             t,
                             openkal::client::RayHitFilter::Walls),
               "outdoor column blocks a body-height ray");
        expect(t > 11.0f && t < 17.0f, "hit the near face of the outdoor column");
        expect(near(floor.sample(kX, kZ, 2000.0f), 2000.0f),
               "terrain under the column stays the standing floor");
    }

    {
        FloorField floor;
        FloorProp wall;
        wall.set_patch(-1, -1);
        wall.add_vertex(kX - kHalf, 0.0f, kZ + 20.0f);
        wall.add_vertex(kX + kHalf, 0.0f, kZ + 20.0f);
        wall.add_vertex(kX - kHalf, 40.0f, kZ + 20.0f);
        wall.add_vertex(kX + kHalf, 0.0f, kZ + 20.0f);
        wall.add_vertex(kX + kHalf, 40.0f, kZ + 20.0f);
        wall.add_vertex(kX - kHalf, 40.0f, kZ + 20.0f);
        wall.pad_bounds_xz(1.0f);
        floor.add_blocker_deck(std::move(wall));
        float t = 0.0f;
        expect(floor.raycast(
                   kX, 11.5f, kZ, 0.0f, 0.0f, 1.0f, 40.0f, t, openkal::client::RayHitFilter::Walls),
               "vertical dungeon wall blocks a body-height ray");
        expect(std::fabs(t - 20.0f) < 0.5f, "wall hit distance");
        expect(
            !floor.raycast(
                kX, 11.5f, kZ, 0.0f, 0.0f, 1.0f, 40.0f, t, openkal::client::RayHitFilter::Ceilings),
            "ceilings filter ignores a vertical wall");
        {
            // Body radius 4: a step whose center would stop 2 units short of the
            // wall still collides (hit 20 < dist 18 + radius 4).
            constexpr float kRadius = 4.0f;
            constexpr float kDist = 18.0f;
            float body_hit = kDist + kRadius;
            expect(floor.raycast(kX,
                                 11.5f,
                                 kZ,
                                 0.0f,
                                 0.0f,
                                 1.0f,
                                 kDist + kRadius,
                                 body_hit,
                                 openkal::client::RayHitFilter::Walls) &&
                       body_hit < kDist + kRadius,
                   "body-radius step is blocked before the wall");
        }
        expect(!floor.raycast(
                   kX, 11.5f, kZ, 1.0f, 0.0f, 0.0f, 10.0f, t, openkal::client::RayHitFilter::Walls),
               "wall is missed when walking parallel");
        expect(floor.segment_blocked(kX, 11.5f, kZ, kX, 11.5f, kZ + 40.0f),
               "nameplate LOS is blocked by a dungeon wall");
        expect(!floor.segment_blocked(kX, 11.5f, kZ, kX, 11.5f, kZ + 10.0f),
               "nameplate LOS stays open in the same room");
        for (float distance : {19.5f, 20.0f, 20.25f, 20.5f, 20.75f, 40.0f}) {
            for (float slack : {0.0f, 0.25f, 0.5f, 1.0f}) {
                float nearest = distance;
                const bool reference = floor.raycast(kX, 11.5f, kZ, 0, 0, 1, distance, nearest) &&
                                       nearest + slack < distance;
                expect(floor.segment_blocked(kX, 11.5f, kZ, kX, 11.5f, kZ + distance, slack) ==
                           reference,
                       "early occlusion agrees with the closest hit at endpoint slack boundaries");
            }
        }
        expect(floor.segment_blocked(kX, 11.5f, kZ + 40, kX, 11.5f, kZ),
               "occlusion also blocks reversed rays");
        expect(!floor.segment_blocked(kX, 11.5f, kZ + 21, kX, 11.5f, kZ + 40) &&
                   floor.segment_blocked(kX, 11.5f, kZ, kX, 11.5f, kZ + 40),
               "camera and player on opposite sides require different nameplate visibility");
    }

    {
        FloorField floor;
        FloorProp ceiling;
        ceiling.set_patch(-1, -1);
        ceiling.add_vertex(kX - kHalf, 80.0f, kZ - kHalf);
        ceiling.add_vertex(kX + kHalf, 80.0f, kZ - kHalf);
        ceiling.add_vertex(kX - kHalf, 80.0f, kZ + kHalf);
        ceiling.add_vertex(kX + kHalf, 80.0f, kZ - kHalf);
        ceiling.add_vertex(kX + kHalf, 80.0f, kZ + kHalf);
        ceiling.add_vertex(kX - kHalf, 80.0f, kZ + kHalf);
        ceiling.pad_bounds_xz(1.0f);
        floor.add_blocker_deck(std::move(ceiling));
        float t = 0.0f;
        expect(floor.raycast(kX, 12.0f, kZ, 0.0f, 1.0f, 0.0f, 200.0f, t),
               "down-facing ceiling is a camera hit");
        expect(std::fabs(t - 68.0f) < 0.5f, "ceiling distance from look height");
        expect(floor.raycast(kX,
                             12.0f,
                             kZ,
                             0.0f,
                             1.0f,
                             0.0f,
                             200.0f,
                             t,
                             openkal::client::RayHitFilter::Ceilings),
               "ceilings filter hits a down-facing slab");
        expect(
            !floor.raycast(
                kX, 12.0f, kZ, 1.0f, 0.0f, 0.0f, 10.0f, t, openkal::client::RayHitFilter::Ceilings),
            "ceilings filter ignores a miss beside the slab");
        expect(!floor.raycast(kX, 12.0f, kZ, 1.0f, 0.0f, 0.0f, 10.0f, t),
               "horizontal miss beside the ceiling");
    }

    {
        FloorField floor;
        auto add_column = [](FloorField& f, float cx, float cz) {
            FloorProp col;
            col.set_patch(-1, -1);
            const float x0 = cx - 4.0f;
            const float x1 = cx + 4.0f;
            const float z0 = cz - 4.0f;
            const float z1 = cz + 4.0f;
            const float y0 = 0.0f;
            const float y1 = 40.0f;
            auto face = [&](float ax,
                            float ay,
                            float az,
                            float bx,
                            float by,
                            float bz,
                            float cxv,
                            float cy,
                            float czv,
                            float dx,
                            float dy,
                            float dz) {
                col.add_vertex(ax, ay, az);
                col.add_vertex(bx, by, bz);
                col.add_vertex(cxv, cy, czv);
                col.add_vertex(bx, by, bz);
                col.add_vertex(dx, dy, dz);
                col.add_vertex(cxv, cy, czv);
            };
            face(x0, y0, z1, x1, y0, z1, x0, y1, z1, x1, y1, z1);
            face(x1, y0, z0, x0, y0, z0, x1, y1, z0, x0, y1, z0);
            face(x1, y0, z1, x1, y0, z0, x1, y1, z1, x1, y1, z0);
            face(x0, y0, z0, x0, y0, z1, x0, y1, z0, x0, y1, z1);
            col.pad_bounds_xz(1.0f);
            f.add_blocker_deck(std::move(col));
        };
        add_column(floor, -12.0f, 0.0f);
        add_column(floor, 12.0f, 0.0f);
        float t = 0.0f;
        expect(floor.raycast(-24.0f,
                             11.5f,
                             0.0f,
                             1.0f,
                             0.0f,
                             0.0f,
                             40.0f,
                             t,
                             openkal::client::RayHitFilter::Walls),
               "statue body blocks a body-height ray");
        expect(t > 7.0f && t < 9.0f, "hit the near face of the first statue");
        expect(!floor.raycast(0.0f,
                              11.5f,
                              0.0f,
                              0.0f,
                              0.0f,
                              1.0f,
                              20.0f,
                              t,
                              openkal::client::RayHitFilter::Walls),
               "gap between the two statues stays walkable");
        expect(floor.segment_blocked(-24.0f, 11.5f, 0.0f, 0.0f, 11.5f, 0.0f),
               "nameplate LOS is blocked by a statue body");
        expect(!floor.segment_blocked(0.0f, 11.5f, 0.0f, 0.0f, 11.5f, 20.0f),
               "nameplate LOS stays open between the statues");
    }

    {
        FloorField floor;
        floor.add_patch(make_flat_patch(32, 32, 2000));
        FloorProp ramp;
        ramp.set_patch(32, 32);
        // 45° wall (rise 40 / run 40). cos(45°) = 0.707 < SLIDE_SLOPE 0.766.
        ramp.add_vertex(kX - kHalf, 2000.0f, kZ);
        ramp.add_vertex(kX - kHalf, 2040.0f, kZ + 40.0f);
        ramp.add_vertex(kX + kHalf, 2000.0f, kZ);
        ramp.add_vertex(kX + kHalf, 2000.0f, kZ);
        ramp.add_vertex(kX - kHalf, 2040.0f, kZ + 40.0f);
        ramp.add_vertex(kX + kHalf, 2040.0f, kZ + 40.0f);
        ramp.pad_bounds_xz(1.0f);
        floor.add_deck(std::move(ramp));
        expect(near(floor.get_floor(kX, kZ + 20.0f), 2000.0f),
               "steep house wall is not a walkable roof ramp");
    }

    {
        FloorField floor;
        FloorProp ramp;
        ramp.set_patch(0, 0);
        // ~11° slope: rise 20 over run 100. cos(atan(0.2)) ≈ 0.98 > SLIDE_SLOPE.
        ramp.add_vertex(0.0f, 23000.0f, 0.0f);
        ramp.add_vertex(0.0f, 23020.0f, 100.0f);
        ramp.add_vertex(40.0f, 23000.0f, 0.0f);
        ramp.add_vertex(40.0f, 23000.0f, 0.0f);
        ramp.add_vertex(0.0f, 23020.0f, 100.0f);
        ramp.add_vertex(40.0f, 23020.0f, 100.0f);
        ramp.pad_bounds_xz(1.0f);
        floor.add_deck(std::move(ramp));
        expect(near(floor.get_floor(20.0f, 80.0f), 23016.0f),
               "GetFloor from the sky stands on a long indoor ramp");
        expect(near(floor.sample(20.0f, 80.0f, 0.0f), 0.0f),
               "MovePlayer from Y=0 does not snap up a long ramp");
        expect(!floor.can_occupy(20.0f, 80.0f, 0.0f),
               "a long ramp is not occupyable from below INNER_HEIGHT");
        float t_hit = 4000.0f;
        expect(floor.raycast_walkable(20.0f, 23100.0f, 80.0f, 0.0f, -1.0f, 0.0f, 200.0f, t_hit),
               "camera pick hits the indoor ramp");
        expect(t_hit > 70.0f && t_hit < 90.0f, "pick distance reaches the ramp surface");
    }

    {
        FloorField floor;
        const float bases[] = {-1024, -512, 0, 512, 1024, 65536};
        const auto wall = [&](float x, float z, int patch, int row) {
            FloorProp prop;
            prop.set_patch(patch, row);
            for (const auto& vertex : {std::array<float, 3>{x, 0, z - 20},
                                       {x, 32, z - 20},
                                       {x, 0, z + 20},
                                       {x, 0, z + 20},
                                       {x, 32, z - 20},
                                       {x, 32, z + 20}}) {
                prop.add_vertex(vertex[0], vertex[1], vertex[2]);
            }
            prop.pad_bounds_xz(1);
            floor.add_blocker_deck(std::move(prop));
        };
        for (int i = 0; i < 6; ++i) {
            const float base = bases[i];
            wall(base + 24, base, i, 11);
            wall(base + 8, base, i, 10);
            auto deck = make_deck(base, 10, base, 20);
            deck.set_patch(i, 12);
            floor.add_deck(std::move(deck));
        }
        for (int i = 0; i < 6; ++i) {
            const float base = bases[i];
            float hit = 0;
            expect(floor.raycast(base - 8, 16, base, 1, 0, 0, 64, hit) && near(hit, 16),
                   "short rays retain the nearest wall across signed spatial-grid boundaries");
            expect(floor.raycast(base + 40, 16, base, -1, 0, 0, 64, hit) && near(hit, 16),
                   "reversed rays retain the nearest wall independent of insertion order");
            expect(floor.raycast(base - 100000, 16, base, 1, 0, 0, 200000, hit) &&
                       near(hit, 100008),
                   "long-ray fallback agrees with analytical wall distance");
            expect(floor.raycast_walkable(base, 32, base, 0, -1, 0, 50, hit) && near(hit, 22),
                   "walkable rays use the deck grid with the same nearest-hit result");
            floor.remove_patch(i, 10);
            expect(floor.raycast(base - 8, 16, base, 1, 0, 0, 64, hit) && near(hit, 32),
                   "removing a patch rebuilds blocker indices before later queries");
            floor.remove_patch(i, 11);
            expect(!floor.raycast(base - 8, 16, base, 1, 0, 0, 64, hit),
                   "removed blockers leave no stale spatial-grid hits");
        }
    }
    expect(kWaterPosY > 1500.0f && kWaterPosY < 1700.0f, "water plane in expected band");
    return openkal::test::g_failures == 0 ? 0 : 1;
}
