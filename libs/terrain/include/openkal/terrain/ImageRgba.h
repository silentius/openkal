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

#include <cstdint>
#include <vector>

namespace openkal::terrain {

    /** RGBA8 image data. */
    struct ImageRgba {
        /** Image width in pixels. */
        int width = 0;

        /** Image height in pixels. */
        int height = 0;

        /** Pixel bytes in row-major RGBA order. */
        std::vector<std::uint8_t> rgba;
    };

}  // namespace openkal::terrain
