/*
 * OpenKal - an open source KalOnline client and server
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "AreaEditorWindow.h"
#include "openkal/engine/engine.h"
#include "openkal/platform/RuntimeProfile.h"

#include <GLFW/glfw3.h>
#include <QApplication>
#include <QMessageBox>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <utility>

#ifndef OPENKAL_REPO_ROOT
#define OPENKAL_REPO_ROOT "."
#endif

namespace {

    void print_usage(const char* executable) {
        std::fprintf(stderr,
                     "Usage: %s [openkal.profile] [n_XXX_YYY.akarea] [options]\n"
                     "\n"
                     "Qt area-attribute editor with searchable profile area list.\n"
                     "  --profile FILE  openkal.profile override\n"
                     "  --data DIR      client data root override\n"
                     "  --out FILE      save target for the initially opened area\n"
                     "  --res N         top-down bake resolution (default 2048)\n",
                     executable);
    }

    [[nodiscard]] bool parse_arguments(int argc, char** argv, AreaEditorOptions& options) {
        options.repo_root = OPENKAL_REPO_ROOT;
        for (int index = 1; index < argc; ++index) {
            const char* argument = argv[index];
            const auto value_after = [&](const char* option) -> const char* {
                if (index + 1 < argc) {
                    return argv[++index];
                }
                std::fprintf(stderr, "missing value for %s\n", option);
                return nullptr;
            };
            if (std::strcmp(argument, "-h") == 0 || std::strcmp(argument, "--help") == 0) {
                print_usage(argv[0]);
                return false;
            }
            if (std::strcmp(argument, "--profile") == 0) {
                const char* value = value_after("--profile");
                if (!value) {
                    return false;
                }
                options.profile_path = value;
            } else if (std::strcmp(argument, "--data") == 0) {
                const char* value = value_after("--data");
                if (!value) {
                    return false;
                }
                options.data_root_override = value;
            } else if (std::strcmp(argument, "--out") == 0) {
                const char* value = value_after("--out");
                if (!value) {
                    return false;
                }
                options.initial_output_path = value;
            } else if (std::strcmp(argument, "--res") == 0) {
                const char* value = value_after("--res");
                if (!value) {
                    return false;
                }
                options.bake_resolution = std::atoi(value);
                if (options.bake_resolution <= 0) {
                    std::fprintf(stderr, "invalid bake resolution: %s\n", value);
                    return false;
                }
            } else if (options.profile_path.empty() &&
                       openkal::platform::is_profile_path(argument)) {
                options.profile_path = argument;
            } else if (options.initial_area_path.empty() && argument[0] != '-') {
                options.initial_area_path = argument;
            } else {
                std::fprintf(stderr, "unknown argument: %s\n", argument);
                print_usage(argv[0]);
                return false;
            }
        }
        return true;
    }

}  // namespace

int main(int argc, char** argv) {
    AreaEditorOptions options;
    if (!parse_arguments(argc, argv, options)) {
        for (int index = 1; index < argc; ++index) {
            if (std::strcmp(argv[index], "-h") == 0 || std::strcmp(argv[index], "--help") == 0) {
                return 0;
            }
        }
        return 2;
    }

    QApplication application(argc, argv);
    QApplication::setApplicationName("area_editor");
    QApplication::setApplicationDisplayName("OpenKal Area Editor");
    QApplication::setOrganizationName("OpenKal");

    if (!glfwInit()) {
        QMessageBox::critical(
            nullptr, "OpenGL initialization failed", "Could not initialize GLFW.");
        return 1;
    }
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    std::unique_ptr<openkal::engine::IEngine> engine(openkal::engine::create_engine());
    if (!engine || !engine->init("OpenKal Area Editor Bake Backend", 64, 64)) {
        QMessageBox::critical(nullptr,
                              "OpenGL initialization failed",
                              "Could not initialize the hidden area-bake backend.");
        glfwTerminate();
        return 1;
    }
    if (GLFWwindow* window = glfwGetCurrentContext()) {
        glfwHideWindow(window);
    }

    int result = 0;
    {
        AreaEditorWindow window(*engine, std::move(options));
        window.show();
        result = application.exec();
    }
    engine->shutdown();
    return result;
}
