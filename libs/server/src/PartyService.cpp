/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/PartyService.h"

#include <algorithm>
#include <random>

namespace openkal::server {
    const PartyService::Group* PartyService::group(std::uint32_t player) const {
        const auto member = m_membership.find(player);
        if (member == m_membership.end()) {
            return nullptr;
        }
        const auto found = m_groups.find(member->second);
        return found == m_groups.end() ? nullptr : &found->second;
    }
    const PartyService::Invitation* PartyService::invitation(std::uint32_t player,
                                                             std::uint64_t now) const {
        const auto found = m_invitations.find(player);
        return found != m_invitations.end() && found->second.expires > now ? &found->second
                                                                           : nullptr;
    }
    std::string
    PartyService::invite_error(std::uint32_t actor, std::uint32_t target, std::uint64_t now) const {
        if (actor == 0 || target == 0 || actor == target) {
            return "Select another online player.";
        }
        const auto cooldown = m_next_invite.find(actor);
        if (cooldown != m_next_invite.end() && cooldown->second > now) {
            return "Please wait before inviting again.";
        }
        const auto* g = group(actor);
        if (g && g->leader != actor) {
            return "Only the party leader can invite players.";
        }
        if (g && g->members.size() >= protocol::kPartyCapacity) {
            return "The party is full (12 members).";
        }
        if (group(target)) {
            return "That player already belongs to a party.";
        }
        if (m_ignored.count(target)) {
            return "That player is ignoring party invitations.";
        }
        if (invitation(target, now)) {
            return "That player already has a pending invitation.";
        }
        return {};
    }
    std::string PartyService::invite(std::uint32_t actor, std::uint32_t target, std::uint64_t now) {
        const auto error = invite_error(actor, target, now);
        if (!error.empty()) {
            return error;
        }
        const auto* g = group(actor);
        m_next_invite[actor] = now + 1000;
        m_invitations[target] = {actor, m_next_token++, now + 30000, g ? g->id : 0};
        return {};
    }
    std::string
    PartyService::reply(std::uint32_t actor, std::uint32_t token, bool accept, std::uint64_t now) {
        const auto* pending = invitation(actor, now);
        if (!pending || pending->token != token) {
            return "The party invitation has expired.";
        }
        const Invitation request = *pending;
        m_invitations.erase(actor);
        if (!accept) {
            return {};
        }
        if (group(actor)) {
            return "You already belong to a party.";
        }
        const auto* current = group(request.inviter);
        if ((request.party != 0 &&
             (!current || current->id != request.party || current->leader != request.inviter)) ||
            (request.party == 0 && current && current->leader != request.inviter)) {
            return "The party has changed. Ask for another invitation.";
        }
        if (current && current->members.size() >= protocol::kPartyCapacity) {
            return "The party is full (12 members).";
        }
        std::uint32_t id = current ? current->id : 0;
        if (!id) {
            id = m_next_group++;
            m_groups.emplace(
                id, Group{id, request.inviter, protocol::PartyLoot::Random, {request.inviter}});
            m_membership[request.inviter] = id;
            m_invitations.erase(request.inviter);
        }
        m_groups.at(id).members.push_back(actor);
        m_loot_rounds.erase(id);
        m_membership[actor] = id;
        return {};
    }
    void PartyService::leave(std::uint32_t player) {
        for (auto it = m_invitations.begin(); it != m_invitations.end();) {
            if (it->first == player || it->second.inviter == player) {
                it = m_invitations.erase(it);
            } else {
                ++it;
            }
        }
        const auto* current = group(player);
        if (!current) {
            return;
        }
        const auto id = current->id;
        m_loot_rounds.erase(id);
        auto& g = m_groups.at(id);
        g.members.erase(std::remove(g.members.begin(), g.members.end(), player), g.members.end());
        m_membership.erase(player);
        if (g.members.size() < 2) {
            for (auto member : g.members) {
                m_membership.erase(member);
            }
            m_groups.erase(id);
        } else if (g.leader == player) {
            g.leader = g.members.front();
        }
    }
    bool PartyService::kick(std::uint32_t actor, std::uint32_t target) {
        const auto* g = group(actor);
        if (!g || g->leader != actor || actor == target || group(target) != g) {
            return false;
        }
        leave(target);
        return true;
    }
    bool PartyService::set_loot(std::uint32_t actor, protocol::PartyLoot loot) {
        const auto* g = group(actor);
        if (!g || g->leader != actor || static_cast<unsigned>(loot) > 1) {
            return false;
        }
        if (g->loot != loot) {
            m_loot_rounds.erase(g->id);
            m_groups.at(g->id).loot = loot;
        }
        return true;
    }

    std::uint32_t PartyService::item_recipient(std::uint32_t actor,
                                               const std::vector<std::uint32_t>& eligible) {
        const auto* g = group(actor);
        if (!g || g->loot != protocol::PartyLoot::Random) {
            return 0;
        }
        std::vector<std::uint32_t> members;
        for (const auto member : g->members) {
            if (std::find(eligible.begin(), eligible.end(), member) != eligible.end()) {
                members.push_back(member);
            }
        }
        if (members.empty()) {
            return 0;
        }
        auto& round = m_loot_rounds[g->id];
        if (round.eligible != members) {
            round.eligible = std::move(members);
            round.remaining.clear();
        }
        if (round.remaining.empty()) {
            round.remaining = round.eligible;
            thread_local std::mt19937 random(std::random_device{}());
            std::shuffle(round.remaining.begin(), round.remaining.end(), random);
        }
        return round.remaining.back();
    }

    void PartyService::record_item_award(std::uint32_t recipient) {
        const auto* g = group(recipient);
        if (!g) {
            return;
        }
        const auto found = m_loot_rounds.find(g->id);
        if (found == m_loot_rounds.end()) {
            return;
        }
        auto& remaining = found->second.remaining;
        remaining.erase(std::remove(remaining.begin(), remaining.end(), recipient),
                        remaining.end());
    }
    void PartyService::set_ignore(std::uint32_t player, bool ignore) {
        if (ignore) {
            m_ignored.insert(player);
            m_invitations.erase(player);
        } else {
            m_ignored.erase(player);
        }
    }
    void PartyService::disconnect(std::uint32_t player) {
        leave(player);
        m_ignored.erase(player);
        m_next_invite.erase(player);
    }
}  // namespace openkal::server
