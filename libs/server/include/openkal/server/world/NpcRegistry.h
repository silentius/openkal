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

#include "openkal/protocol/Quest.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace openkal::server {

    /** A server-authoritative destination offered by a teleport NPC. */
    struct NpcTeleportDestination {
        /** Destination logical map. */
        int map = 0;

        /** Destination server pixel X. */
        int x = 0;

        /** Destination server pixel Y. */
        int y = 0;
    };

    /** Static NPC definition loaded from config/npcs JSON. */
    struct NpcDef {
        /** GenNPC index. */
        int index = 0;

        /** Display name. */
        std::string name;

        /** NPC kind (3 = quest). */
        int kind = 0;

        /** Visual shape. */
        int shape = 0;

        /** Map index. */
        int map = 0;

        /** Optional html talk id (0 = none). */
        int html = 0;

        /** True when this NPC exposes shared account storage. */
        bool storage_keeper = false;
        /** True when this NPC permits trading-post collection. */
        bool trading_post_collector = false;

        /** Optional quest key [index, flag]. */
        int quest_index = 0;

        /** Optional quest flag. */
        int quest_flag = 0;

        /** Whether a quest key is configured. */
        bool has_quest = false;

        /** Teleport destinations configured by repeated NPC definition teleport entries. */
        std::vector<NpcTeleportDestination> teleports;

        /** Scenario monument slot, or -1 when the NPC has no scenario. */
        int scenario_slot = -1;

        /** Scenario item index, or zero when the scenario consumes no item. */
        int scenario_required_item = 0;

        /** Whether a scenario pair is configured. */
        bool has_scenario = false;

        /** Server pixel X. */
        int x = 0;

        /** Server pixel Y. */
        int y = 0;

        /** Server Z. */
        int z = 0;

        /** Direction word. */
        std::uint16_t dir = 0;
    };

    /** Live NPC entity streamed to clients. */
    struct NpcEntity {
        /** Runtime entity id. */
        std::uint32_t id = 0;

        /** Definition index. */
        int index = 0;

        /** Kind. */
        int kind = 0;

        /** Shape. */
        int shape = 0;

        /** Html id. */
        int html = 0;

        /** True when this live NPC exposes shared account storage. */
        bool storage_keeper = false;
        /** True when this NPC permits trading-post collection. */
        bool trading_post_collector = false;

        /** Quest key (0 = none). */
        std::uint32_t quest_key = 0;

        /** Teleport destinations configured for this live NPC. */
        std::vector<NpcTeleportDestination> teleports;

        /** Scenario monument slot, or -1 when the NPC has no scenario. */
        int scenario_slot = -1;

        /** Scenario item index, or zero when the scenario consumes no item. */
        int scenario_required_item = 0;

        /** Whether a scenario pair is configured. */
        bool has_scenario = false;

        /** Position. */
        int x = 0;

        /** Position. */
        int y = 0;

        /** Position. */
        int z = 0;

        /** Logical map. */
        int map = 0;

        /** Facing. */
        std::uint16_t dir = 0;
    };

    /** Loads NPC JSON and holds live NPC entities. */
    class NpcRegistry final {
      public:
        /**
         * Loads NPC definitions from a directory.
         * @param dir config/npcs directory.
         * @return True when at least one NPC loads.
         */
        [[nodiscard]] bool load_dir(const std::string& dir);

        /**
         * Spawns live entities for the given map from loaded defs.
         * @param id_gen Next entity id generator.
         * @param map_index Map filter (negative = all maps).
         */
        void spawn_for_map(std::uint32_t& id_gen, int map_index = 0);

        /**
         * Spawns live entities whose map is in `[min_map, max_map]`.
         * @param id_gen Next entity id generator.
         * @param min_map Inclusive minimum map index.
         * @param max_map Inclusive maximum map index.
         */
        void spawn_for_maps(std::uint32_t& id_gen, int min_map, int max_map);

        /**
         * Appends live entities for one map without clearing already spawned NPCs.
         * @param id_gen Next entity id generator.
         * @param map_index Map index to append.
         */
        void append_for_map(std::uint32_t& id_gen, int map_index);

        /** Adds a configured NPC irrespective of the regular map filters.
         * @param id_gen Next entity ID.
         * @param index Definition index.
         * @return Whether the NPC exists and is spawned.
         */
        [[nodiscard]] bool spawn_by_index(std::uint32_t& id_gen, int index);

        /**
         * Finds a live NPC by runtime id.
         * @param id Entity id.
         * @return Pointer or nullptr.
         */
        [[nodiscard]] const NpcEntity* find(std::uint32_t id) const;

        /**
         * Returns all live NPCs.
         * @return NPC list.
         */
        [[nodiscard]] const std::vector<NpcEntity>& npcs() const {
            return m_npcs;
        }

        /**
         * Builds a CreateNpcInfo for an entity.
         * @param npc Live NPC.
         * @return Protocol payload.
         */
        [[nodiscard]] static protocol::CreateNpcInfo to_create_info(const NpcEntity& npc);

      private:
        /** Loaded definitions by index. */
        std::unordered_map<int, NpcDef> m_defs;

        /** Live entities. */
        std::vector<NpcEntity> m_npcs;
    };

}  // namespace openkal::server
