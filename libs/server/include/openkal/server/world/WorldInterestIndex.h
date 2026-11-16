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

#include <unordered_map>
#include <vector>

namespace openkal::server {

    class GameWorld;
    class NpcRegistry;
    struct MonsterEntity;
    struct GroundItem;
    struct NpcEntity;

    /** Borrows per-map interest candidates in their input traversal order.
     * The index does not track mutations. Rebuild after source containers or entity maps
     * change, and use borrowed pointers only before another source container mutation.
     * Callers must keep the source objects alive and protect the snapshot with their world lock.
     */
    class WorldInterestIndex final {
      public:
        /** Rebuilds all candidates while retaining existing per-map vector capacities.
         * @param world Source of live monsters and ground items.
         * @param npcs Source of live NPC entities.
         */
        void rebuild(const GameWorld& world, const NpcRegistry& npcs);

        /** Returns monster candidates for one map in source traversal order.
         * @param map Logical map to query.
         * @return Borrowed candidates, or a shared empty vector for an unknown map.
         */
        [[nodiscard]] const std::vector<const MonsterEntity*>& monsters(int map) const;

        /** Returns ground-item candidates for one map in source traversal order.
         * @param map Logical map to query.
         * @return Borrowed candidates, or a shared empty vector for an unknown map.
         */
        [[nodiscard]] const std::vector<const GroundItem*>& items(int map) const;

        /** Returns NPC candidates for one map in source traversal order.
         * @param map Logical map to query.
         * @return Borrowed candidates, or a shared empty vector for an unknown map.
         */
        [[nodiscard]] const std::vector<const NpcEntity*>& npcs(int map) const;

      private:
        std::unordered_map<int, std::vector<const MonsterEntity*>> m_monstersByMap;
        std::unordered_map<int, std::vector<const GroundItem*>> m_itemsByMap;
        std::unordered_map<int, std::vector<const NpcEntity*>> m_npcsByMap;
    };

}  // namespace openkal::server
