/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/protocol/Riding.h"

#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketReader.h"
#include "openkal/protocol/PacketWriter.h"

namespace openkal::protocol {
    namespace {
        constexpr std::uint8_t kToggle = 0;
        constexpr std::size_t kBattleRequestSize = 17;
        constexpr std::size_t kStateSize = 77;
        constexpr std::size_t kCacheResultSize = 12;
    }  // namespace

    std::vector<std::uint8_t> Riding::frame_client_toggle() {
        PacketWriter writer;
        writer.write_u8(kToggle);
        return writer.frame(static_cast<std::uint8_t>(C2S::Riding));
    }

    bool Riding::parse_client_toggle(const std::uint8_t* data, std::size_t size) {
        if (!data || size != 1) {
            return false;
        }
        PacketReader reader(data, size);
        std::uint8_t command = 0xff;
        return reader.read_u8(command) && reader.ok() && command == kToggle;
    }

    std::vector<std::uint8_t>
    Riding::frame_client_battle_request(const BattleRidingRequest& request) {
        PacketWriter writer;
        writer.write_u8(static_cast<std::uint8_t>(request.operation));
        writer.write_u32(request.operation_id);
        writer.write_u32(request.mount_iid);
        writer.write_u32(request.primary_iid);
        writer.write_u32(request.secondary_iid);
        return writer.frame(static_cast<std::uint8_t>(C2S::Riding));
    }

    bool Riding::parse_client_battle_request(const std::uint8_t* data,
                                             std::size_t size,
                                             BattleRidingRequest& request) {
        if (!data || size != kBattleRequestSize) {
            return false;
        }
        PacketReader reader(data, size);
        std::uint8_t operation = 0;
        if (!reader.read_u8(operation) ||
            operation < static_cast<std::uint8_t>(BattleRidingOperation::Equip) ||
            operation > static_cast<std::uint8_t>(BattleRidingOperation::StableConvertDuplicate) ||
            !reader.read_u32(request.operation_id) || !reader.read_u32(request.mount_iid) ||
            !reader.read_u32(request.primary_iid) || !reader.read_u32(request.secondary_iid) ||
            !reader.ok() || request.operation_id == 0) {
            return false;
        }
        request.operation = static_cast<BattleRidingOperation>(operation);
        if (battle_riding_operation_requires_mount(request.operation) && request.mount_iid == 0) {
            return false;
        }
        return true;
    }

    std::vector<std::uint8_t> Riding::frame_server_state(const RidingState& state) {
        PacketWriter writer;
        writer.write_u32(state.player_id);
        writer.write_u32(state.iid);
        writer.write_u16(state.item_index);
        writer.write_u64(state.expire_at);
        writer.write_u8(state.mounted ? 1u : 0u);
        writer.write_u8(static_cast<std::uint8_t>(state.kind));
        writer.write_u8(static_cast<std::uint8_t>(state.grade));
        writer.write_u8(state.rarity);
        writer.write_u16(state.satiety);
        writer.write_u16(state.max_satiety);
        writer.write_u32(state.revision);
        for (std::uint32_t iid : state.equipment_iids) {
            writer.write_u32(iid);
        }
        for (std::uint16_t index : state.equipment_item_indices) {
            writer.write_u16(index);
        }
        for (std::uint8_t enchant : state.equipment_enchants) {
            writer.write_u8(enchant);
        }
        for (std::uint16_t ability : state.abilities) {
            writer.write_u16(ability);
        }
        return writer.frame(static_cast<std::uint8_t>(S2C::Riding));
    }

    bool
    Riding::parse_server_state(const std::uint8_t* data, std::size_t size, RidingState& state) {
        if (!data || size != kStateSize) {
            return false;
        }
        PacketReader reader(data, size);
        std::uint8_t mounted = 0;
        std::uint8_t kind = 0;
        std::uint8_t grade = 0;
        if (!reader.read_u32(state.player_id) || !reader.read_u32(state.iid) ||
            !reader.read_u16(state.item_index) || !reader.read_u64(state.expire_at) ||
            !reader.read_u8(mounted) || !reader.read_u8(kind) || !reader.read_u8(grade) ||
            !reader.read_u8(state.rarity) || !reader.read_u16(state.satiety) ||
            !reader.read_u16(state.max_satiety) || !reader.read_u32(state.revision) ||
            !reader.ok() || mounted > 1 || kind > 1 || grade > 3) {
            return false;
        }
        for (std::uint32_t& iid : state.equipment_iids) {
            if (!reader.read_u32(iid)) {
                return false;
            }
        }
        for (std::uint16_t& index : state.equipment_item_indices) {
            if (!reader.read_u16(index)) {
                return false;
            }
        }
        for (std::uint8_t& enchant : state.equipment_enchants) {
            if (!reader.read_u8(enchant)) {
                return false;
            }
        }
        for (std::uint16_t& ability : state.abilities) {
            if (!reader.read_u16(ability)) {
                return false;
            }
        }
        if (!reader.ok()) {
            return false;
        }
        state.mounted = mounted != 0;
        state.kind = static_cast<RidingKind>(kind);
        state.grade = static_cast<BattleRidingGrade>(grade);
        return true;
    }

    std::vector<std::uint8_t> Riding::frame_server_cache_result(const RidingCacheResult& result) {
        PacketWriter writer;
        writer.write_u32(result.cache_iid);
        writer.write_u16(result.seal_item_index);
        writer.write_u16(result.seal_count);
        writer.write_u16(result.reward_item_index);
        writer.write_u16(result.reward_count);
        return writer.frame(static_cast<std::uint8_t>(S2C::RidingCache));
    }

    bool Riding::parse_server_cache_result(const std::uint8_t* data,
                                           std::size_t size,
                                           RidingCacheResult& result) {
        if (!data || size != kCacheResultSize) {
            return false;
        }
        PacketReader reader(data, size);
        return reader.read_u32(result.cache_iid) && reader.read_u16(result.seal_item_index) &&
               reader.read_u16(result.seal_count) && reader.read_u16(result.reward_item_index) &&
               reader.read_u16(result.reward_count) && reader.ok() && result.cache_iid != 0 &&
               result.seal_item_index != 0 && result.seal_count != 0 &&
               result.reward_item_index != 0 && result.reward_count != 0;
    }

}  // namespace openkal::protocol
