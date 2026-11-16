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

#include <cstddef>
#include <cstdint>
#include <vector>

namespace openkal::protocol {

    /** C2S_ATTACK kind: player target. */
    inline constexpr std::uint8_t kCkPlayer = 0;

    /** C2S_ATTACK kind: monster target. */
    inline constexpr std::uint8_t kCkMonster = 1;

    /** S2C_ATTACK/S2C_SKILL flag: hit connected (bit 0). */
    inline constexpr std::uint8_t kAtkHit = 1;

    /** S2C_ATTACK/S2C_SKILL flag: fatal / critical (bit 1). */
    inline constexpr std::uint8_t kAtkFatal = 2;

    /** S2C_ATTACK/S2C_SKILL flag: blocked (bit 2). */
    inline constexpr std::uint8_t kAtkBlock = 4;

    /** S2C_ATTACK Explosive Blow (compatible client `bSuccess |= 8` when palsy > 0). */
    inline constexpr std::uint8_t kAtkExplosive = 8;

    /** OpenKal extension: attack clip only, no hit/miss verdict yet. */
    inline constexpr std::uint8_t kAtkAnim = 32;

    /** Encodes and decodes attack messages. */
    class Attack final {
      public:
        Attack() = delete;

        /**
         * Builds C2S_ATTACK.
         * @param kind Target kind (kCkPlayer / kCkMonster).
         * @param id Target entity id.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_client(std::uint8_t kind,
                                                                    std::uint32_t id);

        /**
         * Parses C2S_ATTACK payload (no outer header).
         * @param data Payload bytes.
         * @param size Payload length.
         * @param kind Out target kind.
         * @param id Out target entity id.
         * @return True when the payload is valid.
         */
        [[nodiscard]] static bool parse_client(const std::uint8_t* data,
                                               std::size_t size,
                                               std::uint8_t& kind,
                                               std::uint32_t& id);

        /**
         * Builds S2C_ATTACK.
         * @param attacker Attacker entity id.
         * @param target Target entity id.
         * @param damage Hit damage.
         * @param palsy Extra Explosive Blow damage included in `damage` (default 0).
         * @param flags Hit flags (default 0).
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_server(std::uint32_t attacker,
                                                                    std::uint32_t target,
                                                                    std::int32_t damage,
                                                                    std::int32_t palsy = 0,
                                                                    std::uint8_t flags = 0);

        /**
         * Parses S2C_ATTACK payload (no outer header).
         * @param data Payload bytes.
         * @param size Payload length.
         * @param attacker Out attacker entity id.
         * @param target Out target entity id.
         * @param damage Out hit damage.
         * @param palsy Out extra Explosive Blow damage.
         * @param flags Out hit flags.
         * @return True when the payload is valid.
         */
        [[nodiscard]] static bool parse_server(const std::uint8_t* data,
                                               std::size_t size,
                                               std::uint32_t& attacker,
                                               std::uint32_t& target,
                                               std::int32_t& damage,
                                               std::int32_t& palsy,
                                               std::uint8_t& flags);
    };

    /**
     * Builds C2S_ATTACK (free wrapper for Attack::frame_client).
     * @param kind Target kind (kCkPlayer / kCkMonster).
     * @param id Target entity id.
     * @return Framed packet bytes.
     */
    [[nodiscard]] std::vector<std::uint8_t> frame_c2s_attack(std::uint8_t kind, std::uint32_t id);

    /**
     * Parses C2S_ATTACK payload (free wrapper for Attack::parse_client).
     * @param data Payload bytes.
     * @param size Payload length.
     * @param kind Out target kind.
     * @param id Out target entity id.
     * @return True when the payload is valid.
     */
    [[nodiscard]] bool parse_c2s_attack(const std::uint8_t* data,
                                        std::size_t size,
                                        std::uint8_t& kind,
                                        std::uint32_t& id);

    /**
     * Builds S2C_ATTACK (free wrapper for Attack::frame_server).
     * @param attacker Attacker entity id.
     * @param target Target entity id.
     * @param damage Hit damage.
     * @param palsy Extra Explosive Blow damage included in `damage` (default 0).
     * @param flags Hit flags (default 0).
     * @return Framed packet bytes.
     */
    [[nodiscard]] std::vector<std::uint8_t> frame_s2c_attack(std::uint32_t attacker,
                                                             std::uint32_t target,
                                                             std::int32_t damage,
                                                             std::int32_t palsy = 0,
                                                             std::uint8_t flags = 0);

    /**
     * Parses S2C_ATTACK payload (free wrapper for Attack::parse_server).
     * @param data Payload bytes.
     * @param size Payload length.
     * @param attacker Out attacker entity id.
     * @param target Out target entity id.
     * @param damage Out hit damage.
     * @param palsy Out extra Explosive Blow damage.
     * @param flags Out hit flags.
     * @return True when the payload is valid.
     */
    [[nodiscard]] bool parse_s2c_attack(const std::uint8_t* data,
                                        std::size_t size,
                                        std::uint32_t& attacker,
                                        std::uint32_t& target,
                                        std::int32_t& damage,
                                        std::int32_t& palsy,
                                        std::uint8_t& flags);

}  // namespace openkal::protocol
