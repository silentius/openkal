/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/MixWindow.h"
#include "openkal/client/NpcTalkWindow.h"
#include "test_util.h"

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <utility>

namespace {
    using namespace openkal::client;
    using openkal::test::expect;

    std::pair<float, float> point(const MixWindow& window, const UiDocument& doc, const char* id) {
        const auto* widget = doc.find_widget(id);
        expect(widget != nullptr, "mix control exists");
        if (!widget) {
            return {};
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
        return {x + w / 2, y + h / 2};
    }

    void fill_recipe(MixWindow& window, const UiDocument& doc, const ItemDb& items, Bag& bag) {
        const int indices[] = {244, 245, 251, 895, 895};
        const char* slots[] = {"mix.center", "mix.north", "mix.east", "mix.south", "mix.west"};
        bag.normal.clear();
        for (int i = 0; i < MixWindow::kSlotCount; ++i) {
            ItemStack stack;
            stack.iid = static_cast<std::uint32_t>(i + 1);
            stack.index = indices[i];
            stack.count = 1;
            bag.normal.push_back(stack);
            const auto [x, y] = point(window, doc, slots[i]);
            expect(window.try_drop_item(stack, items, x, y), "drop recipe item at scaled slot");
        }
    }

    void verify_material_lists(MixWindow& window, const UiDocument& doc, const ItemDb& items) {
        DialogCatalog dialogs;
        expect(dialogs.load_dir(std::string(OPENKAL_TEST_RESOURCE_ROOT) + "/config/dialogs"),
               "load effective NPC dialogs including local overrides");
        const auto* dialog = dialogs.find(277);
        expect(dialog && dialog->links.size() == 5, "mixing NPC has four services and exit");
        UiLayout npc_layout;
        expect(npc_layout.load_file(std::string(OPENKAL_REPO_ROOT) + "/config/ui/npc_talk.json"),
               "load NPC layout");
        NpcTalkWindow npc;
        npc.set_layout(&npc_layout);
        for (float scale : {.75f, 1.0f, 1.5f}) {
            npc.set_scale(scale);
            if (dialog) {
                for (int i = 0; i < 4; ++i) {
                    DialogDef choices = *dialog;
                    choices.lines.clear();
                    for (auto& link : choices.links) {
                        link.text = "Service";
                    }
                    npc.show_dialog(choices, 77);
                    npc.center_on_screen(1920, 1080);
                    const float x = npc.x() + (npc.w() - 58) * scale;
                    const float y = npc.y() + (16 + i * 46 + 20) * scale;
                    expect(npc.handle_click(x, y) == (i < 2 ? NpcTalkClickResult::Mixing
                                                            : NpcTalkClickResult::MixingList),
                           "NPC mixing actions and material-list actions route separately");
                    expect(npc.pending_mixing_mode() == i % 2, "NPC preserves Upgrade/Create mode");
                }
            }
            window.set_scale(scale);
            window.show_materials(0);
            window.center_on_screen(1920, 1080);
            expect(window.showing_materials() && window.mode() == 0 && window.material_page() == 0,
                   "upgrade material list opens on its first page");
            auto activate = [&](const char* id) {
                const auto [x, y] = point(window, doc, id);
                return window.handle_click(x, y, items);
            };
            (void) activate("previous");
            expect(window.material_page() == 0, "previous cannot underflow the first page");
            (void) activate("next");
            expect(window.material_page() == 1, "upgrade next page contains Grade 3 recipes");
            (void) activate("next");
            expect(window.material_page() == 1, "next cannot exceed the last page");
            (void) activate("create");
            expect(window.mode() == 1 && window.material_page() == 0,
                   "Create Weapon selects its own material table and resets paging");
            (void) activate("next");
            expect(window.material_page() == 0, "creation fits one page");
            ItemStack stack;
            stack.iid = 99;
            stack.index = 244;
            stack.count = 1;
            expect(!window.try_drop_item(
                       stack, items, window.x() + 205 * scale, window.y() + 215 * scale),
                   "read-only material list rejects inventory drops");
            expect(window.handle_release(window.x() + 150 * scale,
                                         window.y() + 460 * scale,
                                         items) != MixClickResult::MakeMix,
                   "material-list clicks cannot craft items");
            expect(activate("exit_bottom") == MixClickResult::Close && !window.open(),
                   "material list closes through its scaled Dragon footer");
            window.show_materials(1);
            window.set_open(true, 0);
            expect(!window.showing_materials() && window.w() == 440 * scale,
                   "opening the real mixing service restores its slot document and geometry");
            window.set_open(false);
        }
    }

    void capture(MixWindow& window,
                 const UiDocument& doc,
                 const UiDocument& list_doc,
                 const ItemDb& items,
                 const std::string& prefix) {
        auto engine = std::unique_ptr<openkal::engine::IEngine>(openkal::engine::create_engine());
        if (!engine->init("Dragon mixing verification", 1000, 1100)) {
            expect(false, "initialize mixing preview");
            return;
        }
        WidgetTools ui;
        const char* data = std::getenv("OPENKAL_TEST_DATA_ROOT");
        ui.init(*engine, data ? data : OPENKAL_REPO_ROOT, OPENKAL_TEST_RUNTIME_ROOT);
        FreetypeFont font;
        expect(font.load(*engine, FreetypeFont::find_system_sans(), 13), "load mixing font");
        for (const auto& widget : doc.widgets()) {
            const auto& file = widget.file().empty() ? widget.file_default() : widget.file();
            if (!file.empty()) {
                expect(ui.tex_info(file).width() > 0, "mix artwork loads");
            }
        }
        for (float scale : {1.0f, 1.5f}) {
            window.set_scale(scale);
            window.center_on_screen(1000, 1100);
            for (bool filled : {false, true}) {
                window.set_open(false);
                window.set_open(true, filled ? 1 : 0);
                Bag bag;
                if (filled) {
                    fill_recipe(window, doc, items, bag);
                }
                std::vector<std::uint8_t> previous;
                for (int state = 0; state < 3; ++state) {
                    openkal::engine::FrameInput pointer;
                    const auto [x, y] = point(window, doc, filled ? "mix_start" : "exit_bottom");
                    if (state > 0) {
                        pointer.mouse_x = x;
                        pointer.mouse_y = y;
                    }
                    pointer.mouse_left = state == 2;
                    window.update_pointer(pointer);
                    if (filled && state == 2) {
                        (void) window.handle_click(x, y, items);
                    }
                    expect(engine->begin_offscreen(1000, 1100, .12f, .14f, .13f),
                           "begin mix capture");
                    window.draw(*engine, ui, font, bag, items);
                    std::vector<std::uint8_t> rgba;
                    expect(engine->end_offscreen_rgba(rgba), "read mix capture");
                    if (state == 1 || (filled && state == 2)) {
                        expect(rgba != previous, "Dragon button state changes the rendered image");
                    }
                    previous = rgba;
                    std::ofstream out(prefix + (filled ? "-ready" : "-empty") +
                                          (scale == 1 ? "-native-" : "-scaled-") +
                                          std::to_string(state) + ".ppm",
                                      std::ios::binary);
                    out << "P6\n1000 1100\n255\n";
                    for (std::size_t i = 0; i + 3 < rgba.size(); i += 4) {
                        out.write(reinterpret_cast<const char*>(rgba.data() + i), 3);
                    }
                    expect(out.good(), "save mix preview");
                    (void) window.handle_release(x, y, items);
                }
            }
        }
        for (float scale : {1.0f, 1.5f}) {
            for (int page = 0; page < 3; ++page) {
                window.set_scale(scale);
                window.show_materials(page == 2 ? 1 : 0);
                window.center_on_screen(1280, 1100);
                if (page == 1) {
                    const auto [x, y] = point(window, list_doc, "next");
                    (void) window.handle_click(x, y, items);
                }
                expect(engine->begin_offscreen(1280, 1100, .12f, .14f, .13f),
                       "begin material capture");
                window.draw(*engine, ui, font, Bag{}, items);
                std::vector<std::uint8_t> rgba;
                expect(engine->end_offscreen_rgba(rgba), "read material capture");
                std::ofstream out(prefix + "-list-" + std::to_string(page) +
                                      (scale == 1 ? "-native.ppm" : "-scaled.ppm"),
                                  std::ios::binary);
                out << "P6\n1280 1100\n255\n";
                for (std::size_t i = 0; i + 3 < rgba.size(); i += 4) {
                    out.write(reinterpret_cast<const char*>(rgba.data() + i), 3);
                }
                expect(out.good(), "save material-list preview");
            }
        }
        font.destroy(*engine);
        ui.destroy();
        engine->shutdown();
    }
}  // namespace

int main(int argc, char** argv) {
    UiLayout layout;
    expect(layout.load_file(std::string(OPENKAL_REPO_ROOT) + "/config/ui/mix_master_window.json"),
           "load Dragon mixing layout");
    UiDocument doc;
    expect(doc.load(layout.root()), "parse mixing document");
    ItemDb items;
    expect(items.load(std::string(OPENKAL_TEST_RESOURCE_ROOT) + "/config", OPENKAL_REPO_ROOT),
           "load mixing items");
    openkal::server::MixCatalog recipes;
    expect(recipes.load_dir(std::string(OPENKAL_REPO_ROOT) + "/config/mixing"), "load recipes");
    MixWindow window;
    window.set_layout(&layout);
    window.set_mix_catalog(&recipes);
    for (float scale : {.75f, 1.0f, 1.5f}) {
        window.set_scale(scale);
        window.center_on_screen(1920, 1080);
        window.set_open(true, 1);
        auto [x, y] = point(window, doc, "mix_start");
        (void) window.handle_click(x, y, items);
        expect(window.handle_release(x, y, items) != MixClickResult::MakeMix,
               "empty recipe cannot start");
        Bag bag;
        fill_recipe(window, doc, items, bag);
        expect(window.weapon_iid() == 1 && window.extra_weapon_iid() == 2 &&
                   window.armor_iid() == 3 && window.stone1_iid() != 0 && window.stone2_iid() != 0,
               "Dragon slots preserve classified recipe instance IDs");
        (void) window.handle_click(x, y, items);
        expect(window.handle_release(x - 200 * scale, y, items) != MixClickResult::MakeMix,
               "releasing outside the start button cancels");
        (void) window.handle_click(x, y, items);
        expect(window.handle_release(x, y, items) == MixClickResult::MakeMix,
               "valid recipe starts on release at each scale");
        const auto [sx, sy] = point(window, doc, "mix.south");
        (void) window.handle_click(sx, sy, items);
        (void) window.handle_click(x, y, items);
        expect(window.handle_release(x, y, items) != MixClickResult::MakeMix,
               "removing an ingredient disables mixing");
        const float old_x = window.x(), old_y = window.y();
        (void) window.handle_click(old_x + 160 * scale, old_y - 40 * scale, items);
        openkal::engine::FrameInput pointer;
        pointer.mouse_left = true;
        pointer.mouse_x = old_x + 180 * scale;
        pointer.mouse_y = old_y - 10 * scale;
        window.update_pointer(pointer);
        expect(std::abs(window.x() - old_x - 20 * scale) < .01f &&
                   std::abs(window.y() - old_y - 30 * scale) < .01f,
               "Dragon title drag follows the scaled pointer");
        pointer.mouse_left = false;
        window.update_pointer(pointer);
        const auto [cx, cy] = point(window, doc, "exit_bottom");
        expect(window.contains(cx, cy), "anchored footer belongs to window bounds");
        expect(window.handle_click(cx, cy, items) == MixClickResult::Close && !window.open(),
               "anchored Dragon footer closes at each scale");
        expect(window.weapon_iid() == 0, "closing clears selected recipe");
    }
    UiLayout list_layout;
    expect(
        list_layout.load_file(std::string(OPENKAL_REPO_ROOT) + "/config/ui/mixing_item_list.json"),
        "load material-list layout");
    UiDocument list_doc;
    expect(list_doc.load(list_layout.root()), "parse material-list layout");
    window.set_material_layout(&list_layout);
    verify_material_lists(window, list_doc, items);
    if (argc == 3 && std::string(argv[1]) == "--capture") {
        capture(window, doc, list_doc, items, argv[2]);
    }
    return openkal::test::g_failures ? 1 : 0;
}
