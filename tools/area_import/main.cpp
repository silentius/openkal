/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "LegacyAreaImporter.h"
#include "openkal/map/AreaMapCodec.h"

#include <cstdio>
#include <cstring>
#include <string>

namespace {

    [[nodiscard]] bool parse_cell_coordinates(const std::string& path, int& cell_x, int& cell_y) {
        const std::size_t slash = path.find_last_of("/\\");
        const char* filename = path.c_str() + (slash == std::string::npos ? 0 : slash + 1);
        return std::sscanf(filename, "n_%d_%d.ksm", &cell_x, &cell_y) == 2;
    }

    void print_usage(const char* argv0) {
        std::fprintf(stderr,
                     "Usage: %s <legacy.ksm> <output.akarea> [--smooth] [--cell X Y]\n"
                     "\n"
                     "Imports a legacy KSM cell into OpenKal's .akarea format.\n"
                     "--smooth conservatively removes protrusions and convex corners.\n"
                     "Without --cell, coordinates are read from n_XXX_YYY.ksm.\n",
                     argv0);
    }

}  // namespace

int main(int argc, char** argv) {
    if (argc >= 2 && (std::strcmp(argv[1], "--help") == 0 || std::strcmp(argv[1], "-h") == 0)) {
        print_usage(argv[0]);
        return 0;
    }
    if (argc < 3) {
        print_usage(argv[0]);
        return 2;
    }

    const std::string input = argv[1];
    const std::string output = argv[2];
    int cell_x = 0;
    int cell_y = 0;
    bool have_coordinates = parse_cell_coordinates(input, cell_x, cell_y);
    bool smooth = false;
    for (int index = 3; index < argc; ++index) {
        if (std::strcmp(argv[index], "--smooth") == 0) {
            smooth = true;
            continue;
        }
        if (std::strcmp(argv[index], "--cell") == 0 && index + 2 < argc) {
            if (std::sscanf(argv[index + 1], "%d", &cell_x) != 1 ||
                std::sscanf(argv[index + 2], "%d", &cell_y) != 1) {
                std::fprintf(stderr, "invalid --cell coordinates\n");
                return 2;
            }
            have_coordinates = true;
            index += 2;
            continue;
        }
        std::fprintf(stderr, "unknown or incomplete option: %s\n", argv[index]);
        print_usage(argv[0]);
        return 2;
    }
    if (!have_coordinates) {
        std::fprintf(stderr, "cannot infer cell coordinates; pass --cell X Y\n");
        return 2;
    }

    openkal::map::AreaCell cell;
    if (!openkal::area_import::import_legacy_area(input, cell)) {
        return 1;
    }
    openkal::area_import::SmoothStats smooth_stats;
    if (smooth) {
        smooth_stats = openkal::area_import::smooth_imported_area(cell);
    }
    cell.set_cell_x(cell_x);
    cell.set_cell_y(cell_y);
    if (!openkal::map::AreaMapCodec::save(output, cell)) {
        return 1;
    }
    std::printf("%s -> %s (%d, %d)\n", input.c_str(), output.c_str(), cell_x, cell_y);
    if (smooth) {
        std::printf("smoothed: removed %zu Blocked and %zu NotMoveable subcells\n",
                    smooth_stats.blocked_removed,
                    smooth_stats.not_moveable_removed);
    }
    return 0;
}
