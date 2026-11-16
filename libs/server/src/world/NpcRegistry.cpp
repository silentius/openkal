/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/world/NpcRegistry.h"

#include <algorithm>

#include "openkal/platform/File.h"
#include "openkal/platform/Json.h"
#include "openkal/platform/Log.h"
#include "openkal/platform/NpcDefinitions.h"
#include "openkal/platform/Path.h"

#include <cmath>

namespace openkal::server {
    namespace {

        [[nodiscard]] std::uint16_t dir_from_look(int x, int y, int look_x, int look_y) {
            const double dx = static_cast<double>(look_x - x);
            const double dy = static_cast<double>(look_y - y);
            if (dx == 0.0 && dy == 0.0) {
                return 0;
            }
            double angle = std::atan2(dy, dx);
            if (angle < 0.0) {
                angle += 6.283185307179586;
            }
            return static_cast<std::uint16_t>(angle * 65536.0 / 6.283185307179586);
        }

        [[nodiscard]] bool parse_npc_json(const platform::Json& root, NpcDef& out) {
            out.index = root.at("index").as_int(0);
            if (out.index <= 0) {
                return false;
            }
            out.name = root.at("name").as_string();
            out.kind = root.at("kind").as_int(0);
            out.shape = root.at("shape").as_int(0);
            out.map = root.at("map").as_int(0);
            out.html = root.at("html").as_int(0);
            const platform::Json& storage_keeper = root.at("storage_keeper");
            out.storage_keeper =
                storage_keeper.type() == platform::Json::Type::Bool && storage_keeper.boolean();
            const auto& collector = root.at("trading_post_collector");
            out.trading_post_collector =
                collector.type() == platform::Json::Type::Bool && collector.boolean();
            const platform::Json& xy = root.at("xy");
            if (xy.is_array() && xy.array().size() >= 2) {
                out.x = xy.array()[0].as_int(0);
                out.y = xy.array()[1].as_int(0);
                out.z = xy.array().size() >= 3 ? xy.array()[2].as_int(0) : 0;
            }
            const platform::Json& dir = root.at("dir");
            if (dir.is_array() && dir.array().size() >= 2) {
                out.dir =
                    dir_from_look(out.x, out.y, dir.array()[0].as_int(0), dir.array()[1].as_int(0));
            }
            const platform::Json& quest = root.at("quest");
            if (quest.is_array() && quest.array().size() >= 2) {
                out.has_quest = true;
                out.quest_index = quest.array()[0].as_int(0);
                out.quest_flag = quest.array()[1].as_int(0);
            }
            out.teleports.clear();
            const platform::Json& teleports = root.at("teleports");
            if (teleports.is_array()) {
                for (const platform::Json& row : teleports.array()) {
                    if (!row.is_array() || row.array().size() < 3) {
                        continue;
                    }
                    NpcTeleportDestination destination;
                    destination.map = row.array()[0].as_int(0);
                    destination.x = row.array()[1].as_int(0);
                    destination.y = row.array()[2].as_int(0);
                    out.teleports.push_back(destination);
                }
            }
            const platform::Json& scenario = root.at("scenario");
            if (scenario.is_array() && scenario.array().size() >= 2) {
                out.has_scenario = true;
                out.scenario_slot = scenario.array()[0].as_int(-1);
                out.scenario_required_item = scenario.array()[1].as_int(0);
            }
            return true;
        }

        [[nodiscard]] NpcEntity make_live_npc(std::uint32_t& id_gen, const NpcDef& def) {
            NpcEntity e;
            e.id = id_gen++;
            e.index = def.index;
            e.kind = def.kind;
            e.shape = static_cast<int>(def.shape);
            e.html = def.html;
            e.storage_keeper = def.storage_keeper;
            e.trading_post_collector = def.trading_post_collector;
            if (def.has_quest) {
                e.quest_key = protocol::Quest::make_key(static_cast<std::uint16_t>(def.quest_index),
                                                        static_cast<std::uint8_t>(def.quest_flag));
            }
            e.teleports = def.teleports;
            e.has_scenario = def.has_scenario;
            e.scenario_slot = def.scenario_slot;
            e.scenario_required_item = def.scenario_required_item;
            e.x = def.x;
            e.y = def.y;
            e.z = def.z;
            e.map = def.map;
            e.dir = def.dir;
            return e;
        }

    }  // namespace

    bool NpcRegistry::load_dir(const std::string& dir) {
        m_defs.clear();
        if (!platform::is_directory(dir)) {
            return false;
        }
        for (const auto& root : platform::load_npc_definitions(dir)) {
            NpcDef def;
            if (!parse_npc_json(root, def)) {
                continue;
            }
            m_defs[def.index] = std::move(def);
        }
        log::info("NpcRegistry: loaded " + std::to_string(m_defs.size()) + " defs from " + dir);
        return !m_defs.empty();
    }

    void NpcRegistry::spawn_for_map(std::uint32_t& id_gen, int map_index) {
        m_npcs.clear();
        m_npcs.reserve(m_defs.size());
        for (const auto& [_, def] : m_defs) {
            if (map_index >= 0 && def.map != map_index) {
                continue;
            }
            m_npcs.push_back(make_live_npc(id_gen, def));
        }
        log::info("NpcRegistry: spawned " + std::to_string(m_npcs.size()) + " NPCs");
    }

    void NpcRegistry::spawn_for_maps(std::uint32_t& id_gen, int min_map, int max_map) {
        m_npcs.clear();
        m_npcs.reserve(m_defs.size());
        for (const auto& [_, def] : m_defs) {
            if (def.map < min_map || def.map > max_map) {
                continue;
            }
            m_npcs.push_back(make_live_npc(id_gen, def));
        }
        log::info("NpcRegistry: spawned " + std::to_string(m_npcs.size()) + " NPCs");
    }

    void NpcRegistry::append_for_map(std::uint32_t& id_gen, int map_index) {
        int added = 0;
        for (const auto& [_, def] : m_defs) {
            if (def.map != map_index) {
                continue;
            }
            m_npcs.push_back(make_live_npc(id_gen, def));
            ++added;
        }
        if (added > 0) {
            log::info("NpcRegistry: appended " + std::to_string(added) + " NPCs for map " +
                      std::to_string(map_index));
        }
    }

    bool NpcRegistry::spawn_by_index(std::uint32_t& id_gen, int index) {
        const auto def = m_defs.find(index);
        if (def == m_defs.end()) {
            return false;
        }
        if (std::none_of(m_npcs.begin(), m_npcs.end(), [index](const auto& npc) {
                return npc.index == index;
            })) {
            m_npcs.push_back(make_live_npc(id_gen, def->second));
        }
        return true;
    }

    const NpcEntity* NpcRegistry::find(std::uint32_t id) const {
        for (const auto& n : m_npcs) {
            if (n.id == id) {
                return &n;
            }
        }
        return nullptr;
    }

    protocol::CreateNpcInfo NpcRegistry::to_create_info(const NpcEntity& npc) {
        protocol::CreateNpcInfo info;
        info.id = npc.id;
        info.index = static_cast<std::uint16_t>(npc.index);
        info.shape = static_cast<std::uint8_t>(npc.shape);
        info.x = npc.x;
        info.y = npc.y;
        info.z = npc.z;
        info.dir = npc.dir;
        info.gstate = 0;
        info.standard = 0;
        return info;
    }

}  // namespace openkal::server
