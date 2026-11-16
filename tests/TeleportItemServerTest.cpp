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
#include "openkal/map/Constants.h"
#include "openkal/map/Coords.h"
#include "openkal/map/Teleport.h"
#include "openkal/protocol/FrameParser.h"
#include "openkal/protocol/GameExit.h"
#include "openkal/protocol/Handshake.h"
#include "openkal/protocol/InsertItem.h"
#include "openkal/protocol/Move.h"
#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketReader.h"
#include "openkal/protocol/Teleport.h"
#include "openkal/protocol/UseItem.h"
#include "openkal/server/ServerHost.h"
#include "openkal/server/net/Transport.h"
#include "openkal/server/store/DataStore.h"
#include "test_util.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <thread>
#include <unistd.h>

namespace {
    using namespace openkal;
    using test::expect;

    bool wait_frame(server::ISession& session,
                    std::vector<std::uint8_t>& rx,
                    protocol::S2C expected,
                    std::vector<std::uint8_t>& payload) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        do {
            if (!session.recv_into(rx)) {
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
                rx.erase(rx.begin(), rx.begin() + static_cast<std::ptrdiff_t>(consumed));
                if (type == static_cast<std::uint8_t>(expected)) {
                    return true;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        } while (std::chrono::steady_clock::now() < deadline);
        return false;
    }
}  // namespace

int main() {
    using namespace openkal;
    const auto root = test::temp_dir(("teleport_item_" + std::to_string(getpid())).c_str());
    const auto destination = map::kTeleportLocs[0];
    server::ServerConfig config;
    config.maps_dir = root + "/maps";
    config.items_dir = root + "/items";
    config.monsters_dir = root + "/monsters";
    config.portals_path = root + "/portals.json";
    config.save_dir = root + "/save";
    config.unix_path = root + "/game.sock";
    config.store = server::StoreBackend::Sqlite;
    config.transport = server::TransportKind::Unix;
    config.spawn_x = destination.x + 40;
    config.spawn_y = destination.y;
    config.spawn_z = destination.z;
    config.map_radius_cells = 1;
    config.max_monsters = 0;
    for (const auto& dir : {config.maps_dir, config.items_dir, config.monsters_dir}) {
        test::mkdir_p(dir);
    }
    std::ostringstream area;
    area << config.maps_dir << "/n_" << std::setw(3) << std::setfill('0')
         << destination.x / map::kWorldSize << "_" << std::setw(3)
         << destination.y / map::kWorldSize << ".akarea";
    expect(test::write_open_area(area.str()), "write owned destination map fixture");
    int portal_x = 0, portal_y = 0;
    coords::pixel_to_tile(destination.x + 4 * map::kTileSize, destination.y, portal_x, portal_y);
    const int portal_destination_x = destination.x - 64;
    std::ofstream(config.portals_path)
        << "{\"portals\":[{\"id\":\"movement-test\",\"fromMap\":" << destination.map
        << ",\"fromRect\":[" << portal_x << ", " << portal_y << ", " << portal_x + 1 << ", "
        << portal_y + 1 << "],\"toMap\":" << destination.map << ",\"to\":[" << portal_destination_x
        << ", " << destination.y << ", " << destination.z << "]}]}";
    std::ofstream(config.monsters_dir + "/1.json")
        << R"({"index":1,"stats":{"level":1,"hp":1},"spawns":[]})";
    std::ofstream(config.items_dir + "/192.json")
        << R"({"index":192,"name":"Test city scroll","class":["general","teleport"],"use":1,"stackable":1,"specialty":{"teleport":{"loc":0}}})";
    std::ofstream(config.items_dir + "/193.json")
        << R"({"index":193,"name":"Invalid destination","class":["general","teleport"],"use":1,"stackable":1,"specialty":{"teleport":{"loc":99}}})";
    auto store = server::create_data_store(server::StoreBackend::Sqlite);
    expect(store && store->open(config.save_dir), "open scroll persistence");
    server::CharacterRecord hero;
    hero.name = "ScrollTester";
    hero.map_x = config.spawn_x;
    hero.map_y = config.spawn_y;
    hero.map_z = config.spawn_z;
    expect(store->ensure_default_player("scroll", "scroll", hero), "create scroll character");
    const auto account = store->find_account("scroll");
    if (!account) {
        return 1;
    }
    hero = store->list_characters(account->id).front();
    config.character_id = hero.id;
    for (int iid : {100, 101, 102}) {
        server::ItemRecord item;
        item.iid = iid;
        item.character_id = hero.id;
        item.index = iid == 102 ? 193 : 192;
        item.count = iid == 100 ? 2 : 1;
        item.character_bound = iid == 100;
        expect(store->insert_item(item), "seed independent bound and ordinary scroll instances");
    }
    store->close();
    server::ServerHost host;
    std::unique_ptr<server::ISession> session;
    std::vector<std::uint8_t> rx, payload;
    const auto connect = [&] {
        expect(host.start(config), "start scroll server");
        for (int n = 0; n < 200 && !std::filesystem::exists(config.unix_path); ++n) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        session = server::connect_unix(config.unix_path);
        if (!session) {
            return false;
        }
        rx.clear();
        protocol::S2CCodeFields code;
        return session->send_all(protocol::Handshake::build_connect(0)) &&
               wait_frame(*session, rx, protocol::S2C::Code, payload) &&
               protocol::Handshake::parse_code(payload.data(), payload.size(), code) &&
               session->send_all(protocol::Handshake::build_answer(code.protocol_test_value)) &&
               wait_frame(*session, rx, protocol::S2C::CreatePlayer, payload);
    };
    const auto barrier = [&] {
        return session->send_all(protocol::GameExit::frame_client_request()) &&
               wait_frame(*session, rx, protocol::S2C::AnsGameExit, payload);
    };
    const auto count = [&](int iid) {
        expect(store->open(config.save_dir), "inspect persisted scroll count");
        const auto rows = store->load_items(hero.id);
        store->close();
        const auto it = std::find_if(
            rows.begin(), rows.end(), [iid](const auto& row) { return row.iid == iid; });
        return it == rows.end() ? 0 : it->count;
    };
    const auto receive_teleport = [&] {
        std::uint8_t map_id = 255, cheat = 255;
        int x = 0, y = 0, z = 0;
        return wait_frame(*session, rx, protocol::S2C::Teleport, payload) &&
               protocol::Teleport::parse_server(
                   payload.data(), payload.size(), map_id, x, y, z, cheat) &&
               map_id == destination.map && x == destination.x && y == destination.y &&
               z == destination.z;
    };
    const auto ack = [&] {
        return session->send_all(protocol::Teleport::frame_client_ack(1, destination.z)) &&
               barrier();
    };
    const auto move = [&](std::int8_t dx) {
        return session->send_all(protocol::Move::frame_client_end(dx, 0, 0)) &&
               wait_frame(*session, rx, protocol::S2C::MovePlayerEnd, payload);
    };
    const auto position = [&](int x) {
        std::uint32_t id = 0;
        int actual_x = 0, y = 0, z = 0;
        std::uint16_t direction = 0;
        return wait_frame(*session, rx, protocol::S2C::MoveBefore, payload) &&
               protocol::Move::parse_before(
                   payload.data(), payload.size(), id, actual_x, y, z, direction) &&
               actual_x == x && y == destination.y && z == destination.z;
    };
    const auto portal_entrance_x = portal_x * map::kTileSize;
    expect(connect() && barrier(), "enter world with scroll inventory");
    if (!session) {
        host.stop();
        return 1;
    }
    auto burst = protocol::Move::frame_client_on(30, 0, 0);
    const auto stop = protocol::Move::frame_client_end(30, 0, 0);
    burst.insert(burst.end(), stop.begin(), stop.end());
    expect(session->send_all(burst) &&
               wait_frame(*session, rx, protocol::S2C::MovePlayerOn, payload) &&
               wait_frame(*session, rx, protocol::S2C::MovePlayerEnd, payload),
           "accept a short burst of ordinary movement");
    const auto extra = protocol::Move::frame_client_on(64, 0, 0);
    burst.clear();
    for (int i = 0; i < 8; ++i) {
        burst.insert(burst.end(), extra.begin(), extra.end());
    }
    expect(session->send_all(burst) && position(config.spawn_x + 60),
           "movement frames share a distance budget and excess movement is corrected");
    expect(session->send_all(protocol::UseItem::frame_c2s(101)) && receive_teleport(),
           "ordinary scroll use requests the configured destination");
    expect(count(101) == 0 && count(100) == 2,
           "consume the exact selected ordinary iid without touching the bound stack");
    expect(session->send_all(protocol::UseItem::frame_c2s(100)) && barrier() && count(100) == 2,
           "duplicate use during a pending teleport cannot consume another scroll");
    expect(session->send_all(protocol::Move::frame_client_end(45, 0, 0)) &&
               session->send_all(protocol::Teleport::frame_client_ack(1, destination.z)) &&
               position(destination.x) && barrier(),
           "pending movement cannot alter the server-selected teleport destination");
    expect(ack() && count(100) == 2, "teleport acknowledgments do not consume items");
    expect(store->open(config.save_dir), "inspect acknowledged teleport position");
    const auto moved = store->load_character(hero.id);
    expect(moved && moved->map_x == destination.x && moved->map_y == destination.y,
           "teleport ack persists the unchanged city-scroll destination");
    store->close();
    expect(move(64) && barrier(), "movement resumes with a fresh budget at the destination");
    expect(store->open(config.save_dir), "inspect movement after teleport");
    const auto walked = store->load_character(hero.id);
    expect(walked && walked->map_x == destination.x + 64,
           "only the post-ack movement advances the destination pose");
    store->close();
    const int to_portal = portal_entrance_x - destination.x - 64;
    const auto movement_refill = std::chrono::milliseconds(1000);
    std::this_thread::sleep_for(movement_refill);
    expect(to_portal > 0 && to_portal <= 64 && move(static_cast<std::int8_t>(to_portal)) &&
               wait_frame(*session, rx, protocol::S2C::Teleport, payload) &&
               session->send_all(protocol::Teleport::frame_client_ack(1, destination.z)) &&
               position(portal_destination_x) && move(64) && barrier(),
           "walking into a field portal resets movement at its destination");
    expect(session->send_all(protocol::UseItem::frame_c2s(102)) && barrier() && count(102) == 1,
           "invalid scroll destinations do not consume the item");
    expect(session->send_all(protocol::Teleport::frame_client_request(1)) && barrier() &&
               count(100) == 2,
           "location-only requests cannot teleport without a matching scroll");
    expect(session->send_all(protocol::Teleport::frame_client_request(0)) &&
               wait_frame(*session, rx, protocol::S2C::InsertItem, payload),
           "legacy scroll request sends an authoritative stack update");
    protocol::PacketReader reader(payload);
    std::uint32_t iid = 0, info = 0, remaining = 0;
    std::uint16_t index = 0;
    std::uint8_t prefix = 0;
    expect(reader.read_u32(iid) && reader.read_u16(index) && reader.read_u8(prefix) &&
               reader.read_u32(info) && reader.read_u32(remaining) && iid == 100 &&
               remaining == 1 && protocol::insert_character_bound(info),
           "bound scroll count update keeps its instance id and character-binding flag");
    expect(receive_teleport() && ack() && count(100) == 1,
           "legacy request also consumes exactly one bound scroll");
    session->close();
    host.stop();
    expect(connect() && barrier() && count(100) == 1 && count(101) == 0,
           "relogin preserves consumed scroll quantities for both binding types");
    expect(session->send_all(protocol::UseItem::frame_c2s(100)) &&
               wait_frame(*session, rx, protocol::S2C::TrashItem, payload) && receive_teleport() &&
               ack() && count(100) == 0,
           "using the last bound scroll deletes the stack and confirms teleport");
    expect(session->send_all(protocol::UseItem::frame_c2s(100)) && barrier() && count(100) == 0,
           "an exhausted iid cannot be reused");
    session->close();
    host.stop();
    std::filesystem::remove_all(root);
    return test::g_failures == 0 ? 0 : 1;
}
