/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "ObjectView.h"
#include "openkal/platform/File.h"
#include "openkal/platform/Log.h"
#include "openkal/platform/Path.h"
#include "openkal/platform/RuntimeProfile.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace {

#ifndef OPENKAL_REPO_ROOT
#define OPENKAL_REPO_ROOT "."
#endif

    [[nodiscard]] bool ends_with_gb(std::string_view path) {
        if (path.size() < 3) {
            return false;
        }
        const auto lower = [](char c) {
            return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        };
        return lower(path[path.size() - 3]) == '.' && lower(path[path.size() - 2]) == 'g' &&
               lower(path[path.size() - 1]) == 'b';
    }

    void collect_gb_from_dir(const std::string& dir, std::vector<std::string>& out) {
        const auto files = openkal::platform::list_files_recursive(dir);
        for (const auto& rel : files) {
            if (ends_with_gb(rel)) {
                out.push_back(openkal::platform::join_path(dir, rel));
            }
        }
    }

    [[nodiscard]] std::string resolve_gb_file(const std::string& data_root,
                                              const std::string& objects_dir,
                                              const std::string& arg) {
        if (openkal::platform::file_exists(arg)) {
            return arg;
        }
        if (!ends_with_gb(arg)) {
            const std::string with_ext = arg + ".gb";
            if (openkal::platform::file_exists(with_ext)) {
                return with_ext;
            }
        }
        const std::string asset = openkal::platform::resolve_client_asset(data_root, arg, ".gb");
        if (!asset.empty() && openkal::platform::file_exists(asset)) {
            return asset;
        }

        const std::string stripped = openkal::platform::strip_client_data_prefix(arg);
        const std::string candidates[] = {
            openkal::platform::resolve_path_case(objects_dir, arg),
            openkal::platform::resolve_path_case(objects_dir, stripped),
            openkal::platform::resolve_path_case(data_root, stripped),
            ends_with_gb(arg) ? std::string{}
                              : openkal::platform::resolve_path_case(objects_dir, arg + ".gb"),
            ends_with_gb(stripped)
                ? std::string{}
                : openkal::platform::resolve_path_case(objects_dir, stripped + ".gb"),
        };
        for (const auto& path : candidates) {
            if (!path.empty() && openkal::platform::file_exists(path)) {
                return path;
            }
        }
        return {};
    }

    [[nodiscard]] bool resolve_gb_arg(const std::string& data_root,
                                      const std::string& objects_dir,
                                      const std::string& arg,
                                      std::vector<std::string>& out) {
        if (openkal::platform::is_directory(arg)) {
            const std::size_t before = out.size();
            collect_gb_from_dir(arg, out);
            return out.size() > before;
        }

        const std::string stripped = openkal::platform::strip_client_data_prefix(arg);
        const std::string dir_candidates[] = {
            openkal::platform::resolve_path_case(objects_dir, arg),
            openkal::platform::resolve_path_case(objects_dir, stripped),
            openkal::platform::resolve_path_case(data_root, stripped),
        };
        for (const auto& dir : dir_candidates) {
            if (!dir.empty() && openkal::platform::is_directory(dir)) {
                const std::size_t before = out.size();
                collect_gb_from_dir(dir, out);
                return out.size() > before;
            }
        }

        const std::string file = resolve_gb_file(data_root, objects_dir, arg);
        if (file.empty()) {
            return false;
        }
        out.push_back(file);
        return true;
    }

    void print_usage(const char* argv0) {
        std::fprintf(stderr,
                     "usage: %s [openkal.profile] [--profile FILE] [--data DIR] [--scale N] "
                     "[file.gb | folder ...]\n"
                     "\n"
                     "Browse data/OBJECTS .gb structures. Left: list. Right: "
                     "textured preview.\n"
                     "Pass .gb files or folders to open only those models "
                     "(paths may be absolute, relative to OBJECTS, or OPL-style).\n"
                     "  click / Up Down   select object\n"
                     "  type / Backspace  filter by path\n"
                     "  LMB drag          orbit\n"
                     "  RMB / MMB drag    pan\n"
                     "  wheel             zoom (preview) or scroll (list)\n"
                     "  R                 reset camera\n"
                     "  Esc               clear filter, or quit\n"
                     "  --scale N         UI zoom (default 2, comfortable on 4K)\n",
                     argv0);
    }

}  // namespace

int main(int argc, char** argv) {
    std::string data_arg;
    std::string profile_arg;
    float ui_scale = 2.0f;
    std::vector<std::string> gb_args;

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
        } else {
            gb_args.emplace_back(a);
        }
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

    std::vector<std::string> gb_paths;
    const std::string objects_dir = openkal::platform::resolve_path_case(data_root, "OBJECTS");
    for (const auto& arg : gb_args) {
        if (!resolve_gb_arg(data_root, objects_dir, arg, gb_paths)) {
            openkal::log::error("object_view: no .gb files for " + arg);
            return 2;
        }
    }

    openkal::object_view::ObjectView view(
        data_root, objects_dir, "OBJECTS", "object_view", ui_scale, std::move(gb_paths));
    return view.run();
}
