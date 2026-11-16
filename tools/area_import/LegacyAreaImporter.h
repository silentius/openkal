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

#include "openkal/map/AreaCell.h"

#include <cstddef>
#include <string_view>

namespace openkal::area_import {

    /** Reads legacy area source data into an editable OpenKal area cell.
     * @param path Path to the legacy source file.
     * @param out Receives the imported area cell on success.
     * @return True when the legacy source was recognised and imported.
     */
    [[nodiscard]] bool import_legacy_area(std::string_view path, map::AreaCell& out);

    /** Number of occupied subcells removed by conservative contour smoothing. */
    struct SmoothStats {
        std::size_t blocked_removed = 0;
        std::size_t not_moveable_removed = 0;
    };

    /** Smooths imported Blocked and NotMoveable contours without adding cells.
     *
     * One pass removes supported protrusions and convex corners from components
     * of at least nine subcells. Small objects, thin lines, and map-cell borders
     * are preserved.
     * @param area Imported area cell to update.
     * @return Counts of removed Blocked and NotMoveable subcells.
     */
    [[nodiscard]] SmoothStats smooth_imported_area(map::AreaCell& area);

}  // namespace openkal::area_import
