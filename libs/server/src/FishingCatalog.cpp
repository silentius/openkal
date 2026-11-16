/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/FishingCatalog.h"

#include "openkal/platform/File.h"
#include "openkal/platform/Json.h"
#include "openkal/platform/Log.h"
#include "openkal/server/PayBuff.h"

#include <string>
#include <unordered_set>

namespace openkal::server {
    namespace {

        constexpr int kRollUnits = 1000;

        [[nodiscard]] bool interval_to_ms(int seconds, std::uint64_t& out) {
            if (seconds <= 0) {
                return false;
            }
            out = static_cast<std::uint64_t>(seconds) * 1000u;
            return true;
        }

    }  // namespace

    std::uint64_t fishing_cycle_interval_ms(const FishingCatalog& catalog,
                                            const PayBuffSet& buffs) {
        return buffs.has(protocol::kCbsStoneFishery) ? catalog.accelerated_interval_ms()
                                                     : catalog.base_interval_ms();
    }

    void FishingCatalog::clear() {
        m_ready = false;
        m_base_interval_ms = 0;
        m_accelerated_interval_ms = 0;
        m_rewards.clear();
    }

    bool FishingCatalog::load_file(const std::string& path) {
        clear();
        const auto bytes = platform::read_file(path);
        if (!bytes) {
            log::warn("FishingCatalog: missing " + path);
            return false;
        }
        return load_json_text(std::string(bytes->begin(), bytes->end()));
    }

    bool FishingCatalog::load_json_text(std::string_view text) {
        clear();
        std::string error;
        platform::Json root;
        if (!platform::Json::parse(text, root, &error)) {
            log::warn("FishingCatalog: parse failed: " + error);
            return false;
        }
        if (!root.is_object() || root.at("schema").as_string() != "openkal.fishing/v1") {
            log::warn("FishingCatalog: unsupported or missing schema");
            return false;
        }
        if (!interval_to_ms(root.at("baseIntervalSeconds").as_int(0), m_base_interval_ms) ||
            !interval_to_ms(root.at("acceleratedIntervalSeconds").as_int(0),
                            m_accelerated_interval_ms)) {
            log::warn("FishingCatalog: intervals must be positive seconds");
            clear();
            return false;
        }

        const platform::Json& rewards = root.at("rewards");
        if (!rewards.is_array()) {
            log::warn("FishingCatalog: rewards must be an array");
            clear();
            return false;
        }

        std::unordered_set<int> seen_indices;
        int total_chance = 0;
        for (const auto& value : rewards.array()) {
            if (!value.is_object()) {
                log::warn("FishingCatalog: reward must be an object");
                clear();
                return false;
            }
            FishingReward reward;
            reward.item_index = value.at("itemIndex").as_int(0);
            reward.chance_per_thousand = value.at("chancePerThousand").as_int(0);
            if (reward.item_index <= 0 || reward.chance_per_thousand <= 0 ||
                reward.chance_per_thousand > kRollUnits ||
                !seen_indices.insert(reward.item_index).second ||
                total_chance > kRollUnits - reward.chance_per_thousand) {
                log::warn("FishingCatalog: invalid reward row");
                clear();
                return false;
            }
            total_chance += reward.chance_per_thousand;
            m_rewards.push_back(reward);
        }
        if (m_rewards.empty() || total_chance != kRollUnits) {
            log::warn("FishingCatalog: reward chances must total 1000");
            clear();
            return false;
        }
        m_ready = true;
        return true;
    }

    std::optional<int> FishingCatalog::item_for_roll(int roll) const {
        if (!m_ready || roll < 1 || roll > kRollUnits) {
            return std::nullopt;
        }
        int accumulated = 0;
        for (const FishingReward& reward : m_rewards) {
            accumulated += reward.chance_per_thousand;
            if (roll <= accumulated) {
                return reward.item_index;
            }
        }
        return std::nullopt;
    }

}  // namespace openkal::server
