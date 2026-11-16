/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/chat/ChatService.h"

#include "openkal/protocol/Chatting.h"

#include <cctype>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace openkal::server {
    namespace {

        [[nodiscard]] std::string_view args_after_command(std::string_view raw) {
            if (raw.empty() || raw.front() != '/') {
                return {};
            }
            std::string_view rest = raw.substr(1);
            while (!rest.empty() && (rest.front() == ' ' || rest.front() == '\t')) {
                rest.remove_prefix(1);
            }
            const auto sp = rest.find_first_of(" \t");
            if (sp == std::string_view::npos) {
                return {};
            }
            std::string_view args = rest.substr(sp + 1);
            while (!args.empty() && (args.front() == ' ' || args.front() == '\t')) {
                args.remove_prefix(1);
            }
            return args;
        }

        [[nodiscard]] bool starts_with_ci(std::string_view text, std::string_view prefix) {
            if (text.size() < prefix.size()) {
                return false;
            }
            for (std::size_t i = 0; i < prefix.size(); ++i) {
                const unsigned char a = static_cast<unsigned char>(text[i]);
                const unsigned char b = static_cast<unsigned char>(prefix[i]);
                if (std::tolower(a) != std::tolower(b)) {
                    return false;
                }
            }
            return true;
        }

        [[nodiscard]] bool is_shout_or_yell(std::string_view text) {
            return starts_with_ci(text, "/s ") || starts_with_ci(text, "/s\t") || text == "/s" ||
                   starts_with_ci(text, "/y ") || starts_with_ci(text, "/y\t") || text == "/y";
        }

        [[nodiscard]] std::string format_server_wall_clock() {
            using clock = std::chrono::system_clock;
            const std::time_t t = clock::to_time_t(clock::now());
            std::tm tm{};
#if defined(_WIN32)
            gmtime_s(&tm, &t);
#else
            gmtime_r(&t, &tm);
#endif
            std::ostringstream oss;
            oss << "[ " << std::setfill('0') << std::setw(2) << tm.tm_hour << " : " << std::setw(2)
                << tm.tm_min << " : " << std::setw(2) << tm.tm_sec << " UTC ]";
            return oss.str();
        }

    }  // namespace

    ChatService::ChatService() {
        register_defaults();
    }

    void ChatService::register_defaults() {
        m_commands.register_command("time", [](const ChatCommandContext&) {
            return ChatCommandResult(format_server_wall_clock());
        });
    }

    ChatOutcome ChatService::evaluate(std::string_view raw,
                                      std::string_view speaker,
                                      std::uint64_t now_ms,
                                      std::uint64_t& next_msg_ms) {
        if (raw.empty()) {
            return ChatOutcome(ChatAction::Ignore, {}, {}, {});
        }

        if (protocol::Chatting::is_slash_command(raw)) {
            const std::string name = protocol::Chatting::command_name(raw);
            const ChatCommandContext ctx(raw, args_after_command(raw), speaker);
            const ChatCommandResult result = m_commands.dispatch(name, ctx);
            return ChatOutcome(ChatAction::CommandReply, {}, result.reply(), {});
        }

        if (now_ms < next_msg_ms) {
            return ChatOutcome(
                ChatAction::RateLimited, {}, "Please wait before chatting again.", {});
        }
        const protocol::ChatChannel channel = protocol::Chatting::channel_of(raw);
        next_msg_ms =
            now_ms + (channel == protocol::ChatChannel::Party ? kPartyRateLimitMs : kRateLimitMs);

        if (is_shout_or_yell(raw)) {
            auto body = raw.size() > 2 ? raw.substr(3) : std::string_view{};
            while (!body.empty() && (body.front() == ' ' || body.front() == '\t')) {
                body.remove_prefix(1);
            }
            return ChatOutcome(body.empty() ? ChatAction::Ignore : ChatAction::ShoutBroadcast,
                               body.empty() ? std::string{} : "^" + std::string(body),
                               {},
                               {});
        }

        switch (channel) {
        case protocol::ChatChannel::Party:
            return ChatOutcome(ChatAction::PartyBroadcast, std::string(raw), {}, {});
        case protocol::ChatChannel::Guild:
            return ChatOutcome(ChatAction::GuildBroadcast, std::string(raw), {}, {});
        case protocol::ChatChannel::Alliance:
            return ChatOutcome(
                ChatAction::StubChannel, {}, "Alliance chat is not available yet.", {});
        case protocol::ChatChannel::Squad:
            return ChatOutcome(ChatAction::StubChannel, {}, "Squad chat is not available yet.", {});
        case protocol::ChatChannel::Whisper: {
            std::string target;
            std::string body;
            if (!protocol::Chatting::parse_whisper(raw, target, body) || target.empty()) {
                return ChatOutcome(ChatAction::WhisperFailed, {}, "Invalid whisper.", {});
            }
            // Keep "@Name body" on the wire so the client can color/filter as whisper.
            return ChatOutcome(ChatAction::Whisper, std::string(raw), {}, std::move(target));
        }
        case protocol::ChatChannel::Shout:
            return ChatOutcome(raw.size() <= 1 ? ChatAction::Ignore : ChatAction::ShoutBroadcast,
                               std::string(raw),
                               {},
                               {});
        case protocol::ChatChannel::Common:
        case protocol::ChatChannel::Count:
        default:
            return ChatOutcome(ChatAction::LocalBroadcast, std::string(raw), {}, {});
        }
    }

}  // namespace openkal::server
