/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/protocol/ItemMonster.h"

#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketReader.h"
#include "openkal/protocol/PacketWriter.h"

namespace openkal::protocol {

    std::vector<std::uint8_t> ItemMonster::frame_get(std::uint32_t monster_id) {
        PacketWriter w;
        w.write_u32(monster_id);
        return w.frame(static_cast<std::uint8_t>(C2S::GetItemMonster));
    }

    std::vector<std::uint8_t> ItemMonster::frame_cancel(std::uint32_t monster_id) {
        PacketWriter w;
        w.write_u32(monster_id);
        return w.frame(static_cast<std::uint8_t>(C2S::CancelItemMonster));
    }

    bool
    ItemMonster::parse_id(const std::uint8_t* data, std::size_t size, std::uint32_t& monster_id) {
        PacketReader r(data, size);
        return r.read_u32(monster_id);
    }

    std::vector<std::uint8_t> ItemMonster::frame_message_v(std::uint8_t msg,
                                                           std::uint8_t open_sec) {
        PacketWriter w;
        w.write_u8(msg);
        w.write_u8(open_sec);
        return w.frame(static_cast<std::uint8_t>(S2C::MessageV));
    }

    std::vector<std::uint8_t> ItemMonster::frame_message(std::uint8_t msg) {
        PacketWriter w;
        w.write_u8(msg);
        return w.frame(static_cast<std::uint8_t>(S2C::Message));
    }

    std::vector<std::uint8_t> ItemMonster::frame_gstate(std::uint32_t player_id,
                                                        std::uint32_t gstate) {
        PacketWriter w;
        w.write_u32(player_id);
        w.write_u32(gstate);
        return w.frame(static_cast<std::uint8_t>(S2C::GState));
    }

}  // namespace openkal::protocol
