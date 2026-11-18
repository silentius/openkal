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

#include "openkal/map/Attributes.h"
#include "openkal/map/Constants.h"

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

namespace openkal::map {

    /** Area data for one 256 by 256 world-tile cell.
     *
     * Tile attributes hold named zone flags. Blocked uses four by four subcells
     * per tile. The independent movement layer can resolve eight by eight subcells.
     */
    class AreaCell {
      public:
        /** Number of Blocked and legacy NotMoveable subcells on one tile axis. */
        inline static constexpr int kSubcellsPerTile = 4;

        static_assert(kTileSize % kSubcellsPerTile == 0,
                      "area subcells must divide a world tile evenly");

        /** Packed four by four layer mask for one tile. */
        using SubcellMask = std::uint16_t;

        /** Constructs an empty area cell. */
        AreaCell() = default;

        /** Returns the world cell X index.
         * @return World cell X.
         */
        [[nodiscard]] int cell_x() const {
            return m_cell_x;
        }

        /** Sets the world cell X index.
         * @param value World cell X.
         */
        void set_cell_x(int value) {
            m_cell_x = value;
        }

        /** Returns the world cell Y index.
         * @return World cell Y.
         */
        [[nodiscard]] int cell_y() const {
            return m_cell_y;
        }

        /** Sets the world cell Y index.
         * @param value World cell Y.
         */
        void set_cell_y(int value) {
            m_cell_y = value;
        }

        /** Returns the legacy source version retained during import.
         * @return Imported source version.
         */
        [[nodiscard]] std::int32_t version() const {
            return m_legacy_version;
        }

        /** Sets the legacy source version retained during import.
         * @param value Imported source version.
         */
        void set_version(std::int32_t value) {
            m_legacy_version = value;
        }

        /** Number of stored tile attributes.
         * @return Attribute count.
         */
        [[nodiscard]] std::size_t attr_count() const {
            return m_attrs.size();
        }

        /** True if no tile attributes have been stored.
         * @return True when no attributes are stored.
         */
        [[nodiscard]] bool empty() const {
            return m_attrs.empty();
        }

        /** Replaces tile attributes with count copies of fill.
         * @param count New attribute count.
         * @param fill Fill value for each tile.
         */
        void assign_attrs(std::size_t count, std::uint32_t fill = 0) {
            m_attrs.assign(count, fill);
        }

        /** Ensures a full 256 by 256 tile-attribute grid.
         * @param fill Fill value when resizing.
         */
        void ensure_full_attrs(std::uint32_t fill = 0) {
            const std::size_t want = static_cast<std::size_t>(kTileCount) * kTileCount;
            if (m_attrs.size() != want) {
                m_attrs.assign(want, fill);
            }
        }

        /** Contiguous tile-attribute buffer for importers.
         * @return Pointer to the first attribute dword.
         */
        [[nodiscard]] std::uint32_t* attrs_data() {
            return m_attrs.data();
        }

        /** Contiguous tile-attribute buffer for readers.
         * @return Pointer to the first attribute dword.
         */
        [[nodiscard]] const std::uint32_t* attrs_data() const {
            return m_attrs.data();
        }

        /** Attribute at a flat row-major tile index.
         * @param index Flat attribute index.
         * @return Attribute dword.
         */
        [[nodiscard]] std::uint32_t attr_at_flat(std::size_t index) const {
            return m_attrs[index];
        }

        /** Sets an attribute at a flat row-major tile index.
         * @param index Flat attribute index.
         * @param value Attribute dword.
         */
        void set_attr_flat(std::size_t index, std::uint32_t value) {
            m_attrs[index] = value;
        }

        /** Mutable reference to a local tile attribute.
         * @param tx Local tile X.
         * @param ty Local tile Y.
         * @return Reference to the tile attribute.
         */
        [[nodiscard]] std::uint32_t& attr_ref(int tx, int ty) {
            return m_attrs[static_cast<std::size_t>(ty) * kTileCount +
                           static_cast<std::size_t>(tx)];
        }

        /** True when tile attributes are equal.
         * @param other Other area cell.
         * @return True if attributes match.
         */
        [[nodiscard]] bool attrs_equal(const AreaCell& other) const {
            return m_attrs == other.m_attrs;
        }

        /** Attribute at a local tile; missing tiles are not-moveable.
         * @param tx Local tile X.
         * @param ty Local tile Y.
         * @return Attribute dword at the tile.
         */
        [[nodiscard]] std::uint32_t at(int tx, int ty) const;

        /** True if a local tile permits movement at tile granularity.
         * @param tx Local tile X.
         * @param ty Local tile Y.
         * @return True if the tile has no not-moveable flag.
         */
        [[nodiscard]] bool walkable(int tx, int ty) const;

        /** True if a local tile contains any bits from mask.
         * @param tx Local tile X.
         * @param ty Local tile Y.
         * @param mask Bits to test.
         * @return True when any requested flag is present.
         */
        [[nodiscard]] bool has_flag(int tx, int ty, std::uint32_t mask) const;

        /** Allocates a full high-resolution blocked mask.
         * @param fill Blocked value for every subcell.
         */
        void ensure_blocked_mask(bool fill = false);

        /** Returns the packed four by four blocked mask for one tile.
         * @param tx Local tile X.
         * @param ty Local tile Y.
         * @return Packed mask, or all bits set outside the cell.
         */
        [[nodiscard]] SubcellMask blocked_mask(int tx, int ty) const;

        /** Sets the packed four by four blocked mask for one tile.
         * @param tx Local tile X.
         * @param ty Local tile Y.
         * @param mask Packed blocked mask.
         */
        void set_blocked_mask(int tx, int ty, SubcellMask mask);

        /** Allocates a full four by four not-moveable mask.
         * @param fill Not-moveable value for every subcell.
         */
        void ensure_not_moveable_mask(bool fill = false);

        /** Returns the packed four-by-four summary of restricted movement in one tile.
         * @param tx Local tile X.
         * @param ty Local tile Y.
         * @return One bit per eight-pixel subcell containing a restriction.
         */
        [[nodiscard]] SubcellMask not_moveable_mask(int tx, int ty) const;

        /** Sets the packed four by four not-moveable mask for one tile.
         * @param tx Local tile X.
         * @param ty Local tile Y.
         * @param mask Packed not-moveable mask.
         */
        void set_not_moveable_mask(int tx, int ty, SubcellMask mask);

        /** True if a local high-resolution subcell is blocked.
         * @param sx Local subcell X.
         * @param sy Local subcell Y.
         * @return True when blocked or outside the cell.
         */
        [[nodiscard]] bool blocked_at_subcell(int sx, int sy) const;

        /** Sets whether a local high-resolution subcell is blocked.
         * @param sx Local subcell X.
         * @param sy Local subcell Y.
         * @param blocked New blocked value.
         */
        void set_blocked_at_subcell(int sx, int sy, bool blocked);

        /** True if a local subcell has the not-moveable layer.
         * @param sx Local subcell X.
         * @param sy Local subcell Y.
         * @return True when not moveable or outside the cell.
         */
        [[nodiscard]] bool not_moveable_at_subcell(int sx, int sy) const;

        /** Sets whether a local subcell has the not-moveable layer.
         * @param sx Local subcell X.
         * @param sy Local subcell Y.
         * @param not_moveable New not-moveable value.
         */
        void set_not_moveable_at_subcell(int sx, int sy, bool not_moveable);

        /** Imports all 16 legacy collision bits and clears the independent NotMoveable layer. */
        void import_legacy_blocked_masks();

        /** Returns the packed blocked-mask array.
         * @return One four-by-four mask for each tile.
         */
        [[nodiscard]] const std::vector<SubcellMask>& blocked_masks() const {
            return m_blocked;
        }

        /** Replaces the full packed blocked-mask array.
         * @param masks One four-by-four mask for each tile.
         */
        void set_blocked_masks(std::vector<SubcellMask> masks) {
            m_blocked = std::move(masks);
        }

        /** Returns the packed not-moveable-mask array.
         * @return One four-by-four mask for each tile.
         */
        [[nodiscard]] const std::vector<SubcellMask>& not_moveable_masks() const {
            return m_not_moveable;
        }

        /** Replaces the full packed not-moveable-mask array.
         * @param masks One four-by-four mask for each tile.
         */
        void set_not_moveable_masks(std::vector<SubcellMask> masks);

        /** Packed eight-by-eight player-movement mask for one tile. */
        using MovementMask = std::uint64_t;

        /** Reads a movement mask at four-pixel precision.
         * @param tx Local tile X.
         * @param ty Local tile Y.
         * @return Packed mask, or all bits set outside the cell.
         */
        [[nodiscard]] MovementMask movement_mask(int tx, int ty) const;

        /** Replaces one tile's four-pixel movement mask.
         * @param tx Local tile X.
         * @param ty Local tile Y.
         * @param mask Packed movement mask.
         */
        void set_movement_mask(int tx, int ty, MovementMask mask);

        /** Tests a four-pixel movement cell.
         * @param sx Local movement cell X.
         * @param sy Local movement cell Y.
         * @return True when blocked for movement or outside the map cell.
         */
        [[nodiscard]] bool not_moveable_at_movement_cell(int sx, int sy) const;

        /** Changes one four-pixel movement cell.
         * @param sx Local movement cell X.
         * @param sy Local movement cell Y.
         * @param not_moveable New movement restriction value.
         */
        void set_not_moveable_at_movement_cell(int sx, int sy, bool not_moveable);

        /** Returns the full movement array when four-pixel masks have been loaded or edited.
         * @return One mask per tile, or an empty vector when using coarse masks.
         */
        [[nodiscard]] const std::vector<MovementMask>& movement_masks() const {
            return m_movement_masks;
        }

        /** Replaces all four-pixel movement masks and rebuilds the coarse summary.
         * @param masks One eight-by-eight mask for each tile.
         */
        void set_movement_masks(std::vector<MovementMask> masks);

        /** Checks whether saving at the original resolution would lose information.
         * @return True if any four-pixel cells differ inside an eight-pixel subcell.
         */
        [[nodiscard]] bool has_movement_detail() const;

        /** Calls fn(tx, ty, attr) for each tile in an inclusive local rectangle.
         * @param x0 Rectangle X start.
         * @param y0 Rectangle Y start.
         * @param x1 Rectangle X end.
         * @param y1 Rectangle Y end.
         * @param fn Callback invoked per tile.
         */
        template <typename Fn>
        void for_each_in_rect(int x0, int y0, int x1, int y1, Fn&& fn) const {
            if (m_attrs.empty()) {
                return;
            }
            const int xa = std::max(0, std::min(x0, x1));
            const int xb = std::min(kTileCount - 1, std::max(x0, x1));
            const int ya = std::max(0, std::min(y0, y1));
            const int yb = std::min(kTileCount - 1, std::max(y0, y1));
            for (int ty = ya; ty <= yb; ++ty) {
                for (int tx = xa; tx <= xb; ++tx) {
                    fn(tx, ty, at(tx, ty));
                }
            }
        }

      private:
        int m_cell_x = 0;
        int m_cell_y = 0;
        std::int32_t m_legacy_version = kVersion;
        std::vector<std::uint32_t> m_attrs;
        std::vector<SubcellMask> m_blocked;
        std::vector<SubcellMask> m_not_moveable;
        std::vector<MovementMask> m_movement_masks;
    };

}  // namespace openkal::map
