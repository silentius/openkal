/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/protocol/MState.h"

#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketReader.h"
#include "openkal/protocol/PacketWriter.h"

namespace openkal::protocol {

    std::vector<std::uint8_t> MState::frame(std::uint32_t entity_id, std::uint64_t mstate) {
        PacketWriter writer;
        writer.write_u32(entity_id);
        writer.write_u64(mstate);
        return writer.frame(static_cast<std::uint8_t>(S2C::MState));
    }

    bool MState::parse(const std::uint8_t* data,
                       std::size_t size,
                       std::uint32_t& entity_id,
                       std::uint64_t& mstate) {
        if (!data || size < 12) {
            return false;
        }
        PacketReader reader(data, size);
        return reader.read_u32(entity_id) && reader.read_u64(mstate);
    }

}  // namespace openkal::protocol
