/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/protocol/ItemWear.h"

#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketReader.h"
#include "openkal/protocol/PacketWriter.h"
namespace openkal::protocol {
    namespace {
        std::vector<std::uint8_t> frame_client(C2S opcode, std::uint32_t iid) {
            PacketWriter writer;
            writer.write_u32(iid);
            return writer.frame(static_cast<std::uint8_t>(opcode));
        }
        std::vector<std::uint8_t> frame_server(S2C opcode,
                                               std::uint32_t player_id,
                                               std::uint32_t iid,
                                               std::uint16_t item_index) {
            PacketWriter writer;
            writer.write_u32(player_id);
            writer.write_u32(iid);
            writer.write_u16(item_index);
            return writer.frame(static_cast<std::uint8_t>(opcode));
        }
    }  // namespace
    std::vector<std::uint8_t> ItemWear::frame_client_put_on(std::uint32_t iid) {
        return frame_client(C2S::PutOnItem, iid);
    }
    std::vector<std::uint8_t> ItemWear::frame_client_put_off(std::uint32_t iid) {
        return frame_client(C2S::PutOffItem, iid);
    }
    bool
    ItemWear::parse_client_item_id(const std::uint8_t* data, std::size_t size, std::uint32_t& iid) {
        if (!data || size < 4) {
            return false;
        }
        PacketReader reader(data, size);
        return reader.read_u32(iid);
    }
    std::vector<std::uint8_t> ItemWear::frame_server_put_on(std::uint32_t p,
                                                            std::uint32_t i,
                                                            std::uint16_t x,
                                                            std::uint64_t expire_at) {
        PacketWriter writer;
        writer.write_u32(p);
        writer.write_u32(i);
        writer.write_u16(x);
        if (expire_at != 0) {
            writer.write_u64(expire_at);
        }
        return writer.frame(static_cast<std::uint8_t>(S2C::PutOnItem));
    }
    std::vector<std::uint8_t>
    ItemWear::frame_server_put_off(std::uint32_t p, std::uint32_t i, std::uint16_t x) {
        return frame_server(S2C::PutOffItem, p, i, x);
    }
    bool ItemWear::parse_server_item(const std::uint8_t* data,
                                     std::size_t size,
                                     std::uint32_t& player_id,
                                     std::uint32_t& iid,
                                     std::uint16_t& item_index,
                                     std::uint64_t* expire_at) {
        if (!data || size < 10) {
            return false;
        }
        PacketReader reader(data, size);
        if (!(reader.read_u32(player_id) && reader.read_u32(iid) && reader.read_u16(item_index) &&
              reader.ok())) {
            return false;
        }
        if (expire_at) {
            *expire_at = 0;
            if (reader.remaining() >= 8 && !reader.read_u64(*expire_at)) {
                return false;
            }
        }
        return true;
    }
    std::vector<std::uint8_t> frame_c2s_puton_item(std::uint32_t i) {
        return ItemWear::frame_client_put_on(i);
    }
    std::vector<std::uint8_t> frame_c2s_putoff_item(std::uint32_t i) {
        return ItemWear::frame_client_put_off(i);
    }
    bool parse_c2s_item_id(const std::uint8_t* d, std::size_t s, std::uint32_t& i) {
        return ItemWear::parse_client_item_id(d, s, i);
    }
    std::vector<std::uint8_t>
    frame_s2c_puton_item(std::uint32_t p, std::uint32_t i, std::uint16_t x) {
        return ItemWear::frame_server_put_on(p, i, x);
    }
    std::vector<std::uint8_t>
    frame_s2c_putoff_item(std::uint32_t p, std::uint32_t i, std::uint16_t x) {
        return ItemWear::frame_server_put_off(p, i, x);
    }
    bool parse_s2c_item_wear(const std::uint8_t* data,
                             std::size_t size,
                             std::uint32_t& player_id,
                             std::uint32_t& iid,
                             std::uint16_t& item_index,
                             std::uint64_t* expire_at) {
        return ItemWear::parse_server_item(data, size, player_id, iid, item_index, expire_at);
    }
}  // namespace openkal::protocol
