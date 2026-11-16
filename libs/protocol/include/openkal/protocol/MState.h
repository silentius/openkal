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

    /** Encodes and decodes S2C_MSTATE ("dI" = entity id, mstate). */
    class MState final {
      public:
        MState() = delete;

        /**
         * Frames an S2C_MSTATE packet.
         * @param entity_id Target entity id.
         * @param mstate Combined compatibility behavior bitmask.
         * @return Framed packet.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame(std::uint32_t entity_id,
                                                             std::uint64_t mstate);

        /**
         * Parses an S2C_MSTATE payload.
         * @param data Payload bytes.
         * @param size Payload size.
         * @param entity_id Out entity id.
         * @param mstate Out compatibility behavior bitmask.
         * @return True on success.
         */
        [[nodiscard]] static bool parse(const std::uint8_t* data,
                                        std::size_t size,
                                        std::uint32_t& entity_id,
                                        std::uint64_t& mstate);
    };

}  // namespace openkal::protocol
