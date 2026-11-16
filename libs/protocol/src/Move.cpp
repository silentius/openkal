/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/protocol/Move.h"

#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketReader.h"
#include "openkal/protocol/PacketWriter.h"

namespace openkal::protocol {
    namespace {
        std::vector<std::uint8_t>
        frame_delta(C2S opcode, std::int8_t dx, std::int8_t dy, std::int8_t dz) {
            PacketWriter writer;
            writer.write_u8(static_cast<std::uint8_t>(dx));
            writer.write_u8(static_cast<std::uint8_t>(dy));
            writer.write_u8(static_cast<std::uint8_t>(dz));
            return writer.frame(static_cast<std::uint8_t>(opcode));
        }
        std::vector<std::uint8_t>
        frame_player(S2C opcode, std::uint32_t id, std::int8_t dx, std::int8_t dy, std::int8_t dz) {
            PacketWriter writer;
            writer.write_u32(id);
            writer.write_u8(static_cast<std::uint8_t>(dx));
            writer.write_u8(static_cast<std::uint8_t>(dy));
            writer.write_u8(static_cast<std::uint8_t>(dz));
            return writer.frame(static_cast<std::uint8_t>(opcode));
        }
    }  // namespace

    std::vector<std::uint8_t>
    Move::frame_client_on(std::int8_t dx, std::int8_t dy, std::int8_t dz) {
        return frame_delta(C2S::MoveOn, dx, dy, dz);
    }
    std::vector<std::uint8_t>
    Move::frame_client_end(std::int8_t dx, std::int8_t dy, std::int8_t dz) {
        return frame_delta(C2S::MoveEnd, dx, dy, dz);
    }
    bool Move::parse_client(const std::uint8_t* data,
                            std::size_t size,
                            std::int8_t& dx,
                            std::int8_t& dy,
                            std::int8_t& dz) {
        if (!data || size < 3) {
            return false;
        }
        dx = static_cast<std::int8_t>(data[0]);
        dy = static_cast<std::int8_t>(data[1]);
        dz = static_cast<std::int8_t>(data[2]);
        return true;
    }
    std::vector<std::uint8_t>
    Move::frame_player_on(std::uint32_t id, std::int8_t dx, std::int8_t dy, std::int8_t dz) {
        return frame_player(S2C::MovePlayerOn, id, dx, dy, dz);
    }
    std::vector<std::uint8_t>
    Move::frame_player_end(std::uint32_t id, std::int8_t dx, std::int8_t dy, std::int8_t dz) {
        return frame_player(S2C::MovePlayerEnd, id, dx, dy, dz);
    }
    bool Move::parse_player(const std::uint8_t* data,
                            std::size_t size,
                            std::uint32_t& id,
                            std::int8_t& dx,
                            std::int8_t& dy,
                            std::int8_t& dz) {
        if (!data || size < 7) {
            return false;
        }
        PacketReader reader(data, size);
        std::uint8_t x = 0, y = 0, z = 0;
        if (!reader.read_u32(id) || !reader.read_u8(x) || !reader.read_u8(y) ||
            !reader.read_u8(z)) {
            return false;
        }
        dx = static_cast<std::int8_t>(x);
        dy = static_cast<std::int8_t>(y);
        dz = static_cast<std::int8_t>(z);
        return true;
    }
    std::vector<std::uint8_t>
    Move::frame_before(std::uint32_t id, int x, int y, int z, std::uint16_t dir) {
        PacketWriter writer;
        writer.write_u32(id);
        writer.write_u32(static_cast<std::uint32_t>(x));
        writer.write_u32(static_cast<std::uint32_t>(y));
        writer.write_u32(static_cast<std::uint32_t>(z));
        writer.write_u16(dir);
        return writer.frame(static_cast<std::uint8_t>(S2C::MoveBefore));
    }
    bool Move::parse_before(const std::uint8_t* data,
                            std::size_t size,
                            std::uint32_t& id,
                            int& x,
                            int& y,
                            int& z,
                            std::uint16_t& dir) {
        if (!data || size < 18) {
            return false;
        }
        PacketReader reader(data, size);
        std::uint32_t ux = 0, uy = 0, uz = 0;
        if (!reader.read_u32(id) || !reader.read_u32(ux) || !reader.read_u32(uy) ||
            !reader.read_u32(uz) || !reader.read_u16(dir)) {
            return false;
        }
        x = static_cast<int>(ux);
        y = static_cast<int>(uy);
        z = static_cast<int>(uz);
        return true;
    }

    std::vector<std::uint8_t> frame_c2s_move_on(std::int8_t x, std::int8_t y, std::int8_t z) {
        return Move::frame_client_on(x, y, z);
    }
    std::vector<std::uint8_t> frame_c2s_move_end(std::int8_t x, std::int8_t y, std::int8_t z) {
        return Move::frame_client_end(x, y, z);
    }
    bool parse_c2s_move(
        const std::uint8_t* d, std::size_t s, std::int8_t& x, std::int8_t& y, std::int8_t& z) {
        return Move::parse_client(d, s, x, y, z);
    }
    std::vector<std::uint8_t>
    frame_s2c_move_player_on(std::uint32_t id, std::int8_t x, std::int8_t y, std::int8_t z) {
        return Move::frame_player_on(id, x, y, z);
    }
    std::vector<std::uint8_t>
    frame_s2c_move_player_end(std::uint32_t id, std::int8_t x, std::int8_t y, std::int8_t z) {
        return Move::frame_player_end(id, x, y, z);
    }
    bool parse_s2c_move_player(const std::uint8_t* d,
                               std::size_t s,
                               std::uint32_t& id,
                               std::int8_t& x,
                               std::int8_t& y,
                               std::int8_t& z) {
        return Move::parse_player(d, s, id, x, y, z);
    }
    std::vector<std::uint8_t>
    frame_s2c_move_before(std::uint32_t id, int x, int y, int z, std::uint16_t dir) {
        return Move::frame_before(id, x, y, z, dir);
    }
    bool parse_s2c_move_before(const std::uint8_t* d,
                               std::size_t s,
                               std::uint32_t& id,
                               int& x,
                               int& y,
                               int& z,
                               std::uint16_t& dir) {
        return Move::parse_before(d, s, id, x, y, z, dir);
    }

}  // namespace openkal::protocol
