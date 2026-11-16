/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/LevelMailRewards.h"

#include "openkal/platform/File.h"
#include "openkal/platform/Json.h"
#include "openkal/platform/Log.h"

#include <algorithm>
#include <unordered_set>

namespace openkal::server {
    void LevelMailRewardCatalog::clear() {
        m_ready = false;
        m_rewards.clear();
    }

    bool LevelMailRewardCatalog::load_file(const std::string& path) {
        clear();
        const auto bytes = platform::read_file(path);
        if (!bytes) {
            log::warn("LevelMailRewardCatalog: missing " + path);
            return false;
        }
        return load_json_text(std::string(bytes->begin(), bytes->end()));
    }

    bool LevelMailRewardCatalog::load_json_text(std::string_view text) {
        clear();
        platform::Json root;
        std::string error;
        if (!platform::Json::parse(text, root, &error)) {
            log::warn("LevelMailRewardCatalog: parse failed: " + error);
            return false;
        }
        if (!root.is_object() || root.at("schema").as_string() != "openkal.level_mail_rewards/v1" ||
            !root.at("rewards").is_array()) {
            log::warn("LevelMailRewardCatalog: unsupported or missing schema");
            return false;
        }
        std::unordered_set<int> levels;
        for (const auto& value : root.at("rewards").array()) {
            LevelMailReward reward;
            reward.level = value.at("level").as_int(0);
            reward.item_id = value.at("itemId").as_int(0);
            reward.count = value.at("count").as_int(0);
            if (!value.is_object() || reward.level <= 0 || reward.item_id <= 0 ||
                reward.count <= 0 || !levels.insert(reward.level).second) {
                log::warn("LevelMailRewardCatalog: invalid or duplicate level reward");
                clear();
                return false;
            }
            m_rewards.push_back(reward);
        }
        std::sort(m_rewards.begin(), m_rewards.end(), [](const auto& a, const auto& b) {
            return a.level < b.level;
        });
        m_ready = true;
        return true;
    }

    std::vector<LevelMailReward> LevelMailRewardCatalog::rewards_for_level(int level) const {
        std::vector<LevelMailReward> result;
        for (const auto& reward : m_rewards) {
            if (reward.level == level) {
                result.push_back(reward);
            }
        }
        return result;
    }
}  // namespace openkal::server
