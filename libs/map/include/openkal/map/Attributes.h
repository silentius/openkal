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

#include <cstdint>

namespace openkal::map {

    /** Attribute bit positions in the outdoor tile layout. */
    enum class AttrBit : int {
        kNotMoveable = 0,
        kPortal = 1,
        kVillage = 2,
        kSafety = 3,
        kAttackCastle = 4,
        kDefenceCastle = 5,
        kObject = 6,
    };

    /** Packs an outdoor tile flag as (0x8000 << bit).
     * @param bit Attribute bit to pack.
     * @return Mask with the corresponding high flag bit set.
     */
    [[nodiscard]] inline constexpr std::uint32_t attr_mask(AttrBit bit) {
        return static_cast<std::uint32_t>(0x8000u) << static_cast<unsigned>(bit);
    }

    /** High bit mask for not-moveable tiles. */
    inline constexpr std::uint32_t kAttrNotMoveable = attr_mask(AttrBit::kNotMoveable);

    /** High bit mask for portal tiles. */
    inline constexpr std::uint32_t kAttrPortal = attr_mask(AttrBit::kPortal);

    /** High bit mask for village tiles. */
    inline constexpr std::uint32_t kAttrVillage = attr_mask(AttrBit::kVillage);

    /** High bit mask for safety-zone tiles. */
    inline constexpr std::uint32_t kAttrSafety = attr_mask(AttrBit::kSafety);

    /** High bit mask for attack-castle tiles. */
    inline constexpr std::uint32_t kAttrAttackCastle = attr_mask(AttrBit::kAttackCastle);

    /** High bit mask for defence-castle tiles. */
    inline constexpr std::uint32_t kAttrDefenceCastle = attr_mask(AttrBit::kDefenceCastle);

    /** High bit mask for object tiles. */
    inline constexpr std::uint32_t kAttrObject = attr_mask(AttrBit::kObject);

    /** True if any bit in mask is set on attrs.
     * @param attrs Tile attribute dword.
     * @param mask Bits to test.
     * @return True if attrs & mask is non-zero.
     */
    [[nodiscard]] inline bool has_attr(std::uint32_t attrs, std::uint32_t mask) {
        return (attrs & mask) != 0;
    }

}  // namespace openkal::map
