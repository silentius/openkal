/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/RidingCacheCatalog.h"

#include "openkal/platform/File.h"
#include "openkal/platform/Json.h"
#include "openkal/platform/Log.h"

#include <cmath>
#include <unordered_set>

namespace openkal::server {
    namespace {
        constexpr int kRollUnits = 1000;

        [[nodiscard]] int chance_to_roll_units(float chance) {
            if (chance <= 0.0f) {
                return 0;
            }
            return static_cast<int>(std::lround(static_cast<double>(chance) * 10.0));
        }
    }  // namespace

    void RidingCacheCatalog::clear() {
        m_ready = false;
        m_cache_item_index = 0;
        m_seal_item_index = 0;
        m_seal_count = 0;
        m_rewards.clear();
    }

    bool RidingCacheCatalog::load_file(const std::string& path) {
        clear();
        const auto bytes = platform::read_file(path);
        if (!bytes) {
            log::warn("RidingCacheCatalog: missing " + path);
            return false;
        }
        return load_json_text(std::string(bytes->begin(), bytes->end()));
    }

    bool RidingCacheCatalog::load_json_text(std::string_view text) {
        clear();
        std::string error;
        platform::Json root;
        if (!platform::Json::parse(text, root, &error)) {
            log::warn("RidingCacheCatalog: parse failed: " + error);
            return false;
        }
        if (!root.is_object() || root.at("schema").as_string() != "openkal.riding-cache/v1") {
            log::warn("RidingCacheCatalog: unsupported or missing schema");
            return false;
        }
        m_cache_item_index = root.at("cacheItemIndex").as_int(0);
        m_seal_item_index = root.at("sealItemIndex").as_int(0);
        m_seal_count = root.at("sealCount").as_int(0);
        const platform::Json& rewards = root.at("rewards");
        if (m_cache_item_index <= 0 || m_seal_item_index <= 0 || m_seal_count <= 0 ||
            !rewards.is_array()) {
            log::warn("RidingCacheCatalog: invalid required fields");
            clear();
            return false;
        }

        std::unordered_set<int> seen_indices;
        int total = 0;
        for (const platform::Json& value : rewards.array()) {
            if (!value.is_object()) {
                log::warn("RidingCacheCatalog: reward must be an object");
                clear();
                return false;
            }
            RidingCacheReward reward;
            reward.item_index = value.at("item").as_int(0);
            reward.count = value.at("count").as_int(0);
            reward.chance_per_thousand = chance_to_roll_units(value.at("chance").as_float(0.0f));
            if (reward.item_index <= 0 || reward.count <= 0 || reward.chance_per_thousand <= 0 ||
                reward.chance_per_thousand > kRollUnits ||
                !seen_indices.insert(reward.item_index).second ||
                total > kRollUnits - reward.chance_per_thousand) {
                log::warn("RidingCacheCatalog: invalid reward row");
                clear();
                return false;
            }
            total += reward.chance_per_thousand;
            m_rewards.push_back(reward);
        }
        if (m_rewards.empty() || total != kRollUnits) {
            log::warn("RidingCacheCatalog: reward chances must total 100 percent");
            clear();
            return false;
        }
        m_ready = true;
        return true;
    }

    std::optional<RidingCacheReward> RidingCacheCatalog::reward_for_roll(int roll) const {
        if (!m_ready || roll < 1 || roll > kRollUnits) {
            return std::nullopt;
        }
        int accumulated = 0;
        for (const RidingCacheReward& reward : m_rewards) {
            accumulated += reward.chance_per_thousand;
            if (roll <= accumulated) {
                return reward;
            }
        }
        return std::nullopt;
    }
}  // namespace openkal::server
