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

#include <string_view>

namespace openkal::map {

    /** Reads and writes the OpenKal `.akarea` area-cell format.
     *
     * Version 2 stores tile flags and independent RLE-compressed four by four
     * Blocked and NotMoveable masks for every tile.
     */
    class AreaMapCodec final {
      public:
        /** Disables construction of this static utility class. */
        AreaMapCodec() = delete;

        /** Loads one `.akarea` cell.
         * @param path File to read.
         * @param out Destination area cell.
         * @return True on success.
         */
        [[nodiscard]] static bool load(std::string_view path, AreaCell& out);

        /** Writes one `.akarea` cell deterministically.
         * @param path File to write.
         * @param cell Source area cell.
         * @return True on success.
         */
        [[nodiscard]] static bool save(std::string_view path, const AreaCell& cell);
    };

}  // namespace openkal::map
