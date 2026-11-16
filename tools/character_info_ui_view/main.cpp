/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

// Standalone character-info UI sandbox — no world, net, or MainSvr.
//
// usage: character_info_ui_view [openkal.profile] [--profile FILE] [--data DIR] [--ui DIR]
//                               [--class mage|knight|archer|thief] [--level N]
//                               [--pu N]
//
// Keys: C toggle window · 1-4 class · [ ] level · -/= PU · ,/. font px · Esc
// quit Mouse: +/- OK, Apply Item tabs, close, drag title Edit
// config/ui/character_info.json (or --ui) — layouts hot-reload while running.

#include "openkal/client/CharacterInfoWindow.h"
#include "openkal/client/FreetypeFont.h"
#include "openkal/client/JobTitles.h"
#include "openkal/client/SoftCursor.h"
#include "openkal/client/UiDocument.h"
#include "openkal/client/UiLayout.h"
#include "openkal/client/WidgetTools.h"
#include "openkal/engine/engine.h"
#include "openkal/platform/File.h"
#include "openkal/platform/Log.h"
#include "openkal/platform/Path.h"
#include "openkal/platform/RuntimeProfile.h"
#include "openkal/protocol/Progression.h"
#include "openkal/server/Combat.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

namespace {

#ifndef OPENKAL_REPO_ROOT
#define OPENKAL_REPO_ROOT "."
#endif

    const char* class_name(int clazz) {
        switch (clazz) {
        case 0:
            return "Knight";
        case 1:
            return "Magician";
        case 2:
            return "Archer";
        case 3:
            return "Thief";
        default:
            return "?";
        }
    }

    int parse_class(const char* s) {
        if (!s) {
            return 1;
        }
        if (std::strcmp(s, "0") == 0 || std::strcmp(s, "knight") == 0) {
            return 0;
        }
        if (std::strcmp(s, "1") == 0 || std::strcmp(s, "mage") == 0 ||
            std::strcmp(s, "magician") == 0) {
            return 1;
        }
        if (std::strcmp(s, "2") == 0 || std::strcmp(s, "archer") == 0) {
            return 2;
        }
        if (std::strcmp(s, "3") == 0 || std::strcmp(s, "thief") == 0) {
            return 3;
        }
        return -1;
    }

    void print_usage(const char* argv0) {
        std::fprintf(stderr,
                     "usage: %s [openkal.profile] [--profile FILE] [--data DIR] [--ui DIR] "
                     "[--scale N]\n"
                     "          [--class mage|knight|archer|thief] [--level N] [--pu N]\n"
                     "\n"
                     "Standalone character Info window preview (no game world / server).\n"
                     "  C          toggle Info window\n"
                     "  1..4       Knight / Mage / Archer / Thief\n"
                     "  [ ]        level -/+\n"
                     "  - =        PU points -/+  (Shift=5)\n"
                     "  , .        TTF font pixel size -/+\n"
                     "  B          fill sample Apply Item shopbuff icons\n"
                     "  Esc        quit\n"
                     "  mouse      +/- OK, tabs, close buttons, drag title\n"
                     "  --scale N  UI zoom (e.g. 2 = twice native JSON size)\n"
                     "  hot-reload edit the selected configuration's ui/character_info.json\n",
                     argv0);
    }

    [[nodiscard]] openkal::client::NetClient::CharInfoStats
    make_stats(int clazz, int level, int pu, const std::string& name) {
        namespace combat = openkal::server::combat;
        openkal::client::NetClient::CharInfoStats s;
        const auto base = combat::class_base_plus_bonus(clazz);
        s.name = name;
        s.clazz = clazz;
        s.job = openkal::client::JobTitles::class_name(clazz);
        s.specialty_index = 0;
        s.gov_pos = openkal::client::JobTitles::title(clazz, s.specialty_index);
        s.awaken = "Awaken -";
        s.level = std::max(1, level);
        s.ctb = 0;
        s.str = base.str;
        s.hth = base.hth;
        s.intl = base.intl;
        s.wis = base.wis;
        s.dex = base.dex;
        s.pu_point = std::max(0, pu);

        combat::BaseStats stats = base;
        combat::apply_stat_revise(s.level,
                                  stats,
                                  s.min_attack,
                                  s.max_attack,
                                  s.hit,
                                  s.dodge,
                                  s.min_magic,
                                  s.max_magic,
                                  s.res_fire,
                                  s.res_ice,
                                  s.res_lightning,
                                  s.res_curse,
                                  s.res_palsy);
        s.max_hp = combat::max_hp_from_stats(s.hth, s.level, clazz);
        s.max_mp = combat::max_mp_from_stats(s.wis, s.level, clazz);
        s.hp = s.max_hp;
        s.mp = s.max_mp;
        return s;
    }

}  // namespace

int main(int argc, char** argv) {
    using namespace openkal;

    std::string data_arg;
    std::string profile_arg;
    std::string ui_arg;
    float ui_scale = 2.0f;
    int clazz = 1;
    int level = 2;
    int pu = protocol::kPuPerLevel * 2;

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
        } else if (std::strcmp(a, "--ui") == 0) {
            ui_arg = need("--ui");
        } else if (std::strcmp(a, "--scale") == 0) {
            ui_scale = static_cast<float>(std::atof(need("--scale")));
            if (ui_scale < 0.25f || ui_scale > 8.0f) {
                std::fprintf(stderr, "--scale must be between 0.25 and 8\n");
                return 2;
            }
        } else if (std::strcmp(a, "--class") == 0) {
            clazz = parse_class(need("--class"));
            if (clazz < 0) {
                std::fprintf(stderr, "unknown class\n");
                return 2;
            }
        } else if (std::strcmp(a, "--level") == 0) {
            level = std::max(1, std::atoi(need("--level")));
        } else if (std::strcmp(a, "--pu") == 0) {
            pu = std::max(0, std::atoi(need("--pu")));
        } else if (profile_arg.empty() && platform::is_profile_path(a)) {
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
        platform::RuntimeProfile::load_for_tool(repo_root, profile_arg, &profile_error);
    if (!runtime) {
        log::error(profile_error);
        return 1;
    }
    const std::string data_root = data_arg.empty() ? runtime->profile.data_root() : data_arg;
    if (!platform::is_directory(data_root)) {
        log::error("client data root is not a directory: " + data_root);
        return 1;
    }
    const std::string& config_root = runtime->configuration.configuration_root;
    const std::string ui_dir = !ui_arg.empty() ? ui_arg : platform::join_path(config_root, "ui");

    client::UiLayoutStore ui_layouts;
    ui_layouts.set_directory(ui_dir);
    ui_layouts.preload_defaults();
    if (!client::JobTitles::load(platform::join_path(config_root, "jobs"))) {
        log::warn("JobTitles: failed to load " + platform::join_path(config_root, "jobs"));
    }

    const client::UiLayout* layout = &ui_layouts.get("character_info");
    client::UiDocument v1;
    (void) v1.load(layout->root());
    int font_px =
        v1.is_v1()
            ? v1.font_pixel_size()
            : static_cast<int>(std::lround(client::UiLayout::get_f(layout, "font_size", 12.0f)));
    font_px = std::clamp(font_px, 8, 48);

    client::CharacterInfoWindow win;
    win.set_open(true);
    win.set_scale(ui_scale);
    win.set_layout(layout);
    win.sync_from_layout();

    // Live mutable snapshot so OK can mutate preview stats without a server.
    client::NetClient::CharInfoStats live = make_stats(clazz, level, pu, "Remotay");
    win.set_stats(live);
    win.set_apply_property([&](std::uint8_t property, std::uint8_t count) {
        if (property > 4) {
            return false;
        }
        int* primary = nullptr;
        switch (property) {
        case 0:
            primary = &live.str;
            break;
        case 1:
            primary = &live.hth;
            break;
        case 2:
            primary = &live.intl;
            break;
        case 3:
            primary = &live.wis;
            break;
        case 4:
            primary = &live.dex;
            break;
        default:
            break;
        }
        if (!primary) {
            return false;
        }
        if (server::combat::spend_primary_points(
                live.clazz, static_cast<int>(property), count, *primary, live.pu_point) <= 0) {
            return false;
        }
        live.min_attack = 0;
        live.max_attack = 0;
        live.min_magic = 0;
        live.max_magic = 0;
        live.hit = 0;
        live.dodge = 0;
        live.res_fire = 0;
        live.res_ice = 0;
        live.res_lightning = 0;
        live.res_curse = 0;
        live.res_palsy = 0;
        server::combat::BaseStats stats;
        stats.str = live.str;
        stats.hth = live.hth;
        stats.intl = live.intl;
        stats.wis = live.wis;
        stats.dex = live.dex;
        server::combat::apply_stat_revise(live.level,
                                          stats,
                                          live.min_attack,
                                          live.max_attack,
                                          live.hit,
                                          live.dodge,
                                          live.min_magic,
                                          live.max_magic,
                                          live.res_fire,
                                          live.res_ice,
                                          live.res_lightning,
                                          live.res_curse,
                                          live.res_palsy);
        live.max_hp = server::combat::max_hp_from_stats(live.hth, live.level, live.clazz);
        live.max_mp = server::combat::max_mp_from_stats(live.wis, live.level, live.clazz);
        live.hp = live.max_hp;
        live.mp = live.max_mp;
        pu = live.pu_point;
        win.set_stats(live);
        return true;
    });

    const int window_w = std::max(900, static_cast<int>(std::lround(900.0f * ui_scale)));
    const int window_h = std::max(700, static_cast<int>(std::lround(700.0f * ui_scale)));
    win.center_on_screen(static_cast<float>(window_w), static_cast<float>(window_h));

    std::unique_ptr<engine::IEngine> engine(engine::create_engine());
    if (!engine || !engine->init("openkal character_info_ui_view", window_w, window_h)) {
        log::error("engine init failed (display / GLFW?)");
        return 1;
    }
    engine->set_cursor_mode(engine::CursorMode::Free);

    client::WidgetTools ui;
    ui.init(*engine, data_root);

    const std::string font_path = client::FreetypeFont::find_system_sans();
    client::FreetypeFont ui_font;
    auto reload_font = [&](int px) {
        font_px = std::clamp(px, 8, 48);
        ui_font.destroy(*engine);
        if (font_path.empty() || !ui_font.load(*engine, font_path, font_px)) {
            log::warn("FreeType font missing or failed at px=" + std::to_string(font_px));
            return false;
        }
        return true;
    };
    if (!reload_font(font_px)) {
        log::warn("continuing without TTF — labels will be blank");
    } else {
        log::info("info font " + font_path + " px=" + std::to_string(font_px));
    }

    client::SoftCursor soft_cursor;
    if (!soft_cursor.load(*engine, config_root)) {
        log::warn("soft cursors missing under config/ui/cursors");
    }
    log::info("character_info_ui_view data=" + data_root + " ui=" + ui_dir +
              " scale=" + std::to_string(ui_scale) + " class=" + class_name(clazz));

    auto reseat = [&]() {
        live = make_stats(clazz, level, pu, "Remotay");
        win.clear_pending();
        win.set_stats(live);
    };

    std::string status = "ready — edit character_info.json for offsets";
    engine::FrameInput in;
    while (engine->poll(in)) {
        if (in.quit || in.key_escape_pressed) {
            break;
        }

        if (ui_layouts.poll() > 0) {
            layout = &ui_layouts.get("character_info");
            win.set_layout(layout);
            win.sync_from_layout();
            const int from_layout = static_cast<int>(std::lround(
                client::UiLayout::get_f(layout, "font_size", static_cast<float>(font_px))));
            if (from_layout != font_px) {
                reload_font(from_layout);
            }
            status = "layout reloaded (font_size=" + std::to_string(font_px) + ")";
        }

        if (in.key_c_pressed) {
            win.toggle();
            if (win.open()) {
                win.center_on_screen(static_cast<float>(in.width), static_cast<float>(in.height));
            }
            status = win.open() ? "opened Info" : "closed Info";
        }
        if (in.key_digit_pressed[1]) {
            clazz = 0;
            reseat();
            status = "class Knight";
        }
        if (in.key_digit_pressed[2]) {
            clazz = 1;
            reseat();
            status = "class Magician";
        }
        if (in.key_digit_pressed[3]) {
            clazz = 2;
            reseat();
            status = "class Archer";
        }
        if (in.key_digit_pressed[4]) {
            clazz = 3;
            reseat();
            status = "class Thief";
        }

        for (int i = 0; i < in.text_char_count; ++i) {
            const char ch = in.text_chars[i];
            if (ch == '[') {
                level = std::max(1, level - 1);
                reseat();
                status = "level " + std::to_string(level);
            } else if (ch == ']') {
                level = std::min(200, level + 1);
                reseat();
                status = "level " + std::to_string(level);
            } else if (ch == '-' || ch == '_') {
                const int step = in.key_shift ? 5 : 1;
                pu = std::max(0, pu - step);
                live.pu_point = pu;
                win.set_stats(live);
                status = "PU " + std::to_string(pu);
            } else if (ch == '=' || ch == '+') {
                const int step = in.key_shift ? 5 : 1;
                pu += step;
                live.pu_point = pu;
                win.set_stats(live);
                status = "PU " + std::to_string(pu);
            } else if (ch == ',') {
                if (reload_font(font_px - 1)) {
                    status =
                        "font_px " + std::to_string(font_px) + " (set font_size in JSON to keep)";
                }
            } else if (ch == '.') {
                if (reload_font(font_px + 1)) {
                    status =
                        "font_px " + std::to_string(font_px) + " (set font_size in JSON to keep)";
                }
            } else if (ch == 'b' || ch == 'B') {
                win.set_apply_buffs({{0, "shopbuff_001.bmp"},
                                     {0, "shopbuff_002.bmp"},
                                     {0, "shopbuff_003.bmp"},
                                     {0, "shopbuff_004.bmp"},
                                     {0, "shopbuff_005.bmp"}});
                status = "sample Apply Item shopbuff icons";
            }
        }

        if (std::fabs(win.scale() - ui_scale) > 0.001f) {
            win.set_scale(ui_scale);
            win.sync_from_layout();
        }
        win.update_pointer(in);
        if (in.mouse_left_clicked && !win.dragging()) {
            const auto click = win.handle_click(in.mouse_x, in.mouse_y, &ui);
            if (click == client::CharacterInfoClickResult::ApplyPending) {
                status = "applied pending → PU " + std::to_string(live.pu_point);
            } else if (click == client::CharacterInfoClickResult::Consumed) {
                status = "click consumed";
            }
        }

        engine->begin_frame(0.10f, 0.10f, 0.12f);
        engine->draw_ui_rect(0,
                             0,
                             static_cast<float>(in.width),
                             static_cast<float>(in.height),
                             0.12f,
                             0.11f,
                             0.14f,
                             1.0f);

        win.sync_fonts(*engine);
        win.draw(*engine, ui, ui_font);
        soft_cursor.set(client::CursorKind::Default);

        const std::string hud = std::string(class_name(clazz)) + "  Lv " + std::to_string(level) +
                                "  PU " + std::to_string(live.pu_point) + "  font " +
                                std::to_string(font_px) + "px  scale " + std::to_string(ui_scale) +
                                "  |  " + status;
        ui.draw_text_glyphs(
            12.0f, static_cast<float>(in.height) - 36.0f, 1.0f, hud, 0.9f, 0.9f, 0.85f);
        ui.draw_text_glyphs(12.0f,
                            static_cast<float>(in.height) - 20.0f,
                            1.0f,
                            "C window  1-4 class  [ ] level  -/= PU  ,/. font  B "
                            "buffs  Esc quit  |  edit character_info.json",
                            0.65f,
                            0.65f,
                            0.6f);

        soft_cursor.draw(ui, in.mouse_x, in.mouse_y, in.dt, in.key_ctrl);
        engine->end_frame();
    }

    soft_cursor.destroy(*engine);
    win.destroy_fonts(*engine);
    ui_font.destroy(*engine);
    ui.destroy();
    engine->shutdown();
    return 0;
}
