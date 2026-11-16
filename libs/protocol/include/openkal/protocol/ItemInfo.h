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

    /** Thin S2C_UPDATEITEMINFO fields for transform / animal EXP. */
    struct UpdateItemInfoFields {
        /** Inventory instance id. */
        std::uint32_t iid = 0;

        /** item definition index. */
        std::uint16_t index = 0;

        /** Item / animal level. */
        std::uint8_t level = 1;

        /** Current EXP toward the next level. */
        std::uint32_t exp = 0;

        /** EXP added this update (0 when not a timer pulse). */
        std::uint16_t added = 0;
    };

    /** Encodes and decodes thin S2C_UPDATEITEMINFO payloads. */
    class ItemInfo final {
      public:
        ItemInfo() = delete;

        /**
         * Builds S2C_UPDATEITEMINFO "dwbdw" (iid, index, level, exp, added).
         * @param fields Values to encode.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t>
        frame_update(const UpdateItemInfoFields& fields);

        /**
         * Parses S2C_UPDATEITEMINFO "dwbdw".
         * @param data Payload bytes.
         * @param size Payload length.
         * @param out Decoded fields.
         * @return True when the payload is valid.
         */
        [[nodiscard]] static bool
        parse_update(const std::uint8_t* data, std::size_t size, UpdateItemInfoFields& out);
    };

    /**
     * Builds S2C_UPDATEITEMINFO (free wrapper for ItemInfo::frame_update).
     * @param fields Values to encode.
     * @return Framed packet bytes.
     */
    [[nodiscard]] std::vector<std::uint8_t>
    frame_s2c_update_item_info(const UpdateItemInfoFields& fields);

    /**
     * Parses S2C_UPDATEITEMINFO (free wrapper for ItemInfo::parse_update).
     * @param data Payload bytes.
     * @param size Payload length.
     * @param out Decoded fields.
     * @return True when the payload is valid.
     */
    [[nodiscard]] bool parse_s2c_update_item_info(const std::uint8_t* data,
                                                  std::size_t size,
                                                  UpdateItemInfoFields& out);

}  // namespace openkal::protocol
