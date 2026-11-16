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

    /** C2S_UPGRADEITEM type byte (UPGRADEITEM_TYPE). */
    enum class UpgradeItemType : std::uint8_t {
        Destroy = 0,
        RateUp = 1,
        Level = 2,
    };

    /** Encodes and decodes C2S_UPGRADEITEM "bd" (type, iid). */
    class UpgradeItem final {
      public:
        UpgradeItem() = delete;

        /**
         * Builds C2S_UPGRADEITEM.
         * @param type Upgrade action.
         * @param iid Weapon instance id.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_c2s(UpgradeItemType type,
                                                                 std::uint32_t iid);

        /**
         * Parses C2S_UPGRADEITEM "bd".
         * @param data Payload bytes.
         * @param size Payload length.
         * @param type Out upgrade action.
         * @param iid Out weapon instance id.
         * @return True when the payload is valid.
         */
        [[nodiscard]] static bool parse_c2s(const std::uint8_t* data,
                                            std::size_t size,
                                            UpgradeItemType& type,
                                            std::uint32_t& iid);
    };

}  // namespace openkal::protocol
