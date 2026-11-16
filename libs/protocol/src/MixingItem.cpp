/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "openkal/protocol/MixingItem.h"

#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketReader.h"
#include "openkal/protocol/PacketWriter.h"

namespace openkal::protocol {

    std::vector<std::uint8_t> MixingItem::frame_c2s(std::uint8_t mode,
                                                    std::uint32_t weapon_iid,
                                                    std::uint32_t extra_weapon_iid,
                                                    std::uint32_t armor_iid,
                                                    std::uint32_t stone1_iid,
                                                    std::uint32_t stone2_iid) {
        PacketWriter writer;
        writer.write_u8(mode);
        writer.write_u32(weapon_iid);
        writer.write_u32(extra_weapon_iid);
        writer.write_u32(armor_iid);
        writer.write_u32(stone1_iid);
        writer.write_u32(stone2_iid);
        return writer.frame(static_cast<std::uint8_t>(C2S::Mixing));
    }

    bool MixingItem::parse_c2s(const std::uint8_t* data,
                               std::size_t size,
                               std::uint8_t& out_mode,
                               std::uint32_t& out_weapon_iid,
                               std::uint32_t& out_extra_weapon_iid,
                               std::uint32_t& out_armor_iid,
                               std::uint32_t& out_stone1_iid,
                               std::uint32_t& out_stone2_iid) {
        if (!data) {
            return false;
        }
        PacketReader reader(data, size);
        if (size >= 21) {
            return reader.read_u8(out_mode) && reader.read_u32(out_weapon_iid) &&
                   reader.read_u32(out_extra_weapon_iid) && reader.read_u32(out_armor_iid) &&
                   reader.read_u32(out_stone1_iid) && reader.read_u32(out_stone2_iid);
        }
        if (size < 17) {
            return false;
        }
        out_extra_weapon_iid = 0;
        return reader.read_u8(out_mode) && reader.read_u32(out_weapon_iid) &&
               reader.read_u32(out_armor_iid) && reader.read_u32(out_stone1_iid) &&
               reader.read_u32(out_stone2_iid);
    }

}  // namespace openkal::protocol
