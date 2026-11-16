/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include "openkal/map/WorldGrid.h"

#include <string>
#include <string_view>

namespace openkal::map {

    /** Streams `.akarea` cells into a WorldGrid from a map directory. */
    class MapStream {
      public:
        /** Non-instantiable utility class. */
        MapStream() = delete;

        /** Builds maps_dir/n_XXX_YYY.akarea for a cell slot.
         * @param maps_dir Directory containing .akarea files.
         * @param cell_x Cell X.
         * @param cell_y Cell Y.
         * @return Full path to the .akarea file.
         */
        [[nodiscard]] static std::string
        cell_path(std::string_view maps_dir, int cell_x, int cell_y);

        /** Returns whether a server navigation cell exists on disk.
         * @param maps_dir Directory containing .akarea files.
         * @param cell_x Cell X.
         * @param cell_y Cell Y.
         * @return True when the cell coordinates are valid and its .akarea exists.
         */
        [[nodiscard]] static bool cell_exists(std::string_view maps_dir, int cell_x, int cell_y);

        /** Ensures the cell is present in grid, loading from disk if needed.
         * @param grid World grid to update.
         * @param maps_dir Directory containing .akarea files.
         * @param cell_x Cell X.
         * @param cell_y Cell Y.
         * @return True if the cell is already loaded or newly loaded; false when the
         * cell is unavailable or invalid. A missing .akarea is an unavailable, blocked
         * navigation cell and is not passed to AreaMapCodec.
         */
        [[nodiscard]] static bool
        ensure_cell(WorldGrid& grid, std::string_view maps_dir, int cell_x, int cell_y);

        /** Ensures cells in a Chebyshev radius around a server pixel position.
         * @param grid World grid to update.
         * @param maps_dir Directory containing .akarea files.
         * @param server_px Server pixel X.
         * @param server_py Server pixel Y.
         * @param radius_cells Cell radius to stream.
         * @return Number of cells successfully ensured.
         */
        [[nodiscard]] static int ensure_around(WorldGrid& grid,
                                               std::string_view maps_dir,
                                               int server_px,
                                               int server_py,
                                               int radius_cells);
    };

}  // namespace openkal::map
