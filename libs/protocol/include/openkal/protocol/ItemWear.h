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

    /** Encodes and decodes item wear messages. */
    class ItemWear final {
      public:
        ItemWear() = delete;

        /**
         * Builds C2S_PUTONITEM.
         * @param iid Inventory instance id to equip.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_client_put_on(std::uint32_t iid);

        /**
         * Builds C2S_PUTOFFITEM.
         * @param iid Inventory instance id to unequip.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_client_put_off(std::uint32_t iid);

        /**
         * Parses a C2S put-on/put-off payload (single item id).
         * @param data Payload bytes.
         * @param size Payload length.
         * @param iid Out inventory instance id.
         * @return True when the payload is valid.
         */
        [[nodiscard]] static bool
        parse_client_item_id(const std::uint8_t* data, std::size_t size, std::uint32_t& iid);

        /**
         * Builds S2C_PUTONITEM.
         * @param player_id Player entity id.
         * @param iid Inventory instance id.
         * @param item_index item definition index worn.
         * @param expire_at Optional Unix expiry for a timed pet or riding item.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t>
        frame_server_put_on(std::uint32_t player_id,
                            std::uint32_t iid,
                            std::uint16_t item_index,
                            std::uint64_t expire_at = 0);

        /**
         * Builds S2C_PUTOFFITEM.
         * @param player_id Player entity id.
         * @param iid Inventory instance id.
         * @param item_index item definition index removed.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t>
        frame_server_put_off(std::uint32_t player_id, std::uint32_t iid, std::uint16_t item_index);

        /**
         * Parses an S2C put-on/put-off payload.
         * @param data Payload bytes.
         * @param size Payload length.
         * @param player_id Out player entity id.
         * @param iid Out inventory instance id.
         * @param item_index Out item definition index.
         * @return True when the payload is valid.
         */
        [[nodiscard]] static bool parse_server_item(const std::uint8_t* data,
                                                    std::size_t size,
                                                    std::uint32_t& player_id,
                                                    std::uint32_t& iid,
                                                    std::uint16_t& item_index,
                                                    std::uint64_t* expire_at = nullptr);
    };

    /**
     * Builds C2S_PUTONITEM (free wrapper for ItemWear::frame_client_put_on).
     * @param iid Inventory instance id to equip.
     * @return Framed packet bytes.
     */
    [[nodiscard]] std::vector<std::uint8_t> frame_c2s_puton_item(std::uint32_t iid);

    /**
     * Builds C2S_PUTOFFITEM (free wrapper for ItemWear::frame_client_put_off).
     * @param iid Inventory instance id to unequip.
     * @return Framed packet bytes.
     */
    [[nodiscard]] std::vector<std::uint8_t> frame_c2s_putoff_item(std::uint32_t iid);

    /**
     * Parses a C2S put-on/put-off item id (free wrapper for
     * ItemWear::parse_client_item_id).
     * @param data Payload bytes.
     * @param size Payload length.
     * @param iid Out inventory instance id.
     * @return True when the payload is valid.
     */
    [[nodiscard]] bool
    parse_c2s_item_id(const std::uint8_t* data, std::size_t size, std::uint32_t& iid);

    /**
     * Builds S2C_PUTONITEM (free wrapper for ItemWear::frame_server_put_on).
     * @param player_id Player entity id.
     * @param iid Inventory instance id.
     * @param item_index item definition index worn.
     * @return Framed packet bytes.
     */
    [[nodiscard]] std::vector<std::uint8_t>
    frame_s2c_puton_item(std::uint32_t player_id, std::uint32_t iid, std::uint16_t item_index);

    /**
     * Builds S2C_PUTOFFITEM (free wrapper for ItemWear::frame_server_put_off).
     * @param player_id Player entity id.
     * @param iid Inventory instance id.
     * @param item_index item definition index removed.
     * @return Framed packet bytes.
     */
    [[nodiscard]] std::vector<std::uint8_t>
    frame_s2c_putoff_item(std::uint32_t player_id, std::uint32_t iid, std::uint16_t item_index);

    /** Parses an S2C put-on/put-off item payload. */
    [[nodiscard]] bool parse_s2c_item_wear(const std::uint8_t* data,
                                           std::size_t size,
                                           std::uint32_t& player_id,
                                           std::uint32_t& iid,
                                           std::uint16_t& item_index,
                                           std::uint64_t* expire_at = nullptr);

}  // namespace openkal::protocol
