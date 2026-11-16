/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "fixture_util.h"
#include "openkal/protocol/Attack.h"
#include "openkal/protocol/Chatting.h"
#include "openkal/protocol/CreatePlayer.h"
#include "openkal/protocol/FrameParser.h"
#include "openkal/protocol/Handshake.h"
#include "openkal/protocol/InsertItem.h"
#include "openkal/protocol/Move.h"
#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketReader.h"
#include "openkal/protocol/PacketWriter.h"
#include "openkal/protocol/Party.h"
#include "openkal/protocol/Property.h"
#include "openkal/server/ServerHost.h"
#include "test_util.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <thread>
#include <unistd.h>
#include <unordered_map>

namespace {
    using namespace openkal;
    struct Peer {
        std::unique_ptr<server::ISession> socket;
        std::vector<std::uint8_t> rx;
        std::uint32_t id = 0;
        std::uint32_t challenge = 0;
        bool challenged = false;
        protocol::PartyState party;
        std::vector<protocol::PartyItemNotice> party_items;
        std::vector<std::string> chat;
        bool heard(std::string_view text) const {
            return std::find(chat.begin(), chat.end(), text) != chat.end();
        }
        std::int64_t earned_exp = 0;
        int x = 0;
        int money = 0;
        std::vector<int> geon_receipts;
        int tokens = 0;
        std::unordered_map<int, std::uint32_t> drops;
        void pump() {
            if (!socket || !socket->recv_into(rx)) {
                return;
            }
            for (;;) {
                std::size_t consumed = 0;
                std::uint8_t type = 0;
                std::vector<std::uint8_t> payload;
                if (protocol::FrameParser::pop(rx.data(), rx.size(), consumed, type, payload) !=
                    protocol::FrameParseStatus::Complete) {
                    break;
                }
                rx.erase(rx.begin(), rx.begin() + static_cast<std::ptrdiff_t>(consumed));
                protocol::PacketReader r(payload);
                if (type == static_cast<int>(protocol::S2C::Code)) {
                    protocol::S2CCodeFields code;
                    if (protocol::Handshake::parse_code(payload.data(), payload.size(), code)) {
                        challenge = code.protocol_test_value;
                        challenged = true;
                    }
                } else if (type == static_cast<int>(protocol::S2C::CreatePlayer)) {
                    protocol::CreatePlayerFields fields;
                    if (protocol::CreatePlayer::parse(payload.data(), payload.size(), fields) &&
                        (fields.class_byte & 0x80)) {
                        id = fields.id;
                        x = fields.x;
                    }
                } else if (type == static_cast<int>(protocol::S2C::MovePlayerEnd)) {
                    std::uint32_t player = 0;
                    std::uint8_t dx = 0;
                    if (r.read_u32(player) && r.read_u8(dx) && player == id) {
                        x += static_cast<std::int8_t>(dx);
                    }
                } else if (type == static_cast<int>(protocol::S2C::Chatting)) {
                    std::string speaker, text;
                    if (protocol::Chatting::parse_s2c(
                            payload.data(), payload.size(), speaker, text)) {
                        chat.push_back(std::move(text));
                    }
                } else if (type == static_cast<int>(protocol::S2C::PartyInfo)) {
                    (void) protocol::parse_party_state(payload.data(), payload.size(), party);
                } else if (type == static_cast<int>(protocol::S2C::UpdateParty)) {
                    protocol::PartyItemNotice notice;
                    if (protocol::parse_party_item(payload.data(), payload.size(), notice)) {
                        party_items.push_back(std::move(notice));
                    }
                } else if (type == static_cast<int>(protocol::S2C::UpdateProperty)) {
                    protocol::UpdatePropertyFields fields;
                    if (protocol::Property::parse_update_any(
                            payload.data(), payload.size(), fields) &&
                        fields.kind == protocol::UpdatePropertyKind::Exp) {
                        earned_exp += fields.exp_delta;
                    }
                } else if (type == static_cast<int>(protocol::S2C::CreateItem)) {
                    std::uint16_t index = 0;
                    std::uint32_t item = 0;
                    if (r.read_u16(index) && r.read_u32(item)) {
                        drops[index] = item;
                    }
                } else if (type == static_cast<int>(protocol::S2C::InsertItem)) {
                    std::uint32_t iid = 0, info = 0;
                    std::uint16_t index = 0;
                    std::uint8_t prefix = 0;
                    std::int32_t count = 0;
                    if (r.read_u32(iid) && r.read_u16(index) && r.read_u8(prefix) &&
                        r.read_u32(info) && r.read_i32(count)) {
                        if (index == 31) {
                            if ((info & protocol::kInsertReplace) != 0) {
                                money = count;
                            } else {
                                money += count;
                                if (count > 0) {
                                    geon_receipts.push_back(count);
                                }
                            }
                        }
                        if (index == 9000) {
                            tokens += count;
                        }
                    }
                }
            }
        }
        void command(protocol::PartyCommand command,
                     std::uint32_t target = 0,
                     std::uint32_t token = 0,
                     std::uint8_t value = 0) {
            test::expect(socket->send_all(
                             protocol::frame_party_request({command, target, token, value, {}})),
                         "send party command over socket");
        }
        void pickup(std::uint32_t item) {
            protocol::PacketWriter w;
            w.write_u32(item);
            w.write_u32(15);
            w.write_u32(15);
            test::expect(
                socket->send_all(w.frame(static_cast<std::uint8_t>(protocol::C2S::PickupItem))),
                "send loot pickup");
        }
    };
}  // namespace
int main() {
    using namespace openkal;
    using test::expect;
    const auto root = test::temp_dir(("party_server_" + std::to_string(getpid())).c_str());
    server::ServerConfig config;
    config.maps_dir = root + "/maps";
    config.monsters_dir = root + "/monsters";
    config.items_dir = root + "/items";
    config.groups_dir = root + "/groups";
    config.itemgroups_dir = root + "/itemgroups";
    config.save_dir = root + "/save";
    config.unix_path = root + "/game.sock";
    config.store = server::StoreBackend::Sqlite;
    config.transport = server::TransportKind::Unix;
    config.spawn_x = config.spawn_y = 15 * map::kTileSize;
    config.map_radius_cells = 1;
    config.max_monsters = 3;
    for (const auto& dir : {config.maps_dir,
                            config.monsters_dir,
                            config.items_dir,
                            config.groups_dir,
                            config.itemgroups_dir}) {
        test::mkdir_p(dir);
    }
    expect(test::write_open_area(config.maps_dir + "/n_000_000.akarea"),
           "create owned party map fixture");
    std::ofstream(config.monsters_dir + "/1.json")
        << R"({"index":1,"name":"Party test monster","stats":{"race":0,"level":1,"hp":1,"exp":100,"ai":0,"itemGroups":[[1, 1], [2, 1]]},"spawns":[{"map":0,"area":1,"max":3,"cycle":3600,"rect":[15, 15, 16, 15]}]})";
    std::ofstream(config.items_dir + "/9000.json")
        << R"({"index":9000,"name":"Party test token","class":["general","etc"],"stackable":1})";
    std::ofstream(config.groups_dir + "/1.json")
        << R"({"index":1,"entries":[{"chance":100,"type":"money","amount":101}]})";
    std::ofstream(config.groups_dir + "/2.json")
        << R"({"index":2,"entries":[{"chance":100,"type":"item","item":9000,"count":1}]})";
    std::ofstream(config.itemgroups_dir + "/1.json")
        << R"({"index":1,"entries":[{"chance":100,"group":1}]})";
    std::ofstream(config.itemgroups_dir + "/2.json")
        << R"({"index":2,"entries":[{"chance":100,"group":2}]})";
    std::ofstream(config.items_dir + "/1.json")
        << R"({"index":1,"name":"Party test sword","wear":1,"class":["weapon","sword"],"specialty":{"attack":{"min":500,"max":500},"hit":1000,"hp":10000}})";
    std::ofstream(config.itemgroups_dir + "/1000000.json") << R"({"index":1000000,"entries":[]})";
    auto store = server::create_data_store(server::StoreBackend::Sqlite);
    expect(store->open(config.save_dir), "open synthetic party store");
    server::CharacterRecord hero;
    hero.name = "Party Tester";
    hero.hp = 100;
    hero.mp = 50;
    hero.money = 200;
    hero.map_x = hero.map_y = config.spawn_x;
    hero.weapon_index = 1;
    expect(store->ensure_default_player("party", "party", hero), "seed armed party test character");
    const auto account = store->find_account("party");
    if (!account) {
        return 1;
    }
    const auto characters = store->list_characters(account->id);
    if (characters.empty()) {
        return 1;
    }
    config.character_id = characters.front().id;
    server::ItemRecord sword;
    sword.iid = 1;
    sword.character_id = config.character_id;
    sword.index = 1;
    sword.count = 1;
    sword.slot = 1;
    expect(store->insert_item(sword), "equip synthetic party test sword");
    store->close();
    server::ServerHost host;
    if (!host.start(config)) {
        expect(false, "start party server");
        return 1;
    }
    for (int i = 0; i < 200 && !std::filesystem::exists(config.unix_path); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    Peer a, b, outsider;
    const auto wait = [&](const std::function<bool()>& predicate, int milliseconds = 4000) {
        const auto deadline =
            std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
        do {
            a.pump();
            b.pump();
            outsider.pump();
            if (predicate()) {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        } while (std::chrono::steady_clock::now() < deadline);
        return false;
    };
    for (auto* peer : {&a, &b, &outsider}) {
        peer->socket = server::connect_unix(config.unix_path);
        if (!peer->socket) {
            expect(false, "connect party peer");
            host.stop();
            return 1;
        }
        expect(peer->socket->send_all(protocol::Handshake::build_connect(0)),
               "start party peer handshake");
        expect(wait([&] { return peer->challenged; }), "receive challenge");
        expect(peer->socket->send_all(protocol::Handshake::build_answer(peer->challenge)),
               "answer party peer challenge");
        expect(wait([&] { return peer->id != 0; }), "enter world with independent player ID");
    }
    expect(wait([&] { return a.money == 200 && b.money == 200 && outsider.money == 200; }),
           "login restores each starting balance absolutely");
    expect(a.geon_receipts.empty() && b.geon_receipts.empty() && outsider.geon_receipts.empty(),
           "login balance is never reported as freshly collected Geon");
    const auto a_start_money = a.money, b_start_money = b.money;
    const auto outsider_start_money = outsider.money;
    b.command(protocol::PartyCommand::Reply, 0, 1234, 1);
    expect(wait([&] { return a.party.id == 0 && b.party.id == 0; }),
           "unsolicited acceptance creates no party");
    a.command(protocol::PartyCommand::Invite, b.id);
    if (!wait([&] { return b.party.invitation != 0; })) {
        expect(false, "receive party invitation");
        host.stop();
        return 1;
    }
    b.command(protocol::PartyCommand::Reply, 0, b.party.invitation, 1);
    expect(wait([&] {
               return a.party.id != 0 && a.party.id == b.party.id && a.party.members.size() == 2;
           }),
           "both clients receive shared roster");
    expect(a.party.loot == protocol::PartyLoot::Random &&
               b.party.loot == protocol::PartyLoot::Random,
           "party defaults to automatic item sharing without a loot-mode command");
    expect(a.socket->send_all(protocol::Chatting::frame_c2s("#Party only")), "send party chat");
    expect(wait([&] { return a.heard("#Party only") && b.heard("#Party only"); }),
           "party chat reaches sender and member");
    (void) wait([] { return false; }, 80);
    expect(!outsider.heard("#Party only"), "party chat excludes outsiders");
    expect(outsider.socket->send_all(protocol::Chatting::frame_c2s("^Server announcement")),
           "send shout chat");
    expect(wait([&] {
               return a.heard("^Server announcement") && b.heard("^Server announcement") &&
                      outsider.heard("^Server announcement");
           }),
           "shout reaches every playing client including sender");
    std::uint32_t monster = 0;
    expect(wait([&] {
               for (const auto& m : host.monster_snapshot()) {
                   if (m.hp > 0) {
                       monster = m.id;
                       return true;
                   }
               }
               return false;
           }),
           "spawn reward test monster");
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(12);
    while (a.drops.empty() && std::chrono::steady_clock::now() < deadline) {
        expect(a.socket->send_all(protocol::Attack::frame_client(protocol::kCkMonster, monster)),
               "attack party reward monster");
        (void) wait([&] { return !a.drops.empty(); }, 900);
    }
    expect(wait([&] {
               return a.drops.count(31) && a.drops.count(9000) && a.earned_exp > 0 &&
                      b.earned_exp > 0;
           }),
           "kill gives nearby non-attacker EXP and spawns loot");
    if (!a.drops.count(31) || !a.drops.count(9000) || a.earned_exp <= 0 || b.earned_exp <= 0) {
        host.stop();
        return 1;
    }
    expect(a.earned_exp == b.earned_exp && outsider.earned_exp == 0,
           "party EXP shared equally without rewarding outsider");
    outsider.pickup(a.drops[31]);
    (void) wait([] { return false; }, 100);
    expect(outsider.money == outsider_start_money, "outsider cannot steal protected party Geon");
    b.pickup(a.drops[31]);
    expect(wait([&] { return a.money + b.money == a_start_money + b_start_money + 101; }),
           "Geon split preserves integer remainder");
    expect(a.money > a_start_money && b.money > b_start_money &&
               outsider.money == outsider_start_money,
           "both nearby members receive Geon");
    expect(a.geon_receipts.size() == 1 && b.geon_receipts.size() == 1 &&
               a.geon_receipts.front() == a.money - a_start_money &&
               b.geon_receipts.front() == b.money - b_start_money &&
               a.geon_receipts.front() + b.geon_receipts.front() == 101 &&
               outsider.geon_receipts.empty(),
           "each party member receives exactly their own share as a visible money gain");
    b.pickup(a.drops[31]);
    (void) wait([] { return false; }, 100);
    expect(a.geon_receipts.size() == 1 && b.geon_receipts.size() == 1 &&
               a.money + b.money == a_start_money + b_start_money + 101,
           "repeated Geon pickup duplicates neither money nor its receipt");
    expect(a.party_items.empty() && b.party_items.empty() && outsider.party_items.empty(),
           "Geon sharing does not produce party item notices");
    b.pickup(a.drops[9000]);
    expect(wait([&] { return a.tokens + b.tokens == 1; }),
           "random item awarded once to eligible member");
    const bool received_notice =
        wait([&] { return a.party_items.size() + b.party_items.size() == 1; });
    expect(received_notice, "only the other member receives an item award notice");
    if (!received_notice) {
        host.stop();
        return 1;
    }
    const auto& observer = a.party_items.empty() ? b : a;
    const auto& receiver = a.tokens ? a : b;
    const auto& notice = observer.party_items.back();
    expect(notice.recipient == receiver.id && !notice.recipient_name.empty() &&
               notice.item_index == 9000 && notice.count == 1 &&
               notice.party == receiver.party.id && receiver.party_items.empty() &&
               outsider.party_items.empty(),
           "party item notice identifies actual random recipient and excludes recipient and "
           "outsiders");
    b.pickup(a.drops[9000]);
    (void) wait([] { return false; }, 100);
    expect(a.tokens + b.tokens == 1 && a.party_items.size() + b.party_items.size() == 1,
           "replayed pickup cannot duplicate party loot or its notice");
    const auto first_round_drop = a.drops[9000];
    const auto first_round_exp = a.earned_exp;
    expect(wait([&] {
               for (const auto& m : host.monster_snapshot()) {
                   if (m.hp > 0) {
                       monster = m.id;
                       return true;
                   }
               }
               return false;
           }),
           "find second balanced-distribution monster");
    const auto balanced_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(12);
    while (a.earned_exp == first_round_exp &&
           std::chrono::steady_clock::now() < balanced_deadline) {
        expect(a.socket->send_all(protocol::Attack::frame_client(protocol::kCkMonster, monster)),
               "attack next balanced-distribution monster");
        (void) wait([&] { return a.earned_exp > first_round_exp; }, 900);
    }
    expect(wait([&] { return a.drops[9000] != first_round_drop; }),
           "second kill supplies a fresh item drop");
    b.pickup(a.drops[9000]);
    expect(wait([&] { return a.tokens == 1 && b.tokens == 1; }),
           "same picker distributes two drops exactly 50/50 to two nearby members");
    expect(wait([&] { return a.party_items.size() == 1 && b.party_items.size() == 1; }) &&
               outsider.party_items.empty(),
           "each member sees the other member's one awarded drop");
    // A member can stay in the party while being too far away to share a later kill.
    for (int i = 0; i < 150; ++i) {
        const auto next_step = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
        expect(b.socket->send_all(protocol::Move::frame_client_end(6, 0, 0)),
               "move member beyond sharing radius");
        a.pump();
        b.pump();
        outsider.pump();
        std::this_thread::sleep_until(next_step);
    }
    expect(wait([&] { return b.x >= config.spawn_x + 900; }),
           "server acknowledges distant member position");
    // Fast synthetic combat may finish before the previous party chat's cooldown.
    (void) wait([] { return false; }, 510);
    expect(a.socket->send_all(protocol::Chatting::frame_c2s("#Distant party")),
           "send party chat beyond sight range");
    expect(wait([&] { return b.heard("#Distant party"); }) && !outsider.heard("#Distant party"),
           "party chat reaches distant members without reaching outsiders");
    expect(b.socket->send_all(protocol::Chatting::frame_c2s("/s Distant shout")),
           "send slash shout beyond sight range");
    expect(wait([&] { return a.heard("^Distant shout") && outsider.heard("^Distant shout"); }),
           "slash shout reaches peers beyond sight range with shout marker");
    const auto first_money_drop = a.drops[31], first_item_drop = a.drops[9000];
    const auto a_exp_before = a.earned_exp, b_exp_before = b.earned_exp;
    const auto a_money_before = a.money, b_money_before = b.money;
    const auto a_tokens_before = a.tokens, b_tokens_before = b.tokens;
    const auto a_notices_before = a.party_items.size(), b_notices_before = b.party_items.size();
    a.command(protocol::PartyCommand::Loot, 0, 0, 0);
    expect(wait([&] { return a.party.loot == protocol::PartyLoot::FreeForAll; }),
           "explicit Free for All remains available");
    expect(wait([&] {
               for (const auto& m : host.monster_snapshot()) {
                   if (m.hp > 0) {
                       monster = m.id;
                       return true;
                   }
               }
               return false;
           }),
           "find second reward monster");
    const auto second_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(12);
    while (a.earned_exp == a_exp_before && std::chrono::steady_clock::now() < second_deadline) {
        expect(a.socket->send_all(protocol::Attack::frame_client(protocol::kCkMonster, monster)),
               "attack second party monster");
        (void) wait([&] { return a.earned_exp > a_exp_before; }, 900);
    }
    expect(a.earned_exp > a_exp_before && b.earned_exp == b_exp_before,
           "distant party member receives no EXP");
    expect(
        wait([&] { return a.drops[31] != first_money_drop && a.drops[9000] != first_item_drop; }),
        "receive second kill's fresh ground item IDs");
    a.pickup(a.drops[31]);
    expect(wait([&] { return a.money == a_money_before + 101; }),
           "nearby member receives full Geon when alone in range");
    expect(b.money == b_money_before && b.geon_receipts.size() == 1,
           "distant party member receives no Geon or receipt");
    expect(a.geon_receipts.size() == 2 && a.geon_receipts.back() == 101,
           "pickup with only one nearby recipient reports the complete Geon amount");
    a.pickup(a.drops[9000]);
    expect(wait([&] { return a.tokens == a_tokens_before + 1; }),
           "Free for All awards item to picker");
    expect(b.tokens == b_tokens_before, "Free for All does not move item to another member");
    expect(wait([&] { return b.party_items.size() == b_notices_before + 1; }) &&
               a.party_items.size() == a_notices_before && outsider.party_items.empty() &&
               b.party_items.back().recipient == a.id,
           "Free for All notice reaches distant party member without self or outsider notice");
    b.command(protocol::PartyCommand::Kick, a.id);
    (void) wait([] { return false; }, 100);
    expect(a.party.id != 0 && a.party.id == b.party.id,
           "member cannot kick leader over the network");
    a.command(protocol::PartyCommand::Kick, b.id);
    expect(wait([&] { return a.party.id == 0 && b.party.id == 0; }),
           "leader kick updates both rosters and dissolves two-member party");
    (void) wait([] { return false; }, 1100);
    a.command(protocol::PartyCommand::Invite, b.id);
    expect(wait([&] { return b.party.invitation != 0; }), "invite removed member again");
    b.command(protocol::PartyCommand::Reply, 0, b.party.invitation, 1);
    expect(wait([&] { return a.party.id != 0 && a.party.id == b.party.id; }),
           "removed member may rejoin through a fresh invitation");
    b.command(protocol::PartyCommand::Leave);
    expect(wait([&] { return a.party.id == 0 && b.party.id == 0; }),
           "leave command updates both rosters");
    expect(a.socket->send_all(protocol::Chatting::frame_c2s("#After leaving")),
           "request party chat after leaving");
    expect(wait([&] { return a.heard("You are not in a party."); }),
           "former member cannot send party chat after group dissolution");
    expect(!b.heard("#After leaving") && !outsider.heard("#After leaving"),
           "unaffiliated party message never falls back to public chat");
    const int solo_money_before = a.money;
    a.pickup(first_money_drop);
    expect(wait([&] { return a.money == solo_money_before + 101; }),
           "solo player collects the remaining protected Geon after leaving");
    expect(a.geon_receipts.size() == 3 && a.geon_receipts.back() == 101 &&
               b.geon_receipts.size() == 1 && outsider.geon_receipts.empty(),
           "solo pickup reports its own amount without notifying other players");
    host.stop();
    std::filesystem::remove_all(root);
    return test::g_failures ? 1 : 0;
}
