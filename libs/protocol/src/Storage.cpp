/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/protocol/Storage.h"

#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketReader.h"
#include "openkal/protocol/PacketWriter.h"

#include <array>
#include <limits>

namespace openkal::protocol {
    namespace {
        constexpr std::size_t kStorageItemWireSize = 37;
        constexpr std::size_t kFullStoragePayload = 5 + kStorageCapacity * kStorageItemWireSize;
        static_assert(kStorageCapacity <= std::numeric_limits<std::uint8_t>::max());
    }

    std::vector<std::uint8_t> Storage::frame_info_c2s(std::uint32_t npc_id) {
        PacketWriter writer;
        writer.write_u32(npc_id);
        return writer.frame(static_cast<std::uint8_t>(C2S::StorageInfo));
    }

    bool
    Storage::parse_info_c2s(const std::uint8_t* data, std::size_t size, std::uint32_t& npc_id) {
        PacketReader reader(data, size);
        return data && size == 4 && reader.read_u32(npc_id);
    }

    std::vector<std::uint8_t> Storage::frame_info_s2c(const StorageInfo& info) {
        if (info.items.size() > static_cast<std::size_t>(kStorageCapacity) ||
            info.items.size() > std::numeric_limits<std::uint16_t>::max()) {
            return {};
        }
        PacketWriter writer;
        writer.write_u32(info.npc_id);
        if (info.items.size() == kStorageCapacity) {
            // The full snapshot needs a one-byte count to fit the existing frame limit.
            writer.write_u8(static_cast<std::uint8_t>(info.items.size()));
        } else {
            writer.write_u16(static_cast<std::uint16_t>(info.items.size()));
        }
        for (const StorageItem& item : info.items) {
            writer.write_u32(item.iid);
            writer.write_u16(item.index);
            writer.write_u8(item.prefix);
            writer.write_u16(item.slot);
            writer.write_u32(item.count);
            writer.write_u32(item.info);
            std::array<std::uint8_t, kInsertCharmBytes> charm{};
            pack_insert_charm(charm.data(), item.charm);
            writer.write_bytes(charm.data(), charm.size());
            writer.write_u16(item.transform_level);
            writer.write_u32(item.transform_exp);
        }
        return writer.frame(static_cast<std::uint8_t>(S2C::StorageInfo));
    }

    bool Storage::parse_info_s2c(const std::uint8_t* data, std::size_t size, StorageInfo& info) {
        if (!data || size < 6) {
            return false;
        }
        PacketReader reader(data, size);
        std::uint16_t count = 0;
        if (!reader.read_u32(info.npc_id)) {
            return false;
        }
        if (size == kFullStoragePayload) {
            std::uint8_t full_count = 0;
            if (!reader.read_u8(full_count) || full_count != kStorageCapacity) {
                return false;
            }
            count = full_count;
        } else if (!reader.read_u16(count) || count > kStorageCapacity ||
                   size != 6u + count * kStorageItemWireSize) {
            return false;
        }
        info.items.clear();
        info.items.reserve(count);
        for (std::uint16_t i = 0; i < count; ++i) {
            StorageItem item;
            std::array<std::uint8_t, kInsertCharmBytes> charm{};
            if (!reader.read_u32(item.iid) || !reader.read_u16(item.index) ||
                !reader.read_u8(item.prefix) || !reader.read_u16(item.slot) ||
                !reader.read_u32(item.count) || !reader.read_u32(item.info) ||
                !reader.read_bytes(charm.data(), charm.size()) ||
                !reader.read_u16(item.transform_level) || !reader.read_u32(item.transform_exp) ||
                item.iid == 0 || item.index == 0 || item.count == 0 ||
                item.slot >= kStorageCapacity) {
                info.items.clear();
                return false;
            }
            item.charm = unpack_insert_charm(charm.data(), charm.size());
            info.items.push_back(item);
        }
        return reader.remaining() == 0;
    }

    std::vector<std::uint8_t> Storage::frame_transfer_c2s(bool deposit,
                                                          std::uint32_t npc_id,
                                                          std::uint32_t iid,
                                                          std::uint32_t count) {
        PacketWriter writer;
        writer.write_u32(npc_id);
        writer.write_u32(iid);
        writer.write_u32(count);
        return writer.frame(
            static_cast<std::uint8_t>(deposit ? C2S::PutInStorage : C2S::PutOutStorage));
    }

    bool Storage::parse_transfer_c2s(const std::uint8_t* data,
                                     std::size_t size,
                                     std::uint32_t& npc_id,
                                     std::uint32_t& iid,
                                     std::uint32_t& count) {
        PacketReader reader(data, size);
        return data && size == 12 && reader.read_u32(npc_id) && reader.read_u32(iid) &&
               reader.read_u32(count);
    }

    std::vector<std::uint8_t> Storage::frame_money_c2s(const StorageMoneyTransfer& transfer) {
        if (transfer.npc_id == 0 || transfer.amount <= 0) {
            return {};
        }
        PacketWriter writer;
        writer.write_u32(transfer.npc_id);
        writer.write_u8(transfer.deposit ? 1 : 0);
        writer.write_u64(static_cast<std::uint64_t>(transfer.amount));
        return writer.frame(static_cast<std::uint8_t>(C2S::StorageMoney));
    }

    bool Storage::parse_money_c2s(const std::uint8_t* data,
                                  std::size_t size,
                                  StorageMoneyTransfer& transfer) {
        PacketReader reader(data, size);
        std::uint32_t npc_id = 0;
        std::uint8_t direction = 0;
        std::uint64_t amount = 0;
        if (!data || size != 13 || !reader.read_u32(npc_id) || !reader.read_u8(direction) ||
            !reader.read_u64(amount) || npc_id == 0 || direction > 1 || amount == 0 ||
            amount > std::numeric_limits<std::int64_t>::max()) {
            return false;
        }
        transfer = {npc_id, direction == 1, static_cast<std::int64_t>(amount)};
        return true;
    }

    std::vector<std::uint8_t> Storage::frame_money_s2c(const StorageBalance& balance) {
        if (balance.npc_id == 0 || balance.geon < 0) {
            return {};
        }
        PacketWriter writer;
        writer.write_u32(balance.npc_id);
        writer.write_u64(static_cast<std::uint64_t>(balance.geon));
        return writer.frame(static_cast<std::uint8_t>(S2C::StorageMoney));
    }

    bool
    Storage::parse_money_s2c(const std::uint8_t* data, std::size_t size, StorageBalance& balance) {
        PacketReader reader(data, size);
        std::uint32_t npc_id = 0;
        std::uint64_t geon = 0;
        if (!data || size != 12 || !reader.read_u32(npc_id) || !reader.read_u64(geon) ||
            npc_id == 0 || geon > std::numeric_limits<std::int64_t>::max()) {
            return false;
        }
        balance = {npc_id, static_cast<std::int64_t>(geon)};
        return true;
    }

}  // namespace openkal::protocol
