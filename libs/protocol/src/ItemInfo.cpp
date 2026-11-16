/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/protocol/ItemInfo.h"

#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketReader.h"
#include "openkal/protocol/PacketWriter.h"

namespace openkal::protocol {

    std::vector<std::uint8_t> ItemInfo::frame_update(const UpdateItemInfoFields& fields) {
        PacketWriter writer;
        writer.write_u32(fields.iid);
        writer.write_u16(fields.index);
        writer.write_u8(fields.level);
        writer.write_u32(fields.exp);
        writer.write_u16(fields.added);
        return writer.frame(static_cast<std::uint8_t>(S2C::UpdateItemInfo));
    }

    bool
    ItemInfo::parse_update(const std::uint8_t* data, std::size_t size, UpdateItemInfoFields& out) {
        if (!data || size < 13) {
            return false;
        }
        PacketReader reader(data, size);
        return reader.read_u32(out.iid) && reader.read_u16(out.index) &&
               reader.read_u8(out.level) && reader.read_u32(out.exp) && reader.read_u16(out.added);
    }

    std::vector<std::uint8_t> frame_s2c_update_item_info(const UpdateItemInfoFields& fields) {
        return ItemInfo::frame_update(fields);
    }

    bool parse_s2c_update_item_info(const std::uint8_t* data,
                                    std::size_t size,
                                    UpdateItemInfoFields& out) {
        return ItemInfo::parse_update(data, size, out);
    }

}  // namespace openkal::protocol
