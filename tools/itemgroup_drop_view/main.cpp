/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

// Itemgroup drop simulator — rolls an itemgroup for N kills into a bag UI.
//
// usage: itemgroup_drop_view --itemgroup ID --kills N
//                            [openkal.profile] [--attempts N] [--profile FILE] [--data DIR]
//                            [--items DIR] [--ui DIR] [--groups DIR] [--itemgroups DIR]
//                            [--scale N]
//
// Keys: R re-roll · [ ] kills -/+ · -/= itemgroup -/+ · A/a attempts -/+
//       K toggle inventory · Esc quit

#include "openkal/client/Equipment.h"
#include "openkal/client/FreetypeFont.h"
#include "openkal/client/Inventory.h"
#include "openkal/client/ItemDb.h"
#include "openkal/client/SoftCursor.h"
#include "openkal/client/UiLayout.h"
#include "openkal/client/WidgetTools.h"
#include "openkal/engine/engine.h"
#include "openkal/platform/Log.h"
#include "openkal/platform/Path.h"
#include "openkal/platform/RuntimeProfile.h"
#include "openkal/server/world/DropRoller.h"
#include "openkal/server/world/ItemGroupDb.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

#ifndef OPENKAL_REPO_ROOT
#define OPENKAL_REPO_ROOT "."
#endif

    /** Server-authoritative bag size (registers 0–1). */
    constexpr int kSimBagSlots = 72;

    struct SimSlot {
        int index = 0;
        int prefix = 0;
        int count = 0;
    };

    struct SimResult {
        std::array<SimSlot, kSimBagSlots> slots{};
        std::int64_t money = 0;
        int kills_requested = 0;
        int kills_done = 0;
        int attempts_per_kill = 1;
        int itemgroup = 0;
        int drops_rolled = 0;
        int items_inserted = 0;
        bool bag_full = false;
        std::string note;
    };

    [[nodiscard]] bool sim_try_insert(SimResult& sim,
                                      const openkal::client::ItemDb& catalog,
                                      int item_index,
                                      int prefix,
                                      int count) {
        if (item_index <= 0 || count <= 0) {
            return false;
        }
        if (item_index == openkal::server::kMoneyItemIndex) {
            sim.money += count;
            return true;
        }
        const openkal::client::ItemDef* def = catalog.find(item_index);
        const bool stackable = def && def->is_stackable();
        if (stackable) {
            for (auto& slot : sim.slots) {
                if (slot.index == item_index && slot.prefix == prefix && slot.count > 0) {
                    slot.count += count;
                    return true;
                }
            }
            for (auto& slot : sim.slots) {
                if (slot.index == 0 || slot.count <= 0) {
                    slot.index = item_index;
                    slot.prefix = prefix;
                    slot.count = count;
                    return true;
                }
            }
            return false;
        }
        int free = 0;
        for (const auto& slot : sim.slots) {
            if (slot.index == 0 || slot.count <= 0) {
                ++free;
            }
        }
        if (free < count) {
            return false;
        }
        int remaining = count;
        for (auto& slot : sim.slots) {
            if (remaining <= 0) {
                break;
            }
            if (slot.index == 0 || slot.count <= 0) {
                slot.index = item_index;
                slot.prefix = prefix;
                slot.count = 1;
                --remaining;
            }
        }
        return true;
    }

    [[nodiscard]] int sim_slots_used(const SimResult& sim) {
        int used = 0;
        for (const auto& slot : sim.slots) {
            if (slot.index != 0 && slot.count > 0) {
                ++used;
            }
        }
        return used;
    }

    void apply_sim_to_bag(const SimResult& sim, openkal::client::Bag& bag) {
        bag.clear();
        bag.normal.assign(static_cast<std::size_t>(openkal::client::kBagSlots), {});
        bag.money = sim.money;
        for (int i = 0; i < kSimBagSlots; ++i) {
            const auto& s = sim.slots[static_cast<std::size_t>(i)];
            bag.normal[static_cast<std::size_t>(i)] = {s.index, s.count};
        }
    }

    SimResult run_simulation(const openkal::server::DropRoller& roller,
                             const openkal::client::ItemDb& catalog,
                             int itemgroup,
                             int kills,
                             int attempts_per_kill) {
        SimResult sim;
        sim.itemgroup = itemgroup;
        sim.kills_requested = std::max(0, kills);
        sim.attempts_per_kill = std::max(1, attempts_per_kill);

        for (int kill = 1; kill <= sim.kills_requested; ++kill) {
            for (int attempt = 0; attempt < sim.attempts_per_kill; ++attempt) {
                auto outcome = roller.roll_itemgroup(itemgroup);
                if (!outcome) {
                    continue;
                }
                ++sim.drops_rolled;
                const int index = outcome->item_index;
                const int prefix = outcome->prefix;
                const int count = std::max(1, outcome->count);
                if (!sim_try_insert(sim, catalog, index, prefix, count)) {
                    sim.bag_full = true;
                    sim.kills_done = kill - 1;
                    sim.note = "inventory full during kill " + std::to_string(kill) + " of " +
                               std::to_string(sim.kills_requested) + " (stopped)";
                    return sim;
                }
                if (index != openkal::server::kMoneyItemIndex) {
                    ++sim.items_inserted;
                }
            }
            sim.kills_done = kill;
        }

        sim.note = "completed all " + std::to_string(sim.kills_done) + " kills";
        return sim;
    }

    std::string item_label(const openkal::client::ItemDb& catalog, int index, int prefix) {
        const openkal::client::ItemDef* def = catalog.find(index);
        std::string name = def ? def->name() : ("item " + std::to_string(index));
        if (name.empty()) {
            name = "item " + std::to_string(index);
        }
        if (prefix != 0) {
            name += " +" + std::to_string(prefix);
        }
        return name;
    }

    void print_usage(const char* argv0) {
        std::fprintf(stderr,
                     "usage: %s [openkal.profile] --itemgroup ID --kills N [options]\n"
                     "\n"
                     "Simulate itemgroup drops into a 72-slot inventory and show the bag UI.\n"
                     "  --itemgroup ID     outer itemgroup index to roll each kill\n"
                     "  --kills N          number of kills to simulate\n"
                     "  --attempts N       rolls per kill (default 1; monster (itemgroup X Y) "
                     "uses Y)\n"
                     "  --profile FILE     openkal.profile override\n"
                     "  --groups DIR       config/groups override\n"
                     "  --itemgroups DIR   config/itemgroups override\n"
                     "  --data DIR         client data root (HyperText icons)\n"
                     "  --items DIR        optional items override\n"
                     "  --ui DIR           UI layout dir override\n"
                     "  --scale N          UI zoom (default 2)\n"
                     "\n"
                     "Keys: R re-roll  [ ] kills  -/= itemgroup  A/a attempts  K window  Esc "
                     "quit\n"
                     "Stops early when the bag is full and reports the kill counter.\n",
                     argv0);
    }

}  // namespace

int main(int argc, char** argv) {
    using namespace openkal;

    std::string data_arg;
    std::string profile_arg;
    std::string items_arg;
    std::string ui_arg;
    std::string groups_arg;
    std::string itemgroups_arg;
    float ui_scale = 2.0f;
    int itemgroup = -1;
    int kills = -1;
    int attempts = 1;

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
        } else if (std::strcmp(a, "--items") == 0) {
            items_arg = need("--items");
        } else if (std::strcmp(a, "--ui") == 0) {
            ui_arg = need("--ui");
        } else if (std::strcmp(a, "--groups") == 0) {
            groups_arg = need("--groups");
        } else if (std::strcmp(a, "--itemgroups") == 0) {
            itemgroups_arg = need("--itemgroups");
        } else if (std::strcmp(a, "--scale") == 0) {
            ui_scale = static_cast<float>(std::atof(need("--scale")));
            if (ui_scale < 0.25f || ui_scale > 8.0f) {
                std::fprintf(stderr, "--scale must be between 0.25 and 8\n");
                return 2;
            }
        } else if (std::strcmp(a, "--itemgroup") == 0) {
            itemgroup = std::atoi(need("--itemgroup"));
        } else if (std::strcmp(a, "--kills") == 0) {
            kills = std::atoi(need("--kills"));
        } else if (std::strcmp(a, "--attempts") == 0) {
            attempts = std::max(1, std::atoi(need("--attempts")));
        } else if (profile_arg.empty() && platform::is_profile_path(a)) {
            profile_arg = a;
        } else {
            std::fprintf(stderr, "unknown arg: %s\n", a);
            print_usage(argv[0]);
            return 2;
        }
    }

    if (itemgroup < 0 || kills < 0) {
        std::fprintf(stderr, "--itemgroup and --kills are required\n");
        print_usage(argv[0]);
        return 2;
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
    const std::string groups_dir =
        !groups_arg.empty() ? groups_arg : platform::join_path(config_root, "groups");
    const std::string itemgroups_dir =
        !itemgroups_arg.empty() ? itemgroups_arg : platform::join_path(config_root, "itemgroups");

    server::ItemGroupDb drop_db;
    if (!drop_db.load_groups_dir(groups_dir)) {
        log::error("failed to load groups from " + groups_dir);
        return 1;
    }
    if (!drop_db.load_itemgroups_dir(itemgroups_dir)) {
        log::error("failed to load itemgroups from " + itemgroups_dir);
        return 1;
    }
    if (!drop_db.find_itemgroup(itemgroup)) {
        log::error("itemgroup " + std::to_string(itemgroup) + " not found in " + itemgroups_dir);
        return 1;
    }

    server::DropRoller roller(drop_db);

    client::UiLayoutStore ui_layouts;
    ui_layouts.set_directory(ui_dir);
    ui_layouts.preload_defaults();

    client::ItemDb item_catalog;
    if (!item_catalog.load(config_root, data_root, items_arg)) {
        log::error("failed to load items");
        return 1;
    }

    client::Bag bag;
    client::Wear wear;
    SimResult sim = run_simulation(roller, item_catalog, itemgroup, kills, attempts);
    apply_sim_to_bag(sim, bag);

    client::Window win;
    win.set_open(true);
    win.set_scale(ui_scale);
    win.set_player_clazz(0);
    win.set_player_level(1);
    win.set_catalog(&item_catalog);
    win.set_wear(&wear);
    win.set_layout(&ui_layouts.get("inventory"));
    win.set_register_enabled({true, true, false, false, false, false});
    win.sync_from_layout();
    // Keep capacity at the server default (72) even if layout enables more.
    win.set_register_enabled({true, true, false, false, false, false});

    const int window_w = std::max(1000, static_cast<int>(std::lround(1000.0f * ui_scale)));
    const int window_h = std::max(720, static_cast<int>(std::lround(720.0f * ui_scale)));
    win.center_on_screen(static_cast<float>(window_w), static_cast<float>(window_h));

    std::unique_ptr<engine::IEngine> engine(engine::create_engine());
    if (!engine || !engine->init("openkal itemgroup_drop_view", window_w, window_h)) {
        log::error("engine init failed (display / GLFW?)");
        return 1;
    }
    engine->set_cursor_mode(engine::CursorMode::Free);

    client::WidgetTools ui;
    ui.init(*engine, data_root);
    client::FreetypeFont tip_font;
    {
        const std::string font_path = client::FreetypeFont::find_system_sans();
        if (font_path.empty() ||
            !tip_font.load(*engine, font_path, client::FreetypeFont::kTipPixelSize)) {
            log::warn("item tip FreeType font missing; hover tips use the bitmap font");
        } else {
            log::info("item tip font " + font_path);
        }
    }
    client::SoftCursor soft_cursor;
    if (!soft_cursor.load(*engine, config_root)) {
        log::warn("soft cursors missing under config/ui/cursors");
    }

    auto log_sim = [&](const SimResult& r) {
        log::info("itemgroup " + std::to_string(r.itemgroup) + " kills " +
                  std::to_string(r.kills_done) + "/" + std::to_string(r.kills_requested) +
                  " attempts/kill " + std::to_string(r.attempts_per_kill) + " money +" +
                  std::to_string(r.money) + " slots " + std::to_string(sim_slots_used(r)) + "/" +
                  std::to_string(kSimBagSlots) + " — " + r.note);
    };
    log_sim(sim);

    auto rerun = [&]() {
        if (!drop_db.find_itemgroup(itemgroup)) {
            sim.note = "itemgroup " + std::to_string(itemgroup) + " missing";
            sim.bag_full = false;
            sim.kills_done = 0;
            sim.money = 0;
            sim.slots = {};
            apply_sim_to_bag(sim, bag);
            return;
        }
        sim = run_simulation(roller, item_catalog, itemgroup, kills, attempts);
        apply_sim_to_bag(sim, bag);
        win.set_selected(-1);
        win.set_drag_slot(-1);
        log_sim(sim);
    };

    engine::FrameInput in;
    while (engine->poll(in)) {
        if (in.quit || in.key_escape_pressed) {
            break;
        }

        if (ui_layouts.poll() > 0) {
            win.sync_from_layout();
            win.set_register_enabled({true, true, false, false, false, false});
        }

        if (in.key_k_pressed) {
            win.toggle();
            if (win.open()) {
                win.center_on_screen(static_cast<float>(in.width), static_cast<float>(in.height));
            }
        }
        if (in.key_r_pressed) {
            rerun();
        }

        for (int i = 0; i < in.text_char_count; ++i) {
            const char ch = in.text_chars[i];
            if (ch == '[') {
                kills = std::max(0, kills - (in.key_shift ? 100 : 1));
                rerun();
            } else if (ch == ']') {
                kills = std::min(1'000'000, kills + (in.key_shift ? 100 : 1));
                rerun();
            } else if (ch == '-' || ch == '_') {
                itemgroup = std::max(0, itemgroup - (in.key_shift ? 10 : 1));
                rerun();
            } else if (ch == '=' || ch == '+') {
                itemgroup = std::min(100000, itemgroup + (in.key_shift ? 10 : 1));
                rerun();
            } else if (ch == 'a') {
                attempts = std::max(1, attempts - 1);
                rerun();
            } else if (ch == 'A') {
                attempts = std::min(20, attempts + 1);
                rerun();
            }
        }

        win.layout_size(static_cast<float>(in.width), static_cast<float>(in.height));
        win.update_pointer(in, bag);
        if (win.open() && win.capturing_pointer() && !in.mouse_left) {
            (void) win.handle_release(in.mouse_x, in.mouse_y, bag, wear);
        }
        if (in.mouse_left_clicked && !win.dragging_window()) {
            (void) win.handle_click(in.mouse_x, in.mouse_y, bag, wear);
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
        win.draw(ui, *engine, bag);
        win.draw_drag_ghost(ui, in.mouse_x, in.mouse_y);
        soft_cursor.set(client::CursorKind::Default);
        win.draw_tooltip(ui,
                         *engine,
                         bag,
                         tip_font.ready() ? &tip_font : nullptr,
                         static_cast<float>(in.width),
                         static_cast<float>(in.height));

        const float hud_r = sim.bag_full ? 1.0f : 0.9f;
        const float hud_g = sim.bag_full ? 0.45f : 0.9f;
        const float hud_b = sim.bag_full ? 0.35f : 0.85f;

        const std::string line1 =
            "itemgroup " + std::to_string(itemgroup) + "  kills " + std::to_string(sim.kills_done) +
            "/" + std::to_string(kills) + "  attempts/kill " + std::to_string(attempts) +
            "  money +" + std::to_string(sim.money) + "  slots " +
            std::to_string(sim_slots_used(sim)) + "/" + std::to_string(kSimBagSlots);
        const std::string line2 = sim.note;
        std::string line3 = "R re-roll  [ ] kills  -/= itemgroup  A/a attempts  K window  Esc quit";
        if (win.selected() >= 0 && win.selected() < kSimBagSlots) {
            const auto& s = sim.slots[static_cast<std::size_t>(win.selected())];
            if (s.index != 0 && s.count > 0) {
                line3 = "selected: " + item_label(item_catalog, s.index, s.prefix) + " x" +
                        std::to_string(s.count) + "  (index " + std::to_string(s.index) + ")";
            }
        }

        // Drop summary (top unique stacks by count).
        std::map<std::pair<int, int>, int> totals;
        for (const auto& s : sim.slots) {
            if (s.index != 0 && s.count > 0) {
                totals[{s.index, s.prefix}] += s.count;
            }
        }
        std::vector<std::pair<std::pair<int, int>, int>> ranked(totals.begin(), totals.end());
        std::sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) {
            return a.second > b.second;
        });

        ui.draw_text_glyphs(12.0f, 16.0f, 1.0f, line1, hud_r, hud_g, hud_b);
        ui.draw_text_glyphs(12.0f, 34.0f, 1.0f, line2, hud_r, hud_g, hud_b);
        float y = 54.0f;
        const int max_lines = 12;
        for (int i = 0; i < static_cast<int>(ranked.size()) && i < max_lines; ++i) {
            const int idx = ranked[static_cast<std::size_t>(i)].first.first;
            const int pref = ranked[static_cast<std::size_t>(i)].first.second;
            const int cnt = ranked[static_cast<std::size_t>(i)].second;
            const std::string row =
                "  " + item_label(item_catalog, idx, pref) + "  x" + std::to_string(cnt);
            ui.draw_text_glyphs(12.0f, y, 1.0f, row, 0.78f, 0.82f, 0.75f);
            y += 16.0f;
        }
        if (static_cast<int>(ranked.size()) > max_lines) {
            ui.draw_text_glyphs(12.0f,
                                y,
                                1.0f,
                                "  … +" +
                                    std::to_string(static_cast<int>(ranked.size()) - max_lines) +
                                    " more stack types",
                                0.6f,
                                0.65f,
                                0.55f);
        }

        ui.draw_text_glyphs(
            12.0f, static_cast<float>(in.height) - 20.0f, 1.0f, line3, 0.65f, 0.65f, 0.6f);

        soft_cursor.draw(ui, in.mouse_x, in.mouse_y, in.dt, in.key_ctrl);
        engine->end_frame();
    }

    soft_cursor.destroy(*engine);
    win.destroy_fonts(*engine);
    tip_font.destroy(*engine);
    ui.destroy();
    engine->shutdown();
    return 0;
}
