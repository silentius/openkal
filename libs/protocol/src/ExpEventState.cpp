/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/protocol/ExpEventState.h"

#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketReader.h"
#include "openkal/protocol/PacketWriter.h"

namespace openkal::protocol {
    std::vector<std::uint8_t> ExpEventState::frame(const ExpEventStateFields& fields) {
        PacketWriter writer;
        writer.write_u8(fields.enabled ? 1 : 0);
        writer.write_i32(fields.manager_npc_index);
        writer.write_i32(fields.donation_item_index);
        writer.write_u64(fields.contributed);
        writer.write_u64(fields.target);
        writer.write_i32(fields.active_bonus_percent);
        writer.write_u32(fields.remaining_seconds);
        writer.write_i32(fields.next_bonus_percent);
        return writer.frame(static_cast<std::uint8_t>(S2C::ExpEventState));
    }

    bool
    ExpEventState::parse(const std::uint8_t* data, std::size_t size, ExpEventStateFields& fields) {
        if (data == nullptr || size != 37) {
            return false;
        }
        PacketReader reader(data, size);
        ExpEventStateFields decoded;
        std::uint8_t enabled = 0;
        if (!reader.read_u8(enabled) || enabled > 1 ||
            !reader.read_i32(decoded.manager_npc_index) ||
            !reader.read_i32(decoded.donation_item_index) ||
            !reader.read_u64(decoded.contributed) || !reader.read_u64(decoded.target) ||
            !reader.read_i32(decoded.active_bonus_percent) ||
            !reader.read_u32(decoded.remaining_seconds) ||
            !reader.read_i32(decoded.next_bonus_percent)) {
            return false;
        }
        decoded.enabled = enabled != 0;
        if (decoded.active_bonus_percent < 0 || decoded.next_bonus_percent < 0 ||
            ((decoded.active_bonus_percent > 0) != (decoded.remaining_seconds > 0)) ||
            (decoded.enabled &&
             (decoded.manager_npc_index <= 0 || decoded.donation_item_index <= 0 ||
              decoded.target == 0 || decoded.next_bonus_percent == 0))) {
            return false;
        }
        fields = decoded;
        return true;
    }
}  // namespace openkal::protocol
