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
    for (int grade : {50, 53}) {
        expect(server::revision_hammer_stones(true, false, grade, 0) == 4 &&
                   server::revision_hammer_stones(true, false, grade, 5) == 9,
               "G50 and G53 weapons award 4 to 9 stones");
        expect(server::revision_hammer_stones(false, false, grade, 0) == 2 &&
                   server::revision_hammer_stones(false, false, grade, 5) == 7,
               "G50 and G53 armor award 2 to 7 stones");
        expect(server::revision_hammer_stones(false, true, grade, 0) == 1 &&
                   server::revision_hammer_stones(false, true, grade, 5) == 1,
               "accessory rewards remain exactly one stone");
    }
    expect(server::revision_hammer_stones(true, false, 53, -1) == 4 &&
               server::revision_hammer_stones(true, false, 53, 99) == 9,
           "hammer random bonus stays within the grade-based range");
    expect(server::revision_stones_for_grade(53, 0) == 1 &&
               server::revision_stones_for_grade(53, 5) == 6,
           "failed-enchantment compensation retains its separate existing rule");
    const auto root = test::temp_dir(("revision_hammer_" + std::to_string(getpid())).c_str());
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
    expect(test::write_open_area(config.maps_dir + "/n_000_000.akarea"), "write hammer map");
    const auto fixture = [](const std::string& path, const char* json) {
        std::ofstream out(path);
        out << json;
        expect(out.good(), "write project-owned hammer fixture");
    };
    fixture(config.monsters_dir + "/1.json",
            R"({"index":1,"name":"Unused","stats":{"level":1,"hp":1},"spawns":[]})");
    fixture(config.items_dir + "/23021.json",
            R"({"index":23021,"name":"Revision Hammer","class":["general","etc"],"stackable":1})");
    fixture(config.items_dir + "/517.json",
            R"({"index":517,"name":"Stone of Revision","class":["general","etc"],"stackable":1})");
    const char* targets[] = {
        R"({"index":9000,"class":["weapon","sword"],"wear":1,"level":53})",
        R"({"index":9001,"class":["defense","upperarmor"],"wear":1,"level":50})",
        R"({"index":9002,"class":["ornament","ring"],"wear":1})",
        R"({"index":9003,"class":["ornament","trinket"],"wear":1,"level":30})",
        R"({"index":9004,"class":["general","etc"],"level":40})",
        R"({"index":9005,"class":["weapon","sword"],"wear":1})",
        R"({"index":9006,"class":["weapon","sword"],"wear":1,"level":40})",
        R"({"index":9007,"class":["ornament","necklace"],"wear":1,"level":40})"};
    for (int i = 0; i < 8; ++i) {
        fixture(config.items_dir + "/" + std::to_string(9000 + i) + ".json", targets[i]);
    }
    auto store = server::create_data_store(server::StoreBackend::Sqlite);
    expect(store && store->open(config.save_dir), "open hammer persistence");
    server::CharacterRecord hero;
    hero.name = "Recycler";
    hero.level = 60;
    hero.hp = hero.mp = 100;
    hero.map_x = hero.map_y = config.spawn_x;
    expect(store->ensure_default_player("recycler", "recycler", hero), "create hammer player");
    const auto account = store->find_account("recycler");
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
    for (int i = 0; i < 9; ++i) {
        server::ItemRecord item;
        item.character_id = config.character_id;
        item.iid = 100 + i;
        item.index = i == 0 ? server::kRevisionHammerIndex : 8999 + i;
        item.count = i == 0 ? 4 : 1;
        expect(store->insert_item(item), "seed hammer or equipment");
    }
    server::ItemRecord worn;
    worn.character_id = config.character_id;
    worn.iid = 109;
    worn.index = 9000;
    worn.count = 1;
    worn.slot = server::persist_slot_of(server::WearKind::Weapon);
    expect(store->insert_item(worn), "seed worn conversion target");
    store->close();
    server::ServerHost host;
    expect(host.start(config), "start hammer server");
    for (int attempt = 0; attempt < 200 && !std::filesystem::exists(config.unix_path); ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    auto session = server::connect_unix(config.unix_path);
    if (!session) {
        expect(false, "connect hammer client");
        return 1;
    }
    std::vector<std::uint8_t> rx, payload;
    const auto login = [&] {
        expect(session->send_all(protocol::Handshake::build_connect(0)) &&
                   wait_frame(*session, rx, protocol::S2C::Code, payload),
               "hammer handshake");
        protocol::S2CCodeFields code;
        expect(protocol::Handshake::parse_code(payload.data(), payload.size(), code),
               "decode handshake");
        expect(session->send_all(protocol::Handshake::build_answer(code.protocol_test_value)) &&
                   wait_frame(*session, rx, protocol::S2C::CreatePlayer, payload),
               "enter hammer world");
    };
    login();
    const auto barrier = [&] {
        return session->send_all(protocol::GameExit::frame_client_request()) &&
               wait_frame(*session, rx, protocol::S2C::AnsGameExit, payload);
    };
    const auto inventory = [&] {
        expect(store->open(config.save_dir), "inspect conversion persistence");
        const auto items = store->load_items(config.character_id);
        store->close();
        return items;
    };
    const auto inspect = [&](int iid) -> std::optional<server::ItemRecord> {
        for (const auto& item : inventory()) {
            if (item.iid == iid) {
                return item;
            }
        }
        return {};
    };
    const auto stones = [&] {
        int count = 0;
        for (const auto& item : inventory()) {
            if (item.index == server::kRevisionStoneIndex) {
                count += item.count;
            }
        }
        return count;
    };
    const auto apply = [&](int from, int to) {
        return session->send_all(protocol::EnchantItem::frame_c2s(from, to)) && barrier();
    };
    expect(barrier(), "initial hammer barrier");
    for (int target : {100, 105, 106, 108, 109, 999}) {
        expect(apply(100, target), "send rejected conversion");
        expect(inspect(100) && inspect(100)->count == 4 && stones() == 0,
               "self, non-equipment, ungraded, necklace, worn and missing targets consume nothing");
    }
    const int grades[] = {53, 50, 0, 30};
    for (int i = 0; i < 4; ++i) {
        const int before = stones();
        expect(apply(100, 101 + i), "convert weapon, armor, ring or trinket");
        const int awarded = stones() - before;
        const int base = i == 0 ? 4 : 2;
        expect(!inspect(101 + i) &&
                   (i >= 2 ? awarded == 1 : awarded >= base && awarded <= base + grades[i] / 10),
               "equipment is deleted; rings and trinkets always award exactly one stone");
        const auto hammer = inspect(100);
        expect(i == 3 ? !hammer : hammer && hammer->count == 3 - i,
               "exactly one hammer is consumed and the final hammer instance is deleted");
        const int total = stones();
        expect(apply(100, 101 + i) && stones() == total,
               "replayed conversion cannot duplicate the reward");
    }
    const int total = stones();
    session.reset();
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    session = server::connect_unix(config.unix_path);
    rx.clear();
    if (session) {
        login();
        expect(barrier() && stones() == total && !inspect(100) && !inspect(101) && !inspect(104),
               "relogin preserves conversion rewards and consumed instances");
    } else {
        expect(false, "reconnect hammer client");
    }
    session.reset();
    host.stop();
    std::filesystem::remove_all(root);
    return test::g_failures == 0 ? 0 : 1;
}
