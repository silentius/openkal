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

#include "openkal/protocol/Honor.h"

#include <array>
#include <cstdint>
#include <string>

namespace openkal::server {
    /** One server-authorized event award. The source, instance, reason and character form its
     * persistent replay key. Keep the timestamp and amounts stable when retrying.
     */
    struct HonorGrant {
        std::string source;
        std::string instance;
        std::string reason;
        std::int32_t character_id = 0;
        std::int64_t occurred_at = 0;
        std::int64_t honor_points = 0;
        std::int64_t reward_points = 0;
    };

    /** Atomic inventory exchange against a character's Reward Points. */
    struct HonorRewardExchange {
        std::int32_t character_id = 0;
        std::int64_t week_start = 0;
        /** Positive for a coupon, negative for a purchase. */
        std::int64_t delta = 0;
        protocol::HonorRank minimum_rank = protocol::HonorRank::Unranked;
    };

    /** Outcome of an Honor purchase or coupon redemption. */
    enum class HonorExchangeResult {
        Committed,
        RankTooLow,
        InsufficientPoints,
        BalanceLimit,
        Failed
    };

    /** Checks the amount and character before an exchange can write to storage.
     * @param exchange Requested exchange.
     * @return Whether the request can be evaluated.
     */
    [[nodiscard]] bool valid_honor_exchange(const HonorRewardExchange& exchange);

    /** Checks the exchange against the balance and rank locked by the store.
     * @param status Authoritative character state.
     * @param exchange Requested exchange.
     * @return Committed when the balance and rank allow the exchange, or the rejection reason.
     */
    [[nodiscard]] HonorExchangeResult check_honor_exchange(const protocol::HonorStatus& status,
                                                           const HonorRewardExchange& exchange);

    /** Validates a new event award.
     * @param grant Event-generated award.
     * @return True when the identity and amounts are within bounds.
     */
    [[nodiscard]] bool valid_honor_grant(const HonorGrant& grant);

    /** Finds the start of the Honor week containing a timestamp.
     * @param now Unix timestamp in seconds.
     * @return Timestamp of Monday 00:00 UTC.
     */
    [[nodiscard]] std::int64_t honor_week_start(std::int64_t now);

    /** Number of seconds in an Honor week. */
    inline constexpr std::int64_t kHonorWeekSeconds = 7 * 24 * 60 * 60;

    /** Assigns Honor ranks to characters in descending score order for one week. */
    class HonorRanks {
      public:
        /** Assigns the highest available rank to the next character.
         * @param score Total rank score.
         * @return Assigned rank, respecting the remaining places.
         */
        [[nodiscard]] protocol::HonorRank assign(std::int64_t score);

        /** Resolves a single-player rank directly from the available points.
         * @param honor Local Honor balance.
         * @return Highest eligible rank.
         */
        [[nodiscard]] static protocol::HonorRank local_rank(std::int64_t honor);

      private:
        std::array<int, 10> m_remaining{0, 0, 0, 0, 0, 81, 49, 25, 9, 1};
    };
}  // namespace openkal::server
