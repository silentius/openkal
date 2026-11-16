/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/CharacterInfoWindow.h"
#include "test_util.h"

#include <cstdlib>
#include <fstream>
#include <memory>

namespace {
    using namespace openkal::client;
    using openkal::test::expect;

    NetClient::CharInfoStats sample_stats() {
        NetClient::CharInfoStats stats;
        stats.name = "Remotay";
        stats.job = "Knight";
        stats.guild = "DragonGuard";
        stats.guild_pos = "Member";
        stats.gov_pos = "Commander";
        stats.awaken = "Awaken -";
        stats.clazz = 0;
        stats.level = 75;
        stats.str = 148;
        stats.hth = 120;
        stats.intl = 30;
        stats.wis = 35;
        stats.dex = 70;
        stats.pu_point = 20;
        stats.hp = 4270;
        stats.max_hp = 5320;
        stats.mp = 810;
        stats.max_mp = 1240;
        stats.min_attack = 1012;
        stats.max_attack = 1286;
        stats.min_magic = 120;
        stats.max_magic = 185;
        stats.hit = 286;
        stats.dodge = 72;
        stats.defense = 538;
        stats.absorb = 15;
        stats.res_fire = 35;
        stats.res_ice = 20;
        stats.res_lightning = 15;
        stats.res_curse = 10;
        return stats;
    }

    CharacterInfoClickResult
    click(CharacterInfoWindow& win, const UiDocument& doc, const char* id) {
        const auto* w = doc.find_widget(id);
        expect(w != nullptr, "interaction widget exists");
        if (!w) {
            return CharacterInfoClickResult::Ignored;
        }
        float x, y, width, height;
        UiView::widget_rect(
            *w, nullptr, win.x(), win.y(), win.w(), win.h(), win.scale(), x, y, width, height);
        return win.handle_click(x + width * .5f, y + height * .5f);
    }

    void verify_rail_density(const std::vector<std::uint8_t>& rgba, int x, int y, int w, int h) {
        bool repeated = true;
        bool cropped = true;
        // Compare matching texture phases, including the final partial repeat.
        for (const int left : {x - 40, x + w}) {
            for (int dx = 0; dx < 40; ++dx) {
                const auto same_row = [&](int a, int b) {
                    for (int channel = 0; channel < 3; ++channel) {
                        const auto first =
                            static_cast<std::size_t>(((y + a) * 1000 + left + dx) * 4 + channel);
                        const auto second =
                            static_cast<std::size_t>(((y + b) * 1000 + left + dx) * 4 + channel);
                        if (std::abs(static_cast<int>(rgba[first]) -
                                     static_cast<int>(rgba[second])) > 2) {
                            return false;
                        }
                    }
                    return true;
                };
                repeated = repeated && same_row(200, 264);
                cropped = cropped && same_row(h - 1, h - 65);
            }
        }
        expect(repeated, "side rails repeat at native density");
        expect(cropped, "last partial side rail is cropped, not compressed");
    }

    void capture(CharacterInfoWindow& win, const UiDocument& doc, const std::string& prefix) {
        std::unique_ptr<openkal::engine::IEngine> engine(openkal::engine::create_engine());
        if (!engine->init("Dragon Character Info verification", 900, 900)) {
            expect(false, "initialize GL capture");
            return;
        }
        WidgetTools ui;
        const char* data = std::getenv("OPENKAL_TEST_DATA_ROOT");
        ui.init(*engine, data ? data : OPENKAL_REPO_ROOT, OPENKAL_TEST_RUNTIME_ROOT);
        FreetypeFont fallback;
        win.set_stats(sample_stats());
        win.set_equipment_primary_bonuses({18, 10, 0, 5, 8});
        win.set_open(true);
        win.sync_fonts(*engine);
        for (const auto scale : {1.0f, 1.5f}) {
            win.set_scale(scale);
            win.center_on_screen(1000, 1100);
            std::vector<std::uint8_t> previous;
            for (int state = 0; state < 4; ++state) {
                win.clear_pending();
                if (state > 0) {
                    (void) click(win, doc, "str_plus");
                }
                openkal::engine::FrameInput pointer;
                const auto* apply = doc.find_widget("ok");
                pointer.mouse_x = win.x() + (apply->x() + 30) * scale;
                pointer.mouse_y = win.y() + (apply->y() + 12) * scale;
                if (state < 2) {
                    pointer.mouse_x = 0;
                }
                pointer.mouse_left = state == 3;
                win.update_pointer(pointer);
                expect(engine->begin_offscreen(1000, 1100, .20f, .25f, .22f), "begin capture");
                win.draw(*engine, ui, fallback);
                std::vector<std::uint8_t> rgba;
                expect(engine->end_offscreen_rgba(rgba), "read capture");
                if (scale == 1 && state == 0) {
                    verify_rail_density(rgba,
                                        static_cast<int>(win.x()),
                                        static_cast<int>(win.y()),
                                        static_cast<int>(win.w()),
                                        static_cast<int>(win.h()));
                }
                if (state > 1) {
                    expect(rgba != previous, "hover and pressed artwork change");
                }
                previous = rgba;
                std::ofstream out(prefix + "-" + (scale == 1 ? "native" : "scaled") + "-" +
                                      std::to_string(state) + ".ppm",
                                  std::ios::binary);
                out << "P6\n1000 1100\n255\n";
                for (std::size_t i = 0; i + 3 < rgba.size(); i += 4) {
                    out.write(reinterpret_cast<const char*>(rgba.data() + i), 3);
                }
                expect(out.good(), "save capture");
            }
        }
        expect(fallback.load(*engine, FreetypeFont::find_system_sans(), 13), "resize preview font");
        UiVarMap vars;
        ui_fill_theme_vars(doc, vars);
        ui_fill_dummy_preview(vars);
        vars["ui.title"] = "Character Info";
        vars["ui.ok"] = "Apply";
        for (const auto& size : {std::pair<float, float>{520, 560}, {420, 660}}) {
            expect(engine->begin_offscreen(1000, 1100, .20f, .25f, .22f), "begin resized capture");
            UiView::draw(
                *engine, ui, doc, 240, 200, size.first, size.second, 1, vars, fallback, fallback);
            std::vector<std::uint8_t> rgba;
            expect(engine->end_offscreen_rgba(rgba), "read resized capture");
            verify_rail_density(
                rgba, 240, 200, static_cast<int>(size.first), static_cast<int>(size.second));
            std::ofstream out(prefix + (size.first > 420 ? "-wide.ppm" : "-tall.ppm"),
                              std::ios::binary);
            out << "P6\n1000 1100\n255\n";
            for (std::size_t i = 0; i + 3 < rgba.size(); i += 4) {
                out.write(reinterpret_cast<const char*>(rgba.data() + i), 3);
            }
            expect(out.good(), "save resized capture");
        }
        fallback.destroy(*engine);
        win.destroy_fonts(*engine);
        ui.destroy();
        engine->shutdown();
    }
}  // namespace

int main(int argc, char** argv) {
    UiLayout layout;
    expect(layout.load_file(std::string(OPENKAL_REPO_ROOT) + "/config/ui/character_info.json"),
           "load character layout");
    UiDocument doc;
    expect(doc.load(layout.root()), "load character document");
    CharacterInfoWindow win;
    win.set_layout(&layout);
    win.set_scale(1);
    win.set_open(true);
    win.set_stats(sample_stats());
    win.center_on_screen(1920, 1080);
    expect(win.x() - doc.pad_left() >= 0 && win.y() - doc.pad_top() >= 0 &&
               win.y() + win.h() + doc.pad_bottom() <= 1080,
           "outer frame fits full HD");
    int applied = 0;
    win.set_apply_property([&](std::uint8_t index, std::uint8_t amount) {
        expect(index == 0 && amount == 1, "apply staged STR only");
        ++applied;
        return true;
    });
    expect(click(win, doc, "ok") == CharacterInfoClickResult::Consumed && applied == 0,
           "disabled Apply sends no request");
    (void) click(win, doc, "str_plus");
    (void) click(win, doc, "str_minus");
    (void) click(win, doc, "ok");
    expect(applied == 0, "minus cancels staged points");
    (void) click(win, doc, "str_plus");
    expect(click(win, doc, "ok") == CharacterInfoClickResult::ApplyPending && applied == 1,
           "Apply submits staged stat");
    (void) click(win, doc, "ok");
    expect(applied == 1, "applied points cannot be submitted twice");
    const std::array<const char*, 5> plus_buttons{
        "str_plus", "hth_plus", "intl_plus", "wis_plus", "dex_plus"};
    auto stats = sample_stats();
    stats.pu_point = 500;
    win.set_stats(stats);
    std::array<int, 5> submitted{};
    win.set_apply_property([&](std::uint8_t index, std::uint8_t amount) {
        expect(index < submitted.size(), "valid attribute index");
        if (index < submitted.size()) {
            submitted[index] += amount;
        }
        return true;
    });
    for (const auto* id : plus_buttons) {
        (void) click(win, doc, id);
    }
    (void) click(win, doc, "ok");
    expect(submitted == std::array<int, 5>{1, 1, 1, 1, 1},
           "all five attribute bindings submit correctly");
    submitted.fill(0);
    stats.pu_point = 0;
    win.set_stats(stats);
    for (const auto* id : plus_buttons) {
        (void) click(win, doc, id);
    }
    (void) click(win, doc, "ok");
    expect(submitted == std::array<int, 5>{}, "zero points cannot stage any attribute");
    win.set_stats(sample_stats());
    win.set_apply_property([](std::uint8_t, std::uint8_t) { return false; });
    (void) click(win, doc, "str_plus");
    expect(click(win, doc, "ok") == CharacterInfoClickResult::Consumed,
           "failed submission is retained");
    win.set_apply_property([&](std::uint8_t index, std::uint8_t amount) {
        submitted[index] += amount;
        return true;
    });
    (void) click(win, doc, "ok");
    expect(submitted[0] == 1, "retry submits the pending allocation");

    const float before_x = win.x();
    const float before_y = win.y();
    (void) win.handle_click(before_x + 160, before_y - 35);
    openkal::engine::FrameInput drag;
    drag.mouse_left = true;
    drag.mouse_x = before_x + 190;
    drag.mouse_y = before_y - 15;
    win.update_pointer(drag);
    expect(win.x() == before_x + 30 && win.y() == before_y + 20,
           "title drag moves the whole dialog");
    win.set_open(false);
    win.set_open(true);
    drag.mouse_x += 60;
    win.update_pointer(drag);
    expect(win.x() == before_x + 30, "reopening cannot resume an old drag");
    drag.mouse_left = false;
    win.update_pointer(drag);

    BuffSlotEntry buff;
    buff.icon_file = "shopbuff_004.bmp";
    buff.effect_name = "Strength blessing";
    buff.expire_ms = 120000;
    win.set_apply_buffs({buff});
    const auto* grid = doc.find_widget("apply_buffs");
    expect(win.apply_buff_tooltip(win.x() + grid->x() + 8, win.y() + grid->y() + 8, 1000, nullptr)
                   .find("Strength blessing") != std::string::npos,
           "effect tooltip retained");
    const auto* close = doc.find_widget("exit_bottom");
    for (const float scale : {1.0f, 1.5f}) {
        for (const auto& extra : {std::pair<float, float>{0, 0}, {100, 0}, {0, 100}}) {
            float x, y, w, h;
            const float cw = (doc.window_w() + extra.first) * scale;
            const float ch = (doc.window_h() + extra.second) * scale;
            UiView::widget_rect(*close, nullptr, 40, 84, cw, ch, scale, x, y, w, h);
            expect(x == 40 + cw - 16 * scale && y == 84 + ch,
                   "footer tracks independent window size and scale");
            const auto hit =
                UiView::hit(doc, nullptr, x + w / 2, y + h / 2, 40, 84, cw, ch, scale, {});
            expect(hit.action() == "window.close", "resized footer is clickable");
        }
    }
    expect(click(win, doc, "exit_bottom") == CharacterInfoClickResult::Consumed && !win.open(),
           "footer closes Character Info");
    if (argc == 3 && std::string(argv[1]) == "--capture") {
        capture(win, doc, argv[2]);
    }
    return openkal::test::g_failures == 0 ? 0 : 1;
}
