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

#include "openkal/protocol/Party.h"
#include "test_util.h"

#include <algorithm>
#include <cstdlib>

int main() {
    using namespace openkal;
    using test::expect;
    server::PartyService service;
    expect(!service.invite(1, 1, 1000).empty(), "self invitation rejected");
    service.set_ignore(2, true);
    expect(!service.invite_error(1, 2, 3000).empty(),
           "ignored targets have no contextual invite action");
    expect(!service.invite(1, 2, 2000).empty(), "ignored invitation rejected");
    service.set_ignore(2, false);
    expect(service.invite(1, 2, 3000).empty(), "invite ungrouped player");
    const auto token = service.invitation(2, 3000)->token;
    expect(!service.reply(2, token + 1, true, 3001).empty() && !service.group(1),
           "forged acceptance cannot form a group");
    expect(service.reply(2, token, true, 3001).empty(), "accept party invitation");
    expect(service.group(1) && service.group(1) == service.group(2) &&
               service.group(1)->leader == 1,
           "shared membership and leader");
    expect(service.group(1)->loot == protocol::PartyLoot::Random,
           "new parties automatically share item drops");
    expect(service.item_recipient(99, {1, 2}) == 0 && service.item_recipient(1, {99}) == 0,
           "outsiders cannot select or receive party loot");
    const std::vector<std::uint32_t> pair{2, 1, 2, 99};
    const auto reserved = service.item_recipient(1, pair);
    expect((reserved == 1 || reserved == 2) && service.item_recipient(1, pair) == reserved,
           "failed or uncommitted item award keeps the same turn");
    int first_awards = 0, second_awards = 0;
    for (int i = 0; i < 100; ++i) {
        const auto recipient = service.item_recipient(1, pair);
        first_awards += recipient == 1;
        second_awards += recipient == 2;
        service.record_item_award(recipient);
        expect(std::abs(first_awards - second_awards) <= 1,
               "each member receives one whole drop per two-drop round");
    }
    expect(first_awards == 50 && second_awards == 50,
           "two equally eligible members receive exactly 50 percent each");
    expect(service.item_recipient(1, {1}) == 1, "ineligible member is skipped");
    service.record_item_award(1);
    const auto returning_first = service.item_recipient(1, pair);
    service.record_item_award(returning_first);
    const auto returning_second = service.item_recipient(1, pair);
    expect(returning_second != returning_first,
           "returning member starts a new fair round without banking absent turns");
    service.record_item_award(returning_second);
    expect(!service.reply(2, token, true, 3002).empty(), "replayed acceptance rejected");
    expect(!service.invite(2, 3, 4000).empty(), "ordinary member cannot invite");
    expect(!service.set_loot(2, protocol::PartyLoot::Random), "ordinary member cannot change loot");
    expect(service.set_loot(1, protocol::PartyLoot::Random), "leader selects random loot");
    for (std::uint32_t id = 3; id <= 12; ++id) {
        const auto now = static_cast<std::uint64_t>(id) * 2000;
        expect(service.invite(1, id, now).empty(), "leader invites up to capacity");
        const auto* invite = service.invitation(id, now);
        expect(invite && service.reply(id, invite->token, true, now).empty(),
               "member joins growing party");
    }
    expect(service.group(1)->members.size() == 12, "exactly twelve members");
    std::vector<std::uint32_t> full_party;
    std::vector<int> awards(13, 0);
    for (std::uint32_t id = 1; id <= 12; ++id) {
        full_party.push_back(id);
    }
    for (int i = 0; i < 24; ++i) {
        const auto recipient = service.item_recipient(2, full_party);
        expect(recipient >= 1 && recipient <= 12, "full party recipient is a member");
        if (recipient >= 1 && recipient <= 12) {
            ++awards[recipient];
        }
        service.record_item_award(recipient);
    }
    expect(std::all_of(awards.begin() + 1, awards.end(), [](int count) { return count == 2; }),
           "twelve-player party shares twenty-four drops equally");
    expect(!service.invite(1, 13, 30000).empty(), "thirteenth member rejected");
    expect(!service.kick(2, 3), "ordinary member cannot remove players");
    service.disconnect(1);
    expect(service.group(2) && service.group(2)->leader == 2 &&
               service.group(2)->members.size() == 11,
           "disconnect transfers leadership");
    expect(service.kick(2, 3) && !service.group(3), "leader removes member");
    for (std::uint32_t id = 4; id <= 12; ++id) {
        service.leave(id);
    }
    expect(!service.group(2), "one remaining member dissolves party");
    expect(service.invite(20, 21, 1000).empty(), "create expiring invitation");
    const auto expired = service.invitation(21, 1000)->token;
    expect(!service.reply(21, expired, true, 31000).empty(), "expired invitation rejected");
    expect(service.invite(20, 21, 32000).empty(), "expired invitation can be replaced");
    service.disconnect(20);
    expect(!service.invitation(21, 32000), "disconnect cancels outstanding invitations");
    expect(service.invite(30, 31, 1000).empty(), "create cancellable invitation");
    service.set_ignore(31, true);
    expect(!service.invitation(31, 1000), "ignore option removes pending invitation");
    // Capacity must be checked again at acceptance, after simultaneous invitations.
    server::PartyService race;
    for (std::uint32_t id = 2; id <= 13; ++id) {
        expect(race.invite(1, id, id * 2000).empty(), "parallel invitations issued");
    }
    for (std::uint32_t id = 2; id <= 12; ++id) {
        const auto* invite = race.invitation(id, 26000);
        expect(invite && race.reply(id, invite->token, true, 26000).empty(),
               "accept outstanding invitations");
    }
    const auto overflow = race.invitation(13, 26000)->token;
    expect(!race.reply(13, overflow, true, 26000).empty() && !race.group(13),
           "simultaneous accepts cannot overfill party");

    protocol::PartyItemNotice notice{7, 2, "Companion", 9000, 3};
    auto notice_frame = protocol::frame_party_item(notice);
    protocol::PartyItemNotice item_notice;
    expect(
        protocol::parse_party_item(notice_frame.data() + 3, notice_frame.size() - 3, item_notice) &&
            item_notice.party == 7 && item_notice.recipient == 2 &&
            item_notice.recipient_name == "Companion" && item_notice.item_index == 9000 &&
            item_notice.count == 3,
        "party award notification roundtrip");
    for (std::size_t size = 0; size < notice_frame.size() - 3; ++size) {
        expect(!protocol::parse_party_item(notice_frame.data() + 3, size, item_notice),
               "truncated award notice rejected");
    }
    notice_frame.push_back(0);
    expect(
        !protocol::parse_party_item(notice_frame.data() + 3, notice_frame.size() - 3, item_notice),
        "trailing award notice bytes rejected");
    notice.count = 0;
    notice_frame = protocol::frame_party_item(notice);
    expect(
        !protocol::parse_party_item(notice_frame.data() + 3, notice_frame.size() - 3, item_notice),
        "empty item award rejected");

    protocol::PartyRequest request;
    request.command = protocol::PartyCommand::Invite;
    request.target = 12;
    request.name = "Companion";
    auto frame = protocol::frame_party_request(request);
    protocol::PartyRequest decoded;
    expect(protocol::parse_party_request(frame.data() + 3, frame.size() - 3, decoded) &&
               decoded.target == 12 && decoded.name == "Companion",
           "request roundtrip");
    for (std::size_t size = 0; size < frame.size() - 3; ++size) {
        expect(!protocol::parse_party_request(frame.data() + 3, size, decoded),
               "truncated request rejected");
    }
    frame.push_back(0);
    expect(!protocol::parse_party_request(frame.data() + 3, frame.size() - 3, decoded),
           "trailing request bytes rejected");
    protocol::PartyState state;
    state.id = 1;
    state.leader = 1;
    state.loot = protocol::PartyLoot::Random;
    for (std::uint32_t id = 1; id <= 12; ++id) {
        state.members.push_back({id, "Member", 42, 80, 100, 0});
    }
    frame = protocol::frame_party_state(state);
    protocol::PartyState snapshot;
    expect(protocol::parse_party_state(frame.data() + 3, frame.size() - 3, snapshot) &&
               snapshot.members.size() == 12 && snapshot.loot == protocol::PartyLoot::Random,
           "twelve member snapshot roundtrip");
    for (std::size_t size = 0; size < frame.size() - 3; ++size) {
        if (protocol::parse_party_state(frame.data() + 3, size, snapshot)) {
            expect(false, "truncated snapshot rejected");
        }
    }
    state.members[1].id = 1;
    frame = protocol::frame_party_state(state);
    expect(!protocol::parse_party_state(frame.data() + 3, frame.size() - 3, snapshot),
           "duplicate member IDs rejected");
    return test::g_failures ? 1 : 0;
}
