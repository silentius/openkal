/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/PowerbookCatalog.h"

#include "openkal/map/Constants.h"
#include "openkal/map/MapCatalog.h"
#include "openkal/platform/File.h"
#include "openkal/platform/Json.h"
#include "openkal/platform/Path.h"
#include "openkal/server/world/DropRoller.h"
#include "openkal/server/world/ItemGroupDb.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <tuple>

namespace openkal::client {
    namespace {
        int chance_units(float chance) {
            if (!std::isfinite(chance) || chance <= 0) {
                return 0;
            }
            return static_cast<int>(std::lround(std::min(chance, 100.0f) * 100.0));
        }

        void collect_drops(const server::ItemGroupDb& tables,
                           int index,
                           bool quest,
                           std::map<int, bool>& items,
                           bool& complete) {
            const auto* outer = tables.find_itemgroup(index);
            if (!outer) {
                complete = false;
                return;
            }
            int outer_total = 0;
            for (const auto& reference : outer->entries) {
                const int units = chance_units(reference.chance);
                if (units == 0 || outer_total >= 10000) {
                    continue;
                }
                outer_total += units;
                const auto* group = tables.find_group(reference.group_index);
                if (!group) {
                    complete = false;
                    continue;
                }
                int inner_total = 0;
                for (const auto& entry : group->entries) {
                    const int inner_units = chance_units(entry.chance);
                    if (inner_units == 0 || inner_total >= 10000) {
                        continue;
                    }
                    inner_total += inner_units;
                    const bool money = entry.kind == server::GroupEntry::Kind::Money;
                    const int item = money ? server::kMoneyItemIndex : entry.item_index;
                    if (item <= 0 || (money ? entry.amount : entry.count) <= 0) {
                        continue;
                    }
                    const auto result = items.emplace(item, quest);
                    if (!quest) {
                        result.first->second = false;
                    }
                }
            }
        }
    }  // namespace

    bool PowerbookCatalog::load(const std::string& configuration_root) {
        m_monsters.clear();
        server::ItemGroupDb tables;
        (void) tables.load_groups_dir(platform::join_path(configuration_root, "groups"));
        (void) tables.load_itemgroups_dir(platform::join_path(configuration_root, "itemgroups"));
        map::MapCatalog maps;
        (void) maps.load_json(platform::join_path(configuration_root, "mapinfo.json"));
        const auto directory = platform::join_path(configuration_root, "monsters");
        if (!platform::is_directory(directory)) {
            return false;
        }
        auto files = platform::list_directory(directory);
        std::sort(files.begin(), files.end());
        for (const auto& file : files) {
            if (file.size() < 5 || file.substr(file.size() - 5) != ".json" ||
                file == "bones.json" || file == "manifest.json") {
                continue;
            }
            const auto bytes = platform::read_file(platform::join_path(directory, file));
            platform::Json root;
            if (!bytes || !platform::json_parse(std::string(bytes->begin(), bytes->end()), root) ||
                !root.is_object()) {
                continue;
            }
            PowerbookMonster monster;
            monster.index = root.at("index").as_int(0);
            if (monster.index <= 0) {
                continue;
            }
            monster.name = root.at("name").as_string("Monster " + std::to_string(monster.index));
            const auto& stats = root.at("stats");
            monster.level = stats.at("level").as_int(1);
            std::vector<std::pair<int, int>> slots;
            for (const auto& row : stats.at("itemGroups").array()) {
                if (row.is_array() && row.array().size() >= 2) {
                    slots.emplace_back(row.array()[0].as_int(0), row.array()[1].as_int(0));
                }
            }
            std::map<int, bool> items;
            for (const auto& slot : server::DropRoller::effective_monster_kill_item_groups(
                     slots, stats.at("race").as_int(0))) {
                // An absent optional global table simply adds no global drops on the server.
                if (slot.first == server::kGlobalNormalMonsterItemGroupIndex &&
                    !tables.find_itemgroup(slot.first)) {
                    continue;
                }
                if (slot.first > 0 && slot.second > 0) {
                    collect_drops(tables, slot.first, false, items, monster.drops_complete);
                }
            }
            const auto add_quest = [&](const platform::Json& row) {
                if (row.is_array() && row.array().size() >= 4 && row.array()[0].as_int(0) > 0 &&
                    row.array()[2].as_int(0) > 0 && row.array()[3].as_int(0) > 0) {
                    std::map<int, bool> stage_items;
                    collect_drops(tables,
                                  row.array()[2].as_int(0),
                                  true,
                                  stage_items,
                                  monster.drops_complete);
                    for (const auto& item : stage_items) {
                        monster.quest_drops.push_back(
                            {row.array()[0].as_int(0), row.array()[1].as_int(0), item.first});
                        items.emplace(item.first, true);
                    }
                }
            };
            const auto& quests = stats.at("quests").array();
            if (!quests.empty() && !quests.front().is_array()) {
                add_quest(stats.at("quests"));
            } else {
                for (const auto& row : quests) {
                    add_quest(row);
                }
            }
            for (const auto& item : items) {
                monster.drops.push_back({item.first, item.second});
            }
            for (const auto& row : root.at("spawns").array()) {
                const auto& rect = row.at("rect").array();
                if (!row.is_object() || rect.size() < 4 || row.at("max").as_int(0) <= 0) {
                    continue;
                }
                // Keep pixel conversion within the shared server-coordinate range.
                constexpr int limit = std::numeric_limits<int>::max() / map::kTileSize;
                if (std::any_of(rect.begin(), rect.begin() + 4, [](const platform::Json& value) {
                        const int tile = value.as_int(-1);
                        return tile < 0 || tile > limit;
                    })) {
                    continue;
                }
                PowerbookSpawn spawn;
                spawn.map = row.at("map").as_int(0);
                spawn.area = row.at("area").as_int(0);
                spawn.x0 = rect[0].as_int(0) * map::kTileSize;
                spawn.y0 = rect[1].as_int(0) * map::kTileSize;
                spawn.x1 = rect[2].as_int(0) * map::kTileSize;
                spawn.y1 = rect[3].as_int(0) * map::kTileSize;
                if (spawn.x0 > spawn.x1) {
                    std::swap(spawn.x0, spawn.x1);
                }
                if (spawn.y0 > spawn.y1) {
                    std::swap(spawn.y0, spawn.y1);
                }
                spawn.map = maps.resolve_teleport_map(spawn.x0 + (spawn.x1 - spawn.x0) / 2,
                                                      spawn.y0 + (spawn.y1 - spawn.y0) / 2,
                                                      spawn.map);
                monster.spawns.push_back(spawn);
            }
            m_monsters.push_back(std::move(monster));
        }
        std::sort(m_monsters.begin(), m_monsters.end(), [](const auto& a, const auto& b) {
            return std::tie(a.level, a.name, a.index) < std::tie(b.level, b.name, b.index);
        });
        return !m_monsters.empty();
    }

    const std::vector<PowerbookMonster>& PowerbookCatalog::monsters() const {
        return m_monsters;
    }
}  // namespace openkal::client
