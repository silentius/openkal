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

    /** Item-log value for deletion in the inventory trash response. */
    inline constexpr std::uint8_t kItemLogDelete = 4;

    /** Encodes and decodes C2S_TRASHITEM "d" and S2C_TRASHITEM "db". */
    class TrashItem final {
      public:
        TrashItem() = delete;

        /**
         * Builds C2S_TRASHITEM.
         * @param iid Inventory instance id.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_c2s(std::uint32_t iid);

        /**
         * Parses C2S_TRASHITEM "d".
         * @param data Payload bytes.
         * @param size Payload length.
         * @param iid Out inventory instance id.
         * @return True when the iid was read.
         */
        [[nodiscard]] static bool
        parse_c2s(const std::uint8_t* data, std::size_t size, std::uint32_t& iid);

        /**
         * Builds S2C_TRASHITEM.
         * @param iid Inventory instance id.
         * @param log_type TYPE_LOG byte (TL_DELETE for inventory trash).
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_s2c(std::uint32_t iid,
                                                                 std::uint8_t log_type);

        /**
         * Parses S2C_TRASHITEM "db" (optional trailing buyer id is ignored).
         * @param data Payload bytes.
         * @param size Payload length.
         * @param iid Out inventory instance id.
         * @param log_type Out TYPE_LOG byte.
         * @return True when iid and log type were read.
         */
        [[nodiscard]] static bool parse_s2c(const std::uint8_t* data,
                                            std::size_t size,
                                            std::uint32_t& iid,
                                            std::uint8_t& log_type);
    };

}  // namespace openkal::protocol
