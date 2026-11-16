/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/protocol/UseItem.h"

#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketReader.h"
#include "openkal/protocol/PacketWriter.h"

namespace openkal::protocol {

    std::vector<std::uint8_t> UseItem::frame_c2s(std::uint32_t iid, std::uint8_t auto_hp_percent) {
        PacketWriter writer;
        writer.write_u32(iid);
        if (auto_hp_percent != 0) {
            writer.write_u8(auto_hp_percent);
        }
        return writer.frame(static_cast<std::uint8_t>(C2S::UseItem));
    }

    bool UseItem::parse_c2s(const std::uint8_t* data,
                            std::size_t size,
                            std::uint32_t& iid,
                            std::uint8_t* auto_hp_percent) {
        if (auto_hp_percent) {
            *auto_hp_percent = 0;
        }
        if (!data || (size != 4 && size != 5)) {
            return false;
        }
        PacketReader reader(data, size);
        if (!reader.read_u32(iid)) {
            return false;
        }
        if (size == 5) {
            std::uint8_t percent = 0;
            if (!reader.read_u8(percent) ||
                (percent != 25 && percent != 50 && percent != 75 && percent != 100)) {
                return false;
            }
            if (auto_hp_percent) {
                *auto_hp_percent = percent;
            }
        }
        return true;
    }

    bool UseItem::auto_hp_eligible(std::int32_t hp, std::int32_t max_hp, int percent) {
        return (percent == 25 || percent == 50 || percent == 75 || percent == 100) && hp > 0 &&
               max_hp > 0 &&
               static_cast<std::int64_t>(hp) * 100 < static_cast<std::int64_t>(max_hp) * percent;
    }

}  // namespace openkal::protocol
