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

    /** Cooking mode used by the Speed-up Medicine chemist. */
    inline constexpr std::uint8_t kCookingPotions = 1;

    /** Quantities requested from the three potion recipes. */
    struct CookingOrder {
        std::uint16_t blood_fish = 0;
        std::uint16_t medicine_from_blood_fish = 0;
        std::uint16_t medicine_direct = 0;
    };

    /** Encodes and decodes C2S_COOKING. */
    class Cooking final {
      public:
        Cooking() = delete;

        /** Builds the `bwww` cooking request.
         * @param type Cooking mode; Speed-up Medicine uses kCookingPotions.
         * @param order Requested recipe quantities.
         * @return Framed packet.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_c2s(std::uint8_t type,
                                                                 const CookingOrder& order);

        /** Parses the `bwww` cooking request.
         * @param data Packet payload.
         * @param size Payload byte count.
         * @param type Decoded cooking mode.
         * @param order Decoded recipe quantities.
         * @return True when the complete payload is valid.
         */
        [[nodiscard]] static bool parse_c2s(const std::uint8_t* data,
                                            std::size_t size,
                                            std::uint8_t& type,
                                            CookingOrder& order);
    };

}  // namespace openkal::protocol
