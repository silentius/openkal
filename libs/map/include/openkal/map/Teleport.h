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

namespace openkal::map {

    /** Project-facing city-scroll destination. */
    struct TeleportLoc {
        /** Destination map id. */
        int map = 0;

        /** Destination server X. */
        int x = 0;

        /** Destination server Y. */
        int y = 0;

        /** Destination server Z. */
        int z = 0;

        /** Display name. */
        const char* name = "";
    };

    /** City teleport table; specialty keys 1..N map to indices 0..N-1. */
    inline constexpr TeleportLoc kTeleportLocs[] = {
        {0, 258043, 259381, 16049, "Narootuh"},
        {0, 265022, 262538, 16190, "Jook-Suh Cargo Station"},
        {0, 266040, 284976, 22650, "Geum-Oh Mine"},
        {0, 255825, 288802, 27050, "Pub of the Giant Bird"},
        {0, 267618, 242946, 19630, "Temporary Fort of Geum-Ohee Castle"},
        {0, 232954, 294672, 45300, "City of Priest"},
    };

    /** Number of entries in kTeleportLocs. */
    inline constexpr std::size_t kTeleportLocCount =
        sizeof(kTeleportLocs) / sizeof(kTeleportLocs[0]);

    /** Returns the teleport location at index, or nullptr if out of range.
     * @param index Zero-based table index.
     * @return Pointer to the entry, or nullptr.
     */
    inline const TeleportLoc* teleport_loc(std::size_t index) {
        if (index >= kTeleportLocCount) {
            return nullptr;
        }
        return &kTeleportLocs[index];
    }

}  // namespace openkal::map
