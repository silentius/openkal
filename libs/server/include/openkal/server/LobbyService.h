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
#include "openkal/server/LobbyProtocol.h"
#include "openkal/server/world/StarterKit.h"

#include <functional>
#include <unordered_map>

namespace openkal::server {
    struct PendingLobbyCredentials;
    /** Per-connection authenticated lobby identity and abuse limits. */
    struct LobbyIdentity {
        std::shared_ptr<PendingLobbyCredentials> pending;
        std::int32_t account_id = 0;
        std::int32_t character_id = 0;
        unsigned credential_attempts = 0;
        unsigned requests = 0;
        std::int64_t window_started = 0;
    };

    /** Server-owned account registration and character-selection rules. */
    class LobbyService {
      public:
        /** Creates a lobby service. @param store Server persistence. @param config Server policy.
         * @param items Authoritative item definitions. @param next_iid Global item sequence. */
        LobbyService(IDataStore& store,
                     const ServerConfig& config,
                     const std::unordered_map<int, ItemCombatDef>& items,
                     std::atomic<std::uint32_t>& next_iid);

        /** Handles one bounded request. @param payload Request bytes. @param identity Session
         * identity. @param malformed Set when the request violates the lobby protocol.
         * @return Framed reply. */
        [[nodiscard]] std::vector<std::uint8_t>
        handle(const std::vector<std::uint8_t>& payload, LobbyIdentity& identity, bool& malformed);

        /** Completes pending password work without blocking the world loop.
         * @param identity Session identity.
         * @param busy Checks whether another session owns an account.
         * @return Completed response, or empty while idle or pending.
         */
        [[nodiscard]] std::vector<std::uint8_t> poll(LobbyIdentity& identity,
                                                     const std::function<bool(std::int32_t)>& busy);

        /** Creates a character with the normal validation, spawn and starter-kit transaction.
         * @param draft Requested name, appearance, class and starting stats.
         * @param account Owning authenticated account ID.
         * @return Whether the complete character creation transaction committed.
         */
        [[nodiscard]] bool create_character(const CharacterRecord& draft, std::int32_t account);

      private:
        IDataStore& m_store;
        const ServerConfig& m_config;
        const std::unordered_map<int, ItemCombatDef>& m_items;
        std::atomic<std::uint32_t>& m_next_iid;
        StarterKit m_starters;
        std::string m_dummy_hash;
        std::vector<std::shared_ptr<PendingLobbyCredentials>> m_jobs;
        unsigned m_credentials = 0;
        std::int64_t m_window_started = 0;
    };
}  // namespace openkal::server
