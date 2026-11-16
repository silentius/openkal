/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "MainWindow.h"
#include "model/LootPaths.h"
#include "openkal/platform/Path.h"
#include "openkal/platform/RuntimeProfile.h"

#include <QApplication>
#include <clocale>
#include <cstdio>
#include <cstring>
#include <string>

#ifndef OPENKAL_REPO_ROOT
#define OPENKAL_REPO_ROOT "."
#endif

namespace {

    [[nodiscard]] LootEditorPaths default_paths(const std::string& repo_root,
                                                const std::string& configuration_root) {
        LootEditorPaths p;
        p.repo_root = repo_root;
        p.loot_config_root = configuration_root;
        return p;
    }

    void print_usage(const char* argv0) {
        std::fprintf(
            stderr,
            "usage: %s [openkal.profile] [options]\n"
            "\n"
            "Edit monster drop tables (groups, itemgroups, monsters).\n"
            "  --profile FILE      openkal.profile override\n"
            "  --data DIR         client data root (must contain HyperText/ item icons)\n"
            "  --repo DIR         OpenKal repository root (default: build-time path)\n"
            "  --config DIR       config root containing groups/, itemgroups/, and monsters/\n"
            "  --groups DIR       config/groups override\n"
            "  --itemgroups DIR   config/itemgroups override\n"
            "  --monsters DIR     config/monsters override\n"
            "\n"
            "The internal item config is detected from --repo. Loot tables default to\n"
            "openkal.profile and can be redirected together with --config.\n",
            argv0);
    }

    [[nodiscard]] bool validate_data_root(const std::string& data_root) {
        if (!openkal::platform::is_directory(data_root)) {
            std::fprintf(stderr, "error: data root is not a directory: %s\n", data_root.c_str());
            return false;
        }
        const std::string hypertext = hypertext_dir_for(data_root);
        if (hypertext.empty()) {
            std::fprintf(stderr,
                         "error: no HyperText BMP icons under %s/HyperText\n"
                         "       update data_root in openkal.profile or pass --data\n",
                         data_root.c_str());
            return false;
        }
        return true;
    }

}  // namespace

int main(int argc, char** argv) {
    std::string data_arg;
    std::string repo_arg;
    std::string profile_arg;
    std::string config_arg;
    std::string groups_arg;
    std::string itemgroups_arg;
    std::string monsters_arg;

    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        auto need = [&](const char* name) -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "missing value for %s\n", name);
                print_usage(argv[0]);
                return nullptr;
            }
            return argv[++i];
        };
        if (std::strcmp(a, "-h") == 0 || std::strcmp(a, "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
        if (std::strcmp(a, "--profile") == 0) {
            const char* value = need("--profile");
            if (value == nullptr) {
                return 2;
            }
            profile_arg = value;
        } else if (std::strcmp(a, "--data") == 0) {
            const char* value = need("--data");
            if (value == nullptr) {
                return 2;
            }
            data_arg = value;
        } else if (std::strcmp(a, "--repo") == 0) {
            const char* value = need("--repo");
            if (value == nullptr) {
                return 2;
            }
            repo_arg = value;
        } else if (std::strcmp(a, "--config") == 0) {
            const char* value = need("--config");
            if (value == nullptr) {
                return 2;
            }
            config_arg = value;
        } else if (std::strcmp(a, "--groups") == 0) {
            const char* value = need("--groups");
            if (value == nullptr) {
                return 2;
            }
            groups_arg = value;
        } else if (std::strcmp(a, "--itemgroups") == 0) {
            const char* value = need("--itemgroups");
            if (value == nullptr) {
                return 2;
            }
            itemgroups_arg = value;
        } else if (std::strcmp(a, "--monsters") == 0) {
            const char* value = need("--monsters");
            if (value == nullptr) {
                return 2;
            }
            monsters_arg = value;
        } else if (profile_arg.empty() && openkal::platform::is_profile_path(a)) {
            profile_arg = a;
        } else {
            std::fprintf(stderr, "unknown arg: %s\n", a);
            print_usage(argv[0]);
            return 2;
        }
    }

    const std::string repo_root = repo_arg.empty() ? OPENKAL_REPO_ROOT : repo_arg;
    std::string profile_error;
    const auto runtime = openkal::platform::RuntimeProfile::load_for_tool(
        repo_root, profile_arg, &profile_error, config_arg);
    if (!runtime) {
        std::fprintf(stderr, "error: %s\n", profile_error.c_str());
        return 1;
    }
    const std::string data_root = data_arg.empty() ? runtime->profile.data_root() : data_arg;

    if (!validate_data_root(data_root)) {
        return 1;
    }

    LootEditorPaths paths = default_paths(repo_root, runtime->configuration.configuration_root);
    if (!config_arg.empty()) {
        paths.loot_config_root = config_arg;
        paths.loot_config_root_locked = true;
    }
    if (!groups_arg.empty()) {
        paths.groups_dir = groups_arg;
    }
    if (!itemgroups_arg.empty()) {
        paths.itemgroups_dir = itemgroups_arg;
    }
    if (!monsters_arg.empty()) {
        paths.monsters_dir = monsters_arg;
    }
    paths.data_root = data_root;
    paths.data_root_locked = true;

    QApplication app(argc, argv);
    // Qt sets LC_NUMERIC from the UI locale; keep C printf/JSON decimals as '.'.
    std::setlocale(LC_NUMERIC, "C");
    QApplication::setApplicationName("loot_editor");
    QApplication::setOrganizationName("OpenKal");

    MainWindow window(paths);
    window.show();
    return app.exec();
}
