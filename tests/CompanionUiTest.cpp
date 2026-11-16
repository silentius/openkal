/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/RidingUiHost.h"
#include "openkal/client/TransformWindow.h"
#include "openkal/client/UiLayout.h"
#include "test_util.h"

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <memory>

namespace {
    using namespace openkal::client;
    using openkal::test::expect;

    void verify_controls(TransformWindow& window,
                         const UiDocument& transform,
                         const UiDocument& riding,
                         RidingUiHost& host) {
        UiVarMap vars;
        ui_fill_dummy_preview(vars);
        for (float scale : {0.75f, 1.0f, 1.5f}) {
            window.set_scale(scale);
            window.center_on_screen(1920, 1080);
            for (bool equipped : {false, true}) {
                window.set_open(true);
                window.set_egg(equipped, 0, 0, 41, 0, 100, 3);
                float x, y, w, h;
                UiView::widget_rect(*transform.find_widget("close"),
                                    nullptr,
                                    window.x(),
                                    window.y(),
                                    window.w(),
                                    window.h(),
                                    scale,
                                    x,
                                    y,
                                    w,
                                    h);
                expect(window.handle_click(x + w / 2, y + h / 2) ==
                               TransformClickResult::Consumed &&
                           !window.open(),
                       "Shinsu footer closes equipped and empty windows");
            }
            window.set_open(true);
            const float old_x = window.x(), old_y = window.y();
            (void) window.handle_click(old_x + 160 * scale, old_y - 40 * scale);
            openkal::engine::FrameInput input;
            input.mouse_left = true;
            input.mouse_x = old_x + 180 * scale;
            input.mouse_y = old_y - 10 * scale;
            window.update_pointer(input);
            expect(std::abs(window.x() - old_x - 20 * scale) < 0.01f &&
                       std::abs(window.y() - old_y - 30 * scale) < 0.01f,
                   "Shinsu title drag follows scaled pointer");
            input.mouse_left = false;
            window.update_pointer(input);
            for (const char* id : {"stages", "animals"}) {
                const auto* grid = transform.find_widget(id);
                for (int i = 0; i < 3; ++i) {
                    const float x = window.x() + (grid->x() + i * grid->pitch_x() + 16) * scale;
                    const float y = window.y() + (grid->y() + 16) * scale;
                    expect(window.handle_click(x, y) == TransformClickResult::DragBegin,
                           "Shinsu stage and skill dragging follows layout");
                    window.set_drag_skill_index(-1);
                }
            }
            for (const char* id : {"close", "feed", "stablemaster_exchange"}) {
                float x, y, w, h;
                const auto* button = riding.find_widget(id);
                UiView::widget_rect(*button,
                                    nullptr,
                                    80,
                                    100,
                                    riding.window_w() * scale,
                                    riding.window_h() * scale,
                                    scale,
                                    x,
                                    y,
                                    w,
                                    h);
                const auto hit = UiView::hit(riding,
                                             nullptr,
                                             x + w / 2,
                                             y + h / 2,
                                             80,
                                             100,
                                             riding.window_w() * scale,
                                             riding.window_h() * scale,
                                             scale,
                                             vars,
                                             &host);
                expect(hit.kind() == UiHitKind::Button && hit.action() == button->action(),
                       "Riding footer and management actions follow scaled layout");
                host.set_pointer(hit, false);
                expect(host.button_hovered(id) && !host.button_pressed(id), "Riding hover state");
                host.set_pointer(hit, true);
                expect(host.button_pressed(id), "Riding pressed state");
            }
        }
        host.set_pointer({}, false);
        expect(!host.button_hovered("close"), "Riding hover clears outside buttons");
    }

    void capture(TransformWindow& window,
                 const UiDocument& riding,
                 RidingUiHost& host,
                 ItemDb& items,
                 const std::string& prefix) {
        std::unique_ptr<openkal::engine::IEngine> engine(openkal::engine::create_engine());
        if (!engine->init("Companion UI verification", 1000, 1200)) {
            expect(false, "initialize companion preview");
            return;
        }
        WidgetTools ui;
        const char* data = std::getenv("OPENKAL_TEST_DATA_ROOT");
        ui.init(*engine, data ? data : OPENKAL_REPO_ROOT, OPENKAL_TEST_RUNTIME_ROOT);
        expect(items.load(std::string(OPENKAL_REPO_ROOT) + "/config", OPENKAL_REPO_ROOT),
               "load companion items");
        window.set_items(&items);
        Catalog skills;
        ShinsuDb shinsu;
        expect(skills.load(std::string(OPENKAL_REPO_ROOT) + "/config/skills"),
               "load Shinsu skills");
        expect(shinsu.load_dir(std::string(OPENKAL_REPO_ROOT) + "/config/shinsu"),
               "load Shinsu names");
        window.set_catalog(&skills);
        window.set_shinsu(&shinsu);
        window.sync_fonts(*engine);
        FreetypeFont body, title;
        expect(body.load(*engine, FreetypeFont::find_system_sans(), riding.font_pixel_size()),
               "load companion body font");
        expect(title.load(*engine, FreetypeFont::find_system_sans_bold(), 18),
               "load companion title font");
        auto save = [&](const std::string& suffix) {
            std::vector<std::uint8_t> pixels;
            expect(engine->end_offscreen_rgba(pixels), "read companion preview");
            std::ofstream out(prefix + suffix + ".ppm", std::ios::binary);
            out << "P6\n1000 1200\n255\n";
            for (std::size_t i = 0; i + 3 < pixels.size(); i += 4) {
                out.write(reinterpret_cast<const char*>(pixels.data() + i), 3);
            }
            expect(out.good(), "save companion preview");
        };
        for (float scale : {1.0f, 1.5f}) {
            const std::string size = scale == 1 ? "-native" : "-scaled";
            for (bool equipped : {true, false}) {
                window.set_scale(scale);
                window.center_on_screen(1000, 1200);
                window.set_pos(window.x(), 110 * scale);
                window.set_open(true);
                window.set_egg(equipped, 337, 0, 41, 40, 100, 2);
                expect(engine->begin_offscreen(1000, 1200, 0.12f, 0.14f, 0.16f),
                       "begin Shinsu preview");
                window.draw(*engine, ui);
                save("-shinsu" + size + (equipped ? "" : "-empty"));
            }
            for (int state_index = 0; state_index < 3; ++state_index) {
                UiVarMap vars;
                ui_fill_dummy_preview(vars);
                ui_fill_theme_vars(riding, vars);
                openkal::protocol::RidingState state;
                state.kind = openkal::protocol::RidingKind::Battle;
                state.iid = 10;
                state.item_index = 30100;
                state.satiety = 83;
                state.max_satiety = 100;
                state.equipment_iids = {11, 12, 13, 14, 15};
                state.equipment_item_indices = {30200, 30202, 30203, 30204, 30205};
                if (state_index != 0) {
                    state.kind = openkal::protocol::RidingKind::Common;
                    state.item_index = 30000;
                    vars["riding.name"] = "White Tiger";
                    vars["riding.battle"] = "false";
                    vars["stablemaster.open"] = "false";
                    vars["riding.grade_line"] = "--";
                    vars["riding.satiety"] = "--";
                    vars["riding.equipment_hint"] = "Equipment requires a Battle Riding pet.";
                }
                if (state_index == 2) {
                    state.item_index = 0;
                    state.iid = 0;
                    vars["riding.name"] = "No riding animal registered";
                    vars["riding.status"] = "Not registered";
                    vars["riding.remaining"] = "--";
                    vars["riding.hint"] = "Right-click a riding item to register it.";
                }
                host.set_state(state);
                expect(engine->begin_offscreen(1000, 1200, 0.12f, 0.14f, 0.16f),
                       "begin Riding preview");
                UiView::draw(*engine,
                             ui,
                             riding,
                             80 * scale,
                             100 * scale,
                             riding.window_w() * scale,
                             riding.window_h() * scale,
                             scale,
                             vars,
                             body,
                             title,
                             &host,
                             true);
                save("-riding" + size + "-" + std::to_string(state_index));
            }
        }
        window.destroy_fonts(*engine);
        body.destroy(*engine);
        title.destroy(*engine);
        ui.destroy();
        engine->shutdown();
    }
}  // namespace

int main(int argc, char** argv) {
    UiLayout layout;
    expect(layout.load_file(std::string(OPENKAL_REPO_ROOT) + "/config/ui/transform_window.json"),
           "load Shinsu layout");
    UiDocument transform, riding;
    expect(transform.load(layout.root()), "parse Shinsu layout");
    UiLayout riding_layout;
    expect(riding_layout.load_file(std::string(OPENKAL_REPO_ROOT) + "/config/ui/riding.json"),
           "load Riding layout");
    expect(riding.load(riding_layout.root()), "parse Riding layout");
    TransformWindow window;
    window.set_layout(&layout);
    ItemDb items;
    RidingUiHost host(items);
    verify_controls(window, transform, riding, host);
    if (argc == 2) {
        capture(window, riding, host, items, argv[1]);
    }
    return openkal::test::g_failures == 0 ? 0 : 1;
}
