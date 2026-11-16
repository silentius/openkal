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

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace openkal::protocol {

    /** Riding system used by one registered mount. */
    enum class RidingKind : std::uint8_t {
        Common = 0,
        Battle = 1,
    };

    /** Persistent Battle Riding Pet grade. */
    enum class BattleRidingGrade : std::uint8_t {
        Normal = 0,
        Uncommon = 1,
        Rare = 2,
        Epic = 3,
    };

    /** Authoritative common riding-animal state. */
    struct RidingState {
        std::uint32_t player_id = 0;
        std::uint32_t iid = 0;
        std::uint16_t item_index = 0;
        std::uint64_t expire_at = 0;
        bool mounted = false;
        RidingKind kind = RidingKind::Common;
        BattleRidingGrade grade = BattleRidingGrade::Normal;
        /** Existing ItemRarity numeric value used for the grade presentation. */
        std::uint8_t rarity = 0;
        std::uint16_t satiety = 0;
        std::uint16_t max_satiety = 0;
        /** Optimistic concurrency revision of the per-IID Battle Riding state. */
        std::uint32_t revision = 0;
        std::array<std::uint32_t, 5> equipment_iids{};
        std::array<std::uint16_t, 5> equipment_item_indices{};
        std::array<std::uint8_t, 5> equipment_enchants{};
        std::array<std::uint16_t, 6> abilities{};
    };

    /** Mutating Battle Riding request kind. */
    enum class BattleRidingOperation : std::uint8_t {
        Equip = 1,
        Unequip = 2,
        Feed = 3,
        Combine = 4,
        UnlockAbility = 5,
        RerollAbilities = 6,
        EnchantEquipment = 7,
        UpgradeEquipment = 8,
        StableExchange = 9,
        StablePromote = 10,
        StableGrade = 11,
        StableTimeWrit = 12,
        StableConvertDuplicate = 13,
    };

    /** Returns whether an operation must identify the registered Riding Pet.
     * @param operation Requested Riding mutation.
     * @return True when a non-zero mount IID is required.
     */
    [[nodiscard]] constexpr bool
    battle_riding_operation_requires_mount(BattleRidingOperation operation) noexcept {
        return operation != BattleRidingOperation::StableExchange &&
               operation != BattleRidingOperation::StableConvertDuplicate;
    }

    /** Idempotent request for a Battle Riding or Stablemaster mutation. */
    struct BattleRidingRequest {
        BattleRidingOperation operation = BattleRidingOperation::Equip;
        std::uint32_t operation_id = 0;
        std::uint32_t mount_iid = 0;
        std::uint32_t primary_iid = 0;
        std::uint32_t secondary_iid = 0;
    };

    /** Server-confirmed contents of one successfully opened Riding Cache. */
    struct RidingCacheResult {
        /** Consumed cache item instance. */
        std::uint32_t cache_iid = 0;
        /** Guaranteed Riding Seal item definition and units. */
        std::uint16_t seal_item_index = 0;
        std::uint16_t seal_count = 0;
        /** Additional weighted item definition and units. */
        std::uint16_t reward_item_index = 0;
        std::uint16_t reward_count = 0;
    };

    /** Encodes and decodes the OpenKal-native common and Battle Riding protocol. */
    class Riding final {
      public:
        Riding() = delete;

        /** Builds a C2S request that toggles the registered common mount. */
        [[nodiscard]] static std::vector<std::uint8_t> frame_client_toggle();

        /**
         * Parses a C2S riding request.
         * @param data Payload bytes.
         * @param size Payload length.
         * @return True when the request is a valid toggle command.
         */
        [[nodiscard]] static bool parse_client_toggle(const std::uint8_t* data, std::size_t size);

        /** Builds an idempotent C2S Battle Riding mutation request.
         * @param request Complete operation request.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t>
        frame_client_battle_request(const BattleRidingRequest& request);

        /** Parses a C2S Battle Riding mutation request.
         * @param data Payload bytes.
         * @param size Payload length.
         * @param request Receives the decoded request.
         * @return True when the complete payload is valid.
         */
        [[nodiscard]] static bool parse_client_battle_request(const std::uint8_t* data,
                                                              std::size_t size,
                                                              BattleRidingRequest& request);

        /**
         * Builds an S2C authoritative riding snapshot.
         * @param state State to encode.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_server_state(const RidingState& state);

        /**
         * Parses an S2C authoritative riding snapshot.
         * @param data Payload bytes.
         * @param size Payload length.
         * @param state Receives the state.
         * @return True when the complete payload is valid.
         */
        [[nodiscard]] static bool
        parse_server_state(const std::uint8_t* data, std::size_t size, RidingState& state);

        /** Builds the S2C Riding Cache result used to present an opened cache.
         * @param result Server-authoritative cache contents.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t>
        frame_server_cache_result(const RidingCacheResult& result);

        /** Parses one S2C Riding Cache result.
         * @param data Payload bytes.
         * @param size Payload length.
         * @param result Receives the decoded contents.
         * @return True when the complete payload is valid.
         */
        [[nodiscard]] static bool parse_server_cache_result(const std::uint8_t* data,
                                                            std::size_t size,
                                                            RidingCacheResult& result);
    };

}  // namespace openkal::protocol
