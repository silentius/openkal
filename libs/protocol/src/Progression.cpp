/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/protocol/Progression.h"

#include <algorithm>
#include <cstdlib>

namespace openkal::protocol {

    std::int64_t need_exp_for_level(int level) {
        if (level < 0 || level > kMaxPlayerLevel) {
            return 0;
        }
        return kNeedExp[level];
    }

    std::int64_t revise_kill_exp(std::int64_t base_exp, int player_level, int monster_level) {
        if (base_exp <= 0 || player_level < 1) {
            return 0;
        }
        const int band = std::clamp((player_level - 1) / 10, 0, 9);
        const int diff = monster_level - player_level;
        float host = static_cast<float>(base_exp);
        if (diff < 0) {
            int revise = std::abs(diff);
            if (revise >= 20) {
                revise = 20;
            }
            host -= host * (static_cast<float>(kReviseExpB[band][revise]) / 100.0f);
        } else {
            int revise = diff;
            if (revise >= 20) {
                revise = 20;
            }
            host += host * (static_cast<float>(kReviseExpA[band][revise]) / 100.0f);
        }
        if (host <= 0.0f) {
            return 0;
        }
        return static_cast<std::int64_t>(host);
    }

    std::int64_t cutdown_exp_amount(int level) {
        if (level <= 5 || level > kMaxPlayerLevel) {
            return 0;
        }
        return kNeedExp[level] * (level / 5 + 2) / 100;
    }

}  // namespace openkal::protocol
