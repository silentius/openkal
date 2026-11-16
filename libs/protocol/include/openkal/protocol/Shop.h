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

    /** Maximum item count in a single NPC shop purchase. */
    inline constexpr std::uint16_t kMaxShopBuyQuantity = 10000;

    /** One server-authoritative NPC shop offer. */
    struct ShopOffer {
        /** item definition index. */
        std::uint16_t index = 0;
        /** Unit purchase price. */
        std::uint32_t buy = 0;
    };

    /** Item list returned for an NPC shop. */
    struct ShopItemList {
        /** Runtime NPC id used for later transactions. */
        std::uint32_t npc_id = 0;
        /** Items currently sold by the NPC. */
        std::vector<ShopOffer> offers;
        /** Currency item definition, or zero for Zamogeon. */
        std::uint16_t currency_index = 0;
    };

    /** Encodes and decodes NPC shop packets. */
    class Shop final {
      public:
        Shop() = delete;

        /** Builds C2S_ITEMLIST. @param npc_id Runtime NPC id. @return Framed packet. */
        [[nodiscard]] static std::vector<std::uint8_t> frame_item_list_c2s(std::uint32_t npc_id);

        /** Parses C2S_ITEMLIST. @param data Payload. @param size Payload size. @param out_npc_id
         * Decoded NPC id. @return True on success. */
        [[nodiscard]] static bool
        parse_item_list_c2s(const std::uint8_t* data, std::size_t size, std::uint32_t& out_npc_id);

        /** Builds S2C_ITEMLIST. @param list Shop offers. @return Framed packet. */
        [[nodiscard]] static std::vector<std::uint8_t>
        frame_item_list_s2c(const ShopItemList& list);

        /** Parses S2C_ITEMLIST. @param data Payload. @param size Payload size. @param out Decoded
         * list. @return True on success. */
        [[nodiscard]] static bool
        parse_item_list_s2c(const std::uint8_t* data, std::size_t size, ShopItemList& out);

        /** Builds C2S_BUYITEM. @param npc_id Runtime NPC id. @param index item definition index.
         * @param quantity Count. @return Framed packet. */
        [[nodiscard]] static std::vector<std::uint8_t>
        frame_buy_c2s(std::uint32_t npc_id, std::uint16_t index, std::uint16_t quantity);

        /** Parses C2S_BUYITEM. @param data Payload. @param size Payload size. @param out_npc_id NPC
         * id. @param out_index Item index. @param out_quantity Count. @return True on success. */
        [[nodiscard]] static bool parse_buy_c2s(const std::uint8_t* data,
                                                std::size_t size,
                                                std::uint32_t& out_npc_id,
                                                std::uint16_t& out_index,
                                                std::uint16_t& out_quantity);

        /** Builds C2S_SELLITEM. @param npc_id Runtime NPC id. @param iid Bag instance id. @param
         * quantity Count. @return Framed packet. */
        [[nodiscard]] static std::vector<std::uint8_t>
        frame_sell_c2s(std::uint32_t npc_id, std::uint32_t iid, std::uint16_t quantity);

        /** Parses C2S_SELLITEM. @param data Payload. @param size Payload size. @param out_npc_id
         * NPC id. @param out_iid Bag iid. @param out_quantity Count. @return True on success. */
        [[nodiscard]] static bool parse_sell_c2s(const std::uint8_t* data,
                                                 std::size_t size,
                                                 std::uint32_t& out_npc_id,
                                                 std::uint32_t& out_iid,
                                                 std::uint16_t& out_quantity);
    };

}  // namespace openkal::protocol
