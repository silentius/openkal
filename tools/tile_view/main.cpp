/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "TileExporter.h"
#include "TileView.h"
#include "openkal/map/Constants.h"
#include "openkal/platform/File.h"
#include "openkal/platform/Log.h"
#include "openkal/platform/Path.h"
#include "openkal/platform/RuntimeProfile.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

#ifndef OPENKAL_REPO_ROOT
#define OPENKAL_REPO_ROOT "."
#endif

    bool parse_tile(const std::string& path_or_name, int& patch_x, int& patch_y) {
        std::string base = path_or_name;
        const auto slash = base.find_last_of("/\\");
        if (slash != std::string::npos) {
            base = base.substr(slash + 1);
        }
        const auto dot = base.find_last_of('.');
        if (dot != std::string::npos) {
            base = base.substr(0, dot);
        }
        for (char& c : base) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        int x = 0;
        int y = 0;
        if (std::sscanf(base.c_str(), "n_%d_%d", &x, &y) != 2 &&
            std::sscanf(base.c_str(), "m_%d_%d", &x, &y) != 2 &&
            std::sscanf(base.c_str(), "%d_%d", &x, &y) != 2) {
            return false;
        }
        if (x < 0 || y < 0 || x >= openkal::map::kPatchGrid || y >= openkal::map::kPatchGrid) {
            return false;
        }
        patch_x = x;
        patch_y = y;
        return true;
    }

    void print_usage(const char* argv0) {
        std::fprintf(stderr,
                     "usage: %s [openkal.profile] <n_032_029 | path.kcm> [--profile FILE] "
                     "[--data DIR] "
                     "[--export DIR] "
                     "[--export-only] "
                     "[--scale N]\n"
                     "\n"
                     "Fly a single outdoor tile (KCM terrain + OPL objects).\n"
                     "  WASD / Space / Ctrl   fly (Shift = sprint)\n"
                     "  mouse                 look (click to recapture)\n"
                     "  R                     reset camera\n"
                     "  E                     export related files under DIR/data/\n"
                     "  Esc                   release mouse, then quit\n"
                     "\n"
                     "Export writes relative client paths:\n"
                     "  data/MAPS/n_XXX_YYY.{kcm,opl,dat}\n"
                     "  needed data/MAPS/Tex/*\n"
                     "  data/OBJECTS/... .gb and their textures\n",
                     argv0);
    }

}  // namespace

int main(int argc, char** argv) {
    std::string tile_arg;
    std::string data_arg;
    std::string profile_arg;
    std::string export_arg;
    float ui_scale = 1.0f;
    bool export_only = false;

    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        auto need = [&](const char* name) -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "missing value for %s\n", name);
                print_usage(argv[0]);
                std::exit(2);
            }
            return argv[++i];
        };
        if (std::strcmp(a, "-h") == 0 || std::strcmp(a, "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
        if (std::strcmp(a, "--profile") == 0) {
            profile_arg = need("--profile");
        } else if (std::strcmp(a, "--data") == 0) {
            data_arg = need("--data");
        } else if (std::strcmp(a, "--export") == 0) {
            export_arg = need("--export");
        } else if (std::strcmp(a, "--export-only") == 0) {
            export_only = true;
        } else if (std::strcmp(a, "--scale") == 0) {
            ui_scale = static_cast<float>(std::atof(need("--scale")));
            if (ui_scale < 0.5f || ui_scale > 8.0f) {
                std::fprintf(stderr, "--scale must be between 0.5 and 8\n");
                return 2;
            }
        } else if (a[0] == '-') {
            std::fprintf(stderr, "unknown arg: %s\n", a);
            print_usage(argv[0]);
            return 2;
        } else if (profile_arg.empty() && openkal::platform::is_profile_path(a)) {
            profile_arg = a;
        } else if (tile_arg.empty()) {
            tile_arg = a;
        } else {
            std::fprintf(stderr, "unexpected arg: %s\n", a);
            print_usage(argv[0]);
            return 2;
        }
    }

    if (tile_arg.empty()) {
        print_usage(argv[0]);
        return 2;
    }

    int patch_x = 0;
    int patch_y = 0;
    if (!parse_tile(tile_arg, patch_x, patch_y)) {
        std::fprintf(
            stderr, "could not parse tile name: %s (expected n_032_029)\n", tile_arg.c_str());
        return 2;
    }

    if (export_only && export_arg.empty()) {
        std::fprintf(stderr, "--export-only requires --export DIR\n");
        return 2;
    }

    const std::string repo_root = OPENKAL_REPO_ROOT;
    std::string profile_error;
    const auto runtime =
        openkal::platform::RuntimeProfile::load_for_tool(repo_root, profile_arg, &profile_error);
    if (!runtime) {
        openkal::log::error(profile_error);
        return 1;
    }
    const std::string data_root = data_arg.empty() ? runtime->profile.data_root() : data_arg;
    if (!openkal::platform::is_directory(data_root)) {
        openkal::log::error("client data root is not a directory: " + data_root);
        return 1;
    }

    if (!export_arg.empty()) {
        const openkal::tile_view::ExportResult out =
            openkal::tile_view::TileExporter::export_tile(data_root, export_arg, patch_x, patch_y);
        if (!out.ok()) {
            openkal::log::error("export failed: " + out.message());
            if (export_only) {
                return 1;
            }
        }
        if (export_only) {
            return 0;
        }
    }

    openkal::tile_view::TileView view(data_root, patch_x, patch_y, export_arg, ui_scale);
    return view.run();
}
