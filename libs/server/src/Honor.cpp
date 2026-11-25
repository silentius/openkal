/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/Honor.h"

#include <string_view>

namespace openkal::server {
    namespace {
        constexpr std::array<std::int64_t, 10> kRankThresholds{
            2'790,
            4'960,
            7'750,
            12'090,
            20'150,
            35'030,
            44'640,
            57'970,
            78'740,
            111'600,
        };
    }  // namespace

    bool valid_honor_exchange(const HonorRewardExchange& exchange) {
        return exchange.character_id > 0 && exchange.delta != 0 &&
               exchange.delta >= -protocol::kMaxHonorPoints &&
               exchange.delta <= protocol::kMaxHonorPoints &&
               exchange.minimum_rank <= protocol::HonorRank::General;
    }

    HonorExchangeResult check_honor_exchange(const protocol::HonorStatus& status,
                                             const HonorRewardExchange& exchange) {
        if (status.rank < exchange.minimum_rank) {
            return HonorExchangeResult::RankTooLow;
        }
        if (status.reward_points + exchange.delta < 0) {
            return HonorExchangeResult::InsufficientPoints;
        }
        if (status.reward_points + exchange.delta > protocol::kMaxHonorPoints) {
            return HonorExchangeResult::BalanceLimit;
        }
        return HonorExchangeResult::Committed;
    }

    bool valid_honor_grant(const HonorGrant& grant) {
        const auto valid_key = [](std::string_view key) {
            return !key.empty() && key.size() <= 96 && key.find(char{}) == std::string_view::npos;
        };
        return grant.character_id > 0 && grant.occurred_at > 0 && valid_key(grant.source) &&
               grant.source != "castle_war_legacy" && valid_key(grant.instance) &&
               valid_key(grant.reason) && grant.honor_points >= 0 && grant.reward_points >= 0 &&
               grant.honor_points <= protocol::kMaxHonorPoints &&
               grant.reward_points <= protocol::kMaxHonorPoints &&
               (grant.honor_points > 0 || grant.reward_points > 0);
    }

    std::int64_t honor_week_start(std::int64_t now) {
        // The Unix epoch starts on Thursday, three days after Monday.
        constexpr std::int64_t offset = 3 * 24 * 60 * 60;
        const auto day_in_week =
            (now % kHonorWeekSeconds + offset + kHonorWeekSeconds) % kHonorWeekSeconds;
        return now - day_in_week;
    }

    protocol::HonorRank HonorRanks::assign(std::int64_t score) {
        for (std::size_t rank = kRankThresholds.size(); rank > 0; --rank) {
            if (score < kRankThresholds[rank - 1]) {
                continue;
            }
            if (rank <= static_cast<std::size_t>(protocol::HonorRank::Sergeant)) {
                return static_cast<protocol::HonorRank>(rank);
            }
            auto& remaining = m_remaining[rank - 1];
            if (remaining > 0) {
                --remaining;
                return static_cast<protocol::HonorRank>(rank);
            }
        }
        return protocol::HonorRank::Unranked;
    }

    protocol::HonorRank HonorRanks::local_rank(std::int64_t honor) {
        return HonorRanks{}.assign(honor);
    }
}  // namespace openkal::server
