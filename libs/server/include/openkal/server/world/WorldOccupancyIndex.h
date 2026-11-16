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
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace openkal::server {

    struct MonsterEntity;
    struct PlayerPose;

    /** Owns numeric collision-cell reservations for one world tick.
     * Rebuild before the tick and update after each monster changes its reservation.
     * Player positions remain fixed until the next rebuild. No entity pointers are retained.
     */
    class WorldOccupancyIndex final {
      public:
        /** Captured collision reservation before one monster update. */
        struct Reservation {
            std::uint32_t id = 0;  ///< Monster identity; duplicates are separate entries.
            int map = 0;           ///< Logical map.
            int cell_x = 0;        ///< Horizontal eight-pixel cell coordinate.
            int cell_y = 0;        ///< Vertical eight-pixel cell coordinate.
            bool blocks = false;   ///< Whether this living monster blocks collision.
        };

        /** Captures a monster's source or reserved movement destination.
         * @param monster Monster state to capture.
         * @return Numeric reservation with arithmetic-floor cell coordinates.
         */
        [[nodiscard]] static Reservation reservation(const MonsterEntity& monster);

        /** Replaces all reservations with the current tick's initial population.
         * @param monsters Source monster states, including nonblocking entities.
         * @param players Fixed player positions for this tick.
         */
        void rebuild(const std::vector<MonsterEntity>& monsters,
                     const std::vector<PlayerPose>& players);

        /** Updates one indexed monster without changing overlapping reservations.
         * @param before Reservation captured from this monster's previously indexed state.
         * @param after Monster state after its tick update.
         */
        void update(const Reservation& before, const MonsterEntity& after);

        /** Checks an exact eight-pixel collision cell.
         * @param self Monster identity excluded from every monster entry in the cell.
         * @param px Horizontal server pixel coordinate.
         * @param py Vertical server pixel coordinate.
         * @param map Logical map.
         * @param include_players Whether player positions also block the cell.
         * @return Whether another blocking monster or an included player occupies the cell.
         */
        [[nodiscard]] bool
        blocked(std::uint32_t self, int px, int py, int map, bool include_players) const;

      private:
        struct CellKey {
            int map;
            int x;
            int y;
            bool operator==(const CellKey& other) const;
        };

        struct CellHash {
            std::size_t operator()(const CellKey& key) const;
        };

        struct Cell {
            std::vector<std::uint32_t> monsters;
            std::size_t players = 0;
        };

        std::unordered_map<CellKey, Cell, CellHash> m_cells;
    };

}  // namespace openkal::server
