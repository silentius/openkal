/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/QuestSnitch.h"

#include <algorithm>

namespace openkal::client {
    std::vector<QuestLocation> quest_locations(int quest,
                                               const TaskFlagDef& stage,
                                               int have,
                                               const std::vector<PowerbookMonster>& monsters,
                                               const std::vector<NpcSpawn>& npcs) {
        std::vector<QuestLocation> result;
        if (stage.item_index > 0 && stage.item_count > 0 && have < stage.item_count) {
            for (const auto& monster : monsters) {
                const bool regular = std::any_of(
                    monster.drops.begin(), monster.drops.end(), [&](const PowerbookDrop& drop) {
                        return drop.item_index == stage.item_index && !drop.quest_only;
                    });
                const bool quest_drop = std::any_of(monster.quest_drops.begin(),
                                                    monster.quest_drops.end(),
                                                    [&](const PowerbookQuestDrop& drop) {
                                                        return drop.quest == quest &&
                                                               drop.flag == stage.key &&
                                                               drop.item_index == stage.item_index;
                                                    });
                if (regular || quest_drop) {
                    for (const auto& spawn : monster.spawns) {
                        result.push_back({monster.name, spawn, false});
                    }
                }
            }
        } else if (stage.npc_index > 0) {
            for (const auto& npc : npcs) {
                if (npc.index() == stage.npc_index) {
                    result.push_back({npc.name().empty() ? stage.npc_desc : npc.name(),
                                      {npc.map(),
                                       0,
                                       npc.server_x(),
                                       npc.server_y(),
                                       npc.server_x(),
                                       npc.server_y()},
                                      true});
                }
            }
        }
        return result;
    }
}  // namespace openkal::client
