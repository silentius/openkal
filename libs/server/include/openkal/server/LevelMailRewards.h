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

#include <string>
#include <string_view>
#include <vector>

namespace openkal::server {

    /** One item attachment granted by a level-up system mail. */
    struct LevelMailReward {
        int level = 0;
        int item_id = 0;
        int count = 1;
    };

    /** Validated level-up mail reward configuration. */
    class LevelMailRewardCatalog final {
      public:
        /** Loads an `openkal.level_mail_rewards/v1` document from disk.
         * @param path JSON configuration path.
         * @return True if the document is valid.
         */
        [[nodiscard]] bool load_file(const std::string& path);

        /** Loads an `openkal.level_mail_rewards/v1` document from JSON text.
         * @param text JSON document.
         * @return True if the document is valid.
         */
        [[nodiscard]] bool load_json_text(std::string_view text);

        /** Returns rewards for one newly reached level.
         * @param level Newly reached player level.
         * @return Reward rows in configuration order.
         */
        [[nodiscard]] std::vector<LevelMailReward> rewards_for_level(int level) const;

        /** Returns all validated reward rows.
         * @return Reward rows ordered by level.
         */
        [[nodiscard]] const std::vector<LevelMailReward>& rewards() const {
            return m_rewards;
        }

        /** Reports whether a valid document has been loaded.
         * @return True after successful validation.
         */
        [[nodiscard]] bool ready() const {
            return m_ready;
        }

      private:
        void clear();

        bool m_ready = false;
        std::vector<LevelMailReward> m_rewards;
    };

}  // namespace openkal::server
