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

#include "openkal/terrain/HeightPatch.h"

#include <vector>

namespace openkal::terrain {

    /** Provides outdoor height queries over loaded KCM patches. */
    struct HeightField {
        /** Loaded terrain patches. */
        std::vector<HeightPatch> patches;

        /** Finds a patch by grid coordinates.
         * @param patch_x Horizontal patch index.
         * @param patch_y Vertical patch index.
         * @return Patch pointer, or nullptr when absent.
         */
        [[nodiscard]] const HeightPatch* find_patch(int patch_x, int patch_y) const;

        /** Finds the loaded KCM patch that covers a world XZ, if any.
         * @param world_x World X coordinate.
         * @param world_z World Z coordinate.
         * @return Patch pointer, or nullptr when no patch covers the point.
         */
        [[nodiscard]] const HeightPatch* covering_patch(float world_x, float world_z) const;

        /** Tests whether a loaded KCM patch covers the world XZ.
         * @param world_x World X coordinate.
         * @param world_z World Z coordinate.
         * @return True when sample() would interpolate terrain instead of fallback.
         */
        [[nodiscard]] bool covers(float world_x, float world_z) const;

        /** Reads a clamped raw sample.
         * @param p Source patch.
         * @param lx Local horizontal sample index.
         * @param lz Local vertical sample index.
         * @return Sample height.
         */
        [[nodiscard]] float sample_raw(const HeightPatch& p, int lx, int lz) const;

        /** Samples terrain height at world coordinates.
         * @param world_x World X coordinate.
         * @param world_z World Z coordinate.
         * @param fallback Value returned when terrain is unavailable.
         * @return Interpolated terrain height.
         */
        [[nodiscard]] float sample(float world_x, float world_z, float fallback = 0.0f) const;
    };

}  // namespace openkal::terrain
