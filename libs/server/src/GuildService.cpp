/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/GuildService.h"

#include "openkal/protocol/Party.h"
#include "openkal/server/TestControl.h"

#include <algorithm>
#include <limits>
#include <sstream>

namespace openkal::server {
    namespace {
        using J = platform::Json;
        using C = protocol::GuildCommand;
        J obj() {
            J j;
            j.set_type(J::Type::Object);
            return j;
        }
        J arr() {
            J j;
            j.set_type(J::Type::Array);
            return j;
        }
        J num(std::int64_t n) {
            J j;
            j.set_number(static_cast<double>(n));
            return j;
        }
        J str(std::string_view s) {
            J j;
            j.set_type(J::Type::String);
            j.string() = s;
            return j;
        }
        J boolean(bool b) {
            J j;
            j.set_boolean(b);
            return j;
        }
        void set(J& j, std::string key, J value) {
            j.object()[std::move(key)] = std::move(value);
        }
        std::int64_t n(const J& j, std::string_view k) {
            return static_cast<std::int64_t>(j.at(k).number());
        }
        std::string s(const J& j, std::string_view k) {
            return j.at(k).as_string();
        }
        bool read_only(C c) {
            return c <= C::MyApplications;
        }
        std::uint32_t permission(C c) {
            switch (c) {
            case C::Accept:
            case C::Reject:
            case C::Invitation:
                return 1;
            case C::Expel:
                return 2;
            case C::Announcement:
                return 4;
            case C::Rank:
                return 8;
            case C::Title:
                return 16;
            default:
                return 0;
            }
        }
        std::string fold(std::string text) {
            for (auto& c : text) {
                if (c >= 'A' && c <= 'Z') {
                    c = static_cast<char>(c + 32);
                }
            }
            return text;
        }
    }  // namespace
    GuildService::GuildService(GuildRules rules, IGuildStore& store, IDataStore& characters)
        : m_rules(std::move(rules)), m_store(store), m_characters(characters) {}
    bool GuildService::restore() {
        if (!m_store.load_guilds(m_state) || !m_state.is_object()) {
            return false;
        }
        for (const auto* key : {"guilds", "characters", "applications", "receipts", "ranking"}) {
            if (!m_state.at(key).is_array()) {
                set(m_state, key, arr());
            }
        }
        if (!m_state.get("next_id")) {
            set(m_state, "next_id", num(1));
        }
        const auto previous = m_state;
        for (auto& g : m_state.object()["guilds"].array()) {
            recalculate(g);
        }
        return commit(previous, {});
    }
    std::uint64_t GuildService::revision() const {
        return static_cast<std::uint64_t>(n(m_state, "revision"));
    }

    J GuildService::test_control(const J& request, std::int64_t now) {
        const J previous = m_state;
        auto fail = [&](std::string message) {
            m_state = previous;
            auto out = obj();
            set(out, "error", str(message));
            return out;
        };
        const auto operation = request.at("op").as_string();
        if (operation == "guilds") {
            const int count = request.at("count").as_int(-1);
            if (count < -1 || count > 100) {
                return fail("Guild count must be 0 through 100.");
            }
            if (count >= 0) {
                if (!m_state.at("test_archived_guilds").is_array()) {
                    set(m_state, "test_archived_guilds", arr());
                }
                auto& groups = m_state.object()["guilds"].array();
                auto& archived = m_state.object()["test_archived_guilds"].array();
                std::set<std::int64_t> removed;
                for (auto it = groups.begin(); it != groups.end();) {
                    if (n(*it, "test_ordinal") <= count || !n(*it, "test_ordinal")) {
                        ++it;
                        continue;
                    }
                    removed.insert(n(*it, "id"));
                    auto saved = *it;
                    set(saved, "members", arr());
                    archived.push_back(std::move(saved));
                    it = groups.erase(it);
                }
                auto& applications = m_state.object()["applications"].array();
                applications.erase(
                    std::remove_if(applications.begin(),
                                   applications.end(),
                                   [&](const J& app) { return removed.count(n(app, "guild")); }),
                    applications.end());
                auto& ranking = m_state.object()["ranking"].array();
                ranking.erase(
                    std::remove_if(ranking.begin(),
                                   ranking.end(),
                                   [&](const J& entry) { return removed.count(n(entry, "id")); }),
                    ranking.end());
                if (request.at("active_ids").is_array()) {
                    std::set<std::int64_t> active;
                    for (const auto& id : request.at("active_ids").array()) {
                        active.insert(static_cast<std::int64_t>(id.number()));
                    }
                    for (auto& group : groups) {
                        if (!n(group, "test_ordinal")) {
                            continue;
                        }
                        auto& members = group.object()["members"].array();
                        members.erase(
                            std::remove_if(members.begin(),
                                           members.end(),
                                           [&](const J& m) { return !active.count(n(m, "id")); }),
                            members.end());
                    }
                }
            }
            for (int ordinal = 1; ordinal <= count; ++ordinal) {
                auto& groups = m_state.object()["guilds"].array();
                const auto found = std::find_if(groups.begin(), groups.end(), [&](const J& g) {
                    return n(g, "test_ordinal") == ordinal;
                });
                if (found != groups.end()) {
                    continue;
                }
                const auto name = test_identity("TestGuild", ordinal);
                if (std::any_of(groups.begin(), groups.end(), [&](const J& g) {
                        return s(g, "key") == m_rules.name_key(name);
                    })) {
                    return fail("A non-managed guild already uses " + name + ".");
                }
                auto& archived = m_state.object()["test_archived_guilds"].array();
                const auto saved = std::find_if(archived.begin(), archived.end(), [&](const J& g) {
                    return n(g, "test_ordinal") == ordinal;
                });
                if (saved != archived.end()) {
                    auto restored = *saved;
                    set(restored, "members", arr());
                    groups.push_back(std::move(restored));
                    archived.erase(saved);
                    continue;
                }
                J group = obj();
                const auto id = n(m_state, "next_id");
                set(m_state, "next_id", num(id + 1));
                set(group, "id", num(id));
                set(group, "test_ordinal", num(ordinal));
                set(group, "name", str(name));
                set(group, "key", str(m_rules.name_key(name)));
                set(group, "grade", num(1));
                set(group, "points", num(0));
                set(group, "invitation", str(""));
                set(group, "announcement", str(""));
                set(group, "members", arr());
                set(group, "titles", m_rules.document().at("ranks").at("titles"));
                set(group, "permissions", m_rules.document().at("ranks").at("permissions"));
                groups.push_back(std::move(group));
            }
        } else if (operation == "membership") {
            const int id = request.at("id").as_int();
            const int destination = request.at("guild").as_int(-1);
            const int rank = request.at("rank").as_int(-1);
            const auto character = m_characters.load_character(id);
            if (!character || destination < 0 || rank < 0 || rank > 5) {
                return fail("Invalid member, guild or rank.");
            }
            const auto old_id = membership(id);
            const auto* old_group = guild(old_id);
            if (old_group && !n(*old_group, "test_ordinal")) {
                return fail("Cannot move a member out of a non-managed guild.");
            }
            J previous_member;
            if (old_group && old_id == static_cast<std::uint32_t>(destination)) {
                if (const auto* saved = member(*old_group, id)) {
                    previous_member = *saved;
                }
            }
            auto* target = destination ? guild(destination) : nullptr;
            if (destination && (!target || !n(*target, "test_ordinal"))) {
                return fail("Choose a managed test guild.");
            }
            for (auto& group : m_state.object()["guilds"].array()) {
                auto& members = group.object()["members"].array();
                members.erase(std::remove_if(members.begin(),
                                             members.end(),
                                             [&](const J& m) { return n(m, "id") == id; }),
                              members.end());
            }
            if (target) {
                J mem = previous_member.is_object() ? previous_member : obj();
                set(mem, "id", num(id));
                set(mem, "name", str(character->name));
                set(mem, "level", num(character->level));
                set(mem, "rank", num(rank));
                if (!previous_member.is_object()) {
                    set(mem, "joined", num(now));
                    set(mem, "last_connection", num(now));
                    set(mem, "point_level", num(character->level));
                }
                if (rank == 1) {
                    for (auto& other : target->object()["members"].array()) {
                        if (n(other, "rank") == 1) {
                            set(other, "rank", num(5));
                        }
                    }
                }
                target->object()["members"].array().push_back(mem);
            }
        }
        const int changed_id = request.at("id").as_int();
        for (auto& group : m_state.object()["guilds"].array()) {
            if (!n(group, "test_ordinal")) {
                continue;
            }
            auto& members = group.object()["members"].array();
            if (!members.empty() && std::none_of(members.begin(), members.end(), [&](const J& m) {
                    return n(m, "rank") == 1;
                })) {
                auto next = std::find_if(members.begin(), members.end(), [&](const J& m) {
                    return n(m, "id") != changed_id;
                });
                set(next == members.end() ? members.front() : *next, "rank", num(1));
            }
            recalculate(group);
        }
        if (!commit(previous, {})) {
            return fail("Guild transaction failed.");
        }
        J out = obj();
        set(out, "guilds", arr());
        for (const auto& group : m_state.at("guilds").array()) {
            if (!n(group, "test_ordinal")) {
                continue;
            }
            auto summary = obj();
            for (const auto* field : {"id", "name", "test_ordinal"}) {
                set(summary, field, group.at(field));
            }
            set(summary, "members", arr());
            for (const auto& member : group.at("members").array()) {
                auto entry = obj();
                set(entry, "id", member.at("id"));
                set(entry, "rank", member.at("rank"));
                summary.object()["members"].array().push_back(entry);
            }
            out.object()["guilds"].array().push_back(summary);
        }
        return out;
    }
    J* GuildService::guild(std::uint32_t id) {
        for (auto& g : m_state.object()["guilds"].array()) {
            if (n(g, "id") == id) {
                return &g;
            }
        }
        return nullptr;
    }
    const J* GuildService::guild(std::uint32_t id) const {
        for (const auto& g : m_state.at("guilds").array()) {
            if (n(g, "id") == id) {
                return &g;
            }
        }
        return nullptr;
    }
    J* GuildService::member(J& group, std::uint32_t id) {
        for (auto& m : group.object()["members"].array()) {
            if (n(m, "id") == id) {
                return &m;
            }
        }
        return nullptr;
    }
    const J* GuildService::member(const J& group, std::uint32_t id) const {
        for (const auto& m : group.at("members").array()) {
            if (n(m, "id") == id) {
                return &m;
            }
        }
        return nullptr;
    }
    std::uint32_t GuildService::membership(std::uint32_t id) const {
        for (const auto& g : m_state.at("guilds").array()) {
            if (member(g, id)) {
                return static_cast<std::uint32_t>(n(g, "id"));
            }
        }
        return 0;
    }

    bool GuildService::is_leader(std::uint32_t character) const {
        const auto* group = guild(membership(character));
        const auto* entry = group ? member(*group, character) : nullptr;
        return entry && n(*entry, "rank") == 1;
    }

    std::uint32_t GuildService::guild_id(std::string_view name) const {
        if (!protocol::guild_label_valid(name)) {
            return 0;
        }
        const auto key = fold(std::string(name));
        for (const auto& group : m_state.at("guilds").array()) {
            if (s(group, "key") == key) {
                return static_cast<std::uint32_t>(n(group, "id"));
            }
        }
        return 0;
    }

    bool GuildService::contains_guild(std::uint32_t guild_id) const {
        return guild(guild_id) != nullptr;
    }

    std::string GuildService::guild_name(std::uint32_t guild_id) const {
        const auto* group = guild(guild_id);
        return group ? s(*group, "name") : std::string{};
    }

    J GuildService::test_guild_catalog(std::uint32_t after, std::size_t limit) const {
        std::vector<const J*> groups;
        for (const auto& group : m_state.at("guilds").array()) {
            if (n(group, "id") > after) {
                groups.push_back(&group);
            }
        }
        limit = std::min(limit, groups.size());
        std::partial_sort(
            groups.begin(),
            groups.begin() + limit,
            groups.end(),
            [](const J* left, const J* right) { return n(*left, "id") < n(*right, "id"); });
        auto page = obj();
        auto rows = arr();
        for (std::size_t index = 0; index < limit; ++index) {
            const auto& group = *groups[index];
            auto entry = obj();
            set(entry, "id", group.at("id"));
            set(entry, "name", group.at("name"));
            set(entry, "managed", boolean(n(group, "test_ordinal") != 0));
            rows.array().push_back(std::move(entry));
        }
        set(page, "guilds", std::move(rows));
        set(page,
            "next",
            num(limit > 0 && limit < groups.size() ? n(*groups[limit - 1], "id") : 0));
        return page;
    }

    std::uint32_t GuildService::member_id(std::uint32_t actor, std::string_view name) const {
        const auto* g = guild(membership(actor));
        if (g) {
            for (const auto& m : g->at("members").array()) {
                if (s(m, "name") == name) {
                    return static_cast<std::uint32_t>(n(m, "id"));
                }
            }
        }
        return 0;
    }
    J& GuildService::character_state(std::uint32_t id) {
        auto& rows = m_state.object()["characters"].array();
        for (auto& r : rows) {
            if (n(r, "id") == id) {
                return r;
            }
        }
        auto r = obj();
        set(r, "id", num(id));
        rows.push_back(r);
        return rows.back();
    }
    std::string GuildService::daily_reward_error(std::uint32_t actor,
                                                 std::int64_t now,
                                                 std::int64_t money) const {
        const auto* group = guild(membership(actor));
        const auto* mem = group ? member(*group, actor) : nullptr;
        if (!m_rules.enabled() || !m_rules.flag("daily_reward", "enabled") || !mem ||
            n(*group, "grade") < m_rules.number("daily_reward", "minimum_grade") ||
            group->at("penalized").boolean() ||
            now - n(*mem, "joined") <
                m_rules.number("daily_reward", "minimum_membership_seconds")) {
            return "Daily guild reward is not available.";
        }
        const auto day = (now + m_rules.number("time", "day_offset_minutes") * 60) / 86400;
        for (const auto& state : m_state.at("characters").array()) {
            if (n(state, "id") == actor && state.get("reward_day") &&
                n(state, "reward_day") == day) {
                return "Daily guild reward has already been claimed.";
            }
        }
        const auto amount =
            m_rules.number("daily_reward", "zamogeon_per_grade") * n(*group, "grade");
        return amount > 2000000000 - money ? "Wallet capacity reached." : "";
    }
    std::size_t GuildService::counted(const J& group) const {
        return static_cast<std::size_t>(std::count_if(
            group.at("members").array().begin(),
            group.at("members").array().end(),
            [&](const J& m) {
                return n(m, "rank") != 0 || m_rules.flag("membership", "count_temporary_members");
            }));
    }
    void GuildService::recalculate(J& group) {
        int grade = m_rules.grade(counted(group));
        if (m_rules.document().at("grades").at("on_member_loss").as_string() == "penalty") {
            grade = std::max(grade, static_cast<int>(n(group, "grade")));
        }
        set(group, "grade", num(grade));
        set(group, "penalized", boolean(m_rules.penalized(grade, counted(group))));
    }
    bool GuildService::commit(const J& previous, const std::vector<CharacterRecord>& records) {
        if (J::stringify(previous, 0) == J::stringify(m_state, 0) && records.empty()) {
            return true;
        }
        set(m_state, "revision", num(n(previous, "revision") + 1));
        if (m_store.commit_guilds(
                static_cast<std::uint64_t>(n(previous, "revision")), m_state, records)) {
            return true;
        }
        m_state = previous;
        return false;
    }
    void GuildService::points(J& group, J& mem, std::int64_t amount, std::int64_t now) {
        const auto day = (now + m_rules.number("time", "day_offset_minutes") * 60) / 86400;
        if (n(mem, "day") != day) {
            set(mem, "day", num(day));
            set(mem, "daily", num(0));
        }
        const auto maximum = m_rules.number("points", "maximum");
        set(group, "points", num(std::min(maximum, n(group, "points") + amount)));
        set(mem, "daily", num(std::min(maximum, n(mem, "daily") + amount)));
    }
    GuildOutcome GuildService::create(const protocol::GuildRequest& r,
                                      const GuildContext& context,
                                      std::int64_t now) {
        GuildOutcome result;
        const auto key = m_rules.name_key(r.text);
        if (key.empty()) {
            result.error = "Guild names require 1 to 14 letters or digits without spaces.";
            return result;
        }
        for (const auto& g : m_state.at("guilds").array()) {
            if (s(g, "key") == key) {
                result.error = "Guild name already exists.";
                return result;
            }
        }
        const auto fee = m_rules.number("creation", "fee_zamogeon");
        if (r.expected_fee != static_cast<std::uint64_t>(fee)) {
            result.error = "Creation price changed. Refresh the guild window.";
            return result;
        }
        std::vector<CharacterRecord> founders{context.actor};
        if (m_rules.flag("creation", "require_party")) {
            if (context.party_leader != static_cast<std::uint32_t>(context.actor.id)) {
                result.error = "The party leader must create the guild.";
                return result;
            }
            founders = context.party;
        }
        if (founders.size() <
                static_cast<std::size_t>(m_rules.number("creation", "minimum_members")) ||
            (m_rules.flag("creation", "require_full_party") &&
             founders.size() != protocol::kPartyCapacity)) {
            result.error = "Not enough founding party members.";
            return result;
        }
        const auto cap = m_rules.number("membership", "maximum_members");
        if (cap && founders.size() > static_cast<std::size_t>(cap)) {
            result.error = "Founding party exceeds the guild member cap.";
            return result;
        }
        std::set<std::uint32_t> identities;
        for (const auto& founder : founders) {
            if (founder.id <= 0 ||
                !identities.insert(static_cast<std::uint32_t>(founder.id)).second ||
                founder.level < m_rules.number("creation", "minimum_level") ||
                founder.deletion_state != CharacterDeletionState::Active ||
                membership(founder.id) || n(character_state(founder.id), "rejoin") > now) {
                result.error =
                    "Every founder must be eligible, guildless and outside the rejoin lock.";
                return result;
            }
        }
        if (!identities.count(context.actor.id)) {
            result.error = "The founder must be in the founding party.";
            return result;
        }
        if (context.actor.money < fee) {
            result.error = "Guild creation requires " + std::to_string(fee) +
                           " Zamogeon; you have " + std::to_string(context.actor.money) + ".";
            return result;
        }
        if (founders.size() > 1 && m_rules.flag("creation", "require_consent")) {
            auto it = m_formations.find(context.actor.id);
            if (it == m_formations.end() || it->second.participants != identities ||
                it->second.request.text != r.text || it->second.expires <= now) {
                Formation formation;
                formation.request = r;
                formation.founder = context.actor.id;
                formation.participants = identities;
                formation.accepted.insert(context.actor.id);
                formation.expires = now + m_rules.number("creation", "consent_seconds");
                m_formations[context.actor.id] = std::move(formation);
                result.error = "Waiting for every founding member to accept in the Guild window.";
                return result;
            }
            if (it->second.accepted != identities) {
                result.error = "Waiting for founding consent.";
                return result;
            }
        }
        J group = obj();
        const auto id = n(m_state, "next_id");
        set(m_state, "next_id", num(id + 1));
        set(group, "id", num(id));
        set(group, "name", str(r.text));
        set(group, "key", str(key));
        set(group, "grade", num(1));
        set(group, "points", num(0));
        set(group, "invitation", str(""));
        set(group, "announcement", str(""));
        set(group, "members", arr());
        set(group, "titles", m_rules.document().at("ranks").at("titles"));
        set(group, "permissions", m_rules.document().at("ranks").at("permissions"));
        for (auto founder : founders) {
            J mem = obj();
            set(mem, "id", num(founder.id));
            set(mem, "name", str(founder.name));
            set(mem, "level", num(founder.level));
            set(mem, "rank", num(founder.id == context.actor.id ? 1 : 5));
            set(mem, "joined", num(now));
            set(mem, "last_connection", num(now));
            set(mem, "point_level", num(founder.level));
            group.object()["members"].array().push_back(mem);
            if (founder.id == context.actor.id) {
                founder.money -= fee;
            }
            result.characters.push_back(founder);
        }
        recalculate(group);
        m_state.object()["guilds"].array().push_back(group);
        auto& apps = m_state.object()["applications"].array();
        apps.erase(std::remove_if(apps.begin(),
                                  apps.end(),
                                  [&](const J& a) {
                                      return identities.count(
                                          static_cast<std::uint32_t>(n(a, "character")));
                                  }),
                   apps.end());
        J receipt = obj();
        set(receipt, "actor", num(context.actor.id));
        set(receipt, "request", str(std::to_string(r.request_id)));
        set(receipt, "at", num(now));
        m_state.object()["receipts"].array().push_back(receipt);
        m_formations.erase(context.actor.id);
        result.changed = true;
        return result;
    }
    std::string GuildService::application_error(const CharacterRecord& actor,
                                                std::uint32_t id,
                                                std::int64_t now) const {
        const auto* group = guild(id);
        std::int64_t rejoin = 0;
        for (const auto& state : m_state.at("characters").array()) {
            if (n(state, "id") == actor.id) {
                rejoin = n(state, "rejoin");
            }
        }
        if (!m_rules.enabled() || !group || actor.id <= 0 || membership(actor.id) ||
            actor.deletion_state != CharacterDeletionState::Active || rejoin > now ||
            actor.level < m_rules.number("membership", "minimum_level")) {
            return "You cannot apply to this guild.";
        }
        const auto cap = m_rules.number("membership", "maximum_members");
        if (cap && group->at("members").array().size() >= static_cast<std::size_t>(cap)) {
            return "Guild membership capacity reached.";
        }
        if (actor.money < m_rules.number("membership", "application_fee_zamogeon")) {
            return "Not enough Zamogeon for this guild application.";
        }
        for (const auto& a : m_state.at("applications").array()) {
            if (n(a, "guild") == id && n(a, "character") == actor.id && n(a, "expires") > now) {
                return "Application already pending.";
            }
        }
        return {};
    }

    GuildOutcome GuildService::execute(const protocol::GuildRequest& r,
                                       const GuildContext& context,
                                       std::int64_t now) {
        GuildOutcome result;
        if (!m_rules.enabled()) {
            result.error = "Guilds are disabled on this server.";
            return result;
        }
        if (context.actor.id <= 0) {
            result.error = "Authenticated character required.";
            return result;
        }
        if (read_only(r.command)) {
            return result;
        }
        if (!r.request_id) {
            result.error = "Missing request ID.";
            return result;
        }
        const auto actor = static_cast<std::uint32_t>(context.actor.id);
        for (const auto& receipt : m_state.at("receipts").array()) {
            if (n(receipt, "actor") == actor &&
                s(receipt, "request") == std::to_string(r.request_id)) {
                return result;
            }
        }
        const auto pending = m_formations.find(actor);
        const bool continuing_creation = r.command == C::Create && pending != m_formations.end() &&
                                         pending->second.request.request_id == r.request_id &&
                                         pending->second.expires > now;
        if (r.command != C::FormationReply && !continuing_creation && r.revision != revision()) {
            result.error = "Guild data changed. Refresh and try again.";
            return result;
        }
        const J previous = m_state;
        if (r.command == C::Create) {
            result = create(r, context, now);
        } else if (r.command == C::FormationReply) {
            const auto it = m_formations.find(r.target);
            if (it == m_formations.end() || it->second.expires <= now ||
                !it->second.participants.count(actor)) {
                result.error = "Founding request expired.";
            } else if (!r.value) {
                m_formations.erase(it);
                result.error = "Guild formation declined.";
            } else {
                it->second.accepted.insert(actor);
                if (it->second.accepted == it->second.participants) {
                    GuildContext founder = context;
                    const auto owner = std::find_if(
                        context.party.begin(), context.party.end(), [&](const auto& c) {
                            return c.id == static_cast<int>(it->second.founder);
                        });
                    if (owner == context.party.end()) {
                        result.error = "Founding party changed.";
                    } else {
                        founder.actor = *owner;
                        result = create(it->second.request, founder, now);
                    }
                }
            }
        } else {
            const auto own = membership(actor);
            auto* group = guild(r.guild ? r.guild : own);
            auto* me = group ? member(*group, actor) : nullptr;
            const auto rank = me ? static_cast<int>(n(*me, "rank")) : 0;
            const auto rights =
                rank > 0 ? static_cast<unsigned>(group->at("permissions").array()[rank].as_int())
                         : 0;
            if (r.command == C::Apply) {
                const auto fee = m_rules.number("membership", "application_fee_zamogeon");
                result.error = application_error(context.actor, r.guild, now);
                if (result.error.empty() && r.expected_fee != static_cast<std::uint64_t>(fee)) {
                    result.error = "Application price changed. Refresh before applying.";
                }
                if (result.error.empty()) {
                    J app = obj();
                    set(app, "guild", num(r.guild));
                    set(app, "character", num(actor));
                    set(app, "name", str(context.actor.name));
                    set(app, "level", num(context.actor.level));
                    set(app,
                        "expires",
                        num(now + m_rules.number("membership", "application_lifetime_seconds")));
                    auto& pending = m_state.object()["applications"].array();
                    pending.erase(std::remove_if(pending.begin(),
                                                 pending.end(),
                                                 [&](const J& old) {
                                                     return n(old, "guild") == r.guild &&
                                                            n(old, "character") == actor;
                                                 }),
                                  pending.end());
                    pending.push_back(app);
                    auto c = context.actor;
                    c.money -= fee;
                    result.characters.push_back(c);
                }
            } else if (r.command == C::Withdraw) {
                auto& apps = m_state.object()["applications"].array();
                apps.erase(std::remove_if(apps.begin(),
                                          apps.end(),
                                          [&](const J& a) {
                                              return n(a, "character") == actor &&
                                                     n(a, "guild") == r.guild;
                                          }),
                           apps.end());
            } else if (!group || !me) {
                result.error = "You are not a member of this guild.";
            } else if (permission(r.command) && !(rights & permission(r.command))) {
                result.error = "Your rank lacks this permission.";
            } else if (r.command == C::Accept || r.command == C::Reject) {
                auto& apps = m_state.object()["applications"].array();
                const auto application = std::find_if(apps.begin(), apps.end(), [&](const J& a) {
                    return n(a, "guild") == n(*group, "id") && n(a, "character") == r.target &&
                           n(a, "expires") > now;
                });
                if (application == apps.end()) {
                    result.error = "Application is no longer available.";
                } else if (r.command == C::Reject) {
                    apps.erase(application);
                } else {
                    auto candidate =
                        m_characters.load_character(static_cast<std::int32_t>(r.target));
                    for (const auto& c : context.party) {
                        if (c.id == static_cast<int>(r.target)) {
                            candidate = c;
                        }
                    }
                    const auto fee = m_rules.number("membership", "admission_fee_zamogeon");
                    const auto cap = m_rules.number("membership", "maximum_members");
                    const auto temporaries =
                        std::count_if(group->at("members").array().begin(),
                                      group->at("members").array().end(),
                                      [](const J& m) { return n(m, "rank") == 0; });
                    if (!candidate || candidate->deletion_state != CharacterDeletionState::Active ||
                        membership(r.target) ||
                        candidate->level < m_rules.number("membership", "minimum_level") ||
                        n(character_state(r.target), "rejoin") > now) {
                        result.error = "Applicant is no longer eligible.";
                    } else if ((cap && group->at("members").array().size() >=
                                           static_cast<std::size_t>(cap)) ||
                               (r.value &&
                                temporaries >=
                                    m_rules.number("membership", "temporary_member_limit"))) {
                        result.error = "Guild membership capacity reached.";
                    } else if (r.expected_fee != static_cast<std::uint64_t>(fee) ||
                               context.actor.money < fee) {
                        result.error = "Admission price changed or insufficient officer Zamogeon.";
                    } else {
                        J mem = obj();
                        set(mem, "id", num(r.target));
                        set(mem, "name", str(candidate->name));
                        set(mem, "level", num(candidate->level));
                        set(mem, "rank", num(r.value ? 0 : 5));
                        set(mem, "joined", num(now));
                        const auto last = n(character_state(r.target), "last_connection");
                        set(mem, "last_connection", num(last ? last : now));
                        set(mem, "point_level", num(candidate->level));
                        group->object()["members"].array().push_back(mem);
                        auto payer = context.actor;
                        payer.money -= fee;
                        result.characters.push_back(payer);
                        apps.erase(std::remove_if(
                                       apps.begin(),
                                       apps.end(),
                                       [&](const J& a) { return n(a, "character") == r.target; }),
                                   apps.end());
                        recalculate(*group);
                    }
                }
            } else if (r.command == C::Leave || r.command == C::Expel) {
                const auto target = r.command == C::Leave ? actor : r.target;
                const auto* victim = member(*group, target);
                if (!victim ||
                    (n(*victim, "rank") == 1 && group->at("members").array().size() > 1) ||
                    (r.command == C::Expel && (n(*victim, "rank") == 1 || n(*victim, "rank") == 2 ||
                                               (rank != 1 && n(*victim, "rank") <= rank)))) {
                    result.error = "Transfer leadership or demote the protected member first.";
                } else {
                    set(character_state(target),
                        "rejoin",
                        num(now + m_rules.number("membership", "rejoin_lock_seconds")));
                    auto& members = group->object()["members"].array();
                    members.erase(std::remove_if(members.begin(),
                                                 members.end(),
                                                 [&](const J& m) { return n(m, "id") == target; }),
                                  members.end());
                    if (members.empty()) {
                        const auto gid = n(*group, "id");
                        auto& groups = m_state.object()["guilds"].array();
                        groups.erase(std::remove_if(groups.begin(),
                                                    groups.end(),
                                                    [&](const J& g) { return n(g, "id") == gid; }),
                                     groups.end());
                        auto& apps = m_state.object()["applications"].array();
                        apps.erase(std::remove_if(apps.begin(),
                                                  apps.end(),
                                                  [&](const J& a) { return n(a, "guild") == gid; }),
                                   apps.end());
                    } else {
                        recalculate(*group);
                    }
                }
            } else if (r.command == C::Rank || r.command == C::Transfer) {
                auto* target = member(*group, r.target);
                const auto new_rank = r.command == C::Transfer ? 1 : static_cast<int>(r.value);
                if (!target || r.target == actor || new_rank < 1 || new_rank > 5 ||
                    (r.command == C::Transfer && (rank != 1 || n(*target, "rank") != 2)) ||
                    (r.command == C::Rank &&
                     (new_rank == 1 || n(*target, "rank") == 1 ||
                      (rank != 1 && (new_rank <= rank || n(*target, "rank") <= rank ||
                                     new_rank == 2 || n(*target, "rank") == 2))))) {
                    result.error = "Protected rank transition.";
                } else if (r.command == C::Rank && n(*target, "rank") == 2 && new_rank != 5) {
                    result.error = "Demote the sub-leader to OfficialMember first.";
                } else {
                    const auto occupied =
                        std::count_if(group->at("members").array().begin(),
                                      group->at("members").array().end(),
                                      [&](const J& m) {
                                          return n(m, "rank") == new_rank && n(m, "id") != r.target;
                                      });
                    if (r.command != C::Transfer && static_cast<std::size_t>(occupied) >=
                                                        m_rules.quota(new_rank, counted(*group))) {
                        result.error = "Configured officer quota reached.";
                    } else {
                        if (r.command == C::Transfer) {
                            set(*me, "rank", num(5));
                        }
                        set(*target, "rank", num(new_rank));
                        recalculate(*group);
                    }
                }
            } else if (r.command == C::Permissions) {
                if (rank != 1 || r.target < 2 || r.target > 5 || r.value > 31) {
                    result.error = "Only the leader may set rank options.";
                } else {
                    group->object()["permissions"].array()[r.target] = num(r.value);
                }
            } else if (r.command == C::Title) {
                if (r.target < 1 || r.target > 5 || !protocol::guild_label_valid(r.text)) {
                    result.error = "Rank titles require 1 to 14 characters without spaces.";
                } else {
                    group->object()["titles"].array()[r.target] = str(r.text);
                }
            } else if (r.command == C::Invitation || r.command == C::Announcement) {
                if ((r.text.size() >
                         static_cast<std::size_t>(m_rules.number("names", "maximum_text")) ||
                     !protocol::guild_text_valid(
                         r.text,
                         static_cast<std::size_t>(m_rules.number("names", "maximum_text")),
                         r.command == C::Announcement))) {
                    result.error = "Text exceeds the configured limit.";
                } else {
                    set(*group,
                        r.command == C::Invitation ? "invitation" : "announcement",
                        str(r.text));
                }
            } else if (r.command == C::DailyReward) {
                const auto day = (now + m_rules.number("time", "day_offset_minutes") * 60) / 86400;
                result.error = daily_reward_error(actor, now, context.actor.money);
                if (result.error.empty()) {
                    const auto amount =
                        m_rules.number("daily_reward", "zamogeon_per_grade") * n(*group, "grade");
                    auto payer = context.actor;
                    payer.money += amount;
                    result.characters.push_back(payer);
                    set(character_state(actor), "reward_day", num(day));
                }
            } else {
                result.error = "Unsupported guild operation.";
            }
        }
        if (!result.error.empty()) {
            m_state = previous;
            return result;
        }
        J receipt = obj();
        set(receipt, "actor", num(actor));
        set(receipt, "request", str(std::to_string(r.request_id)));
        set(receipt, "at", num(now));
        auto& receipts = m_state.object()["receipts"].array();
        receipts.erase(std::remove_if(receipts.begin(),
                                      receipts.end(),
                                      [&](const J& row) { return n(row, "at") < now - 604800; }),
                       receipts.end());
        if (r.command != C::Create) {
            receipts.push_back(receipt);
        }
        if (!commit(previous, result.characters)) {
            result.error = "Guild persistence failed. Refresh before retrying.";
            result.characters.clear();
            return result;
        }
        result.changed = true;
        return result;
    }
    J GuildService::display(std::uint32_t character) const {
        auto d = obj();
        const auto* g = guild(membership(character));
        set(d, "guild", num(g ? n(*g, "id") : 0));
        set(d, "name", str(g ? s(*g, "name") : ""));
        set(d, "grade", num(g ? n(*g, "grade") : 0));
        set(d,
            "visible",
            boolean(g && !g->at("penalized").boolean() &&
                    n(*g, "grade") >= m_rules.number("features", "name_display_minimum_grade")));
        if (g) {
            const auto* m = member(*g, character);
            const auto rank = n(*m, "rank");
            set(d,
                "title",
                str(rank ? g->at("titles").array()[rank].as_string() : "Temporary Member"));
        }
        return d;
    }
    std::array<int, 3> GuildService::bonuses(std::uint32_t character) const {
        const auto* g = guild(membership(character));
        if (!g || !m_rules.flag("buffs", "enabled") || g->at("penalized").boolean() ||
            n(*g, "grade") < m_rules.number("buffs", "minimum_grade")) {
            return {};
        }
        const auto grade = n(*g, "grade");
        return {static_cast<int>(std::min<std::int64_t>(
                    100000, m_rules.number("buffs", "primary_per_grade") * grade)),
                static_cast<int>(std::min<std::int64_t>(
                    1000000, m_rules.number("buffs", "hp_per_grade") * grade)),
                static_cast<int>(std::min<std::int64_t>(
                    100000, m_rules.number("buffs", "defense_per_grade") * grade))};
    }
    J GuildService::view(const protocol::GuildRequest& r,
                         std::uint32_t actor,
                         std::int64_t now,
                         std::optional<std::int64_t> money) const {
        J view = obj();
        set(view, "revision", num(revision()));
        J public_rules = m_rules.document();
        public_rules.object().erase("ranks");
        set(view, "rules", std::move(public_rules));
        set(view, "own", display(actor));
        if (!money) {
            const auto saved = m_characters.load_character(static_cast<std::int32_t>(actor));
            if (saved) {
                money = saved->money;
            }
        }
        set(view.object()["own"],
            "daily_reward_available",
            boolean(money && daily_reward_error(actor, now, *money).empty()));
        set(view, "rows", arr());
        set(view, "page", num(r.page));
        set(view, "command", num(static_cast<int>(r.command)));
        const auto own = membership(actor);
        if (const auto* mine = guild(own)) {
            const auto* mem = member(*mine, actor);
            const auto rank = n(*mem, "rank");
            set(view.object()["own"], "rank", num(rank));
            set(view.object()["own"],
                "rights",
                num(rank ? mine->at("permissions").array()[rank].as_int() : 0));
        }
        const auto* group = guild(r.guild ? r.guild : own);
        std::vector<J> rows;
        if (r.command == C::Ranking) {
            for (const auto& row : m_state.at("ranking").array()) {
                if (guild(static_cast<std::uint32_t>(n(row, "id")))) {
                    rows.push_back(row);
                }
            }
            // Keep daily point snapshots, but make newly founded guilds discoverable immediately.
            for (const auto& g : m_state.at("guilds").array()) {
                if (std::none_of(rows.begin(), rows.end(), [&](const J& row) {
                        return n(row, "id") == n(g, "id");
                    })) {
                    J row = obj();
                    for (const auto* key : {"id", "name", "grade", "points"}) {
                        set(row, key, g.at(key));
                    }
                    rows.push_back(row);
                }
            }
            std::sort(rows.begin(), rows.end(), [](const J& a, const J& b) {
                return n(a, "points") != n(b, "points") ? n(a, "points") > n(b, "points")
                                                        : fold(s(a, "name")) < fold(s(b, "name"));
            });
        } else if (r.command == C::Search) {
            for (const auto& g : m_state.at("guilds").array()) {
                if (fold(s(g, "name")).find(fold(r.text)) != std::string::npos) {
                    J row = obj();
                    for (const auto* key : {"id", "name", "grade", "points"}) {
                        set(row, key, g.at(key));
                    }
                    rows.push_back(row);
                }
            }
        } else if (r.command == C::MyApplications) {
            for (const auto& a : m_state.at("applications").array()) {
                if (n(a, "character") == actor && n(a, "expires") > now) {
                    auto row = a;
                    const auto* g = guild(static_cast<std::uint32_t>(n(a, "guild")));
                    if (g) {
                        set(row, "name", g->at("name"));
                        set(row, "id", a.at("guild"));
                        rows.push_back(row);
                    }
                }
            }
        }
        if (group) {
            J summary = obj();
            for (const auto* key : {"id", "name", "grade", "points", "penalized"}) {
                set(summary, key, group->at(key));
            }
            set(summary, "count", num(group->at("members").array().size()));
            set(summary, "counted", num(counted(*group)));
            if (r.command == C::Detail && r.value == 1 && member(*group, actor)) {
                set(summary, "announcement", group->at("announcement"));
            } else {
                set(summary, "invitation", group->at("invitation"));
            }
            if (member(*group, actor)) {
                for (const auto* key : {"titles", "permissions"}) {
                    set(summary, key, group->at(key));
                }
                J rank_options = arr();
                for (int office = 1; office <= 5; ++office) {
                    J option = obj();
                    set(option, "rank", num(office));
                    set(option,
                        "count",
                        num(std::count_if(group->at("members").array().begin(),
                                          group->at("members").array().end(),
                                          [&](const J& m) { return n(m, "rank") == office; })));
                    set(option, "unlimited", boolean(office == 5));
                    set(option,
                        "limit",
                        num(office == 5 ? 0 : m_rules.quota(office, counted(*group))));
                    rank_options.array().push_back(std::move(option));
                }
                set(summary, "rank_options", std::move(rank_options));
                set(summary, "rank", member(*group, actor)->at("rank"));
                set(summary,
                    "rights",
                    num(n(*member(*group, actor), "rank")
                            ? group->at("permissions")
                                  .array()[n(*member(*group, actor), "rank")]
                                  .as_int()
                            : 0));
                int online = 0;
                for (const auto& mem : group->at("members").array()) {
                    for (const auto& p : m_presence) {
                        if (p.character == n(mem, "id")) {
                            ++online;
                        }
                    }
                }
                set(summary, "online", num(online));
                if (r.command == C::Members || r.command == C::Snapshot) {
                    for (auto mem : group->at("members").array()) {
                        bool online = false;
                        for (const auto& p : m_presence) {
                            if (p.character == n(mem, "id")) {
                                online = true;
                                set(mem, "level", num(p.level));
                            }
                        }
                        set(mem, "online", boolean(online));
                        const auto day =
                            (now + m_rules.number("time", "day_offset_minutes") * 60) / 86400;
                        if (n(mem, "day") != day) {
                            set(mem, "daily", num(0));
                        }
                        rows.push_back(mem);
                    }
                }
                if (r.command == C::Applications && (summary.at("rights").as_int() & 1)) {
                    for (const auto& a : m_state.at("applications").array()) {
                        if (n(a, "guild") == n(*group, "id") && n(a, "expires") > now &&
                            !membership(static_cast<std::uint32_t>(n(a, "character")))) {
                            auto row = a;
                            set(row, "id", a.at("character"));
                            rows.push_back(row);
                        }
                    }
                }
            }
            set(view, "guild", summary);
        }
        for (const auto& [id, f] : m_formations) {
            if (f.expires > now && f.participants.count(actor)) {
                set(view, "formation", num(id));
                set(view, "formation_name", str(f.request.text));
                set(view, "formation_accepted", boolean(f.accepted.count(actor) != 0));
                break;
            }
        }
        set(view, "total", num(rows.size()));
        const auto begin =
            std::min<std::size_t>(rows.size(), static_cast<std::size_t>(r.page) * 10);
        const auto end = std::min(rows.size(), begin + 10);
        for (auto i = begin; i < end; ++i) {
            view.object()["rows"].array().push_back(rows[i]);
        }
        return view;
    }
    bool GuildService::advance(const std::vector<GuildPresence>& present,
                               std::int64_t now,
                               std::uint64_t steady) {
        const auto previous = m_state;
        const auto elapsed =
            m_last_steady && steady >= m_last_steady ? (steady - m_last_steady) / 1000 : 0;
        if (!m_last_steady) {
            m_last_steady = steady;
        } else {
            m_last_steady += elapsed * 1000;
        }
        for (const auto& current : present) {
            const bool was = std::any_of(m_presence.begin(), m_presence.end(), [&](const auto& p) {
                return p.character == current.character;
            });
            if (current.character && !was) {
                set(character_state(current.character), "last_connection", num(now));
            }
        }
        for (const auto& previous_presence : m_presence) {
            const bool online = std::any_of(present.begin(), present.end(), [&](const auto& p) {
                return p.character == previous_presence.character;
            });
            if (previous_presence.character && !online) {
                set(character_state(previous_presence.character), "last_connection", num(now));
            }
        }
        for (auto& group : m_state.object()["guilds"].array()) {
            for (auto& mem : group.object()["members"].array()) {
                const auto current =
                    std::find_if(present.begin(), present.end(), [&](const auto& p) {
                        return p.character == n(mem, "id");
                    });
                const bool was =
                    std::any_of(m_presence.begin(), m_presence.end(), [&](const auto& p) {
                        return p.character == n(mem, "id");
                    });
                if (current != present.end()) {
                    set(mem, "level", num(current->level));
                    if (!was || !n(mem, "last_connection")) {
                        set(mem, "last_connection", num(now));
                    }
                    if (was && m_rules.flag("points", "enabled")) {
                        auto seconds = n(mem, "seconds") + static_cast<std::int64_t>(elapsed);
                        const auto hours = seconds / 3600;
                        set(mem, "seconds", num(seconds % 3600));
                        if (hours) {
                            points(group,
                                   mem,
                                   hours * m_rules.number("points", "connected_hour_points"),
                                   now);
                        }
                    }
                } else if (was) {
                    set(mem, "last_connection", num(now));
                }
            }
        }
        m_presence = present;
        const auto interval = m_rules.number("time", "ranking_refresh_seconds");
        const auto day = (now + m_rules.number("time", "day_offset_minutes") * 60) / interval;
        if (!m_state.get("ranking_day") || n(m_state, "ranking_day") != day) {
            std::vector<J> ranks;
            for (const auto& g : m_state.at("guilds").array()) {
                J row = obj();
                for (const auto* key : {"id", "name", "key", "grade", "points"}) {
                    set(row, key, g.at(key));
                }
                ranks.push_back(row);
            }
            std::sort(ranks.begin(), ranks.end(), [](const J& a, const J& b) {
                if (n(a, "points") != n(b, "points")) {
                    return n(a, "points") > n(b, "points");
                }
                if (s(a, "key") != s(b, "key")) {
                    return s(a, "key") < s(b, "key");
                }
                return n(a, "id") < n(b, "id");
            });
            J ranking = arr();
            ranking.array() = std::move(ranks);
            set(m_state, "ranking", ranking);
            set(m_state, "ranking_day", num(day));
        }
        auto& applications = m_state.object()["applications"].array();
        applications.erase(
            std::remove_if(applications.begin(),
                           applications.end(),
                           [&](const J& a) {
                               return n(a, "expires") <= now ||
                                      membership(static_cast<std::uint32_t>(n(a, "character")));
                           }),
            applications.end());
        for (auto it = m_formations.begin(); it != m_formations.end();) {
            if (it->second.expires <= now) {
                it = m_formations.erase(it);
            } else {
                ++it;
            }
        }
        if (now - m_last_persist < 30 && J::stringify(previous, 0) == J::stringify(m_state, 0)) {
            return true;
        }
        // Persist connected remainders before any later operation can publish them.
        m_last_persist = now;
        return commit(previous, {});
    }
    bool GuildService::level_up(const CharacterRecord& character, std::int64_t now) {
        auto* g = guild(membership(character.id));
        if (!g || !m_rules.flag("points", "enabled")) {
            return true;
        }
        auto* mem = member(*g, character.id);
        if (character.level <= n(*mem, "point_level")) {
            return true;
        }
        const auto previous = m_state;
        const auto amount =
            character.level <= m_rules.number("points", "low_level_end")
                ? m_rules.number("points", "low_level_points")
                : std::max<std::int64_t>(
                      0, character.level - m_rules.number("points", "level_offset"));
        points(*g, *mem, amount, now);
        set(*mem, "point_level", num(character.level));
        set(*mem, "level", num(character.level));
        return commit(previous, {character});
    }
}  // namespace openkal::server
