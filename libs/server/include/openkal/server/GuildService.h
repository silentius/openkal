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
#include "openkal/protocol/Guild.h"
#include "openkal/server/GuildRules.h"
#include "openkal/server/GuildStore.h"
#include "openkal/server/store/DataStore.h"

#include <array>
#include <map>
#include <optional>
#include <set>

namespace openkal::server {
    /** Current authenticated identities; all values are supplied by ServerHost. */
    struct GuildContext {
        CharacterRecord actor;
        std::vector<CharacterRecord> party;
        std::uint32_t party_leader = 0;
    };
    /** Effects applied to live sessions only after successful persistence. */
    struct GuildOutcome {
        std::string error;
        std::vector<CharacterRecord> characters;
        bool changed = false;
    };
    /** Presence is transient; membership remains after disconnect. */
    struct GuildPresence {
        std::uint32_t character = 0, player = 0;
        int level = 1;
    };
    /** Server-thread-owned guild domain and transaction coordinator. */
    class GuildService final {
      public:
        /** @param rules Immutable validated policy. @param store Guild transactions. @param
         * characters Character lookup. */
        GuildService(GuildRules rules, IGuildStore& store, IDataStore& characters);
        /** @return Whether the durable state could be restored. */
        [[nodiscard]] bool restore();
        /** @param request Authenticated operation. @param context Live actor/party. @param now Unix
         * seconds.
         * @return Committed effects or rejection. */
        [[nodiscard]] GuildOutcome execute(const protocol::GuildRequest& request,
                                           const GuildContext& context,
                                           std::int64_t now);
        /** @param request Requested page. @param actor Character ID. @param now Unix seconds.
         * @param money Live wallet balance; uses the saved character when omitted.
         * @return Private view for this actor. */
        platform::Json view(const protocol::GuildRequest& request,
                            std::uint32_t actor,
                            std::int64_t now,
                            std::optional<std::int64_t> money = std::nullopt) const;
        /** @param character Persistent identity. @return Current guild ID, or zero. */
        std::uint32_t membership(std::uint32_t character) const;

        /** Checks the current guild leader rank.
         * @param character Persistent character ID.
         * @return Whether the character leads their guild.
         */
        [[nodiscard]] bool is_leader(std::uint32_t character) const;

        /** Looks up a guild using its canonical name.
         * @param name Guild name with ASCII case ignored.
         * @return Persistent guild ID, or zero when absent.
         */
        [[nodiscard]] std::uint32_t guild_id(std::string_view name) const;

        /** Checks whether a guild still exists.
         * @param guild_id Persistent guild ID.
         * @return Whether the guild exists.
         */
        [[nodiscard]] bool contains_guild(std::uint32_t guild_id) const;

        /** Looks up a guild's display name.
         * @param guild_id Persistent guild ID.
         * @return Display name, or an empty string when absent.
         */
        [[nodiscard]] std::string guild_name(std::uint32_t guild_id) const;

        /** Lists active regular and managed guilds by increasing ID for the test tool.
         * @param after Exclusive guild ID cursor, or zero for the first page.
         * @param limit Maximum rows to return.
         * @return A page of guild identities and the next cursor, or zero at the end.
         */
        [[nodiscard]] platform::Json test_guild_catalog(std::uint32_t after,
                                                        std::size_t limit) const;

        /** Checks whether the actor can submit a new application under current policy.
         * @param actor Live character. @param guild Target guild. @param now Unix seconds.
         * @return Empty when available, otherwise the rejection reason.
         */
        [[nodiscard]] std::string application_error(const CharacterRecord& actor,
                                                    std::uint32_t guild,
                                                    std::int64_t now) const;
        /** @param actor Guild member. @param name Exact character name. @return Durable member ID
         * or zero. */
        std::uint32_t member_id(std::uint32_t actor, std::string_view name) const;
        /** @param character Persistent identity. @return Public guild identity and effective
         * display state. */
        platform::Json display(std::uint32_t character) const;
        /** @param character Persistent identity. @return Primary-stat, HP, and defense additions.
         */
        std::array<int, 3> bonuses(std::uint32_t character) const;
        /** @param present Current players. @param now Unix seconds. @param steady Monotonic
         * milliseconds.
         * @return Whether all required writes succeeded. */
        [[nodiscard]] bool
        advance(const std::vector<GuildPresence>& present, std::int64_t now, std::uint64_t steady);
        /** @param character Updated level-up record. @param now Unix seconds. @return Successful
         * award/skip. */
        [[nodiscard]] bool level_up(const CharacterRecord& character, std::int64_t now);
        /** @return Current committed revision. */
        std::uint64_t revision() const;

        /** Configures explicitly marked test guilds without gameplay fees or quotas.
         * Only the opt-in test-control handler may call this operation.
         * @param request Ensure count, or character/guild/rank assignment.
         * @param now Unix seconds.
         * @return Managed guilds, or an error property; failed writes restore prior state.
         */
        [[nodiscard]] platform::Json test_control(const platform::Json& request, std::int64_t now);
        /** @return Immutable owner rules. */
        const GuildRules& rules() const {
            return m_rules;
        }

      private:
        struct Formation {
            protocol::GuildRequest request;
            std::uint32_t founder = 0;
            std::set<std::uint32_t> participants, accepted;
            std::int64_t expires = 0;
        };
        platform::Json* guild(std::uint32_t id);
        const platform::Json* guild(std::uint32_t id) const;
        platform::Json* member(platform::Json& group, std::uint32_t id);
        const platform::Json* member(const platform::Json& group, std::uint32_t id) const;
        platform::Json& character_state(std::uint32_t id);
        std::string
        daily_reward_error(std::uint32_t actor, std::int64_t now, std::int64_t money) const;
        std::size_t counted(const platform::Json& group) const;
        void recalculate(platform::Json& group);
        void points(platform::Json& group,
                    platform::Json& member,
                    std::int64_t amount,
                    std::int64_t now);
        bool commit(const platform::Json& previous, const std::vector<CharacterRecord>& records);
        GuildOutcome create(const protocol::GuildRequest& request,
                            const GuildContext& context,
                            std::int64_t now);
        GuildRules m_rules;
        IGuildStore& m_store;
        IDataStore& m_characters;
        platform::Json m_state;
        std::map<std::uint32_t, Formation> m_formations;
        std::vector<GuildPresence> m_presence;
        std::uint64_t m_last_steady = 0;
        std::int64_t m_last_persist = 0;
    };
}  // namespace openkal::server
