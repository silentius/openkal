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

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace openkal::server {

    class PayBuffSet;

    /** One weighted item outcome from `openkal.fishing/v1`. */
    struct FishingReward {
        /** item definition index to insert into the player's bag. */
        int item_index = 0;

        /** Inclusive units in a one-through-1000 roll. */
        int chance_per_thousand = 0;
    };

    /** Validated fishing timing and reward configuration. */
    class FishingCatalog final {
      public:
        /** Loads an `openkal.fishing/v1` document from disk.
         * @param path JSON configuration path.
         * @return True if the document is valid.
         */
        [[nodiscard]] bool load_file(const std::string& path);

        /** Loads an `openkal.fishing/v1` document from JSON text.
         * @param text JSON document.
         * @return True if the document is valid.
         */
        [[nodiscard]] bool load_json_text(std::string_view text);

        /** Reports whether a valid document is loaded.
         * @return True when fishing can be scheduled.
         */
        [[nodiscard]] bool ready() const {
            return m_ready;
        }

        /** Returns the standard interval before the next catch.
         * @return Interval in milliseconds.
         */
        [[nodiscard]] std::uint64_t base_interval_ms() const {
            return m_base_interval_ms;
        }

        /** Returns the interval used by an active Fishing Stone buff.
         * @return Interval in milliseconds.
         */
        [[nodiscard]] std::uint64_t accelerated_interval_ms() const {
            return m_accelerated_interval_ms;
        }

        /** Picks the configured result for one configured probability roll.
         * @param roll Integer in the inclusive range [1, 1000].
         * @return Item index when the roll is valid.
         */
        [[nodiscard]] std::optional<int> item_for_roll(int roll) const;

        /** Returns all validated reward rows.
         * @return Reward rows in configuration order.
         */
        [[nodiscard]] const std::vector<FishingReward>& rewards() const {
            return m_rewards;
        }

      private:
        /** Clears loaded state before a new parse attempt. */
        void clear();

        /** True after successful validation. */
        bool m_ready = false;

        /** Standard wait between catches. */
        std::uint64_t m_base_interval_ms = 0;

        /** Future Fishing Stone wait between catches. */
        std::uint64_t m_accelerated_interval_ms = 0;

        /** Weighted catch outcomes. */
        std::vector<FishingReward> m_rewards;
    };

    /** Selects the next fishing-cycle interval from the active buffs.
     * @param catalog Validated fishing timing configuration.
     * @param buffs Player's current timed item buffs.
     * @return Interval in milliseconds for the next cycle.
     */
    [[nodiscard]] std::uint64_t fishing_cycle_interval_ms(const FishingCatalog& catalog,
                                                          const PayBuffSet& buffs);

}  // namespace openkal::server
