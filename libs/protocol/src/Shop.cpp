/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "openkal/protocol/Shop.h"

#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketReader.h"
#include "openkal/protocol/PacketWriter.h"

#include <limits>

namespace openkal::protocol {

    std::vector<std::uint8_t> Shop::frame_item_list_c2s(std::uint32_t npc_id) {
        PacketWriter writer;
        writer.write_u32(npc_id);
        return writer.frame(static_cast<std::uint8_t>(C2S::ItemList));
    }

    bool Shop::parse_item_list_c2s(const std::uint8_t* data,
                                   std::size_t size,
                                   std::uint32_t& out_npc_id) {
        PacketReader reader(data, size);
        return data && size == 4 && reader.read_u32(out_npc_id);
    }

    std::vector<std::uint8_t> Shop::frame_item_list_s2c(const ShopItemList& list) {
        if (list.offers.size() > std::numeric_limits<std::uint16_t>::max()) {
            return {};
        }
        PacketWriter writer;
        writer.write_u32(list.npc_id);
        writer.write_u16(static_cast<std::uint16_t>(list.offers.size()));
        for (const ShopOffer& offer : list.offers) {
            writer.write_u16(offer.index);
            writer.write_u32(offer.buy);
        }
        if (list.currency_index != 0) {
            writer.write_u16(list.currency_index);
        }
        return writer.frame(static_cast<std::uint8_t>(S2C::ItemList));
    }

    bool Shop::parse_item_list_s2c(const std::uint8_t* data, std::size_t size, ShopItemList& out) {
        if (!data || size < 6) {
            return false;
        }
        PacketReader reader(data, size);
        std::uint16_t count = 0;
        if (!reader.read_u32(out.npc_id) || !reader.read_u16(count) ||
            (size != 6u + count * 6u && size != 8u + count * 6u)) {
            return false;
        }
        out.currency_index = 0;
        out.offers.clear();
        out.offers.reserve(count);
        for (std::uint16_t i = 0; i < count; ++i) {
            ShopOffer offer;
            if (!reader.read_u16(offer.index) || !reader.read_u32(offer.buy)) {
                out.offers.clear();
                return false;
            }
            out.offers.push_back(offer);
        }
        if (size == 8u + count * 6u &&
            (!reader.read_u16(out.currency_index) || out.currency_index == 0)) {
            return false;
        }
        return true;
    }

    std::vector<std::uint8_t>
    Shop::frame_buy_c2s(std::uint32_t npc_id, std::uint16_t index, std::uint16_t quantity) {
        PacketWriter writer;
        writer.write_u32(npc_id);
        writer.write_u16(index);
        writer.write_u16(quantity);
        return writer.frame(static_cast<std::uint8_t>(C2S::BuyItem));
    }

    bool Shop::parse_buy_c2s(const std::uint8_t* data,
                             std::size_t size,
                             std::uint32_t& out_npc_id,
                             std::uint16_t& out_index,
                             std::uint16_t& out_quantity) {
        PacketReader reader(data, size);
        return data && size == 8 && reader.read_u32(out_npc_id) && reader.read_u16(out_index) &&
               reader.read_u16(out_quantity);
    }

    std::vector<std::uint8_t>
    Shop::frame_sell_c2s(std::uint32_t npc_id, std::uint32_t iid, std::uint16_t quantity) {
        PacketWriter writer;
        writer.write_u32(npc_id);
        writer.write_u32(iid);
        writer.write_u16(quantity);
        return writer.frame(static_cast<std::uint8_t>(C2S::SellItem));
    }

    bool Shop::parse_sell_c2s(const std::uint8_t* data,
                              std::size_t size,
                              std::uint32_t& out_npc_id,
                              std::uint32_t& out_iid,
                              std::uint16_t& out_quantity) {
        PacketReader reader(data, size);
        return data && size == 10 && reader.read_u32(out_npc_id) && reader.read_u32(out_iid) &&
               reader.read_u16(out_quantity);
    }

}  // namespace openkal::protocol
