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
#include "openkal/platform/Json.h"

#include <cstdint>
#include <deque>
#include <optional>
#include <random>
#include <string>
#include <unordered_set>
#include <vector>
namespace openkal::map {
    class MapCatalog;
}
namespace openkal::server {
    class GameWorld;
    /** Identity, population and reward settings for one plant type. */
    struct HarvestTypeConfig {
        /** Unique type key from the configuration, such as aloe_vera. */
        std::string type;
        /** Monster definition used to render and interact with this plant. */
        int entity_index = 0;
        /** Stackable ingredient definition awarded on a successful harvest. */
        int item_index = 0;
        /** Number of ingredients awarded by each successful harvest. */
        int items_per_pick = 1;
        /** Enables random generation of this plant type. */
        bool enabled = false;
        /** World cap for this type, including slots waiting to regrow. */
        int max_plants = 256;
        /** Maximum maintained nodes in one outdoor area cell. */
        int plants_per_area = 8;
        /** Delay after a successful harvest before a slot regrows. */
        int respawn_seconds = 600;
    };
    /** Server-owned plant types; an empty list disables gathering populations. */
    struct HarvestConfig {
        /** Independently configured plant types. */
        std::vector<HarvestTypeConfig> types;

        /** Resolves plant identity regardless of whether random spawning is enabled.
         * @param entity_index Monster definition index.
         * @return Matching type, or nullptr when the entity is not a configured plant.
         */
        [[nodiscard]] const HarvestTypeConfig* find_type(int entity_index) const;
    };
    /** Parses strictly bounded plant settings.
     * @param value Settings object.
     * @return Settings or no value for invalid input.
     */
    [[nodiscard]] std::optional<HarvestConfig> parse_harvest_config(const platform::Json& value);

    /** Loads the shared embedded/standalone server defaults.
     * @param path Server plant settings JSON.
     * @return Settings or no value for missing/invalid input.
     */
    [[nodiscard]] std::optional<HarvestConfig> load_harvest_config(const std::string& path);

    /** Maintains authoritative outdoor gathering nodes and their regrowth timers. */
    class HarvestPlants {
      public:
        /** Creates an independently randomized population.
         * @param seed Random seed; a supplied seed permits reproducible tests.
         */
        explicit HarvestPlants(std::uint32_t seed = std::random_device{}());

        /** Gradually populates loaded outdoor areas and advances regrowth.
         * Placement work is bounded per update so network processing remains responsive.
         * @param world Authoritative world.
         * @param catalog Explicit outdoor/indoor area definitions.
         * @param config Population limits.
         * @param next_id Shared entity ID allocator.
         * @param dt Elapsed seconds.
         */
        void update(GameWorld& world,
                    const map::MapCatalog& catalog,
                    const HarvestConfig& config,
                    std::uint32_t& next_id,
                    float dt);

        /** Checks terrain masks and space around a gathering position.
         * @param world Authoritative world.
         * @param x Server X coordinate.
         * @param y Server Y coordinate.
         * @return True for open terrain outside villages, portals and objects.
         */
        [[nodiscard]] static bool valid_position(const GameWorld& world, int x, int y);

      private:
        struct Node {
            std::size_t type_index = 0;
            int map = 0;
            int cell_x = 0;
            int cell_y = 0;
            std::uint32_t id = 0;
            float remaining = 0;
        };
        struct Position {
            int map = 0;
            int x = 0;
            int y = 0;
        };
        struct PendingArea {
            std::size_t type_index = 0;
            int map = 0;
            int cell_x = 0;
            int cell_y = 0;
            int remaining = 0;
        };
        [[nodiscard]] bool spawn(GameWorld& world,
                                 Node& node,
                                 const HarvestTypeConfig& type,
                                 std::uint32_t& next_id,
                                 std::vector<Position>& positions);
        std::mt19937 m_random;
        std::vector<Node> m_nodes;
        std::unordered_set<int> m_areas;
        /** Initial population work carried across updates instead of blocking one tick. */
        std::deque<PendingArea> m_pending_areas;
    };
}  // namespace openkal::server
