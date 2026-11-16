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

    /**
     * Encodes and decodes C2S_MIXING (weapon + materials).
     *
     * Request payload scheme:
     *   "bddddd" == mode(u8), weapon_iid, extra_weapon_iid, armor_iid, stone1_iid, stone2_iid.
     * Legacy 17-byte payloads omit extra_weapon_iid (treated as 0).
     * - mode 0: mix upgrade (weapon + extra weapon + armor + 1 stone;
     *   G3 Mystery/Blood sends stone2).
     * - mode 1: making (weapon + extra weapon + armor + 2 stones).
     */
    class MixingItem final {
      public:
        MixingItem() = delete;

        /**
         * Mix Master stones: drop indices 895–905 and cash copies 1166/1167.
         * They never stack; C2S_MIXING addresses each stone by instance iid.
         * @param index item definition index.
         * @return True when the index is a mixing stone.
         */
        [[nodiscard]] static bool is_mixing_stone(int index) {
            return (index >= 895 && index <= 905) || index == 1166 || index == 1167;
        }

        /**
         * Builds C2S_MIXING request frame.
         * @param mode Mixing kind (0 upgrade, 1 making).
         * @param weapon_iid Bag iid of the center weapon instance.
         * @param extra_weapon_iid Bag iid of the extra weapon.
         * @param armor_iid Bag iid of the material armor/defense instance.
         * @param stone1_iid Bag iid of the first elemental stone instance.
         * @param stone2_iid Bag iid of the second stone (0 unless making or G3 mix).
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_c2s(std::uint8_t mode,
                                                                 std::uint32_t weapon_iid,
                                                                 std::uint32_t extra_weapon_iid,
                                                                 std::uint32_t armor_iid,
                                                                 std::uint32_t stone1_iid,
                                                                 std::uint32_t stone2_iid);

        /**
         * Parses C2S_MIXING request payload.
         * @param data Payload bytes.
         * @param size Payload length.
         * @param out_mode Out mode.
         * @param out_weapon_iid Out weapon iid.
         * @param out_extra_weapon_iid Out extra weapon iid (0 for legacy frames).
         * @param out_armor_iid Out armor iid.
         * @param out_stone1_iid Out stone1 iid.
         * @param out_stone2_iid Out stone2 iid.
         * @return True when all fields were decoded.
         */
        [[nodiscard]] static bool parse_c2s(const std::uint8_t* data,
                                            std::size_t size,
                                            std::uint8_t& out_mode,
                                            std::uint32_t& out_weapon_iid,
                                            std::uint32_t& out_extra_weapon_iid,
                                            std::uint32_t& out_armor_iid,
                                            std::uint32_t& out_stone1_iid,
                                            std::uint32_t& out_stone2_iid);
    };

}  // namespace openkal::protocol
