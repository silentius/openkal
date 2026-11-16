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
#include "openkal/server/net/Transport.h"

#include <chrono>
#include <deque>
#include <functional>
#include <optional>

namespace openkal::server {
    /** Reserved whisper identity; '!' cannot occur in a player character name. */
    inline constexpr const char* kManagementSpeaker = "!Support";

    /** Creates a local management error response. @param message Description. @return JSON error.
     */
    [[nodiscard]] platform::Json management_error(std::string_view message);

    /** A completed management response or a nonwaiting delivery-completion check. */
    class ManagementReply final {
      public:
        /** Creates an immediately available response.
         * @param response Completed operation result.
         */
        ManagementReply(platform::Json response);

        /** Creates a response that waits for an independently owned operation.
         * @param poll_response Nonwaiting callback returning no value until completion.
         */
        explicit ManagementReply(std::function<std::optional<platform::Json>()> poll_response);

        /** Checks completion once without waiting.
         * @return Completed response, or no value while the operation remains pending.
         */
        [[nodiscard]] std::optional<platform::Json> poll();

      private:
        std::optional<platform::Json> m_response;
        std::function<std::optional<platform::Json>()> m_poll_response;
    };

    /** Owner-only Unix-socket API, serviced exclusively on the authoritative world thread. */
    class ManagementApi final {
      public:
        /** Opens an optional local endpoint without replacing existing filesystem entries.
         * @param path Unix socket path; empty disables the API.
         * @param token Explicit secret of 32 through 256 bytes.
         * @param error Startup failure description without secrets.
         * @return Whether the endpoint is ready or intentionally disabled. */
        [[nodiscard]] bool start(const std::string& path, std::string token, std::string& error);

        /** Closes all local connections and removes this instance's socket. */
        void close();

        /** Services bounded requests without waiting for incomplete connections.
         * @param handler World-thread command executor, called only after authentication. */
        void poll(const std::function<ManagementReply(const platform::Json&)>& handler);

        /** Reports whether the endpoint is listening. @return Whether management is enabled. */
        bool enabled() const;

        /** Appends a bounded support PM event.
         * @param character Character ID. @param name Character name.
         * @param body Message body. @param outgoing Whether sent by the console. */
        void record_pm(int character, std::string_view name, std::string_view body, bool outgoing);

        /** Reads at most 25 support events without consuming other consoles' cursors.
         * @param after Last processed event ID.
         * @return Events, next cursor, gap flag and continuation flag. */
        [[nodiscard]] platform::Json events(std::uint64_t after) const;

      private:
        struct Peer {
            std::unique_ptr<ISession> session;
            std::vector<std::uint8_t> bytes;
            std::chrono::steady_clock::time_point created;
            std::optional<ManagementReply> reply;
        };
        std::unique_ptr<IAcceptor> m_acceptor;
        std::string m_token;
        std::string m_instance;
        std::vector<Peer> m_peers;
        std::deque<platform::Json> m_events;
        std::uint64_t m_cursor = 0;
    };

    /** Executes one bounded local request; GUI callers use a worker thread.
     * @param path Unix endpoint. @param token Server management secret.
     * @param command Operation and arguments.
     * @return Server response, or an error; mutations are never automatically retried. */
    [[nodiscard]] platform::Json
    management_request(const std::string& path, const std::string& token, platform::Json command);
}  // namespace openkal::server
