/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/protocol/Property.h"

#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketReader.h"
#include "openkal/protocol/PacketWriter.h"

namespace openkal::protocol {

    std::vector<std::uint8_t> Property::frame_c2s_update(std::uint8_t property,
                                                         std::uint8_t count) {
        if (count == 0) {
            return {};
        }
        PacketWriter writer;
        writer.write_u8(property);
        if (count > 1) {
            writer.write_u8(count);
        }
        return writer.frame(static_cast<std::uint8_t>(C2S::UpdateProperty));
    }

    bool Property::parse_c2s_update(const std::uint8_t* data,
                                    std::size_t size,
                                    std::uint8_t& property,
                                    std::uint8_t& count) {
        if (!data || size < 1 || size > 2) {
            return false;
        }
        PacketReader reader(data, size);
        count = 1;
        if (!reader.read_u8(property)) {
            return false;
        }
        if (reader.remaining() > 0 && !reader.read_u8(count)) {
            return false;
        }
        return count > 0 && reader.remaining() == 0;
    }

    bool
    Property::parse_c2s_update(const std::uint8_t* data, std::size_t size, std::uint8_t& property) {
        std::uint8_t count = 0;
        return parse_c2s_update(data, size, property, count);
    }

    std::vector<std::uint8_t> Property::frame_update(std::uint8_t property, std::int32_t value) {
        PacketWriter writer;
        writer.write_u8(property);
        writer.write_i32(value);
        return writer.frame(static_cast<std::uint8_t>(S2C::UpdateProperty));
    }

    std::vector<std::uint8_t> Property::frame_update_level(std::int32_t level) {
        PacketWriter writer;
        writer.write_u8(static_cast<std::uint8_t>(PropertyType::Level));
        writer.write_i32(level);
        return writer.frame(static_cast<std::uint8_t>(S2C::UpdateProperty));
    }

    std::vector<std::uint8_t> Property::frame_update_exp(std::int64_t current, std::int64_t delta) {
        PacketWriter writer;
        writer.write_u8(static_cast<std::uint8_t>(PropertyType::Exp));
        writer.write_u64(static_cast<std::uint64_t>(current));
        writer.write_u64(static_cast<std::uint64_t>(delta));
        return writer.frame(static_cast<std::uint8_t>(S2C::UpdateProperty));
    }

    std::vector<std::uint8_t> Property::frame_update_points(std::uint8_t property,
                                                            std::int32_t value) {
        return frame_update(property, value);
    }

    std::vector<std::uint8_t> Property::frame_update_vital(std::uint8_t property,
                                                           std::int32_t current,
                                                           std::int32_t maximum) {
        PacketWriter writer;
        writer.write_u8(property);
        writer.write_i32(current);
        writer.write_i32(maximum);
        return writer.frame(static_cast<std::uint8_t>(S2C::UpdateProperty));
    }

    bool Property::parse_update(const std::uint8_t* data,
                                std::size_t size,
                                std::uint8_t& property,
                                std::int32_t& value) {
        UpdatePropertyFields fields;
        if (!parse_update_any(data, size, fields) || fields.kind != UpdatePropertyKind::Word) {
            return false;
        }
        property = fields.property;
        value = fields.word_value;
        return true;
    }

    bool Property::parse_update_any(const std::uint8_t* data,
                                    std::size_t size,
                                    UpdatePropertyFields& out) {
        if (!data || size < 5) {
            return false;
        }
        PacketReader reader(data, size);
        if (!reader.read_u8(out.property)) {
            return false;
        }
        if (out.property == static_cast<std::uint8_t>(PropertyType::Exp)) {
            if (size < 1 + 8 + 8) {
                return false;
            }
            std::uint64_t cur = 0;
            std::uint64_t delta = 0;
            if (!reader.read_u64(cur) || !reader.read_u64(delta)) {
                return false;
            }
            out.kind = UpdatePropertyKind::Exp;
            out.exp_current = static_cast<std::int64_t>(cur);
            out.exp_delta = static_cast<std::int64_t>(delta);
            return true;
        }
        if (out.property == static_cast<std::uint8_t>(PropertyType::Level)) {
            if (!reader.read_i32(out.level_value)) {
                return false;
            }
            out.kind = UpdatePropertyKind::Level;
            return true;
        }
        if (out.property == static_cast<std::uint8_t>(PropertyType::Hp) ||
            out.property == static_cast<std::uint8_t>(PropertyType::Mp)) {
            if (!reader.read_i32(out.vital_cur) || !reader.read_i32(out.vital_max)) {
                return false;
            }
            out.kind = UpdatePropertyKind::Vital;
            return true;
        }
        if (!reader.read_i32(out.word_value)) {
            return false;
        }
        out.kind = UpdatePropertyKind::Word;
        return true;
    }

    std::vector<std::uint8_t> frame_c2s_update_property(std::uint8_t property) {
        return Property::frame_c2s_update(property);
    }

    bool
    parse_c2s_update_property(const std::uint8_t* data, std::size_t size, std::uint8_t& property) {
        return Property::parse_c2s_update(data, size, property);
    }

    std::vector<std::uint8_t> frame_s2c_update_property(std::uint8_t p, std::int32_t v) {
        return Property::frame_update(p, v);
    }

    std::vector<std::uint8_t> frame_s2c_update_property_level(std::int32_t level) {
        return Property::frame_update_level(level);
    }

    std::vector<std::uint8_t> frame_s2c_update_property_exp(std::int64_t current,
                                                            std::int64_t delta) {
        return Property::frame_update_exp(current, delta);
    }

    std::vector<std::uint8_t> frame_s2c_update_property_vital(std::uint8_t property,
                                                              std::int32_t current,
                                                              std::int32_t maximum) {
        return Property::frame_update_vital(property, current, maximum);
    }

    bool parse_s2c_update_property(const std::uint8_t* d,
                                   std::size_t s,
                                   std::uint8_t& p,
                                   std::int32_t& v) {
        return Property::parse_update(d, s, p, v);
    }

    bool
    parse_s2c_update_property_any(const std::uint8_t* d, std::size_t s, UpdatePropertyFields& o) {
        return Property::parse_update_any(d, s, o);
    }

    std::vector<std::uint8_t> Property::frame_update_mproperty(std::uint32_t entity_id,
                                                               std::uint8_t type,
                                                               std::int16_t value) {
        PacketWriter writer;
        writer.write_u32(entity_id);
        writer.write_u8(type);
        writer.write_u16(static_cast<std::uint16_t>(value));
        return writer.frame(static_cast<std::uint8_t>(S2C::UpdateMProperty));
    }

    bool Property::parse_update_mproperty(const std::uint8_t* data,
                                          std::size_t size,
                                          std::uint32_t& entity_id,
                                          std::uint8_t& type,
                                          std::int16_t& value) {
        if (!data) {
            return false;
        }
        PacketReader reader(data, size);
        std::uint16_t raw = 0;
        if (!reader.read_u32(entity_id) || !reader.read_u8(type) || !reader.read_u16(raw)) {
            return false;
        }
        value = static_cast<std::int16_t>(raw);
        return true;
    }

}  // namespace openkal::protocol
