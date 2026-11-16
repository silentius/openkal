/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketReader.h"
#include "openkal/protocol/PacketWriter.h"
#include "openkal/server/store/DataStore.h"

namespace openkal::server {
    /** OpenKal-only TLS lobby request opcode, outside the compatibility range. */
    inline constexpr std::uint8_t kLobbyRequest = 240;
    /** OpenKal-only lobby response opcode. */
    inline constexpr std::uint8_t kLobbyResponse = 241;
    static_assert(static_cast<int>(protocol::C2S::End) < kLobbyRequest,
                  "Lobby request must not overlap compatibility opcodes");
    static_assert(static_cast<int>(protocol::S2C::End) < kLobbyRequest,
                  "Lobby response must not overlap compatibility opcodes");
    /** Versioned lobby operations; world entry retains the existing handshake. */
    enum class LobbyOperation : std::uint8_t {
        Capabilities,
        Register,
        Login,
        Characters,
        Deleted,
        Create,
        Delete,
        Restore,
        Select,
        Items
    };

    /** Writes a character snapshot. @param writer Payload writer. @param record Snapshot. */
    void write_lobby_character(protocol::PacketWriter& writer, const CharacterRecord& record);

    /** Reads a bounded character snapshot. @param reader Payload reader. @param record Destination.
     * @return True on success. */
    [[nodiscard]] bool read_lobby_character(protocol::PacketReader& reader,
                                            CharacterRecord& record);

    /** Writes an item preview. @param writer Payload writer. @param record Item row. */
    void write_lobby_item(protocol::PacketWriter& writer, const ItemRecord& record);

    /** Reads an item preview. @param reader Payload reader. @param record Destination. @return True
     * on success. */
    [[nodiscard]] bool read_lobby_item(protocol::PacketReader& reader, ItemRecord& record);
}  // namespace openkal::server
