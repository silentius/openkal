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

#include <cstddef>
#include <cstdint>
#include <vector>

namespace openkal::protocol {

    /** Encodes and decodes the graceful game-exit handshake. */
    class GameExit final {
      public:
        GameExit() = delete;

        /** Builds the empty C2S_GAMEEXIT request.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_client_request();

        /** Parses the empty C2S_GAMEEXIT payload.
         * @param data Payload bytes; may be null for an empty payload.
         * @param size Payload length.
         * @return True when the payload is valid.
         */
        [[nodiscard]] static bool parse_client_request(const std::uint8_t* data, std::size_t size);

        /** Builds S2C_ANS_GAMEEXIT.
         * @param saved True when the authoritative session was persisted.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_server_answer(bool saved);

        /** Parses an S2C_ANS_GAMEEXIT payload.
         * @param data Payload bytes.
         * @param size Payload length.
         * @param saved Out persistence result.
         * @return True when the payload is valid.
         */
        [[nodiscard]] static bool
        parse_server_answer(const std::uint8_t* data, std::size_t size, bool& saved);
    };

}  // namespace openkal::protocol
