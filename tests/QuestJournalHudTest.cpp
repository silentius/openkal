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
#include "openkal/client/Inventory.h"
#include "openkal/client/ItemDb.h"
#include "openkal/client/QuestJournalWindow.h"
#include "openkal/client/UiLayout.h"
#include "openkal/server/world/ItemGroupDb.h"
#include "test_util.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <memory>
#include <unistd.h>

namespace {
    using namespace openkal::client;
    using openkal::test::expect;

    void verify_locations() {
        TaskFlagDef stage;
        stage.key = 2;
        stage.item_index = 140;
        stage.item_count = 2;
        stage.npc_index = 7;
        PowerbookMonster monster;
        monster.name = "Training Beast";
        monster.drops = {{140, true}};
        monster.quest_drops = {{1, 2, 140}};
        monster.spawns = {{0, 1, 256000, 256000, 258000, 258000},
                          {0, 2, 258000, 258000, 259000, 259000}};
        NpcSpawn npc;
        npc.set_index(7);
        npc.set_name("Quest Keeper");
        npc.set_map(3);
        npc.set_server_x(258000);
        npc.set_server_y(258100);
        auto locations = quest_locations(1, stage, 0, {monster}, {npc});
        expect(locations.size() == 2 && !locations[0].npc,
               "incomplete item objective lists spawn areas");
        expect(quest_locations(2, stage, 0, {monster}, {npc}).empty(),
               "other quest drops are excluded");
        stage.key = 3;
        expect(quest_locations(1, stage, 0, {monster}, {npc}).empty(),
               "other stage drops are excluded");
        stage.key = 2;
        locations = quest_locations(1, stage, 2, {monster}, {npc});
        expect(locations.size() == 1 && locations[0].npc && locations[0].spawn.map == 3 &&
                   locations[0].spawn.x0 == 258000 && locations[0].spawn.y0 == 258100,
               "completed requirement switches to the exact return NPC");
        expect(quest_locations(1, stage, 2, {monster}, {}).empty(),
               "missing NPC has no invented location");
        monster.drops = {{140, false}};
        expect(quest_locations(2, stage, 0, {monster}, {}).size() == 2,
               "regular drops are available across quests");
        stage.item_count = 0;
        expect(quest_locations(1, stage, 0, {}, {npc}).size() == 1,
               "talk objective resolves the NPC without monsters");
    }

    std::string fixtures() {
        const auto root =
            openkal::test::temp_dir(("quest_snitch_" + std::to_string(getpid())).c_str());
        std::filesystem::create_directories(root + "/tasks");
        std::filesystem::create_directories(root + "/items");
        std::filesystem::create_directories(root + "/monsters");
        // All quest, item and spawn records here are project-owned synthetic data.
        for (int i = 1; i <= 6; ++i) {
            std::ofstream(root + "/tasks/" + std::to_string(i) + ".json")
                << "{\"key\":" + std::to_string(i) +
                       R"(,"name":"A request from the Quest Keeper","desc":"Help the village prepare for winter.","flags":[{"key":1,"desc":"Collect a training token, then return to the Quest Keeper.","item":[2, 140, 1],"npcIndex":7,"npcDesc":"Quest Keeper"}]})";
        }
        std::ofstream(root + "/items/140.json")
            << R"({"index":140,"name":"Training Token","class":["general", "etc"]})";
        std::ofstream(root + "/monsters/1.json")
            << R"({"index":1,"name":"Training Beast","stats":{"level":3,"itemGroups":[[1, 1]]},"spawns":[{"map":0,"area":1,"max":1,"rect":[8000, 8000, 8100, 8100]},{"map":0,"area":2,"max":1,"rect":[8120, 8000, 8160, 8040]}]})";
        std::filesystem::create_directories(root + "/groups");
        std::filesystem::create_directories(root + "/itemgroups");
        openkal::server::GroupDef group{
            1, "", {{openkal::server::GroupEntry::Kind::Item, 100, 140, 0, 1, 0}}};
        expect(openkal::server::ItemGroupDb::save_group(root + "/groups", group),
               "save quest loot fixture");
        expect(
            openkal::server::ItemGroupDb::save_itemgroup(root + "/itemgroups", {1, "", {{100, 1}}}),
            "save quest loot reference");
        return root;
    }

    void verify_controls(QuestJournalWindow& window, const UiDocument& doc) {
        for (const float scale : {0.75f, 1.0f, 1.5f}) {
            window.set_scale(scale);
            window.set_open(true);
            window.center_on_screen(1920, 1080);
            expect(window.contains(window.x(), window.y()), "client consumes pointer");
            expect(!window.contains(-100, -100), "outside pointer ignored");
            const float old_x = window.x();
            const float old_y = window.y();
            expect(window.handle_click(old_x + 160 * scale, old_y - 40 * scale),
                   "title starts drag");
            openkal::engine::FrameInput input;
            input.mouse_left = true;
            input.mouse_x = old_x + 180 * scale;
            input.mouse_y = old_y - 10 * scale;
            window.update_pointer(input);
            expect(std::abs(window.x() - old_x - 20 * scale) < 0.01f &&
                       std::abs(window.y() - old_y - 30 * scale) < 0.01f,
                   "drag follows pointer at each scale");
            input.mouse_left = false;
            window.update_pointer(input);
            float x, y, w, h;
            UiView::widget_rect(*doc.find_widget("close"),
                                nullptr,
                                window.x(),
                                window.y(),
                                window.w() * scale,
                                window.h() * scale,
                                scale,
                                x,
                                y,
                                w,
                                h);
            expect(window.handle_click(x + w / 2, y + h / 2) && !window.open(),
                   "anchored footer button closes window");
        }
    }

    void capture(QuestJournalWindow& window, const std::string& prefix) {
        std::unique_ptr<openkal::engine::IEngine> engine(openkal::engine::create_engine());
        if (!engine->init("Quest Journal verification", 1200, 900)) {
            expect(false, "initialize capture engine");
            return;
        }
        WidgetTools ui;

        const auto root = fixtures();
        ui.init(*engine, root, OPENKAL_TEST_RUNTIME_ROOT);
        std::vector<std::uint8_t> tile_pixels(16 * 16 * 4, 255);
        for (std::size_t i = 0; i < tile_pixels.size(); i += 4) {
            tile_pixels[i] = 45;
            tile_pixels[i + 1] = static_cast<std::uint8_t>(70 + (i / 64) * 3);
            tile_pixels[i + 2] = 60;
        }
        for (int x = 28; x <= 34; x += 2) {
            for (int y = 28; y <= 34; y += 2) {
                char name[128];
                std::snprintf(name, sizeof(name), "/UI/MiniMap/e/minimap_n_e_%03d_%03d.gtx", x, y);
                expect(openkal::test::write_rgba_dds(root + name, 16, 16, tile_pixels),
                       "write owned map tile");
            }
        }

        TaskCatalog tasks;
        ItemDb items;
        expect(tasks.load_dir(root + "/tasks"), "load quests");
        expect(items.load(root, OPENKAL_REPO_ROOT), "load item names");
        const std::vector<openkal::protocol::QuestFlagEntry> entries{
            {1, 1, 0}, {2, 1, 0}, {3, 1, 0}, {4, 1, 0}, {5, 1, 0}, {6, 1, 0}};
        NpcSpawn npc;
        npc.set_index(7);
        npc.set_name("Quest Keeper");
        npc.set_server_x(258000);
        npc.set_server_y(258000);
        const std::vector<NpcSpawn> npcs{npc};
        UiLayout snitch_layout;
        expect(snitch_layout.load_file(std::string(OPENKAL_REPO_ROOT) +
                                       "/config/ui/quest_snitch.json"),
               "load Snitch layout");
        window.set_snitch_context(&snitch_layout, root, npcs);
        window.sync_quests(entries, tasks, items);
        window.set_open(true);
        window.sync_fonts(*engine);
        FreetypeFont fallback;
        Bag bag;
        for (const float scale : {1.0f, 1.5f}) {
            window.set_scale(scale);
            window.center_on_screen(1200, 900);
            auto render = [&]() {
                expect(engine->begin_offscreen(1200, 900, 0.12f, 0.14f, 0.16f), "begin capture");
                window.draw(*engine, ui, fallback, bag);
                std::vector<std::uint8_t> pixels;
                expect(engine->end_offscreen_rgba(pixels), "read rendered journal");
                return pixels;
            };
            const auto first = render();
            expect(window.handle_click(window.x() + 30 * scale, window.y() + 82 * scale),
                   "select second quest");
            const auto selected = render();
            expect(first != selected, "row selection changes rendered details");
            std::ofstream out(prefix + (scale == 1 ? "-native.ppm" : "-scaled.ppm"),
                              std::ios::binary);
            out << "P6\n1200 900\n255\n";
            for (std::size_t i = 0; i + 3 < selected.size(); i += 4) {
                out.write(reinterpret_cast<const char*>(selected.data() + i), 3);
            }
            expect(out.good(), "save rendered preview");
            expect(window.handle_click(window.x() + 654 * scale, window.y() + 404 * scale),
                   "Snitch button consumes click");
            expect(window.snitch_open(), "Snitch opens for the selected objective");
            (void) render();
            (void) render();
            const auto snitch = render();
            expect(snitch != selected, "objective map changes rendered window");
            std::ofstream map_out(prefix + (scale == 1 ? "-snitch.ppm" : "-snitch-scaled.ppm"),
                                  std::ios::binary);
            map_out << "P6\n1200 900\n255\n";
            for (std::size_t i = 0; i + 3 < snitch.size(); i += 4) {
                map_out.write(reinterpret_cast<const char*>(snitch.data() + i), 3);
            }
            bag.quest.resize(1);
            bag.quest[0].index = 140;
            bag.quest[0].count = 1;
            expect(render() != selected, "live item progress changes rendered journal");
            const auto npc_image = render();
            expect(npc_image != snitch, "completed item requirement updates the objective map");
            std::ofstream npc_out(prefix + (scale == 1 ? "-npc.ppm" : "-npc-scaled.ppm"),
                                  std::ios::binary);
            npc_out << "P6\n1200 900\n255\n";
            for (std::size_t i = 0; i + 3 < npc_image.size(); i += 4) {
                npc_out.write(reinterpret_cast<const char*>(npc_image.data() + i), 3);
            }
            expect(window.close_snitch() && window.open(), "closing Snitch preserves journal");
            bag.quest.clear();
            (void) window.handle_click(window.x() + 30 * scale, window.y() + 62 * scale);
        }
        window.sync_quests({}, tasks, items);
        expect(engine->begin_offscreen(1200, 900, 0.12f, 0.14f, 0.16f), "begin empty journal");
        window.draw(*engine, ui, fallback, bag);
        std::vector<std::uint8_t> empty;
        expect(engine->end_offscreen_rgba(empty), "render empty journal");
        std::filesystem::remove_all(root);
        window.destroy_fonts(*engine);
        ui.destroy();
        engine->shutdown();
    }
}  // namespace

int main(int argc, char** argv) {
    UiLayout layout;
    expect(layout.load_file(std::string(OPENKAL_REPO_ROOT) + "/config/ui/quest_journal.json"),
           "load quest layout");
    UiDocument doc;
    expect(doc.load(layout.root()), "parse quest document");
    QuestJournalWindow window;
    window.set_layout(&layout);
    verify_controls(window, doc);
    verify_locations();
    const auto root = fixtures();
    TaskCatalog tasks;
    ItemDb items;
    expect(tasks.load_dir(root + "/tasks"), "load synthetic quest objectives");
    const std::vector<NpcSpawn> npcs;
    UiLayout snitch_layout;
    expect(snitch_layout.load_file(std::string(OPENKAL_REPO_ROOT) + "/config/ui/quest_snitch.json"),
           "load objective map layout");
    UiDocument snitch_doc;
    expect(snitch_doc.load(snitch_layout.root()), "parse objective map layout");
    window.set_snitch_context(&snitch_layout, root, npcs);
    window.sync_quests({{1, 1, 0}}, tasks, items);
    window.set_scale(1);
    window.set_open(true);
    window.center_on_screen(1920, 1080);
    expect(window.handle_click(window.x() + 654, window.y() + 404) && window.snitch_open(),
           "Snitch button opens the objective window");
    const float snitch_x = window.x() + window.w() + 48;
    const float snitch_y = window.y();
    expect(window.contains(snitch_x + 200, snitch_y + 100),
           "objective window captures pointer outside journal");
    float close_x, close_y, close_w, close_h;
    UiView::widget_rect(*snitch_doc.find_widget("close"),
                        nullptr,
                        snitch_x,
                        snitch_y,
                        snitch_doc.window_w(),
                        snitch_doc.window_h(),
                        1,
                        close_x,
                        close_y,
                        close_w,
                        close_h);
    expect(window.handle_click(close_x + close_w / 2, close_y + close_h / 2) &&
               !window.snitch_open() && window.open(),
           "objective footer closes only the child window");
    (void) window.handle_click(window.x() + 654, window.y() + 404);
    window.set_open(false);
    expect(!window.snitch_open(), "closing quest journal closes its objective window");
    window.set_open(true);
    window.sync_quests({}, tasks, items);
    (void) window.handle_click(window.x() + 654, window.y() + 404);
    expect(!window.snitch_open(), "empty journal cannot open Snitch");
    std::filesystem::remove_all(root);

    if (argc == 2) {
        capture(window, argv[1]);
    }
    return openkal::test::g_failures == 0 ? 0 : 1;
}
