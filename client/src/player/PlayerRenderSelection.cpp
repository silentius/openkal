/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/PlayerRenderSelection.h"

#include <algorithm>
#include <tuple>

namespace openkal::client {
    std::unordered_set<std::uint32_t>
    select_player_ids(std::vector<PlayerRenderCandidate> candidates, int limit) {
        std::unordered_set<std::uint32_t> result;
        candidates.erase(std::remove_if(candidates.begin(),
                                        candidates.end(),
                                        [&](const auto& player) {
                                            if (!player.party_member) {
                                                return false;
                                            }
                                            result.insert(player.id);
                                            return true;
                                        }),
                         candidates.end());
        if (limit < 0) {
            return result;
        }
        const auto count = limit == 0
                               ? candidates.size()
                               : std::min(candidates.size(), static_cast<std::size_t>(limit));
        std::partial_sort(candidates.begin(),
                          candidates.begin() + count,
                          candidates.end(),
                          [](const auto& lhs, const auto& rhs) {
                              return std::make_tuple(!lhs.selected, lhs.distance_squared, lhs.id) <
                                     std::make_tuple(!rhs.selected, rhs.distance_squared, rhs.id);
                          });
        result.reserve(result.size() + count);
        for (std::size_t i = 0; i < count; ++i) {
            result.insert(candidates[i].id);
        }
        return result;
    }
}  // namespace openkal::client
