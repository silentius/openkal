/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/protocol/Emote.h"

#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketReader.h"
#include "openkal/protocol/PacketWriter.h"

namespace openkal::protocol {
    std::vector<std::uint8_t> Emote::frame_client(std::uint8_t motion) {
        if (motion == 0 || motion > kEmoteCount) {
            return {};
        }
        PacketWriter writer;
        writer.write_u8(motion);
        return writer.frame(static_cast<std::uint8_t>(C2S::PlayerAnimation));
    }

    bool Emote::parse_client(const std::uint8_t* data, std::size_t size, std::uint8_t& motion) {
        if (!data || size != 1 || data[0] == 0 || data[0] > kEmoteCount) {
            return false;
        }
        motion = data[0];
        return true;
    }

    std::vector<std::uint8_t> Emote::frame_server(std::uint32_t player_id, std::uint8_t motion) {
        if (player_id == 0 || motion == 0 || motion > kEmoteCount) {
            return {};
        }
        PacketWriter writer;
        writer.write_u32(player_id);
        writer.write_u8(motion);
        return writer.frame(static_cast<std::uint8_t>(S2C::PlayerAnimation));
    }

    bool Emote::parse_server(const std::uint8_t* data, std::size_t size, PlayerEmote& event) {
        if (!data || size != 5) {
            return false;
        }
        PacketReader reader(data, size);
        PlayerEmote parsed;
        if (!reader.read_u32(parsed.player_id) || !reader.read_u8(parsed.motion) ||
            parsed.player_id == 0 || parsed.motion == 0 || parsed.motion > kEmoteCount) {
            return false;
        }
        event = parsed;
        return true;
    }
}  // namespace openkal::protocol
