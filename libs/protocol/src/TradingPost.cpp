/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/protocol/TradingPost.h"

#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketReader.h"
#include "openkal/protocol/PacketWriter.h"

#include <limits>

namespace openkal::protocol {
    namespace {
        bool valid(const TradingPostPickup& pickup) {
            return pickup.geons >= 0 && pickup.geons <= kTradingPostMaxPrice &&
                   ((pickup.item_index > 0 && pickup.quantity > 0) ||
                    (pickup.item_index == 0 && pickup.quantity == 0 && pickup.geons > 0));
        }

        bool valid(const TradingPostRequest& r) {
            const auto& f = r.filter;
            return static_cast<unsigned>(r.command) <= 5 && static_cast<unsigned>(r.view) <= 3 &&
                   static_cast<unsigned>(f.order) <= 3 && f.clazz >= -1 && f.clazz <= 3 &&
                   static_cast<int>(f.type) >= -1 && static_cast<int>(f.type) <= 8 &&
                   f.prefix >= -1 && f.grade >= -1 && f.page <= 1000000 && r.operation &&
                   r.quantity > 0 && r.unit_price >= 0 && r.unit_price <= kTradingPostMaxPrice &&
                   (r.command != TradingPostCommand::Sell || (r.iid && r.unit_price)) &&
                   (r.command != TradingPostCommand::Buy || (r.listing && r.unit_price)) &&
                   (r.command != TradingPostCommand::Cancel || r.listing) &&
                   (r.command != TradingPostCommand::Collect ||
                    (r.listing && r.npc_id && r.view == TradingPostView::Collection)) &&
                   (r.command != TradingPostCommand::CollectAll ||
                    (r.npc_id && r.view == TradingPostView::Collection));
        }
    }  // namespace
    TradingPostType trading_post_type(std::string_view big, std::string_view small, bool usable) {
        if (big == "quest") {
            return TradingPostType::Quest;
        }
        if (big == "weapon") {
            return TradingPostType::Weapon;
        }
        if (big == "defense") {
            if (small == "helmet") {
                return TradingPostType::HeadGear;
            }
            if (small == "upperarmor" || small == "upper") {
                return TradingPostType::Chest;
            }
            if (small == "gauntlet") {
                return TradingPostType::Gloves;
            }
            if (small == "lowerarmor" || small == "pants") {
                return TradingPostType::Shorts;
            }
            if (small == "boots") {
                return TradingPostType::Boots;
            }
        }
        return usable ? TradingPostType::Consumables : TradingPostType::Other;
    }
    bool trading_post_prefixes(TradingPostType type) {
        return type >= TradingPostType::Weapon && type <= TradingPostType::Boots;
    }
    std::vector<std::uint8_t> frame_trading_post_request(const TradingPostRequest& r) {
        if (!valid(r)) {
            return {};
        }
        PacketWriter w;
        w.write_u8(r.npc_id || r.command == TradingPostCommand::Collect ? 2 : 1);
        w.write_u8(static_cast<std::uint8_t>(r.command));
        w.write_u8(static_cast<std::uint8_t>(r.view));
        w.write_u64(r.operation);
        w.write_i32(r.filter.clazz);
        w.write_i32(static_cast<int>(r.filter.type));
        w.write_i32(r.filter.prefix);
        w.write_i32(r.filter.grade);
        w.write_u8(static_cast<std::uint8_t>(r.filter.order));
        w.write_u32(r.filter.page);
        w.write_u64(r.listing);
        w.write_u32(r.iid);
        w.write_i32(r.quantity);
        w.write_u64(static_cast<std::uint64_t>(r.unit_price));
        if (r.npc_id || r.command == TradingPostCommand::Collect) {
            w.write_u32(r.npc_id);
        }
        return w.frame(static_cast<std::uint8_t>(C2S::TradingPost));
    }
    bool parse_trading_post_request(const std::uint8_t* data,
                                    std::size_t size,
                                    TradingPostRequest& out) {
        PacketReader p(data, size);
        TradingPostRequest r;
        std::uint8_t version = 0, command = 0, view = 0, order = 0;
        std::int32_t type = 0;
        std::uint64_t price = 0;
        if (!p.read_u8(version) || (version != 1 && version != 2) || !p.read_u8(command) ||
            !p.read_u8(view) || !p.read_u64(r.operation) || !p.read_i32(r.filter.clazz) ||
            !p.read_i32(type) || !p.read_i32(r.filter.prefix) || !p.read_i32(r.filter.grade) ||
            !p.read_u8(order) || !p.read_u32(r.filter.page) || !p.read_u64(r.listing) ||
            !p.read_u32(r.iid) || !p.read_i32(r.quantity) || !p.read_u64(price) ||
            price > kTradingPostMaxPrice || (version == 2 && !p.read_u32(r.npc_id)) ||
            p.remaining()) {
            return false;
        }
        r.command = static_cast<TradingPostCommand>(command);
        r.view = static_cast<TradingPostView>(view);
        r.filter.type = static_cast<TradingPostType>(type);
        r.filter.order = static_cast<TradingPostOrder>(order);
        r.unit_price = static_cast<std::int64_t>(price);
        if (!valid(r)) {
            return false;
        }
        out = std::move(r);
        return true;
    }
    std::vector<std::uint8_t> frame_trading_post_state(const TradingPostState& s) {
        if (s.entries.size() > kTradingPostPageSize || s.status.size() > 256 ||
            static_cast<unsigned>(s.view) > 3 || (s.total_pages && s.page >= s.total_pages)) {
            return {};
        }
        PacketWriter w;
        w.write_u8(2);
        w.write_u8(static_cast<std::uint8_t>(s.view));
        w.write_u64(s.acknowledged);
        w.write_u32(s.page);
        w.write_u32(s.total_pages);
        w.write_u8(s.more ? 1 : 0);
        w.write_i32(s.purchased);
        w.write_string(s.status);
        w.write_u8(static_cast<std::uint8_t>(s.entries.size()));
        for (const auto& e : s.entries) {
            if (!e.id || e.item.index <= 0 || e.item.count <= 0 || e.unit_price < 0 ||
                e.unit_price > kTradingPostMaxPrice || static_cast<unsigned>(e.delivery) > 2) {
                return {};
            }
            w.write_u64(e.id);
            w.write_i32(e.item.index);
            w.write_i32(e.item.count);
            w.write_u64(static_cast<std::uint64_t>(e.unit_price));
            w.write_u8(static_cast<std::uint8_t>(e.delivery));
            for (auto a : e.item.attributes) {
                w.write_i32(a);
            }
            w.write_u64(e.item.pet_expire_at);
            w.write_u64(e.item.ride_expire_at);
        }
        return w.frame(static_cast<std::uint8_t>(S2C::TradingPost));
    }
    bool
    parse_trading_post_state(const std::uint8_t* data, std::size_t size, TradingPostState& out) {
        PacketReader p(data, size);
        TradingPostState s;
        std::uint8_t version = 0, view = 0, more = 0, count = 0;
        if (!p.read_u8(version) || (version != 1 && version != 2) || !p.read_u8(view) || view > 3 ||
            !p.read_u64(s.acknowledged) || !p.read_u32(s.page) || s.page > 1000000) {
            return false;
        }
        s.total_pages = 0;
        if (version == 2 &&
            (!p.read_u32(s.total_pages) || (s.total_pages && s.page >= s.total_pages))) {
            return false;
        }
        if (!p.read_u8(more) || more > 1 || !p.read_i32(s.purchased) || s.purchased < 0 ||
            !p.read_string(s.status) || s.status.size() > 256 || !p.read_u8(count) ||
            count > kTradingPostPageSize) {
            return false;
        }
        s.view = static_cast<TradingPostView>(view);
        s.more = more != 0;
        for (unsigned i = 0; i < count; ++i) {
            TradingPostEntry e;
            std::uint64_t price = 0;
            std::uint8_t kind = 0;
            if (!p.read_u64(e.id) || !e.id || !p.read_i32(e.item.index) || e.item.index <= 0 ||
                !p.read_i32(e.item.count) || e.item.count <= 0 || !p.read_u64(price) ||
                price > kTradingPostMaxPrice || !p.read_u8(kind) || kind > 2) {
                return false;
            }
            e.unit_price = static_cast<std::int64_t>(price);
            e.delivery = static_cast<TradingPostDelivery>(kind);
            for (auto& a : e.item.attributes) {
                if (!p.read_i32(a)) {
                    return false;
                }
            }
            if (!p.read_u64(e.item.pet_expire_at) || !p.read_u64(e.item.ride_expire_at)) {
                return false;
            }
            s.entries.push_back(e);
        }
        if (p.remaining()) {
            return false;
        }
        out = std::move(s);
        return true;
    }
    std::vector<std::uint8_t> frame_trading_post_collection(bool pending) {
        PacketWriter w;
        w.write_u8(pending ? 1 : 0);
        return w.frame(static_cast<std::uint8_t>(S2C::TradingPostCollection));
    }

    bool parse_trading_post_collection(const std::uint8_t* data, std::size_t size, bool& pending) {
        PacketReader p(data, size);
        std::uint8_t value = 0;
        if (!p.read_u8(value) || value > 1 || p.remaining()) {
            return false;
        }
        pending = value != 0;
        return true;
    }

    std::vector<std::uint8_t> frame_trading_post_pickup(const TradingPostPickup& pickup) {
        if (!valid(pickup)) {
            return {};
        }
        PacketWriter w;
        w.write_i32(pickup.item_index);
        w.write_i32(pickup.quantity);
        w.write_u64(static_cast<std::uint64_t>(pickup.geons));
        return w.frame(static_cast<std::uint8_t>(S2C::TradingPostPickup));
    }

    bool
    parse_trading_post_pickup(const std::uint8_t* data, std::size_t size, TradingPostPickup& out) {
        PacketReader p(data, size);
        TradingPostPickup pickup;
        std::uint64_t geons = 0;
        if (!p.read_i32(pickup.item_index) || !p.read_i32(pickup.quantity) || !p.read_u64(geons) ||
            geons > kTradingPostMaxPrice || p.remaining()) {
            return false;
        }
        pickup.geons = static_cast<std::int64_t>(geons);
        if (!valid(pickup)) {
            return false;
        }
        out = pickup;
        return true;
    }
}  // namespace openkal::protocol
