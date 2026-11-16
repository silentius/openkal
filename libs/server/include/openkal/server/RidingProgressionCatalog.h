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

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace openkal::server {
    /** One possible Battle Riding promotion outcome. */
    struct RidingPromotionResult {
        int item_index = 0;
        int chance_per_thousand = 0;
    };

    /** One normal-mount-to-Battle-Riding promotion rule. */
    struct RidingPromotion {
        int item_index = 0;
        int seal_cost = 0;
        std::vector<RidingPromotionResult> results;
    };

    /** Validated, server-authoritative Stablemaster progression settings. */
    class RidingProgressionCatalog final {
      public:
        /** Loads an `openkal.riding-progression/v1` document.
         * @param path JSON configuration path.
         * @return True when the document is valid.
         */
        [[nodiscard]] bool load_file(const std::string& path);
        /** @return True after successful loading. */
        [[nodiscard]] bool ready() const {
            return m_ready;
        }
        /** @return Riding Seal item index. */
        [[nodiscard]] int seal_item_index() const {
            return m_seal_item_index;
        }
        /** @return Starter Horse item index. */
        [[nodiscard]] int starter_item_index() const {
            return m_starter_item_index;
        }
        /** @return Starter exchange Seal cost. */
        [[nodiscard]] int starter_seal_cost() const {
            return m_starter_seal_cost;
        }
        /** @return Costs for Normal-to-Uncommon through Rare-to-Epic. */
        [[nodiscard]] const std::array<int, 3>& grade_costs() const {
            return m_grade_costs;
        }
        /** @return Seal reward from converting a duplicate. */
        [[nodiscard]] int duplicate_seal_reward() const {
            return m_duplicate_seal_reward;
        }
        /** @return Stable Time Writ item index. */
        [[nodiscard]] int time_writ_item_index() const {
            return m_time_writ_item_index;
        }
        /** @return Stable Time Writ extension in days. */
        [[nodiscard]] int time_writ_extension_days() const {
            return m_time_writ_extension_days;
        }
        /** @return Stable Time Writ maximum remaining days. */
        [[nodiscard]] int time_writ_maximum_days() const {
            return m_time_writ_maximum_days;
        }
        /** Finds a promotion rule for a normal mount.
         * @param item_index Normal Riding item index.
         * @return Matching rule, if configured.
         */
        [[nodiscard]] const RidingPromotion* promotion_for(int item_index) const;
        /** Rolls a configured promotion result.
         * @param promotion Promotion rule to resolve.
         * @param roll Inclusive roll in [1, 1000].
         * @return Result item index or zero.
         */
        [[nodiscard]] int promotion_result_for_roll(const RidingPromotion& promotion,
                                                    int roll) const;

      private:
        bool m_ready = false;
        int m_seal_item_index = 41322;
        int m_starter_item_index = 0;
        int m_starter_seal_cost = 0;
        std::array<int, 3> m_grade_costs{};
        int m_duplicate_seal_reward = 0;
        int m_time_writ_item_index = 0;
        int m_time_writ_extension_days = 0;
        int m_time_writ_maximum_days = 0;
        std::vector<RidingPromotion> m_promotions;
    };
}  // namespace openkal::server
