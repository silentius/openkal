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

#include "openkal/platform/Json.h"
#include "openkal/server/LobbyProtocol.h"
#include "openkal/server/net/Transport.h"

#include <functional>

namespace openkal::server {
    /** Authenticated, opt-in test control; never part of the compatibility protocol. */
    inline constexpr std::uint8_t kTestRequest = 242, kTestResponse = 243;

    /** Builds deterministic test identities.
     * @param prefix Identity prefix.
     * @param ordinal One-based test index.
     * @return Prefix followed by a zero-padded index.
     */
    [[nodiscard]] std::string test_identity(std::string_view prefix, int ordinal);

    /** Encodes a complete lobby record for local graphical attachment.
     * @param character Authoritative character.
     * @return Hex-encoded lobby snapshot.
     */
    [[nodiscard]] std::string test_character_snapshot(const CharacterRecord& character);

    /** Decodes a bounded lobby record.
     * @param snapshot Hex-encoded snapshot.
     * @return Character, or no value on malformed input.
     */
    [[nodiscard]] std::optional<CharacterRecord> read_test_character(std::string_view snapshot);

    /** Frames an authenticated test command.
     * @param token Server's explicit test-control secret.
     * @param command JSON operation and arguments.
     * @return Framed request.
     */
    [[nodiscard]] std::vector<std::uint8_t> frame_test_request(std::string_view token,
                                                               const platform::Json& command);

    /** Bounded synchronous requests; callers run this outside the GUI thread. */
    class TestControlClient final {
      public:
        /** Opens a trusted TLS connection.
         * @param host Server IPv4 address.
         * @param port Server port.
         * @param token Explicit test-control secret.
         * @param tls Trust settings.
         * @param receive_buffer Optional socket receive-buffer size set before connecting; zero
         * keeps the system default.
         * @return True on connection success.
         */
        [[nodiscard]] bool connect(const std::string& host,
                                   std::uint16_t port,
                                   std::string token,
                                   TlsConfig tls,
                                   int receive_buffer = 0);

        /** Executes a command before transferring the connection to gameplay.
         * @param command JSON operation.
         * @param pump Optional background-session pump.
         * @return JSON response; error property describes failure.
         */
        [[nodiscard]] platform::Json request(const platform::Json& command,
                                             const std::function<void()>& pump = {});

        /** Transfers a selected test player's connection.
         * @return Live session.
         */
        [[nodiscard]] std::unique_ptr<ISession> release_session();

      private:
        std::unique_ptr<ISession> m_session;
        std::string m_token;
    };
}  // namespace openkal::server
