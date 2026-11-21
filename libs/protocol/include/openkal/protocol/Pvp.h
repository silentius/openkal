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
#include <string_view>
#include <vector>

namespace openkal::protocol {

    /** Public name used for masked characters. */
    inline constexpr std::string_view kAssassinName = "Assassin";
    /** Warning shown when a masked player comes within range. */
    inline constexpr std::string_view kAssassinWarning = "The assassin is coming up close";

    /** Dialog shared by the Assassin List boards. */
    inline constexpr int kAssassinListDialog = 400001;
    /** Number of defeats retained by the Assassin List. */
    inline constexpr std::size_t kAssassinListLimit = 50;

    /** Character names at the time of a masked Assassin's defeat. */
    struct AssassinListEntry {
        std::string assassin;
        std::string last_hitter;
    };

    /** Newest-first board snapshot associated with the NPC that was clicked. */
    struct AssassinList {
        std::uint32_t npc_id = 0;
        std::vector<AssassinListEntry> entries;
    };

    /** Encodes a Town Board reply.
     * @param list Board NPC and recorded defeats.
     * @return Framed server packet.
     */
    [[nodiscard]] std::vector<std::uint8_t> frame_assassin_list(const AssassinList& list);

    /** Decodes a Town Board reply.
     * @param data Packet payload.
     * @param size Payload length.
     * @param list Parsed snapshot.
     * @return Whether the payload is valid.
     */
    [[nodiscard]] bool
    parse_assassin_list(const std::uint8_t* data, std::size_t size, AssassinList& list);

    /** The subject's allegiance from the recipient's perspective. */
    enum class PvpRelation : std::uint8_t { Neutral, Ally, Enemy };

    /** Observer-specific combat and presentation state of a visible character. */
    struct PvpStatus {
        std::uint32_t player_id = 0;
        PvpRelation relation = PvpRelation::Neutral;
        bool attackable = false;
        bool defeated = false;
        bool masked = false;
        /** True while the player is dueling or recovering from a duel defeat. */
        bool dueling = false;
    };

    /** Encodes a PvP status update.
     * @param status Subject and its state from the observer's perspective.
     * @return Framed server packet.
     */
    [[nodiscard]] std::vector<std::uint8_t> frame_pvp_status(const PvpStatus& status);

    /** Decodes a PvP status update.
     * @param data Packet payload.
     * @param size Payload length.
     * @param status Parsed status.
     * @return Whether the payload is valid.
     */
    [[nodiscard]] bool
    parse_pvp_status(const std::uint8_t* data, std::size_t size, PvpStatus& status);

}  // namespace openkal::protocol
