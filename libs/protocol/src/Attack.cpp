/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/protocol/Attack.h"

#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketReader.h"
#include "openkal/protocol/PacketWriter.h"

namespace openkal::protocol {

    std::vector<std::uint8_t> Attack::frame_client(std::uint8_t kind, std::uint32_t id) {
        PacketWriter writer;
        writer.write_u8(kind);
        writer.write_u32(id);
        return writer.frame(static_cast<std::uint8_t>(C2S::Attack));
    }

    bool Attack::parse_client(const std::uint8_t* data,
                              std::size_t size,
                              std::uint8_t& kind,
                              std::uint32_t& id) {
        if (!data || size < 5) {
            return false;
        }
        PacketReader reader(data, size);
        return reader.read_u8(kind) && reader.read_u32(id);
    }

    std::vector<std::uint8_t> Attack::frame_server(std::uint32_t attacker,
                                                   std::uint32_t target,
                                                   std::int32_t damage,
                                                   std::int32_t palsy,
                                                   std::uint8_t flags) {
        PacketWriter writer;
        writer.write_u32(attacker);
        writer.write_u32(target);
        writer.write_i32(damage);
        writer.write_i32(palsy);
        writer.write_u8(flags);
        return writer.frame(static_cast<std::uint8_t>(S2C::Attack));
    }

    bool Attack::parse_server(const std::uint8_t* data,
                              std::size_t size,
                              std::uint32_t& attacker,
                              std::uint32_t& target,
                              std::int32_t& damage,
                              std::int32_t& palsy,
                              std::uint8_t& flags) {
        if (!data || size < 17) {
            return false;
        }
        PacketReader reader(data, size);
        return reader.read_u32(attacker) && reader.read_u32(target) && reader.read_i32(damage) &&
               reader.read_i32(palsy) && reader.read_u8(flags);
    }

    std::vector<std::uint8_t> frame_c2s_attack(std::uint8_t kind, std::uint32_t id) {
        return Attack::frame_client(kind, id);
    }
    bool parse_c2s_attack(const std::uint8_t* data,
                          std::size_t size,
                          std::uint8_t& kind,
                          std::uint32_t& id) {
        return Attack::parse_client(data, size, kind, id);
    }
    std::vector<std::uint8_t> frame_s2c_attack(std::uint32_t attacker,
                                               std::uint32_t target,
                                               std::int32_t damage,
                                               std::int32_t palsy,
                                               std::uint8_t flags) {
        return Attack::frame_server(attacker, target, damage, palsy, flags);
    }
    bool parse_s2c_attack(const std::uint8_t* data,
                          std::size_t size,
                          std::uint32_t& attacker,
                          std::uint32_t& target,
                          std::int32_t& damage,
                          std::int32_t& palsy,
                          std::uint8_t& flags) {
        return Attack::parse_server(data, size, attacker, target, damage, palsy, flags);
    }

}  // namespace openkal::protocol
