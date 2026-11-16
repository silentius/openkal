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
#include "openkal/client/UiDocument.h"
#include "openkal/client/UiSettings.h"
#include "openkal/client/UiView.h"
#include "test_util.h"

#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <unistd.h>

namespace {
    using namespace openkal::client;
    using openkal::test::expect;

    void verify_layout_cache() {
        const auto path = std::filesystem::temp_directory_path() /
                          ("openkal-layout-cache-" + std::to_string(::getpid()) + ".json");
        const auto write_layout = [&](int width) {
            std::ofstream file(path);
            file << R"({"schema":"openkal.ui/v1","id":"cache","window":{"w":)" << width
                 << R"(},"widgets":[{"id":"label","type":"text","text":"Cached"}]})";
        };
        write_layout(123);
        UiLayout layout;
        expect(layout.load_file(path.string()), "load cache test layout");
        const UiDocument* cached = &UiView::document(&layout);
        expect(cached == &UiView::document(&layout) && cached->window_w() == 123,
               "repeated frame reads reuse the parsed widget document");
        UiDocument editable;
        UiView::reload(editable, &layout);
        editable.set_window_w(999);
        editable.widgets().clear();
        UiView::reload(editable, &layout);
        expect(editable.window_w() == 123 && editable.widgets().size() == 1,
               "mutable document copies cannot corrupt the shared layout");
        UiLayout copy = layout;
        expect(&UiView::document(&copy) == cached, "layout copies share immutable documents");
        const auto original_time = std::filesystem::last_write_time(path);
        write_layout(456);
        std::filesystem::last_write_time(path, original_time + std::chrono::seconds(2));
        expect(layout.reload_if_stale(), "changed layout hot reloads");
        expect(UiView::document(&layout).window_w() == 456 &&
                   UiView::document(&copy).window_w() == 123,
               "hot reload refreshes widgets without changing an independent layout copy");
        {
            std::ofstream file(path);
            file << "invalid JSON";
        }
        std::filesystem::last_write_time(path, original_time + std::chrono::seconds(4));
        expect(!layout.reload_if_stale() && UiView::document(&layout).window_w() == 456,
               "failed hot reload preserves the last valid widget document");
        expect(!layout.load_file(path.string()) && UiView::document(&layout).widgets().empty(),
               "explicit invalid load clears cached widgets");
        write_layout(789);
        expect(layout.load_file(path.string()) && UiView::document(&layout).window_w() == 789,
               "explicit valid load replaces the cached fallback");
        UiView::reload(editable, nullptr);
        expect(editable.widgets().empty() && editable.id().empty(),
               "unbinding a layout clears the destination");
        std::filesystem::remove(path);
    }

    void capture(const std::string& path, const UiLayout& layout) {
        constexpr int width = 900, height = 540;
        std::unique_ptr<openkal::engine::IEngine> engine(openkal::engine::create_engine());
        if (!engine->init("Dragon menu verification", width, height)) {
            expect(false, "initialize menu GL capture");
            return;
        }
        WidgetTools ui;
        ui.init(*engine, OPENKAL_REPO_ROOT, OPENKAL_TEST_RUNTIME_ROOT);
        FreetypeFont font;
        expect(font.load(*engine, font.find_system_sans(), 13), "load menu FreeType font");
        expect(engine->begin_offscreen(width, height, 0.12f, 0.14f, 0.13f), "begin menu capture");
        UiDocument doc;
        UiView::reload(doc, &layout);
        for (const auto& widget : doc.widgets()) {
            const auto& file = widget.file().empty() ? widget.file_default() : widget.file();
            if (!file.empty()) {
                const auto info = ui.tex_info(file);
                expect(info.width() > 0 && info.height() > 0, "all menu textures load at runtime");
            }
        }
        MenuBarWidgetData data;
        const auto draw = [&](float x, float y, float scale, const char* label) {
            font.draw(*engine, x, y - 23, label, 0.95f, 0.90f, 0.80f);
            MenuBarWidget::draw(ui,
                                *engine,
                                font,
                                x,
                                y,
                                472 * scale,
                                42 * scale,
                                scale,
                                data,
                                width,
                                height,
                                &layout);
        };
        draw(20, 40, 1, "Native size / idle");
        data.pointer_enabled = true;
        data.mouse_x = 20 + 21 + 3 * 36 + 17;
        data.mouse_y = 161;
        data.unread_messages = 3;
        data.trading_post_collection_pending = true;
        draw(20, 140, 1, "Hover / Inventory (I) / 3 unread");
        data.pointer_enabled = false;
        data.open[static_cast<std::size_t>(HudMenuAction::Inventory)] = true;
        data.open[static_cast<std::size_t>(HudMenuAction::Quest)] = true;
        data.open[static_cast<std::size_t>(HudMenuAction::Messages)] = true;
        data.unread_messages = 42;
        draw(20, 260, 1, "Inventory, quests and messages open / 42 unread");
        data.unread_messages = 100;
        draw(20, 360, 1.5f, "150% UI scale");
        data.pointer_enabled = true;
        data.mouse_x = 862;
        data.mouse_y = 518;
        draw(464, 498, 1, "Bottom-right edge / tooltip stays inside screen");
        std::vector<std::uint8_t> rgba;
        expect(engine->end_offscreen_rgba(rgba), "read menu capture");
        if (rgba.size() == width * height * 4) {
            const auto pixel = [&](int x, int y, int channel) {
                return rgba[static_cast<std::size_t>((y * width + x) * 4 + channel)];
            };
            int changed = 0, jade = 0;
            for (int y = 0; y < 34; ++y) {
                for (int x = 0; x < 34; ++x) {
                    changed += pixel(149 + x, 44 + y, 0) != pixel(149 + x, 144 + y, 0);
                    const int red = pixel(149 + x, 264 + y, 0);
                    const int green = pixel(149 + x, 264 + y, 1);
                    jade += y > 25 && green > red + 15;
                }
            }
            int white_outline = 0, black_text = 0, badge_changes = 0;
            bool centered = true;
            for (int y = 0; y < 34; ++y) {
                for (int x = 0; x < 34; ++x) {
                    const int red = pixel(365 + x, 144 + y, 0);
                    const int green = pixel(365 + x, 144 + y, 1);
                    const int blue = pixel(365 + x, 144 + y, 2);
                    if (red == pixel(365 + x, 44 + y, 0) && green == pixel(365 + x, 44 + y, 1) &&
                        blue == pixel(365 + x, 44 + y, 2)) {
                        continue;
                    }
                    ++badge_changes;
                    white_outline += red > 190 && green > 190 && blue > 190;
                    black_text += red < 90 && green < 90 && blue < 90;
                    centered = centered && x >= 10 && x <= 24 && y >= 8 && y <= 25;
                }
            }
            expect(centered, "unread count stays centered inside the envelope without a bar");
            expect(badge_changes > 10 && white_outline > 3 && black_text > 3,
                   "unread count has black text and a white outline, hidden for an empty inbox");
            int market_white = 0, market_black = 0, market_changes = 0;
            for (int y = 0; y < 34; ++y) {
                for (int x = 0; x < 34; ++x) {
                    const int red = pixel(401 + x, 144 + y, 0);
                    const int green = pixel(401 + x, 144 + y, 1);
                    const int blue = pixel(401 + x, 144 + y, 2);
                    if (red == pixel(401 + x, 44 + y, 0) && green == pixel(401 + x, 44 + y, 1) &&
                        blue == pixel(401 + x, 44 + y, 2)) {
                        continue;
                    }
                    ++market_changes;
                    market_white += red > 190 && green > 190 && blue > 190;
                    market_black += red < 90 && green < 90 && blue < 90;
                }
            }
            expect(
                market_changes > 10 && market_white > 3 && market_black > 3,
                "pending collection shows an outlined black exclamation mark, hidden when empty");
            expect(changed > 100, "hover renders a distinct button state");
            expect(jade > 10, "open-window jade marker survives blocked pointer");
            std::ofstream out(path, std::ios::binary);
            out << "P6\n" << width << ' ' << height << "\n255\n";
            for (std::size_t i = 0; i < rgba.size(); i += 4) {
                out.write(reinterpret_cast<const char*>(rgba.data() + i), 3);
            }
            expect(out.good(), "write menu preview");
        }
        font.destroy(*engine);
        ui.destroy();
        engine->shutdown();
    }
}  // namespace

int main(int argc, char** argv) {
    verify_layout_cache();
    UiLayoutStore layouts;
    layouts.set_directory(std::string(OPENKAL_REPO_ROOT) + "/config/ui");
    const auto& layout = layouts.get("menu_bar");
    expect(layout.loaded(), "load menu layout");
    UiDocument doc;
    UiView::reload(doc, &layout);
    const std::array<HudMenuAction, 12> actions{{HudMenuAction::Sinsu,
                                                 HudMenuAction::Riding,
                                                 HudMenuAction::Character,
                                                 HudMenuAction::Inventory,
                                                 HudMenuAction::Skill,
                                                 HudMenuAction::Quest,
                                                 HudMenuAction::Party,
                                                 HudMenuAction::Community,
                                                 HudMenuAction::Map,
                                                 HudMenuAction::Messages,
                                                 HudMenuAction::TradingPost,
                                                 HudMenuAction::System}};
    for (float scale : {0.75f, 1.0f, 1.5f}) {
        GameHud hud;
        hud.set_layouts(&layouts);
        hud.set_scale(scale);
        hud.reset_defaults(1920, 1080);
        hud.set_panel_pos(HudPanelId::MenuBar, 600, 80);
        const auto& panel = static_cast<const GameHud&>(hud).panel(HudPanelId::MenuBar);
        expect(panel.w() == 472 * scale && panel.h() == 42 * scale, "compact scaled geometry");
        openkal::engine::FrameInput input;
        std::size_t index = 0;
        for (const auto& widget : doc.widgets()) {
            if (widget.type() != UiWidgetType::Button) {
                continue;
            }
            input.mouse_x = panel.x() + (widget.x() + widget.w() / 2) * scale;
            input.mouse_y = panel.y() + (widget.y() + widget.h() / 2) * scale;
            input.mouse_left = input.mouse_left_clicked = true;
            expect(hud.handle_pointer(input, 1920, 1080) == HudPointerResult::MenuAction,
                   "icon centers dispatch through overlaid artwork");
            expect(index < actions.size() && hud.last_menu_action() == actions[index],
                   "each button dispatches its own action");
            expect(!hud.is_dragging(), "buttons never start a panel drag");
            input.mouse_left = input.mouse_left_clicked = false;
            (void) hud.handle_pointer(input, 1920, 1080);
            expect(hud.last_menu_action() == HudMenuAction::None, "release clears prior action");
            ++index;
        }
        input.mouse_x = panel.x() + 362 * scale;
        input.mouse_y = panel.y() + 20 * scale;
        input.mouse_left = input.mouse_left_clicked = true;
        expect(hud.handle_pointer(input, 1920, 1080) == HudPointerResult::MenuAction &&
                   hud.last_menu_action() == HudMenuAction::Messages && !hud.is_dragging(),
               "badge area opens messages instead of dragging the menu");
        input.mouse_left = input.mouse_left_clicked = false;
        (void) hud.handle_pointer(input, 1920, 1080);
        hud.set_trading_post_collection_pending(true);
        input.mouse_x = panel.x() + 398 * scale;
        input.mouse_y = panel.y() + 20 * scale;
        input.mouse_left = input.mouse_left_clicked = true;
        expect(hud.handle_pointer(input, 1920, 1080) == HudPointerResult::MenuAction &&
                   hud.last_menu_action() == HudMenuAction::TradingPost && !hud.is_dragging(),
               "collection indicator preserves the trading-post button hit region");
        input.mouse_left = input.mouse_left_clicked = false;
        (void) hud.handle_pointer(input, 1920, 1080);
        expect(index == actions.size(), "all eleven actions remain directly accessible");
        input.mouse_x = panel.x() + 10 * scale;
        input.mouse_y = panel.y() + 21 * scale;
        input.mouse_left = input.mouse_left_clicked = true;
        expect(hud.handle_pointer(input, 1920, 1080) == HudPointerResult::Handled &&
                   hud.is_dragging(),
               "left end cap starts drag without activating a button");
        input.mouse_left_clicked = false;
        input.mouse_x += 100;
        input.mouse_y += 70;
        (void) hud.handle_pointer(input, 1920, 1080);
        input.mouse_left = false;
        (void) hud.handle_pointer(input, 1920, 1080);
        expect(!hud.is_dragging() && panel.x() == 700 && panel.y() == 150,
               "dragged menu retains its new position");
        const auto path = std::filesystem::temp_directory_path() /
                          ("openkal-menu-" + std::to_string(::getpid()) + ".json");
        UiSettings saved;
        saved.set_read_mail_ids(42, {7, 4000000000u});
        saved.set_read_mail_ids(43, {9});
        saved.capture_character(hud, 42, "Menu test", scale);
        expect(saved.save(path.string()), "save menu position");
        UiSettings restored;
        expect(restored.load(path.string()), "reload menu position");
        expect(restored.read_mail_ids(42) == std::set<std::uint32_t>({7, 4000000000u}) &&
                   restored.read_mail_ids(43) == std::set<std::uint32_t>({9}) &&
                   restored.read_mail_ids(44).empty(),
               "read mail persists independently per character, including large IDs");
        GameHud reopened;
        reopened.set_layouts(&layouts);
        reopened.reset_defaults(1920, 1080);
        restored.apply_character(reopened, 42, 1);
        const auto& moved = static_cast<const GameHud&>(reopened).panel(HudPanelId::MenuBar);
        expect(std::abs(moved.x() - 700 / scale) < 0.01f &&
                   std::abs(moved.y() - 150 / scale) < 0.01f,
               "saved position is restored in logical pixels");
        reopened.set_panel_pos(HudPanelId::MenuBar, 1900, 1060);
        reopened.clamp_to_screen(1920, 1080);
        expect(moved.x() + moved.w() <= 1920 && moved.y() + moved.h() <= 1080,
               "restored menu is clamped inside the viewport");
        std::filesystem::remove(path);
    }
    expect(MenuBarWidget::action_for("unknown") == HudMenuAction::None,
           "unknown action cannot activate a window");
    if (argc == 3 && std::string(argv[1]) == "--capture") {
        capture(argv[2], layout);
    }
    return openkal::test::g_failures == 0 ? 0 : 1;
}
