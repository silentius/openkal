/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/protocol/Cooking.h"

#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketReader.h"
#include "openkal/protocol/PacketWriter.h"

namespace openkal::protocol {

    std::vector<std::uint8_t> Cooking::frame_c2s(std::uint8_t type, const CookingOrder& order) {
        PacketWriter writer;
        writer.write_u8(type);
        writer.write_u16(order.blood_fish);
        writer.write_u16(order.medicine_from_blood_fish);
        writer.write_u16(order.medicine_direct);
        return writer.frame(static_cast<std::uint8_t>(C2S::Cooking));
    }

    bool Cooking::parse_c2s(const std::uint8_t* data,
                            std::size_t size,
                            std::uint8_t& type,
                            CookingOrder& order) {
        PacketReader reader(data, size);
        return data && size == 7 && reader.read_u8(type) && reader.read_u16(order.blood_fish) &&
               reader.read_u16(order.medicine_from_blood_fish) &&
               reader.read_u16(order.medicine_direct) && reader.remaining() == 0;
    }

}  // namespace openkal::protocol
