/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/protocol/Guild.h"

#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketReader.h"
#include "openkal/protocol/PacketWriter.h"

namespace openkal::protocol {
    namespace {
        constexpr std::uint8_t kVersion = 1;
        constexpr std::size_t kViewLimit = 7600;
        bool valid(const GuildRequest& r) {
            return static_cast<unsigned>(r.command) <=
                       static_cast<unsigned>(GuildCommand::DailyReward) &&
                   r.text.size() <= 4096 &&
                   ((r.command == GuildCommand::Create || r.command == GuildCommand::Title)
                        ? guild_label_valid(r.text)
                        : guild_text_valid(
                              r.text, 1024, r.command == GuildCommand::Announcement)) &&
                   r.page <= 1000000 && r.value <= 65535;
        }
        bool
        text_valid(std::string_view text, std::size_t maximum, bool multiline, bool allow_spaces) {
            std::size_t count = 0;
            for (std::size_t i = 0; i < text.size();) {
                const auto lead = static_cast<unsigned char>(text[i++]);
                std::uint32_t value = 0, minimum = 0;
                int continuation = 0;
                if (lead < 0x80) {
                    value = lead;
                } else if (lead >= 0xC2 && lead <= 0xDF) {
                    value = lead & 31;
                    continuation = 1;
                    minimum = 0x80;
                } else if (lead >= 0xE0 && lead <= 0xEF) {
                    value = lead & 15;
                    continuation = 2;
                    minimum = 0x800;
                } else if (lead >= 0xF0 && lead <= 0xF4) {
                    value = lead & 7;
                    continuation = 3;
                    minimum = 0x10000;
                } else {
                    return false;
                }
                while (continuation--) {
                    if (i >= text.size()) {
                        return false;
                    }
                    const auto next = static_cast<unsigned char>(text[i++]);
                    if ((next & 0xC0) != 0x80) {
                        return false;
                    }
                    value = (value << 6) | (next & 63);
                }
                if (value < minimum || value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF) ||
                    (value < 32 && !(multiline && value == '\n')) ||
                    (value >= 127 && value <= 159) ||
                    (!allow_spaces &&
                     (value == 32 || value == 0xA0 || value == 0x1680 ||
                      (value >= 0x2000 && value <= 0x200A) || value == 0x2028 || value == 0x2029 ||
                      value == 0x202F || value == 0x205F || value == 0x3000 || value == 0xFEFF)) ||
                    ++count > maximum) {
                    return false;
                }
            }
            return true;
        }
    }  // namespace
    bool guild_text_valid(std::string_view text, std::size_t maximum, bool multiline) {
        return text_valid(text, maximum, multiline, true);
    }
    bool guild_label_valid(std::string_view text) {
        return !text.empty() && text_valid(text, kGuildLabelMaximum, false, false);
    }
    std::vector<std::uint8_t> frame_guild_request(const GuildRequest& r) {
        if (!valid(r)) {
            return {};
        }
        PacketWriter w;
        w.write_u8(kVersion);
        w.write_u8(static_cast<std::uint8_t>(r.command));
        w.write_u64(r.request_id);
        w.write_u32(r.guild);
        w.write_u32(r.target);
        w.write_u32(r.page);
        w.write_u32(r.value);
        w.write_u64(r.revision);
        w.write_u64(r.expected_fee);
        w.write_string(r.text);
        return w.frame(static_cast<std::uint8_t>(C2S::Guild));
    }
    bool parse_guild_request(const std::uint8_t* data, std::size_t size, GuildRequest& out) {
        if (size > 4200) {
            return false;
        }
        PacketReader r(data, size);
        GuildRequest next;
        std::uint8_t version = 0, command = 0;
        if (!r.read_u8(version) || version != kVersion || !r.read_u8(command) ||
            !r.read_u64(next.request_id) || !r.read_u32(next.guild) || !r.read_u32(next.target) ||
            !r.read_u32(next.page) || !r.read_u32(next.value) || !r.read_u64(next.revision) ||
            !r.read_u64(next.expected_fee) || !r.read_string(next.text) || r.remaining()) {
            return false;
        }
        next.command = static_cast<GuildCommand>(command);
        if (!valid(next)) {
            return false;
        }
        out = std::move(next);
        return true;
    }
    std::vector<std::uint8_t> frame_guild_response(const GuildResponse& response) {
        if (!response.data.is_object()) {
            return {};
        }
        const auto text = platform::Json::stringify(response.data, 0);
        if (text.size() > kViewLimit || static_cast<unsigned>(response.kind) > 1) {
            return {};
        }
        PacketWriter w;
        w.write_u8(kVersion);
        w.write_u8(static_cast<std::uint8_t>(response.kind));
        w.write_u64(response.request_id);
        w.write_string(text);
        return w.frame(static_cast<std::uint8_t>(S2C::Guild));
    }
    bool parse_guild_response(const std::uint8_t* data, std::size_t size, GuildResponse& out) {
        if (size > kViewLimit + 11) {
            return false;
        }
        PacketReader r(data, size);
        GuildResponse next;
        std::uint8_t version = 0, kind = 0;
        std::string text;
        if (!r.read_u8(version) || version != kVersion || !r.read_u8(kind) || kind > 1 ||
            !r.read_u64(next.request_id) || !r.read_string(text) || text.size() > kViewLimit ||
            r.remaining() || !platform::Json::parse(text, next.data) || !next.data.is_object()) {
            return false;
        }
        next.kind = static_cast<GuildMessage>(kind);
        out = std::move(next);
        return true;
    }
}  // namespace openkal::protocol
