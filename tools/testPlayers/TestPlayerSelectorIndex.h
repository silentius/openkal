/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include <cstddef>
#include <map>
#include <unordered_map>
#include <utility>
#include <vector>

namespace openkal::server {
    struct MonsterDef;
    struct MonsterSpawn;
    class ItemGroupDb;
}  // namespace openkal::server

/** Immutable spawn and drop membership shared by autonomous player selectors. */
class TestPlayerSelectorIndex {
  public:
    /** Builds owned membership without retaining pointers into the source catalogs.
     * @param definitions Monster definitions in their selection order.
     * @param spawns Authored spawn records in their previous order.
     * @param tables Nested itemgroup and group drop tables.
     */
    TestPlayerSelectorIndex(const std::vector<openkal::server::MonsterDef>& definitions,
                            const std::vector<openkal::server::MonsterSpawn>& spawns,
                            const openkal::server::ItemGroupDb& tables);

    /** Returns matching spawn maps, preserving order and repeated maps.
     * @param definition_index Monster definition id referenced by spawn records.
     * @return Ordered maps, or an empty vector when no spawn references the id.
     */
    const std::vector<int>& spawn_maps(int definition_index) const;

    /** Tests ordinary or matching quest-drop membership for one definition occurrence.
     * @param definition_ordinal Definition position, keeping duplicate ids independent.
     * @param quest_index Active quest id.
     * @param flag Active quest flag.
     * @param item_index Required item id.
     * @return True when the selector's drop predicates match; false for invalid ordinals.
     */
    bool
    drops_item(std::size_t definition_ordinal, int quest_index, int flag, int item_index) const;

  private:
    struct DefinitionItems {
        /** Sorted unique item ids for binary-search membership. */
        std::vector<int> ordinary;
        /** Sorted unique item ids per quest/flag pair for binary-search membership. */
        std::map<std::pair<int, int>, std::vector<int>> quests;
    };
    std::unordered_map<int, std::vector<int>> m_spawnMaps;
    std::vector<DefinitionItems> m_definitions;
};
