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

#include "openkal/protocol/Progression.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace openkal::protocol {

    /** Encodes and decodes S2C_EFFECT ("db" = entity id, effect type). */
    class Effect final {
      public:
        Effect() = delete;

        /**
         * Frames an S2C_EFFECT packet.
         * @param entity_id Target entity id.
         * @param effect Effect type byte.
         * @return Framed packet.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame(std::uint32_t entity_id,
                                                             std::uint8_t effect);

        /**
         * Frames the E_DAMAGED S2C_EFFECT variant ("dbw").
         * @param entity_id Target player entity id.
         * @param damage Applied hit damage.
         * @return Framed packet.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_damaged(std::uint32_t entity_id,
                                                                     std::uint16_t damage);

        /**
         * Parses an S2C_EFFECT payload.
         * @param data Payload bytes.
         * @param size Payload size.
         * @param entity_id Out entity id.
         * @param effect Out effect type.
         * @return True on success.
         */
        [[nodiscard]] static bool parse(const std::uint8_t* data,
                                        std::size_t size,
                                        std::uint32_t& entity_id,
                                        std::uint8_t& effect);

        /**
         * Parses the E_DAMAGED S2C_EFFECT variant ("dbw").
         * @param data Payload bytes.
         * @param size Payload size.
         * @param entity_id Out target player entity id.
         * @param damage Out applied hit damage.
         * @return True when the payload is a complete E_DAMAGED packet.
         */
        [[nodiscard]] static bool parse_damaged(const std::uint8_t* data,
                                                std::size_t size,
                                                std::uint32_t& entity_id,
                                                std::uint16_t& damage);
    };

    [[nodiscard]] std::vector<std::uint8_t> frame_s2c_effect(std::uint32_t entity_id,
                                                             std::uint8_t effect);
    [[nodiscard]] bool parse_s2c_effect(const std::uint8_t* data,
                                        std::size_t size,
                                        std::uint32_t& entity_id,
                                        std::uint8_t& effect);

}  // namespace openkal::protocol
