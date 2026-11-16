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

    /** One sit-food channel (compatibility behavior or compatibility behavior). */
    struct RestFoodChannel {
        /** True while the buff is active. */
        bool active = false;

        /** Amount added each 1s pulse (specialty.buff[2]). */
        int amount = 0;

        /** Monotonic expire time in milliseconds. */
        std::uint64_t expire_ms = 0;

        /** Monotonic time of the next 1s heal pulse. */
        std::uint64_t next_pulse_ms = 0;
    };

    /** Outcome of one RestFoodSet::tick. */
    struct RestFoodPulse {
        /** HP to add this tick (0 when none). */
        int hp_add = 0;

        /** MP to add this tick (0 when none). */
        int mp_add = 0;

        /** True when the HP channel expired this tick. */
        bool hp_expired = false;

        /** True when the MP channel expired this tick. */
        bool mp_expired = false;
    };

    /**
     * Runtime sit HP/MP food (related compatibility states).
     * Pulses amount once per second for duration seconds, then expires.
     */
    class RestFoodSet {
      public:
        /** @return Combined related compatibility states bits. */
        [[nodiscard]] std::uint64_t mstate() const;

        /** @return True when either channel is active. */
        [[nodiscard]] bool active() const;

        /** @return True while compatibility behavior is set. */
        [[nodiscard]] bool hp_active() const {
            return m_hp.active;
        }

        /** @return True while compatibility behavior is set. */
        [[nodiscard]] bool mp_active() const {
            return m_mp.active;
        }

        /**
         * Starts or refreshes sit HP food (compatibility behavior / UpdateBuff).
         * @param amount Per-second HP add.
         * @param duration_sec Lifetime in seconds.
         * @param now_ms Monotonic milliseconds.
         * @return True when the channel is active afterwards.
         */
        [[nodiscard]] bool apply_hp(int amount, int duration_sec, std::uint64_t now_ms);

        /**
         * Starts or refreshes sit MP food (compatibility behavior / UpdateBuff).
         * @param amount Per-second MP add.
         * @param duration_sec Lifetime in seconds.
         * @param now_ms Monotonic milliseconds.
         * @return True when the channel is active afterwards.
         */
        [[nodiscard]] bool apply_mp(int amount, int duration_sec, std::uint64_t now_ms);

        /** Clears compatibility behavior (compatibility behavior). */
        void cancel_hp();

        /** Clears compatibility behavior (compatibility behavior). */
        void cancel_mp();

        /** Clears both channels (Rest(FALSE) CancelBuff). */
        void cancel_all();

        /**
         * Applies health and mana restoration for this tick.
         * @param now_ms Monotonic milliseconds.
         * @return Pulse amounts and expire flags.
         */
        [[nodiscard]] RestFoodPulse tick(std::uint64_t now_ms);

      private:
        /**
         * Starts or refreshes one channel.
         * @param channel Channel to write.
         * @param amount Per-second add.
         * @param duration_sec Lifetime in seconds.
         * @param now_ms Monotonic milliseconds.
         * @return True when the channel is active afterwards.
         */
        [[nodiscard]] static bool
        apply_channel(RestFoodChannel& channel, int amount, int duration_sec, std::uint64_t now_ms);

        /**
         * Pulses and possibly expires one channel.
         * @param channel Channel to tick.
         * @param now_ms Monotonic milliseconds.
         * @param add Out amount to apply (0 when none).
         * @param expired Out true when the channel ended this tick.
         */
        static void
        tick_channel(RestFoodChannel& channel, std::uint64_t now_ms, int& add, bool& expired);

        /** Sit HP food. */
        RestFoodChannel m_hp;

        /** Sit MP food. */
        RestFoodChannel m_mp;
    };

}  // namespace openkal::server
