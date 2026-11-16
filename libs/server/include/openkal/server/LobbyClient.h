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
#include "openkal/server/net/Transport.h"

#include <functional>

namespace openkal::server {
    /** Owns a remote TLS lobby and supplies character-selection data without database access. */
    class LobbyClient final : public ICharacterRoster {
      public:
        /** Connects and reads server capabilities.
         * @param host IPv4 address.
         * @param port Server port.
         * @param tls Client trust and expected server identity; defaults to environment settings.
         * @return True on success.
         */
        [[nodiscard]] bool connect(const std::string& host,
                                   std::uint16_t port,
                                   TlsConfig tls = tls_config_from_environment());

        /** Opens the owner-only local test-session bridge.
         * @param path Unix-domain endpoint created by the test-player tool.
         * @return True when the bridge reports a supported lobby.
         */
        [[nodiscard]] bool connect_local(const std::string& path);

        /** Registers an account when allowed. @param name Username. @param email Address. @param
         * password Plaintext transmitted only over TLS. @return True on success. */
        [[nodiscard]] bool register_account(const std::string& name,
                                            const std::string& email,
                                            const std::string& password);

        /** Authenticates this session. @param name Username. @param password Password. @return True
         * on success. */
        [[nodiscard]] bool login(const std::string& name, const std::string& password);

        /** Selects an owned character without entering the world. @param id Character id. @return
         * Authoritative snapshot. */
        [[nodiscard]] std::optional<CharacterRecord> select(std::int32_t id);

        /** Transfers the selected connection to the game client. @return Live TLS session. */
        [[nodiscard]] std::unique_ptr<ISession> release_session();

        /** Closes the lobby and clears its account identity. */
        void close();

        /** @return Last actionable server or transport error. */
        const std::string& error() const;

        /** @return Whether the server allows in-client registration. */
        bool registration_allowed() const;

        /** Reports whether the TLS lobby is still connected.
         * @return True while the transport remains alive.
         */
        bool connected() const;

        /** @return Authenticated account id, or zero. */
        std::int32_t account_id() const;

        /** Sets a bounded-wait UI pump. @param callback Returns false to cancel a request. */
        void set_wait_callback(std::function<bool()> callback);

        /** @copydoc ICharacterRoster::list_characters */
        [[nodiscard]] std::vector<CharacterRecord>
        list_characters(std::int32_t account_id) override;

        /** @copydoc ICharacterRoster::list_deleted_characters */
        [[nodiscard]] std::vector<CharacterRecord>
        list_deleted_characters(std::int32_t account_id, std::int64_t now_seconds) override;

        /** @copydoc ICharacterRoster::load_character */
        [[nodiscard]] std::optional<CharacterRecord> load_character(std::int32_t id) override;

        /** @copydoc ICharacterRoster::save_character */
        [[nodiscard]] bool save_character(const CharacterRecord& character) override;

        /** @copydoc ICharacterRoster::load_items */
        [[nodiscard]] std::vector<ItemRecord> load_items(std::int32_t character_id) override;

        /** @copydoc ICharacterRoster::schedule_character_deletion */
        [[nodiscard]] bool schedule_character_deletion(std::int32_t id,
                                                       std::int32_t account_id,
                                                       std::int64_t now_seconds) override;

        /** @copydoc ICharacterRoster::restore_character */
        [[nodiscard]] bool restore_character(std::int32_t id,
                                             std::int32_t account_id,
                                             std::int64_t now_seconds) override;

      private:
        [[nodiscard]] bool request(LobbyOperation op,
                                   const protocol::PacketWriter& payload,
                                   std::vector<std::uint8_t>& response);
        [[nodiscard]] std::vector<CharacterRecord> characters(LobbyOperation op);
        std::unique_ptr<ISession> m_session;
        std::string m_error;
        std::int32_t m_account_id = 0;
        bool m_registration_allowed = false;
        std::function<bool()> m_wait;
    };
}  // namespace openkal::server
