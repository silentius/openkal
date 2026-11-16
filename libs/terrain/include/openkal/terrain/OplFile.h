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

#include "openkal/map/Constants.h"

#include <cstdint>
#include <string>
#include <vector>

namespace openkal::terrain {

    /** Placed object from an OPL file. */
    struct OplInstance {
        /** Model path. */
        std::string name;

        /** Patch-normalized X position. */
        float tx = 0;

        /** Patch-normalized Y position mapped to world Z. */
        float ty = 0;

        /** World height. */
        float tz = 0;

        /** Rotation quaternion X component. */
        float qx = 0;

        /** Rotation quaternion Y component. */
        float qy = 0;

        /** Rotation quaternion Z component. */
        float qz = 0;

        /** Rotation quaternion W component. */
        float qw = 1;

        /** X scale. */
        float sx = 1;

        /** Y scale. */
        float sy = 1;

        /** Z scale. */
        float sz = 1;
    };

    /** Parsed OPL object-placement file. */
    struct OplFile {
        /** Horizontal patch index. */
        int patch_x = 0;

        /** Vertical patch index. */
        int patch_y = 0;

        /** File format version. */
        std::uint32_t version = 0;

        /** File checksum. */
        std::uint32_t checksum = 0;

        /** Placed object instances. */
        std::vector<OplInstance> instances;
    };

    /** World size of one OPL patch. */
    inline constexpr float kOplPatchScale =
        static_cast<float>(map::kMapSize * map::kTerrainCellSize);

    /** World size of the complete OPL grid. */
    inline constexpr float kOplFullScale =
        static_cast<float>(map::kMapSize * map::kTerrainCellSize * map::kPatchGrid);

}  // namespace openkal::terrain
