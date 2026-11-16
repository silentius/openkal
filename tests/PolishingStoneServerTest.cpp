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
#include "openkal/protocol/EnchantItem.h"
#include "openkal/protocol/FrameParser.h"
#include "openkal/protocol/GameExit.h"
#include "openkal/protocol/Handshake.h"
#include "openkal/protocol/InsertItem.h"
#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketReader.h"
#include "openkal/server/PrefixEnchant.h"
#include "openkal/server/ServerHost.h"
#include "openkal/server/net/Transport.h"
#include "openkal/server/store/DataStore.h"
#include "test_util.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sqlite3.h>
#include <thread>
#include <unistd.h>

namespace {
    using namespace openkal;
    using openkal::test::expect;

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
    const auto root = test::temp_dir(("polishing_" + std::to_string(getpid())).c_str());
    server::ServerConfig config;
    config.maps_dir = root + "/maps";
    config.monsters_dir = root + "/monsters";
    config.items_dir = root + "/items";
    config.save_dir = root + "/save";
    config.unix_path = root + "/game.sock";
    config.store = server::StoreBackend::Sqlite;
    config.transport = server::TransportKind::Unix;
    config.spawn_x = config.spawn_y = 15 * map::kTileSize;
    config.map_radius_cells = 1;
    config.max_monsters = 0;
    for (const auto& dir : {config.maps_dir, config.items_dir, config.monsters_dir}) {
        test::mkdir_p(dir);
    }
    expect(test::write_open_area(config.maps_dir + "/n_000_000.akarea"), "write polishing map");
    const auto fixture = [](const std::string& path, const char* json) {
        std::ofstream out(path);
        out << json;
        expect(out.good(), "write project-owned polishing fixture");
    };
    fixture(config.monsters_dir + "/1.json",
            R"({"index":1,"name":"Unused","stats":{"level":1,"hp":1},"spawns":[]})");
    fixture(config.items_dir + "/23017.json",
            R"({"index":23017,"name":"Polishing Stone","class":["general","etc"],"stackable":1})");
    fixture(
        config.items_dir + "/9000.json",
        R"({"index":9000,"name":"Fragile sword","class":["weapon","sword"],"wear":1,"level":40,"endurance":30})");
    fixture(
        config.items_dir + "/9001.json",
        R"({"index":9001,"name":"Fragile armor","class":["defense","upperarmor"],"wear":1,"level":40,"endurance":30})");
    fixture(
        config.items_dir + "/9002.json",
        R"({"index":9002,"name":"Guaranteed failed enchantment","class":["general","charm"],"stackable":1,"specialty":{"changeprefix":["weapon",0,0,0,0]}})");
    fixture(
        config.items_dir + "/9003.json",
        R"({"index":9003,"name":"Armor failed enchantment","class":["general","charm"],"stackable":1,"specialty":{"changeprefix":["defense",0,0,0,0]}})");
    fixture(
        config.items_dir + "/9004.json",
        R"({"index":9004,"name":"Guaranteed prefix removal","class":["general","charm"],"stackable":1,"specialty":{"changeprefix":["weapon",100,0,100,0]}})");
    auto store = server::create_data_store(server::StoreBackend::Sqlite);
    expect(store && store->open(config.save_dir), "open polishing persistence");
    server::CharacterRecord hero;
    hero.name = "Polisher";
    hero.level = 40;
    hero.hp = hero.mp = 100;
    hero.map_x = hero.map_y = config.spawn_x;
    expect(store->ensure_default_player("polisher", "polisher", hero), "create polishing player");
    const auto account = store->find_account("polisher");
    if (!account) {
        return 1;
    }
    const auto characters = store->list_characters(account->id);
    if (characters.empty()) {
        return 1;
    }
    config.character_id = characters.front().id;
    for (const auto& item : store->load_items(config.character_id)) {
        expect(store->delete_item(item.iid), "clear starter inventory");
    }
    for (int i = 0; i < 5; ++i) {
        server::ItemRecord item;
        item.character_id = config.character_id;
        item.iid = 100 + i;
        item.index = i == 0 ? server::kPolishingStoneIndex : 8999 + i;
        item.count = (i == 0 || i >= 3) ? 50 : 1;
        if (i == 1 || i == 2) {
            item.cur_end = 1;
            item.max_end = 30;
        }
        expect(store->insert_item(item), "seed polishing target or material");
    }
    server::ItemRecord success_weapon;
    success_weapon.character_id = config.character_id;
    success_weapon.iid = 110;
    success_weapon.index = 9000;
    success_weapon.count = 1;
    success_weapon.prefix = 1;
    success_weapon.cur_end = 9;
    success_weapon.max_end = 30;
    expect(store->insert_item(success_weapon), "seed successful-enchantment target");
    server::ItemRecord success_material;
    success_material.character_id = config.character_id;
    success_material.iid = 111;
    success_material.index = 9004;
    success_material.count = 1;
    expect(store->insert_item(success_material), "seed successful-enchantment material");
    auto copied = store->load_items(config.character_id).front();
    copied.enchant_protected = true;
    expect(store->insert_item(copied), "upsert protected item");
    const auto saved_copy = store->load_items(config.character_id).front();
    expect(saved_copy.enchant_protected, "item upsert preserves protection");
    copied.enchant_protected = false;
    expect(store->update_item(copied), "clear saved protection");
    store->close();
    sqlite3* legacy = nullptr;
    expect(sqlite3_open((config.save_dir + "/" + server::kSqliteFileName).c_str(), &legacy) ==
               SQLITE_OK,
           "open synthetic old-schema database");
    expect(
        sqlite3_exec(
            legacy, "ALTER TABLE items DROP COLUMN enchant_protected", nullptr, nullptr, nullptr) ==
            SQLITE_OK,
        "simulate a database predating protection");
    sqlite3_close(legacy);
    expect(store->open(config.save_dir), "migrate old items even with an existing schema marker");
    for (const auto& item : store->load_items(config.character_id)) {
        expect(!item.enchant_protected, "migration defaults every old item to unprotected");
    }
    store->close();
    server::ServerHost host;
    expect(host.start(config), "start polishing server");
    for (int attempt = 0; attempt < 200 && !std::filesystem::exists(config.unix_path); ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    auto session = server::connect_unix(config.unix_path);
    if (!session) {
        expect(false, "connect polishing client");
        return 1;
    }
    std::vector<std::uint8_t> rx, payload;
    const auto login = [&] {
        expect(session->send_all(protocol::Handshake::build_connect(0)) &&
                   wait_frame(*session, rx, protocol::S2C::Code, payload),
               "polishing handshake");
        protocol::S2CCodeFields code;
        expect(protocol::Handshake::parse_code(payload.data(), payload.size(), code),
               "decode handshake");
        expect(session->send_all(protocol::Handshake::build_answer(code.protocol_test_value)) &&
                   wait_frame(*session, rx, protocol::S2C::CreatePlayer, payload),
               "enter polishing world");
    };
    login();
    const auto barrier = [&] {
        return session->send_all(protocol::GameExit::frame_client_request()) &&
               wait_frame(*session, rx, protocol::S2C::AnsGameExit, payload);
    };
    const auto inspect = [&](int iid) -> std::optional<server::ItemRecord> {
        expect(store->open(config.save_dir), "inspect saved protection");
        const auto items = store->load_items(config.character_id);
        store->close();
        for (const auto& item : items) {
            if (item.iid == iid) {
                return item;
            }
        }
        return {};
    };
    const auto apply = [&](int from, int to) {
        return session->send_all(protocol::EnchantItem::frame_c2s(from, to)) && barrier();
    };
    expect(barrier() && inspect(101) && !inspect(101)->enchant_protected,
           "existing items start without protection");
    expect(apply(100, 101) && inspect(101)->enchant_protected && inspect(100)->count == 49,
           "one stone protects exactly one weapon instance");
    expect(apply(100, 101) && inspect(100)->count == 49,
           "duplicate protection does not consume a stone");
    expect(apply(100, 103) && !inspect(103)->enchant_protected && inspect(100)->count == 49,
           "non-equipment target rejects stone without consumption");
    expect(apply(104, 101) && inspect(101)->enchant_protected && inspect(104)->count == 50,
           "wrong enchantment target preserves protection and materials");
    expect(apply(100, 102) && inspect(102)->enchant_protected && inspect(100)->count == 48,
           "armor receives independent protection");
    expect(apply(100, 110) && inspect(110)->enchant_protected && inspect(100)->count == 47,
           "apply separate protection before guaranteed success");
    expect(apply(111, 110) && inspect(110) && !inspect(110)->enchant_protected &&
               inspect(110)->prefix == 0 && inspect(110)->cur_end == 9 && !inspect(111),
           "successful prefix roll consumes protection and material without endurance loss");
    // Reconnect to verify both persistent markers and their INSERTITEM flags.
    session.reset();
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    session = server::connect_unix(config.unix_path);
    rx.clear();
    if (!session) {
        expect(false, "reconnect polishing client");
        return 1;
    }
    login();
    int protected_items = 0;
    for (int attempt = 0; attempt < 10 && protected_items < 2; ++attempt) {
        if (!wait_frame(*session, rx, protocol::S2C::InsertItem, payload)) {
            break;
        }
        protocol::PacketReader reader(payload.data(), payload.size());
        std::uint32_t iid = 0, info = 0;
        std::uint16_t index = 0;
        std::uint8_t prefix = 0;
        if (reader.read_u32(iid) && reader.read_u16(index) && reader.read_u8(prefix) &&
            reader.read_u32(info) && (iid == 101 || iid == 102) &&
            (info & protocol::kInsertEnchantProtected) != 0) {
            ++protected_items;
        }
    }
    expect(protected_items == 2, "relogin transports both protected instance flags");
    for (const auto& pair : {std::pair{103, 101}, std::pair{104, 102}}) {
        expect(apply(pair.first, pair.second), "perform protected failed enchantment");
        const auto saved = inspect(pair.second);
        expect(saved && !saved->enchant_protected && saved->cur_end == 1 && saved->max_end == 30,
               "failed enchantment clears persistent protection and retains fragile item intact");
        expect(inspect(pair.first)->count == 49, "normal enchantment material cost remains one");
        for (int attempt = 0; attempt < 20 && inspect(pair.second); ++attempt) {
            expect(apply(pair.first, pair.second), "perform unprotected failed enchantment");
        }
        expect(!inspect(pair.second), "subsequent failures can destroy the unprotected item");
    }
    session.reset();
    host.stop();
    std::filesystem::remove_all(root);
    return test::g_failures == 0 ? 0 : 1;
}
