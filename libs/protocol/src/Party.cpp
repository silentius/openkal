/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/protocol/Party.h"

#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketReader.h"
#include "openkal/protocol/PacketWriter.h"

#include <unordered_set>

namespace openkal::protocol {
    std::vector<std::uint8_t> frame_party_item(const PartyItemNotice& notice) {
        PacketWriter w;
        w.write_u32(notice.party);
        w.write_u32(notice.recipient);
        w.write_string(notice.recipient_name);
        w.write_i32(notice.item_index);
        w.write_i32(notice.count);
        return w.frame(static_cast<std::uint8_t>(S2C::UpdateParty));
    }

    bool parse_party_item(const std::uint8_t* data, std::size_t size, PartyItemNotice& out) {
        PacketReader r(data, size);
        PartyItemNotice next;
        if (!r.read_u32(next.party) || next.party == 0 || !r.read_u32(next.recipient) ||
            next.recipient == 0 || !r.read_string(next.recipient_name) ||
            next.recipient_name.empty() || next.recipient_name.size() > 32 ||
            !r.read_i32(next.item_index) || next.item_index <= 0 || next.item_index > 65535 ||
            !r.read_i32(next.count) || next.count <= 0 || r.remaining() != 0) {
            return false;
        }
        out = std::move(next);
        return true;
    }

    std::vector<std::uint8_t> frame_party_request(const PartyRequest& request) {
        PacketWriter w;
        w.write_u8(static_cast<std::uint8_t>(request.command));
        w.write_u32(request.target);
        w.write_u32(request.invitation);
        w.write_u8(request.value);
        w.write_string(request.name);
        return w.frame(static_cast<std::uint8_t>(C2S::AskParty));
    }
    bool parse_party_request(const std::uint8_t* data, std::size_t size, PartyRequest& out) {
        PacketReader r(data, size);
        PartyRequest next;
        std::uint8_t command = 0;
        if (!r.read_u8(command) || command > static_cast<std::uint8_t>(PartyCommand::Ignore) ||
            !r.read_u32(next.target) || !r.read_u32(next.invitation) || !r.read_u8(next.value) ||
            next.value > 1 || !r.read_string(next.name) || next.name.size() > 32 ||
            r.remaining() != 0) {
            return false;
        }
        next.command = static_cast<PartyCommand>(command);
        out = std::move(next);
        return true;
    }
    std::vector<std::uint8_t> frame_party_state(const PartyState& state) {
        if (state.members.size() > kPartyCapacity) {
            return {};
        }
        PacketWriter w;
        w.write_u32(state.id);
        w.write_u32(state.leader);
        w.write_u8(static_cast<std::uint8_t>(state.loot));
        w.write_u32(state.inviter);
        w.write_u32(state.invitation);
        w.write_string(state.inviter_name);
        w.write_u8(static_cast<std::uint8_t>(state.members.size()));
        for (const auto& m : state.members) {
            w.write_u32(m.id);
            w.write_string(m.name);
            w.write_i32(m.level);
            w.write_i32(m.hp);
            w.write_i32(m.max_hp);
            w.write_i32(m.map);
        }
        return w.frame(static_cast<std::uint8_t>(S2C::PartyInfo));
    }
    bool parse_party_state(const std::uint8_t* data, std::size_t size, PartyState& out) {
        PacketReader r(data, size);
        PartyState next;
        std::uint8_t loot = 0, count = 0;
        if (!r.read_u32(next.id) || !r.read_u32(next.leader) || !r.read_u8(loot) || loot > 1 ||
            !r.read_u32(next.inviter) || !r.read_u32(next.invitation) ||
            !r.read_string(next.inviter_name) || next.inviter_name.size() > 32 ||
            !r.read_u8(count) || count > kPartyCapacity) {
            return false;
        }
        next.loot = static_cast<PartyLoot>(loot);
        std::unordered_set<std::uint32_t> ids;
        for (unsigned i = 0; i < count; ++i) {
            PartyMember m;
            if (!r.read_u32(m.id) || m.id == 0 || !ids.insert(m.id).second ||
                !r.read_string(m.name) || m.name.size() > 32 || !r.read_i32(m.level) ||
                !r.read_i32(m.hp) || !r.read_i32(m.max_hp) || !r.read_i32(m.map)) {
                return false;
            }
            next.members.push_back(std::move(m));
        }
        if (r.remaining() != 0 || (next.id == 0 ? count != 0 || next.leader != 0
                                                : count < 2 || ids.count(next.leader) == 0)) {
            return false;
        }
        out = std::move(next);
        return true;
    }
}  // namespace openkal::protocol
