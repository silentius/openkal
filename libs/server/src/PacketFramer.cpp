/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/protocol/InsertItem.h"
#include "openkal/protocol/Move.h"
#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketWriter.h"
#include "openkal/server/Packets.h"

namespace openkal::server {

    std::vector<std::uint8_t> PacketFramer::s2c_code(std::uint32_t protocol_test_value) {
        protocol::PacketWriter w;
        // Connection-code payload: test value, code, timing, flags and server metadata.
        w.write_u32(protocol_test_value);
        w.write_u8(0);   // decode code (plaintext)
        w.write_u32(0);  // time stamp
        w.write_u32(0);  // time start
        w.write_u32(0);  // system flags
        w.write_u32(0);  // event flags
        w.write_u8(1);   // server id
        w.write_u8(0);   // age limit
        w.write_u8(0);   // country
        return w.frame(static_cast<std::uint8_t>(protocol::S2C::Code));
    }

    std::vector<std::uint8_t> PacketFramer::create_monster(const MonsterEntity& m) {
        protocol::PacketWriter w;
        // "wdddwdddIsbds"
        w.write_u16(static_cast<std::uint16_t>(m.def_index));
        w.write_u32(m.id);
        w.write_u32(static_cast<std::uint32_t>(m.x));
        w.write_u32(static_cast<std::uint32_t>(m.y));
        w.write_u16(static_cast<std::uint16_t>(m.dir));
        w.write_i32(m.hp);
        w.write_i32(m.max_hp);
        w.write_u32(0);  // gstate
        w.write_u32(0);  // mstate
        w.write_u64(0);  // guild name ptr slot as I ? write empty string via 's'
                         // style: use write_string
        // PacketWriter write_format uses I as u64; guild is string in real protocol
        // ('s' after I?). Format letters: w d d d w d d d I s b d s Re-clear and
        // write carefully with write_format where possible.
        w.clear();
        w.write_u16(static_cast<std::uint16_t>(m.def_index));
        w.write_u32(m.id);
        w.write_u32(static_cast<std::uint32_t>(m.x));
        w.write_u32(static_cast<std::uint32_t>(m.y));
        w.write_u16(static_cast<std::uint16_t>(m.dir));
        w.write_i32(m.hp);
        w.write_i32(m.max_hp);
        w.write_u32(m.gstate);
        w.write_u32(0);
        w.write_u64(0);  // I ? unused guild id placeholder matching writer letter
        w.write_string("");
        w.write_u8(static_cast<std::uint8_t>(m.race));
        w.write_u32(0);  // gid
        w.write_string(m.name);
        return w.frame(static_cast<std::uint8_t>(protocol::S2C::CreateMonster));
    }

    std::vector<std::uint8_t>
    PacketFramer::move_monster_on(std::uint32_t id, std::int8_t dx, std::int8_t dy, bool run) {
        protocol::PacketWriter w;
        w.write_u32(id);
        w.write_u8(static_cast<std::uint8_t>(dx));
        w.write_u8(static_cast<std::uint8_t>(dy));
        w.write_u8(run ? 1 : 0);
        return w.frame(static_cast<std::uint8_t>(protocol::S2C::MoveMonsterOn));
    }

    std::vector<std::uint8_t>
    PacketFramer::move_monster_end(std::uint32_t id, std::int8_t dx, std::int8_t dy, bool run) {
        protocol::PacketWriter w;
        w.write_u32(id);
        w.write_u8(static_cast<std::uint8_t>(dx));
        w.write_u8(static_cast<std::uint8_t>(dy));
        w.write_u8(run ? 1 : 0);
        return w.frame(static_cast<std::uint8_t>(protocol::S2C::MoveMonsterEnd));
    }

    std::vector<std::uint8_t> PacketFramer::remove_monster(std::uint32_t id) {
        protocol::PacketWriter w;
        w.write_u32(id);
        return w.frame(static_cast<std::uint8_t>(protocol::S2C::RemoveMonster));
    }

    std::vector<std::uint8_t> PacketFramer::create_item(const GroundItem& item) {
        protocol::PacketWriter w;
        // "wdddd" — index, id, x, y, num
        w.write_u16(static_cast<std::uint16_t>(item.item_index));
        w.write_u32(item.id);
        w.write_u32(static_cast<std::uint32_t>(item.x));
        w.write_u32(static_cast<std::uint32_t>(item.y));
        w.write_u32(static_cast<std::uint32_t>(item.count));
        return w.frame(static_cast<std::uint8_t>(protocol::S2C::CreateItem));
    }

    std::vector<std::uint8_t> PacketFramer::remove_item(std::uint32_t id) {
        protocol::PacketWriter w;
        w.write_u32(id);
        return w.frame(static_cast<std::uint8_t>(protocol::S2C::RemoveItem));
    }

    std::vector<std::uint8_t> PacketFramer::insert_item(std::uint32_t iid,
                                                        std::uint16_t item_index,
                                                        std::uint8_t prefix,
                                                        std::uint32_t info,
                                                        std::int32_t num,
                                                        const protocol::InsertCharmFields& charm,
                                                        std::uint64_t expire_at) {
        protocol::PacketWriter w;
        // Simplified compatibility behavior plus OpenKal charm and authoritative-expiry extensions.
        w.write_u32(iid);
        w.write_u16(item_index);
        w.write_u8(prefix);
        w.write_u32(info);
        w.write_u32(static_cast<std::uint32_t>(num));
        std::uint8_t charm_bytes[protocol::kInsertCharmBytes];
        protocol::pack_insert_charm(charm_bytes, charm);
        w.write_bytes(charm_bytes, protocol::kInsertCharmBytes);
        w.write_u64(expire_at);
        return w.frame(static_cast<std::uint8_t>(protocol::S2C::InsertItem));
    }

    std::vector<std::uint8_t> PacketFramer::message(std::uint8_t msg_id) {
        protocol::PacketWriter w;
        w.write_u8(msg_id);
        return w.frame(static_cast<std::uint8_t>(protocol::S2C::Message));
    }

    std::vector<std::uint8_t>
    PacketFramer::move_player_on(std::uint32_t id, std::int8_t dx, std::int8_t dy, std::int8_t dz) {
        return protocol::Move::frame_player_on(id, dx, dy, dz);
    }

    std::vector<std::uint8_t> PacketFramer::move_player_end(std::uint32_t id,
                                                            std::int8_t dx,
                                                            std::int8_t dy,
                                                            std::int8_t dz) {
        return protocol::Move::frame_player_end(id, dx, dy, dz);
    }

    std::vector<std::uint8_t>
    PacketFramer::move_before(std::uint32_t id, int x, int y, int z, std::uint16_t dir) {
        return protocol::Move::frame_before(id, x, y, z, dir);
    }

    std::vector<std::uint8_t> frame_s2c_code(std::uint32_t protocol_test_value) {
        return PacketFramer::s2c_code(protocol_test_value);
    }

    std::vector<std::uint8_t> frame_create_monster(const MonsterEntity& monster) {
        return PacketFramer::create_monster(monster);
    }

    std::vector<std::uint8_t>
    frame_move_monster_on(std::uint32_t id, std::int8_t dx, std::int8_t dy, bool run) {
        return PacketFramer::move_monster_on(id, dx, dy, run);
    }

    std::vector<std::uint8_t>
    frame_move_monster_end(std::uint32_t id, std::int8_t dx, std::int8_t dy, bool run) {
        return PacketFramer::move_monster_end(id, dx, dy, run);
    }

    std::vector<std::uint8_t> frame_remove_monster(std::uint32_t id) {
        return PacketFramer::remove_monster(id);
    }

    std::vector<std::uint8_t> frame_create_item(const GroundItem& item) {
        return PacketFramer::create_item(item);
    }

    std::vector<std::uint8_t> frame_remove_item(std::uint32_t id) {
        return PacketFramer::remove_item(id);
    }

    std::vector<std::uint8_t> frame_insert_item(std::uint32_t iid,
                                                std::uint16_t item_index,
                                                std::uint8_t prefix,
                                                std::uint32_t info,
                                                std::int32_t num,
                                                const protocol::InsertCharmFields& charm,
                                                std::uint64_t expire_at) {
        return PacketFramer::insert_item(iid, item_index, prefix, info, num, charm, expire_at);
    }

    std::vector<std::uint8_t> frame_message(std::uint8_t msg_id) {
        return PacketFramer::message(msg_id);
    }

    std::vector<std::uint8_t>
    frame_move_player_on(std::uint32_t id, std::int8_t dx, std::int8_t dy, std::int8_t dz) {
        return PacketFramer::move_player_on(id, dx, dy, dz);
    }

    std::vector<std::uint8_t>
    frame_move_player_end(std::uint32_t id, std::int8_t dx, std::int8_t dy, std::int8_t dz) {
        return PacketFramer::move_player_end(id, dx, dy, dz);
    }

    std::vector<std::uint8_t>
    frame_move_before(std::uint32_t id, int x, int y, int z, std::uint16_t dir) {
        return PacketFramer::move_before(id, x, y, z, dir);
    }

}  // namespace openkal::server
