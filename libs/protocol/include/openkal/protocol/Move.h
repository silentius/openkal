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

    /** Base running speed in horizontal world units per second. */
    inline constexpr float kRunSpeed = 32000.0f / 533.0f;

    /** Walk on the Air speed in horizontal world units per second. */
    inline constexpr float kWalkOnAirSpeed = 32000.0f / 400.0f;

    /** Encodes and decodes movement messages. */
    class Move final {
      public:
        Move() = delete;
        [[nodiscard]] static std::vector<std::uint8_t>
        frame_client_on(std::int8_t dx, std::int8_t dy, std::int8_t dz);
        [[nodiscard]] static std::vector<std::uint8_t>
        frame_client_end(std::int8_t dx, std::int8_t dy, std::int8_t dz);
        [[nodiscard]] static bool parse_client(const std::uint8_t* data,
                                               std::size_t size,
                                               std::int8_t& dx,
                                               std::int8_t& dy,
                                               std::int8_t& dz);
        [[nodiscard]] static std::vector<std::uint8_t>
        frame_player_on(std::uint32_t id, std::int8_t dx, std::int8_t dy, std::int8_t dz);
        [[nodiscard]] static std::vector<std::uint8_t>
        frame_player_end(std::uint32_t id, std::int8_t dx, std::int8_t dy, std::int8_t dz);
        [[nodiscard]] static bool parse_player(const std::uint8_t* data,
                                               std::size_t size,
                                               std::uint32_t& id,
                                               std::int8_t& dx,
                                               std::int8_t& dy,
                                               std::int8_t& dz);
        [[nodiscard]] static std::vector<std::uint8_t>
        frame_before(std::uint32_t id, int x, int y, int z, std::uint16_t dir);
        [[nodiscard]] static bool parse_before(const std::uint8_t* data,
                                               std::size_t size,
                                               std::uint32_t& id,
                                               int& x,
                                               int& y,
                                               int& z,
                                               std::uint16_t& dir);
    };

    [[nodiscard]] std::vector<std::uint8_t>
    frame_c2s_move_on(std::int8_t dx, std::int8_t dy, std::int8_t dz);
    [[nodiscard]] std::vector<std::uint8_t>
    frame_c2s_move_end(std::int8_t dx, std::int8_t dy, std::int8_t dz);
    [[nodiscard]] bool parse_c2s_move(const std::uint8_t* data,
                                      std::size_t size,
                                      std::int8_t& dx,
                                      std::int8_t& dy,
                                      std::int8_t& dz);
    [[nodiscard]] std::vector<std::uint8_t>
    frame_s2c_move_player_on(std::uint32_t id, std::int8_t dx, std::int8_t dy, std::int8_t dz);
    [[nodiscard]] std::vector<std::uint8_t>
    frame_s2c_move_player_end(std::uint32_t id, std::int8_t dx, std::int8_t dy, std::int8_t dz);
    [[nodiscard]] bool parse_s2c_move_player(const std::uint8_t* data,
                                             std::size_t size,
                                             std::uint32_t& id,
                                             std::int8_t& dx,
                                             std::int8_t& dy,
                                             std::int8_t& dz);
    [[nodiscard]] std::vector<std::uint8_t>
    frame_s2c_move_before(std::uint32_t id, int x, int y, int z, std::uint16_t dir);
    [[nodiscard]] bool parse_s2c_move_before(const std::uint8_t* data,
                                             std::size_t size,
                                             std::uint32_t& id,
                                             int& x,
                                             int& y,
                                             int& z,
                                             std::uint16_t& dir);

}  // namespace openkal::protocol
