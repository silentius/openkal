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
#include "openkal/protocol/Party.h"

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace openkal::server {
    /** Server-thread-owned membership, expiring invitations and fair item distribution. */
    class PartyService final {
      public:
        /** One party, ordered by join time for deterministic leadership succession. */
        struct Group {
            std::uint32_t id = 0;
            std::uint32_t leader = 0;
            protocol::PartyLoot loot = protocol::PartyLoot::Random;
            std::vector<std::uint32_t> members;
        };
        /** Invitation token prevents acceptance of replaced or expired requests. */
        struct Invitation {
            std::uint32_t inviter = 0;
            std::uint32_t token = 0;
            std::uint64_t expires = 0;
            std::uint32_t party = 0;
        };
        /** @param player Online player ID. @return Current group, or null. */
        const Group* group(std::uint32_t player) const;

        /** @param player Recipient ID. @param now Monotonic milliseconds.
         * @return Current invitation, or null when expired. */
        const Invitation* invitation(std::uint32_t player, std::uint64_t now) const;

        /** Checks an invitation without changing cooldowns or party state.
         * @param actor Inviter. @param target Recipient. @param now Monotonic milliseconds.
         * @return Empty when available, otherwise the rejection reason.
         */
        [[nodiscard]] std::string
        invite_error(std::uint32_t actor, std::uint32_t target, std::uint64_t now) const;

        /** @param actor Inviter ID. @param target Online recipient ID. @param now Monotonic
         * milliseconds.
         * @return Empty on success, otherwise user-facing rejection reason. */
        [[nodiscard]] std::string
        invite(std::uint32_t actor, std::uint32_t target, std::uint64_t now);

        /** @param actor Recipient ID. @param token Invitation token. @param accept Accept or
         * reject.
         * @param now Monotonic milliseconds. @return Empty on success, otherwise rejection reason.
         */
        [[nodiscard]] std::string
        reply(std::uint32_t actor, std::uint32_t token, bool accept, std::uint64_t now);

        /** Leaves a party and cancels invitations involving this player.
         * @param player Departing player ID. */
        void leave(std::uint32_t player);

        /** @param actor Leader ID. @param target Member ID. @return Whether removal succeeded. */
        [[nodiscard]] bool kick(std::uint32_t actor, std::uint32_t target);

        /** @param actor Leader ID. @param loot Distribution mode. @return Whether change succeeded.
         */
        [[nodiscard]] bool set_loot(std::uint32_t actor, protocol::PartyLoot loot);

        /** Selects the next member in a shuffled, equally shared item round.
         * Selection alone does not advance the round. Eligibility changes restart it.
         * @param actor Party member requesting pickup.
         * @param eligible Living nearby online member IDs; outsiders and duplicates are ignored.
         * @return Recipient ID, or zero when no eligible shared recipient exists. */
        [[nodiscard]] std::uint32_t item_recipient(std::uint32_t actor,
                                                   const std::vector<std::uint32_t>& eligible);

        /** Advances distribution only after an item was successfully awarded.
         * @param recipient Member who received the complete item/stack. */
        void record_item_award(std::uint32_t recipient);

        /** @param player Player ID. @param ignore Whether new invitations are blocked. */
        void set_ignore(std::uint32_t player, bool ignore);

        /** Removes session state on disconnect. @param player Disconnected ID. */
        void disconnect(std::uint32_t player);

      private:
        struct LootRound {
            std::vector<std::uint32_t> eligible;
            std::vector<std::uint32_t> remaining;
        };
        std::uint32_t m_next_group = 1;
        std::uint32_t m_next_token = 1;
        std::unordered_map<std::uint32_t, Group> m_groups;
        std::unordered_map<std::uint32_t, std::uint32_t> m_membership;
        std::unordered_map<std::uint32_t, Invitation> m_invitations;
        std::unordered_map<std::uint32_t, std::uint64_t> m_next_invite;
        std::unordered_set<std::uint32_t> m_ignored;
        /** Remaining recipients in each party's current shuffled sharing round. */
        std::unordered_map<std::uint32_t, LootRound> m_loot_rounds;
    };
}  // namespace openkal::server
