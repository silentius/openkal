/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/protocol/Chatting.h"

#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketReader.h"
#include "openkal/protocol/PacketWriter.h"

#include <cctype>

namespace openkal::protocol {
    namespace {

        [[nodiscard]] std::string truncate_chat(std::string_view text) {
            if (text.size() <= kChatMaxChars) {
                return std::string(text);
            }
            return std::string(text.substr(0, kChatMaxChars));
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

    }  // namespace

    std::vector<std::uint8_t> Chatting::frame_c2s(std::string_view text) {
        PacketWriter w;
        w.write_string(truncate_chat(text));
        return w.frame(static_cast<std::uint8_t>(C2S::Chatting));
    }

    bool Chatting::parse_c2s(const std::uint8_t* data, std::size_t size, std::string& out) {
        PacketReader r(data, size);
        if (!r.read_string(out)) {
            return false;
        }
        if (out.size() > kChatMaxChars) {
            out.resize(kChatMaxChars);
        }
        return true;
    }

    std::vector<std::uint8_t> Chatting::frame_s2c(std::string_view speaker, std::string_view text) {
        PacketWriter w;
        w.write_string(speaker);
        w.write_string(truncate_chat(text));
        return w.frame(static_cast<std::uint8_t>(S2C::Chatting));
    }

    bool Chatting::parse_s2c(const std::uint8_t* data,
                             std::size_t size,
                             std::string& speaker,
                             std::string& text) {
        PacketReader r(data, size);
        return r.read_string(speaker) && r.read_string(text);
    }

    ChatChannel Chatting::channel_of(std::string_view text) {
        if (text.empty()) {
            return ChatChannel::Common;
        }
        switch (text.front()) {
        case '#':
            return ChatChannel::Party;
        case '$':
            return ChatChannel::Guild;
        case '@':
            return ChatChannel::Whisper;
        case '~':
            return ChatChannel::Alliance;
        case '^':
            return ChatChannel::Shout;
        case '!':
            return ChatChannel::Common;
        default:
            return ChatChannel::Common;
        }
    }

    std::string_view Chatting::display_body(std::string_view text) {
        if (text.empty()) {
            return text;
        }
        if (text.front() == '@') {
            auto rest = text.substr(1);
            const auto target_start = rest.find_first_not_of(" \t");
            if (target_start == std::string_view::npos) {
                return {};
            }
            rest.remove_prefix(target_start);
            const auto target_end = rest.find_first_of(" \t");
            if (target_end == std::string_view::npos) {
                return {};
            }
            rest.remove_prefix(target_end);
            const auto body_start = rest.find_first_not_of(" \t");
            return body_start == std::string_view::npos ? std::string_view{}
                                                        : rest.substr(body_start);
        }
        if (text.front() == '!' || channel_of(text) != ChatChannel::Common) {
            return text.substr(1);
        }
        return text;
    }

    bool Chatting::parse_whisper(std::string_view text, std::string& target, std::string& body) {
        target.clear();
        body.clear();
        if (text.empty() || text.front() != '@') {
            return false;
        }
        std::string_view rest = text.substr(1);
        while (!rest.empty() && (rest.front() == ' ' || rest.front() == '\t')) {
            rest.remove_prefix(1);
        }
        const auto sp = rest.find_first_of(" \t");
        if (sp == std::string_view::npos) {
            target.assign(rest.begin(), rest.end());
            return !target.empty();
        }
        target.assign(rest.begin(), rest.begin() + static_cast<std::ptrdiff_t>(sp));
        std::string_view rem = rest.substr(sp + 1);
        while (!rem.empty() && (rem.front() == ' ' || rem.front() == '\t')) {
            rem.remove_prefix(1);
        }
        body.assign(rem.begin(), rem.end());
        return !target.empty();
    }

    bool Chatting::is_slash_command(std::string_view text) {
        if (text.empty() || text.front() != '/') {
            return false;
        }
        // Shout / yell are routed as chat, not ChatCommand.
        if (starts_with_ci(text, "/s ") || starts_with_ci(text, "/s\t") || text == "/s" ||
            starts_with_ci(text, "/y ") || starts_with_ci(text, "/y\t") || text == "/y") {
            return false;
        }
        return true;
    }

    std::string Chatting::command_name(std::string_view text) {
        if (!is_slash_command(text)) {
            return {};
        }
        std::string_view rest = text.substr(1);
        while (!rest.empty() && (rest.front() == ' ' || rest.front() == '\t')) {
            rest.remove_prefix(1);
        }
        const auto sp = rest.find_first_of(" \t");
        std::string_view name = (sp == std::string_view::npos) ? rest : rest.substr(0, sp);
        std::string out;
        out.reserve(name.size());
        for (char c : name) {
            out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
        return out;
    }

}  // namespace openkal::protocol
