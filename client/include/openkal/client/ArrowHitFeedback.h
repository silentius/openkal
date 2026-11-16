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

#include "openkal/engine/engine.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace openkal::client {

    /** Matches confirmed attack outcomes to visible arrows, in order per attacker/target. */
    class ArrowHitFeedback {
      public:
        /** One confirmed outcome whose arrow has reached the target. */
        struct Impact {
            std::uint32_t attacker = 0;
            std::uint32_t target = 0;
            std::uint8_t flags = 0;
            engine::Vec3 origin;
            engine::Vec3 position;
            float scale = 1.0f;
        };

        /** Tracks a normal arrow from its animation-keyed release.
         * @param attacker Shooter entity ID.
         * @param target Target entity ID.
         * @param origin Actual projectile launch position.
         * @param scale Target impact scale.
         * @return Token passed back at the arrow's actual impact; zero for unknown entities.
         */
        [[nodiscard]] std::uint64_t launch(std::uint32_t attacker,
                                           std::uint32_t target,
                                           const engine::Vec3& origin,
                                           float scale);

        /** Records a server outcome, including misses so later hits cannot use the wrong arrow.
         * @param attacker Shooter entity ID.
         * @param target Target entity ID.
         * @param flags Confirmed attack flags.
         */
        void confirm(std::uint32_t attacker, std::uint32_t target, std::uint8_t flags);

        /** Records arrival from the projectile simulation, without predicting a flight delay.
         * @param token Token returned by launch().
         * @param position Actual world-space impact position.
         */
        void arrive(std::uint64_t token, const engine::Vec3& position);

        /** Removes orphaned launches/results after five seconds without fabricating impacts.
         * @param dt Elapsed seconds.
         */
        void update(float dt);

        /** Consumes an outcome only after both confirmation and projectile arrival.
         * @param out Receives one matched outcome, including misses.
         * @return True when an outcome was ready.
         */
        [[nodiscard]] bool take(Impact& out);

      private:
        struct Shot {
            std::uint64_t token = 0;
            Impact impact;
            std::optional<std::uint8_t> flags;
            bool arrived = false;
            float age = 0.0f;
        };
        struct Result {
            std::uint32_t attacker = 0;
            std::uint32_t target = 0;
            std::uint8_t flags = 0;
            float age = 0.0f;
        };
        std::uint64_t m_next_token = 0;
        std::vector<Shot> m_shots;
        /** Outcomes received before the animation releases an arrow. */
        std::vector<Result> m_results;
    };
}  // namespace openkal::client
