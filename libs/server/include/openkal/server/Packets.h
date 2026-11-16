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
#include "openkal/server/world/GameWorld.h"

#include <cstdint>
#include <vector>

namespace openkal::server {

    /** Builds server packet frames without retaining state. */
    class PacketFramer final {
      public:
        PacketFramer() = delete;

        /** Builds the handshake challenge frame.
         * @param protocol_test_value Challenge value.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t> s2c_code(std::uint32_t protocol_test_value);

        /** Builds a monster creation frame.
         * @param monster Monster state.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t> create_monster(const MonsterEntity& monster);

        /** Builds an in-progress monster movement frame.
         * @param id Monster identifier.
         * @param dx X delta.
         * @param dy Y delta.
         * @param run Whether the movement is running.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t>
        move_monster_on(std::uint32_t id, std::int8_t dx, std::int8_t dy, bool run);

        /** Builds a completed monster movement frame.
         * @param id Monster identifier.
         * @param dx X delta.
         * @param dy Y delta.
         * @param run Whether the movement is running.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t>
        move_monster_end(std::uint32_t id, std::int8_t dx, std::int8_t dy, bool run);

        /** Builds a monster removal frame.
         * @param id Monster identifier.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t> remove_monster(std::uint32_t id);

        /** Builds a ground-item creation frame (S2C_CREATEITEM "wdddd").
         * @param item Ground item state.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t> create_item(const GroundItem& item);

        /** Builds a ground-item removal frame (S2C_REMOVEITEM "d").
         * @param id World entity id.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t> remove_item(std::uint32_t id);

        /** Builds a bag-insert frame (S2C_INSERTITEM simplified PutInfo).
         * @param iid Inventory instance id.
         * @param item_index item definition index.
         * @param prefix Prefix id.
         * @param info Specialty bitmask (0 for now).
         * @param num Stack count or money amount.
         * @param charm Optional talisman / Explosive Blow bytes.
         * @param expire_at Authoritative item expiry as unix seconds, or 0 for permanent.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t>
        insert_item(std::uint32_t iid,
                    std::uint16_t item_index,
                    std::uint8_t prefix,
                    std::uint32_t info,
                    std::int32_t num,
                    const protocol::InsertCharmFields& charm = {},
                    std::uint64_t expire_at = 0);

        /** Builds S2C_MESSAGE "b".
         * @param msg_id MESSAGE_TYPE ordinal.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t> message(std::uint8_t msg_id);

        /** Builds an in-progress player movement frame.
         * @param id Player identifier.
         * @param dx X delta.
         * @param dy Y delta.
         * @param dz Z delta.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t>
        move_player_on(std::uint32_t id, std::int8_t dx, std::int8_t dy, std::int8_t dz);

        /** Builds a completed player movement frame.
         * @param id Player identifier.
         * @param dx X delta.
         * @param dy Y delta.
         * @param dz Z delta.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t>
        move_player_end(std::uint32_t id, std::int8_t dx, std::int8_t dy, std::int8_t dz);

        /** Builds an authoritative player position frame.
         * @param id Player identifier.
         * @param x World X coordinate.
         * @param y World Y coordinate.
         * @param z World Z coordinate.
         * @param dir Facing direction.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t>
        move_before(std::uint32_t id, int x, int y, int z, std::uint16_t dir = 0);
    };

    /** Compatibility wrapper for PacketFramer::s2c_code. */
    [[nodiscard]] std::vector<std::uint8_t> frame_s2c_code(std::uint32_t protocol_test_value);

    /** Compatibility wrapper for PacketFramer::create_monster. */
    [[nodiscard]] std::vector<std::uint8_t> frame_create_monster(const MonsterEntity& monster);

    /** Compatibility wrapper for PacketFramer::move_monster_on. */
    [[nodiscard]] std::vector<std::uint8_t>
    frame_move_monster_on(std::uint32_t id, std::int8_t dx, std::int8_t dy, bool run);

    /** Compatibility wrapper for PacketFramer::move_monster_end. */
    [[nodiscard]] std::vector<std::uint8_t>
    frame_move_monster_end(std::uint32_t id, std::int8_t dx, std::int8_t dy, bool run);

    /** Compatibility wrapper for PacketFramer::remove_monster. */
    [[nodiscard]] std::vector<std::uint8_t> frame_remove_monster(std::uint32_t id);

    /** Compatibility wrapper for PacketFramer::create_item. */
    [[nodiscard]] std::vector<std::uint8_t> frame_create_item(const GroundItem& item);

    /** Compatibility wrapper for PacketFramer::remove_item. */
    [[nodiscard]] std::vector<std::uint8_t> frame_remove_item(std::uint32_t id);

    /** Compatibility wrapper for PacketFramer::insert_item.
     * @param iid Inventory instance id.
     * @param item_index item definition index.
     * @param prefix Prefix id.
     * @param info Specialty bitmask.
     * @param num Stack count or money amount.
     * @param charm Optional talisman / Explosive Blow bytes.
     * @param expire_at Authoritative item expiry as unix seconds, or 0 for permanent.
     * @return Framed packet bytes.
     */
    [[nodiscard]] std::vector<std::uint8_t>
    frame_insert_item(std::uint32_t iid,
                      std::uint16_t item_index,
                      std::uint8_t prefix,
                      std::uint32_t info,
                      std::int32_t num,
                      const protocol::InsertCharmFields& charm = {},
                      std::uint64_t expire_at = 0);

    /** Compatibility wrapper for PacketFramer::message. */
    [[nodiscard]] std::vector<std::uint8_t> frame_message(std::uint8_t msg_id);

    /** Compatibility wrapper for PacketFramer::move_player_on. */
    [[nodiscard]] std::vector<std::uint8_t>
    frame_move_player_on(std::uint32_t id, std::int8_t dx, std::int8_t dy, std::int8_t dz);

    /** Compatibility wrapper for PacketFramer::move_player_end. */
    [[nodiscard]] std::vector<std::uint8_t>
    frame_move_player_end(std::uint32_t id, std::int8_t dx, std::int8_t dy, std::int8_t dz);

    /** Compatibility wrapper for PacketFramer::move_before. */
    [[nodiscard]] std::vector<std::uint8_t>
    frame_move_before(std::uint32_t id, int x, int y, int z, std::uint16_t dir = 0);

}  // namespace openkal::server
