/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/protocol/TrashItem.h"

#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketReader.h"
#include "openkal/protocol/PacketWriter.h"

namespace openkal::protocol {

    std::vector<std::uint8_t> TrashItem::frame_c2s(std::uint32_t iid) {
        PacketWriter writer;
        writer.write_u32(iid);
        return writer.frame(static_cast<std::uint8_t>(C2S::TrashItem));
    }

    bool TrashItem::parse_c2s(const std::uint8_t* data, std::size_t size, std::uint32_t& iid) {
        if (!data || size < 4) {
            return false;
        }
        PacketReader reader(data, size);
        return reader.read_u32(iid);
    }

    std::vector<std::uint8_t> TrashItem::frame_s2c(std::uint32_t iid, std::uint8_t log_type) {
        PacketWriter writer;
        writer.write_u32(iid);
        writer.write_u8(log_type);
        return writer.frame(static_cast<std::uint8_t>(S2C::TrashItem));
    }

    bool TrashItem::parse_s2c(const std::uint8_t* data,
                              std::size_t size,
                              std::uint32_t& iid,
                              std::uint8_t& log_type) {
        if (!data || size < 5) {
            return false;
        }
        PacketReader reader(data, size);
        return reader.read_u32(iid) && reader.read_u8(log_type);
    }

}  // namespace openkal::protocol
