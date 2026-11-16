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
#include "openkal/client/ConsumableCraftingWindow.h"
#include "openkal/client/NpcCatalog.h"
#include "openkal/client/NpcTalkWindow.h"
#include "openkal/platform/File.h"
#include "openkal/platform/NpcDefinitions.h"
#include "test_util.h"

#include <filesystem>
#include <fstream>
#include <memory>
#include <unistd.h>

namespace {
    using namespace openkal;
    using test::expect;
    constexpr float screen_w = 1100, screen_h = 900;

    std::pair<float, float> point(const client::UiDocument& doc, const char* id, float scale) {
        const auto* widget = doc.find_widget(id);
        expect(widget != nullptr, "crafting control exists");
        if (!widget) {
            return {};
        }
        float x, y, w, h;
        client::UiView::widget_rect(*widget,
                                    nullptr,
                                    std::max(8.0f, (screen_w - 600 * scale) * .5f),
                                    std::max(8.0f, (screen_h - 424 * scale) * .35f),
                                    600 * scale,
                                    424 * scale,
                                    scale,
                                    x,
                                    y,
                                    w,
                                    h);
        return {x + w / 2, y + h / 2};
    }
}  // namespace

int main(int argc, char** argv) {
    using namespace openkal;
    for (const auto& file :
         std::filesystem::directory_iterator(std::string(OPENKAL_REPO_ROOT) + "/config/quests")) {
        const auto bytes = platform::read_file(file.path().string());
        platform::Json quest;
        if (bytes && platform::json_parse(std::string(bytes->begin(), bytes->end()), quest)) {
            expect(
                quest.at("index").as_int() != server::kConsumableCraftingQuestIndex,
                "crafting request namespace does not intercept existing quests or event donations");
        }
    }
    client::UiLayout layout;
    expect(layout.load_file(std::string(OPENKAL_REPO_ROOT) + "/config/ui/consumable_crafting.json"),
           "load reusable crafting layout");
    client::UiDocument doc;
    expect(doc.load(layout.root()), "parse crafting document");
    client::ConsumableCraftingWindow window;
    window.set_layout(&layout);
    window.sync_from_layout();
    client::Bag bag;
    for (int index : {23000,
                      23001,
                      23002,
                      23003,
                      23004,
                      23005,
                      23006,
                      server::kAloeVeraItemIndex,
                      server::kBlueLotusLeafItemIndex,
                      server::kGinsengRootItemIndex}) {
        client::ItemStack stack;
        stack.iid = index;
        stack.index = index;
        stack.count = 8;
        bag.normal.push_back(stack);
    }
    for (float scale : {.75f, 1.0f, 1.5f}) {
        window.set_scale(scale);
        window.set_open(true);
        window.center_on_screen(screen_w, screen_h);
        for (const auto family : {server::ConsumableFamily::Medicine,
                                  server::ConsumableFamily::MagicPotion,
                                  server::ConsumableFamily::AutoMedicine}) {
            window.set_family(family);
            const auto& recipes = server::consumable_recipes(family);
            expect(recipes.size() == (family == server::ConsumableFamily::AutoMedicine ? 4 : 3),
                   "all configured family recipes are visible");
            for (std::size_t row = 0; row < recipes.size(); ++row) {
                const auto id = "make_" + std::to_string(row);
                const auto [x, y] = point(doc, id.c_str(), scale);
                expect(window.contains(x, y), "scaled craft controls remain within bounds");
                expect(window.handle_click(x, y, bag) ==
                               client::ConsumableCraftingClickResult::Make &&
                           window.take_recipe() == recipes[row].flag && window.open(),
                       "each family dispatches its own recipe without closing");
                expect(window.take_recipe() == 0, "craft request is consumed once");
                if (family == server::ConsumableFamily::AutoMedicine) {
                    client::Bag single;
                    for (const auto& material : recipes[row].ingredients) {
                        client::ItemStack stack;
                        stack.iid = material.index;
                        stack.index = material.index;
                        stack.count = 1;
                        single.normal.push_back(stack);
                    }
                    expect(window.handle_click(x, y, single) ==
                                   client::ConsumableCraftingClickResult::Make &&
                               window.take_recipe() == recipes[row].flag,
                           "one Medicine and one root enable every auto conversion");
                    single.normal.front().count = 0;
                    expect(window.handle_click(x, y, single) !=
                               client::ConsumableCraftingClickResult::Make,
                           "a root alone cannot enable an auto conversion");
                }
                client::Bag empty;
                expect(window.handle_click(x, y, empty) !=
                           client::ConsumableCraftingClickResult::Make,
                       "live empty inventory disables crafting before a draw");
                auto missing = bag;
                const int material = recipes[row].ingredients.back().index;
                for (auto& stack : missing.normal) {
                    if (stack.index == material) {
                        stack.count = 0;
                    }
                }
                expect(window.handle_click(x, y, missing) !=
                           client::ConsumableCraftingClickResult::Make,
                       "other herbs cannot substitute for the family's ingredient");
            }
            if (recipes.size() == 3) {
                const auto [x, y] = point(doc, "make_3", scale);
                expect(window.handle_click(x, y, bag) !=
                           client::ConsumableCraftingClickResult::Make,
                       "fourth recipe control is hidden for three-recipe families");
            }
        }
        const auto [cx, cy] = point(doc, "close", scale);
        expect(window.handle_click(cx, cy, bag) == client::ConsumableCraftingClickResult::Close &&
                   !window.open(),
               "shared window closes at every UI scale");
    }
    // One resolver feeds server and client; test relative location without user data.
    const auto dir = test::temp_dir(("crafting_npcs_" + std::to_string(getpid())).c_str());
    std::filesystem::copy_file(std::string(OPENKAL_REPO_ROOT) +
                                   "/config/npcs/60001_medicine_alchemist.json",
                               dir + "/60001.json",
                               std::filesystem::copy_options::overwrite_existing);
    expect(platform::load_npc_definitions(dir).empty(),
           "missing anchor never spawns a stray NPC at origin");
    {
        std::ofstream out(dir + "/1.json");
        out << R"({"index":1,"html":192,"name":"Synthetic chemist","shape":7,"map":2,"xy":[1000,2000,30],"dir":[1000,2100],"appearance":{"boneId":7}})";
    }
    client::NpcCatalog npcs;
    expect(npcs.load_json_dir(dir) && npcs.spawns().size() == 2,
           "client loads anchor and companion");
    const auto& npc = npcs.spawns().back();
    expect(npc.index() == 60001 && npc.server_x() == 257454 && npc.server_y() == 258802 &&
               npc.server_z() == 16080 && npc.map() == 2 && npc.shape() == 7 &&
               npc.dir_x() == 257454 && npc.dir_y() == 258902,
           "companion uses its fixed position while inheriting map, appearance and facing");
    client::DialogCatalog dialogs;
    expect(dialogs.load_dir(std::string(OPENKAL_REPO_ROOT) + "/config/dialogs"),
           "load alchemist dialogue");
    const auto* dialog = dialogs.find(server::kConsumableCraftingDialogId);
    expect(dialog && dialog->links.size() == 4, "alchemist has three family entries and exit");
    if (dialog) {
        const client::NpcTalkClickResult expected[] = {
            client::NpcTalkClickResult::CraftMedicine,
            client::NpcTalkClickResult::CraftMagicPotion,
            client::NpcTalkClickResult::CraftAutoMedicine};
        for (int row = 0; row < 3; ++row) {
            auto single = *dialog;
            single.lines.clear();
            single.links = {dialog->links[row]};
            client::NpcTalkWindow talk;
            talk.show_dialog(single, 77);
            talk.center_on_screen(screen_w, screen_h);
            expect(talk.handle_click(talk.x() + talk.w() - 58, talk.y() + 36) == expected[row] &&
                       talk.npc_id() == 77,
                   "each family uses the shared crafting window dispatch");
        }
    }
    if (argc > 1) {
        std::unique_ptr<engine::IEngine> engine(engine::create_engine());
        const bool initialized = engine->init("Consumable crafting verification", 1100, 900);
        expect(initialized, "initialize crafting preview");
        if (!initialized) {
            return 1;
        }
        client::WidgetTools ui;
        ui.init(*engine, OPENKAL_REPO_ROOT, OPENKAL_TEST_RUNTIME_ROOT);
        client::FreetypeFont font;
        expect(font.load(*engine, client::FreetypeFont::find_system_sans(), 13),
               "load crafting TrueType font");
        for (float scale : {1.0f, 1.5f}) {
            for (const auto family : {server::ConsumableFamily::Medicine,
                                      server::ConsumableFamily::MagicPotion,
                                      server::ConsumableFamily::AutoMedicine}) {
                window.set_family(family);
                window.set_open(true);
                window.set_scale(scale);
                window.center_on_screen(screen_w, screen_h);
                expect(engine->begin_offscreen(1100, 900, .12f, .14f, .16f),
                       "begin crafting preview");
                window.draw(*engine, ui, font, bag);
                std::vector<std::uint8_t> rgba;
                expect(engine->end_offscreen_rgba(rgba), "capture reusable crafting window");
                std::ofstream out(std::string(argv[1]) + "-" +
                                      std::to_string(static_cast<int>(family)) + "-" +
                                      std::to_string(scale) + ".ppm",
                                  std::ios::binary);
                out << "P6\n1100 900\n255\n";
                for (std::size_t i = 0; i + 3 < rgba.size(); i += 4) {
                    out.write(reinterpret_cast<const char*>(rgba.data() + i), 3);
                }
                expect(out.good(), "save crafting capture");
            }
        }
        font.destroy(*engine);
        ui.destroy();
        engine->shutdown();
    }
    std::filesystem::remove_all(dir);
    return test::g_failures ? 1 : 0;
}
