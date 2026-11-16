/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "TestPlayerSelectorIndex.h"

#include "openkal/server/world/GameWorld.h"
#include "openkal/server/world/ItemGroupDb.h"

#include <algorithm>

namespace {
    void deduplicate(std::vector<int>& items) {
        std::sort(items.begin(), items.end());
        items.erase(std::unique(items.begin(), items.end()), items.end());
    }
}  // namespace

TestPlayerSelectorIndex::TestPlayerSelectorIndex(
    const std::vector<openkal::server::MonsterDef>& definitions,
    const std::vector<openkal::server::MonsterSpawn>& spawns,
    const openkal::server::ItemGroupDb& tables)
    : m_definitions(definitions.size()) {
    for (const auto& spawn : spawns) {
        m_spawnMaps[spawn.def_index].push_back(spawn.map_index);
    }

    std::unordered_map<int, std::vector<int>> groupItems;
    const auto items_for_group = [&](int group) -> const std::vector<int>& {
        auto [cached, inserted] = groupItems.try_emplace(group);
        if (!inserted) {
            return cached->second;
        }
        auto& items = cached->second;
        if (const auto* outer = tables.find_itemgroup(group)) {
            for (const auto& reference : outer->entries) {
                const auto* inner = tables.find_group(reference.group_index);
                if (!inner || reference.chance <= 0) {
                    continue;
                }
                for (const auto& item : inner->entries) {
                    if (item.kind == openkal::server::GroupEntry::Kind::Item && item.chance > 0) {
                        items.push_back(item.item_index);
                    }
                }
            }
        }
        deduplicate(items);
        return items;
    };
    const auto append_group = [&](std::vector<int>& items, int group) {
        const auto& source = items_for_group(group);
        items.insert(items.end(), source.begin(), source.end());
    };
    for (std::size_t ordinal = 0; ordinal < definitions.size(); ++ordinal) {
        const auto& definition = definitions[ordinal];
        auto& items = m_definitions[ordinal];
        for (const auto& drop : definition.quest_drops) {
            append_group(items.quests[{drop.quest_index, drop.flag}], drop.itemgroup);
        }
        for (const auto& drop : definition.item_groups) {
            if (drop.second > 0) {
                append_group(items.ordinary, drop.first);
            }
        }
        deduplicate(items.ordinary);
        for (auto& [key, questItems] : items.quests) {
            (void) key;
            deduplicate(questItems);
        }
    }
}

const std::vector<int>& TestPlayerSelectorIndex::spawn_maps(int definition_index) const {
    const auto found = m_spawnMaps.find(definition_index);
    static const std::vector<int> empty;
    return found == m_spawnMaps.end() ? empty : found->second;
}

bool TestPlayerSelectorIndex::drops_item(std::size_t definition_ordinal,
                                         int quest_index,
                                         int flag,
                                         int item_index) const {
    if (definition_ordinal >= m_definitions.size()) {
        return false;
    }
    const auto& items = m_definitions[definition_ordinal];
    const auto quest = items.quests.find({quest_index, flag});
    return (quest != items.quests.end() &&
            std::binary_search(quest->second.begin(), quest->second.end(), item_index)) ||
           std::binary_search(items.ordinary.begin(), items.ordinary.end(), item_index);
}
