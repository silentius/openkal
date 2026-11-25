/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/CastleWarService.h"

#include "openkal/server/GuildService.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <tuple>

namespace openkal::server {
    namespace {
        using Json = platform::Json;

        constexpr std::int64_t kSecondsPerDay = 86400;
        // The platform serializer preserves full integer precision below 1e15.
        constexpr std::int64_t kMaxJsonInteger = 999999999999999;
        constexpr auto kWriteError = "Castle War state could not be saved.";

        Json array() {
            Json result;
            result.set_type(Json::Type::Array);
            return result;
        }

        template <typename... Values> Json row(Values... values) {
            Json result = array();
            const auto append = [&](double value) {
                Json field;
                field.set_number(value);
                result.array().push_back(std::move(field));
            };
            (append(static_cast<double>(values)), ...);
            return result;
        }

        template <typename T> bool read_number(const Json& field, T& output) {
            const double value = field.number();
            const double minimum = static_cast<double>(std::numeric_limits<T>::lowest());
            const double maximum = static_cast<double>(std::numeric_limits<T>::max());
            if (!field.is_number() || !std::isfinite(value) || std::floor(value) != value ||
                value < std::max<double>(minimum, -kMaxJsonInteger) ||
                value > std::min<double>(maximum, kMaxJsonInteger)) {
                return false;
            }
            output = static_cast<T>(value);
            return true;
        }

        bool read_number(const Json& field, CastleWarSide& output) {
            int side = 0;
            if (!read_number(field, side) || side < 0 || side > 2) {
                return false;
            }
            output = static_cast<CastleWarSide>(side);
            return true;
        }

        template <typename... Values> bool read_row(const Json& value, Values&... fields) {
            if (!value.is_array() || value.array().size() != sizeof...(Values)) {
                return false;
            }
            std::size_t offset = 0;
            return (read_number(value.array()[offset++], fields) && ...);
        }

        template <typename F> bool read_rows(const Json& document, const char* key, F read) {
            const auto& value = document.at(key);
            return value.is_array() &&
                   std::all_of(value.array().begin(), value.array().end(), read);
        }

        CastleWarPhase round_phase(const CastleWarRound& round, std::int64_t now) {
            if (now < round.declaration || now >= round.end) {
                return CastleWarPhase::Idle;
            }
            if (now < round.preparation) {
                return CastleWarPhase::Declaration;
            }
            if (now < round.admission) {
                return CastleWarPhase::Preparation;
            }
            if (now < round.battle) {
                return CastleWarPhase::Admission;
            }
            return CastleWarPhase::Battle;
        }

        void increment(int& value) {
            if (value < std::numeric_limits<int>::max()) {
                ++value;
            }
        }

        std::size_t side_offset(CastleWarSide side) {
            return side == CastleWarSide::Attack ? 0 : 1;
        }
    }  // namespace

    CastleWarService::CastleWarService(CastleWarRules rules,
                                       ICastleWarStore& storage,
                                       GuildService& guilds)
        : m_rules(std::move(rules)), m_storage(storage), m_guilds(guilds) {}

    bool CastleWarService::restore() {
        Json document;
        State loaded;
        if (!m_storage.load_castle_war(document) || !decode(document, loaded)) {
            return false;
        }
        for (std::size_t index = 0; index < loaded.flags.size(); ++index) {
            auto& flag = loaded.flags[index];
            flag.hp = flag.hp <= 0 ? m_rules.flags[index].hit_points
                                   : std::min(flag.hp, m_rules.flags[index].hit_points);
        }
        m_state = std::move(loaded);
        m_last_checkpoint = 0;
        m_dirty = false;
        return true;
    }

    bool CastleWarService::advance(std::int64_t now) {
        if (!m_rules.enabled) {
            return true;
        }
        if (restore_pending()) {
            if (!restore_when_returned()) {
                return true;
            }
            if (!checkpoint()) {
                return false;
            }
        }
        if (m_state.round.battle && !m_state.finalized && now >= m_state.round.end &&
            !finalize(m_state.round.end)) {
            return false;
        }
        if (!simulating() && !start_scheduled_round(now)) {
            return false;
        }
        const auto current = phase(now);
        if (current >= CastleWarPhase::Preparation && !m_state.locked && !lock_registration(now)) {
            return false;
        }
        if (current == CastleWarPhase::Battle) {
            if (hold_complete(now)) {
                return finalize(m_state.hold_since + m_rules.hold_seconds);
            }
            regenerate(now);
            if (m_dirty && now - m_last_checkpoint >= m_rules.match_score_flush_seconds) {
                if (!checkpoint()) {
                    return false;
                }
                m_last_checkpoint = now;
            }
        }
        return true;
    }

    bool CastleWarService::start_scheduled_round(std::int64_t now) {
        const auto upcoming = m_rules.round(now);
        if (now < upcoming.declaration || now >= upcoming.end ||
            upcoming.battle <= m_state.round.battle ||
            (m_state.round.battle && now < m_state.round.end)) {
            return true;
        }
        State next = m_state;
        next.round = upcoming;
        next.declarations.clear();
        next.registrations.clear();
        next.hold_since = 0;
        next.next_regen = 0;
        next.locked = false;
        next.finalized = false;
        for (auto it = next.alliances.begin(); it != next.alliances.end();) {
            if (!allied(it->first, it->second.leader, now)) {
                it = next.alliances.erase(it);
            } else {
                ++it;
            }
        }
        for (auto it = next.applications.begin(); it != next.applications.end();) {
            if (it->second.expires <= now || !m_guilds.contains_guild(it->first.first) ||
                !m_guilds.contains_guild(it->first.second)) {
                it = next.applications.erase(it);
            } else {
                ++it;
            }
        }
        for (std::size_t i = 0; i < next.flags.size(); ++i) {
            next.flags[i] = {CastleWarSide::None, 0, m_rules.flags[i].hit_points};
        }
        return commit(std::move(next));
    }

    bool CastleWarService::checkpoint() {
        return !m_dirty || commit(m_state);
    }

    CastleWarPhase CastleWarService::phase(std::int64_t now) const {
        if (!m_rules.enabled) {
            return CastleWarPhase::Idle;
        }
        if (simulating()) {
            return m_state.finalized ? CastleWarPhase::Idle : round_phase(m_state.round, now);
        }
        if (m_state.round.battle && now >= m_state.round.declaration && now < m_state.round.end) {
            return m_state.finalized ? CastleWarPhase::Idle : round_phase(m_state.round, now);
        }
        return m_rules.phase(now);
    }

    CastleWarRound CastleWarService::round(std::int64_t now) const {
        if (simulating() ||
            (m_state.round.battle && now >= m_state.round.declaration && now < m_state.round.end)) {
            return m_state.round;
        }
        return m_rules.round(now);
    }

    std::int64_t CastleWarService::phase_ends_at(std::int64_t now) const {
        const auto current = round(now);
        switch (phase(now)) {
        case CastleWarPhase::Declaration:
            return current.preparation;
        case CastleWarPhase::Preparation:
            return current.admission;
        case CastleWarPhase::Admission:
            return current.battle;
        case CastleWarPhase::Battle:
            return m_state.hold_since > 0
                       ? std::min(current.end, m_state.hold_since + m_rules.hold_seconds)
                       : current.end;
        case CastleWarPhase::Idle:
            if (simulating()) {
                return 0;
            }
            return now < current.declaration ? current.declaration
                                             : m_rules.round(current.end).declaration;
        }
        return 0;
    }

    std::string CastleWarService::simulation_start_blocker(std::int64_t now) const {
        if (!m_rules.enabled) {
            return "Castle War requires an enabled layout.";
        }
        if (restore_pending()) {
            return "Wait for the regular schedule to be restored.";
        }
        if (simulating()) {
            return "Restore the regular schedule before starting another simulation.";
        }
        const auto current = phase(now);
        const auto pending_online =
            std::any_of(m_state.players.begin(), m_state.players.end(), [&](const auto& entry) {
                return entry.second.online ||
                       (!m_state.finalized && entry.second.round == m_state.round.battle);
            });
        if (pending_online || current == CastleWarPhase::Admission ||
            current == CastleWarPhase::Battle) {
            return "Wait for the regular siege and participant returns to finish.";
        }
        return {};
    }

    std::string CastleWarService::start_simulation(const CastleWarSimulation& settings,
                                                   std::int64_t now) {
        if (auto error = simulation_start_blocker(now); !error.empty()) {
            return error;
        }
        if (settings.declaration_seconds < 0 || settings.declaration_seconds > 600 ||
            settings.preparation_seconds < 0 || settings.preparation_seconds > 600 ||
            settings.admission_seconds < 1 || settings.admission_seconds > 600 ||
            settings.battle_seconds < 1 || settings.battle_seconds > 3600 ||
            settings.capture_unlock_seconds < 0 ||
            settings.capture_unlock_seconds >= settings.battle_seconds ||
            settings.hold_seconds < 1 || settings.hold_seconds > 3600 ||
            settings.flag_hit_points < 1 || settings.flag_hit_points > 1000000) {
            return "Castle War simulation durations or flag hit points are out of range.";
        }
        if (settings.attack.empty()) {
            return "Choose at least one attacking guild.";
        }
        State next;
        next.round.declaration = now;
        next.round.preparation = now + settings.declaration_seconds;
        next.round.admission = next.round.preparation + settings.preparation_seconds;
        next.round.battle = next.round.admission + settings.admission_seconds;
        next.round.end = next.round.battle + settings.battle_seconds;
        std::set<std::uint32_t> guilds;
        const auto group_error = [&](const std::vector<std::uint32_t>& group) -> std::string {
            if (group.empty()) {
                return "Each Castle War alliance needs a leading guild.";
            }
            if (group.size() > static_cast<std::size_t>(m_rules.max_allied_guilds) + 1) {
                return "A Castle War alliance exceeds the configured guild limit.";
            }
            for (const auto guild : group) {
                if (!m_guilds.contains_guild(guild)) {
                    return "A Castle War guild does not exist.";
                }
                if (!guilds.insert(guild).second) {
                    return "A guild can belong to only one Castle War alliance.";
                }
                if (guild != group.front()) {
                    next.alliances.emplace(guild, Alliance{group.front(), next.round.end});
                }
            }
            return {};
        };
        for (const auto& group : settings.attack) {
            if (const auto error = group_error(group); !error.empty()) {
                return error;
            }
            next.declarations.insert(group.front());
        }
        if (!settings.defense.empty()) {
            if (const auto error = group_error(settings.defense); !error.empty()) {
                return error;
            }
            next.owner = settings.defense.front();
        }

        m_regular.emplace(RegularSchedule{std::move(m_state), m_rules, m_last_checkpoint, m_dirty});
        m_state = std::move(next);
        m_last_checkpoint = 0;
        m_dirty = false;
        m_rules.capture_unlock_seconds = settings.capture_unlock_seconds;
        m_rules.hold_seconds = settings.hold_seconds;
        for (std::size_t flag = 0; flag < m_rules.flags.size(); ++flag) {
            m_rules.flags[flag].hit_points = settings.flag_hit_points;
            m_state.flags[flag].hp = settings.flag_hit_points;
        }
        return advance(now) ? std::string{} : kWriteError;
    }

    std::string CastleWarService::finish_simulation(std::int64_t now) {
        if (!simulating()) {
            return "No Castle War simulation is active.";
        }
        if (phase(now) != CastleWarPhase::Battle) {
            return "The Castle War simulation is not in battle.";
        }
        return advance(now) && (m_state.finalized || finalize(now)) ? std::string{} : kWriteError;
    }

    std::string CastleWarService::restore_schedule(std::int64_t now) {
        if (!simulating()) {
            return {};
        }
        m_state.finalized = true;
        m_regular->restore_pending = true;
        return advance(now) ? std::string{} : kWriteError;
    }

    bool CastleWarService::simulating() const {
        return m_regular.has_value();
    }

    bool CastleWarService::restore_pending() const {
        return simulating() && m_regular->restore_pending;
    }

    bool CastleWarService::restore_when_returned() {
        if (std::any_of(m_state.players.begin(), m_state.players.end(), [](const auto& entry) {
                return entry.second.online;
            })) {
            return false;
        }
        for (const auto& [character, player] : m_state.players) {
            m_simulation_returns.insert_or_assign(character, player.data);
        }
        m_state = std::move(m_regular->state);
        m_rules = std::move(m_regular->rules);
        m_last_checkpoint = m_regular->last_checkpoint;
        m_dirty = m_regular->dirty;
        m_regular.reset();
        return true;
    }

    const CastleWarRules& CastleWarService::rules() const {
        return m_rules;
    }

    std::uint32_t CastleWarService::owner() const {
        return m_guilds.contains_guild(m_state.owner) ? m_state.owner : 0;
    }

    CastleWarSide CastleWarService::last_winner() const {
        return m_state.winner;
    }

    std::int64_t CastleWarService::honor(std::uint32_t character) const {
        const auto result = m_state.honor.find(character);
        return result == m_state.honor.end() ? 0 : result->second;
    }

    bool CastleWarService::alliance_changes_open(std::int64_t now) const {
        if (simulating() ||
            (m_state.round.battle && now >= m_state.round.preparation && now < m_state.round.end)) {
            return false;
        }
        const auto current = m_rules.phase(now);
        return m_rules.enabled &&
               (current == CastleWarPhase::Idle || current == CastleWarPhase::Declaration);
    }

    bool
    CastleWarService::allied(std::uint32_t guild, std::uint32_t alliance, std::int64_t now) const {
        const auto link = m_state.alliances.find(guild);
        return link != m_state.alliances.end() && link->second.leader == alliance &&
               link->second.expires > now && m_guilds.contains_guild(guild) &&
               m_guilds.contains_guild(alliance);
    }

    std::uint32_t CastleWarService::leader(std::uint32_t guild, std::int64_t now) const {
        const auto link = m_state.alliances.find(guild);
        if (link != m_state.alliances.end() && allied(guild, link->second.leader, now)) {
            return link->second.leader;
        }
        return guild;
    }

    std::string CastleWarService::invite(std::uint32_t character,
                                         std::uint32_t target_guild,
                                         int days,
                                         std::int64_t now) {
        if (!alliance_changes_open(now)) {
            return "Alliance changes are closed during Castle War.";
        }
        const auto applicant = m_guilds.membership(character);
        if (!m_guilds.is_leader(character) || target_guild == applicant ||
            !m_guilds.contains_guild(target_guild)) {
            return "A guild leader must apply to a different leading guild.";
        }
        if (days < m_rules.alliance_min_days || days > m_rules.alliance_max_days) {
            return "Alliance duration is outside the allowed range.";
        }
        if (leader(applicant, now) != applicant || leader(target_guild, now) != target_guild ||
            std::any_of(m_state.alliances.begin(), m_state.alliances.end(), [&](const auto& item) {
                return allied(item.first, applicant, now);
            })) {
            return "Only a guild without allies can apply to a leading guild.";
        }
        State next = m_state;
        for (auto it = next.applications.begin(); it != next.applications.end();) {
            if (it->first.second == applicant || it->second.expires <= now) {
                it = next.applications.erase(it);
            } else {
                ++it;
            }
        }
        next.applications[{target_guild, applicant}] = {days,
                                                        now + m_rules.alliance_invitation_seconds};
        return commit(std::move(next)) ? "" : kWriteError;
    }

    std::string CastleWarService::accept(std::uint32_t character,
                                         std::uint32_t inviting_guild,
                                         std::int64_t now) {
        if (!alliance_changes_open(now)) {
            return "Alliance changes are closed during Castle War.";
        }
        const auto alliance = m_guilds.membership(character);
        if (!m_guilds.is_leader(character) || !m_guilds.contains_guild(inviting_guild) ||
            leader(alliance, now) != alliance || leader(inviting_guild, now) != inviting_guild) {
            return "Only the leading guild can accept an independent applicant.";
        }
        const auto application = m_state.applications.find({alliance, inviting_guild});
        if (application == m_state.applications.end() || application->second.expires <= now ||
            application->second.days < m_rules.alliance_min_days ||
            application->second.days > m_rules.alliance_max_days) {
            return "There is no active alliance application from that guild.";
        }
        if (std::any_of(m_state.alliances.begin(), m_state.alliances.end(), [&](const auto& item) {
                return allied(item.first, inviting_guild, now);
            })) {
            return "A guild leading an alliance cannot join another alliance.";
        }
        const auto member_count =
            std::count_if(m_state.alliances.begin(),
                          m_state.alliances.end(),
                          [&](const auto& item) { return allied(item.first, alliance, now); });
        if (member_count >= m_rules.max_allied_guilds) {
            return "This alliance has reached the guild limit.";
        }
        State next = m_state;
        next.alliances[inviting_guild] = {alliance,
                                          now + application->second.days * kSecondsPerDay};
        next.applications.erase({alliance, inviting_guild});
        return commit(std::move(next)) ? "" : kWriteError;
    }

    std::string CastleWarService::leave(std::uint32_t character, std::int64_t now) {
        if (!alliance_changes_open(now) || !m_guilds.is_leader(character)) {
            return "Only a guild leader can leave while alliance changes are open.";
        }
        const auto guild = m_guilds.membership(character);
        State next = m_state;
        auto removed = next.alliances.erase(guild);
        for (auto it = next.alliances.begin(); it != next.alliances.end();) {
            if (it->second.leader == guild) {
                it = next.alliances.erase(it);
                ++removed;
            } else {
                ++it;
            }
        }
        if (removed == 0) {
            return "Your guild has no Castle War alliance.";
        }
        return commit(std::move(next)) ? "" : kWriteError;
    }

    std::string CastleWarService::declare_attack(std::uint32_t character, std::int64_t now) {
        if (!advance(now)) {
            return kWriteError;
        }
        if (phase(now) != CastleWarPhase::Declaration || !m_guilds.is_leader(character)) {
            return "Only a guild leader can register during the declaration period.";
        }
        const auto guild = m_guilds.membership(character);
        if (leader(guild, now) != guild || (owner() && leader(owner(), now) == guild)) {
            return "Only the leading guild of a group outside the defense may register.";
        }
        if (m_state.declarations.count(guild)) {
            return {};
        }
        if (simulating()) {
            return "This guild is not assigned to attack in the simulation.";
        }
        State next = m_state;
        next.declarations.insert(guild);
        return commit(std::move(next)) ? "" : kWriteError;
    }

    bool CastleWarService::lock_registration(std::int64_t now) {
        State next = m_state;
        const auto cutoff = next.round.preparation;
        const auto defender = owner();
        if (defender) {
            const auto group = leader(defender, cutoff);
            for (const auto& [member, link] : m_state.alliances) {
                if (allied(member, group, cutoff)) {
                    next.registrations[member] = {group, CastleWarSide::Defense};
                }
            }
            next.registrations[group] = {group, CastleWarSide::Defense};
            for (auto& flag : next.flags) {
                flag.side = CastleWarSide::Defense;
                flag.alliance = group;
            }
        }
        for (auto guild : next.declarations) {
            if (!m_guilds.contains_guild(guild) || leader(guild, cutoff) != guild ||
                next.registrations.count(guild)) {
                continue;
            }
            next.registrations[guild] = {guild, CastleWarSide::Attack};
            for (const auto& [member, link] : m_state.alliances) {
                if (allied(member, guild, cutoff)) {
                    next.registrations[member] = {guild, CastleWarSide::Attack};
                }
            }
        }
        next.locked = true;
        next.next_regen = std::max(now, next.round.battle) + m_rules.flag_regen_seconds;
        return commit(std::move(next));
    }

    std::string CastleWarService::admit(std::uint32_t character,
                                        int level,
                                        CastleWarPosition return_position,
                                        std::int64_t now) {
        if (!advance(now)) {
            return kWriteError;
        }
        if (phase(now) != CastleWarPhase::Admission || !m_state.locked) {
            return "The Castle War admission window is closed.";
        }
        if (const auto old = m_state.players.find(character); old != m_state.players.end()) {
            return old->second.round == m_state.round.battle
                       ? "You have already joined this siege."
                       : "Your previous Castle War return is pending.";
        }
        if (m_simulation_returns.count(character) ||
            (simulating() && m_regular->state.players.count(character))) {
            return "Your previous Castle War return is pending.";
        }
        const auto guild = m_guilds.membership(character);
        const auto entry = m_state.registrations.find(guild);
        if (entry == m_state.registrations.end() || level < m_rules.minimum_level) {
            return "Your guild or character is not eligible to join this siege.";
        }
        State next = m_state;
        auto& player = next.players[character];
        player.data = {
            guild, entry->second.alliance, entry->second.side, level, return_position, 0};
        player.round = next.round.battle;
        player.online = true;
        player.settlement_eligible = true;
        return commit(std::move(next)) ? "" : kWriteError;
    }

    bool CastleWarService::registered(std::uint32_t character) const {
        return m_state.registrations.count(m_guilds.membership(character)) != 0;
    }

    std::optional<CastleWarParticipant>
    CastleWarService::participant(std::uint32_t character) const {
        const auto entry = m_state.players.find(character);
        if (entry != m_state.players.end()) {
            return entry->second.data;
        }
        const auto pending = m_simulation_returns.find(character);
        if (pending != m_simulation_returns.end()) {
            return pending->second;
        }
        if (simulating()) {
            const auto regular = m_regular->state.players.find(character);
            if (regular != m_regular->state.players.end()) {
                return regular->second.data;
            }
        }
        return std::nullopt;
    }

    bool CastleWarService::active(std::uint32_t character, std::int64_t now) const {
        const auto entry = m_state.players.find(character);
        const auto current = phase(now);
        return entry != m_state.players.end() && entry->second.round == m_state.round.battle &&
               (current == CastleWarPhase::Admission || current == CastleWarPhase::Battle);
    }

    CastleWarPosition CastleWarService::camp(CastleWarSide side) const {
        return side == CastleWarSide::Attack ? m_rules.attack_camp : m_rules.defense_camp;
    }

    bool CastleWarService::disconnect(std::uint32_t character, std::int64_t now) {
        if (!active(character, now) || hold_complete(now) ||
            !m_state.players.at(character).settlement_eligible) {
            const auto found = m_state.players.find(character);
            if (found != m_state.players.end()) {
                found->second.online = false;
            }
            return true;
        }
        State next = m_state;
        auto& player = next.players.at(character);
        player.online = false;
        player.settlement_eligible = false;
        player.kills = 0;
        player.deaths = 0;
        player.activity = 0;
        player.skills = 0;
        return commit(std::move(next));
    }

    bool CastleWarService::reconnect(std::uint32_t character, std::int64_t now) {
        return reset_in_camp(character, now, true);
    }

    bool CastleWarService::revive(std::uint32_t character, std::int64_t now) {
        return reset_in_camp(character, now, false);
    }

    bool
    CastleWarService::reset_in_camp(std::uint32_t character, std::int64_t now, bool reset_score) {
        if (!active(character, now)) {
            return false;
        }
        State next = m_state;
        auto& player = next.players.at(character);
        if (reset_score && !hold_complete(now)) {
            player.kills = 0;
            player.deaths = 0;
            player.activity = 0;
            player.skills = 0;
        }
        player.online = true;
        if (!hold_complete(now)) {
            player.settlement_eligible = true;
        }
        player.data.protected_until = now + m_rules.protection_seconds;
        return commit(std::move(next));
    }

    bool CastleWarService::depart(std::uint32_t character) {
        const auto entry = m_state.players.find(character);
        if (entry == m_state.players.end()) {
            m_simulation_returns.erase(character);
            if (simulating() && m_regular->state.players.erase(character)) {
                m_regular->dirty = true;
            }
            return true;
        }
        if (entry->second.round == m_state.round.battle && !m_state.finalized) {
            return false;
        }
        State next = m_state;
        next.players.erase(character);
        return commit(std::move(next));
    }

    bool
    CastleWarService::opposed(std::uint32_t source, std::uint32_t target, std::int64_t now) const {
        return active(source, now) && active(target, now) &&
               m_state.players.at(source).data.side != m_state.players.at(target).data.side;
    }

    bool
    CastleWarService::hostile(std::uint32_t source, std::uint32_t target, std::int64_t now) const {
        if (!battle_open(now) || !opposed(source, target, now)) {
            return false;
        }
        const auto& attacker = m_state.players.at(source);
        const auto& defender = m_state.players.at(target);
        return attacker.online && defender.online && now >= attacker.data.protected_until &&
               now >= defender.data.protected_until;
    }

    bool
    CastleWarService::record_kill(std::uint32_t source, std::uint32_t target, std::int64_t now) {
        if (!battle_open(now) || !opposed(source, target, now)) {
            return false;
        }
        auto& attacker = m_state.players.at(source);
        auto& victim = m_state.players.at(target);
        if (!attacker.online || !victim.online) {
            return false;
        }
        increment(attacker.kills);
        increment(attacker.activity);
        increment(victim.deaths);
        m_dirty = true;
        return true;
    }

    bool CastleWarService::record_activity(std::uint32_t character,
                                           CastleWarActivity activity,
                                           std::int64_t now) {
        if (!battle_open(now) || !active(character, now)) {
            return false;
        }
        auto& player = m_state.players.at(character);
        if (!player.online) {
            return false;
        }
        if (activity == CastleWarActivity::Skill) {
            if (player.skills >= 10) {
                return false;
            }
            ++player.skills;
        }
        increment(player.activity);
        m_dirty = true;
        return true;
    }

    bool CastleWarService::update_level(std::uint32_t character, int level, std::int64_t now) {
        if (!active(character, now) || level <= 0) {
            return false;
        }
        auto& player = m_state.players.at(character);
        if (player.data.level != level) {
            player.data.level = level;
            m_dirty = true;
        }
        return true;
    }

    CastleWarObjective CastleWarService::objective(std::size_t flag) const {
        return flag < m_state.flags.size() ? m_state.flags[flag] : CastleWarObjective{};
    }

    bool CastleWarService::can_damage_flag(std::size_t flag,
                                           std::uint32_t character,
                                           std::int64_t now) const {
        if (flag >= m_state.flags.size() || !battle_open(now) || !active(character, now) ||
            now < m_state.round.battle + m_rules.capture_unlock_seconds) {
            return false;
        }
        const auto& attacker = m_state.players.at(character);
        return attacker.online && now >= attacker.data.protected_until &&
               attacker.data.side != m_state.flags[flag].side;
    }

    bool CastleWarService::damage_flag(std::size_t flag,
                                       std::uint32_t character,
                                       int damage,
                                       std::int64_t now) {
        if (damage <= 0 || !can_damage_flag(flag, character, now)) {
            return false;
        }
        regenerate(now);
        if (damage < m_state.flags[flag].hp) {
            m_state.flags[flag].hp -= damage;
            m_dirty = true;
            return true;
        }
        State next = m_state;
        auto& claimant = next.players.at(character);
        next.flags[flag] = {
            claimant.data.side, claimant.data.alliance, m_rules.flags[flag].hit_points};
        increment(claimant.captures);
        increment(claimant.activity);
        next.hold_since = all_flags_side(next) != CastleWarSide::None ? now : 0;
        return commit(std::move(next));
    }

    void CastleWarService::regenerate(std::int64_t now) {
        if (m_rules.flag_regen_seconds <= 0 || m_rules.flag_regen_points <= 0 ||
            now < m_state.next_regen) {
            return;
        }
        const auto elapsed = 1 + (now - m_state.next_regen) / m_rules.flag_regen_seconds;
        m_state.next_regen = now + m_rules.flag_regen_seconds -
                             (now - m_state.next_regen) % m_rules.flag_regen_seconds;
        for (std::size_t flag = 0; flag < m_state.flags.size(); ++flag) {
            auto& objective = m_state.flags[flag];
            const int missing = m_rules.flags[flag].hit_points - objective.hp;
            const auto effective_ticks =
                std::min<std::int64_t>(elapsed, missing / m_rules.flag_regen_points + 1LL);
            const auto gained =
                std::min<std::int64_t>(missing, effective_ticks * m_rules.flag_regen_points);
            if (gained > 0) {
                objective.hp += static_cast<int>(gained);
                m_dirty = true;
            }
        }
    }

    CastleWarSide CastleWarService::all_flags_side(const State& state) const {
        const auto side = state.flags.front().side;
        if (side == CastleWarSide::None ||
            !std::all_of(state.flags.begin(), state.flags.end(), [&](const auto& flag) {
                return flag.side == side;
            })) {
            return CastleWarSide::None;
        }
        return side;
    }

    std::array<int, 2> CastleWarService::score() const {
        return score(m_state);
    }

    bool CastleWarService::hold_complete(std::int64_t now) const {
        return m_state.hold_since && now - m_state.hold_since >= m_rules.hold_seconds &&
               all_flags_side(m_state) != CastleWarSide::None;
    }

    bool CastleWarService::battle_open(std::int64_t now) const {
        return phase(now) == CastleWarPhase::Battle && !hold_complete(now);
    }

    std::array<int, 2> CastleWarService::score(const State& state) const {
        std::array<int, 2> result{};
        for (std::size_t flag = 0; flag < state.flags.size(); ++flag) {
            if (state.flags[flag].side != CastleWarSide::None) {
                result[side_offset(state.flags[flag].side)] += m_rules.flags[flag].points;
            }
        }
        return result;
    }

    std::uint32_t CastleWarService::select_lord(CastleWarSide winner) const {
        struct Result {
            int flags = 0;
            std::int64_t kills = 0;
            std::int64_t levels = 0;
            std::int64_t players = 0;
        };
        std::map<std::uint32_t, Result> results;
        for (const auto& [character, player] : m_state.players) {
            if (player.round != m_state.round.battle || player.data.side != winner ||
                !player.settlement_eligible || !m_guilds.contains_guild(player.data.alliance)) {
                continue;
            }
            auto& candidate = results[player.data.alliance];
            candidate.kills += player.kills;
            candidate.levels += player.data.level;
            ++candidate.players;
        }
        for (std::size_t offset = 0; offset < m_state.flags.size(); ++offset) {
            const auto& flag = m_state.flags[offset];
            if (flag.side == winner && m_guilds.contains_guild(flag.alliance)) {
                results[flag.alliance].flags += m_rules.flags[offset].points;
            }
        }
        std::uint32_t lord = winner == CastleWarSide::Defense ? owner() : 0;
        std::tuple<int, std::int64_t, long double> best{-1, 0, 0};
        for (const auto& [guild, result] : results) {
            const long double average =
                result.players > 0 ? static_cast<long double>(result.levels) / result.players : 0;
            const auto performance = std::make_tuple(result.flags, result.kills, average);
            if (performance > best || (performance == best && guild == owner())) {
                lord = guild;
                best = performance;
            }
        }
        return lord;
    }

    bool CastleWarService::finalize(std::int64_t occurred_at) {
        const auto [attack, defense] = score();
        const auto winner = attack > defense ? CastleWarSide::Attack : CastleWarSide::Defense;
        const auto previous_owner = owner();
        const auto lord = select_lord(winner);
        State next = m_state;
        next.finalized = true;
        next.winner = winner;
        next.owner = lord;
        if (lord != previous_owner || !lord) {
            next.defenses = 0;
            next.tax_policy = 1;
        } else if (winner == CastleWarSide::Defense &&
                   std::any_of(next.players.begin(), next.players.end(), [&](const auto& entry) {
                       return entry.second.round == next.round.battle &&
                              entry.second.data.side == CastleWarSide::Attack;
                   })) {
            increment(next.defenses);
            next.defenses = std::min(next.defenses, m_rules.max_defenses);
        }
        std::vector<HonorGrant> awards;
        const auto instance = std::to_string(m_state.round.battle);
        for (const auto& [character, player] : m_state.players) {
            if (player.round != m_state.round.battle || !player.settlement_eligible ||
                player.activity < m_rules.activity_minimum) {
                continue;
            }
            auto reward = static_cast<std::int64_t>(m_rules.honor_loser);
            if (player.data.side == winner) {
                reward = player.data.guild == lord ? m_rules.honor_winner : m_rules.honor_ally;
            }
            reward += std::min<std::int64_t>(m_rules.honor_flag_max, player.captures * 500LL);
            reward += std::min<std::int64_t>(m_rules.honor_kill_max, player.kills * 15LL);
            reward += std::min<std::int64_t>(m_rules.honor_activity_max, player.activity * 10LL);
            auto& earned = next.honor[character];
            earned = std::min(kMaxJsonInteger, earned + reward);
            if (!simulating() && reward > 0) {
                awards.push_back({"castle_war",
                                  instance,
                                  "settlement",
                                  static_cast<std::int32_t>(character),
                                  occurred_at,
                                  reward,
                                  0});
            }
        }
        return commit(std::move(next), awards);
    }

    std::pair<int, int> CastleWarService::pvp_bonus(std::uint32_t character,
                                                    std::int64_t now) const {
        if (!battle_open(now) || !active(character, now)) {
            return {};
        }
        const auto& player = m_state.players.at(character);
        if (!player.online || player.data.side != CastleWarSide::Attack || player.deaths == 0) {
            return {};
        }
        const auto ratio = player.kills * 100LL / player.deaths;
        if (ratio == 0 || ratio >= 190) {
            return {};
        }
        if (ratio >= 140) {
            return {10, 10};
        }
        if (ratio >= 80) {
            return {15, 15};
        }
        if (ratio >= 40) {
            return {20, 25};
        }
        return {30, 40};
    }

    int CastleWarService::tax_percent() const {
        const auto& state = simulating() ? m_regular->state : m_state;
        return m_rules.tax_percent[m_guilds.contains_guild(state.owner) ? state.tax_policy : 1];
    }

    int CastleWarService::tax_multiplier_percent() const {
        const auto& state = simulating() ? m_regular->state : m_state;
        if (!m_guilds.contains_guild(state.owner) || m_rules.max_defenses <= 0) {
            return 100;
        }
        return 100 + static_cast<int>(100LL * std::min(state.defenses, m_rules.max_defenses) /
                                      m_rules.max_defenses);
    }

    std::string CastleWarService::set_tax(std::uint32_t character, int policy) {
        if (simulating()) {
            return "Castle War simulation does not change territory prices.";
        }
        if (policy < 0 || policy >= static_cast<int>(m_rules.tax_percent.size()) ||
            !m_guilds.is_leader(character) || m_guilds.membership(character) != owner()) {
            return "Only the castle lord may select a valid tax policy.";
        }
        if (policy == m_state.tax_policy) {
            return {};
        }
        State next = m_state;
        next.tax_policy = policy;
        return commit(std::move(next)) ? "" : kWriteError;
    }

    bool CastleWarService::commit(State next, const std::vector<HonorGrant>& awards) {
        if (simulating()) {
            m_state = std::move(next);
            m_dirty = false;
            return true;
        }
        if (m_state.revision >= static_cast<std::uint64_t>(kMaxJsonInteger)) {
            return false;
        }
        next.revision = m_state.revision + 1;
        if (!m_storage.commit_castle_war(m_state.revision, encode(next), awards)) {
            return false;
        }
        m_state = std::move(next);
        m_dirty = false;
        return true;
    }

    platform::Json CastleWarService::encode(const State& state) {
        Json document;
        document.set_type(Json::Type::Object);
        auto& fields = document.object();
        fields["revision"].set_number(static_cast<double>(state.revision));
        fields["version"].set_number(1);
        fields["castle"] = row(state.owner, state.tax_policy, state.defenses, state.winner);
        fields["round"] = row(state.round.declaration,
                              state.round.preparation,
                              state.round.admission,
                              state.round.battle,
                              state.round.end,
                              state.hold_since,
                              state.next_regen);
        fields["locked"].set_boolean(state.locked);
        fields["finalized"].set_boolean(state.finalized);
        for (auto key : {"alliances",
                         "applications",
                         "declarations",
                         "registrations",
                         "players",
                         "flags",
                         "honor"}) {
            fields[key] = array();
        }
        for (const auto& [guild, alliance] : state.alliances) {
            fields["alliances"].array().push_back(row(guild, alliance.leader, alliance.expires));
        }
        for (const auto& [pair, application] : state.applications) {
            fields["applications"].array().push_back(
                row(pair.first, pair.second, application.days, application.expires));
        }
        for (auto guild : state.declarations) {
            Json declaration;
            declaration.set_number(guild);
            fields["declarations"].array().push_back(std::move(declaration));
        }
        for (const auto& [guild, registration] : state.registrations) {
            fields["registrations"].array().push_back(
                row(guild, registration.alliance, registration.side));
        }
        for (const auto& [character, player] : state.players) {
            const auto& enrollment = player.data;
            const auto& position = enrollment.return_position;
            fields["players"].array().push_back(row(character,
                                                    player.round,
                                                    enrollment.guild,
                                                    enrollment.alliance,
                                                    enrollment.side,
                                                    enrollment.level,
                                                    position.map,
                                                    position.x,
                                                    position.y,
                                                    position.z,
                                                    enrollment.protected_until,
                                                    player.kills,
                                                    player.deaths,
                                                    player.captures,
                                                    player.activity,
                                                    player.skills,
                                                    player.settlement_eligible));
        }
        for (const auto& flag : state.flags) {
            fields["flags"].array().push_back(row(flag.side, flag.alliance, flag.hp));
        }
        for (const auto& [character, honor] : state.honor) {
            fields["honor"].array().push_back(row(character, honor));
        }
        return document;
    }

    bool CastleWarService::decode(const Json& document, State& output) {
        if (!read_number(document.at("revision"), output.revision)) {
            return false;
        }
        if (output.revision == 0) {
            return document.object().size() == 1;
        }
        int version = 0;
        auto& round = output.round;
        if (!read_number(document.at("version"), version) || version != 1 ||
            !read_row(document.at("castle"),
                      output.owner,
                      output.tax_policy,
                      output.defenses,
                      output.winner) ||
            output.tax_policy < 0 || output.tax_policy > 2 || output.defenses < 0 ||
            !read_row(document.at("round"),
                      round.declaration,
                      round.preparation,
                      round.admission,
                      round.battle,
                      round.end,
                      output.hold_since,
                      output.next_regen) ||
            (round.battle &&
             !(round.declaration < round.preparation && round.preparation <= round.admission &&
               round.admission < round.battle && round.battle < round.end)) ||
            document.at("locked").type() != Json::Type::Bool ||
            document.at("finalized").type() != Json::Type::Bool) {
            return false;
        }
        output.locked = document.at("locked").boolean();
        output.finalized = document.at("finalized").boolean();
        std::size_t flag = 0;
        return read_rows(document,
                         "alliances",
                         [&](const Json& value) {
                             std::uint32_t guild = 0;
                             Alliance link;
                             return read_row(value, guild, link.leader, link.expires) && guild &&
                                    link.leader && guild != link.leader &&
                                    output.alliances.emplace(guild, link).second;
                         }) &&
               read_rows(
                   document,
                   "applications",
                   [&](const Json& value) {
                       std::pair<std::uint32_t, std::uint32_t> pair;
                       Application request;
                       return read_row(
                                  value, pair.first, pair.second, request.days, request.expires) &&
                              pair.first && pair.second && pair.first != pair.second &&
                              request.days > 0 && output.applications.emplace(pair, request).second;
                   }) &&
               read_rows(document,
                         "declarations",
                         [&](const Json& value) {
                             std::uint32_t guild = 0;
                             return read_number(value, guild) && guild &&
                                    output.declarations.insert(guild).second;
                         }) &&
               read_rows(document,
                         "registrations",
                         [&](const Json& value) {
                             std::uint32_t guild = 0;
                             Registration registration;
                             return read_row(
                                        value, guild, registration.alliance, registration.side) &&
                                    guild && registration.alliance &&
                                    registration.side != CastleWarSide::None &&
                                    output.registrations.emplace(guild, registration).second;
                         }) &&
               read_rows(document,
                         "players",
                         [&](const Json& value) {
                             std::uint32_t character = 0;
                             int eligible = 0;
                             Player player;
                             auto& data = player.data;
                             auto& position = data.return_position;
                             const bool valid =
                                 read_row(value,
                                          character,
                                          player.round,
                                          data.guild,
                                          data.alliance,
                                          data.side,
                                          data.level,
                                          position.map,
                                          position.x,
                                          position.y,
                                          position.z,
                                          data.protected_until,
                                          player.kills,
                                          player.deaths,
                                          player.captures,
                                          player.activity,
                                          player.skills,
                                          eligible) &&
                                 character && player.round <= round.battle && data.guild &&
                                 data.alliance && data.side != CastleWarSide::None &&
                                 data.level > 0 && player.kills >= 0 && player.deaths >= 0 &&
                                 player.captures >= 0 && player.activity >= 0 &&
                                 player.skills >= 0 && player.skills <= 10 && eligible >= 0 &&
                                 eligible <= 1;
                             player.settlement_eligible = eligible != 0;
                             return valid && output.players.emplace(character, player).second;
                         }) &&
               read_rows(
                   document,
                   "flags",
                   [&](const Json& value) {
                       if (flag >= output.flags.size()) {
                           return false;
                       }
                       auto& objective = output.flags[flag++];
                       return read_row(value, objective.side, objective.alliance, objective.hp) &&
                              objective.hp >= 0 &&
                              (objective.side == CastleWarSide::None) == (objective.alliance == 0);
                   }) &&
               flag == output.flags.size() && read_rows(document, "honor", [&](const Json& value) {
                   std::uint32_t character = 0;
                   std::int64_t honor = 0;
                   return read_row(value, character, honor) && character && honor >= 0 &&
                          output.honor.emplace(character, honor).second;
               });
    }
}  // namespace openkal::server
