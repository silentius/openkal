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

namespace openkal::server {

    /** Outcome of a registered chat command. */
    class ChatCommandResult {
      public:
        ChatCommandResult() = default;

        /**
         * Builds a successful command reply.
         * @param reply Text echoed to the sender (empty = silent success).
         */
        explicit ChatCommandResult(std::string reply) : m_ok(true), m_reply(std::move(reply)) {}

        /**
         * Builds a command result.
         * @param ok True when the command was recognised and handled.
         * @param reply Reply text for the sender.
         */
        ChatCommandResult(bool ok, std::string reply) : m_ok(ok), m_reply(std::move(reply)) {}

        /** @return True when the command was handled. */
        [[nodiscard]] bool ok() const {
            return m_ok;
        }

        /** @return Reply text for the sender. */
        [[nodiscard]] const std::string& reply() const {
            return m_reply;
        }

      private:
        /** True when the command was handled. */
        bool m_ok = false;

        /** Reply text for the sender. */
        std::string m_reply;
    };

    /** Context passed to chat command handlers. */
    class ChatCommandContext {
      public:
        /**
         * Builds a context for one invocation.
         * @param raw Full chat text including leading '/'.
         * @param args Text after the command token (may be empty).
         * @param speaker Sender character name.
         */
        ChatCommandContext(std::string_view raw, std::string_view args, std::string_view speaker)
            : m_raw(raw), m_args(args), m_speaker(speaker) {}

        /** @return Full chat text including leading '/'. */
        [[nodiscard]] std::string_view raw() const {
            return m_raw;
        }

        /** @return Text after the command token. */
        [[nodiscard]] std::string_view args() const {
            return m_args;
        }

        /** @return Sender character name. */
        [[nodiscard]] std::string_view speaker() const {
            return m_speaker;
        }

      private:
        /** Full chat text. */
        std::string_view m_raw;

        /** Arguments after the command name. */
        std::string_view m_args;

        /** Sender character name. */
        std::string_view m_speaker;
    };

    /** Handler for one slash command (name without leading '/'). */
    using ChatCommandHandler = std::function<ChatCommandResult(const ChatCommandContext&)>;

    /**
     * Extensible slash-command table used by ChatService.
     *
     * Register new commands with register_command("foo", handler) — Host does not
     * need a new branch.
     */
    class ChatCommandRegistry {
      public:
        ChatCommandRegistry() = default;

        /**
         * Registers or replaces a command handler.
         * @param name Command name without leading '/' (compared case-insensitively
         * via lowercase keys).
         * @param handler Callback invoked when the command is dispatched.
         */
        void register_command(std::string name, ChatCommandHandler handler);

        /**
         * Dispatches a slash command.
         * @param command_name Lowercase command name without '/'.
         * @param ctx Invocation context.
         * @return Handler result, or ok=false when the command is unknown.
         */
        [[nodiscard]] ChatCommandResult dispatch(std::string_view command_name,
                                                 const ChatCommandContext& ctx) const;

        /**
         * Reports whether a command is registered.
         * @param command_name Lowercase command name without '/'.
         * @return True when registered.
         */
        [[nodiscard]] bool has_command(std::string_view command_name) const;

      private:
        /** Handlers keyed by lowercase command name. */
        std::unordered_map<std::string, ChatCommandHandler> m_handlers;
    };

}  // namespace openkal::server
