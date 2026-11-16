/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/protocol/EnchantItem.h"

#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketReader.h"
#include "openkal/protocol/PacketWriter.h"

namespace openkal::protocol {

    std::vector<std::uint8_t> EnchantItem::frame_c2s(std::uint32_t from_iid, std::uint32_t to_iid) {
        PacketWriter writer;
        writer.write_u32(from_iid);
        writer.write_u32(to_iid);
        return writer.frame(static_cast<std::uint8_t>(C2S::EnchantItem));
    }

    bool EnchantItem::parse_c2s(const std::uint8_t* data,
                                std::size_t size,
                                std::uint32_t& from_iid,
                                std::uint32_t& to_iid) {
        if (!data || size < 8) {
            return false;
        }
        PacketReader reader(data, size);
        return reader.read_u32(from_iid) && reader.read_u32(to_iid);
    }

}  // namespace openkal::protocol
