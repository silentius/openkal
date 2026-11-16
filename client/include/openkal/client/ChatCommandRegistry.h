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

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace openkal::client {

    /** Outcome of a client-local chat command. */
    class ClientChatCommandResult {
      public:
        ClientChatCommandResult() = default;

        /**
         * Builds a result.
         * @param handled True when the client consumed the input (do not send to
         * server).
         * @param info_line Optional info line to show locally.
         */
        ClientChatCommandResult(bool handled, std::string info_line)
            : m_handled(handled), m_info_line(std::move(info_line)) {}

        /** @return True when input was consumed locally. */
        [[nodiscard]] bool handled() const {
            return m_handled;
        }

        /** @return Local info line (may be empty). */
        [[nodiscard]] const std::string& info_line() const {
            return m_info_line;
        }

      private:
        /** True when handled locally. */
        bool m_handled = false;

        /** Local info line. */
        std::string m_info_line;
    };

    /** Context for a client-local command. */
    class ClientChatCommandContext {
      public:
        /**
         * Builds a context.
         * @param raw Full chat text.
         * @param args Arguments after the command name.
         */
        ClientChatCommandContext(std::string_view raw, std::string_view args)
            : m_raw(raw), m_args(args) {}

        /** @return Full chat text. */
        [[nodiscard]] std::string_view raw() const {
            return m_raw;
        }

        /** @return Arguments. */
        [[nodiscard]] std::string_view args() const {
            return m_args;
        }

      private:
        /** Full chat text. */
        std::string_view m_raw;

        /** Arguments. */
        std::string_view m_args;
    };

    /** Handler for one client-local slash command. */
    using ClientChatCommandHandler =
        std::function<ClientChatCommandResult(const ClientChatCommandContext&)>;

    /**
     * Client-local slash commands (do not send C2S_CHATTING when handled).
     *
     * Server-authoritative commands such as /time are intentionally not registered
     * here so they are forwarded to the server.
     */
    class ChatCommandRegistry {
      public:
        ChatCommandRegistry() = default;

        /**
         * Registers or replaces a local command.
         * @param name Command name without '/'.
         * @param handler Callback.
         */
        void register_command(std::string name, ClientChatCommandHandler handler);

        /**
         * Tries to dispatch a slash command locally.
         * @param raw Full chat text.
         * @return handled=false when the text should be sent to the server.
         */
        [[nodiscard]] ClientChatCommandResult try_dispatch(std::string_view raw) const;

      private:
        /** Handlers keyed by lowercase name. */
        std::unordered_map<std::string, ClientChatCommandHandler> m_handlers;
    };

}  // namespace openkal::client
