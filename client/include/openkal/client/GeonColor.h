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

namespace openkal::client {
    /** Matching universal-UI and inventory-tooltip colors for a Geon amount. */
    struct GeonColor {
        /** Universal UI RGB color. */
        const char* hex;
        /** Inventory tooltip markup, including the color introducer. */
        const char* markup;
    };

    /** Selects the currency color using strict upper bounds.
     * @param amount Geon amount, including 64-bit stack totals.
     * @return White, green, yellow, orange, blue or purple presentation. */
    [[nodiscard]] constexpr GeonColor geon_color(std::int64_t amount) {
        return amount < 100000       ? GeonColor{"#FFFFFF", "#8"}
               : amount < 1000000    ? GeonColor{"#00FF00", "#4"}
               : amount < 10000000   ? GeonColor{"#FFFF00", "#3"}
               : amount < 100000000  ? GeonColor{"#FF8040", "#2"}
               : amount < 1000000000 ? GeonColor{"#0080FF", "#6"}
                                     : GeonColor{"#AA55FF", "#9"};
    }
}  // namespace openkal::client
