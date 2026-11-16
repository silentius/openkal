/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/DialogCatalog.h"
#include "openkal/client/ItemDb.h"
#include "openkal/client/NpcCatalog.h"
#include "openkal/client/TaskCatalog.h"
#include "openkal/server/world/GameWorld.h"
#include "openkal/server/world/NpcRegistry.h"
#include "openkal/server/world/QuestDb.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
    void require(bool condition, const std::string& message) {
        if (!condition) {
            throw std::runtime_error(message);
        }
    }
}  // namespace

int main(int argc, char** argv) {
    try {
        require(argc == 2, "Expected the converted configuration directory");
        const std::string root = argv[1];
        openkal::client::ItemDb items;
        require(items.load(root, root + "/missing-assets"), "Client item loading failed");
        const auto* token = items.find(51001);
        require(token && token->stackable() && token->is_quest(),
                "Legacy plural or quest classification was lost");
        const auto* sword = items.find(51002);
        require(sword && sword->wearable() && !sword->stackable() && sword->class_limit() == 0 &&
                    sword->level_limit() == 5 && sword->min_attack() == 3 &&
                    sword->max_attack() == 10,
                "Client equipment fields were lost");
        const auto* medicine = items.find(51003);
        require(medicine && medicine->stackable() && medicine->refresh_hp() == 100 &&
                    medicine->cooltime_ms() == 1300,
                "Client consumable fields were lost");

        openkal::server::GameWorld world;
        require(world.load_items_json_dir(root + "/items"), "Server item loading failed");
        const auto& server_token = world.item_defs().at(51001);
        require(server_token.stackable && server_token.pay && !server_token.wearable,
                "Server stackability or pay metadata was lost");
        const auto& server_sword = world.item_defs().at(51002);
        require(server_sword.is_weapon && server_sword.class_small == "sword" &&
                    server_sword.wearable && !server_sword.stackable &&
                    server_sword.class_limit == 0 && server_sword.level_limit == 5 &&
                    server_sword.min_attack == 3 && server_sword.max_attack == 10,
                "Server equipment fields were lost");
        const auto& server_medicine = world.item_defs().at(51003);
        require(server_medicine.usable && server_medicine.stackable &&
                    server_medicine.refresh_hp == 100 && server_medicine.cooltime_ms == 1300,
                "Server consumable fields were lost");

        openkal::client::NpcCatalog npcs;
        require(npcs.load_json_dir(root + "/npcs"), "Client NPC loading failed");
        const auto& spawns = npcs.spawns();
        require(spawns.size() == 2, "Converted or OpenKal-only NPC missing");
        const auto guide = std::find_if(
            spawns.begin(), spawns.end(), [](const auto& row) { return row.index() == 51001; });
        require(guide != spawns.end() && guide->name() == "Maintained Guide" &&
                    guide->overhead_icon() == "test.bmp" && guide->name_offset_y(6) == 0 &&
                    guide->name_offset_pixels() == 10 && guide->server_z() == 300,
                "NPC presentation overrides were lost");
        const auto placed = std::find_if(
            spawns.begin(), spawns.end(), [](const auto& row) { return row.index() == 51002; });
        require(placed != spawns.end() && placed->shape() == 7 && placed->server_x() == 1010 &&
                    placed->server_y() == 2020 && placed->server_z() == 330 &&
                    placed->dir_x() == 2010 && placed->dir_y() == 2020,
                "NPC placement or inherited appearance changed");
        require(npcs.find_rig(7) && npcs.find_bone(9), "NPC rig or bone sidecar missing");

        openkal::server::NpcRegistry registry;
        require(registry.load_dir(root + "/npcs"), "Server NPC loading failed");
        std::uint32_t next_id = 1;
        registry.spawn_for_map(next_id, 2);
        require(registry.npcs().size() == 2, "Server NPC map resolution changed");
        const auto& live = registry.npcs();
        const auto service = std::find_if(
            live.begin(), live.end(), [](const auto& row) { return row.index == 51001; });
        require(service != live.end() && service->storage_keeper &&
                    service->trading_post_collector && service->html == 192 &&
                    service->quest_key != 0 && service->teleports.size() == 1 &&
                    service->teleports[0].x == 4000 && service->has_scenario &&
                    service->scenario_required_item == 51001,
                "NPC service, quest, teleport or scenario data was lost");

        require(world.load_monsters_json_dir(root + "/monsters", 1000, 2000, 0),
                "Monster output is incompatible");
        require(world.defs().size() == 1 && world.spawns().size() == 1,
                "Monster definition or spawn missing");
        require(world.load_prefixes_json_dir(root + "/prefixes") &&
                    world.prefix_defs().count(1) == 1,
                "Prefix output is incompatible");
        require(world.load_drop_tables(root + "/groups", root + "/itemgroups"),
                "Drop-table output is incompatible");
        require(world.load_skills_json_dir(root + "/skills") && !world.skill_defs().empty(),
                "Skill output is incompatible");
        require(world.load_pets_json_dir(root + "/pets") && world.pet_defs().count(51004) == 1 &&
                    world.pet_defs().count(51005) == 1,
                "Pet output is incompatible");
        openkal::server::QuestDb quests;
        require(quests.load_dir(root + "/quests") && quests.find(91, 1),
                "Quest output is incompatible");
        openkal::client::TaskCatalog tasks;
        require(tasks.load_dir(root + "/tasks") && tasks.find(91) &&
                    tasks.find(91)->name == "Synthetic Task" && tasks.find(91)->npc_index == 51001,
                "Journal task output is incompatible");
        openkal::client::DialogCatalog dialogs;
        require(dialogs.load_dir(root + "/dialogs") && dialogs.find(900001) &&
                    !dialogs.find(900001)->links.empty() &&
                    dialogs.find(900001)->links.front().quest_index == 91,
                "Dialog output is incompatible");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
