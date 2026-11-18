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
#include "openkal/map/AreaMapCodec.h"
#include "openkal/map/Attributes.h"
#include "openkal/map/WorldGrid.h"
#include "openkal/platform/File.h"
#include "test_util.h"

#include <array>
#include <cstdint>
#include <string>

int main() {
    using openkal::test::expect;

    openkal::map::AreaCell cell;
    const bool loaded = openkal::map::AreaMapCodec::load(OPENKAL_AREA_FIXTURE_PATH, cell);
    expect(loaded, "load project-owned area fixture");
    if (!loaded) {
        return 1;
    }

    expect(cell.cell_x() == 0 && cell.cell_y() == 0, "synthetic area uses cell zero");
    const std::size_t tiles = 256u * 256u;
    const bool complete = cell.attr_count() == tiles && cell.blocked_masks().size() == tiles &&
                          cell.not_moveable_masks().size() == tiles;
    expect(complete, "all three area layers cover the complete cell");
    if (!complete) {
        return 1;
    }
    expect(cell.at(0, 0) == 0 && cell.blocked_mask(0, 0) == 0 && cell.not_moveable_mask(0, 0) == 0,
           "untouched tile has empty layers");

    const std::array<std::uint32_t, 6> flags = {openkal::map::kAttrPortal,
                                                openkal::map::kAttrVillage,
                                                openkal::map::kAttrSafety,
                                                openkal::map::kAttrAttackCastle,
                                                openkal::map::kAttrDefenceCastle,
                                                openkal::map::kAttrObject};
    for (int x = 1; x <= static_cast<int>(flags.size()); ++x) {
        expect(cell.at(x, 1) == flags[static_cast<std::size_t>(x - 1)],
               "individual zone flags survive loading");
        expect(cell.blocked_mask(x, 1) == 0 && cell.not_moveable_mask(x, 1) == 0,
               "zone flags leave collision layers empty");
    }
    expect(cell.at(7, 1) ==
               (openkal::map::kAttrPortal | openkal::map::kAttrVillage | openkal::map::kAttrSafety),
           "combined zone flags survive loading");

    expect(cell.blocked_mask(2, 4) == 0xffffu && cell.not_moveable_mask(2, 4) == 0 &&
               cell.walkable(2, 4),
           "blocked geometry alone does not restrict movement");
    expect(cell.blocked_mask(3, 4) == 0 && cell.not_moveable_mask(3, 4) == 0xffffu &&
               !cell.walkable(3, 4),
           "movement restriction does not require blocked geometry");
    expect(cell.blocked_mask(5, 4) == 0xffffu && cell.not_moveable_mask(5, 4) == 0xffffu,
           "both collision layers can fill the same tile");

    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            expect(cell.blocked_at_subcell(16 + x, 16 + y) == (x == y),
                   "blocked diagonal preserves subcell orientation");
            expect(cell.not_moveable_at_subcell(16 + x, 16 + y) == (x + y == 3),
                   "not-moveable diagonal preserves independent orientation");
        }
    }
    for (int bit = 0; bit < 16; ++bit) {
        const auto mask = static_cast<openkal::map::AreaCell::SubcellMask>(1u << bit);
        expect(cell.blocked_mask(16 + bit, 8) == mask && cell.not_moveable_mask(16 + bit, 8) == 0,
               "every blocked mask bit loads independently");
        expect(cell.not_moveable_mask(16 + bit, 9) == mask && cell.blocked_mask(16 + bit, 9) == 0,
               "every not-moveable mask bit loads independently");
        const int sx = (16 + bit) * 4 + bit % 4;
        expect(cell.blocked_at_subcell(sx, 32 + bit / 4) &&
                   cell.not_moveable_at_subcell(sx, 36 + bit / 4),
               "each isolated bit maps to its row-major subcell");
    }
    bool rectangles_match = true;
    for (int y = 63; y <= 68; ++y) {
        for (int x = 63; x <= 128; ++x) {
            const bool in_rows = y >= 64 && y < 68;
            const auto blocked = in_rows && x >= 64 && x < 96 ? 0xffffu : 0u;
            const auto movement = in_rows && x >= 96 && x < 128 ? 0xffffu : 0u;
            rectangles_match &=
                cell.blocked_mask(x, y) == blocked && cell.not_moveable_mask(x, y) == movement;
        }
    }
    expect(rectangles_match, "repeated mask runs preserve rectangle edges and layer separation");
    expect(cell.blocked_mask(255, 255) == 0x8001u && cell.not_moveable_mask(255, 255) == 0x0180u &&
               cell.at(255, 255) == (openkal::map::kAttrPortal | openkal::map::kAttrObject |
                                     openkal::map::kAttrNotMoveable),
           "last tile preserves zone flags and distinct partial masks");

    int blocked_full = 0;
    int blocked_partial = 0;
    int movement_full = 0;
    int movement_partial = 0;
    int blocked_subcells = 0;
    int movement_subcells = 0;
    int overlapping_subcells = 0;
    int walkable = 0;
    bool movement_flags_consistent = true;
    std::array<int, 6> flag_counts{};
    for (int y = 0; y < 256; ++y) {
        for (int x = 0; x < 256; ++x) {
            const auto blocked = cell.blocked_mask(x, y);
            const auto movement = cell.not_moveable_mask(x, y);
            blocked_full += blocked == 0xffffu;
            blocked_partial += blocked != 0 && blocked != 0xffffu;
            movement_full += movement == 0xffffu;
            movement_partial += movement != 0 && movement != 0xffffu;
            walkable += cell.walkable(x, y);
            movement_flags_consistent &=
                cell.has_flag(x, y, openkal::map::kAttrNotMoveable) == (movement != 0);
            for (std::size_t i = 0; i < flags.size(); ++i) {
                flag_counts[i] += cell.has_flag(x, y, flags[i]);
            }
            for (int bit = 0; bit < 16; ++bit) {
                blocked_subcells += (blocked & (1u << bit)) != 0;
                movement_subcells += (movement & (1u << bit)) != 0;
                overlapping_subcells += (blocked & movement & (1u << bit)) != 0;
            }
        }
    }
    expect(blocked_full == 130 && blocked_partial == 18, "synthetic blocked tile counts");
    expect(movement_full == 130 && movement_partial == 18, "synthetic movement tile counts");
    expect(blocked_subcells == 2102 && movement_subcells == 2102 && overlapping_subcells == 16,
           "layer population and overlap match the independent test patterns");
    expect(walkable == 65388, "only the 148 movement-layer tiles restrict walking");
    expect(movement_flags_consistent, "tile movement flags agree with their subcell layer");
    expect(flag_counts == std::array<int, 6>{3, 2, 2, 1, 1, 2},
           "zone counts contain only the deliberately placed flags");

    const std::string output = openkal::test::temp_dir("area_map_fixture") + "/roundtrip.akarea";
    const bool saved = openkal::map::AreaMapCodec::save(output, cell);
    expect(saved, "save loaded area fixture");
    if (saved) {
        const auto original = openkal::platform::File::read(OPENKAL_AREA_FIXTURE_PATH);
        const auto roundtrip = openkal::platform::File::read(output);
        expect(original && roundtrip && *original == *roundtrip,
               "all fixture layers serialize back to identical bytes");
    }

    auto refined = cell;
    refined.set_not_moveable_at_movement_cell(7, 7, true);
    expect(refined.has_movement_detail() && refined.movement_mask(0, 0) == (std::uint64_t{1} << 63),
           "one 4x4 cell can change independently at the last bit of a tile");
    expect(refined.movement_mask(3, 4) == ~std::uint64_t{0},
           "refining an existing file preserves its coarse movement masks");

    const std::string fine_path = openkal::test::temp_dir("area_map_fixture") + "/fine.akarea";
    openkal::map::AreaCell fine;
    const bool fine_loaded = openkal::map::AreaMapCodec::save(fine_path, refined) &&
                             openkal::map::AreaMapCodec::load(fine_path, fine);
    expect(fine_loaded, "save and reload a version 3 area");
    if (fine_loaded) {
        const auto bytes = openkal::platform::File::read(fine_path);
        expect(bytes && bytes->size() >= 6 && (*bytes)[4] == 3 && (*bytes)[5] == 0,
               "four-pixel movement detail uses version 3");
        expect(fine.movement_masks() == refined.movement_masks() &&
                   fine.blocked_masks() == cell.blocked_masks(),
               "version 3 preserves both collision layers at their own resolution");
        openkal::map::WorldGrid grid;
        grid.set_cell(0, 0, std::move(fine));
        expect(grid.not_moveable_world_pixel(28, 28) && grid.not_moveable_world_pixel(31, 31) &&
                   !grid.not_moveable_world_pixel(27, 28) &&
                   !grid.not_moveable_world_pixel(28, 32) && !grid.walkable_world_pixel(28, 28) &&
                   grid.walkable_world_pixel(27, 28),
               "server and player movement agree at four-pixel cell boundaries");
    }

    return openkal::test::g_failures == 0 ? 0 : 1;
}
