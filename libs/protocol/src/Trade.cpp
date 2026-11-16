/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/protocol/Trade.h"

#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketReader.h"
#include "openkal/protocol/PacketWriter.h"

#include <unordered_set>

namespace openkal::protocol {
    bool is_trade_packet(std::uint8_t type) {
        return type == static_cast<std::uint8_t>(C2S::AskTrade);
    }
    std::vector<std::uint8_t> frame_trade_request(const TradeRequest& r) {
        if (r.items.size() > kTradeItems || r.money < 0) {
            return {};
        }
        PacketWriter w;
        w.write_u8(static_cast<std::uint8_t>(r.command));
        w.write_u32(r.target);
        w.write_u32(r.token);
        w.write_u32(r.revision);
        w.write_u32(r.operation);
        w.write_i32(r.money);
        w.write_u8(static_cast<std::uint8_t>(r.items.size()));
        for (const auto& item : r.items) {
            w.write_u32(item.iid);
            w.write_i32(item.count);
            w.write_u8(item.slot);
        }
        return w.frame(static_cast<std::uint8_t>(C2S::AskTrade));
    }
    bool parse_trade_request(const std::uint8_t* data, std::size_t size, TradeRequest& out) {
        PacketReader r(data, size);
        TradeRequest v;
        std::uint8_t cmd = 0, n = 0;
        if (!r.read_u8(cmd) || cmd > static_cast<unsigned>(TradeCommand::Inventory) ||
            !r.read_u32(v.target) || !r.read_u32(v.token) || !r.read_u32(v.revision) ||
            !r.read_u32(v.operation) || !r.read_i32(v.money) || v.money < 0 || !r.read_u8(n) ||
            n > kTradeItems) {
            return false;
        }
        v.command = static_cast<TradeCommand>(cmd);
        std::unordered_set<std::uint32_t> ids;
        std::unordered_set<unsigned> slots;
        for (unsigned i = 0; i < n; ++i) {
            TradeAmount item;
            if (!r.read_u32(item.iid) || !item.iid || !ids.insert(item.iid).second ||
                !r.read_i32(item.count) || item.count <= 0 || !r.read_u8(item.slot) ||
                item.slot >= kTradeItems || !slots.insert(item.slot).second) {
                return false;
            }
            v.items.push_back(item);
        }
        if (r.remaining()) {
            return false;
        }
        out = std::move(v);
        return true;
    }
    std::vector<std::uint8_t> frame_trade_state(const TradeState& s) {
        PacketWriter w;
        w.write_u8(static_cast<std::uint8_t>(s.phase));
        w.write_u32(s.token);
        w.write_u32(s.revision);
        w.write_u32(s.acknowledged);
        w.write_u16(s.inventory_slots);
        w.write_string(s.partner);
        w.write_string(s.status);
        w.write_u8(s.inviter);
        w.write_u8(s.confirmed);
        w.write_u8(s.partner_confirmed);
        w.write_i32(s.money);
        w.write_i32(s.partner_money);
        for (const auto* list : {&s.own, &s.other, &s.available}) {
            const bool inventory = list == &s.available;
            if (list->size() > (inventory ? kTradeInventorySlots : kTradeItems)) {
                return {};
            }
            w.write_u16(static_cast<std::uint16_t>(list->size()));
            for (const auto& item : *list) {
                w.write_u32(item.iid);
                w.write_u16(static_cast<std::uint16_t>(item.index));
                w.write_i32(item.count);
                w.write_u16(item.slot);
                w.write_u8(item.flags);
                if (inventory) {
                    w.write_i32(item.attribute(TradeAttribute::Prefix));
                    w.write_i32(item.attribute(TradeAttribute::AnimalLevel));
                } else {
                    for (auto value : item.attributes) {
                        w.write_i32(value);
                    }
                    w.write_u64(item.pet_expire_at);
                    w.write_u64(item.ride_expire_at);
                }
            }
        }
        return w.frame(static_cast<std::uint8_t>(S2C::OpenTrade));
    }
    bool parse_trade_state(const std::uint8_t* data, std::size_t size, TradeState& out) {
        PacketReader r(data, size);
        TradeState s;
        std::uint8_t phase = 0, a = 0, b = 0, c = 0;
        if (!r.read_u8(phase) || phase > 3 || !r.read_u32(s.token) || !r.read_u32(s.revision) ||
            !r.read_u32(s.acknowledged) || !r.read_u16(s.inventory_slots) ||
            s.inventory_slots > kTradeInventorySlots || !r.read_string(s.partner) ||
            s.partner.size() > 64 || !r.read_string(s.status) || s.status.size() > 256 ||
            !r.read_u8(a) || a > 1 || !r.read_u8(b) || b > 1 || !r.read_u8(c) || c > 1 ||
            !r.read_i32(s.money) || s.money < 0 || !r.read_i32(s.partner_money) ||
            s.partner_money < 0) {
            return false;
        }
        s.phase = static_cast<TradePhase>(phase);
        s.inviter = a;
        s.confirmed = b;
        s.partner_confirmed = c;
        if (s.locked() && (!s.token || !s.revision)) {
            return false;
        }
        for (auto* list : {&s.own, &s.other, &s.available}) {
            const bool inventory = list == &s.available;
            std::uint16_t n = 0;
            std::unordered_set<std::uint32_t> ids;
            std::unordered_set<unsigned> slots;
            if (!r.read_u16(n) || n > (inventory ? kTradeInventorySlots : kTradeItems)) {
                return false;
            }
            for (unsigned i = 0; i < n; ++i) {
                TradeItem item;
                std::uint16_t index = 0;
                if (!r.read_u32(item.iid) || !item.iid || !ids.insert(item.iid).second ||
                    !r.read_u16(index) || !index || !r.read_i32(item.count) || item.count <= 0 ||
                    !r.read_u16(item.slot) ||
                    item.slot >= (inventory ? s.inventory_slots : kTradeItems) ||
                    !slots.insert(item.slot).second || !r.read_u8(item.flags) || item.flags > 15) {
                    return false;
                }
                item.index = index;
                if (inventory) {
                    if (!r.read_i32(
                            item.attributes[static_cast<std::size_t>(TradeAttribute::Prefix)]) ||
                        !r.read_i32(item.attributes[static_cast<std::size_t>(
                            TradeAttribute::AnimalLevel)])) {
                        return false;
                    }
                } else {
                    for (auto& value : item.attributes) {
                        if (!r.read_i32(value)) {
                            return false;
                        }
                    }
                    if (!r.read_u64(item.pet_expire_at) || !r.read_u64(item.ride_expire_at)) {
                        return false;
                    }
                }
                list->push_back(item);
            }
        }
        if (r.remaining()) {
            return false;
        }
        out = std::move(s);
        return true;
    }
}  // namespace openkal::protocol
