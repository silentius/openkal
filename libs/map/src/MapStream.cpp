/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/map/MapStream.h"

#include "openkal/map/AreaMapCodec.h"
#include "openkal/map/Coords.h"
#include "openkal/platform/File.h"
#include "openkal/platform/Path.h"

#include <array>
#include <cstdio>
#include <string>

namespace openkal::map {

    std::string MapStream::cell_path(std::string_view maps_dir, int cell_x, int cell_y) {
        std::array<char, 64> name{};
        std::snprintf(name.data(), name.size(), "n_%03d_%03d.akarea", cell_x, cell_y);
        return platform::join_path(std::string(maps_dir), name.data());
    }

    bool MapStream::cell_exists(std::string_view maps_dir, int cell_x, int cell_y) {
        return cell_x >= 0 && cell_y >= 0 &&
               platform::file_exists(cell_path(maps_dir, cell_x, cell_y));
    }

    bool
    MapStream::ensure_cell(WorldGrid& grid, std::string_view maps_dir, int cell_x, int cell_y) {
        if (cell_x < 0 || cell_y < 0) {
            return false;
        }
        if (grid.cell(cell_x, cell_y)) {
            return true;
        }
        if (!cell_exists(maps_dir, cell_x, cell_y)) {
            return false;
        }
        AreaCell cell;
        if (!AreaMapCodec::load(cell_path(maps_dir, cell_x, cell_y), cell)) {
            return false;
        }
        cell.set_cell_x(cell_x);
        cell.set_cell_y(cell_y);
        grid.set_cell(cell_x, cell_y, std::move(cell));
        return true;
    }

    int MapStream::ensure_around(WorldGrid& grid,
                                 std::string_view maps_dir,
                                 int server_px,
                                 int server_py,
                                 int radius_cells) {
        int tile_x = 0;
        int tile_y = 0;
        coords::pixel_to_tile(server_px, server_py, tile_x, tile_y);
        int cell_x = 0;
        int cell_y = 0;
        int local_tx = 0;
        int local_ty = 0;
        coords::world_tile_to_cell(tile_x, tile_y, cell_x, cell_y, local_tx, local_ty);

        int loaded = 0;
        for (int dy = -radius_cells; dy <= radius_cells; ++dy) {
            for (int dx = -radius_cells; dx <= radius_cells; ++dx) {
                if (ensure_cell(grid, maps_dir, cell_x + dx, cell_y + dy)) {
                    ++loaded;
                }
            }
        }
        return loaded;
    }

}  // namespace openkal::map
