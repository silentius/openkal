/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/world/WorldOccupancyIndex.h"

#include "openkal/protocol/MonsterRace.h"
#include "openkal/server/world/GameWorld.h"

#include <algorithm>
#include <functional>

namespace openkal::server {
    namespace {
        int cell_coordinate(int pixel) {
            return pixel / 8 - (pixel % 8 < 0 ? 1 : 0);
        }
    }  // namespace

    bool WorldOccupancyIndex::CellKey::operator==(const CellKey& other) const {
        return map == other.map && x == other.x && y == other.y;
    }

    std::size_t WorldOccupancyIndex::CellHash::operator()(const CellKey& key) const {
        auto hash = std::hash<int>{}(key.map);
        for (int coordinate : {key.x, key.y}) {
            hash ^= std::hash<int>{}(coordinate) + 0x9e3779b9U + (hash << 6) + (hash >> 2);
        }
        return hash;
    }

    WorldOccupancyIndex::Reservation
    WorldOccupancyIndex::reservation(const MonsterEntity& monster) {
        const bool moving = monster.dirty_move || monster.moving;
        return {monster.id,
                monster.map,
                cell_coordinate(moving ? monster.target_x : monster.x),
                cell_coordinate(moving ? monster.target_y : monster.y),
                monster.hp > 0 && protocol::MonsterRaceInfo::blocks_collision(monster.race)};
    }

    void WorldOccupancyIndex::rebuild(const std::vector<MonsterEntity>& monsters,
                                      const std::vector<PlayerPose>& players) {
        m_cells.clear();
        m_cells.reserve(monsters.size() + players.size());
        for (const auto& monster : monsters) {
            const auto occupied = reservation(monster);
            if (occupied.blocks) {
                m_cells[{occupied.map, occupied.cell_x, occupied.cell_y}].monsters.push_back(
                    occupied.id);
            }
        }
        for (const auto& player : players) {
            ++m_cells[{player.map, cell_coordinate(player.x), cell_coordinate(player.y)}].players;
        }
    }

    void WorldOccupancyIndex::update(const Reservation& before, const MonsterEntity& after) {
        const auto occupied = reservation(after);
        if (before.blocks == occupied.blocks &&
            (!before.blocks ||
             (before.id == occupied.id && before.map == occupied.map &&
              before.cell_x == occupied.cell_x && before.cell_y == occupied.cell_y))) {
            return;
        }
        if (before.blocks) {
            const auto found = m_cells.find({before.map, before.cell_x, before.cell_y});
            if (found != m_cells.end()) {
                auto& cell = found->second;
                const auto monster =
                    std::find(cell.monsters.begin(), cell.monsters.end(), before.id);
                if (monster != cell.monsters.end()) {
                    cell.monsters.erase(monster);
                }
                if (cell.monsters.empty() && cell.players == 0) {
                    m_cells.erase(found);
                }
            }
        }
        if (occupied.blocks) {
            m_cells[{occupied.map, occupied.cell_x, occupied.cell_y}].monsters.push_back(
                occupied.id);
        }
    }

    bool WorldOccupancyIndex::blocked(
        std::uint32_t self, int px, int py, int map, bool include_players) const {
        const auto found = m_cells.find({map, cell_coordinate(px), cell_coordinate(py)});
        if (found == m_cells.end()) {
            return false;
        }
        const auto& cell = found->second;
        return (include_players && cell.players != 0) ||
               std::any_of(cell.monsters.begin(), cell.monsters.end(), [self](std::uint32_t id) {
                   return id != self;
               });
    }

}  // namespace openkal::server
