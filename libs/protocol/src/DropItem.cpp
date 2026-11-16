/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/protocol/DropItem.h"

#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketReader.h"
#include "openkal/protocol/PacketWriter.h"

namespace openkal::protocol {

    std::vector<std::uint8_t> DropItem::frame_c2s(std::uint32_t iid, std::uint32_t count) {
        PacketWriter writer;
        writer.write_u32(iid);
        writer.write_u32(count);
        return writer.frame(static_cast<std::uint8_t>(C2S::DropItem));
    }

    bool DropItem::parse_c2s(const std::uint8_t* data,
                             std::size_t size,
                             std::uint32_t& iid,
                             std::uint32_t& count) {
        if (!data || size < 8) {
            return false;
        }
        PacketReader reader(data, size);
        return reader.read_u32(iid) && reader.read_u32(count);
    }

}  // namespace openkal::protocol
