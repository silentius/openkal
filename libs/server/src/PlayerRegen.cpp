/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/PlayerRegen.h"

#include "openkal/protocol/Transform.h"
#include "openkal/server/Combat.h"

#include <algorithm>

namespace openkal::server {
    namespace player_regen {

        std::int32_t standing_hp_amount(std::int32_t max_hp) {
            if (max_hp <= 0) {
                return 0;
            }
            const std::int64_t hundredth = static_cast<std::int64_t>(max_hp) / 100;
            if (hundredth >= 40) {
                return 40;
            }
            return combat::clamp_nonneg_i32(hundredth);
        }

        std::int32_t standing_mp_amount(std::int32_t max_mp) {
            if (max_mp <= 0) {
                return 0;
            }
            const std::int64_t hundredth = static_cast<std::int64_t>(max_mp) / 100;
            if (hundredth >= 20) {
                return 20;
            }
            return combat::clamp_nonneg_i32(hundredth);
        }

        int sit_random_hi(int rest_point) {
            if (rest_point <= 0) {
                return 0;
            }
            return 2 * rest_point;
        }

        std::int32_t sit_hp_amount(int rest_point, int roll) {
            const std::int64_t point = std::max(0, rest_point);
            return combat::clamp_nonneg_i32(7 * point + std::max(0, roll) + 25);
        }

        std::int32_t sit_mp_amount(int rest_point, int roll) {
            const std::int64_t point = std::max(0, rest_point);
            return combat::clamp_nonneg_i32(7 * point + std::max(0, roll) + 20);
        }

        bool add_clamped(std::int32_t& current, std::int32_t max_value, std::int32_t amount) {
            if (amount <= 0 || max_value <= 0) {
                return false;
            }
            const std::int32_t before = current;
            std::int64_t next = static_cast<std::int64_t>(current) + amount;
            if (next > max_value) {
                next = max_value;
            }
            current = combat::clamp_nonneg_i32(next);
            return current != before;
        }

        std::uint64_t sit_first_due_ms(std::uint64_t sit_at_ms) {
            return sit_at_ms + static_cast<std::uint64_t>(kSitFirstDelayMs);
        }

        std::uint64_t sit_next_due_ms(std::uint64_t last_pulse_ms) {
            return last_pulse_ms + static_cast<std::uint64_t>(kSitIntervalMs);
        }

        TransformExpTickResult
        add_transform_exp(TransformExpState& state, int player_level, int added) {
            TransformExpTickResult result;
            if (state.index <= 0 || state.level >= kMaxTransformLevel || added <= 0) {
                return result;
            }
            result.pulsed = true;
            result.added = added;
            state.exp += added;

            const int need = protocol::need_transform_exp_for_level(state.level);
            if (need > 0 && state.exp >= need) {
                if (player_level <= state.level) {
                    state.exp -= added;
                    return result;
                }
                state.exp -= need;
                ++state.level;
                if (state.level > kMaxTransformLevel) {
                    state.level = kMaxTransformLevel;
                }
                result.leveled = true;
            }
            result.applied = true;
            return result;
        }

        TransformExpTickResult tick_transform_exp(TransformExpState& state, int player_level) {
            TransformExpTickResult result;
            if (state.index <= 0 || state.level >= kMaxTransformLevel) {
                state.put_on_ticks = 0;
                return result;
            }
            if (++state.put_on_ticks < kTransformExpTicks) {
                return result;
            }
            state.put_on_ticks -= kTransformExpTicks;
            return add_transform_exp(state, player_level, kTransformTimerExp);
        }

    }  // namespace player_regen
}  // namespace openkal::server
