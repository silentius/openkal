/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/GameHud.h"
#include "openkal/client/MinimapWidget.h"
#include "openkal/client/UiSettings.h"
#include "openkal/client/WorldMapWidget.h"
#include "openkal/map/Coords.h"
#include "test_util.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <memory>
#include <unistd.h>

namespace {
    using openkal::client::MinimapWidget;
    using openkal::test::expect;

    void capture(const std::string& path,
                 const std::string& data_root,
                 const openkal::client::UiLayout& layout) {
        std::unique_ptr<openkal::engine::IEngine> engine(openkal::engine::create_engine());
        if (!engine->init("Minimap verification", 900, 620)) {
            expect(false, "initialize GL capture");
            return;
        }
        openkal::client::WidgetTools ui;
        ui.init(*engine, data_root, OPENKAL_TEST_RUNTIME_ROOT);
        openkal::client::FreetypeFont font, bold;
        expect(font.load(*engine, font.find_system_sans(), 12), "load minimap font");
        expect(bold.load(*engine, bold.find_system_sans_bold(), 12), "load bold minimap font");
        expect(engine->begin_offscreen(900, 620, 0.10f, 0.14f, 0.18f), "begin UI capture");

        // Exercise masking of both primitive paths, and clearing it for subsequent UI.
        engine->set_ui_clip_circle(820, 520, 25);
        engine->draw_ui_rect(780, 480, 80, 80, 1, 0, 0, 1);
        engine->draw_ui_triangle(780, 480, 860, 480, 820, 560, 0, 1, 0, 1);
        engine->set_ui_clip_circle(0, 0, 0);
        engine->draw_ui_rect(800, 580, 20, 20, 0, 0, 1, 1);

        openkal::client::MinimapWidgetData data;
        data.set_world_pos(openkal::coords::to_client(openkal::coords::kNarootuhServerX),
                           openkal::coords::to_client(openkal::coords::kNarootuhServerY));
        for (int i = 0; i < 3; ++i) {
            data.set_area_name(i == 0 ? "Village of Narootuh"
                                      : (i == 1 ? "Naraeha" : "The City of Priest"));
            data.set_zoom(i == 0 ? 1 : (i == 1 ? 4 : 8));
            data.set_camera_yaw_rad(static_cast<float>(i));
            data.set_player_yaw_rad(static_cast<float>(i) + 0.5f);
            MinimapWidget::draw(ui, font, bold, 20.0f + i * 285.0f, 20, 260, 272, data, &layout);
        }
        data.set_dungeon(openkal::map::MiniMapSize::dungeon_defaults("tomb"), 6, 4, 32);
        data.set_world_pos(openkal::coords::to_client(6 * openkal::map::kWorldSize + 8192),
                           openkal::coords::to_client(4 * openkal::map::kWorldSize + 8192));
        data.set_area_name("D1");
        MinimapWidget::draw(ui, font, bold, 25, 330, 195, 204, data, &layout);
        data.set_zoom(1);
        data.set_area_name("D5");
        MinimapWidget::draw(ui, font, bold, 280, 320, 260, 272, data, &layout);
        std::vector<std::uint8_t> rgba;
        expect(engine->end_offscreen_rgba(rgba), "read minimap capture");
        if (rgba.size() == 900 * 620 * 4) {
            const auto pixel = [&](int x, int y, int channel) {
                return rgba[static_cast<std::size_t>((y * 900 + x) * 4 + channel)];
            };
            expect(pixel(820, 520, 1) > 240, "circle center draws triangle");
            expect(pixel(785, 485, 0) < 40 && pixel(785, 485, 1) < 50,
                   "UI rectangle and triangle both clip outside circle");
            expect(pixel(810, 590, 2) > 240, "clip is cleared for later UI");
            expect(pixel(240, 78, 0) < 40 && pixel(240, 78, 1) < 50,
                   "minimap tile corners do not fill the transparent frame corners");
            std::ofstream out(path, std::ios::binary);
            out << "P6\n900 620\n255\n";
            for (std::size_t i = 0; i < rgba.size(); i += 4) {
                out.write(reinterpret_cast<const char*>(rgba.data() + i), 3);
            }
            expect(out.good(), "write preview");
        }
        font.destroy(*engine);
        bold.destroy(*engine);
        ui.destroy();
        engine->shutdown();
    }
}  // namespace

int main(int argc, char** argv) {
    using namespace openkal::client;
    UiLayoutStore layouts;
    layouts.set_directory(std::string(OPENKAL_REPO_ROOT) + "/config/ui");
    const auto& layout = layouts.get("minimap");
    expect(layout.loaded(), "load minimap layout");
    GameHud hud;
    hud.set_layouts(&layouts);
    hud.reset_defaults(1920, 1080);
    hud.set_panel_pos(HudPanelId::Minimap, 400, 200);
    const auto& panel = static_cast<const GameHud&>(hud).panel(HudPanelId::Minimap);
    expect(panel.movable() && panel.w() == 260 && panel.h() == 272, "native movable minimap");
    expect(!hud.contains(401, 201), "transparent upper-left corner passes through");
    expect(!hud.contains(659, 471), "transparent lower-right corner passes through");
    expect(hud.contains(530, 349), "map center captures pointer");
    expect(hud.contains(562, 230), "nameplate captures pointer");

    float x = 0, y = 0, w = 0, h = 0;
    expect(MinimapWidget::zoom_plus_rect(400, 200, 390, 408, &layout, x, y, w, h),
           "scaled zoom hitbox available");
    expect(x == 724 && y == 488 && w == 39 && h == 39,
           "zoom hitbox uses document size and panel scale");
    openkal::engine::FrameInput input;
    input.mouse_x = 629;
    input.mouse_y = 405;
    input.mouse_left = input.mouse_left_clicked = true;
    expect(hud.handle_pointer(input, 1920, 1080) == HudPointerResult::Handled && !hud.is_dragging(),
           "zoom button consumes click without starting drag");
    input.mouse_x = 562;
    input.mouse_y = 230;
    expect(hud.handle_pointer(input, 1920, 1080) == HudPointerResult::Handled && hud.is_dragging(),
           "nameplate starts dragging");
    input.mouse_left_clicked = false;
    input.mouse_x += 200;
    input.mouse_y += 120;
    (void) hud.handle_pointer(input, 1920, 1080);
    input.mouse_left = false;
    (void) hud.handle_pointer(input, 1920, 1080);
    expect(!hud.is_dragging() && panel.x() == 600 && panel.y() == 320,
           "drag release keeps new position");

    const auto path = std::filesystem::temp_directory_path() /
                      ("openkal-minimap-hud-" + std::to_string(::getpid()) + ".json");
    UiSettings settings;
    settings.capture_character(hud, 42, "Minimap test", 1);
    expect(settings.save(path.string()), "save dragged HUD position");
    UiSettings restored;
    expect(restored.load(path.string()), "reload HUD settings from disk");
    GameHud reopened;
    reopened.set_layouts(&layouts);
    reopened.set_scale(1.5f);
    reopened.reset_defaults(1920, 1080);
    restored.apply_character(reopened, 42, 1.5f);
    const auto& saved = static_cast<const GameHud&>(reopened).panel(HudPanelId::Minimap);
    expect(saved.x() == 900 && saved.y() == 480,
           "saved logical position survives restart and scale");
    reopened.set_panel_pos(HudPanelId::Minimap, 1900, 1060);
    reopened.clamp_to_screen(1920, 1080);
    expect(saved.x() + saved.w() <= 1920 && saved.y() + saved.h() <= 1080,
           "larger minimap is clamped after restoring old positions");
    std::filesystem::remove(path);

    MinimapWidgetData data;
    data.set_world_pos(openkal::coords::to_client(openkal::coords::kNarootuhServerX),
                       openkal::coords::to_client(openkal::coords::kNarootuhServerY));
    expect(MinimapWidget::coordinates_text(data) == "8040, 8085", "coordinates use world tiles");
    data.set_origin_patch(6);
    data.set_world_pos(openkal::coords::to_client(openkal::coords::kNarootuhServerX, 6),
                       openkal::coords::to_client(openkal::coords::kNarootuhServerY, 6));
    expect(MinimapWidget::coordinates_text(data) == "8040, 8085",
           "coordinates honor profile origin");
    data.set_player_yaw_rad(1.25f);
    expect(data.player_yaw_rad() == 1.25f, "player yaw is retained for the rotating map marker");
    data.set_world_pos(0.0f, 0.0f);
    float world_x = 0.0f;
    float world_y = 0.0f;
    WorldMapWidget::marker_position(1920.0f, 1080.0f, data, world_x, world_y);
    data.set_world_pos(8192.0f, 8192.0f);
    float moved_world_x = 0.0f;
    float moved_world_y = 0.0f;
    WorldMapWidget::marker_position(1920.0f, 1080.0f, data, moved_world_x, moved_world_y);
    expect(moved_world_x - world_x > 25.0f && moved_world_y - world_y < -15.0f,
           "world-map marker follows the minimap-section atlas");
    expect(WorldMapWidget::close_contains(1848.0f, 1048.0f, 1920.0f, 1080.0f),
           "world-map lower-right close target is reachable");
    expect(!WorldMapWidget::close_contains(100.0f, 100.0f, 1920.0f, 1080.0f),
           "world-map close target does not consume the map body");
    if (argc == 4 && std::string(argv[1]) == "--capture") {
        capture(argv[2], argv[3], layout);
    }
    return openkal::test::g_failures == 0 ? 0 : 1;
}
