/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/protocol/PlayerOptions.h"

#include "openkal/protocol/Guild.h"
#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketReader.h"
#include "openkal/protocol/PacketWriter.h"

#include <algorithm>

namespace openkal::protocol {
    namespace {
        bool valid(const PlayerOptionsRequest& r) {
            return r.request_id && r.target &&
                   static_cast<unsigned>(r.action) <=
                       static_cast<unsigned>(PlayerAction::ApplyGuild);
        }
        bool valid(const PlayerOptionsState& s) {
            if (!s.request_id || !s.target || s.actions.size() > 16 || s.name.size() > 128 ||
                s.guild_name.size() > 128 || s.status.size() > 512 ||
                !guild_text_valid(s.name, 128) || !guild_text_valid(s.guild_name, 128) ||
                !guild_text_valid(s.status, 512)) {
                return false;
            }
            std::vector<PlayerAction> seen;
            for (const auto action : s.actions) {
                if (action == PlayerAction::Query || action > PlayerAction::ApplyGuild ||
                    std::find(seen.begin(), seen.end(), action) != seen.end()) {
                    return false;
                }
                seen.push_back(action);
            }
            return true;
        }
    }  // namespace
    std::vector<std::uint8_t> frame_player_options_request(const PlayerOptionsRequest& r) {
        if (!valid(r)) {
            return {};
        }
        PacketWriter w;
        w.write_u8(1);
        w.write_u8(static_cast<std::uint8_t>(r.action));
        w.write_u64(r.request_id);
        w.write_u32(r.target);
        w.write_u32(r.guild);
        w.write_u64(r.expected_fee);
        return w.frame(static_cast<std::uint8_t>(C2S::PlayerOptions));
    }
    bool parse_player_options_request(const std::uint8_t* data,
                                      std::size_t size,
                                      PlayerOptionsRequest& out) {
        PacketReader r(data, size);
        PlayerOptionsRequest next;
        std::uint8_t version = 0, action = 0;
        if (size != 26 || !r.read_u8(version) || version != 1 || !r.read_u8(action) ||
            !r.read_u64(next.request_id) || !r.read_u32(next.target) || !r.read_u32(next.guild) ||
            !r.read_u64(next.expected_fee) || r.remaining()) {
            return false;
        }
        next.action = static_cast<PlayerAction>(action);
        if (!valid(next)) {
            return false;
        }
        out = next;
        return true;
    }
    std::vector<std::uint8_t> frame_player_options_state(const PlayerOptionsState& s) {
        if (!valid(s)) {
            return {};
        }
        PacketWriter w;
        w.write_u8(1);
        w.write_u64(s.request_id);
        w.write_u32(s.target);
        w.write_u32(s.guild);
        w.write_u64(s.application_fee);
        w.write_string(s.name);
        w.write_string(s.guild_name);
        w.write_string(s.status);
        w.write_u8(static_cast<std::uint8_t>(s.actions.size()));
        for (const auto action : s.actions) {
            w.write_u8(static_cast<std::uint8_t>(action));
        }
        return w.frame(static_cast<std::uint8_t>(S2C::PlayerOptions));
    }
    bool parse_player_options_state(const std::uint8_t* data,
                                    std::size_t size,
                                    PlayerOptionsState& out) {
        if (size > 900) {
            return false;
        }
        PacketReader r(data, size);
        PlayerOptionsState next;
        std::uint8_t version = 0, count = 0;
        if (!r.read_u8(version) || version != 1 || !r.read_u64(next.request_id) ||
            !r.read_u32(next.target) || !r.read_u32(next.guild) ||
            !r.read_u64(next.application_fee) || !r.read_string(next.name) ||
            !r.read_string(next.guild_name) || !r.read_string(next.status) || !r.read_u8(count) ||
            count > 16) {
            return false;
        }
        for (unsigned i = 0; i < count; ++i) {
            std::uint8_t action = 0;
            if (!r.read_u8(action)) {
                return false;
            }
            next.actions.push_back(static_cast<PlayerAction>(action));
        }
        if (r.remaining() || !valid(next)) {
            return false;
        }
        out = std::move(next);
        return true;
    }
}  // namespace openkal::protocol
