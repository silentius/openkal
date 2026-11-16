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

#include "openkal/server/chat/ChatCommandRegistry.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace openkal::server {

    /** Routing decision produced by ChatService::evaluate. */
    enum class ChatAction : std::uint8_t {
        /** Drop silently (empty input). */
        Ignore = 0,
        /** Sender exceeded the rate limit. */
        RateLimited,
        /** Slash command handled; reply text goes to the sender only. */
        CommandReply,
        /** Broadcast speaker+text to players in sight (includes sender). */
        LocalBroadcast,
        /** Broadcast to the sender's current party, across maps and sight range. */
        PartyBroadcast,
        /** Broadcast to every playing client on this server. */
        ShoutBroadcast,
        /** Broadcast to authenticated members of the sender's current guild. */
        GuildBroadcast,
        /** Deliver whisper to target name; also echo to sender. */
        Whisper,
        /** Channel not implemented yet; reply to sender. */
        StubChannel,
        /** Whisper target missing or invalid. */
        WhisperFailed,
    };

    /** Evaluated chat message ready for Host to apply. */
    class ChatOutcome {
      public:
        ChatOutcome() = default;

        /**
         * Builds an outcome.
         * @param action Routing action.
         * @param outbound_text Text placed in S2C_CHATTING (prefix preserved when
         * broadcast).
         * @param reply System/command reply for the sender.
         * @param whisper_target Whisper target character name.
         */
        ChatOutcome(ChatAction action,
                    std::string outbound_text,
                    std::string reply,
                    std::string whisper_target)
            : m_action(action), m_outbound_text(std::move(outbound_text)),
              m_reply(std::move(reply)), m_whisper_target(std::move(whisper_target)) {}

        /** @return Routing action. */
        [[nodiscard]] ChatAction action() const {
            return m_action;
        }

        /** @return Outbound chat text (for local/whisper packets). */
        [[nodiscard]] const std::string& outbound_text() const {
            return m_outbound_text;
        }

        /** @return Reply text for the sender (commands / stubs). */
        [[nodiscard]] const std::string& reply() const {
            return m_reply;
        }

        /** @return Whisper target character name. */
        [[nodiscard]] const std::string& whisper_target() const {
            return m_whisper_target;
        }

      private:
        /** Routing action. */
        ChatAction m_action = ChatAction::Ignore;

        /** Outbound chat text. */
        std::string m_outbound_text;

        /** Reply text for the sender. */
        std::string m_reply;

        /** Whisper target character name. */
        std::string m_whisper_target;
    };

    /**
     * Server chat router: rate limit, slash commands, prefix channels.
     *
     * Host applies ChatOutcome and authenticates party/guild membership.
     * Alliance and Squad remain unavailable until their membership systems exist.
     */
    class ChatService {
      public:
        /** Builds a service with the default /time command registered. */
        ChatService();

        /** @return Mutable command registry for additional commands. */
        [[nodiscard]] ChatCommandRegistry& commands() {
            return m_commands;
        }

        /** @return Read-only command registry. */
        [[nodiscard]] const ChatCommandRegistry& commands() const {
            return m_commands;
        }

        /**
         * Evaluates one inbound chat string.
         * @param raw Raw C2S chat text.
         * @param speaker Sender character name.
         * @param now_ms Monotonic milliseconds used for rate limiting.
         * @param next_msg_ms In/out: earliest time the sender may chat again.
         * @return Routing outcome for Host to apply.
         */
        [[nodiscard]] ChatOutcome evaluate(std::string_view raw,
                                           std::string_view speaker,
                                           std::uint64_t now_ms,
                                           std::uint64_t& next_msg_ms);

        /** Minimum milliseconds between chat messages (uses 1000). */
        static constexpr std::uint64_t kRateLimitMs = 1000;

        /** Existing party-chat interval in milliseconds. */
        static constexpr std::uint64_t kPartyRateLimitMs = 500;

      private:
        /** Registers built-in commands such as /time. */
        void register_defaults();

        /** Slash-command table. */
        ChatCommandRegistry m_commands;
    };

}  // namespace openkal::server
