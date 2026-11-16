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
#include "openkal/protocol/FrameParser.h"
#include "openkal/protocol/GameExit.h"
#include "openkal/protocol/Handshake.h"
#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/UseItem.h"
#include "openkal/server/ServerHost.h"
#include "openkal/server/net/Transport.h"
#include "openkal/server/store/DataStore.h"
#include "test_util.h"

#include <chrono>
#include <filesystem>
#include <fstream>
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
    const auto root = test::temp_dir(("auto_hp_" + std::to_string(getpid())).c_str());
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
    expect(test::write_open_area(config.maps_dir + "/n_000_000.akarea"), "write auto hp map");
    for (const auto& file : std::filesystem::directory_iterator(std::string(OPENKAL_REPO_ROOT) +
                                                                "/config/items/general/refresh")) {
        if (file.path().extension() == ".json") {
            std::filesystem::copy_file(file.path(),
                                       std::filesystem::path(config.items_dir) /
                                           file.path().filename(),
                                       std::filesystem::copy_options::overwrite_existing);
        }
    }
    {
        std::ofstream out(config.monsters_dir + "/1.json");
        out << R"({"index":1,"name":"Unused","stats":{"level":1,"hp":1},"spawns":[]})";
    }
    auto store = server::create_data_store(server::StoreBackend::Sqlite);
    expect(store && store->open(config.save_dir), "open auto hp persistence");
    server::CharacterRecord hero;
    hero.name = "AutoHpTester";
    hero.level = 90;
    hero.hp = 1;
    hero.mp = 100;
    hero.map_x = hero.map_y = config.spawn_x;
    expect(store->ensure_default_player("auto_hp", "auto_hp", hero), "create auto hp character");
    const auto account = store->find_account("auto_hp");
    if (!account) {
        return 1;
    }
    const auto chars = store->list_characters(account->id);
    if (chars.empty()) {
        return 1;
    }
    config.character_id = chars.front().id;
    for (const auto& item : store->load_items(config.character_id)) {
        expect(store->delete_item(item.iid), "clear starter inventory");
    }
    for (int index : {23008, 23009, 23010, 23011, 23000}) {
        server::ItemRecord record;
        record.character_id = config.character_id;
        record.iid = index;
        record.index = index;
        record.count = 5;
        expect(store->insert_item(record), "seed medicine stacks");
    }
    store->close();
    server::ServerHost host;
    expect(host.start(config), "start auto hp server");
    for (int attempt = 0; attempt < 200 && !std::filesystem::exists(config.unix_path); ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    auto session = server::connect_unix(config.unix_path);
    expect(session != nullptr, "connect auto hp client");
    if (!session) {
        return 1;
    }
    std::vector<std::uint8_t> rx, payload;
    expect(session->send_all(protocol::Handshake::build_connect(0)) &&
               wait_frame(*session, rx, protocol::S2C::Code, payload),
           "auto hp handshake");
    protocol::S2CCodeFields code;
    expect(protocol::Handshake::parse_code(payload.data(), payload.size(), code),
           "parse handshake");
    expect(session->send_all(protocol::Handshake::build_answer(code.protocol_test_value)) &&
               wait_frame(*session, rx, protocol::S2C::CreatePlayer, payload),
           "enter auto hp world");
    const auto barrier = [&] {
        expect(session->send_all(protocol::GameExit::frame_client_request()) &&
                   wait_frame(*session, rx, protocol::S2C::AnsGameExit, payload),
               "save barrier");
    };
    const auto count = [&](int iid) {
        expect(store->open(config.save_dir), "read medicine inventory");
        const auto items = store->load_items(config.character_id);
        store->close();
        for (const auto& record : items) {
            if (record.iid == iid) {
                return record.count;
            }
        }
        return 0;
    };
    const auto use = [&](int iid, std::uint8_t threshold = 0) {
        expect(session->send_all(protocol::UseItem::frame_c2s(iid, threshold)), "request medicine");
        barrier();
    };
    use(23008);
    expect(count(23008) == 5, "server rejects manual use of auto medicine");
    use(23000, 75);
    expect(count(23000) == 5, "server rejects automatic use of manual medicine");
    use(23008, 60);
    expect(count(23008) == 5, "server rejects unsupported auto hp threshold");
    use(23008, 75);
    expect(count(23008) == 4, "server consumes one automatic dose below threshold");
    use(23011, 100);
    expect(count(23011) == 5, "server enforces cooldown across auto medicine sizes");
    use(23000);
    expect(count(23000) == 5, "server shares cooldown with manual medicine");
    std::this_thread::sleep_for(std::chrono::milliseconds(1400));
    use(23011, 100);
    expect(count(23011) == 4, "server allows automatic medicine after cooldown");
    std::this_thread::sleep_for(std::chrono::milliseconds(1400));
    use(23008, 25);
    expect(count(23008) == 4, "server rejects dose when healed above requested threshold");
    session->close();
    host.stop();
    std::filesystem::remove_all(root);
    return test::g_failures == 0 ? 0 : 1;
}
