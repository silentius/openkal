/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/protocol/MapState.h"

#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketReader.h"
#include "openkal/protocol/PacketWriter.h"

namespace openkal::protocol {

    std::vector<std::uint8_t> MapState::frame(const MapStateFields& fields) {
        PacketWriter writer;
        writer.write_u8(fields.map);
        writer.write_u8(fields.phase);
        writer.write_u8(fields.active_mask);
        writer.write_u8(static_cast<std::uint8_t>(fields.timer));
        writer.write_u32(fields.timer_seconds);
        return writer.frame(static_cast<std::uint8_t>(S2C::MapState));
    }

    bool MapState::parse(const std::uint8_t* data, std::size_t size, MapStateFields& fields) {
        if (data == nullptr || size != 8) {
            return false;
        }
        PacketReader reader(data, size);
        std::uint8_t timer = 0;
        if (!reader.read_u8(fields.map) || !reader.read_u8(fields.phase) ||
            !reader.read_u8(fields.active_mask) || !reader.read_u8(timer) ||
            !reader.read_u32(fields.timer_seconds)) {
            return false;
        }
        if (timer > static_cast<std::uint8_t>(MapStateTimer::Lord)) {
            return false;
        }
        fields.timer = static_cast<MapStateTimer>(timer);
        return true;
    }

}  // namespace openkal::protocol
