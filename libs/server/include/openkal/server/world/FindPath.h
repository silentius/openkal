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

#include <functional>
#include <utility>
#include <vector>

namespace openkal::server {

    /** One bounded grid-search result using 32-pixel steps. */
    struct FindPathResult {
        /** True when at least one step toward the goal was found. */
        bool found = false;

        /** Waypoints from the first step after start through the last node. */
        std::vector<std::pair<int, int>> waypoints;
    };

    /**
     * Tests whether a 32-pixel (or shorter) step is free of terrain and occupants.
     * @param from_x Start pixel X.
     * @param from_y Start pixel Y.
     * @param dx Pixel delta X.
     * @param dy Pixel delta Y.
     * @return True when the complete step is traversable.
     */
    using IsMoveTileFn = std::function<bool(int from_x, int from_y, int dx, int dy)>;

    /**
     * Finds a bounded path using eight-directional 32-pixel grid steps.
     * @param start_x Start pixel X.
     * @param start_y Start pixel Y.
     * @param end_x Goal pixel X.
     * @param end_y Goal pixel Y.
     * @param range_sq Attack-range squared (runtime state); also accepts 48px.
     * @param is_move_tile Occupancy + terrain probe along each candidate step.
     * @return Path waypoints when a detour exists.
     */
    [[nodiscard]] FindPathResult find_monster_path(int start_x,
                                                   int start_y,
                                                   int end_x,
                                                   int end_y,
                                                   int range_sq,
                                                   const IsMoveTileFn& is_move_tile);

}  // namespace openkal::server
