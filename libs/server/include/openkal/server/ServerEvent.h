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

#include "openkal/protocol/ExpEventState.h"
#include "openkal/protocol/Quest.h"
#include "openkal/server/store/DataStore.h"

#include <cstdint>
#include <string>

namespace openkal::server {

    /** Result returned after a player submits coins. */
    struct ExpEventDonationResult {
        bool accepted = false;
        bool activated = false;
        bool extended = false;
        std::int64_t donated_items = 0;
        std::int64_t progress_items = 0;
        std::int64_t next_target_items = 0;
        std::int32_t bonus_percent = 0;
        std::int64_t active_until = 0;
    };

    /** A quest step selected by a HyperText quest link. */
    struct ExpEventQuestStep {
        std::uint16_t index = 0;
        std::uint8_t flag = 0;

        [[nodiscard]] constexpr std::uint32_t key() const {
            return protocol::Quest::make_key(index, flag);
        }
    };

    /** Configures and persists the global community EXP event. */
    class ServerEventManager final {
      public:
        [[nodiscard]] bool load_file(const std::string& path);
        [[nodiscard]] bool restore(IDataStore& store, std::int64_t now_unix);

        /** Expires the event and applies funded activation or extension steps.
         * @param store Persistence for the updated event state.
         * @param now_unix Current server time in seconds.
         * @return True when no transition is needed or the transition was saved.
         */
        [[nodiscard]] bool advance(IDataStore& store, std::int64_t now_unix);

        [[nodiscard]] bool contribute(IDataStore& store,
                                      std::int64_t item_count,
                                      std::int64_t now_unix,
                                      ExpEventDonationResult& result);

        [[nodiscard]] bool enabled() const {
            return m_enabled;
        }
        [[nodiscard]] int manager_npc_index() const {
            return m_manager_npc_index;
        }
        [[nodiscard]] std::uint32_t donate_quest_key() const {
            return m_donate_quest.key();
        }
        [[nodiscard]] std::uint32_t donate_hundred_quest_key() const {
            return m_donate_hundred_quest.key();
        }
        /** Resolves a donation link without removing any inventory items.
         * @param quest_key Selected NPC quest link.
         * @param available Number of donation coins in the bag.
         * @return All available coins, exactly 100, or zero for an invalid/unaffordable request.
         */
        [[nodiscard]] int donation_amount(std::uint32_t quest_key, int available) const;

        [[nodiscard]] int donation_item_index() const {
            return m_donation_item_index;
        }
        [[nodiscard]] std::int32_t active_bonus_percent(std::int64_t now_unix) const;
        [[nodiscard]] std::int32_t remaining_seconds(std::int64_t now_unix) const;
        [[nodiscard]] std::int64_t progress_items() const {
            return m_state.contribution_count;
        }
        [[nodiscard]] std::int64_t next_target_items() const;

        /** Builds the public noticeboard state from the persisted event.
         * @param now_unix Current server time in seconds.
         * @return Current progress, reward and countdown.
         */
        [[nodiscard]] protocol::ExpEventStateFields snapshot(std::int64_t now_unix) const;

      private:
        /** Applies complete contribution batches to a staged state.
         * @param state Pending event state to update.
         * @param now_unix Current server time in seconds.
         * @return False if the resulting timer cannot be represented.
         */
        [[nodiscard]] bool apply_funded_steps(ServerEventRecord& state,
                                              std::int64_t now_unix) const;
        [[nodiscard]] bool save(IDataStore& store) const;

        bool m_enabled = false;
        std::string m_event_id = "exp_event";
        int m_manager_npc_index = 5037;
        ExpEventQuestStep m_donate_quest{9501, 1};
        ExpEventQuestStep m_donate_hundred_quest{9502, 1};
        int m_donation_item_index = 23016;
        std::int64_t m_duration_seconds = 3600;
        std::int64_t m_extension_seconds = 900;
        std::int64_t m_required_items = 500;
        std::int32_t m_bonus_step_percent = 10;
        std::int32_t m_max_bonus_percent = 50;
        ServerEventRecord m_state{m_event_id};
    };

}  // namespace openkal::server
