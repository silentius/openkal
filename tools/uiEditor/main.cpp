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
#include "openkal/platform/File.h"
#include "openkal/platform/Path.h"
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
                     "usage: %s [openkal.profile] [--profile FILE] [--data DIR] [--repo DIR] "
                     "[file.json]\n"
                     "\n"
                     "Edit openkal.ui/v1 dialog JSON.\n"
                     "  --profile FILE  openkal.profile override\n"
                     "  --data DIR   client data root (UI/ and HyperText/)\n"
                     "  --repo DIR   OpenKal repository root (default: build-time path)\n"
                     "  file.json    optional document to open\n",
                     argv0);
    }

}  // namespace

int main(int argc, char** argv) {
    std::string data_arg;
    std::string repo_arg;
    std::string profile_arg;
    std::string file_arg;
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
        } else if (a[0] == '-') {
            std::fprintf(stderr, "unknown arg: %s\n", a);
            print_usage(argv[0]);
            return 2;
        } else if (profile_arg.empty() && openkal::platform::is_profile_path(a)) {
            profile_arg = a;
        } else {
            file_arg = a;
        }
    }

    const std::string repo_root = repo_arg.empty() ? OPENKAL_REPO_ROOT : repo_arg;
    std::string profile_error;
    const auto runtime =
        openkal::platform::RuntimeProfile::load_for_tool(repo_root, profile_arg, &profile_error);
    if (!runtime) {
        std::fprintf(stderr, "error: %s\n", profile_error.c_str());
        return 1;
    }
    const std::string data_root = data_arg.empty() ? runtime->profile.data_root() : data_arg;
    if (!openkal::platform::is_directory(data_root)) {
        std::fprintf(stderr, "error: client data root is not a directory: %s\n", data_root.c_str());
        return 1;
    }

    QApplication app(argc, argv);
    QApplication::setApplicationName("ui_editor");
    QApplication::setOrganizationName("OpenKal");

    MainWindow window(repo_root, runtime->configuration.configuration_root, data_root);
    if (!file_arg.empty() && !window.open_document(file_arg)) {
        return 1;
    }
    window.show();
    return app.exec();
}
