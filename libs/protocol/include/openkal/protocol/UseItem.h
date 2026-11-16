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

    /** Encodes C2S_USEITEM with an optional OpenKal Auto-HP threshold byte. */
    class UseItem final {
      public:
        UseItem() = delete;

        /**
         * Builds C2S_USEITEM.
         * @param iid Inventory instance id.
         * @param auto_hp_percent Auto-HP threshold (25, 50, 75, 100), or 0 for manual use.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_c2s(std::uint32_t iid,
                                                                 std::uint8_t auto_hp_percent = 0);

        /**
         * Parses C2S_USEITEM "d".
         * @param data Payload bytes.
         * @param size Payload length.
         * @param iid Out inventory instance id.
         * @param auto_hp_percent Optional output threshold; 0 for a manual request.
         * @return True when the field was read.
         */
        [[nodiscard]] static bool parse_c2s(const std::uint8_t* data,
                                            std::size_t size,
                                            std::uint32_t& iid,
                                            std::uint8_t* auto_hp_percent = nullptr);

        /** Tests the living player's HP against a supported Auto-HP threshold.
         * @param hp Current health.
         * @param max_hp Maximum health.
         * @param percent Configured percentage (25, 50, 75, 100).
         * @return True only when health is strictly below the threshold.
         */
        [[nodiscard]] static bool
        auto_hp_eligible(std::int32_t hp, std::int32_t max_hp, int percent);
    };

}  // namespace openkal::protocol
