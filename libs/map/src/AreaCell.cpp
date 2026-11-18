/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/map/AreaCell.h"

namespace openkal::map {
    namespace {

        constexpr std::size_t kAreaTileCount = static_cast<std::size_t>(kTileCount) * kTileCount;

        [[nodiscard]] std::size_t tile_index(int tx, int ty) {
            return static_cast<std::size_t>(ty) * kTileCount + static_cast<std::size_t>(tx);
        }

        constexpr int kMovementScale = kCellSize / kMovementCellSize;
        static_assert(kMovementScale == 2);
        constexpr AreaCell::MovementMask kMovementBlock = 0x0303u;

        [[nodiscard]] unsigned movement_block_shift(unsigned x, unsigned y) {
            return kMovementScale * (x + kMovementCellsPerTile * y);
        }

        [[nodiscard]] AreaCell::MovementMask expand_movement_mask(AreaCell::SubcellMask coarse) {
            if (coarse == 0xffffu) {
                return ~AreaCell::MovementMask{0};
            }
            AreaCell::MovementMask fine = 0;
            for (unsigned y = 0; y < kCellsPerTile && coarse != 0; ++y) {
                for (unsigned x = 0; x < kCellsPerTile; ++x) {
                    if ((coarse & (1u << (y * kCellsPerTile + x))) != 0) {
                        fine |= kMovementBlock << movement_block_shift(x, y);
                    }
                }
            }
            return fine;
        }

        [[nodiscard]] AreaCell::SubcellMask coarse_movement_mask(AreaCell::MovementMask fine) {
            if (fine == ~AreaCell::MovementMask{0}) {
                return 0xffffu;
            }
            AreaCell::SubcellMask coarse = 0;
            for (unsigned y = 0; y < kCellsPerTile && fine != 0; ++y) {
                for (unsigned x = 0; x < kCellsPerTile; ++x) {
                    if ((fine & (kMovementBlock << movement_block_shift(x, y))) != 0) {
                        coarse |= AreaCell::SubcellMask{1} << (y * kCellsPerTile + x);
                    }
                }
            }
            return coarse;
        }

    }  // namespace

    std::uint32_t AreaCell::at(int tx, int ty) const {
        if (tx < 0 || ty < 0 || tx >= kTileCount || ty >= kTileCount || m_attrs.empty()) {
            return kAttrNotMoveable;
        }
        return m_attrs[tile_index(tx, ty)];
    }

    bool AreaCell::walkable(int tx, int ty) const {
        return !has_attr(at(tx, ty), kAttrNotMoveable);
    }

    bool AreaCell::has_flag(int tx, int ty, std::uint32_t mask) const {
        return has_attr(at(tx, ty), mask);
    }

    void AreaCell::ensure_blocked_mask(bool fill) {
        if (m_blocked.size() != kAreaTileCount) {
            m_blocked.assign(kAreaTileCount, fill ? SubcellMask{0xffffu} : SubcellMask{0});
        }
    }

    AreaCell::SubcellMask AreaCell::blocked_mask(int tx, int ty) const {
        if (tx < 0 || ty < 0 || tx >= kTileCount || ty >= kTileCount ||
            m_blocked.size() != kAreaTileCount) {
            return 0xffffu;
        }
        return m_blocked[tile_index(tx, ty)];
    }

    void AreaCell::set_blocked_mask(int tx, int ty, SubcellMask mask) {
        if (tx < 0 || ty < 0 || tx >= kTileCount || ty >= kTileCount) {
            return;
        }
        ensure_blocked_mask();
        m_blocked[tile_index(tx, ty)] = mask;
    }

    void AreaCell::ensure_not_moveable_mask(bool fill) {
        if (m_not_moveable.size() != kAreaTileCount) {
            m_not_moveable.assign(kAreaTileCount, fill ? SubcellMask{0xffffu} : SubcellMask{0});
        }
    }

    AreaCell::SubcellMask AreaCell::not_moveable_mask(int tx, int ty) const {
        if (tx < 0 || ty < 0 || tx >= kTileCount || ty >= kTileCount ||
            m_not_moveable.size() != kAreaTileCount) {
            return 0xffffu;
        }
        return m_not_moveable[tile_index(tx, ty)];
    }

    void AreaCell::set_not_moveable_mask(int tx, int ty, SubcellMask mask) {
        if (tx < 0 || ty < 0 || tx >= kTileCount || ty >= kTileCount) {
            return;
        }
        ensure_full_attrs(0u);
        ensure_not_moveable_mask();
        const std::size_t index = tile_index(tx, ty);
        m_not_moveable[index] = mask;
        if (!m_movement_masks.empty()) {
            m_movement_masks[index] = expand_movement_mask(mask);
        }
        if (mask != 0) {
            m_attrs[index] |= kAttrNotMoveable;
        } else {
            m_attrs[index] &= ~kAttrNotMoveable;
        }
    }

    bool AreaCell::blocked_at_subcell(int sx, int sy) const {
        constexpr int kSubcellCount = kTileCount * kSubcellsPerTile;
        if (sx < 0 || sy < 0 || sx >= kSubcellCount || sy >= kSubcellCount) {
            return true;
        }
        const int tx = sx / kSubcellsPerTile;
        const int ty = sy / kSubcellsPerTile;
        if (m_blocked.size() != kAreaTileCount) {
            const unsigned legacy_bit = static_cast<unsigned>(sx % kSubcellsPerTile) +
                                        4u * static_cast<unsigned>(sy % kSubcellsPerTile);
            return (at(tx, ty) & (std::uint32_t{1} << legacy_bit)) != 0;
        }
        const unsigned bit = static_cast<unsigned>((sx % kSubcellsPerTile) +
                                                   kSubcellsPerTile * (sy % kSubcellsPerTile));
        return (blocked_mask(tx, ty) & (SubcellMask{1} << bit)) != 0;
    }

    void AreaCell::set_blocked_at_subcell(int sx, int sy, bool blocked) {
        constexpr int kSubcellCount = kTileCount * kSubcellsPerTile;
        if (sx < 0 || sy < 0 || sx >= kSubcellCount || sy >= kSubcellCount) {
            return;
        }
        const int tx = sx / kSubcellsPerTile;
        const int ty = sy / kSubcellsPerTile;
        const unsigned bit = static_cast<unsigned>((sx % kSubcellsPerTile) +
                                                   kSubcellsPerTile * (sy % kSubcellsPerTile));
        SubcellMask mask = blocked_mask(tx, ty);
        if (blocked) {
            mask |= SubcellMask{1} << bit;
        } else {
            mask &= static_cast<SubcellMask>(~(SubcellMask{1} << bit));
        }
        set_blocked_mask(tx, ty, mask);
    }

    bool AreaCell::not_moveable_at_subcell(int sx, int sy) const {
        constexpr int kSubcellCount = kTileCount * kSubcellsPerTile;
        if (sx < 0 || sy < 0 || sx >= kSubcellCount || sy >= kSubcellCount) {
            return true;
        }
        const int tx = sx / kSubcellsPerTile;
        const int ty = sy / kSubcellsPerTile;
        if (m_not_moveable.size() != kAreaTileCount) {
            return has_attr(at(tx, ty), kAttrNotMoveable);
        }
        const unsigned bit = static_cast<unsigned>((sx % kSubcellsPerTile) +
                                                   kSubcellsPerTile * (sy % kSubcellsPerTile));
        return (not_moveable_mask(tx, ty) & (SubcellMask{1} << bit)) != 0;
    }

    void AreaCell::set_not_moveable_at_subcell(int sx, int sy, bool not_moveable) {
        constexpr int kSubcellCount = kTileCount * kSubcellsPerTile;
        if (sx < 0 || sy < 0 || sx >= kSubcellCount || sy >= kSubcellCount) {
            return;
        }
        const int tx = sx / kSubcellsPerTile;
        const int ty = sy / kSubcellsPerTile;
        if (!m_movement_masks.empty()) {
            const auto block = kMovementBlock << movement_block_shift(sx % kSubcellsPerTile,
                                                                      sy % kSubcellsPerTile);
            const auto current = movement_mask(tx, ty);
            set_movement_mask(tx, ty, not_moveable ? current | block : current & ~block);
            return;
        }
        const unsigned bit = static_cast<unsigned>((sx % kSubcellsPerTile) +
                                                   kSubcellsPerTile * (sy % kSubcellsPerTile));
        SubcellMask mask = not_moveable_mask(tx, ty);
        if (not_moveable) {
            mask |= SubcellMask{1} << bit;
        } else {
            mask &= static_cast<SubcellMask>(~(SubcellMask{1} << bit));
        }
        set_not_moveable_mask(tx, ty, mask);
    }

    void AreaCell::import_legacy_blocked_masks() {
        m_movement_masks.clear();
        ensure_blocked_mask();
        ensure_not_moveable_mask();
        for (int ty = 0; ty < kTileCount; ++ty) {
            for (int tx = 0; tx < kTileCount; ++tx) {
                const std::uint16_t legacy = static_cast<std::uint16_t>(at(tx, ty));
                const std::size_t index = tile_index(tx, ty);
                m_blocked[index] = legacy;
                // MMap::GetSvMap uses all 16 low bits for collision; bit 15
                // only blocks subcell (3, 3). The independent OpenKal layer
                // has no legacy equivalent and must not expand that corner.
                m_not_moveable[index] = 0u;
                m_attrs[index] &= 0xffff0000u;
            }
        }
    }

    void AreaCell::set_not_moveable_masks(std::vector<SubcellMask> masks) {
        m_movement_masks.clear();
        m_not_moveable = std::move(masks);
        if (m_not_moveable.size() != kAreaTileCount || m_attrs.size() != kAreaTileCount) {
            return;
        }
        for (std::size_t index = 0; index < kAreaTileCount; ++index) {
            if (m_not_moveable[index] != 0) {
                m_attrs[index] |= kAttrNotMoveable;
            } else {
                m_attrs[index] &= ~kAttrNotMoveable;
            }
        }
    }

    AreaCell::MovementMask AreaCell::movement_mask(int tx, int ty) const {
        if (tx < 0 || ty < 0 || tx >= kTileCount || ty >= kTileCount) {
            return ~MovementMask{0};
        }
        if (m_not_moveable.size() != kAreaTileCount) {
            return has_attr(at(tx, ty), kAttrNotMoveable) ? ~MovementMask{0} : 0;
        }
        const auto index = tile_index(tx, ty);
        return m_movement_masks.empty() ? expand_movement_mask(m_not_moveable[index])
                                        : m_movement_masks[index];
    }

    void AreaCell::set_movement_mask(int tx, int ty, MovementMask mask) {
        if (tx < 0 || ty < 0 || tx >= kTileCount || ty >= kTileCount) {
            return;
        }
        const auto coarse = coarse_movement_mask(mask);
        if (m_movement_masks.empty() && expand_movement_mask(coarse) != mask) {
            ensure_not_moveable_mask();
            m_movement_masks.resize(kAreaTileCount);
            std::transform(m_not_moveable.begin(),
                           m_not_moveable.end(),
                           m_movement_masks.begin(),
                           expand_movement_mask);
        }
        set_not_moveable_mask(tx, ty, coarse);
        if (!m_movement_masks.empty()) {
            m_movement_masks[tile_index(tx, ty)] = mask;
        }
    }

    bool AreaCell::not_moveable_at_movement_cell(int sx, int sy) const {
        if (sx < 0 || sy < 0 || sx >= kMovementCellCount || sy >= kMovementCellCount) {
            return true;
        }
        if (m_movement_masks.empty()) {
            return not_moveable_at_subcell(sx / kMovementScale, sy / kMovementScale);
        }
        const int bit =
            sx % kMovementCellsPerTile + kMovementCellsPerTile * (sy % kMovementCellsPerTile);
        return (movement_mask(sx / kMovementCellsPerTile, sy / kMovementCellsPerTile) &
                (MovementMask{1} << bit)) != 0;
    }

    void AreaCell::set_not_moveable_at_movement_cell(int sx, int sy, bool not_moveable) {
        if (sx < 0 || sy < 0 || sx >= kMovementCellCount || sy >= kMovementCellCount) {
            return;
        }
        const int tx = sx / kMovementCellsPerTile;
        const int ty = sy / kMovementCellsPerTile;
        const int bit =
            sx % kMovementCellsPerTile + kMovementCellsPerTile * (sy % kMovementCellsPerTile);
        const auto current = movement_mask(tx, ty);
        const auto changed = MovementMask{1} << bit;
        set_movement_mask(tx, ty, not_moveable ? current | changed : current & ~changed);
    }

    void AreaCell::set_movement_masks(std::vector<MovementMask> masks) {
        if (masks.size() != kAreaTileCount) {
            return;
        }
        std::vector<SubcellMask> coarse(kAreaTileCount);
        std::transform(masks.begin(), masks.end(), coarse.begin(), coarse_movement_mask);
        set_not_moveable_masks(std::move(coarse));
        m_movement_masks = std::move(masks);
    }

    bool AreaCell::has_movement_detail() const {
        for (std::size_t index = 0; index < m_movement_masks.size(); ++index) {
            if (m_movement_masks[index] != expand_movement_mask(m_not_moveable[index])) {
                return true;
            }
        }
        return false;
    }

}  // namespace openkal::map
