/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/protocol/BuffState.h"

#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketReader.h"
#include "openkal/protocol/PacketWriter.h"

namespace openkal::protocol {

    std::string_view charm_effect_fx_name(std::uint8_t effect) {
        switch (effect) {
        case 4:
            return "UR_01-1";
        case 5:
            return "UR_01-3";
        case 6:
            return "UR_02-5";
        case 7:
            return "UR_01-4";
        case 8:
            return "UR_03-2";
        case 9:
            return "UR_02-3";
        case 10:
            return "UR_04-2";
        case 11:
            return "UR_03-3";
        default:
            return {};
        }
    }

    std::string_view refresh_effect_fx_name(std::uint8_t effect) {
        switch (effect) {
        case 1:
            return "effect_skill01";
        case 2:
            return "effect_skill04";
        default:
            return {};
        }
    }

    std::string_view oneshot_effect_fx_name(std::uint8_t effect) {
        const std::string_view charm = charm_effect_fx_name(effect);
        if (!charm.empty()) {
            return charm;
        }
        return refresh_effect_fx_name(effect);
    }

    std::vector<std::uint8_t> BuffState::frame_add(std::uint64_t bstate,
                                                   std::int32_t remain,
                                                   std::int32_t skill_index,
                                                   std::int32_t item_index) {
        PacketWriter w;
        w.write_u8(static_cast<std::uint8_t>(BStateMsg::AddBState));
        w.write_u64(bstate);
        w.write_i32(remain);
        w.write_i32(skill_index);
        w.write_i32(item_index);
        return w.frame(static_cast<std::uint8_t>(S2C::MessageV));
    }

    std::vector<std::uint8_t> BuffState::frame_sub(std::uint64_t bstate) {
        PacketWriter w;
        w.write_u8(static_cast<std::uint8_t>(BStateMsg::SubBState));
        w.write_u64(bstate);
        return w.frame(static_cast<std::uint8_t>(S2C::MessageV));
    }

    bool BuffState::parse_add(const std::uint8_t* data,
                              std::size_t size,
                              std::uint64_t& bstate,
                              std::int32_t& remain,
                              std::int32_t& skill_index,
                              std::int32_t& item_index) {
        PacketReader r(data, size);
        skill_index = 0;
        item_index = 0;
        if (!r.read_u64(bstate) || !r.read_i32(remain)) {
            return false;
        }
        if (r.remaining() >= 4) {
            if (!r.read_i32(skill_index)) {
                return false;
            }
        }
        if (r.remaining() >= 4) {
            return r.read_i32(item_index);
        }
        return true;
    }

    bool BuffState::parse_sub(const std::uint8_t* data, std::size_t size, std::uint64_t& bstate) {
        PacketReader r(data, size);
        return r.read_u64(bstate);
    }

}  // namespace openkal::protocol
