/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/protocol/CreatePlayer.h"

#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketReader.h"
#include "openkal/protocol/PacketWriter.h"

#include <algorithm>
namespace openkal::protocol {
    std::vector<std::uint8_t> CreatePlayer::frame(std::uint32_t id,
                                                  const std::string& name,
                                                  std::uint8_t class_byte,
                                                  int x,
                                                  int y,
                                                  int z,
                                                  std::uint16_t dir,
                                                  std::uint32_t gstate,
                                                  const std::uint16_t wear[7],
                                                  std::uint8_t face,
                                                  std::uint8_t hair,
                                                  const SkinIndices& skins,
                                                  std::uint16_t shield) {
        PacketWriter w;
        w.write_u32(id);
        w.write_string(name);
        w.write_u8(class_byte);
        w.write_u32(static_cast<std::uint32_t>(x));
        w.write_u32(static_cast<std::uint32_t>(y));
        w.write_u32(static_cast<std::uint32_t>(z));
        w.write_u16(dir);
        w.write_u32(gstate);
        for (int i = 0; i < 7; ++i) {
            w.write_u16(wear[i]);
        }
        w.write_u8(face);
        w.write_u8(hair);
        // Preserve the legacy payload when neither cosmetics nor a shield require the extension.
        if (shield != 0 ||
            std::any_of(skins.begin(), skins.end(), [](auto index) { return index != 0; })) {
            w.write_u8(1);  // Cosmetic extension version.
            for (auto index : skins) {
                w.write_u16(index);
            }
            w.write_u16(shield);
        }
        return w.frame(static_cast<std::uint8_t>(S2C::CreatePlayer));
    }
    bool CreatePlayer::parse(const std::uint8_t* data, std::size_t size, CreatePlayerFields& out) {
        if (!data) {
            return false;
        }
        PacketReader r(data, size);
        if (!r.read_u32(out.id) || !r.read_string(out.name) || !r.read_u8(out.class_byte)) {
            return false;
        }
        std::uint32_t ux = 0, uy = 0, uz = 0;
        if (!r.read_u32(ux) || !r.read_u32(uy) || !r.read_u32(uz) || !r.read_u16(out.dir) ||
            !r.read_u32(out.gstate)) {
            return false;
        }
        out.x = static_cast<int>(ux);
        out.y = static_cast<int>(uy);
        out.z = static_cast<int>(uz);
        for (int i = 0; i < 7; ++i) {
            if (!r.read_u16(out.wear[i])) {
                return false;
            }
        }
        out.skins.fill(0);
        out.shield = 0;
        if (!r.read_u8(out.face) || !r.read_u8(out.hair)) {
            return false;
        }
        if (r.remaining() == 0) {
            return true;
        }
        std::uint8_t version = 0;
        if (r.remaining() != 3 + 2 * kSkinSlots || !r.read_u8(version) || version != 1) {
            return false;
        }
        for (auto& index : out.skins) {
            if (!r.read_u16(index)) {
                return false;
            }
        }
        return r.read_u16(out.shield);
    }
    std::vector<std::uint8_t> frame_s2c_create_player(std::uint32_t id,
                                                      const std::string& n,
                                                      std::uint8_t c,
                                                      int x,
                                                      int y,
                                                      int z,
                                                      std::uint16_t d,
                                                      std::uint32_t g,
                                                      const std::uint16_t w[7],
                                                      std::uint8_t f,
                                                      std::uint8_t h,
                                                      const SkinIndices& skins,
                                                      std::uint16_t shield) {
        return CreatePlayer::frame(id, n, c, x, y, z, d, g, w, f, h, skins, shield);
    }
    bool parse_s2c_create_player(const std::uint8_t* d, std::size_t s, CreatePlayerFields& o) {
        return CreatePlayer::parse(d, s, o);
    }

    std::vector<std::uint8_t> CreatePlayer::frame_remove(std::uint32_t id) {
        PacketWriter w;
        w.write_u32(id);
        return w.frame(static_cast<std::uint8_t>(S2C::RemovePlayer));
    }

    bool
    CreatePlayer::parse_remove(const std::uint8_t* data, std::size_t size, std::uint32_t& out_id) {
        if (!data || size < 4) {
            return false;
        }
        PacketReader r(data, size);
        return r.read_u32(out_id);
    }

    std::vector<std::uint8_t> frame_s2c_remove_player(std::uint32_t id) {
        return CreatePlayer::frame_remove(id);
    }

    bool
    parse_s2c_remove_player(const std::uint8_t* data, std::size_t size, std::uint32_t& out_id) {
        return CreatePlayer::parse_remove(data, size, out_id);
    }
}  // namespace openkal::protocol
