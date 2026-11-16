/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/Nameplate.h"
#include "test_util.h"

#include <cmath>
#include <string>
#include <string_view>

namespace openkal::client {

    int icon_draws = 0;
    float icon_x = 0, icon_y = 0, icon_w = 0, icon_h = 0;
    TexInfo WidgetTools::tex_info(const std::string&) {
        return TexInfo({}, 64, 32);
    }

    void WidgetTools::draw_tex(const std::string&, float x, float y, float w, float h, float) {
        ++icon_draws;
        icon_x = x;
        icon_y = y;
        icon_w = w;
        icon_h = h;
    }

    void
    WidgetTools::draw_text_centered(float, float, float, const std::string&, float, float, float) {}

    float WidgetTools::text_width(float, const std::string&) const {
        return 0.0f;
    }

    void WidgetTools::draw_bar(float, float, float, float, float, float, float, float) {}

    void WidgetTools::draw_progress_lcr(const char*, float, float, float, float, float) {}

    float FreetypeFont::text_width(std::string_view, float) const {
        return 0.0f;
    }

    void FreetypeFont::draw_centered(
        engine::IEngine&, float, float, std::string_view, float, float, float, float, float) const {
    }

    void FreetypeFont::draw(
        engine::IEngine&, float, float, std::string_view, float, float, float, float, float) const {
    }

}  // namespace openkal::client

namespace {

    bool near_rgb(const openkal::client::RgbColor& c, int r, int g, int b) {
        const float s = 255.0f;
        return std::fabs(c.r() * s - static_cast<float>(r)) < 0.6f &&
               std::fabs(c.g() * s - static_cast<float>(g)) < 0.6f &&
               std::fabs(c.b() * s - static_cast<float>(b)) < 0.6f;
    }

}  // namespace

int main() {
    using openkal::client::Nameplate;
    using openkal::test::expect;
    using openkal::test::g_failures;

    openkal::client::WidgetTools ui;
    Nameplate::draw_npc_icon(ui, 100, 100, "", 20, 1);
    expect(openkal::client::icon_draws == 0, "NPCs without an icon keep their normal nameplate");
    Nameplate::draw_npc_icon(ui, 100, 100, "owned-icon.bmp", 20, 1.5f);
    expect(openkal::client::icon_draws == 1 && openkal::client::icon_x == 76 &&
               openkal::client::icon_y == 53 && openkal::client::icon_w == 48 &&
               openkal::client::icon_h == 24,
           "NPC overhead icon scales, preserves its aspect ratio and stays above the name");
    Nameplate::draw_npc_icon(ui, 100, 100, "owned-icon.bmp", 0, 1);
    expect(openkal::client::icon_y == 82,
           "configured NPC icon remains above the head even when the name is hidden");

    expect(near_rgb(Nameplate::npc_color(), 180, 150, 255), "npc fixed color");
    expect(near_rgb(Nameplate::player_color(), 255, 255, 255), "player name white");

    // Player 1–10 bracket: same level is yellow, 12+ above is purple, far below
    // is gray.
    expect(near_rgb(Nameplate::monster_color(1, 1), 230, 230, 0), "lv1 vs lv1 yellow");
    expect(near_rgb(Nameplate::monster_color(1, 13), 210, 90, 225), "lv1 vs lv13 purple");
    expect(near_rgb(Nameplate::monster_color(10, 1), 170, 170, 170), "lv10 vs lv1 gray");
    expect(near_rgb(Nameplate::monster_color(1, 4), 255, 180, 60), "lv1 vs lv4 orange");
    expect(near_rgb(Nameplate::monster_color(5, 3), 0, 255, 0), "lv5 vs lv3 green");
    expect(near_rgb(Nameplate::monster_color(1, 8), 255, 0, 0), "lv1 vs lv8 red");
    expect(near_rgb(Nameplate::monster_color(1, 1, 4), 255, 255, 255), "chest race white");

    expect(!Nameplate::show_monster(1, Nameplate::kMonsterRaceMagic), "hide magic race");
    expect(Nameplate::show_monster(1, 0), "show normal race");
    expect(!Nameplate::show_monster_hp(4),
           "gathering plants, Wooden Boxes and Treasure Boxes have no HP bar");
    expect(Nameplate::show_monster_hp(0) && Nameplate::show_monster_hp(1),
           "combat monsters retain their HP bar");
    expect(!Nameplate::show_monster(101, 0), "hide castle gate");
    expect(!Nameplate::show_npc(16, true), "hide flag pole");
    expect(Nameplate::show_npc(3, true), "show merchant");
    expect(!Nameplate::show_npc(3, false), "hide nameFlag 0");
    expect(Nameplate::monster_label("Demon Plunderer", 5) == "Demon Plunderer [5]", "level suffix");
    expect(Nameplate::monster_label("Demon Plunderer", 5, 0) == "Demon Plunderer [5]",
           "combat race keeps level");
    expect(Nameplate::monster_label("Treasure Box", 1, 4) == "Treasure Box",
           "chest race hides level");
    expect(Nameplate::monster_label("", 5).empty(), "empty name stays empty");
    expect(Nameplate::monster_label("", 5, 4).empty(), "empty chest name stays empty");
    expect(Nameplate::debug_label("Demon Plunderer [5]", 42) == "Demon Plunderer [5] [Index: 42]",
           "debug label includes index");
    expect(Nameplate::debug_label("", 42).empty(), "empty debug label stays empty");
    expect(Nameplate::player_label("Hero") == "[Hero]", "player name uses brackets");
    expect(Nameplate::player_label("").empty(), "missing player name creates no empty brackets");
    expect(Nameplate::guild_label("Leader", "OpenKal") == "Leader OpenKal",
           "guild subtitle places the current title before the guild");
    expect(Nameplate::guild_label("Official Member", "OpenKal") == "Official Member OpenKal",
           "multiword member titles remain intact");
    expect(Nameplate::guild_label("Leader", "").empty(), "hidden guild also hides its title");
    expect(Nameplate::guild_label("", "OpenKal") == "OpenKal",
           "missing title adds no leading space");
    expect(Nameplate::debug_label(Nameplate::player_label("Hero"), 7) == "[Hero] [Index: 7]",
           "debug suffix preserves the bracketed player identity");
    expect(std::fabs(Nameplate::kTextScale - 18.0f / 14.0f) < 0.001f,
           "nameplate TTF scale matches item info overlay");
    expect(std::fabs(Nameplate::kGlyphScale - 1.8f) < 0.001f,
           "nameplate bitmap fallback matches item info overlay");

    return g_failures ? 1 : 0;
}
