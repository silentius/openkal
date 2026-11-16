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

namespace openkal::server {
    namespace player_regen {

        /** Standing HP pulse interval (compatibility behavior runtime state). */
        inline constexpr int kStandHpIntervalMs = 6000;

        /** Standing MP pulse interval (compatibility behavior runtime state). */
        inline constexpr int kStandMpIntervalMs = 12000;

        /** Delay from sit until the first rest regen pulse. */
        inline constexpr int kSitFirstDelayMs = 5000;

        /** Rest regen interval after the first pulse. */
        inline constexpr int kSitIntervalMs = 4000;

        /** Wall-clock interval between transform wear ticks. */
        inline constexpr int kTransformTickMs = 1000;

        /** Transform timer ticks before +10 animal EXP (1 tick = 1s). */
        inline constexpr int kTransformExpTicks = 180;

        /** Animal EXP granted by the wear timer. */
        inline constexpr int kTransformTimerExp = 10;

        /** Max transform / animal level (MAX_TRANSLV). */
        inline constexpr int kMaxTransformLevel = 100;

        /**
         * Standing HP add (min(40, maxHP/100)).
         * @param max_hp Current max HP.
         * @return Amount to add, or 0 when max_hp is non-positive.
         */
        [[nodiscard]] std::int32_t standing_hp_amount(std::int32_t max_hp);

        /**
         * Standing MP add (min(20, maxMP/100)).
         * @param max_mp Current max MP.
         * @return Amount to add, or 0 when max_mp is non-positive.
         */
        [[nodiscard]] std::int32_t standing_mp_amount(std::int32_t max_mp);

        /**
         * Inclusive RANDOM(0, 2*restPoint) upper bound.
         * @param rest_point compatibility behavior passive points.
         * @return Upper bound passed to irand (0 when rest_point is 0).
         */
        [[nodiscard]] int sit_random_hi(int rest_point);

        /**
         * Sit HP add: 7*restPoint + roll + 25.
         * @param rest_point compatibility behavior passive points.
         * @param roll Result of RANDOM(0, 2*restPoint).
         * @return Amount to add (at least 25).
         */
        [[nodiscard]] std::int32_t sit_hp_amount(int rest_point, int roll);

        /**
         * Sit MP add: 7*restPoint + roll + 20.
         * @param rest_point compatibility behavior passive points.
         * @param roll Result of RANDOM(0, 2*restPoint).
         * @return Amount to add (at least 20).
         */
        [[nodiscard]] std::int32_t sit_mp_amount(int rest_point, int roll);

        /**
         * Adds `amount` to current, clamped to [0, max_value].
         * @param current Current value (updated).
         * @param max_value Inclusive maximum.
         * @param amount Delta to add (ignored when <= 0).
         * @return True when current changed.
         */
        [[nodiscard]] bool
        add_clamped(std::int32_t& current, std::int32_t max_value, std::int32_t amount);

        /**
         * First sit-regen due time (sit_at + 5000 ms).
         * @param sit_at_ms Monotonic ms when Rest(TRUE) was applied.
         * @return Due timestamp.
         */
        [[nodiscard]] std::uint64_t sit_first_due_ms(std::uint64_t sit_at_ms);

        /**
         * Subsequent sit-regen due time (last pulse + 4000 ms).
         * @param last_pulse_ms Monotonic ms of the previous sit pulse.
         * @return Due timestamp.
         */
        [[nodiscard]] std::uint64_t sit_next_due_ms(std::uint64_t last_pulse_ms);

        /** Equipped transform animal EXP state (compatibility behavior subset). */
        struct TransformExpState {
            /** item definition index; 0 = none equipped. */
            int index = 0;

            /** Animal level (1..kMaxTransformLevel). */
            int level = 1;

            /** Current animal EXP toward the next level. */
            int exp = 0;

            /** Wear-duration tick counter (runtime state). */
            int put_on_ticks = 0;
        };

        /** Outcome of one timer tick against a worn transform item. */
        struct TransformExpTickResult {
            /** True when the 180-tick pulse fired. */
            bool pulsed = false;

            /** True when EXP was kept (not reverted by the player-level gate). */
            bool applied = false;

            /** EXP attempted this pulse (10 on the timer path). */
            int added = 0;

            /** True when animal level increased. */
            bool leveled = false;
        };

        /**
         * Advances compatibility behavior(bTimer=TRUE) by one second.
         * @param state Worn transform; unchanged when index is 0.
         * @param player_level Owner character level.
         * @return Pulse / apply / level-up flags.
         */
        [[nodiscard]] TransformExpTickResult tick_transform_exp(TransformExpState& state,
                                                                int player_level);

        /**
         * Applies compatibility behavior(bTimer=FALSE) kill EXP.
         * @param state Worn transform; unchanged when index is 0.
         * @param player_level Owner character level.
         * @param added EXP to add (kill_transform_exp_add).
         * @return Pulse / apply / level-up flags.
         */
        [[nodiscard]] TransformExpTickResult
        add_transform_exp(TransformExpState& state, int player_level, int added);

    }  // namespace player_regen
}  // namespace openkal::server
