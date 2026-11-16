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

#include "openkal/terrain/OplFile.h"

#include <string>

namespace openkal::terrain {

    /** Loads OPL object-placement files. */
    class OplLoader final {
      public:
        /** Loads an OPL file.
         * @param path OPL file path.
         * @param out Destination object-placement data.
         * @return True when the file was loaded.
         */
        [[nodiscard]] static bool load(const std::string& path, OplFile& out);
    };

    /** Loads an OPL file.
     * @param path OPL file path.
     * @param out Destination object-placement data.
     * @return True when the file was loaded.
     */
    [[nodiscard]] bool load_opl(const std::string& path, OplFile& out);

    /** Converts an OPL translation to client world coordinates.
     * @param inst Source object instance.
     * @param patch_x Horizontal patch index.
     * @param patch_y Vertical patch index.
     * @param wx Destination world X.
     * @param wy Destination world Y.
     * @param wz Destination world Z.
     */
    void opl_world_translation(
        const OplInstance& inst, int patch_x, int patch_y, float& wx, float& wy, float& wz);

}  // namespace openkal::terrain
