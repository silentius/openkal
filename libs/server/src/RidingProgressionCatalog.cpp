/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/RidingProgressionCatalog.h"

#include "openkal/platform/File.h"
#include "openkal/platform/Json.h"

#include <cmath>

namespace openkal::server {
    bool RidingProgressionCatalog::load_file(const std::string& path) {
        m_ready = false;
        m_promotions.clear();
        const auto bytes = platform::read_file(path);
        platform::Json root;
        std::string error;
        if (!bytes ||
            !platform::Json::parse(std::string(bytes->begin(), bytes->end()), root, &error) ||
            !root.is_object() || root.at("schema").as_string() != "openkal.riding-progression/v1") {
            return false;
        }
        const auto& starter = root.at("starterExchange");
        const auto& costs = root.at("gradeUpgradeSealCosts");
        const auto& writ = root.at("timeWrit");
        if (!starter.is_object() || !costs.is_array() || costs.array().size() != 3 ||
            !writ.is_object()) {
            return false;
        }
        m_seal_item_index = root.at("sealItem").as_int(0);
        m_starter_item_index = starter.at("item").as_int(0);
        m_starter_seal_cost = starter.at("sealCost").as_int(0);
        for (std::size_t i = 0; i < m_grade_costs.size(); ++i) {
            m_grade_costs[i] = costs.array()[i].as_int(0);
        }
        m_duplicate_seal_reward = root.at("duplicateSealReward").as_int(0);
        m_time_writ_item_index = writ.at("item").as_int(0);
        m_time_writ_extension_days = writ.at("extensionDays").as_int(0);
        m_time_writ_maximum_days = writ.at("maximumDays").as_int(0);
        if (m_seal_item_index <= 0 || m_starter_item_index <= 0 || m_starter_seal_cost <= 0 ||
            m_duplicate_seal_reward <= 0 || m_time_writ_item_index <= 0 ||
            m_time_writ_extension_days <= 0 ||
            m_time_writ_maximum_days < m_time_writ_extension_days) {
            return false;
        }
        for (int cost : m_grade_costs) {
            if (cost <= 0) {
                return false;
            }
        }
        const auto& promotions = root.at("promotions");
        if (!promotions.is_array()) {
            return false;
        }
        for (const auto& row : promotions.array()) {
            RidingPromotion promotion{row.at("item").as_int(0), row.at("sealCost").as_int(0), {}};
            int total = 0;
            const auto& results = row.at("results");
            if (promotion.item_index <= 0 || promotion.seal_cost <= 0 || !results.is_array()) {
                return false;
            }
            for (const auto& result : results.array()) {
                RidingPromotionResult r{
                    result.at("item").as_int(0),
                    static_cast<int>(std::lround(result.at("chance").as_float(0.0f) * 10.0f))};
                if (r.item_index <= 0 || r.chance_per_thousand <= 0 ||
                    total > 1000 - r.chance_per_thousand) {
                    return false;
                }
                total += r.chance_per_thousand;
                promotion.results.push_back(r);
            }
            if (total != 1000) {
                return false;
            }
            m_promotions.push_back(std::move(promotion));
        }
        return m_ready = !m_promotions.empty();
    }
    const RidingPromotion* RidingProgressionCatalog::promotion_for(int item_index) const {
        for (const auto& promotion : m_promotions) {
            if (promotion.item_index == item_index) {
                return &promotion;
            }
        }
        return nullptr;
    }
    int RidingProgressionCatalog::promotion_result_for_roll(const RidingPromotion& promotion,
                                                            int roll) const {
        int total = 0;
        for (const auto& result : promotion.results) {
            total += result.chance_per_thousand;
            if (roll <= total) {
                return result.item_index;
            }
        }
        return 0;
    }
}  // namespace openkal::server
