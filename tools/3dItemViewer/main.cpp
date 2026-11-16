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
        const auto lower = [](char value) {
            return static_cast<char>(std::tolower(static_cast<unsigned char>(value)));
        };
        return lower(path[path.size() - 3]) == '.' && lower(path[path.size() - 2]) == 'g' &&
               lower(path[path.size() - 1]) == 'b';
    }

    void collect_gb_from_dir(const std::string& directory, std::vector<std::string>& out) {
        for (const auto& relative : openkal::platform::list_files_recursive(directory)) {
            if (ends_with_gb(relative)) {
                out.push_back(openkal::platform::join_path(directory, relative));
            }
        }
    }

    [[nodiscard]] bool resolve_model_arg(const std::string& data_root,
                                         const std::string& items_dir,
                                         const std::string& argument,
                                         std::vector<std::string>& out) {
        if (openkal::platform::is_directory(argument)) {
            collect_gb_from_dir(argument, out);
            return true;
        }
        if (openkal::platform::file_exists(argument)) {
            out.push_back(argument);
            return ends_with_gb(argument);
        }

        const std::string stripped = openkal::platform::strip_client_data_prefix(argument);
        const std::string candidates[] = {
            openkal::platform::resolve_path_case(items_dir, argument),
            openkal::platform::resolve_path_case(items_dir, stripped),
            openkal::platform::resolve_path_case(data_root, stripped),
        };
        for (const auto& path : candidates) {
            if (openkal::platform::is_directory(path)) {
                const std::size_t before = out.size();
                collect_gb_from_dir(path, out);
                return out.size() > before;
            }
            if (openkal::platform::file_exists(path) && ends_with_gb(path)) {
                out.push_back(path);
                return true;
            }
            if (!ends_with_gb(path)) {
                const std::string with_extension = path + ".gb";
                if (openkal::platform::file_exists(with_extension)) {
                    out.push_back(with_extension);
                    return true;
                }
            }
        }
        return false;
    }

    void print_usage(const char* argv0) {
        std::fprintf(stderr,
                     "usage: %s [openkal.profile] [--profile FILE] [--data DIR] [--scale N] "
                     "[file.gb | folder ...]\n"
                     "\n"
                     "Browse equipment and weapon models under data/Items (or data/Item).\n"
                     "Pass .gb files or folders to restrict the model list.\n"
                     "  click / Up Down   select model\n"
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
    std::vector<std::string> model_args;

    for (int i = 1; i < argc; ++i) {
        const char* argument = argv[i];
        const auto need_value = [&](const char* name) -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "missing value for %s\n", name);
                print_usage(argv[0]);
                std::exit(2);
            }
            return argv[++i];
        };
        if (std::strcmp(argument, "-h") == 0 || std::strcmp(argument, "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
        if (std::strcmp(argument, "--profile") == 0) {
            profile_arg = need_value("--profile");
        } else if (std::strcmp(argument, "--data") == 0) {
            data_arg = need_value("--data");
        } else if (std::strcmp(argument, "--scale") == 0) {
            ui_scale = static_cast<float>(std::atof(need_value("--scale")));
            if (ui_scale < 0.5f || ui_scale > 8.0f) {
                std::fprintf(stderr, "--scale must be between 0.5 and 8\n");
                return 2;
            }
        } else if (argument[0] == '-') {
            std::fprintf(stderr, "unknown argument: %s\n", argument);
            print_usage(argv[0]);
            return 2;
        } else if (profile_arg.empty() && openkal::platform::is_profile_path(argument)) {
            profile_arg = argument;
        } else {
            model_args.emplace_back(argument);
        }
    }

    std::string profile_error;
    const auto runtime = openkal::platform::RuntimeProfile::load_for_tool(
        OPENKAL_REPO_ROOT, profile_arg, &profile_error);
    if (!runtime) {
        openkal::log::error(profile_error);
        return 1;
    }
    const std::string data_root = data_arg.empty() ? runtime->profile.data_root() : data_arg;
    if (!openkal::platform::is_directory(data_root)) {
        openkal::log::error("client data root is not a directory: " + data_root);
        return 1;
    }

    std::string items_dir = openkal::platform::resolve_path_case(data_root, "Items");
    if (!openkal::platform::is_directory(items_dir)) {
        items_dir = openkal::platform::resolve_path_case(data_root, "Item");
    }
    if (!openkal::platform::is_directory(items_dir)) {
        openkal::log::error("no Items or Item directory under " + data_root);
        return 1;
    }

    std::vector<std::string> model_paths;
    for (const auto& argument : model_args) {
        if (!resolve_model_arg(data_root, items_dir, argument, model_paths)) {
            openkal::log::error("3dItemViewer: no .gb models for " + argument);
            return 2;
        }
    }

    openkal::object_view::ObjectView view(
        data_root, items_dir, "ITEMS", "3dItemViewer", ui_scale, std::move(model_paths));
    return view.run();
}
