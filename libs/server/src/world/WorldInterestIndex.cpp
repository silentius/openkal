/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/world/WorldInterestIndex.h"

#include "openkal/server/world/GameWorld.h"
#include "openkal/server/world/NpcRegistry.h"

namespace openkal::server {
    namespace {
        template <class Entity>
        void rebuild_candidates(const std::vector<Entity>& entities,
                                std::unordered_map<int, std::vector<const Entity*>>& buckets) {
            for (auto& entry : buckets) {
                entry.second.clear();
            }
            std::vector<const Entity*>* current = nullptr;
            int current_map = 0;
            for (const auto& entity : entities) {
                if (!current || entity.map != current_map) {
                    current_map = entity.map;
                    current = &buckets[current_map];
                }
                current->push_back(&entity);
            }
        }

        template <class Entity>
        const std::vector<const Entity*>&
        candidates(const std::unordered_map<int, std::vector<const Entity*>>& buckets, int map) {
            const auto found = buckets.find(map);
            if (found != buckets.end()) {
                return found->second;
            }
            static const std::vector<const Entity*> empty;
            return empty;
        }
    }  // namespace

    void WorldInterestIndex::rebuild(const GameWorld& world, const NpcRegistry& npcs) {
        rebuild_candidates(world.monsters(), m_monstersByMap);
        rebuild_candidates(world.ground_items(), m_itemsByMap);
        rebuild_candidates(npcs.npcs(), m_npcsByMap);
    }

    const std::vector<const MonsterEntity*>& WorldInterestIndex::monsters(int map) const {
        return candidates(m_monstersByMap, map);
    }

    const std::vector<const GroundItem*>& WorldInterestIndex::items(int map) const {
        return candidates(m_itemsByMap, map);
    }

    const std::vector<const NpcEntity*>& WorldInterestIndex::npcs(int map) const {
        return candidates(m_npcsByMap, map);
    }

}  // namespace openkal::server
