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

    /** Encodes and decodes C2S_ENCHANTITEM "dd" (fromIID, toIID). */
    class EnchantItem final {
      public:
        EnchantItem() = delete;

        /**
         * Builds C2S_ENCHANTITEM.
         * @param from_iid Talisman instance id (or item definition index fallback).
         * @param to_iid Destination gear instance id (or item definition index fallback).
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_c2s(std::uint32_t from_iid,
                                                                 std::uint32_t to_iid);

        /**
         * Parses C2S_ENCHANTITEM "dd".
         * @param data Payload bytes.
         * @param size Payload length.
         * @param from_iid Out talisman id.
         * @param to_iid Out destination id.
         * @return True when both ids were read.
         */
        [[nodiscard]] static bool parse_c2s(const std::uint8_t* data,
                                            std::size_t size,
                                            std::uint32_t& from_iid,
                                            std::uint32_t& to_iid);
    };

}  // namespace openkal::protocol
