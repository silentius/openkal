/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/protocol/Teleport.h"

#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketReader.h"
#include "openkal/protocol/PacketWriter.h"
namespace openkal::protocol {
    std::vector<std::uint8_t>
    Teleport::frame_server(std::uint8_t map, int x, int y, int z, std::uint8_t cheat) {
        PacketWriter w;
        w.write_u8(map);
        w.write_u32(static_cast<std::uint32_t>(x));
        w.write_u32(static_cast<std::uint32_t>(y));
        w.write_u32(static_cast<std::uint32_t>(z));
        w.write_u8(cheat);
        return w.frame(static_cast<std::uint8_t>(S2C::Teleport));
    }
    bool Teleport::parse_server(const std::uint8_t* d,
                                std::size_t s,
                                std::uint8_t& map,
                                int& x,
                                int& y,
                                int& z,
                                std::uint8_t& cheat) {
        if (!d || s < 14) {
            return false;
        }
        PacketReader r(d, s);
        std::uint32_t ux = 0, uy = 0, uz = 0;
        if (!r.read_u8(map) || !r.read_u32(ux) || !r.read_u32(uy) || !r.read_u32(uz) ||
            !r.read_u8(cheat)) {
            return false;
        }
        x = static_cast<int>(ux);
        y = static_cast<int>(uy);
        z = static_cast<int>(uz);
        return true;
    }
    std::vector<std::uint8_t> Teleport::frame_client_request(std::uint8_t location) {
        PacketWriter w;
        w.write_u8(location);
        return w.frame(static_cast<std::uint8_t>(C2S::Teleport));
    }
    std::vector<std::uint8_t> Teleport::frame_client_ack(std::uint8_t ok, int z) {
        PacketWriter w;
        w.write_u8(ok);
        w.write_u32(static_cast<std::uint32_t>(z));
        return w.frame(static_cast<std::uint8_t>(C2S::Teleport));
    }
    bool
    Teleport::parse_client_request(const std::uint8_t* d, std::size_t s, std::uint8_t& location) {
        if (!d || s != 1) {
            return false;
        }
        location = d[0];
        return true;
    }
    bool
    Teleport::parse_client_ack(const std::uint8_t* d, std::size_t s, std::uint8_t& ok, int& z) {
        if (!d || s < 5) {
            return false;
        }
        PacketReader r(d, s);
        std::uint32_t uz = 0;
        if (!r.read_u8(ok) || !r.read_u32(uz)) {
            return false;
        }
        z = static_cast<int>(uz);
        return true;
    }
    std::vector<std::uint8_t>
    frame_s2c_teleport(std::uint8_t m, int x, int y, int z, std::uint8_t c) {
        return Teleport::frame_server(m, x, y, z, c);
    }
    bool parse_s2c_teleport(const std::uint8_t* d,
                            std::size_t s,
                            std::uint8_t& m,
                            int& x,
                            int& y,
                            int& z,
                            std::uint8_t& c) {
        return Teleport::parse_server(d, s, m, x, y, z, c);
    }
    std::vector<std::uint8_t> frame_c2s_teleport_request(std::uint8_t l) {
        return Teleport::frame_client_request(l);
    }
    std::vector<std::uint8_t> frame_c2s_teleport_ack(std::uint8_t o, int z) {
        return Teleport::frame_client_ack(o, z);
    }
    bool parse_c2s_teleport_request(const std::uint8_t* d, std::size_t s, std::uint8_t& l) {
        return Teleport::parse_client_request(d, s, l);
    }
    bool parse_c2s_teleport_ack(const std::uint8_t* d, std::size_t s, std::uint8_t& o, int& z) {
        return Teleport::parse_client_ack(d, s, o, z);
    }
}  // namespace openkal::protocol
