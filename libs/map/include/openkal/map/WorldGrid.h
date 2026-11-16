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

#include "openkal/map/AreaCell.h"

#include <cstdint>
#include <unordered_map>

namespace openkal::map {

    /** Slot stride matching server runtime state[x+(y<<6)]. */
    inline constexpr int kWorldCellSlots = 64;

    /** Maps cell (cx, cy) to the server-style slot index.
     * @param cell_x Cell X.
     * @param cell_y Cell Y.
     * @return Slot index cell_x + (cell_y << 6).
     */
    [[nodiscard]] inline int cell_slot_index(int cell_x, int cell_y) {
        return cell_x + (cell_y << 6);
    }

    /** Multi-cell world index: cell (cx, cy) -> AreaCell. */
    class WorldGrid {
      public:
        /** Inserts or replaces the cell at (cell_x, cell_y).
         * @param cell_x Cell X.
         * @param cell_y Cell Y.
         * @param grid Cell data to store (coords overwritten to match slot).
         */
        void set_cell(int cell_x, int cell_y, AreaCell grid);

        /** Returns the cell at (cell_x, cell_y), or nullptr if missing.
         * @param cell_x Cell X.
         * @param cell_y Cell Y.
         * @return Pointer to the cell, or nullptr.
         */
        [[nodiscard]] const AreaCell* cell(int cell_x, int cell_y) const;

        /** Returns the mutable cell at (cell_x, cell_y), or nullptr if missing.
         * @param cell_x Cell X.
         * @param cell_y Cell Y.
         * @return Pointer to the cell, or nullptr.
         */
        [[nodiscard]] AreaCell* cell(int cell_x, int cell_y);

        /** Attribute at absolute world tile (pixel>>5). False if cell missing.
         * @param world_tx Absolute world tile X.
         * @param world_ty Absolute world tile Y.
         * @param out Receives the attribute dword on success.
         * @return True if the cell exists.
         */
        [[nodiscard]] bool attr_at_world_tile(int world_tx, int world_ty, std::uint32_t& out) const;

        /** Tile-level walkable check (high SM_ATTR_NOTMOVEABLE bit).
         * @param world_tx Absolute world tile X.
         * @param world_ty Absolute world tile Y.
         * @return True if walkable; false if blocked or cell missing.
         */
        [[nodiscard]] bool walkable_world_tile(int world_tx, int world_ty) const;

        /** Pixel-level walkable check via Blocked and NotMoveable subcell masks.
         * @param px Server pixel X.
         * @param py Server pixel Y.
         * @return True if the subcell is walkable; false if blocked or cell missing.
         */
        [[nodiscard]] bool walkable_world_pixel(int px, int py) const;

        /** True if the NotMoveable mask covers a world pixel.
         * @param px Server pixel X.
         * @param py Server pixel Y.
         * @return True if the subcell is not moveable; false if the cell is missing.
         */
        [[nodiscard]] bool not_moveable_world_pixel(int px, int py) const;

        /** True if world pixel attrs include any bit from mask.
         * @param px Server pixel X.
         * @param py Server pixel Y.
         * @param mask Bits to test.
         * @return True if mask matches; false if cell missing.
         */
        [[nodiscard]] bool has_attr_world_pixel(int px, int py, std::uint32_t mask) const;

        /** Number of loaded cells.
         * @return Cell count.
         */
        [[nodiscard]] std::size_t cell_count() const {
            return m_cells.size();
        }

      private:
        /** Slot index -> cell grid. */
        std::unordered_map<int, AreaCell> m_cells;
    };

}  // namespace openkal::map
