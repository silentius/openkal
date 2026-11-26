/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/HarvestPlants.h"

#include "openkal/map/Attributes.h"
#include "openkal/map/Constants.h"
#include "openkal/map/MapCatalog.h"
#include "openkal/platform/File.h"
#include "openkal/server/CastleWarRules.h"
#include "openkal/server/world/GameWorld.h"

#include <algorithm>
#include <cmath>
namespace openkal::server {
    const HarvestTypeConfig* HarvestConfig::find_type(int entity_index) const {
        for (const auto& type : types) {
            if (type.entity_index == entity_index) {
                return &type;
            }
        }
        return nullptr;
    }

    std::optional<HarvestConfig> parse_harvest_config(const platform::Json& value) {
        if (!value.is_object() || !value.at("types").is_object() || value.object().size() != 1) {
            return std::nullopt;
        }
        HarvestConfig result;
        std::unordered_set<int> entities;
        for (const auto& [name, entry] : value.at("types").object()) {
            if (name.empty() || !entry.is_object() ||
                entry.at("enabled").type() != platform::Json::Type::Bool) {
                return std::nullopt;
            }
            const auto integer =
                [&](const char* key, int fallback, int low, int high) -> std::optional<int> {
                const auto& number = entry.at(key);
                if (!entry.get(key)) {
                    return fallback >= low ? std::optional<int>(fallback) : std::nullopt;
                }
                if (!number.is_number() || !std::isfinite(number.number()) ||
                    std::floor(number.number()) != number.number() || number.number() < low ||
                    number.number() > high) {
                    return std::nullopt;
                }
                return number.as_int();
            };
            const auto entity = integer("entity_index", 0, 1, 65535);
            const auto item = integer("item_index", 0, 1, 65535);
            const auto amount = integer("items_per_pick", 1, 1, 1000);
            const auto max = integer("max_plants", 256, 0, 10000);
            const auto per_area = integer("plants_per_area", 8, 0, 128);
            const auto respawn = integer("respawn_seconds", 600, 1, 86400);
            if (!entity || !item || !amount || !max || !per_area || !respawn ||
                !entities.insert(*entity).second) {
                return std::nullopt;
            }
            HarvestTypeConfig type;
            type.type = name;
            type.entity_index = *entity;
            type.item_index = *item;
            type.items_per_pick = *amount;
            type.enabled = entry.at("enabled").boolean();
            type.max_plants = *max;
            type.plants_per_area = *per_area;
            type.respawn_seconds = *respawn;
            result.types.push_back(std::move(type));
        }
        return result;
    }

    std::optional<HarvestConfig> load_harvest_config(const std::string& path) {
        const auto bytes = platform::read_file(path);
        platform::Json value;
        if (!bytes || !platform::json_parse(std::string(bytes->begin(), bytes->end()), value)) {
            return std::nullopt;
        }
        return parse_harvest_config(value);
    }

    HarvestPlants::HarvestPlants(std::uint32_t seed) : m_random(seed) {}

    bool HarvestPlants::valid_position(const GameWorld& world, int x, int y) {
        constexpr auto excluded = map::kAttrVillage | map::kAttrPortal | map::kAttrObject |
                                  map::kAttrAttackCastle | map::kAttrDefenceCastle;
        for (int dy = -16; dy <= 16; dy += map::kCellSize) {
            for (int dx = -16; dx <= 16; dx += map::kCellSize) {
                if (!world.grid().walkable_world_pixel(x + dx, y + dy, excluded)) {
                    return false;
                }
            }
        }
        return true;
    }

    bool HarvestPlants::spawn(GameWorld& world,
                              Node& node,
                              const HarvestTypeConfig& type,
                              std::uint32_t& next_id,
                              std::vector<Position>& positions,
                              const CastleWarRules* castle_rules) {
        std::uniform_int_distribution<int> position(4, map::kCellCount - 5);
        for (int attempt = 0; attempt < 128; ++attempt) {
            const int x = node.cell_x * map::kWorldSize + position(m_random) * map::kCellSize + 4;
            const int y = node.cell_y * map::kWorldSize + position(m_random) * map::kCellSize + 4;
            if ((castle_rules && castle_rules->contains(node.map, x, y)) ||
                !valid_position(world, x, y)) {
                continue;
            }
            bool crowded = false;
            for (const auto& plant : positions) {
                if (plant.map != node.map) {
                    continue;
                }
                const auto dx = static_cast<long long>(plant.x) - x;
                const auto dy = static_cast<long long>(plant.y) - y;
                if (dx * dx + dy * dy < 256 * 256) {
                    crowded = true;
                    break;
                }
            }
            if (crowded) {
                continue;
            }
            const auto id = world.spawn_monster_at(next_id, type.entity_index, node.map, x, y, 0);
            if (id) {
                node.id = *id;
                positions.push_back({node.map, x, y});
                return true;
            }
        }
        return false;
    }

    void HarvestPlants::update(GameWorld& world,
                               const map::MapCatalog& catalog,
                               const HarvestConfig& config,
                               std::uint32_t& next_id,
                               float dt,
                               const CastleWarRules* castle_rules) {
        if (std::none_of(config.types.begin(), config.types.end(), [](const auto& type) {
                return type.enabled && type.max_plants > 0 && type.plants_per_area > 0;
            })) {
            return;
        }
        std::unordered_set<std::uint32_t> alive;
        alive.reserve(m_nodes.size());
        std::vector<Position> positions;
        positions.reserve(m_nodes.size());
        // Copy values: spawning can reallocate the world's entity vector.
        for (const auto& plant : world.monsters()) {
            if (plant.hp > 0 && config.find_type(plant.def_index)) {
                alive.insert(plant.id);
                positions.push_back({plant.map, plant.x, plant.y});
            }
        }
        int placements_left = 8;
        std::vector<int> populations(config.types.size(), 0);
        for (auto& node : m_nodes) {
            const auto& type = config.types.at(node.type_index);
            ++populations[node.type_index];
            if (!type.enabled) {
                continue;
            }
            if (node.id != 0) {
                if (alive.find(node.id) != alive.end()) {
                    continue;
                }
                node.id = 0;
                node.remaining = static_cast<float>(type.respawn_seconds);
            } else {
                node.remaining = std::max(0.0f, node.remaining - std::max(0.0f, dt));
                if (node.remaining == 0 && placements_left > 0) {
                    --placements_left;
                    if (!spawn(world, node, type, next_id, positions, castle_rules)) {
                        node.remaining = 10;
                    }
                }
            }
        }
        std::vector<map::MapCatalog::OutdoorCell> areas;
        for (const auto& area : catalog.outdoor_cells()) {
            if (area.map_index < 0 || area.map_index > 255) {
                continue;
            }
            const int key = map::cell_slot_index(area.cell_x, area.cell_y);
            if (m_areas.find(key) == m_areas.end() && world.grid().cell(area.cell_x, area.cell_y)) {
                areas.push_back(area);
            }
        }
        std::shuffle(areas.begin(), areas.end(), m_random);
        for (const auto& area : areas) {
            if (!m_areas.insert(map::cell_slot_index(area.cell_x, area.cell_y)).second) {
                continue;
            }
            for (std::size_t i = 0; i < config.types.size(); ++i) {
                const auto& type = config.types[i];
                if (type.enabled && type.max_plants > 0 && type.plants_per_area > 0) {
                    m_pending_areas.push_back(
                        {i, area.map_index, area.cell_x, area.cell_y, type.plants_per_area});
                }
            }
        }
        while (!m_pending_areas.empty() && placements_left > 0) {
            auto& area = m_pending_areas.front();
            const auto& type = config.types.at(area.type_index);
            if (!type.enabled || populations[area.type_index] >= type.max_plants) {
                m_pending_areas.pop_front();
                continue;
            }
            Node node;
            node.type_index = area.type_index;
            node.map = area.map;
            node.cell_x = area.cell_x;
            node.cell_y = area.cell_y;
            --placements_left;
            if (spawn(world, node, type, next_id, positions, castle_rules)) {
                m_nodes.push_back(node);
                ++populations[node.type_index];
            }
            if (--area.remaining == 0) {
                m_pending_areas.pop_front();
            }
        }
    }
}  // namespace openkal::server
