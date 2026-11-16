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

    /** Encodes and decodes C2S_DROPITEM "dd" (iid, count). */
    class DropItem final {
      public:
        DropItem() = delete;

        /**
         * Builds C2S_DROPITEM.
         * @param iid Inventory instance id.
         * @param count Stack count to drop.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_c2s(std::uint32_t iid,
                                                                 std::uint32_t count);

        /**
         * Parses C2S_DROPITEM "dd".
         * @param data Payload bytes.
         * @param size Payload length.
         * @param iid Out inventory instance id.
         * @param count Out stack count.
         * @return True when both fields were read.
         */
        [[nodiscard]] static bool parse_c2s(const std::uint8_t* data,
                                            std::size_t size,
                                            std::uint32_t& iid,
                                            std::uint32_t& count);
    };

}  // namespace openkal::protocol
