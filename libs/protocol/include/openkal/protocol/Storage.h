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

#include "openkal/protocol/InsertItem.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace openkal::protocol {

    /** Number of account-storage register pages. */
    inline constexpr int kStorageRegisters = 6;

    /** Number of item cells on one account-storage register. */
    inline constexpr int kStorageSlotsPerRegister = 36;

    /** Total number of item cells in account storage. */
    inline constexpr int kStorageCapacity = kStorageRegisters * kStorageSlotsPerRegister;

    /** One item instance in an account-storage snapshot. */
    struct StorageItem {
        std::uint32_t iid = 0;
        std::uint16_t index = 0;
        std::uint8_t prefix = 0;
        std::uint16_t slot = 0;
        std::uint32_t count = 0;
        std::uint32_t info = 0;
        InsertCharmFields charm;
        std::uint16_t transform_level = 1;
        std::uint32_t transform_exp = 0;
    };

    /** Complete account-storage state returned by the server. */
    struct StorageInfo {
        std::uint32_t npc_id = 0;
        std::vector<StorageItem> items;
    };

    /** Encodes and decodes account-storage packets. */
    class Storage final {
      public:
        Storage() = delete;

        /** Builds C2S_STORAGEINFO. @param npc_id Runtime storage-keeper id. @return Framed packet.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_info_c2s(std::uint32_t npc_id);

        /** Parses C2S_STORAGEINFO. @param data Payload. @param size Payload size. @param npc_id
         * Runtime storage-keeper id. @return True on success. */
        [[nodiscard]] static bool
        parse_info_c2s(const std::uint8_t* data, std::size_t size, std::uint32_t& npc_id);

        /** Builds S2C_STORAGEINFO. @param info Complete account storage. @return Framed packet. */
        [[nodiscard]] static std::vector<std::uint8_t> frame_info_s2c(const StorageInfo& info);

        /** Parses S2C_STORAGEINFO. @param data Payload. @param size Payload size. @param info
         * Decoded snapshot. @return True on success. */
        [[nodiscard]] static bool
        parse_info_s2c(const std::uint8_t* data, std::size_t size, StorageInfo& info);

        /** Builds one deposit or withdrawal request.
         * @param deposit True for C2S_PUTINSTORAGE, false for C2S_PUTOUTSTORAGE.
         * @param npc_id Runtime storage-keeper id.
         * @param iid Source item instance id.
         * @param count Units to transfer.
         * @return Framed packet.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_transfer_c2s(bool deposit,
                                                                          std::uint32_t npc_id,
                                                                          std::uint32_t iid,
                                                                          std::uint32_t count);

        /** Parses one deposit or withdrawal request.
         * @param data Payload.
         * @param size Payload size.
         * @param npc_id Runtime storage-keeper id.
         * @param iid Source item instance id.
         * @param count Units to transfer.
         * @return True on success.
         */
        [[nodiscard]] static bool parse_transfer_c2s(const std::uint8_t* data,
                                                     std::size_t size,
                                                     std::uint32_t& npc_id,
                                                     std::uint32_t& iid,
                                                     std::uint32_t& count);
    };

}  // namespace openkal::protocol
