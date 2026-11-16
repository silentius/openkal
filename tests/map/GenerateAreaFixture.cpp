/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/map/AreaMapCodec.h"
#include "openkal/map/Attributes.h"

#include <array>
#include <cstdint>
#include <cstdio>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "Usage: %s OUTPUT.akarea\n", argv[0]);
        return 1;
    }

    openkal::map::AreaCell cell;
    cell.set_cell_x(0);
    cell.set_cell_y(0);
    cell.ensure_full_attrs();
    cell.ensure_blocked_mask(false);
    cell.ensure_not_moveable_mask(false);

    const std::array<std::uint32_t, 6> flags = {openkal::map::kAttrPortal,
                                                openkal::map::kAttrVillage,
                                                openkal::map::kAttrSafety,
                                                openkal::map::kAttrAttackCastle,
                                                openkal::map::kAttrDefenceCastle,
                                                openkal::map::kAttrObject};
    for (int x = 1; x <= static_cast<int>(flags.size()); ++x) {
        cell.attr_ref(x, 1) = flags[static_cast<std::size_t>(x - 1)];
    }
    cell.attr_ref(7, 1) =
        openkal::map::kAttrPortal | openkal::map::kAttrVillage | openkal::map::kAttrSafety;

    cell.set_blocked_mask(2, 4, 0xffffu);
    cell.set_not_moveable_mask(3, 4, 0xffffu);
    cell.set_blocked_mask(4, 4, 0x8421u);
    cell.set_not_moveable_mask(4, 4, 0x1248u);
    cell.set_blocked_mask(5, 4, 0xffffu);
    cell.set_not_moveable_mask(5, 4, 0xffffu);

    for (int bit = 0; bit < 16; ++bit) {
        const auto mask = static_cast<openkal::map::AreaCell::SubcellMask>(1u << bit);
        cell.set_blocked_mask(16 + bit, 8, mask);
        cell.set_not_moveable_mask(16 + bit, 9, mask);
    }

    for (int y = 64; y < 68; ++y) {
        for (int x = 64; x < 96; ++x) {
            cell.set_blocked_mask(x, y, 0xffffu);
            cell.set_not_moveable_mask(x + 32, y, 0xffffu);
        }
    }

    cell.attr_ref(255, 255) = openkal::map::kAttrPortal | openkal::map::kAttrObject;
    cell.set_blocked_mask(255, 255, 0x8001u);
    cell.set_not_moveable_mask(255, 255, 0x0180u);

    if (!openkal::map::AreaMapCodec::save(argv[1], cell)) {
        return 1;
    }
    std::printf("Wrote synthetic area fixture: %s\n", argv[1]);
    return 0;
}
