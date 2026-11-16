/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/protocol/UpgradeItem.h"

#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketReader.h"
#include "openkal/protocol/PacketWriter.h"

namespace openkal::protocol {

    std::vector<std::uint8_t> UpgradeItem::frame_c2s(UpgradeItemType type, std::uint32_t iid) {
        PacketWriter writer;
        writer.write_u8(static_cast<std::uint8_t>(type));
        writer.write_u32(iid);
        return writer.frame(static_cast<std::uint8_t>(C2S::UpgradeItem));
    }

    bool UpgradeItem::parse_c2s(const std::uint8_t* data,
                                std::size_t size,
                                UpgradeItemType& type,
                                std::uint32_t& iid) {
        if (!data || size < 5) {
            return false;
        }
        PacketReader reader(data, size);
        std::uint8_t raw = 0;
        if (!reader.read_u8(raw) || !reader.read_u32(iid)) {
            return false;
        }
        if (raw > static_cast<std::uint8_t>(UpgradeItemType::Level)) {
            return false;
        }
        type = static_cast<UpgradeItemType>(raw);
        return true;
    }

}  // namespace openkal::protocol
