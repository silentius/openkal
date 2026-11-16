/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/ServerEvent.h"

#include "openkal/platform/File.h"
#include "openkal/platform/Json.h"

#include <algorithm>
#include <limits>

namespace openkal::server {

    namespace {

        [[nodiscard]] bool
        load_quest_step(const platform::Json& event, const char* name, ExpEventQuestStep& out) {
            const platform::Json& quest = event.at(name);
            if (!quest.is_object()) {
                return false;
            }
            const int index = quest.at("index").as_int(-1);
            const int flag = quest.at("flag").as_int(-1);
            if (index <= 0 || index > 65535 || flag < 0 || flag > 255) {
                return false;
            }
            out.index = static_cast<std::uint16_t>(index);
            out.flag = static_cast<std::uint8_t>(flag);
            return true;
        }

    }  // namespace

    bool ServerEventManager::load_file(const std::string& path) {
        m_enabled = false;
        const auto bytes = platform::File::read(path);
        if (!bytes) {
            return false;
        }
        platform::Json root;
        std::string error;
        if (!platform::json_parse(std::string(bytes->begin(), bytes->end()), root, &error) ||
            !root.is_object()) {
            return false;
        }
        const platform::Json& events = root.at("events");
        if (!events.is_array()) {
            return false;
        }
        for (const platform::Json& event : events.array()) {
            if (!event.is_object() || event.at("id").as_string() != m_event_id) {
                continue;
            }
            m_manager_npc_index = event.at("managerNpcIndex").as_int(m_manager_npc_index);
            if (!load_quest_step(event, "donateQuest", m_donate_quest) ||
                !load_quest_step(event, "donateHundredQuest", m_donate_hundred_quest)) {
                return false;
            }
            m_donation_item_index = event.at("donationItem").as_int(m_donation_item_index);
            m_duration_seconds = event.at("durationSeconds").as_int(3600);
            m_extension_seconds = event.at("extensionSeconds").as_int(900);
            m_required_items = event.at("requiredItems").as_int(500);
            m_bonus_step_percent = event.at("bonusStepPercent").as_int(10);
            m_max_bonus_percent = event.at("maxBonusPercent").as_int(50);
            m_enabled = m_manager_npc_index > 0 && m_donation_item_index > 0 &&
                        m_duration_seconds > 0 && m_extension_seconds > 0 && m_required_items > 0 &&
                        m_bonus_step_percent > 0 && m_max_bonus_percent >= m_bonus_step_percent &&
                        m_donate_quest.key() != m_donate_hundred_quest.key();
            break;
        }
        return m_enabled;
    }

    bool ServerEventManager::restore(IDataStore& store, std::int64_t now_unix) {
        m_state = {m_event_id};
        if (const auto stored = store.load_server_event(m_event_id)) {
            m_state = *stored;
            return advance(store, now_unix);
        }
        return save(store);
    }

    bool ServerEventManager::apply_funded_steps(ServerEventRecord& state,
                                                std::int64_t now_unix) const {
        if (state.active_until <= now_unix) {
            state.active_until = 0;
            state.active_bonus_percent = 0;
        }
        std::int64_t batches = state.contribution_count / m_required_items;
        if (batches <= 0) {
            return true;
        }
        state.contribution_count %= m_required_items;
        if (state.active_bonus_percent <= 0) {
            if (now_unix > std::numeric_limits<std::int64_t>::max() - m_duration_seconds) {
                return false;
            }
            state.active_bonus_percent = m_bonus_step_percent;
            state.active_until = now_unix + m_duration_seconds;
            --batches;
        }
        if (batches >
            (std::numeric_limits<std::int64_t>::max() - state.active_until) / m_extension_seconds) {
            return false;
        }
        const std::int64_t steps_to_cap = (static_cast<std::int64_t>(m_max_bonus_percent) -
                                           state.active_bonus_percent + m_bonus_step_percent - 1) /
                                          m_bonus_step_percent;
        state.active_bonus_percent = static_cast<std::int32_t>(std::min<std::int64_t>(
            m_max_bonus_percent,
            state.active_bonus_percent +
                std::min(batches, std::max<std::int64_t>(0, steps_to_cap)) * m_bonus_step_percent));
        state.active_until += batches * m_extension_seconds;
        return true;
    }

    bool ServerEventManager::advance(IDataStore& store, std::int64_t now_unix) {
        if (!m_enabled) {
            return true;
        }
        ServerEventRecord next = m_state;
        if (!apply_funded_steps(next, now_unix)) {
            return false;
        }
        if (next.contribution_count == m_state.contribution_count &&
            next.active_bonus_percent == m_state.active_bonus_percent &&
            next.active_until == m_state.active_until) {
            return true;
        }
        if (!store.save_server_event(next)) {
            return false;
        }
        m_state = next;
        return true;
    }

    int ServerEventManager::donation_amount(std::uint32_t quest_key, int available) const {
        if (!m_enabled || available <= 0) {
            return 0;
        }
        if (quest_key == m_donate_quest.key()) {
            return available;
        }
        return quest_key == m_donate_hundred_quest.key() && available >= 100 ? 100 : 0;
    }

    bool ServerEventManager::contribute(IDataStore& store,
                                        std::int64_t item_count,
                                        std::int64_t now_unix,
                                        ExpEventDonationResult& result) {
        result = {};
        if (!m_enabled || item_count <= 0 || m_state.contribution_count < 0 ||
            item_count > std::numeric_limits<std::int64_t>::max() - m_state.contribution_count) {
            return false;
        }
        ServerEventRecord next = m_state;
        next.contribution_count += item_count;
        if (!apply_funded_steps(next, now_unix) || !store.save_server_event(next)) {
            return false;
        }
        const bool was_active = active_bonus_percent(now_unix) > 0;
        result.activated = !was_active && next.active_bonus_percent > 0;
        result.extended = was_active && next.active_until > m_state.active_until;
        m_state = next;
        result.accepted = true;
        result.donated_items = item_count;
        result.progress_items = m_state.contribution_count;
        result.next_target_items = next_target_items();
        result.bonus_percent = active_bonus_percent(now_unix);
        result.active_until = m_state.active_until;
        return true;
    }

    std::int32_t ServerEventManager::active_bonus_percent(std::int64_t now_unix) const {
        return m_state.active_until > now_unix ? m_state.active_bonus_percent : 0;
    }

    std::int32_t ServerEventManager::remaining_seconds(std::int64_t now_unix) const {
        return m_state.active_until > now_unix
                   ? static_cast<std::int32_t>(std::min<std::int64_t>(
                         m_state.active_until - now_unix, std::numeric_limits<std::int32_t>::max()))
                   : 0;
    }

    std::int64_t ServerEventManager::next_target_items() const {
        return m_required_items;
    }

    protocol::ExpEventStateFields ServerEventManager::snapshot(std::int64_t now_unix) const {
        if (!m_enabled) {
            return {};
        }
        protocol::ExpEventStateFields result;
        result.enabled = true;
        result.manager_npc_index = m_manager_npc_index;
        result.donation_item_index = m_donation_item_index;
        result.contributed =
            static_cast<std::uint64_t>(std::max<std::int64_t>(0, progress_items()));
        result.target = static_cast<std::uint64_t>(next_target_items());
        result.active_bonus_percent = active_bonus_percent(now_unix);
        result.remaining_seconds = static_cast<std::uint32_t>(remaining_seconds(now_unix));
        result.next_bonus_percent = static_cast<std::int32_t>(std::min<std::int64_t>(
            m_max_bonus_percent,
            static_cast<std::int64_t>(result.active_bonus_percent) + m_bonus_step_percent));
        return result;
    }

    bool ServerEventManager::save(IDataStore& store) const {
        return store.save_server_event(m_state);
    }

}  // namespace openkal::server
