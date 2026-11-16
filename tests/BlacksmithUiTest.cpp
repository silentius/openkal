/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/BlacksmithWindow.h"
#include "openkal/server/PrefixEnchant.h"
#include "test_util.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <unistd.h>
#include <utility>

namespace {
    using namespace openkal::client;
    using openkal::test::expect;

    std::pair<float, float>
    point(const BlacksmithWindow& window, const UiDocument& doc, const char* id) {
        const auto* widget = doc.find_widget(id);
        expect(widget != nullptr, "blacksmith control exists");
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

    Bag sample_bag() {
        Bag bag;
        ItemStack weapon;
        weapon.index = 91001;
        weapon.iid = 101;
        weapon.count = 1;
        weapon.upgrade_level = 3;
        weapon.x_attack = 5;
        bag.normal.push_back(weapon);
        ItemStack stones;
        stones.index = openkal::server::kRevisionStoneIndex;
        stones.iid = 102;
        stones.count = 100;
        bag.normal.push_back(stones);
        bag.money = 12000;
        return bag;
    }

    void verify_controls(BlacksmithWindow& window, const UiDocument& doc, const ItemDb& items) {
        for (const float scale : {.75f, 1.0f, 1.5f}) {
            window.set_scale(scale);
            window.set_open(true);
            window.center_on_screen(1920, 1080);
            Bag bag = sample_bag();
            auto click = [&](const char* id) {
                const auto [x, y] = point(window, doc, id);
                return window.handle_click(x, y, bag, items);
            };
            expect(click("revise") == BlacksmithClickResult::Consumed &&
                       click("explosive_blow") == BlacksmithClickResult::Consumed,
                   "empty weapon slot disables both actions");
            const auto [sx, sy] = point(window, doc, "weapon");
            expect(!window.try_drop_weapon(
                       bag.normal[0], items, window.x() + 300 * scale, window.y() + 170 * scale),
                   "dropping elsewhere in the dialog does not select a weapon");
            for (const int index : {91002, 91003}) {
                auto invalid = bag.normal[0];
                invalid.index = index;
                expect(!window.try_drop_weapon(invalid, items, sx, sy),
                       "slot rejects armor and weapons below grade 40");
            }
            auto equipped = bag.normal[0];
            equipped.equipped = true;
            expect(!window.try_drop_weapon(equipped, items, sx, sy),
                   "slot rejects equipped weapon");
            expect(window.try_drop_weapon(bag.normal[0], items, sx - 18 * scale, sy),
                   "framed slot accepts a valid weapon at its outer edge");
            expect(window.weapon_iid() == 101 && click("revise") == BlacksmithClickResult::Revise &&
                       click("explosive_blow") == BlacksmithClickResult::ExplosiveBlow,
                   "valid actions preserve the selected weapon instance");
            expect(bag.normal[0].upgrade_level == 3 && bag.normal[0].upgrade_rate == 0 &&
                       bag.normal[1].count == 100 && bag.money == 12000,
                   "UI requests do not optimistically consume resources or upgrade the weapon");
            bag.normal[1].count = 0;
            expect(click("revise") == BlacksmithClickResult::Consumed,
                   "live stone removal disables Revise before the next draw");
            bag.normal[1].count = 100;
            bag.normal[0].upgrade_rate = 100;
            expect(click("revise") == BlacksmithClickResult::Consumed,
                   "full success chance disables further revision");
            bag.normal[0].upgrade_rate = 0;
            bag.money = 0;
            expect(click("explosive_blow") == BlacksmithClickResult::Consumed &&
                       click("revise") == BlacksmithClickResult::Revise,
                   "Geon shortage affects the upgrade without blocking an affordable revision");
            bag.money = 12000;
            bag.normal[0].upgrade_level = 5;
            expect(click("explosive_blow") == BlacksmithClickResult::Consumed,
                   "weapon cap disables further upgrades");
            bag.normal[0].upgrade_level = 3;
            bag.normal[0].equipped = true;
            expect(click("explosive_blow") == BlacksmithClickResult::Consumed &&
                       click("revise") == BlacksmithClickResult::Consumed,
                   "equipping the selected weapon invalidates both actions immediately");
            bag.normal[0].equipped = false;
            bag.normal[0].count = 0;
            expect(click("explosive_blow") == BlacksmithClickResult::Consumed,
                   "removed selected instance cannot trigger a stale action");
            bag.normal[0].count = 1;
            const float old_x = window.x(), old_y = window.y();
            (void) window.handle_click(old_x + 160 * scale, old_y - 40 * scale, bag, items);
            openkal::engine::FrameInput input;
            input.mouse_left = true;
            input.mouse_x = old_x + 190 * scale;
            input.mouse_y = old_y - 20 * scale;
            window.update_pointer(input);
            expect(std::abs(window.x() - old_x - 30 * scale) < .01f &&
                       std::abs(window.y() - old_y - 20 * scale) < .01f,
                   "Dragon title dragging follows the scaled pointer");
            input.mouse_left = false;
            window.update_pointer(input);
            expect(click("explosive_blow") == BlacksmithClickResult::ExplosiveBlow,
                   "document action hitboxes follow the moved window");
            (void) click("weapon");
            expect(window.weapon_iid() == 0 && click("revise") == BlacksmithClickResult::Consumed,
                   "clicking the weapon slot removes the selection");
            const auto [nx, ny] = point(window, doc, "weapon");
            expect(window.try_drop_weapon(bag.normal[0], items, nx, ny),
                   "weapon can be selected again after dragging");
            expect(click("close") == BlacksmithClickResult::Close && !window.open() &&
                       window.weapon_iid() == 0,
                   "anchored Dragon footer closes and clears the selection at every scale");
        }
    }

    void capture(BlacksmithWindow& window,
                 const UiDocument& doc,
                 const ItemDb& items,
                 const std::string& prefix) {
        std::unique_ptr<openkal::engine::IEngine> engine(openkal::engine::create_engine());
        const bool ready = engine->init("Explosive Blow UI verification", 1100, 1000);
        expect(ready, "initialize blacksmith preview");
        if (!ready) {
            return;
        }
        WidgetTools ui;
        ui.init(*engine, OPENKAL_REPO_ROOT, OPENKAL_TEST_RUNTIME_ROOT);
        FreetypeFont font;
        expect(font.load(*engine, FreetypeFont::find_system_sans(), 18),
               "load caller-sized body font for document scaling");
        for (const auto& widget : doc.widgets()) {
            const auto file = widget.file().empty() ? widget.file_default() : widget.file();
            if (!file.empty()) {
                expect(ui.tex_info(file).width() > 0, "Dragon artwork resolves");
            }
        }
        for (const float scale : {.75f, 1.0f, 1.5f}) {
            std::vector<std::uint8_t> short_name;
            for (int state = 0; state < 4; ++state) {
                window.set_scale(scale);
                window.set_open(false);
                window.set_open(true);
                window.center_on_screen(1100, 1000);
                window.sync_fonts(*engine);
                Bag bag = sample_bag();
                if (state > 0) {
                    const auto [x, y] = point(window, doc, "weapon");
                    expect(window.try_drop_weapon(bag.normal[0], items, x, y),
                           "preview selects weapon");
                }
                if (state == 2) {
                    bag.money = 0;
                    bag.normal[1].count = 0;
                }
                if (state == 3) {
                    bag.normal[0].index = 91004;
                }
                std::vector<std::uint8_t> previous;
                for (int pointer_state = 0; pointer_state < 3; ++pointer_state) {
                    const auto [x, y] = point(window, doc, state == 0 ? "close" : "explosive_blow");
                    openkal::engine::FrameInput pointer;
                    if (pointer_state > 0) {
                        pointer.mouse_x = x;
                        pointer.mouse_y = y;
                    }
                    pointer.mouse_left = pointer_state == 2;
                    window.update_pointer(pointer);
                    expect(engine->begin_offscreen(1100, 1000, .12f, .14f, .16f),
                           "begin blacksmith capture");
                    window.draw(*engine, ui, font, bag, items);
                    std::vector<std::uint8_t> rgba;
                    expect(engine->end_offscreen_rgba(rgba), "read blacksmith capture");
                    if (pointer_state > 0 && state != 2) {
                        expect(rgba != previous,
                               "enabled Dragon button renders hover and pressed states");
                    }
                    previous = rgba;
                    if (pointer_state == 0 && state == 1) {
                        short_name = rgba;
                    }
                    if (pointer_state == 0 && state == 3 && short_name.size() == rgba.size()) {
                        const auto* name = doc.find_widget("weapon_name");
                        float nx, ny, nw, nh;
                        UiView::widget_rect(*name,
                                            nullptr,
                                            window.x(),
                                            window.y(),
                                            window.w() * scale,
                                            window.h() * scale,
                                            scale,
                                            nx,
                                            ny,
                                            nw,
                                            nh);
                        bool contained = true;
                        for (int y = 0; y < 1000; ++y) {
                            for (int x = 0; x < 1100; ++x) {
                                if (x >= std::floor(nx) && x < std::ceil(nx + nw) &&
                                    y >= std::floor(ny) && y < std::ceil(ny + nh)) {
                                    continue;
                                }
                                const auto pixel = static_cast<std::size_t>(y * 1100 + x) * 4;
                                for (std::size_t c = 0; c < 4; ++c) {
                                    contained =
                                        contained && rgba[pixel + c] == short_name[pixel + c];
                                }
                            }
                        }
                        expect(contained,
                               "long weapon names change pixels only inside their text box");
                    }
                    std::ofstream out(prefix + "-" + std::to_string(static_cast<int>(scale * 100)) +
                                          "-" + std::to_string(state) + "-" +
                                          std::to_string(pointer_state) + ".ppm",
                                      std::ios::binary);
                    out << "P6\n1100 1000\n255\n";
                    for (std::size_t i = 0; i + 3 < rgba.size(); i += 4) {
                        out.write(reinterpret_cast<const char*>(rgba.data() + i), 3);
                    }
                    expect(out.good(), "save blacksmith preview");
                }
            }
        }
        window.destroy_fonts(*engine);
        font.destroy(*engine);
        ui.destroy();
        engine->shutdown();
    }
}  // namespace

int main(int argc, char** argv) {
    const auto root = std::filesystem::temp_directory_path() /
                      ("openkal_blacksmith_" + std::to_string(::getpid()));
    std::filesystem::create_directories(root);
    const char* definitions[] = {
        R"({"index":91001,"name":"Training Sword","image":"revisionHammer","class":["weapon","sword"],"wear":1,"level":50})",
        R"({"index":91002,"name":"Training Armor","class":["defense","upperarmor"],"wear":1,"level":50})",
        R"({"index":91003,"name":"Low Grade Sword","class":["weapon","sword"],"wear":1,"level":39})",
        R"({"index":91004,"name":"Training Sword With An Intentionally Long Display Name That Must Fit The Weapon Panel","image":"revisionHammer","class":["weapon","sword"],"wear":1,"level":50})"};
    for (int i = 0; i < 4; ++i) {
        std::ofstream out(root / (std::to_string(i) + ".json"));
        out << definitions[i];
    }
    ItemDb items;
    expect(items.load(std::string(OPENKAL_REPO_ROOT) + "/config",
                      OPENKAL_TEST_RUNTIME_ROOT,
                      root.string()),
           "load project-owned synthetic weapons without optional data");
    UiLayout layout;
    expect(layout.load_file(std::string(OPENKAL_REPO_ROOT) + "/config/ui/blacksmith.json"),
           "load Dragon blacksmith layout");
    UiDocument doc;
    expect(doc.load(layout.root()), "parse Dragon blacksmith document");
    BlacksmithWindow window;
    window.set_layout(&layout);
    verify_controls(window, doc, items);
    if (argc == 3 && std::string(argv[1]) == "--capture") {
        capture(window, doc, items, argv[2]);
    }
    std::filesystem::remove_all(root);
    return openkal::test::g_failures ? 1 : 0;
}
