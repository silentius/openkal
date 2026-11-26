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

#include "openkal/protocol/MonsterRace.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace openkal::client {
    namespace {

        [[nodiscard]] RgbColor from_rgb8(int r, int g, int b) {
            return RgbColor{static_cast<float>(r) / 255.0f,
                            static_cast<float>(g) / 255.0f,
                            static_cast<float>(b) / 255.0f};
        }

    }  // namespace

    RgbColor Nameplate::npc_color() {
        return from_rgb8(180, 150, 255);
    }

    RgbColor Nameplate::player_color() {
        return from_rgb8(255, 255, 255);
    }

    RgbColor Nameplate::monster_color(int player_level, int monster_level, int race) {
        if (protocol::MonsterRaceInfo::is_openable(race)) {
            return from_rgb8(255, 255, 255);
        }
        // Choose colours by player-level bracket and monster/player level difference.
        const int level_form[8][2] = {
            {1, 10}, {11, 20}, {21, 30}, {31, 40}, {41, 50}, {51, 60}, {61, 70}, {71, 255}};
        const int vs_form[8][7][2] = {
            {{-255, -7}, {-6, -4}, {-3, -1}, {0, 2}, {3, 6}, {7, 11}, {12, 255}},
            {{-255, -8}, {-7, -5}, {-4, -2}, {-1, 2}, {3, 6}, {7, 11}, {12, 255}},
            {{-255, -9}, {-8, -6}, {-5, -3}, {-2, 1}, {2, 5}, {6, 10}, {11, 255}},
            {{-255, -10}, {-9, -7}, {-6, -4}, {-3, 1}, {2, 5}, {6, 10}, {11, 255}},
            {{-255, -11}, {-10, -8}, {-7, -5}, {-4, 0}, {1, 4}, {5, 9}, {10, 255}},
            {{-255, -12}, {-11, -9}, {-8, -6}, {-5, 0}, {1, 4}, {5, 9}, {10, 255}},
            {{-255, -13}, {-12, -10}, {-9, -7}, {-6, 0}, {1, 3}, {4, 8}, {9, 255}},
            {{-255, -14}, {-13, -11}, {-10, -8}, {-7, 0}, {1, 3}, {4, 8}, {9, 255}}};
        const RgbColor color_form[7] = {from_rgb8(170, 170, 170),
                                        from_rgb8(58, 76, 244),
                                        from_rgb8(0, 255, 0),
                                        from_rgb8(230, 230, 0),
                                        from_rgb8(255, 180, 60),
                                        from_rgb8(255, 0, 0),
                                        from_rgb8(210, 90, 225)};

        const int pl = std::clamp(player_level, 1, 255);
        const int ml = std::clamp(monster_level, 1, 255);
        const int delta = ml - pl;
        int select = 0;
        for (int i = 0; i < 8; ++i) {
            if (level_form[i][0] > pl || level_form[i][1] < pl) {
                continue;
            }
            for (int k = 0; k < 7; ++k) {
                if (vs_form[i][k][0] <= delta && vs_form[i][k][1] >= delta) {
                    select = k;
                    break;
                }
            }
            break;
        }
        return color_form[select];
    }

    bool Nameplate::show_monster(int index, int race) {
        if (!protocol::MonsterRaceInfo::shows_nameplate(race)) {
            return false;
        }
        switch (index) {
        case 101:
        case 213:
        case 446:
        case 447:
        case 455:
        case 456:
        case 457:
        case 458:
        case 459:
        case 460:
            return false;
        default:
            return true;
        }
    }

    bool Nameplate::show_monster_hp(int race) {
        return !protocol::MonsterRaceInfo::is_openable(race);
    }

    bool Nameplate::show_npc(int shape, bool name_enabled) {
        if (!name_enabled) {
            return false;
        }
        // Occupation-war flag pole, goblin-tower chair, and iron door.
        return shape != 16 && shape != 18 && shape != 28;
    }

    std::string Nameplate::monster_label(const std::string& name, int level, int race) {
        if (name.empty()) {
            return {};
        }
        if (protocol::MonsterRaceInfo::is_openable(race)) {
            return name;
        }
        return name + " [" + std::to_string(level) + "]";
    }

    std::string Nameplate::debug_label(const std::string& label, std::int64_t index) {
        if (label.empty()) {
            return {};
        }
        return label + " [Index: " + std::to_string(index) + "]";
    }

    std::string Nameplate::player_label(std::string_view name, std::string_view rank) {
        if (name.empty()) {
            return {};
        }
        const std::string label = "[" + std::string(name) + "]";
        return rank.empty() ? label : std::string(rank) + " " + label;
    }

    std::string Nameplate::guild_label(std::string_view title, std::string_view guild) {
        if (guild.empty()) {
            return {};
        }
        return title.empty() ? std::string(guild) : std::string(title) + " " + std::string(guild);
    }

    void Nameplate::draw_player(engine::IEngine& engine,
                                const FreetypeFont& font,
                                WidgetTools& ui,
                                float screen_x,
                                float screen_y,
                                const std::string& name,
                                const std::string& guild,
                                const RgbColor& color) {
        if (!font.ready() || name.empty()) {
            return;
        }
        const float gap = guild.empty() ? 0.0f : font.line_height() * kTextScale + 6.0f;
        draw(engine, font, ui, screen_x, screen_y - gap, name, color);
        if (!guild.empty()) {
            draw(engine, font, ui, screen_x, screen_y, guild, color);
        }
    }

    void Nameplate::draw(engine::IEngine& engine,
                         const FreetypeFont& font,
                         WidgetTools& ui,
                         float screen_x,
                         float screen_y,
                         const std::string& name,
                         const RgbColor& color) {
        if (name.empty()) {
            return;
        }
        const float pad_x = 3.0f;
        const float pad_y = 2.0f;
        const float x = std::floor(screen_x);
        if (font.ready()) {
            const float text_w = font.text_width(name, kTextScale);
            const float text_h = font.line_height() * kTextScale;
            const float y = std::floor(screen_y) - text_h;
            engine.draw_ui_rect(x - text_w * 0.5f - pad_x,
                                y - pad_y,
                                text_w + pad_x * 2.0f,
                                text_h + pad_y * 2.0f,
                                0.0f,
                                0.0f,
                                0.0f,
                                kBackdropAlpha);
            font.draw_centered(engine, x, y, name, color.r(), color.g(), color.b(), kTextScale);
            return;
        }
        const float text_w = ui.text_width(kGlyphScale, name);
        const float text_h = static_cast<float>(ui.font_glyph_height()) * kGlyphScale;
        const float y = std::floor(screen_y) - text_h;
        engine.draw_ui_rect(x - text_w * 0.5f - pad_x,
                            y - pad_y,
                            text_w + pad_x * 2.0f,
                            text_h + pad_y * 2.0f,
                            0.0f,
                            0.0f,
                            0.0f,
                            kBackdropAlpha);
        ui.draw_text_centered(x, y, kGlyphScale, name, color.r(), color.g(), color.b());
    }

    void Nameplate::draw_npc_icon(WidgetTools& ui,
                                  float x,
                                  float y,
                                  const std::string& file,
                                  float name_height,
                                  float scale) {
        if (file.empty()) {
            return;
        }
        const auto image = ui.tex_info(file);
        if (image.width() <= 0 || image.height() <= 0) {
            return;
        }
        const auto s = std::max(.25f, scale);
        const auto factor = 32.f * s / std::max(image.width(), image.height());
        const auto w = image.width() * factor, h = image.height() * factor;
        ui.draw_tex(file, std::floor(x - w * .5f), std::floor(y - name_height - 2.f * s - h), w, h);
    }

    void Nameplate::draw_hp(
        WidgetTools& ui, float screen_x, float screen_y, float ratio, bool enlarged) {
        const float width = enlarged ? kLargeHpBarW : kHpBarW;
        const float height = enlarged ? kLargeHpBarH : kHpBarH;
        const float x = std::floor(screen_x) - width * 0.5f;
        const float y = std::floor(screen_y);
        ui.draw_bar(x, y, width, height, ratio, 0.85f, 0.12f, 0.12f);
        ui.draw_progress_lcr("main-prograss-red", x, y, width, height, ratio);
    }

}  // namespace openkal::client
