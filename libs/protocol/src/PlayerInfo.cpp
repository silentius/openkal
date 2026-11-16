/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/protocol/PlayerInfo.h"

#include "openkal/protocol/FrameParser.h"
#include "openkal/protocol/Opcodes.h"

namespace openkal::protocol {

    bool PlayerInfo::encode_characters(PacketWriter& w, const std::vector<PlayerInfoChar>& chars) {
        for (const auto& c : chars) {
            // "dsbdddddddbb" — job BYTE, level/gid/stats int32, face/hair BYTE.
            w.write_i32(c.pid);
            w.write_string(c.name);
            w.write_u8(c.job);
            w.write_i32(c.level);
            w.write_i32(c.gid);
            w.write_i32(c.str);
            w.write_i32(c.hth);
            w.write_i32(c.intel);
            w.write_i32(c.wis);
            w.write_i32(c.dex);
            w.write_u8(c.face);
            w.write_u8(c.hair);
            if (c.wear.size() > 255) {
                return false;
            }
            w.write_u8(static_cast<std::uint8_t>(c.wear.size()));
            for (std::uint16_t idx : c.wear) {
                w.write_u16(idx);
            }
        }
        return true;
    }

    bool PlayerInfo::decode_characters(PacketReader& r,
                                       std::uint8_t count,
                                       std::vector<PlayerInfoChar>& out) {
        out.clear();
        out.reserve(count);
        for (std::uint8_t i = 0; i < count; ++i) {
            PlayerInfoChar c;
            if (!r.read_i32(c.pid) || !r.read_string(c.name) || !r.read_u8(c.job) ||
                !r.read_i32(c.level) || !r.read_i32(c.gid) || !r.read_i32(c.str) ||
                !r.read_i32(c.hth) || !r.read_i32(c.intel) || !r.read_i32(c.wis) ||
                !r.read_i32(c.dex) || !r.read_u8(c.face) || !r.read_u8(c.hair)) {
                return false;
            }

            std::uint8_t wear_n = 0;
            if (!r.read_u8(wear_n)) {
                return false;
            }
            c.wear.resize(wear_n);
            for (std::uint8_t w = 0; w < wear_n; ++w) {
                if (!r.read_u16(c.wear[w])) {
                    return false;
                }
            }
            out.push_back(std::move(c));
        }
        return r.ok();
    }

    bool PlayerInfo::encode_payload(PacketWriter& w, const PlayerInfoMsg& msg) {
        if (msg.characters.size() > 255) {
            return false;
        }
        PacketWriter blob;
        if (!encode_characters(blob, msg.characters)) {
            return false;
        }
        // "bdbm"
        w.write_u8(msg.auth_type);
        w.write_u32(static_cast<std::uint32_t>(msg.exp_time));
        w.write_u8(static_cast<std::uint8_t>(msg.characters.size()));
        w.write_bytes(blob.payload().data(), blob.payload().size());
        return true;
    }

    bool PlayerInfo::decode_payload(PacketReader& r, PlayerInfoMsg& out) {
        std::uint8_t auth = 0;
        std::uint32_t exp = 0;
        std::uint8_t count = 0;
        if (!r.read_format("bdb", &auth, &exp, &count)) {
            return false;
        }
        out.auth_type = auth;
        out.exp_time = static_cast<std::int32_t>(exp);
        return decode_characters(r, count, out.characters);
    }

    std::vector<std::uint8_t> PlayerInfo::frame(const PlayerInfoMsg& msg) {
        PacketWriter w;
        if (!encode_payload(w, msg)) {
            return {};
        }
        return w.frame(static_cast<std::uint8_t>(S2C::PlayerInfo));
    }

    bool PlayerInfo::parse(const std::vector<std::uint8_t>& frame, PlayerInfoMsg& out) {
        std::size_t consumed = 0;
        std::uint8_t type = 0;
        std::vector<std::uint8_t> payload;
        if (!FrameParser::try_pop(frame.data(), frame.size(), consumed, type, payload)) {
            return false;
        }
        if (type != static_cast<std::uint8_t>(S2C::PlayerInfo) || consumed != frame.size()) {
            return false;
        }
        PacketReader r(payload);
        return decode_payload(r, out) && r.remaining() == 0;
    }

    bool encode_playerinfo_chars(PacketWriter& writer,
                                 const std::vector<PlayerInfoChar>& characters) {
        return PlayerInfo::encode_characters(writer, characters);
    }
    bool decode_playerinfo_chars(PacketReader& reader,
                                 std::uint8_t count,
                                 std::vector<PlayerInfoChar>& out) {
        return PlayerInfo::decode_characters(reader, count, out);
    }
    bool encode_playerinfo_payload(PacketWriter& writer, const PlayerInfoMsg& message) {
        return PlayerInfo::encode_payload(writer, message);
    }
    bool decode_playerinfo_payload(PacketReader& reader, PlayerInfoMsg& out) {
        return PlayerInfo::decode_payload(reader, out);
    }
    std::vector<std::uint8_t> frame_s2c_playerinfo(const PlayerInfoMsg& message) {
        return PlayerInfo::frame(message);
    }
    bool parse_s2c_playerinfo(const std::vector<std::uint8_t>& frame, PlayerInfoMsg& out) {
        return PlayerInfo::parse(frame, out);
    }

}  // namespace openkal::protocol
