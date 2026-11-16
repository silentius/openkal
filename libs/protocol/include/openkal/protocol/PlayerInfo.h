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

#include "openkal/protocol/PacketReader.h"
#include "openkal/protocol/PacketWriter.h"

#include <cstdint>
#include <string>
#include <vector>

namespace openkal::protocol {

    /**
     * One character slot as carried by S2C_PLAYERINFO (intentional wire DTO).
     * Base int32-stat layout (no DEF_UI_PACKET / DEF_CHAR_EXP): "dsbdddddddbb" +
     * wear "b" + "w"*N.
     */
    struct PlayerInfoChar {
        std::int32_t pid = 0;
        std::string name;

        /** Job: 0 knight / 1 mage / 2 archer. */
        std::uint8_t job = 0;

        std::int32_t level = 1;
        std::int32_t gid = 0;
        std::int32_t str = 10;
        std::int32_t hth = 10;
        std::int32_t intel = 10;
        std::int32_t wis = 10;
        std::int32_t dex = 10;
        std::uint8_t face = 0;
        std::uint8_t hair = 0;

        /** Equipped item definition indices. */
        std::vector<std::uint16_t> wear;
    };

    /**
     * Full S2C_PLAYERINFO payload (intentional wire DTO).
     * Layout "bdbm" = auth, exp_time, count, char-blob.
     */
    struct PlayerInfoMsg {
        std::uint8_t auth_type = 0;
        std::int32_t exp_time = 0;
        std::vector<PlayerInfoChar> characters;
    };

    /** Encodes and decodes player-info messages. */
    class PlayerInfo final {
      public:
        PlayerInfo() = delete;
        [[nodiscard]] static bool encode_characters(PacketWriter& writer,
                                                    const std::vector<PlayerInfoChar>& characters);
        [[nodiscard]] static bool decode_characters(PacketReader& reader,
                                                    std::uint8_t count,
                                                    std::vector<PlayerInfoChar>& out);
        [[nodiscard]] static bool encode_payload(PacketWriter& writer,
                                                 const PlayerInfoMsg& message);
        [[nodiscard]] static bool decode_payload(PacketReader& reader, PlayerInfoMsg& out);
        [[nodiscard]] static std::vector<std::uint8_t> frame(const PlayerInfoMsg& message);
        [[nodiscard]] static bool parse(const std::vector<std::uint8_t>& frame, PlayerInfoMsg& out);
    };

    [[nodiscard]] bool encode_playerinfo_chars(PacketWriter& writer,
                                               const std::vector<PlayerInfoChar>& characters);
    [[nodiscard]] bool decode_playerinfo_chars(PacketReader& reader,
                                               std::uint8_t count,
                                               std::vector<PlayerInfoChar>& out);
    [[nodiscard]] bool encode_playerinfo_payload(PacketWriter& writer,
                                                 const PlayerInfoMsg& message);
    [[nodiscard]] bool decode_playerinfo_payload(PacketReader& reader, PlayerInfoMsg& out);
    [[nodiscard]] std::vector<std::uint8_t> frame_s2c_playerinfo(const PlayerInfoMsg& message);
    [[nodiscard]] bool parse_s2c_playerinfo(const std::vector<std::uint8_t>& frame,
                                            PlayerInfoMsg& out);

}  // namespace openkal::protocol
