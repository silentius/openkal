/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/map/AreaMapCodec.h"
#include "openkal/map/Attributes.h"
#include "openkal/platform/File.h"
#include "openkal/platform/Path.h"
#include "openkal/platform/RuntimeProfile.h"

#include <cstdio>
#include <cstring>
#include <string>

#ifndef OPENKAL_REPO_ROOT
#define OPENKAL_REPO_ROOT "."
#endif

int main(int argc, char** argv) {
    std::string profile_path;
    std::string area_path;
    for (int index = 1; index < argc; ++index) {
        if (std::strcmp(argv[index], "-h") == 0 || std::strcmp(argv[index], "--help") == 0) {
            std::fprintf(stderr,
                         "usage: %s [openkal.profile] [--profile FILE] "
                         "<file.akarea | cell-name>\n",
                         argv[0]);
            return 0;
        }
        if (std::strcmp(argv[index], "--profile") == 0) {
            if (index + 1 >= argc) {
                std::fprintf(stderr, "missing value for --profile\n");
                return 2;
            }
            profile_path = argv[++index];
        } else if (profile_path.empty() && openkal::platform::is_profile_path(argv[index])) {
            profile_path = argv[index];
        } else if (area_path.empty()) {
            area_path = argv[index];
        } else {
            std::fprintf(stderr, "unexpected argument: %s\n", argv[index]);
            return 2;
        }
    }
    if (area_path.empty()) {
        std::fprintf(stderr,
                     "usage: %s [openkal.profile] [--profile FILE] <file.akarea | cell-name>\n",
                     argv[0]);
        return 2;
    }

    std::string profile_error;
    const auto runtime = openkal::platform::RuntimeProfile::load_for_tool(
        OPENKAL_REPO_ROOT, profile_path, &profile_error);
    if (!runtime) {
        std::fprintf(stderr, "%s\n", profile_error.c_str());
        return 1;
    }
    if (!openkal::platform::file_exists(area_path)) {
        std::string candidate =
            openkal::platform::resolve_path_case(runtime->profile.server_map_dir(), area_path);
        if (!openkal::platform::file_exists(candidate) &&
            area_path.find('.') == std::string::npos) {
            candidate = openkal::platform::resolve_path_case(runtime->profile.server_map_dir(),
                                                             area_path + ".akarea");
        }
        if (openkal::platform::file_exists(candidate)) {
            area_path = candidate;
        }
    }

    openkal::map::AreaCell grid;
    if (!openkal::map::AreaMapCodec::load(area_path, grid)) {
        return 1;
    }

    std::printf("file: %s\n", area_path.c_str());
    std::printf("cell: %d, %d\n", grid.cell_x(), grid.cell_y());
    std::printf("tiles: %dx%d\n", openkal::map::kTileCount, openkal::map::kTileCount);
    std::printf("Blocked/NotMoveable resolution: %dx%d subcells per tile\n",
                openkal::map::AreaCell::kSubcellsPerTile,
                openkal::map::AreaCell::kSubcellsPerTile);

    int hist[8] = {};
    int walkable = 0;
    for (int y = 0; y < openkal::map::kTileCount; ++y) {
        for (int x = 0; x < openkal::map::kTileCount; ++x) {
            const std::uint32_t a = grid.at(x, y);
            if (grid.walkable(x, y)) {
                ++walkable;
            }
            for (int bit = 0; bit < 7; ++bit) {
                if (openkal::map::has_attr(
                        a, openkal::map::attr_mask(static_cast<openkal::map::AttrBit>(bit)))) {
                    ++hist[bit];
                }
            }
        }
    }

    std::printf(
        "walkable: %d / %d\n", walkable, openkal::map::kTileCount * openkal::map::kTileCount);
    const char* names[] = {
        "NOTMOVEABLE", "PORTAL", "VILLAGE", "SAFETY", "ATTACK_CASTLE", "DEFENCE_CASTLE", "OBJECT"};
    for (int bit = 0; bit < 7; ++bit) {
        std::printf("  %s: %d\n", names[bit], hist[bit]);
    }

    // Sample center tile.
    const int cx = openkal::map::kTileCount / 2;
    const int cy = openkal::map::kTileCount / 2;
    std::printf("sample tile (%d,%d) attr=0x%08x walkable=%d\n",
                cx,
                cy,
                grid.at(cx, cy),
                grid.walkable(cx, cy) ? 1 : 0);
    return 0;
}
