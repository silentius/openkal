/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/EssenceBox.h"

#include "fixture_util.h"
#include "openkal/map/Constants.h"
#include "openkal/protocol/CreatePlayer.h"
#include "openkal/protocol/FrameParser.h"
#include "openkal/protocol/GameExit.h"
#include "openkal/protocol/Handshake.h"
#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/TrashItem.h"
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
    using openkal::test::expect;
    using namespace openkal;

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
    server::EssenceBox box;
    expect(box.load_file(std::string(OPENKAL_REPO_ROOT) + "/config/collection/essence_box.json"),
           "load shipped essence box configuration");
    expect(box.min_count() == 1 && box.max_count() == 5, "shipped quantity range");
    expect(box.item_for_roll(0) == 24057 && box.item_for_roll(0.79999) == 24057 &&
               box.item_for_roll(0.8) == 24058 && box.item_for_roll(0.94999) == 24058 &&
               box.item_for_roll(0.95) == 24059 && box.item_for_roll(0.99999) == 24059,
           "grade probability boundaries");
    expect(box.item_for_roll(-0.1) == 0 && box.item_for_roll(1) == 0,
           "reject invalid random values");
    for (
        const auto* invalid :
        {R"({"essence_count":{"min":0,"max":5},"grade_percent":{"low":80,"middle":15,"high":5}})",
         R"({"essence_count":{"min":6,"max":5},"grade_percent":{"low":80,"middle":15,"high":5}})",
         R"({"essence_count":{"min":1.5,"max":5},"grade_percent":{"low":80,"middle":15,"high":5}})",
         R"({"essence_count":{"min":1,"max":217},"grade_percent":{"low":80,"middle":15,"high":5}})",
         R"({"essence_count":{"min":1,"max":5},"grade_percent":{"low":80,"middle":15,"high":6}})",
         R"({"essence_count":{"min":1,"max":5},"grade_percent":{"low":80,"middle":21,"high":-1}})",
         R"({"essence_count":{"min":1,"max":5},"grade_percent":{"low":100}})",
         "{"}) {
        expect(!box.load_json_text(invalid) && box.max_count() == 0 && box.item_for_roll(0) == 0,
               "invalid table disables opening");
    }
    const std::string fixed =
        R"({"essence_count":{"min":2,"max":2},"grade_percent":{"low":0,"middle":0,"high":100}})";
    expect(box.load_json_text(fixed) && box.item_for_roll(0) == 24059,
           "zero probability grades are never selected");
    const auto root = test::temp_dir(("essence_box_" + std::to_string(getpid())).c_str());
    server::ServerConfig config;
    config.maps_dir = root + "/maps";
    config.monsters_dir = root + "/monsters";
    config.items_dir = root + "/items";
    config.save_dir = root + "/save";
    config.unix_path = root + "/game.sock";
    config.store = server::StoreBackend::Sqlite;
    config.transport = server::TransportKind::Unix;
    config.spawn_x = config.spawn_y = 15 * map::kTileSize;
    config.spawn_z = 0;
    config.map_radius_cells = 1;
    config.max_monsters = 0;
    for (const auto& dir :
         {config.maps_dir, config.items_dir, config.monsters_dir, root + "/collection"}) {
        test::mkdir_p(dir);
    }
    expect(test::write_open_area(config.maps_dir + "/n_000_000.akarea"), "write box test map");
    for (const auto& file : std::filesystem::directory_iterator(std::string(OPENKAL_REPO_ROOT) +
                                                                "/config/items/collection")) {
        std::filesystem::copy_file(file.path(),
                                   std::filesystem::path(config.items_dir) / file.path().filename(),
                                   std::filesystem::copy_options::overwrite_existing);
    }
    {
        std::ofstream out(root + "/collection/essence_box.json");
        out << fixed;
    }
    {
        std::ofstream out(config.monsters_dir + "/1.json");
        out << R"({"index":1,"name":"Unused","stats":{"level":1,"hp":1},"spawns":[]})";
    }
    auto store = server::create_data_store(server::StoreBackend::Sqlite);
    expect(store && store->open(config.save_dir), "open essence box persistence");
    server::CharacterRecord hero;
    hero.name = "BoxTester";
    hero.level = 90;
    hero.hp = hero.mp = 100;
    hero.map_x = hero.map_y = config.spawn_x;
    expect(store->ensure_default_player("boxes", "boxes", hero), "create box test character");
    const auto account = store->find_account("boxes");
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
    server::ItemRecord single;
    single.character_id = config.character_id;
    single.iid = 100;
    single.index = 23012;
    single.count = 1;
    expect(store->insert_item(single), "seed single box");
    auto stacked = single;
    stacked.iid = 101;
    stacked.prefix = 1;
    stacked.count = 2;
    expect(store->insert_item(stacked), "seed stack of boxes");
    auto reward = single;
    reward.iid = 999;
    reward.index = 24059;
    auto conflicting = reward;
    conflicting.iid = 101;
    expect(!store->consume_item_with_rewards(single, {reward, conflicting}),
           "reward conflict rolls back the entire exchange");
    const auto unchanged = store->load_items(config.character_id);
    expect(unchanged.size() == 2 && unchanged[0].iid == 100 && unchanged[0].count == 1,
           "rollback restores consumed box and removes staged reward");
    auto stale = single;
    stale.count = 2;
    expect(!store->consume_item_with_rewards(stale, {reward}), "reject stale box count");
    for (int i = 0; i < 70; ++i) {
        auto filler = reward;
        filler.iid = 200 + i;
        expect(store->insert_item(filler), "fill inventory");
    }
    store->close();
    server::ServerHost host;
    expect(host.start(config), "start essence box server");
    for (int attempt = 0; attempt < 200 && !std::filesystem::exists(config.unix_path); ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    auto session = server::connect_unix(config.unix_path);
    expect(session != nullptr, "connect essence box client");
    if (!session) {
        return 1;
    }
    std::vector<std::uint8_t> rx, payload;
    expect(session->send_all(protocol::Handshake::build_connect(0)) &&
               wait_frame(*session, rx, protocol::S2C::Code, payload),
           "box handshake");
    protocol::S2CCodeFields code;
    expect(protocol::Handshake::parse_code(payload.data(), payload.size(), code),
           "parse handshake");
    expect(session->send_all(protocol::Handshake::build_answer(code.protocol_test_value)) &&
               wait_frame(*session, rx, protocol::S2C::CreatePlayer, payload),
           "enter box test world");
    const auto barrier = [&] {
        expect(session->send_all(protocol::GameExit::frame_client_request()) &&
                   wait_frame(*session, rx, protocol::S2C::AnsGameExit, payload),
               "save barrier");
    };
    const auto items = [&] {
        expect(store->open(config.save_dir), "read saved rewards");
        auto result = store->load_items(config.character_id);
        store->close();
        return result;
    };
    const auto open = [&](int iid) {
        expect(session->send_all(protocol::UseItem::frame_c2s(iid)), "send box use");
        barrier();
    };
    open(100);
    expect(items().size() == 72,
           "full General rejects opening when consuming the box releases only one reward cell");
    expect(session->send_all(protocol::TrashItem::frame_c2s(200)), "free one General cell");
    barrier();
    open(100);
    auto current = items();
    expect(current.size() == 72,
           "consuming a General box releases the second required reward cell");
    int rolled = 0;
    for (const auto& item : current) {
        expect(item.iid != 100, "single box was consumed");
        if (item.iid >= 270) {
            ++rolled;
            expect(item.index == 24059 && item.count == 1 && item.random_stat_type >= 100 &&
                       item.random_stat_type <= 121 && item.random_stat_value > 0,
                   "high grade rewards have individual persistent abilities");
        }
    }
    expect(rolled == 2, "configured fixed total and 100 percent grade applied");
    open(100);
    expect(items().size() == 72, "replayed consumed instance gives no additional rewards");
    for (int iid = 202; iid <= 205; ++iid) {
        expect(session->send_all(protocol::TrashItem::frame_c2s(iid)), "free reward cells");
    }
    barrier();
    open(101);
    expect(items().size() == 68, "server cooldown rejects rapid box opening");
    std::this_thread::sleep_for(std::chrono::milliseconds(1550));
    open(101);
    current = items();
    expect(current.size() == 70, "stacked box grants rewards without releasing its cell");
    bool found_stack = false;
    for (const auto& item : current) {
        if (item.iid == 101) {
            found_stack = true;
            expect(item.count == 1, "consume exactly one box");
        }
    }
    expect(found_stack, "remaining box is preserved");
    std::this_thread::sleep_for(std::chrono::milliseconds(1550));
    open(101);
    current = items();
    expect(current.size() == 71, "last stacked box replaced by two essences");
    for (const auto& item : current) {
        expect(item.index != 23012, "all boxes consumed");
    }
    session->close();
    host.stop();
    expect(items().size() == 71, "rewards survive server shutdown");
    std::filesystem::remove_all(root);
    return test::g_failures ? 1 : 0;
}
