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

    /** First shared Action-tab emote shortcut. */
    inline constexpr int kFirstEmoteShortcut = 2022;

    /** Number of human action-10 motions. */
    inline constexpr int kEmoteCount = 20;

    /** One validated player emote notification. */
    struct PlayerEmote {
        std::uint32_t player_id = 0;  ///< Authoritative emitting player.
        std::uint8_t motion = 0;      ///< One-based action-10 motion.
    };

    /** Encodes OpenKal's PlayerAnimation emote messages. */
    class Emote final {
      public:
        Emote() = delete;

        /** Converts a shared shortcut to its action-10 motion.
         * @param shortcut Skill-bar binding.
         * @return One-based motion, or zero for other bindings.
         */
        [[nodiscard]] static constexpr int motion_for_shortcut(int shortcut) {
            return shortcut >= kFirstEmoteShortcut && shortcut < kFirstEmoteShortcut + kEmoteCount
                       ? shortcut - kFirstEmoteShortcut + 1
                       : 0;
        }

        /** Builds a PlayerAnimation request.
         * @param motion One-based motion (1..20).
         * @return Framed request, or empty for an invalid motion.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_client(std::uint8_t motion);

        /** Reads an exact one-byte request.
         * @param data Payload bytes.
         * @param size Payload size.
         * @param motion Receives the validated motion.
         * @return True for a valid request.
         */
        [[nodiscard]] static bool
        parse_client(const std::uint8_t* data, std::size_t size, std::uint8_t& motion);

        /** Builds an authoritative PlayerAnimation notification.
         * @param player_id Emitting player.
         * @param motion One-based motion (1..20).
         * @return Framed notification, or empty for invalid fields.
         */
        [[nodiscard]] static std::vector<std::uint8_t> frame_server(std::uint32_t player_id,
                                                                    std::uint8_t motion);

        /** Reads an exact five-byte notification.
         * @param data Payload bytes.
         * @param size Payload size.
         * @param event Receives the validated notification.
         * @return True for valid fields.
         */
        [[nodiscard]] static bool
        parse_server(const std::uint8_t* data, std::size_t size, PlayerEmote& event);
    };
}  // namespace openkal::protocol
