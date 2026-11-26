/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/map/WorldGrid.h"

#include "openkal/map/Attributes.h"
#include "openkal/map/Coords.h"

namespace openkal::map {

    void WorldGrid::set_cell(int cell_x, int cell_y, AreaCell grid) {
        grid.set_cell_x(cell_x);
        grid.set_cell_y(cell_y);
        m_cells[cell_slot_index(cell_x, cell_y)] = std::move(grid);
    }

    const AreaCell* WorldGrid::cell(int cell_x, int cell_y) const {
        auto it = m_cells.find(cell_slot_index(cell_x, cell_y));
        return it == m_cells.end() ? nullptr : &it->second;
    }

    AreaCell* WorldGrid::cell(int cell_x, int cell_y) {
        auto it = m_cells.find(cell_slot_index(cell_x, cell_y));
        return it == m_cells.end() ? nullptr : &it->second;
    }

    bool WorldGrid::attr_at_world_tile(int world_tx, int world_ty, std::uint32_t& out) const {
        int cx = 0;
        int cy = 0;
        int lx = 0;
        int ly = 0;
        coords::world_tile_to_cell(world_tx, world_ty, cx, cy, lx, ly);
        const AreaCell* c = cell(cx, cy);
        if (!c) {
            return false;
        }
        out = c->at(lx, ly);
        return true;
    }

    bool WorldGrid::walkable_world_tile(int world_tx, int world_ty) const {
        std::uint32_t a = 0;
        if (!attr_at_world_tile(world_tx, world_ty, a)) {
            return false;
        }
        return !has_attr(a, kAttrNotMoveable);
    }

    bool WorldGrid::walkable_world_pixel(int px, int py) const {
        return walkable_world_pixel(px, py, 0);
    }

    bool WorldGrid::walkable_world_pixel(int px, int py, std::uint32_t excluded_attributes) const {
        int tx = 0;
        int ty = 0;
        coords::pixel_to_tile(px, py, tx, ty);
        int cx = 0;
        int cy = 0;
        int local_tx = 0;
        int local_ty = 0;
        coords::world_tile_to_cell(tx, ty, cx, cy, local_tx, local_ty);
        const AreaCell* area = cell(cx, cy);
        if (area == nullptr) {
            return false;
        }
        const int sx = local_tx * AreaCell::kSubcellsPerTile +
                       (px & (kTileSize - 1)) / (kTileSize / AreaCell::kSubcellsPerTile);
        const int sy = local_ty * AreaCell::kSubcellsPerTile +
                       (py & (kTileSize - 1)) / (kTileSize / AreaCell::kSubcellsPerTile);
        const int mx =
            local_tx * kMovementCellsPerTile + (px & (kTileSize - 1)) / kMovementCellSize;
        const int my =
            local_ty * kMovementCellsPerTile + (py & (kTileSize - 1)) / kMovementCellSize;
        return !area->blocked_at_subcell(sx, sy) && !area->not_moveable_at_movement_cell(mx, my) &&
               (excluded_attributes == 0 ||
                !has_attr(area->at(local_tx, local_ty), excluded_attributes));
    }

    bool WorldGrid::not_moveable_world_pixel(int px, int py) const {
        int tx = 0;
        int ty = 0;
        coords::pixel_to_tile(px, py, tx, ty);
        int cx = 0;
        int cy = 0;
        int local_tx = 0;
        int local_ty = 0;
        coords::world_tile_to_cell(tx, ty, cx, cy, local_tx, local_ty);
        const AreaCell* area = cell(cx, cy);
        if (area == nullptr) {
            return false;
        }
        const int sx =
            local_tx * kMovementCellsPerTile + (px & (kTileSize - 1)) / kMovementCellSize;
        const int sy =
            local_ty * kMovementCellsPerTile + (py & (kTileSize - 1)) / kMovementCellSize;
        return area->not_moveable_at_movement_cell(sx, sy);
    }

    bool WorldGrid::has_attr_world_pixel(int px, int py, std::uint32_t mask) const {
        int tx = 0;
        int ty = 0;
        coords::pixel_to_tile(px, py, tx, ty);
        std::uint32_t a = 0;
        if (!attr_at_world_tile(tx, ty, a)) {
            return false;
        }
        return has_attr(a, mask);
    }

}  // namespace openkal::map
