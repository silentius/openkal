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
#include <unordered_set>
#include <vector>

namespace openkal::client {
    /** One eligible remote player, ranked by selection priority and squared distance. */
    struct PlayerRenderCandidate {
        std::uint32_t id = 0;
        float distance_squared = 0.0f;
        bool selected = false;
        /** Own-party members are always included without consuming the non-party budget. */
        bool party_member = false;
    };

    /** Selects a deterministic, nearest-first remote player budget.
     * @param candidates Eligible players, with target priority and own-party membership.
     * @param limit Non-party count; zero includes all and negative includes only own-party members.
     * @return Selected entity ids. Equal distances are ordered by entity id.
     */
    [[nodiscard]] std::unordered_set<std::uint32_t>
    select_player_ids(std::vector<PlayerRenderCandidate> candidates, int limit);
}  // namespace openkal::client
