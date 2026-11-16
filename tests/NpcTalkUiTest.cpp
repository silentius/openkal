/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/NpcTalkWindow.h"
#include "openkal/client/UiLayout.h"
#include "test_util.h"

#include <cmath>
#include <fstream>
#include <memory>
#include <string>
#include <utility>

namespace {
    using namespace openkal::client;
    using openkal::test::expect;

    std::pair<float, float>
    control(const NpcTalkWindow& window, const UiDocument& doc, const char* id) {
        const auto* widget = doc.find_widget(id);
        expect(widget != nullptr, "NPC control exists");
        if (!widget) {
            return {};
        }
        float x, y, w, h;
        UiView::widget_rect(*widget,
                            nullptr,
                            window.x(),
                            window.y(),
                            window.w() * window.scale(),
                            window.h() * window.scale(),
                            window.scale(),
                            x,
                            y,
                            w,
                            h);
        return {x + w / 2, y + h / 2};
    }

    void navigate(NpcTalkWindow& window, const UiDocument& doc, const char* id) {
        const auto [x, y] = control(window, doc, id);
        expect(window.contains(x, y), "navigation stays inside the window");
        expect(window.handle_click(x, y) == NpcTalkClickResult::Consumed && window.open(),
               "navigation keeps the conversation open");
    }

    DialogDef choices() {
        DialogDef dialog;
        dialog.height = 200;
        for (int i = 0; i < 12; ++i) {
            DialogLink link;
            link.type = std::to_string(1000 + i);
            link.text = "Option " + std::to_string(i + 1);
            dialog.links.push_back(link);
        }
        return dialog;
    }

    void verify_pages(NpcTalkWindow& window, const UiDocument& doc) {
        const auto dialog = choices();
        for (const float scale : {.75f, 1.0f, 1.5f}) {
            window.set_scale(scale);
            for (int selected = 0; selected < 12; ++selected) {
                window.show_dialog(dialog, 77, "Tester");
                window.center_on_screen(1920, 1080);
                navigate(window, doc, "previous");
                for (int page = 0; page < selected / 2; ++page) {
                    navigate(window, doc, "next");
                }
                if (selected >= 10) {
                    navigate(window, doc, "next");
                }
                const float old_x = window.x(), old_y = window.y();
                (void) window.handle_click(old_x + 160 * scale, old_y - 40 * scale);
                openkal::engine::FrameInput input;
                input.mouse_left = true;
                input.mouse_x = old_x + 200 * scale;
                input.mouse_y = old_y - 10 * scale;
                window.update_pointer(input);
                input.mouse_left = false;
                window.update_pointer(input);
                expect(std::abs(window.x() - old_x - 40 * scale) < .01f &&
                           std::abs(window.y() - old_y - 30 * scale) < .01f,
                       "drag moves the paged window");
                const float y = window.y() + (36 + (selected % 2) * 46) * scale;
                expect(window.handle_click(window.x() + 24 * scale, y) ==
                               NpcTalkClickResult::Consumed &&
                           window.open(),
                       "response text does not activate the adjacent service");
                expect(window.handle_click(window.x() + (window.w() - 58) * scale, y) ==
                           NpcTalkClickResult::OpenDialog,
                       "visible response remains clickable after dragging and scaling");
                expect(window.pending_dialog_id() == 1000 + selected && window.npc_id() == 77,
                       "each page selects its own link and preserves NPC context");
            }
            window.show_dialog(dialog);
            for (int i = 0; i < 3; ++i) {
                navigate(window, doc, "next");
            }
            navigate(window, doc, "previous");
            const float x = window.x() + (window.w() - 58) * scale;
            const float y = window.y() + 36 * scale;
            expect(window.handle_click(x, y) == NpcTalkClickResult::OpenDialog &&
                       window.pending_dialog_id() == 1004,
                   "previous page restores its response mapping");
            window.show_dialog(dialog);
            openkal::engine::FrameInput wheel;
            wheel.mouse_x = window.x() + 30 * scale;
            wheel.mouse_y = window.y() + 30 * scale;
            wheel.mouse_scroll = -1;
            window.update_pointer(wheel);
            expect(window.handle_click(x, y) == NpcTalkClickResult::OpenDialog &&
                       window.pending_dialog_id() == 1002,
                   "mouse wheel advances the conversation");
            window.show_dialog(dialog);
            expect(window.handle_click(x, y) == NpcTalkClickResult::OpenDialog &&
                       window.pending_dialog_id() == 1000,
                   "opening another conversation resets its page");
        }
    }

    void verify_npc_lifetime(NpcTalkWindow& window) {
        window.show_dialog(choices(), 77);
        expect(!window.close_if_npc_unavailable(true) && window.open(),
               "a conversation remains open while its NPC is available");
        window.set_closable(false);
        expect(window.close_if_npc_unavailable(false) && !window.open(),
               "leaving or losing an NPC closes even a dialog without a close control");
        expect(!window.close_if_npc_unavailable(false),
               "an already closed conversation produces no repeated close event");
        window.show_dialog(choices());
        window.set_closable(false);
        expect(!window.close_if_npc_unavailable(false) && window.open(),
               "local dialogs such as revival do not depend on a nearby NPC");
    }

    void verify_guild_executive(NpcTalkWindow& window) {
        DialogCatalog catalog;
        expect(catalog.load_dir(std::string(OPENKAL_REPO_ROOT) + "/config/dialogs"),
               "guild executive dialog loads without optional user data");
        const auto* menu = catalog.find(184);
        expect(menu && menu->links.size() == 3, "guild executive exposes implemented services");
        if (!menu) {
            return;
        }
        for (const float scale : {.75f, 1.0f, 1.5f}) {
            window.set_scale(scale);
            for (int i = 0; i < 3; ++i) {
                window.show_dialog(*menu, 210);
                window.center_on_screen(1920, 1080);
                const auto result = window.handle_click(window.x() + (window.w() - 58) * scale,
                                                        window.y() + (54 + i * 46) * scale);
                const NpcTalkClickResult expected[] = {NpcTalkClickResult::GuildCreate,
                                                       NpcTalkClickResult::Guild,
                                                       NpcTalkClickResult::Close};
                expect(result == expected[i] && !window.open(),
                       "executive service buttons dispatch instead of silently closing");
            }
        }
    }

    void verify_blacksmith(NpcTalkWindow& window, const UiDocument& doc) {
        DialogCatalog catalog;
        expect(catalog.load_dir(std::string(OPENKAL_REPO_ROOT) + "/config/dialogs"),
               "blacksmith dialogs load without optional user data");
        const auto* menu = catalog.find(243);
        expect(menu && menu->links.size() == 7, "blacksmith menu covers all choices");
        if (!menu) {
            return;
        }
        for (const float scale : {.75f, 1.0f, 1.5f}) {
            window.set_scale(scale);
            for (std::size_t i = 0; i < menu->links.size(); ++i) {
                window.show_dialog(*menu, 99);
                window.center_on_screen(1920, 1080);
                if (i >= 5) {
                    navigate(window, doc, "next");
                }
                const float y = i < 5 ? 54.0f + static_cast<float>(i) * 46.0f
                                      : 36.0f + static_cast<float>(i - 5) * 46.0f;
                const auto result = window.handle_click(window.x() + (window.w() - 58) * scale,
                                                        window.y() + y * scale);
                const auto& type = menu->links[i].type;
                if (type == "blacksmith") {
                    expect(result == NpcTalkClickResult::Blacksmith,
                           "Explosive Blow opens the implemented service");
                } else if (type == "close") {
                    expect(i + 1 == menu->links.size() && result == NpcTalkClickResult::Close,
                           "only End conversation closes the menu");
                } else {
                    expect(result == NpcTalkClickResult::OpenDialog,
                           "help and availability entries open a page instead of closing");
                    const auto* target = catalog.find(window.pending_dialog_id());
                    expect(target && !target->lines.empty(), "every numeric menu target exists");
                    if (target) {
                        bool returns = false;
                        for (const auto& link : target->links) {
                            returns = returns || link.type == "243";
                            expect(link.type == "close" || link.type == "blacksmith" ||
                                       (link.type == "243" && catalog.find(243)),
                                   "submenu has no dangling action");
                        }
                        expect(returns, "each instruction page offers a return to the menu");
                    }
                }
            }
        }
        DialogDef long_body;
        long_body.lines.assign(30, "A line of dialogue.");
        long_body.height = 200;
        long_body.links.push_back({"blacksmith", "Open service"});
        window.set_scale(1);
        window.show_dialog(long_body);
        expect(window.handle_click(window.x() + window.w() - 58, window.y() + 36) ==
                       NpcTalkClickResult::Consumed &&
                   window.open(),
               "off-page responses cannot be selected through body text");
        for (int i = 0; i < 8; ++i) {
            navigate(window, doc, "next");
        }
        expect(window.handle_click(window.x() + window.w() - 58, window.y() + 36) ==
                   NpcTalkClickResult::Blacksmith,
               "long explanations retain their final service response");
        DialogDef unknown;
        unknown.links.push_back({"unsupported_service", "Unavailable"});
        window.set_scale(1);
        window.show_dialog(unknown);
        expect(window.handle_click(window.x() + window.w() - 58, window.y() + 36) ==
                       NpcTalkClickResult::Consumed &&
                   window.open(),
               "unknown external actions do not silently end conversations");
    }

    void capture(NpcTalkWindow& window, const UiDocument& doc, const std::string& prefix) {
        std::unique_ptr<openkal::engine::IEngine> engine(openkal::engine::create_engine());
        const bool initialized = engine->init("NPC conversation verification", 1100, 900);
        expect(initialized, "initialize NPC preview");
        if (!initialized) {
            return;
        }
        WidgetTools ui;
        ui.init(*engine, OPENKAL_REPO_ROOT, OPENKAL_TEST_RUNTIME_ROOT);
        FreetypeFont font;
        expect(font.load(*engine, FreetypeFont::find_system_sans(), 13), "load NPC FreeType font");
        window.sync_fonts(*engine);
        DialogCatalog catalog;
        (void) catalog.load_dir(std::string(OPENKAL_REPO_ROOT) + "/config/dialogs");
        for (const float scale : {1.0f, 1.5f}) {
            window.set_scale(scale);
            for (const int id : {184, 243, 244, 990243, 990244, 990246, 990320}) {
                const auto* dialog = catalog.find(id);
                if (!dialog) {
                    continue;
                }
                window.show_dialog(*dialog, 77);
                window.center_on_screen(1100, 900);
                std::vector<std::uint8_t> previous;
                for (int page = 0; page < 10; ++page) {
                    expect(engine->begin_offscreen(1100, 900, .12f, .14f, .16f),
                           "begin NPC capture");
                    window.draw(*engine, ui, font);
                    std::vector<std::uint8_t> rgba;
                    expect(engine->end_offscreen_rgba(rgba), "read NPC capture");
                    if (rgba == previous) {
                        break;
                    }
                    previous = rgba;
                    std::ofstream out(prefix + "-" + std::to_string(id) + "-" +
                                          (scale == 1 ? "native-" : "scaled-") +
                                          std::to_string(page) + ".ppm",
                                      std::ios::binary);
                    out << "P6\n1100 900\n255\n";
                    for (std::size_t i = 0; i + 3 < rgba.size(); i += 4) {
                        out.write(reinterpret_cast<const char*>(rgba.data() + i), 3);
                    }
                    expect(out.good(), "save NPC preview");
                    navigate(window, doc, "next");
                }
            }
        }
        // Exercise the same short response after measured FreeType layout and a drag.
        window.show_dialog(choices(), 77);
        expect(engine->begin_offscreen(1100, 900, 0, 0, 0), "begin measured response verification");
        window.draw(*engine, ui, font);
        std::vector<std::uint8_t> pixels;
        (void) engine->end_offscreen_rgba(pixels);
        navigate(window, doc, "next");
        const float s = window.scale();
        (void) window.handle_click(window.x() + 160 * s, window.y() - 40 * s);
        openkal::engine::FrameInput drag;
        drag.mouse_left = true;
        drag.mouse_x = window.x() + 180 * s;
        drag.mouse_y = window.y() - 10 * s;
        window.update_pointer(drag);
        expect(window.handle_click(window.x() + (window.w() - 58) * s, window.y() + 36 * s) ==
                       NpcTalkClickResult::OpenDialog &&
                   window.pending_dialog_id() == 1002,
               "measured layout stays clickable before redrawing a moved window");
        window.destroy_fonts(*engine);
        font.destroy(*engine);
        ui.destroy();
        engine->shutdown();
    }
}  // namespace

int main(int argc, char** argv) {
    UiLayout layout;
    expect(layout.load_file(std::string(OPENKAL_REPO_ROOT) + "/config/ui/npc_talk.json"),
           "load NPC document");
    UiDocument doc;
    expect(doc.load(layout.root()), "parse NPC document");
    NpcTalkWindow window;
    window.set_layout(&layout);
    verify_pages(window, doc);
    verify_npc_lifetime(window);
    verify_blacksmith(window, doc);
    verify_guild_executive(window);
    DialogCatalog collection_dialogs;
    expect(collection_dialogs.load_dir(std::string(OPENKAL_REPO_ROOT) + "/config/dialogs"),
           "load collection dialogue");
    if (const auto* dialog = collection_dialogs.find(990320)) {
        window.set_scale(1);
        window.show_dialog(*dialog, 77);
        expect(window.handle_click(window.x() + window.w() - 58, window.y() + 54) ==
                       NpcTalkClickResult::TradingPostCollection &&
                   window.npc_id() == 77 && !window.open(),
               "Trading Post Agent opens collection using the actual NPC entity");
    } else {
        expect(false, "collection dialogue exists");
    }
    if (argc == 3 && std::string(argv[1]) == "--capture") {
        capture(window, doc, argv[2]);
    }
    return openkal::test::g_failures ? 1 : 0;
}
