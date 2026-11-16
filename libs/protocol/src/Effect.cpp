/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/protocol/Effect.h"

#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketReader.h"
#include "openkal/protocol/PacketWriter.h"

namespace openkal::protocol {

    std::vector<std::uint8_t> Effect::frame(std::uint32_t entity_id, std::uint8_t effect) {
        PacketWriter writer;
        writer.write_u32(entity_id);
        writer.write_u8(effect);
        return writer.frame(static_cast<std::uint8_t>(S2C::Effect));
    }

    std::vector<std::uint8_t> Effect::frame_damaged(std::uint32_t entity_id, std::uint16_t damage) {
        PacketWriter writer;
        writer.write_u32(entity_id);
        writer.write_u8(static_cast<std::uint8_t>(EffectType::Damaged));
        writer.write_u16(damage);
        return writer.frame(static_cast<std::uint8_t>(S2C::Effect));
    }

    bool Effect::parse(const std::uint8_t* data,
                       std::size_t size,
                       std::uint32_t& entity_id,
                       std::uint8_t& effect) {
        if (!data || size < 5) {
            return false;
        }
        PacketReader reader(data, size);
        return reader.read_u32(entity_id) && reader.read_u8(effect);
    }

    bool Effect::parse_damaged(const std::uint8_t* data,
                               std::size_t size,
                               std::uint32_t& entity_id,
                               std::uint16_t& damage) {
        if (!data || size < 7) {
            return false;
        }
        PacketReader reader(data, size);
        std::uint8_t effect = 0;
        return reader.read_u32(entity_id) && reader.read_u8(effect) &&
               effect == static_cast<std::uint8_t>(EffectType::Damaged) && reader.read_u16(damage);
    }

    std::vector<std::uint8_t> frame_s2c_effect(std::uint32_t entity_id, std::uint8_t effect) {
        return Effect::frame(entity_id, effect);
    }

    bool parse_s2c_effect(const std::uint8_t* data,
                          std::size_t size,
                          std::uint32_t& entity_id,
                          std::uint8_t& effect) {
        return Effect::parse(data, size, entity_id, effect);
    }

}  // namespace openkal::protocol
