/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/protocol/Transform.h"

#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketReader.h"
#include "openkal/protocol/PacketWriter.h"
#include "openkal/protocol/Skill.h"

namespace openkal::protocol {

    std::vector<std::uint8_t> Transform::frame_c2s(std::uint8_t flag, std::uint8_t level) {
        PacketWriter w;
        w.write_u8(flag);
        w.write_u8(level);
        return w.frame(static_cast<std::uint8_t>(C2S::Transform));
    }

    bool Transform::parse_c2s(const std::uint8_t* data,
                              std::size_t size,
                              std::uint8_t& flag,
                              std::uint8_t& level) {
        if (!data || size < 2) {
            return false;
        }
        PacketReader r(data, size);
        return r.read_u8(flag) && r.read_u8(level);
    }

    std::vector<std::uint8_t> Transform::frame_c2s_skill(std::uint8_t skill) {
        PacketWriter w;
        w.write_u8(skill);
        return w.frame(static_cast<std::uint8_t>(C2S::TransformSkill));
    }

    bool
    Transform::parse_c2s_skill(const std::uint8_t* data, std::size_t size, std::uint8_t& skill) {
        if (!data || size < 1) {
            return false;
        }
        PacketReader r(data, size);
        return r.read_u8(skill);
    }

    std::vector<std::uint8_t> Transform::frame_action_pre(std::uint32_t entity_id,
                                                          std::uint8_t stage) {
        PacketWriter w;
        w.write_u32(entity_id);
        w.write_u8(kAtPreTransform);
        w.write_u8(stage);
        return w.frame(static_cast<std::uint8_t>(S2C::Action));
    }

    std::vector<std::uint8_t> Transform::frame_action_form(std::uint32_t entity_id,
                                                           std::uint8_t tflag) {
        PacketWriter w;
        w.write_u32(entity_id);
        w.write_u8(kAtTransform);
        w.write_u8(tflag);
        return w.frame(static_cast<std::uint8_t>(S2C::Action));
    }

}  // namespace openkal::protocol
