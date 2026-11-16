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
    inline constexpr std::size_t kPartyCapacity = 12;
    /** OpenKal sharing radius in world pixels; the public guide specifies proximity only. */
    inline constexpr int kPartyShareRadius = 800;
    enum class PartyCommand : std::uint8_t { Snapshot, Invite, Reply, Leave, Kick, Loot, Ignore };
    enum class PartyLoot : std::uint8_t { FreeForAll, Random };
    /** Project-owned payload carried by the reserved party opcodes. */
    struct PartyRequest {
        PartyCommand command = PartyCommand::Snapshot;
        std::uint32_t target = 0;
        std::uint32_t invitation = 0;
        std::uint8_t value = 0;
        std::string name;
    };
    /** One online member's authoritative status. */
    struct PartyMember {
        std::uint32_t id = 0;
        std::string name;
        int level = 1;
        int hp = 0;
        int max_hp = 1;
        int map = 0;
    };
    /** Membership and the recipient's outstanding invitation, if any. */
    struct PartyState {
        std::uint32_t id = 0;
        std::uint32_t leader = 0;
        PartyLoot loot = PartyLoot::FreeForAll;
        std::uint32_t inviter = 0;
        std::uint32_t invitation = 0;
        std::string inviter_name;
        std::vector<PartyMember> members;
    };

    /** Successful ground-item award, sent only to the recipient's other party members. */
    struct PartyItemNotice {
        std::uint32_t party = 0;
        std::uint32_t recipient = 0;
        std::string recipient_name;
        int item_index = 0;
        int count = 0;
    };

    /** @param notice Authoritative item award. @return Framed UpdateParty notification. */
    [[nodiscard]] std::vector<std::uint8_t> frame_party_item(const PartyItemNotice& notice);

    /** @param data Payload bytes. @param size Payload length. @param out Parsed award.
     * @return Whether the complete item notification is valid. */
    [[nodiscard]] bool
    parse_party_item(const std::uint8_t* data, std::size_t size, PartyItemNotice& out);
    /** @param request Command to encode. @return Framed client request. */
    [[nodiscard]] std::vector<std::uint8_t> frame_party_request(const PartyRequest& request);

    /** @param data Payload bytes. @param size Payload size. @param out Parsed request.
     * @return Whether the complete payload is valid. */
    [[nodiscard]] bool
    parse_party_request(const std::uint8_t* data, std::size_t size, PartyRequest& out);

    /** @param state Snapshot to encode. @return Framed server snapshot. */
    [[nodiscard]] std::vector<std::uint8_t> frame_party_state(const PartyState& state);

    /** @param data Payload bytes. @param size Payload size. @param out Parsed state.
     * @return Whether the complete payload is valid. */
    [[nodiscard]] bool
    parse_party_state(const std::uint8_t* data, std::size_t size, PartyState& out);
}  // namespace openkal::protocol
