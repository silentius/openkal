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
#include "openkal/protocol/Chatting.h"
#include "openkal/protocol/CreatePlayer.h"
#include "openkal/protocol/FrameParser.h"
#include "openkal/protocol/Handshake.h"
#include "openkal/protocol/Move.h"
#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/Teleport.h"
#include "openkal/server/ServerHost.h"
#include "openkal/server/store/DataStore.h"
#include "openkal/terrain/IndoorCollision.h"
#include "test_util.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>
#include <unistd.h>

namespace {
    using namespace openkal;

    // Project-owned synthetic MAP/GB fixture: one triangular floor, no external assets.
    bool write_dungeon(const std::string& root) {
        std::vector<std::uint8_t> bytes(60, 0);
        bytes[0] = 2;
        bytes[1] = 1;
        const char names[] = "room.map\0floor.gb\0";
        const std::uint32_t size = sizeof(names);
        std::memcpy(bytes.data() + 12, &size, sizeof(size));
        test::append_bytes(bytes, names, size);
        test::append_pod(bytes, std::uint32_t{9});
        test::append_pod(bytes, std::uint32_t{0});
        for (float value : {10.0f, 100.0f, 10.0f, 0.0f, 0.0f, 0.0f}) {
            test::append_pod(bytes, value);
        }
        for (int i = 0; i < 3; ++i) {
            test::append_pod(bytes, std::uint32_t{0});
        }
        return test::write_bytes(root + "/room.map", bytes) &&
               test::write_cls_only_gb_v11(root + "/floor.gb");
    }

    bool wait_frame(server::ISession& peer,
                    std::vector<std::uint8_t>& rx,
                    protocol::S2C expected,
                    std::vector<std::uint8_t>& payload) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (std::chrono::steady_clock::now() < deadline) {
            if (!peer.recv_into(rx)) {
                return false;
            }
            for (;;) {
                std::size_t consumed = 0;
                std::uint8_t type = 0;
                const auto status =
                    protocol::FrameParser::pop(rx.data(), rx.size(), consumed, type, payload);
                if (status == protocol::FrameParseStatus::Incomplete) {
                    break;
                }
                if (status == protocol::FrameParseStatus::Invalid) {
                    return false;
                }
                rx.erase(rx.begin(), rx.begin() + consumed);
                if (type == static_cast<std::uint8_t>(expected)) {
                    return true;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return false;
    }

    bool login(server::ISession& peer,
               std::vector<std::uint8_t>& rx,
               protocol::CreatePlayerFields& player) {
        std::vector<std::uint8_t> payload;
        protocol::S2CCodeFields code;
        return peer.send_all(protocol::Handshake::build_connect(0)) &&
               wait_frame(peer, rx, protocol::S2C::Code, payload) &&
               protocol::Handshake::parse_code(payload.data(), payload.size(), code) &&
               peer.send_all(protocol::Handshake::build_answer(code.protocol_test_value)) &&
               wait_frame(peer, rx, protocol::S2C::CreatePlayer, payload) &&
               protocol::CreatePlayer::parse(payload.data(), payload.size(), player);
    }
}  // namespace

int main() {
    using namespace openkal;
    using test::expect;
    const auto root =
        test::temp_dir(("dungeon_server_" + std::to_string(getpid()) + "_" +
                        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))
                           .c_str());
    expect(write_dungeon(root), "write synthetic dungeon");
    terrain::IndoorCollision collision;
    expect(collision.load(root, root + "/room.map", 0, 0), "load synthetic dungeon collision");
    if (collision.floor()) {
        expect(terrain::IndoorCollision::standing(*collision.floor(), {2, 0, 2}),
               "loaded collision-only floor supports a player");
    }
    expect(collision.load(root, root + "/room.map", 6, 4), "load shifted dungeon collision");
    if (collision.floor()) {
        expect(terrain::IndoorCollision::standing(*collision.floor(), {49154, 0, 32770}),
               "server geometry uses catalog cell origin");
        expect(!terrain::IndoorCollision::standing(*collision.floor(), {2, 0, 2}),
               "unshifted coordinates cannot occupy a shifted dungeon");
    }
    test::mkdir_p(root + "/monsters");
    std::ofstream(root + "/monsters/1.json") << R"({"index":1,"name":"Fixture",
        "stats":{"race":0,"level":1,"hp":10,"attack":{"min":1,"max":1}},
        "spawns":[{"map":0,"area":1,"max":1,"cycle":1,"rect":[480, 480, 490, 490]}]})";
    std::ofstream(root + "/mapinfo.json") << R"({"maps":[
        {"index":1,"kind":1,"xy":[0, 0],"mapname":"room"}]})";
    test::mkdir_p(root + "/areas");
    test::mkdir_p(root + "/items");
    std::ofstream(root + "/items/1.json")
        << R"({"index":1,"name":"Fixture Sword","class":["weapon","sword"]})";
    std::ofstream(root + "/items/193.json")
        << R"({"index":193,"name":"Fixture city scroll","class":["general","teleport"],"use":1,"stackable":1,"specialty":{"teleport":{"loc":1}}})";
    expect(test::write_open_area(root + "/areas/n_000_000.akarea"),
           "write overworld recovery area");
    server::ServerConfig config;
    config.maps_dir = root + "/areas";
    config.monsters_dir = root + "/monsters";
    config.items_dir = root + "/items";
    config.save_dir = root + "/save";
    config.indoor_geometry_root = root;
    config.map_catalog_config = root + "/mapinfo.json";
    config.spawn_x = 480;
    config.spawn_y = 480;
    config.spawn_z = 100;
    config.unix_path = root + "/server.sock";
    config.max_monsters = 1;
    auto store = server::create_data_store(server::StoreBackend::Sqlite);
    expect(store->open(config.save_dir), "open dungeon fixture store");
    server::CharacterRecord character;
    character.name = "DungeonTester";
    character.map = 1;
    character.map_x = 2;
    character.map_y = 2;
    character.map_z = 0;
    expect(store->ensure_default_player("dungeon", "dungeon", character), "create indoor save");
    const auto account = store->find_account("dungeon");
    if (!account) {
        return 1;
    }
    const auto characters = store->list_characters(account->id);
    if (characters.empty()) {
        return 1;
    }
    character = characters.front();
    config.character_id = character.id;
    server::ItemRecord scroll;
    scroll.iid = 1;
    scroll.character_id = character.id;
    scroll.index = 193;
    scroll.count = 1;
    expect(store->insert_item(scroll), "provide the scroll used by the teleport height test");
    store->close();

    for (int scenario = 0; scenario < 3; ++scenario) {
        if (scenario != 0) {
            expect(store->open(config.save_dir), "reopen dungeon save");
            character.map = 1;
            character.map_x = 2;
            character.map_y = 2;
            character.map_z = scenario == 1 ? 10000 : 0;
            expect(store->save_character(character), "write invalid or unavailable saved pose");
            store->close();
        }
        if (scenario == 2) {
            std::filesystem::remove(root + "/floor.gb");
            expect(!collision.load(root, root + "/room.map", 0, 0) && !collision.floor(),
                   "missing region geometry fails closed and clears old collision");
        }
        server::ServerHost host;
        if (!host.start(config)) {
            expect(false, "start dungeon server");
            break;
        }
        std::unique_ptr<server::ISession> peer;
        for (int attempt = 0; attempt < 100 && !peer; ++attempt) {
            peer = server::connect_unix(config.unix_path);
            if (!peer) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }
        expect(peer != nullptr, "connect dungeon peer");
        if (!peer) {
            host.stop();
            break;
        }
        std::vector<std::uint8_t> rx, payload;
        protocol::CreatePlayerFields player;
        expect(login(*peer, rx, player), "log in dungeon character");
        if (scenario != 0) {
            expect(player.x == config.spawn_x && player.y == config.spawn_y &&
                       player.z == config.spawn_z,
                   "invalid or unverifiable saved dungeon position recovers to spawn");
        } else {
            expect(player.x == 2 && player.y == 2 && player.z == 0,
                   "valid saved dungeon position remains unchanged");
            for (const auto& packet : {protocol::Move::frame_client_on(0, 0, 127),
                                       protocol::Move::frame_client_on(0, 0, -127),
                                       protocol::Move::frame_client_on(30, 0, 0)}) {
                expect(peer->send_all(packet), "send malicious movement packet");
                expect(wait_frame(*peer, rx, protocol::S2C::MoveBefore, payload),
                       "server rejects height or outside movement");
                std::uint32_t id = 0;
                int x = 0, y = 0, z = 0;
                std::uint16_t dir = 0;
                expect(protocol::Move::parse_before(
                           payload.data(), payload.size(), id, x, y, z, dir) &&
                           x == 2 && y == 2 && z == 0,
                       "rejection preserves complete authoritative pose");
            }
            expect(peer->send_all(protocol::Chatting::frame_c2s("/fly")),
                   "request forbidden dungeon fly");
            expect(peer->send_all(protocol::Move::frame_client_on(127, 0, 0)),
                   "try fly movement bypass");
            expect(wait_frame(*peer, rx, protocol::S2C::MoveBefore, payload),
                   "fly command cannot bypass dungeon movement validation");
            expect(peer->send_all(protocol::Move::frame_client_on(1, 0, 0)),
                   "send normal movement");
            expect(wait_frame(*peer, rx, protocol::S2C::MovePlayerOn, payload),
                   "server accepts normal movement");
            // A city teleport is server-selected; the ACK must not choose an arbitrary height.
            expect(peer->send_all(protocol::Teleport::frame_client_request(1)),
                   "request city teleport");
            expect(wait_frame(*peer, rx, protocol::S2C::Teleport, payload),
                   "receive authoritative teleport");
            std::uint8_t map = 0, cheat = 0;
            int x = 0, y = 0, z = 0;
            expect(protocol::Teleport::parse_server(
                       payload.data(), payload.size(), map, x, y, z, cheat),
                   "parse teleport destination");
            expect(peer->send_all(protocol::Teleport::frame_client_ack(1, 999999)),
                   "forge teleport ACK height");
            expect(peer->send_all(protocol::Move::frame_client_on(127, 0, 0)),
                   "request correction after teleport");
            expect(wait_frame(*peer, rx, protocol::S2C::MoveBefore, payload),
                   "receive teleport pose correction");
            std::uint32_t id = 0;
            int actual_x = 0, actual_y = 0, actual_z = 0;
            std::uint16_t dir = 0;
            expect(protocol::Move::parse_before(
                       payload.data(), payload.size(), id, actual_x, actual_y, actual_z, dir) &&
                       actual_z == z && actual_z != 999999,
                   "teleport ACK cannot override authoritative height");
        }
        peer->close();
        host.stop();
    }
    std::filesystem::remove_all(root);
    return test::g_failures == 0 ? 0 : 1;
}
