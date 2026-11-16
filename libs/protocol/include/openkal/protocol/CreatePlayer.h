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
#include "openkal/protocol/Skin.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
namespace openkal::protocol {
    /** Decoded local create-player fields. */
    struct CreatePlayerFields {
        std::uint32_t id = 0;
        std::string name;
        std::uint8_t class_byte = 0;
        int x = 0;
        int y = 0;
        int z = 0;
        std::uint16_t dir = 0;
        std::uint32_t gstate = 0;
        std::uint16_t wear[7]{};
        /** Optional OpenKal cosmetic appearance extension. */
        SkinIndices skins{};
        /** Actual shield definition, carried by the cosmetic appearance extension. */
        std::uint16_t shield = 0;
        std::uint8_t face = 0;
        std::uint8_t hair = 0;
    };
    /** Encodes and decodes create-player messages. */
    class CreatePlayer final {
      public:
        CreatePlayer() = delete;
        [[nodiscard]] static std::vector<std::uint8_t> frame(std::uint32_t id,
                                                             const std::string& name,
                                                             std::uint8_t class_byte,
                                                             int x,
                                                             int y,
                                                             int z,
                                                             std::uint16_t dir,
                                                             std::uint32_t gstate,
                                                             const std::uint16_t wear[7],
                                                             std::uint8_t face,
                                                             std::uint8_t hair,
                                                             const SkinIndices& skins = {},
                                                             std::uint16_t shield = 0);
        [[nodiscard]] static bool
        parse(const std::uint8_t* data, std::size_t size, CreatePlayerFields& out);

        /**
         * Encodes S2C_REMOVEPLAYER (Write(S2C_REMOVEPLAYER, "d", id)).
         * @param id Entity id to despawn.
         * @return Framed packet.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_remove(std::uint32_t id);

        /**
         * Decodes S2C_REMOVEPLAYER.
         * @param data Payload bytes.
         * @param size Payload size.
         * @param out_id Receives the entity id.
         * @return True when parsed.
         */
        [[nodiscard]] static bool
        parse_remove(const std::uint8_t* data, std::size_t size, std::uint32_t& out_id);
    };
    [[nodiscard]] std::vector<std::uint8_t> frame_s2c_create_player(std::uint32_t id,
                                                                    const std::string& name,
                                                                    std::uint8_t class_byte,
                                                                    int x,
                                                                    int y,
                                                                    int z,
                                                                    std::uint16_t dir,
                                                                    std::uint32_t gstate,
                                                                    const std::uint16_t wear[7],
                                                                    std::uint8_t face,
                                                                    std::uint8_t hair,
                                                                    const SkinIndices& skins = {},
                                                                    std::uint16_t shield = 0);
    [[nodiscard]] bool
    parse_s2c_create_player(const std::uint8_t* data, std::size_t size, CreatePlayerFields& out);

    /**
     * Encodes S2C_REMOVEPLAYER.
     * @param id Entity id to despawn.
     * @return Framed packet.
     */
    [[nodiscard]] std::vector<std::uint8_t> frame_s2c_remove_player(std::uint32_t id);

    /**
     * Decodes S2C_REMOVEPLAYER.
     * @param data Payload bytes.
     * @param size Payload size.
     * @param out_id Receives the entity id.
     * @return True when parsed.
     */
    [[nodiscard]] bool
    parse_s2c_remove_player(const std::uint8_t* data, std::size_t size, std::uint32_t& out_id);
}  // namespace openkal::protocol
