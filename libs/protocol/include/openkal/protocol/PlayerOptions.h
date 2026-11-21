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
#include <string>
#include <vector>

namespace openkal::protocol {
    /** Extensible actions on a selected online player; Query has no side effects. */
    enum class PlayerAction : std::uint8_t {
        Query,
        InviteParty,
        ApplyGuild,
        ChallengeDuel,
        AcceptDuel,
        DeclineDuel
    };

    /** Actor identity is taken from the authenticated session. */
    struct PlayerOptionsRequest {
        PlayerAction action = PlayerAction::Query;
        std::uint64_t request_id = 0;
        std::uint32_t target = 0, guild = 0;
        std::uint64_t expected_fee = 0;
    };

    /** Only actions currently permitted by the server are included. */
    struct PlayerOptionsState {
        std::uint64_t request_id = 0;
        std::uint32_t target = 0, guild = 0;
        std::string name, guild_name, status;
        std::uint64_t application_fee = 0;
        std::vector<PlayerAction> actions;
    };

    /** @param request Operation. @return Framed request or empty for invalid input. */
    [[nodiscard]] std::vector<std::uint8_t>
    frame_player_options_request(const PlayerOptionsRequest& request);

    /** @param data Payload. @param size Length. @param out Destination. @return Valid complete
     * input. */
    [[nodiscard]] bool parse_player_options_request(const std::uint8_t* data,
                                                    std::size_t size,
                                                    PlayerOptionsRequest& out);

    /** @param state Available actions and feedback. @return Framed state or empty for invalid
     * input. */
    [[nodiscard]] std::vector<std::uint8_t>
    frame_player_options_state(const PlayerOptionsState& state);

    /** @param data Payload. @param size Length. @param out Destination. @return Valid complete
     * input. */
    [[nodiscard]] bool
    parse_player_options_state(const std::uint8_t* data, std::size_t size, PlayerOptionsState& out);
}  // namespace openkal::protocol
