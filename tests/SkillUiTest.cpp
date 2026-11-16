/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/SkillWindow.h"
#include "openkal/protocol/Property.h"
#include "openkal/protocol/TalentTree.h"
#include "test_util.h"

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <memory>

namespace {
    using openkal::client::SkillClickResult;
    using openkal::client::SkillTab;
    using openkal::client::SkillWindow;
    using openkal::client::UiDocument;
    using openkal::client::UiView;
    using openkal::test::expect;

    SkillClickResult click(SkillWindow& window, const UiDocument& doc, const char* id) {
        const auto* widget = doc.find_widget(id);
        expect(widget != nullptr, "skill control exists");
        if (!widget) {
            return SkillClickResult::Ignored;
        }
        float x, y, w, h;
        UiView::widget_rect(*widget,
                            nullptr,
                            window.x(),
                            window.y(),
                            window.w(),
                            window.h(),
                            window.scale(),
                            x,
                            y,
                            w,
                            h);
        return window.handle_click(x + w / 2, y + h / 2);
    }

    void beacon_skill_checks(const openkal::client::UiLayout& layout, const UiDocument& doc) {
        using namespace openkal;
        client::Catalog catalog;
        (void) catalog.load("");
        client::PlayerSkills player;
        SkillWindow window;
        window.set_layout(&layout);
        window.set_catalog(&catalog);
        window.set_player(&player);
        window.set_player_clazz(2);
        window.set_player_level(110);
        window.set_open(true);
        window.set_tab(SkillTab::Talents);
        const auto* grid = doc.find_widget("active_grid");
        for (float scale : {.75f, 1.0f, 1.5f}) {
            window.set_scale(scale);
            window.center_on_screen(1920, 1080);
            const float x = window.x() + (grid->x() + 16) * scale;
            const float y = window.y() + (grid->y() + 16) * scale;
            (void) player.set_learned_level(protocol::kBeaconSkill, 0);
            expect(window.handle_click(x, y) != SkillClickResult::DragBegin,
                   "locked Beacon cannot be equipped");
            (void) player.set_learned_level(protocol::kBeaconSkill, 1);
            expect(window.handle_click(x, y) == SkillClickResult::DragBegin &&
                       window.drag_skill_index() == protocol::kBeaconSkill,
                   "unlocked Beacon can be dragged from the Talents active grid at each scale");
            window.set_drag_skill_index(-1);
        }
        window.sync_cooldown(protocol::kBeaconSkill, 90000, 120000);
        expect(window.on_cooldown(protocol::kBeaconSkill) &&
                   std::abs(window.cooldown_fraction(protocol::kBeaconSkill) - .75f) < .001f,
               "reconnect restores Beacon's remaining cooldown against its full 120 seconds");
        window.tick(10);
        window.sync_cooldown(protocol::kBeaconSkill, 80000, 120000);
        expect(std::abs(window.cooldown_fraction(protocol::kBeaconSkill) - 2.0f / 3) < .001f,
               "buff snapshots do not reset the skill cooldown overlay");
        window.sync_cooldown(protocol::kBeaconSkill, 0, 120000);
        expect(!window.on_cooldown(protocol::kBeaconSkill), "server can clear Beacon's cooldown");
    }

    void capture(SkillWindow& window, const UiDocument& doc, const std::string& prefix) {
        auto engine = std::unique_ptr<openkal::engine::IEngine>(openkal::engine::create_engine());
        if (!engine->init("Dragon skill dialog verification", 1000, 1100)) {
            expect(false, "initialize skill preview");
            return;
        }
        openkal::client::WidgetTools ui;
        const char* data = std::getenv("OPENKAL_TEST_DATA_ROOT");
        ui.init(*engine, data ? data : OPENKAL_REPO_ROOT, OPENKAL_TEST_RUNTIME_ROOT);
        window.sync_fonts(*engine);
        window.set_open(true);
        window.set_player_specialty(1);
        for (float scale : {1.0f, 1.5f}) {
            window.set_scale(scale);
            window.center_on_screen(1000, 1100);
            for (const auto tab : {SkillTab::Common, SkillTab::Action, SkillTab::Talents}) {
                window.set_tab(tab);
                std::vector<std::uint8_t> previous;
                for (int state = 0; state < 3; ++state) {
                    openkal::engine::FrameInput pointer;
                    if (state > 0) {
                        float x, y, w, h;
                        UiView::widget_rect(*doc.find_widget("close"),
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
                        pointer.mouse_x = x + w / 2;
                        pointer.mouse_y = y + h / 2;
                    }
                    pointer.mouse_left = state == 2;
                    window.update_pointer(pointer);
                    expect(engine->begin_offscreen(1000, 1100, .20f, .25f, .22f),
                           "begin skill capture");
                    window.draw(*engine, ui);
                    std::vector<std::uint8_t> rgba;
                    expect(engine->end_offscreen_rgba(rgba), "read skill capture");
                    if (state > 0) {
                        expect(rgba != previous, "skill close hover and pressed artwork differ");
                    }
                    previous = rgba;
                    std::ofstream out(
                        prefix +
                            (tab == SkillTab::Common
                                 ? "-common"
                                 : (tab == SkillTab::Talents ? "-talents" : "-action")) +
                            (scale == 1 ? "-native" : "-scaled") + "-" + std::to_string(state) +
                            ".ppm",
                        std::ios::binary);
                    out << "P6\n1000 1100\n255\n";
                    for (std::size_t i = 0; i + 3 < rgba.size(); i += 4) {
                        out.write(reinterpret_cast<const char*>(rgba.data() + i), 3);
                    }
                    expect(out.good(), "save skill preview");
                }
            }
        }
        window.destroy_fonts(*engine);
        ui.destroy();
        engine->shutdown();
    }
}  // namespace

int main(int argc, char** argv) {
    openkal::client::UiLayout layout;
    expect(
        layout.load_file(std::string(OPENKAL_TEST_RESOURCE_ROOT) + "/config/ui/skill_window.json"),
        "load skill layout");
    UiDocument doc;
    expect(doc.load(layout.root()), "parse skill document");
    openkal::client::Catalog catalog;
    expect(catalog.load_json_dir(std::string(OPENKAL_TEST_RESOURCE_ROOT) + "/config/skills"),
           "load skills");
    openkal::client::PlayerSkills player;
    player.apply_server_info({{1, 3}, {4, 5}}, catalog, 1);
    SkillWindow window;
    window.set_layout(&layout);
    window.set_catalog(&catalog);
    window.set_player(&player);
    window.set_player_clazz(1);
    window.set_player_level(75);
    for (float scale : {.75f, 1.0f, 1.5f}) {
        window.set_scale(scale);
        window.center_on_screen(1920, 1080);
        window.set_open(true);
        window.set_player_specialty(1);
        window.set_tab(SkillTab::Common);
        (void) click(window, doc, "tab_job1");
        expect(window.tab() == SkillTab::Common, "locked job button cannot select a page");
        (void) click(window, doc, "tab_action");
        expect(window.tab() == SkillTab::Action, "Action page is always available");
        window.set_player_specialty(openkal::protocol::kSpecialtyThirdJobA);
        const char* tabs[] = {
            "tab_common", "tab_job1", "tab_job2", "tab_job3", "tab_awaken", "tab_action"};
        for (int i = 0; i < 6; ++i) {
            (void) click(window, doc, tabs[i]);
            expect(window.tab() == static_cast<SkillTab>(i), "unlocked page follows its button");
        }
        (void) click(window, doc, "tab_common");
        const auto* grid = doc.find_widget("active_grid");
        int drags = 0;
        for (int i = 0; i < grid->cols() * grid->rows(); ++i) {
            const auto result = window.handle_click(
                window.x() + (grid->x() + (i % grid->cols()) * grid->pitch_x() + 16) * scale,
                window.y() + (grid->y() + (i / grid->cols()) * grid->pitch_y() + 16) * scale);
            if (result == SkillClickResult::DragBegin) {
                ++drags;
                expect(player.is_learned(window.drag_skill_index()),
                       "drag carries a learned skill");
                window.set_drag_skill_index(-1);
            }
        }
        expect(drags >= 2, "both learned Mage skills can be dragged at each scale");
        const float x = window.x(), y = window.y();
        (void) window.handle_click(x + 160 * scale, y - 40 * scale);
        openkal::engine::FrameInput pointer;
        pointer.mouse_left = true;
        pointer.mouse_x = x + 180 * scale;
        pointer.mouse_y = y - 10 * scale;
        window.update_pointer(pointer);
        expect(std::abs(window.x() - x - 20 * scale) < .01f &&
                   std::abs(window.y() - y - 30 * scale) < .01f,
               "Dragon title dragging follows pointer");
        pointer.mouse_left = false;
        window.update_pointer(pointer);
        expect(click(window, doc, "close") == SkillClickResult::Consumed && !window.open(),
               "anchored footer closes instead of dragging frame");
    }
    beacon_skill_checks(layout, doc);
    if (argc == 2) {
        for (int i = 0; i < openkal::protocol::kTalentSkillCount; ++i) {
            (void) player.set_learned_level(openkal::protocol::kTalentSkillFirst + i, 1);
        }
        (void) player.set_learned_level(openkal::protocol::kBeaconSkill, 1);
        capture(window, doc, argv[1]);
    }
    return openkal::test::g_failures == 0 ? 0 : 1;
}
