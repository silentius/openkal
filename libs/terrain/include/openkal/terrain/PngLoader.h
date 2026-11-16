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

#include "openkal/terrain/ImageRgba.h"

#include <string>

namespace openkal::terrain {

    /** Loads a PNG as top-to-bottom RGBA8 pixels, preserving transparency.
     * @param path PNG image path.
     * @param out Destination image, cleared on failure.
     * @return True if the PNG decoded within the texture size limit.
     */
    [[nodiscard]] bool load_png_rgba(const std::string& path, ImageRgba& out);

}  // namespace openkal::terrain
