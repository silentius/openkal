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

    /** Encodes and decodes teleport messages. */
    class Teleport final {
      public:
        Teleport() = delete;

        /**
         * Builds S2C_TELEPORT.
         * @param map Destination map index.
         * @param x Server pixel X.
         * @param y Server pixel Y.
         * @param z Height / Z.
         * @param cheat Anti-cheat / mode byte from the protocol.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t>
        frame_server(std::uint8_t map, int x, int y, int z, std::uint8_t cheat);

        /**
         * Parses S2C_TELEPORT payload (no outer header).
         * @param data Payload bytes.
         * @param size Payload length.
         * @param map Out map index.
         * @param x Out server pixel X.
         * @param y Out server pixel Y.
         * @param z Out height / Z.
         * @param cheat Out anti-cheat / mode byte.
         * @return True when the payload is valid.
         */
        [[nodiscard]] static bool parse_server(const std::uint8_t* data,
                                               std::size_t size,
                                               std::uint8_t& map,
                                               int& x,
                                               int& y,
                                               int& z,
                                               std::uint8_t& cheat);

        /**
         * Builds C2S_TELEPORT request (city / scroll location).
         * @param location Specialty teleport location index.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_client_request(std::uint8_t location);

        /**
         * Builds C2S_TELEPORT ack after the client finishes loading.
         * @param ok Non-zero when load succeeded.
         * @param z Client-reported Z.
         * @return Framed packet bytes.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_client_ack(std::uint8_t ok, int z);

        /**
         * Parses C2S_TELEPORT request payload.
         * @param data Payload bytes.
         * @param size Payload length.
         * @param location Out specialty teleport location index.
         * @return True when the payload is valid.
         */
        [[nodiscard]] static bool
        parse_client_request(const std::uint8_t* data, std::size_t size, std::uint8_t& location);

        /**
         * Parses C2S_TELEPORT ack payload.
         * @param data Payload bytes.
         * @param size Payload length.
         * @param ok Out success flag.
         * @param z Out client-reported Z.
         * @return True when the payload is valid.
         */
        [[nodiscard]] static bool
        parse_client_ack(const std::uint8_t* data, std::size_t size, std::uint8_t& ok, int& z);
    };

    /**
     * Builds S2C_TELEPORT (free wrapper for Teleport::frame_server).
     * @param map Destination map index.
     * @param x Server pixel X.
     * @param y Server pixel Y.
     * @param z Height / Z.
     * @param cheat Anti-cheat / mode byte.
     * @return Framed packet bytes.
     */
    [[nodiscard]] std::vector<std::uint8_t>
    frame_s2c_teleport(std::uint8_t map, int x, int y, int z, std::uint8_t cheat);

    /**
     * Parses S2C_TELEPORT payload (free wrapper for Teleport::parse_server).
     * @param data Payload bytes.
     * @param size Payload length.
     * @param map Out map index.
     * @param x Out server pixel X.
     * @param y Out server pixel Y.
     * @param z Out height / Z.
     * @param cheat Out anti-cheat / mode byte.
     * @return True when the payload is valid.
     */
    [[nodiscard]] bool parse_s2c_teleport(const std::uint8_t* data,
                                          std::size_t size,
                                          std::uint8_t& map,
                                          int& x,
                                          int& y,
                                          int& z,
                                          std::uint8_t& cheat);

    /**
     * Builds C2S_TELEPORT request (free wrapper for
     * Teleport::frame_client_request).
     * @param location Specialty teleport location index.
     * @return Framed packet bytes.
     */
    [[nodiscard]] std::vector<std::uint8_t> frame_c2s_teleport_request(std::uint8_t location);

    /**
     * Builds C2S_TELEPORT ack (free wrapper for Teleport::frame_client_ack).
     * @param ok Non-zero when load succeeded.
     * @param z Client-reported Z.
     * @return Framed packet bytes.
     */
    [[nodiscard]] std::vector<std::uint8_t> frame_c2s_teleport_ack(std::uint8_t ok, int z);

    /**
     * Parses C2S_TELEPORT request (free wrapper for
     * Teleport::parse_client_request).
     * @param data Payload bytes.
     * @param size Payload length.
     * @param location Out specialty teleport location index.
     * @return True when the payload is valid.
     */
    [[nodiscard]] bool
    parse_c2s_teleport_request(const std::uint8_t* data, std::size_t size, std::uint8_t& location);

    /**
     * Parses C2S_TELEPORT ack (free wrapper for Teleport::parse_client_ack).
     * @param data Payload bytes.
     * @param size Payload length.
     * @param ok Out success flag.
     * @param z Out client-reported Z.
     * @return True when the payload is valid.
     */
    [[nodiscard]] bool
    parse_c2s_teleport_ack(const std::uint8_t* data, std::size_t size, std::uint8_t& ok, int& z);

}  // namespace openkal::protocol
