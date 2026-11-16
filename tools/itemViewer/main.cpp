/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "ItemViewerWindow.h"
#include "model/ItemIconCache.h"
#include "model/LootPaths.h"
#include "openkal/client/ItemDb.h"
#include "openkal/platform/RuntimeProfile.h"

#include <QApplication>
#include <cstdio>
#include <cstring>
#include <string>

#ifndef OPENKAL_REPO_ROOT
#define OPENKAL_REPO_ROOT "."
#endif

namespace {

    void print_usage(const char* argv0) {
        std::fprintf(stderr,
                     "usage: %s [openkal.profile] [options]\n"
                     "\n"
                     "Browse OpenKal items and their complete definitions.\n"
                     "  --profile FILE  openkal.profile override\n"
                     "  --data DIR   client data root (contains HyperText/ item icons)\n"
                     "  --repo DIR   OpenKal repository root (default: build-time path)\n"
                     "  --items DIR  config/items override\n",
                     argv0);
    }

}  // namespace

int main(int argc, char** argv) {
    std::string data_root;
    std::string repo_root = OPENKAL_REPO_ROOT;
    std::string profile_path;
    std::string items_dir;

    for (int i = 1; i < argc; ++i) {
        const char* argument = argv[i];
        const auto need_value = [&](const char* option) -> const char* {
            if (i + 1 < argc) {
                return argv[++i];
            }
            std::fprintf(stderr, "missing value for %s\n", option);
            return nullptr;
        };
        if (std::strcmp(argument, "-h") == 0 || std::strcmp(argument, "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
        if (std::strcmp(argument, "--profile") == 0) {
            const char* value = need_value("--profile");
            if (!value) {
                return 2;
            }
            profile_path = value;
        } else if (std::strcmp(argument, "--data") == 0) {
            const char* value = need_value("--data");
            if (!value) {
                return 2;
            }
            data_root = value;
        } else if (std::strcmp(argument, "--repo") == 0) {
            const char* value = need_value("--repo");
            if (!value) {
                return 2;
            }
            repo_root = value;
        } else if (std::strcmp(argument, "--items") == 0) {
            const char* value = need_value("--items");
            if (!value) {
                return 2;
            }
            items_dir = value;
        } else if (profile_path.empty() && openkal::platform::is_profile_path(argument)) {
            profile_path = argument;
        } else {
            std::fprintf(stderr, "unknown argument: %s\n", argument);
            print_usage(argv[0]);
            return 2;
        }
    }

    std::string profile_error;
    const auto runtime =
        openkal::platform::RuntimeProfile::load_for_tool(repo_root, profile_path, &profile_error);
    if (!runtime) {
        std::fprintf(stderr, "%s\n", profile_error.c_str());
        return 1;
    }
    if (data_root.empty()) {
        data_root = runtime->profile.data_root();
    }

    openkal::client::ItemDb catalog;
    if (!catalog.load(runtime->configuration.configuration_root, data_root, items_dir)) {
        std::fprintf(stderr,
                     "failed to load the item catalog from profile %s.\n",
                     runtime->profile_path.c_str());
        return 1;
    }

    QApplication application(argc, argv);
    QApplication::setApplicationName("item_viewer");
    QApplication::setOrganizationName("OpenKal");

    ItemIconCache icons(catalog);
    ItemViewerWindow window(catalog, icons);
    window.show();
    return application.exec();
}
