/*
 * OpenKal - an open source KalOnline client and server
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "ObjectMask.h"
#include "layers.h"
#include "openkal/map/AreaMapCodec.h"
#include "openkal/map/Coords.h"
#include "openkal/map/MapCatalog.h"
#include "openkal/terrain/Floor.h"
#include "test_util.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

    openkal::area_editor::ObjectMaskMesh
    make_box(float min_x, float min_y, float min_z, float max_x, float max_y, float max_z) {
        openkal::area_editor::ObjectMaskMesh mesh;
        mesh.positions = {{min_x, min_y, min_z},
                          {max_x, min_y, min_z},
                          {max_x, min_y, max_z},
                          {min_x, min_y, max_z},
                          {min_x, max_y, min_z},
                          {max_x, max_y, min_z},
                          {max_x, max_y, max_z},
                          {min_x, max_y, max_z}};
        mesh.indices = {0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7, 0, 1, 5, 0, 5, 4,
                        1, 2, 6, 1, 6, 5, 2, 3, 7, 2, 7, 6, 3, 0, 4, 3, 4, 7};
        return mesh;
    }

    openkal::area_editor::ObjectMaskMesh
    make_walls(float min_x, float min_y, float min_z, float max_x, float max_y, float max_z) {
        openkal::area_editor::ObjectMaskMesh mesh =
            make_box(min_x, min_y, min_z, max_x, max_y, max_z);
        mesh.indices = {0, 1, 5, 0, 5, 4, 1, 2, 6, 1, 6, 5, 2, 3, 7, 2, 7, 6, 3, 0, 4, 3, 4, 7};
        return mesh;
    }

    [[nodiscard]] bool has_cell(const openkal::area_editor::ObjectMask& mask, int x, int y) {
        const std::uint32_t cell =
            static_cast<std::uint32_t>(y * openkal::map::kMovementCellCount + x);
        for (const std::uint32_t candidate : mask.footprint_cells) {
            if (candidate == cell) {
                return true;
            }
        }
        return false;
    }

    int check_user_data() {
        using openkal::area_editor::is_fully_blocked;
        using openkal::test::expect;

        openkal::map::AreaCell known_blocked;
        const std::string known_blocked_path =
            std::string(OPENKAL_TEST_RESOURCE_ROOT) + "/RuntimeData/areas/n_046_027.akarea";
        expect(openkal::map::AreaMapCodec::load(known_blocked_path, known_blocked),
               "known fully blocked area loads");
        expect(is_fully_blocked(known_blocked), "n_046_027 is classified as fully blocked");

        openkal::map::MapCatalog map_catalog;
        const std::string map_catalog_path =
            std::string(OPENKAL_TEST_RESOURCE_ROOT) + "/config/mapinfo.json";
        expect(map_catalog.load(map_catalog_path), "editor world layout map definition loads");
        bool contains_narootuh = false;
        bool contains_unlisted_extra = false;
        for (const auto& entry : map_catalog.entries()) {
            contains_narootuh =
                contains_narootuh || (entry.kind() == 1 && entry.x() == 31 && entry.y() == 31);
            contains_unlisted_extra =
                contains_unlisted_extra || (entry.kind() == 1 && entry.x() == 10 && entry.y() == 2);
        }
        expect(contains_narootuh, "map definition outdoor layout contains Narootuh");
        expect(!contains_unlisted_extra,
               "files outside map definition are excluded from the world layout");
        return openkal::test::g_failures == 0 ? 0 : 1;
    }

}  // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::string_view(argv[1]) == "--user-data") {
        return check_user_data();
    }

    using openkal::area_editor::AttrEditor;
    using openkal::area_editor::build_object_mask;
    using openkal::area_editor::clip_footprint_to_rect;
    using openkal::area_editor::is_fully_blocked;
    using openkal::area_editor::LayerId;
    using openkal::area_editor::ObjectMask;
    using openkal::area_editor::ObjectMaskMesh;
    using openkal::area_editor::pick_object_mask;
    using openkal::test::expect;

    std::vector<ObjectMaskMesh> tree_geometry;
    tree_geometry.push_back(make_box(100.0f, 0.0f, 200.0f, 132.0f, 300.0f, 232.0f));
    ObjectMaskMesh canopy = make_box(60.0f, 100.0f, 160.0f, 172.0f, 400.0f, 272.0f);
    canopy.alpha_surface = true;
    tree_geometry.push_back(std::move(canopy));
    const ObjectMask tree = build_object_mask(
        "tree", tree_geometry, openkal::engine::Mat4::identity(), 0.0f, 0.0f, 2048.0f, 0.0f, true);
    expect(!tree.footprint_cells.empty(), "tree footprint is generated");
    expect(pick_object_mask({tree}, 70, 170) != nullptr, "tree canopy remains pickable");

    bool footprint_stays_on_trunk = true;
    for (const std::uint32_t cell : tree.footprint_cells) {
        const int x = static_cast<int>(cell % openkal::map::kMovementCellCount);
        const int y = static_cast<int>(cell / openkal::map::kMovementCellCount);
        footprint_stays_on_trunk =
            footprint_stays_on_trunk && x >= 100 && x < 132 && y >= 200 && y < 232;
    }
    expect(footprint_stays_on_trunk, "head-height footprint excludes the tree canopy");

    const ObjectMask house =
        build_object_mask("house",
                          {make_box(300.0f, 0.0f, 300.0f, 400.0f, 400.0f, 380.0f)},
                          openkal::engine::Mat4::identity(),
                          0.0f,
                          0.0f,
                          2048.0f,
                          0.0f,
                          false);
    expect(house.footprint_cells.size() == 100u * 80u, "house outline is filled completely");
    expect(!house.collision_contours.empty(), "house keeps a collision contour");
    expect(!house.display_contours.empty(), "house keeps a display contour");
    const std::vector<ObjectMask> house_masks{house};
    expect(pick_object_mask(house_masks, 350, 340) == &house_masks.front(),
           "house interior is pickable");

    const ObjectMask walls =
        build_object_mask("walls",
                          {make_walls(10.0f, 0.0f, 20.0f, 30.0f, 300.0f, 60.0f)},
                          openkal::engine::Mat4::identity(),
                          0.0f,
                          0.0f,
                          2048.0f,
                          0.0f,
                          false);
    expect(walls.footprint_cells.size() == 20u * 40u, "wall cross-section fills the interior");
    const std::vector<ObjectMask> wall_masks{walls};
    expect(pick_object_mask(wall_masks, 20, 40) == &wall_masks.front(),
           "wall interior is pickable");
    expect(pick_object_mask(wall_masks, 0, 0) == nullptr, "empty ground is not an object");

    std::vector<ObjectMaskMesh> ell_parts;
    ell_parts.push_back(make_box(0.0f, 0.0f, 0.0f, 40.0f, 250.0f, 80.0f));
    ell_parts.push_back(make_box(40.0f, 0.0f, 0.0f, 80.0f, 250.0f, 40.0f));
    const ObjectMask ell = build_object_mask(
        "ell", ell_parts, openkal::engine::Mat4::identity(), 0.0f, 0.0f, 2048.0f, 0.0f, false);
    expect(has_cell(ell, 10, 10) && has_cell(ell, 60, 10) && has_cell(ell, 10, 60),
           "L footprint covers both wings");
    expect(!has_cell(ell, 60, 60), "L footprint leaves the inner corner empty");
    expect(ell.footprint_cells.size() == 80u * 40u + 40u * 40u, "L footprint is not a convex hull");
    expect(pick_object_mask({ell}, 60, 60) == nullptr,
           "L notch is not part of the display silhouette");

    const std::vector<std::uint32_t> clipped = clip_footprint_to_rect(house, 350, 300, 450, 379);
    bool clip_inside = clipped.size() == 50u * 80u;
    for (const std::uint32_t cell : clipped) {
        const int x = static_cast<int>(cell % openkal::map::kMovementCellCount);
        const int y = static_cast<int>(cell / openkal::map::kMovementCellCount);
        clip_inside = clip_inside && x >= 350 && x <= 399 && y >= 300 && y <= 379;
    }
    expect(clip_inside, "rectangle clip keeps only the house cells inside the selection");

    {
        using openkal::area_editor::movement_clearance_contours;
        constexpr int kStride = openkal::map::kMovementCellCount;
        ObjectMask left;
        left.min_x = 0;
        left.max_x = 1;
        left.min_y = left.max_y = 1;
        left.footprint_cells = {kStride, kStride + 1u};
        ObjectMask right = left;
        right.min_x = 1;
        right.max_x = 2;
        right.footprint_cells = {kStride + 1u, kStride + 2u};
        const auto contours = movement_clearance_contours({left, right});
        bool exterior_only = contours.size() == 1 && contours.front().size() == 5;
        for (const auto& contour : contours) {
            for (const auto& point : contour) {
                exterior_only &= (point.x == 0 || point.x == 3) && (point.y == 1 || point.y == 2);
            }
        }
        expect(exterior_only, "overlapping clearance masks share one exterior contour");
    }

    {
        namespace terrain = openkal::terrain;
        namespace map = openkal::map;
        constexpr int patch = openkal::coords::kDefaultOriginPatch - 1;
        constexpr float origin = -static_cast<float>(map::kWorldSize);
        std::vector<ObjectMask> masks{
            build_object_mask("boardwalk",
                              {make_box(4.0f, 1490.0f, 8.0f, 32.0f, 1600.0f, 32.0f)},
                              openkal::engine::Mat4::translation(origin, 0.0f, origin),
                              origin,
                              origin,
                              static_cast<float>(map::kWorldSize),
                              1600.0f,
                              false)};
        terrain::HeightPatch water;
        water.patch_x = water.patch_y = patch;
        water.heights.assign(map::kHeightSamples * map::kHeightSamples, 1500);
        terrain::FloorField floor;
        floor.add_patch(std::move(water));
        terrain::FloorProp deck;
        deck.add_vertex(origin + 8.0f, 1580.0f, origin + 7.0f);
        deck.add_vertex(origin + 8.0f, 1580.0f, origin + 32.0f);
        deck.add_vertex(origin + 28.5f, 1580.0f, origin + 7.0f);
        deck.add_vertex(origin + 28.5f, 1580.0f, origin + 7.0f);
        deck.add_vertex(origin + 8.0f, 1580.0f, origin + 32.0f);
        deck.add_vertex(origin + 28.5f, 1580.0f, origin + 32.0f);
        floor.add_deck(std::move(deck));

        openkal::area_editor::restrict_movement_clearance(masks, floor, origin, origin);
        const auto& mask = masks.front();
        expect(mask.edit_cells(false).size() == 42,
               "automatic paint still uses the object footprint");
        const auto clearance = clip_footprint_to_rect(mask, 1, 2, 7, 7, true);
        expect(clearance == mask.edit_cells(true) && clearance.size() == 8,
               "object and rectangle clearance keep one full subcell along the water edge");

        const auto outlines = openkal::area_editor::movement_clearance_contours(masks);
        bool outline_matches = outlines.size() == 1 && outlines.front().size() == 5;
        for (const auto& outline : outlines) {
            for (const auto& point : outline) {
                outline_matches &= (point.x == 4 || point.x == 6) && (point.y == 3 || point.y == 7);
            }
        }
        expect(outline_matches, "movement clearance outline follows the inset subcells");

        AttrEditor movement(map::AreaCell{});
        movement.set_active(LayerId::NotMoveable);
        expect(movement.paint_subcell_mask(mask.edit_cells(false), false) == 42,
               "prepare blocked boardwalk and surrounding water");
        expect(movement.paint_subcell_mask(clearance, true) == 8,
               "clear only supported boardwalk cells");
        expect(movement.grid().not_moveable_at_movement_cell(1, 3) &&
                   movement.grid().not_moveable_at_movement_cell(2, 3) &&
                   movement.grid().not_moveable_at_movement_cell(3, 3) &&
                   !movement.grid().not_moveable_at_movement_cell(4, 3) &&
                   movement.grid().not_moveable_at_movement_cell(6, 3) &&
                   movement.grid().not_moveable_at_movement_cell(4, 2),
               "removal keeps a blocked perimeter on supported boardwalk edges");
        const auto before_second_erase = movement.grid().movement_mask(0, 0);
        expect(movement.paint_subcell_mask({3u * map::kMovementCellCount + 3u}, true) == 1 &&
                   movement.undo() && movement.grid().movement_mask(0, 0) == before_second_erase,
               "undo preserves the earlier fine boardwalk clearance");
        std::vector<std::uint8_t> overlay;
        const int overlay_size = movement.build_overlay_rgba(overlay);
        expect(overlay_size == map::kMovementCellCount &&
                   overlay[(3 * overlay_size + 3) * 4 + 3] != 0 &&
                   overlay[(3 * overlay_size + 4) * 4 + 3] == 0,
               "overlay distinguishes the blocked margin from the fine clearance");
        movement.toggle_visibility(LayerId::NotMoveable);
        movement.set_active(LayerId::Blocked);
        expect(movement.build_overlay_rgba(overlay) == map::kCellCount,
               "blocked-only overlay uses the eight-pixel grid");

        std::vector<ObjectMask> narrow_masks{
            build_object_mask("narrow boardwalk",
                              {make_box(16.0f, 1490.0f, 8.0f, 24.0f, 1600.0f, 32.0f)},
                              openkal::engine::Mat4::translation(origin, 0.0f, origin),
                              origin,
                              origin,
                              static_cast<float>(map::kWorldSize),
                              1600.0f,
                              false)};
        openkal::area_editor::restrict_movement_clearance(narrow_masks, floor, origin, origin);
        expect(!narrow_masks.front().edit_cells(false).empty() &&
                   narrow_masks.front().edit_cells(true).empty() &&
                   openkal::area_editor::movement_clearance_contours(narrow_masks).empty(),
               "a narrow boardwalk retains its entire movement mask");

        terrain::HeightPatch dry_ground;
        dry_ground.patch_x = dry_ground.patch_y = patch;
        dry_ground.heights.assign(map::kHeightSamples * map::kHeightSamples, 1620);
        terrain::FloorField dry_floor;
        dry_floor.add_patch(std::move(dry_ground));
        openkal::area_editor::restrict_movement_clearance(masks, dry_floor, origin, origin);
        expect(mask.edit_cells(true) == mask.edit_cells(false),
               "automatic removal uses the ordinary footprint on dry ground");
    }

    openkal::map::AreaCell grid;
    AttrEditor editor(std::move(grid));
    editor.set_active(LayerId::Safety);
    const std::size_t changed = editor.paint_subcell_mask(tree.footprint_cells, false);
    expect(changed == 5u * 4u, "subcell footprint collapses to unique zone tiles");
    expect(editor.tile_has_layer(13, 26, LayerId::Safety), "active zone is painted under tree");
    expect(editor.undo(), "automatic paint is one undo step");
    expect(!editor.tile_has_layer(13, 26, LayerId::Safety), "undo restores automatic zone paint");

    editor.set_active(LayerId::Blocked);
    const std::vector<std::uint32_t> exact_cells = {10u * openkal::map::kMovementCellCount + 9u,
                                                    10u * openkal::map::kMovementCellCount + 10u};
    expect(editor.paint_subcell_mask(exact_cells, false) == 2u,
           "collision layer keeps subcell precision");
    expect(editor.grid().blocked_at_subcell(4, 5) && editor.grid().blocked_at_subcell(5, 5),
           "collision subcells are painted");
    expect(editor.undo(), "collision mask is one undo step");
    expect(!editor.grid().blocked_at_subcell(4, 5) && !editor.grid().blocked_at_subcell(5, 5),
           "collision undo restores both subcells");

    openkal::map::AreaCell fully_blocked;
    fully_blocked.ensure_full_attrs();
    fully_blocked.set_blocked_masks(std::vector<openkal::map::AreaCell::SubcellMask>(
        static_cast<std::size_t>(openkal::map::kTileCount) * openkal::map::kTileCount, 0xffffu));
    expect(is_fully_blocked(fully_blocked), "full collision masks classify the area as blocked");
    fully_blocked.set_blocked_mask(12, 34, 0xfffeu);
    expect(!is_fully_blocked(fully_blocked),
           "one open subcell keeps the area out of the fully blocked class");

    return openkal::test::g_failures == 0 ? 0 : 1;
}
