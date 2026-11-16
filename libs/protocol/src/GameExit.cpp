/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/protocol/GameExit.h"

#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketWriter.h"

namespace openkal::protocol {

    std::vector<std::uint8_t> GameExit::frame_client_request() {
        PacketWriter writer;
        return writer.frame(static_cast<std::uint8_t>(C2S::GameExit));
    }

    bool GameExit::parse_client_request(const std::uint8_t* data, std::size_t size) {
        (void) data;
        return size == 0;
    }

    std::vector<std::uint8_t> GameExit::frame_server_answer(bool saved) {
        PacketWriter writer;
        writer.write_u8(saved ? 1u : 0u);
        return writer.frame(static_cast<std::uint8_t>(S2C::AnsGameExit));
    }

    bool GameExit::parse_server_answer(const std::uint8_t* data, std::size_t size, bool& saved) {
        if (data == nullptr || size != 1) {
            return false;
        }
        saved = data[0] != 0;
        return true;
    }

}  // namespace openkal::protocol
