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

}  // namespace openkal::map
