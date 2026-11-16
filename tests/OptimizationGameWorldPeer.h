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

#include "openkal/server/world/GameWorld.h"
#include "openkal/server/world/WorldOccupancyIndex.h"

#include <utility>

namespace openkal::server {
    /** Test access to unchanged predicates and owned combat catalog inputs. */
    class GameWorldCoveragePeer {
      public:
        /** Uses a borrowed occupancy snapshot only during one test callback.
         * @param world World under test.
         * @param index Current numeric reservation snapshot.
         * @param players Player vector represented by the snapshot.
         * @param callback Query-only operation; source containers must remain unchanged.
         * @return Callback result.
         */
        template <typename Callback>
        static decltype(auto) with_occupancy(GameWorld& world,
                                             WorldOccupancyIndex& index,
                                             const std::vector<PlayerPose>& players,
                                             Callback&& callback) {
            struct Restore {
                WorldOccupancyIndex*& index_slot;
                const std::vector<PlayerPose>*& players_slot;
                WorldOccupancyIndex* previous_index;
                const std::vector<PlayerPose>* previous_players;
                ~Restore() {
                    index_slot = previous_index;
                    players_slot = previous_players;
                }
            } restore{world.m_tick_occupancy,
                      world.m_tick_players,
                      world.m_tick_occupancy,
                      world.m_tick_players};
            world.m_tick_occupancy = &index;
            world.m_tick_players = &players;
            return std::forward<Callback>(callback)();
        }

        /** Selects indexed or full-scan world ticks for differential tests.
         * @param world World under test.
         * @param enabled Whether tick-local occupancy snapshots are enabled.
         */
        static void enable_occupancy_index(GameWorld& world, bool enabled) {
            world.m_use_occupancy_index = enabled;
        }

        /** Checks that no tick-local borrowed occupancy state remains active.
         * @param world World under test.
         * @return Whether all borrowed query context is clear.
         */
        static bool occupancy_inactive(const GameWorld& world) {
            return !world.m_tick_occupancy && !world.m_tick_players &&
                   !world.m_tick_occupancy_scope;
        }

        /** Queries the existing occupancy predicate.
         * @param world World under test.
         * @param self Entity excluded from collisions.
         * @param x Query pixel X.
         * @param y Query pixel Y.
         * @param map Logical map.
         * @param players Optional player poses.
         * @return Whether another body occupies the cell.
         */
        static bool blocked(const GameWorld& world,
                            std::uint32_t self,
                            int x,
                            int y,
                            int map,
                            const std::vector<PlayerPose>* players) {
            return world.occupancy_blocked(self, x, y, map, players);
        }

        /** Queries the existing sampled movement predicate.
         * @param world World under test.
         * @param self Entity excluded from collisions.
         * @param x Start pixel X.
         * @param y Start pixel Y.
         * @param dx Pixel X displacement.
         * @param dy Pixel Y displacement.
         * @param map Logical map.
         * @param players Player poses.
         * @return Whether every sampled destination is free.
         */
        static bool move(const GameWorld& world,
                         std::uint32_t self,
                         int x,
                         int y,
                         int dx,
                         int dy,
                         int map,
                         const std::vector<PlayerPose>& players) {
            return world.is_move_tile(self, x, y, dx, dy, map, players);
        }

        /** Reads the existing target reservation.
         * @param monster Monster under test.
         * @return Reserved pixel coordinates.
         */
        static std::pair<int, int> reservation(const MonsterEntity& monster) {
            int x = 0, y = 0;
            GameWorld::occupy_pixel(monster, x, y);
            return {x, y};
        }

        /** Installs an independently authored item value for calculator tests.
         * @param world World under test.
         * @param item Owned synthetic definition.
         */
        static void add_item(GameWorld& world, ItemCombatDef item) {
            world.m_item_defs[item.index] = std::move(item);
        }

        /** Installs an independently authored prefix value for calculator tests.
         * @param world World under test.
         * @param prefix Owned synthetic definition.
         */
        static void add_prefix(GameWorld& world, PrefixCombatDef prefix) {
            world.m_prefix_defs[prefix.index] = std::move(prefix);
        }

        /** Installs an independently authored riding value for calculator tests.
         * @param world World under test.
         * @param index Item definition index.
         * @param riding Owned synthetic definition.
         */
        static void add_riding(GameWorld& world, int index, RidingDef riding) {
            world.m_riding_defs[index] = std::move(riding);
        }

        /** Installs an independently authored pet value for scheduled-player tests.
         * @param world World under test.
         * @param pet Owned synthetic definition.
         */
        static void add_pet(GameWorld& world, PetDef pet) {
            world.m_pet_defs[pet.item_index] = std::move(pet);
        }
    };
}  // namespace openkal::server
