/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/MinimapWidget.h"
#include "openkal/map/Coords.h"

#include <array>

namespace openkal::client {
    namespace {

        struct AreaBounds {
            std::string_view name;
            int left;
            int top;
            int right;
            int bottom;
            bool village;
        };

        // World-tile bounds identify settlements; the Village attribute supplies
        // their actual boundary. Safety alone also covers the surrounding fields.
        // Display names follow https://gameagit.com/kalonline/Map.
        constexpr std::array<AreaBounds, 11> kAreas = {{
            {"Village of Narootuh", 7980, 8000, 8100, 8140, true},
            {"Jook-Suh Cargo Station", 8230, 8120, 8340, 8240, true},
            {"Geum-Oh Mine", 8240, 8800, 8380, 8970, true},
            {"The Pub of the Giant Bird", 7960, 8980, 8060, 9070, true},
            {"Temporary Fort", 8320, 7500, 8440, 7660, true},
            {"The City of Priest", 7190, 9060, 7380, 9260, true},
            {"The Royal Tomb", 7480, 8950, 7900, 9400, false},
            {"Tower of Priest", 7240, 8600, 7500, 8900, false},
            {"Doggebi Tower", 8670, 7650, 8920, 7900, false},
            {"Forest of Statues", 8300, 6900, 9100, 7700, false},
            {"Traitor's Mountain Hangout", 6600, 9000, 7460, 9950, false},
        }};

    }  // namespace

    std::string_view MinimapWidget::area_name(float world_x,
                                              float world_z,
                                              int origin_patch,
                                              const map::WorldGrid& areas) {
        const int pixel_x = coords::to_server(world_x, origin_patch);
        const int pixel_y = coords::to_server(world_z, origin_patch);
        const bool village = areas.has_attr_world_pixel(pixel_x, pixel_y, map::kAttrVillage);
        int tile_x = 0;
        int tile_y = 0;
        coords::pixel_to_tile(pixel_x, pixel_y, tile_x, tile_y);
        for (const AreaBounds& area : kAreas) {
            if (tile_x >= area.left && tile_x <= area.right && tile_y >= area.top &&
                tile_y <= area.bottom && (!area.village || village)) {
                return area.name;
            }
        }
        return "Naraeha";
    }

    std::string_view MinimapWidget::dungeon_area_name(int map_index) {
        // Map IDs are not dungeon numbers: D1 uses map 2 and D2 uses map 1.
        // Include the alternate floor IDs present in config/mapinfo.json.
        switch (map_index) {
        case 2:
            return "D1";
        case 1:
            return "D2";
        case 3:
            return "D3";
        case 4:
            return "D4";
        case 8:
        case 11:
        case 12:
        case 13:
        case 15:
        case 17:
        case 18:
        case 21:
        case 22:
        case 23:
        case 24:
            return "D5";
        case 25:
        case 32:
        case 34:
        case 35:
        case 36:
        case 37:
        case 38:
        case 39:
        case 42:
        case 43:
            return "D6";
        default:
            return "Dungeon";
        }
    }

}  // namespace openkal::client
