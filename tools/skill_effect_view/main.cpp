/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "SkillEffectView.h"
#include "openkal/platform/Log.h"
#include "openkal/platform/Path.h"
#include "openkal/platform/RuntimeProfile.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

#ifndef OPENKAL_REPO_ROOT
#define OPENKAL_REPO_ROOT "."
#endif

    void print_usage(const char* argv0) {
        std::fprintf(stderr,
                     "usage: %s [openkal.profile] [--profile FILE] [--data DIR] [--skills DIR] "
                     "[--monsters DIR] [--npcs DIR] [--scale N]\n"
                     "\n"
                     "Browse skill .gb models and Data/Effect sprite / FX shaders.\n"
                     "  Skills tab   config/skills JSON `effect` stems (prep/fire/hit)\n"
                     "  Models tab   every .gb under data/Skill and data/Effect\n"
                     "  Sprite tab   Effect/Sprite particle shaders (same loader as the client)\n"
                     "  Fx tab       Effect/Fx shaders (particles + optional model stages)\n"
                     "  Mobs tab     monster/NPC JSON `particles` / `fx` attaches\n"
                     "  click / Up Down   select row\n"
                     "  Left Right        gubun slot (prep / fire / hit) on Skills\n"
                     "  type / Backspace  filter\n"
                     "  LMB drag          orbit\n"
                     "  RMB / MMB drag    pan\n"
                     "  wheel             zoom (preview) or scroll (list)\n"
                     "  Space             replay clip / rewind particles\n"
                     "  R                 reset camera\n"
                     "  Esc               clear filter, or quit\n"
                     "  --scale N         UI zoom (default 2, comfortable on 4K)\n",
                     argv0);
    }

}  // namespace

int main(int argc, char** argv) {
    std::string data_arg;
    std::string profile_arg;
    std::string skills_arg;
    std::string monsters_arg;
    std::string npcs_arg;
    float ui_scale = 2.0f;

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
        } else if (std::strcmp(a, "--skills") == 0) {
            skills_arg = need("--skills");
        } else if (std::strcmp(a, "--monsters") == 0) {
            monsters_arg = need("--monsters");
        } else if (std::strcmp(a, "--npcs") == 0) {
            npcs_arg = need("--npcs");
        } else if (std::strcmp(a, "--scale") == 0) {
            ui_scale = static_cast<float>(std::atof(need("--scale")));
            if (ui_scale < 0.5f || ui_scale > 8.0f) {
                std::fprintf(stderr, "--scale must be between 0.5 and 8\n");
                return 2;
            }
        } else if (profile_arg.empty() && openkal::platform::is_profile_path(a)) {
            profile_arg = a;
        } else {
            std::fprintf(stderr, "unknown arg: %s\n", a);
            print_usage(argv[0]);
            return 2;
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
    const std::string& config_root = runtime->configuration.configuration_root;
    const std::string skills_dir =
        !skills_arg.empty() ? skills_arg : openkal::platform::join_path(config_root, "skills");
    const std::string monsters_dir = !monsters_arg.empty()
                                         ? monsters_arg
                                         : openkal::platform::join_path(config_root, "monsters");
    const std::string npcs_dir =
        !npcs_arg.empty() ? npcs_arg : openkal::platform::join_path(config_root, "npcs");

    openkal::skill_effect_view::SkillEffectView view(
        data_root, skills_dir, monsters_dir, npcs_dir, ui_scale);
    return view.run();
}
