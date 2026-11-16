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

#include "openkal/terrain/Floor.h"

#include <string>

namespace openkal::terrain {

    /** CPU-only dungeon geometry shared by authoritative position checks and tests. */
    class IndoorCollision {
      public:
        /** Loads every region and obstacle; incomplete geometry fails closed.
         * @param root Runtime map asset directory.
         * @param path Resolved indoor map file.
         * @param cell_x Even-aligned server cell X.
         * @param cell_y Even-aligned server cell Y.
         * @return True when all required geometry loaded.
         */
        [[nodiscard]] bool
        load(const std::string& root, const std::string& path, int cell_x, int cell_y);

        /** Resolves a standing surface near or below a trusted height, never a roof above it.
         * @param floor Collision geometry in the same coordinate space as the position.
         * @param position Feet position (Y is vertical).
         * @param height Receives the supporting surface height.
         * @return True when a floor and standing headroom exist.
         */
        [[nodiscard]] static bool
        resolve_standing(const FloorField& floor, engine::Vec3 position, float& height);

        /** Tests a grounded position against floor and headroom.
         * @param floor Collision geometry.
         * @param position Feet position (Y is vertical).
         * @return True when feet are on a valid floor within protocol rounding tolerance.
         */
        [[nodiscard]] static bool standing(const FloorField& floor, engine::Vec3 position);

        /** Checks the complete movement segment, including supported falling motion.
         * @param floor Collision geometry.
         * @param from Previously accepted feet position.
         * @param to Requested feet position.
         * @return True for a supported, unobstructed move without upward flight.
         */
        [[nodiscard]] static bool
        can_move(const FloorField& floor, engine::Vec3 from, engine::Vec3 to);

        /** Returns the loaded server-coordinate collision field.
         * @return Collision field, or nullptr after an incomplete load.
         */
        [[nodiscard]] const FloorField* floor() const;

      private:
        FloorField m_floor;
        bool m_loaded = false;
    };

}  // namespace openkal::terrain
