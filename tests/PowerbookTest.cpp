/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "fixture_util.h"
#include "openkal/client/ClientInput.h"
#include "openkal/client/ItemDb.h"
#include "openkal/client/ItemTooltip.h"
#include "openkal/client/PowerbookWindow.h"
#include "openkal/client/UiLayout.h"
#include "openkal/server/world/ItemGroupDb.h"
#include "test_util.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <unistd.h>

namespace {
    using namespace openkal;
    using test::expect;

    void write_text(const std::string& path, const std::string& text) {
        std::filesystem::create_directories(std::filesystem::path(path).parent_path());
        std::ofstream(path) << text;
    }

    void fixtures(const std::string& root) {
        std::filesystem::create_directories(root + "/groups");
        std::filesystem::create_directories(root + "/itemgroups");
        server::GroupDef regular;
        regular.index = 1;
        for (int i = 0; i < 74; ++i) {
            server::GroupEntry entry;
            entry.item_index = 100 + i;
            entry.chance = 1;
            regular.entries.push_back(entry);
            write_text(root + "/items/" + std::to_string(100 + i) + ".json",
                       "{\"index\":" + std::to_string(100 + i) + ",\"name\":\"Practice item " +
                           std::to_string(i + 1) + "\",\"image\":\"practice\"}");
        }
        write_text(
            root + "/items/101.json",
            R"({"index":101,"name":"Training Talisman","image":"practice","class":["general", "etc"],"limit":{"level":20},"description":"Adds +5 Attack to a training weapon.#nA synthetic talisman with a deliberately long explanation so the Powerbook hover overlay must wrap the entire description without dropping its effect, changing its colors, or running beyond the edge of a small viewport."})");
        // A project-owned 24-bit BMP: gold diamond on a dark blue background.
        std::vector<std::uint8_t> bmp(54 + 32 * 32 * 3, 0);
        const auto put = [&](int offset, std::uint32_t value) {
            for (int byte = 0; byte < 4; ++byte) {
                bmp[static_cast<std::size_t>(offset + byte)] =
                    static_cast<std::uint8_t>(value >> (byte * 8));
            }
        };
        bmp[0] = 'B';
        bmp[1] = 'M';
        put(2, static_cast<std::uint32_t>(bmp.size()));
        put(10, 54);
        put(14, 40);
        put(18, 32);
        put(22, 32);
        bmp[26] = 1;
        bmp[28] = 24;
        for (int y = 0; y < 32; ++y) {
            for (int x = 0; x < 32; ++x) {
                const bool gold = std::abs(x - 16) + std::abs(y - 16) < 13;
                const auto offset = static_cast<std::size_t>(54 + (y * 32 + x) * 3);
                bmp[offset] = gold ? 48 : 42;
                bmp[offset + 1] = gold ? 188 : 30;
                bmp[offset + 2] = gold ? 238 : 20;
            }
        }
        expect(test::write_bytes(root + "/HyperText/practice.bmp", bmp), "write owned item icon");
        regular.entries.push_back({server::GroupEntry::Kind::Money, 10, 0, 0, 1, 50});
        regular.entries.push_back({server::GroupEntry::Kind::Item, 16, 100, 9, 1, 0});
        // Beyond the picker's full cumulative range; this cannot drop.
        regular.entries.push_back({server::GroupEntry::Kind::Item, 100, 999, 0, 1, 0});
        expect(server::ItemGroupDb::save_group(root + "/groups", regular),
               "write owned regular fixture");
        server::GroupDef quest{2,
                               "",
                               {{server::GroupEntry::Kind::Item, 50, 100, 0, 1, 0},
                                {server::GroupEntry::Kind::Item, 50, 800, 0, 1, 0}}};
        expect(server::ItemGroupDb::save_group(root + "/groups", quest),
               "write owned quest fixture");
        server::GroupDef global{3, "", {{server::GroupEntry::Kind::Item, 100, 900, 0, 1, 0}}};
        expect(server::ItemGroupDb::save_group(root + "/groups", global),
               "write owned global fixture");
        for (const auto& group :
             {server::ItemGroupDef{1, "", {{100, 1}, {100, 9999}}},
              server::ItemGroupDef{2, "", {{100, 2}}},
              server::ItemGroupDef{server::kGlobalNormalMonsterItemGroupIndex, "", {{100, 3}}}}) {
            expect(server::ItemGroupDb::save_itemgroup(root + "/itemgroups", group),
                   "write owned outer fixture");
        }
        for (int i = 31; i >= 1; --i) {
            const int level = std::min(i, 30);
            const auto name = i == 1    ? "Training Beast"
                              : i >= 30 ? "Twin Beast"
                                        : "Practice Monster " + std::to_string(i);
            write_text(root + "/monsters/" + std::to_string(i) + ".json",
                       "{\"index\":" + std::to_string(i) + ",\"name\":\"" + name +
                           "\",\"stats\":{\"level\":" + std::to_string(level) +
                           ",\"race\":0,\"itemGroups\":[[1, 2], [2, 0]],\"quests\":[1, 0, 2, 1]},"
                           "\"spawns\":[{\"area\":1,\"map\":0,\"max\":2,\"rect\":[8100, 8120, "
                           "8000, 8000]},"
                           "{\"area\":2,\"map\":0,\"max\":1,\"rect\":[8140, 8020, 8170, 8060]},"
                           "{\"area\":3,\"map\":0,\"max\":1,\"rect\":[520, 520, 550, 550]},"
                           "{\"area\":4,\"max\":0,\"rect\":[0, 0, 1, 1]}]}");
        }
        write_text(
            root + "/monsters/missing.json",
            R"({"index":98,"name":"Missing tables","stats":{"level":98,"race":9,"itemGroups":[[99, 1]]}})");
        write_text(root + "/monsters/static.json",
                   R"({"index":99,"name":"Static prop","stats":{"level":99,"race":9}})");
        write_text(root + "/monsters/invalid.json", "not json");
        write_text(
            root + "/mapinfo.json",
            R"({"maps":[{"index":7,"kind":2,"xy":[2, 2],"sizeX":2,"sizeY":2,"mapname":"Map\\practice"}]})");
        write_text(
            root + "/ui/dungeon_minimaps.json",
            R"({"maps":[{"mapname":"practice","sizeX":512,"sizeY":512,"tileWidth":256,"tileHeight":256,"mapLeft":0,"mapBottom":0,"correction":0}]})");
    }

    void catalog_checks(const std::string& root) {
        client::PowerbookCatalog catalog;
        expect(catalog.load(root), "read all synthetic monster definitions");
        const auto& monsters = catalog.monsters();
        expect(monsters.size() == 33 && monsters.front().index == 1,
               "include monsters without spawns; skip malformed input");
        expect(monsters[29].index == 30 && monsters[30].index == 31,
               "level ties have deterministic name and ID order");
        const auto& beast = monsters.front();
        expect(beast.quest_drops.size() == 2 && beast.quest_drops[0].quest == 1 &&
                   beast.quest_drops[0].flag == 0 && beast.quest_drops[1].item_index == 800,
               "preserve quest and flag association independently of ordinary drops");
        expect(beast.drops.size() == 77 && beast.drops_complete,
               "merge normal, global and quest drops; deduplicate prefix variants");
        const auto find_drop = [&](int id) {
            return std::find_if(beast.drops.begin(), beast.drops.end(), [id](const auto& drop) {
                return drop.item_index == id;
            });
        };
        expect(find_drop(31) != beast.drops.end() && find_drop(900) != beast.drops.end(),
               "include money and effective global drops");
        expect(find_drop(800) != beast.drops.end() && find_drop(800)->quest_only &&
                   !find_drop(100)->quest_only,
               "quest-only marking does not override an ordinary drop");
        expect(find_drop(999) == beast.drops.end(), "exclude unreachable cumulative entries");
        expect(beast.spawns.size() == 3 && beast.spawns[0].x0 == 8000 * 32 &&
                   beast.spawns[0].y1 == 8120 * 32 && beast.spawns[2].map == 7,
               "normalize spawn rectangles, skip disabled spawns, align indoor map IDs");
        expect(!monsters[31].drops_complete && monsters[32].drops.empty(),
               "missing references reported; static props do not gain global combat loot");
        expect(!catalog.load(root + "/absent") && catalog.monsters().empty(),
               "reload clears stale entries when optional data is absent");
    }

    void ui_checks(const std::string& root, const char* capture) {
        for (const float scale : {.5f, 1.0f, 2.0f}) {
            float x = 0, y = 0;
            client::ItemTooltip::place_near_cursor(100 * scale,
                                                   100 * scale,
                                                   200 * scale,
                                                   80 * scale,
                                                   800 * scale,
                                                   600 * scale,
                                                   x,
                                                   y,
                                                   scale);
            expect(x == 106 * scale && y == 120 * scale,
                   "tooltip pointer clearance scales with the UI");
            client::ItemTooltip::place_near_cursor(790 * scale,
                                                   590 * scale,
                                                   200 * scale,
                                                   80 * scale,
                                                   800 * scale,
                                                   600 * scale,
                                                   x,
                                                   y,
                                                   scale);
            expect(x == 584 * scale && y == 504 * scale,
                   "scaled tooltip flips above and left at the viewport edges");
        }
        client::UiLayout layout;
        expect(layout.load_file(std::string(OPENKAL_REPO_ROOT) + "/config/ui/powerbook.json"),
               "load universal Powerbook document");
        client::ItemDb items;
        expect(items.load(root, root, root + "/items"), "load owned item labels");
        client::PowerbookWindow book;
        client::WidgetTools ui;
        book.set_layout(&layout);
        book.open(root, items);
        auto vars = book.variables();
        expect(book.is_open() && vars["powerbook.monster.0.name"] == "Training Beast" &&
                   vars["powerbook.monster.0.level"] == "1",
               "open selects the lowest-level monster");
        expect(vars["powerbook.drop_status"] == "77 possible items" &&
                   vars["powerbook.drop.1.name"] == "Practice item 1",
               "show possible item labels without chances");
        engine::FrameInput input;
        input.width = 1600;
        input.height = 900;
        input.mouse_x = 100;
        input.mouse_y = 250;
        input.mouse_scroll = -1;
        book.update(input, ui);
        expect(book.variables()["powerbook.monster.0.level"] == "4", "wheel scrolls monster rows");
        input.mouse_x = 450;
        book.update(input, ui);
        expect(book.variables()["powerbook.monster.0.level"] == "4" &&
                   book.variables()["powerbook.drop.0.name"] == "Practice item 18",
               "drop scrolling is independent of the monster list");
        input.mouse_scroll = -10000;
        input.mouse_x = 100;
        book.update(input, ui);
        expect(book.variables()["powerbook.monster.15.level"] == "99",
               "scroll clamps at the final row");
        input.mouse_scroll = 0;
        input.mouse_x = 350;
        input.mouse_y = 213;
        input.mouse_left = input.mouse_left_clicked = true;
        book.update(input, ui);
        input.mouse_left_clicked = false;
        input.mouse_y = 740;
        book.update(input, ui);
        expect(book.variables()["powerbook.monster.15.level"] == "99",
               "drag scrollbar to the bottom");
        input.mouse_y = 212;
        book.update(input, ui);
        expect(book.variables()["powerbook.monster.0.level"] == "1", "drag scrollbar to the top");
        input.mouse_left = false;
        book.update(input, ui);
        // A 4:3 viewport uses the same uniform transform for drawing and hit testing.
        input.width = 1024;
        input.height = 768;
        input.mouse_x = 100 * .64f;
        input.mouse_y = 96 + 236 * .64f;
        input.mouse_left_clicked = true;
        book.update(input, ui);
        expect(book.variables()["powerbook.selection"] == "Practice Monster 2  /  Lv. 2" &&
                   book.variables()["powerbook.drop.0.name"] == "Money",
               "letterboxed row selection resets the drop scroll");
        const auto tooltip_text = [](const std::vector<client::TipLine>& lines) {
            std::string text;
            for (const auto& line : lines) {
                for (const auto& span : line.spans()) {
                    text += span.text();
                }
                text += '\n';
            }
            return text;
        };
        input.mouse_left_clicked = false;
        input.mouse_x = 554 * .64f;
        input.mouse_y = 96 + 220 * .64f;
        book.set_player(0, 5);
        book.update(input, ui);
        auto tooltip = book.tooltip_lines();
        expect(tooltip_text(tooltip).find("Training Talisman") != std::string::npos &&
                   tooltip_text(tooltip).find("Adds +5 Attack") != std::string::npos,
               "hovering a framed slot shows the talisman name and full effect");
        expect(std::any_of(tooltip.begin(),
                           tooltip.end(),
                           [](const auto& line) {
                               return std::any_of(
                                   line.spans().begin(), line.spans().end(), [](const auto& span) {
                                       return span.text().find("level 20") != std::string::npos &&
                                              span.g() < .5f;
                                   });
                           }),
               "requirement colors use the current character level");
        input.mouse_scroll = -1;
        book.update(input, ui);
        expect(tooltip_text(book.tooltip_lines()).find("Practice item 20") != std::string::npos &&
                   tooltip_text(book.tooltip_lines()).find("Training Talisman") ==
                       std::string::npos,
               "scrolling under a stationary pointer immediately updates the tooltip");
        input.mouse_scroll = -10000;
        book.update(input, ui);
        input.mouse_x = (412 + 3 * 60 + 8) * .64f;
        input.mouse_y = 96 + (206 + 9 * 56 + 8) * .64f;
        input.mouse_scroll = 0;
        book.update(input, ui);
        expect(tooltip_text(book.tooltip_lines()).find("Requires an eligible quest.") !=
                   std::string::npos,
               "last partial grid row is reachable and identifies quest-only drops");
        input.mouse_x = (412 + 4 * 60 + 8) * .64f;
        book.update(input, ui);
        expect(tooltip_text(book.tooltip_lines()).find("Item #900") != std::string::npos,
               "missing item definitions retain a readable catalog ID");
        input.mouse_x = (412 + 5 * 60 + 8) * .64f;
        book.update(input, ui);
        expect(book.tooltip_lines().empty(), "unused cells in a partial row have no stale tooltip");
        input.mouse_x = 100 * .64f;
        book.update(input, ui);
        expect(book.tooltip_lines().empty(), "leaving the grid clears the overlay");
        input.width = 1600;
        input.height = 900;
        input.mouse_x = 1450;
        input.mouse_y = 790;
        input.mouse_left_clicked = true;
        book.update(input, ui);
        book.update(input, ui);
        expect(book.variables()["powerbook.spawn_status"] == "Area 3  /  Map 7  /  3 of 3",
               "spawn navigation reaches a different logical map");
        input.key_p_pressed = true;
        book.update(input, ui);
        expect(!book.is_open(), "P closes the book");
        expect(book.tooltip_lines().empty(), "closing the book clears hover state");
        book.open(root, items);
        expect(book.variables()["powerbook.spawn_status"] == "Area 3  /  Map 7  /  3 of 3",
               "reopening preserves the selected spawn");
        input = {};
        input.width = 1024;
        input.height = 768;
        input.mouse_x = 1572 * .64f;
        input.mouse_y = 96 + 872 * .64f;
        input.mouse_left_clicked = true;
        book.update(input, ui);
        expect(!book.is_open(), "shared footer X closes the book outside the client box");
        book.open(root, items);
        input.key_p = input.key_p_pressed = true;
        input.key_o = input.key_o_pressed = true;
        input.key_g = input.key_g_pressed = true;
        client::mute_ui_input(input);
        expect(!input.key_p && !input.key_p_pressed && !input.key_o && !input.key_o_pressed &&
                   !input.key_g && !input.key_g_pressed && !input.mouse_left_clicked,
               "modal input muting includes the new key and prevents click-through");
        input.key_escape_pressed = true;
        book.update(input, ui);
        expect(!book.is_open(), "Escape closes the book");
        if (!capture) {
            return;
        }
        auto renderer = std::unique_ptr<engine::IEngine>(engine::create_engine());
        const bool initialized = renderer->init("OpenKal Powerbook verification", 1600, 900);
        expect(initialized, "initialize capture renderer");
        if (!initialized) {
            return;
        }
        ui.init(*renderer, root, OPENKAL_TEST_RUNTIME_ROOT);
        std::vector<std::uint8_t> pixels(16 * 16 * 4, 255);
        for (int y = 0; y < 16; ++y) {
            for (int x = 0; x < 16; ++x) {
                const auto i = static_cast<std::size_t>(y * 16 + x) * 4;
                pixels[i] = static_cast<std::uint8_t>(35 + 3 * x);
                pixels[i + 1] = static_cast<std::uint8_t>(75 + 3 * y);
                pixels[i + 2] = 62;
            }
        }
        for (int px = 28; px <= 34; px += 2) {
            for (int py = 28; py <= 34; py += 2) {
                char file[128];
                std::snprintf(
                    file, sizeof(file), "/UI/MiniMap/e/minimap_n_e_%03d_%03d.gtx", px, py);
                expect(test::write_rgba_dds(root + file, 16, 16, pixels),
                       "write owned outdoor map tile");
            }
        }
        for (int tile = 0; tile < 4; ++tile) {
            char file[128];
            std::snprintf(
                file, sizeof(file), "/UI/MiniMap/minimap_Dungeon_practice_%03d.GTX", tile);
            expect(test::write_rgba_dds(root + file, 16, 16, pixels),
                   "write owned dungeon map tile");
        }
        book.open(root, items);
        for (const auto& size :
             {std::pair{1600, 900}, std::pair{1280, 720}, std::pair{1024, 768}}) {
            input = {};
            input.width = size.first;
            input.height = size.second;
            input.mouse_x = 1450 * (size.first / 1600.0f);
            input.mouse_y =
                (size.second - size.first * 900.0f / 1600.0f) * .5f + 790 * (size.first / 1600.0f);
            input.mouse_left_clicked = true;
            input.mouse_x = 450 * (size.first / 1600.0f);
            input.mouse_y =
                (size.second - size.first * 900.0f / 1600.0f) * .5f + 220 * (size.first / 1600.0f);
            input.mouse_left_clicked = false;
            input.mouse_scroll = 1000;
            book.update(input, ui);
            input.mouse_scroll = 0;
            // Capture both map projections; the first click wraps back to the outdoor spawn.
            for (int spawn = 0; spawn < 3; ++spawn) {
                input.mouse_x = 1450 * (size.first / 1600.0f);
                input.mouse_y = (size.second - size.first * 900.0f / 1600.0f) * .5f +
                                790 * (size.first / 1600.0f);
                input.mouse_left_clicked = true;
                book.update(input, ui);
                input.mouse_x = 554 * (size.first / 1600.0f);
                input.mouse_y = (size.second - size.first * 900.0f / 1600.0f) * .5f +
                                220 * (size.first / 1600.0f);
                input.mouse_left_clicked = false;
                book.update(input, ui);
                for (int frame = 0; frame < 5; ++frame) {
                    expect(renderer->begin_offscreen(size.first, size.second, 0, 0, 0),
                           "begin Powerbook capture");
                    book.draw(*renderer,
                              ui,
                              static_cast<float>(size.first),
                              static_cast<float>(size.second));
                    expect(renderer->end_offscreen_rgba(pixels), "read Powerbook capture pixels");
                }
                expect(tooltip_text(book.tooltip_lines()).find("Adds +5 Attack") !=
                           std::string::npos,
                       "rendered overlay retains the talisman effect at every viewport size");
                std::ofstream out(std::string(capture) + "-" + std::to_string(size.first) + "-" +
                                      std::to_string(spawn) + ".ppm",
                                  std::ios::binary);
                out << "P6\n" << size.first << ' ' << size.second << "\n255\n";
                for (std::size_t i = 0; i + 3 < pixels.size(); i += 4) {
                    out.write(reinterpret_cast<const char*>(pixels.data() + i), 3);
                }
            }
        }
        book.destroy(*renderer);
        ui.destroy();
        renderer->shutdown();
    }
}  // namespace

int main(int argc, char** argv) {
    const auto root = test::temp_dir(("powerbook_" + std::to_string(getpid())).c_str());
    fixtures(root);
    catalog_checks(root);
    ui_checks(root, argc > 1 ? argv[1] : nullptr);
    std::filesystem::remove_all(root);
    return test::g_failures ? 1 : 0;
}
