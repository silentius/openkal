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

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace openkal::server {

    /** One weighted additional Riding Cache reward. */
    struct RidingCacheReward {
        /** Static item definition inserted into the bag. */
        int item_index = 0;
        /** Number of inserted item units. */
        int count = 1;
        /** Internal units converted from the configured percentage chance. */
        int chance_per_thousand = 0;
    };

    /** Validated server-authoritative Riding Cache reward definition. */
    class RidingCacheCatalog final {
      public:
        /** Loads an `openkal.riding-cache/v1` document from disk.
         * @param path JSON configuration path.
         * @return True when the complete document is valid.
         */
        [[nodiscard]] bool load_file(const std::string& path);

        /** Loads an `openkal.riding-cache/v1` document from text.
         * @param text JSON document.
         * @return True when the complete document is valid.
         */
        [[nodiscard]] bool load_json_text(std::string_view text);

        /** @return True once a valid catalog has been loaded. */
        [[nodiscard]] bool ready() const {
            return m_ready;
        }

        /** @return Usable Riding Cache item index. */
        [[nodiscard]] int cache_item_index() const {
            return m_cache_item_index;
        }

        /** @return Guaranteed Riding Seal item index. */
        [[nodiscard]] int seal_item_index() const {
            return m_seal_item_index;
        }

        /** @return Guaranteed Riding Seal units per successful opening. */
        [[nodiscard]] int seal_count() const {
            return m_seal_count;
        }

        /** Resolves one additional reward.
         * @param roll Integer in the inclusive range [1, 1000].
         * @return The configured reward when the roll is valid.
         */
        [[nodiscard]] std::optional<RidingCacheReward> reward_for_roll(int roll) const;

        /** @return Validated reward rows in configuration order. */
        [[nodiscard]] const std::vector<RidingCacheReward>& rewards() const {
            return m_rewards;
        }

      private:
        /** Clears all loaded data before parsing. */
        void clear();

        bool m_ready = false;
        int m_cache_item_index = 0;
        int m_seal_item_index = 0;
        int m_seal_count = 0;
        std::vector<RidingCacheReward> m_rewards;
    };

}  // namespace openkal::server
