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
#include "openkal/protocol/PacketWriter.h"
#include "openkal/protocol/Quest.h"
#include "openkal/protocol/TradingPost.h"
#include "openkal/server/ServerHost.h"
#include "openkal/server/net/Transport.h"
#include "openkal/server/store/DataStore.h"
#include "test_util.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sqlite3.h>
#include <thread>
#include <unistd.h>
#include <vector>

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
    const auto root = test::temp_dir(("stack_server_" + std::to_string(getpid())).c_str());
    server::ServerConfig config;
    config.maps_dir = root + "/maps";
    config.monsters_dir = root + "/monsters";
    config.items_dir = root + "/items";
    config.npcs_dir = root + "/npcs";
    config.save_dir = root + "/save";
    config.unix_path = root + "/game.sock";
    config.store = server::StoreBackend::Sqlite;
    config.transport = server::TransportKind::Unix;
    config.spawn_x = config.spawn_y = 15 * map::kTileSize;
    config.map_radius_cells = 1;
    config.max_monsters = 0;
    config.character_id = 1;
    for (const auto& dir :
         {config.maps_dir, config.items_dir, config.monsters_dir, config.npcs_dir}) {
        test::mkdir_p(dir);
    }
    expect(test::write_open_area(config.maps_dir + "/n_000_000.akarea"), "write owned stack map");
    {
        std::ofstream file(config.monsters_dir + "/1.json");
        file << R"({"index":1,"stats":{"level":1,"hp":1},"spawns":[]})";
    }
    for (const auto& entry :
         {std::pair<int, const char*>{23000, "general"}, {23001, "general"}, {23002, "quest"}}) {
        std::ofstream file(config.items_dir + "/" + std::to_string(entry.first) + ".json");
        file << "{\"index\":" << entry.first << ",\"name\":\"Owned stack fixture\",\"class\":[\""
             << entry.second << "\",\"etc\"],\"stackable\":" << (entry.first == 23001 ? 0 : 1)
             << "}";
    }
    {
        std::ofstream file(config.npcs_dir + "/60003.json");
        file
            << "{\"index\":60003,\"map\":0,\"shape\":75,\"kind\":3,\"trading_post_collector\":true,\"xy\":["
            << config.spawn_x + 16 << ", " << config.spawn_y << ", 0]}";
    }
    server::SqliteStore store;
    expect(store.open(config.save_dir), "open stack fixture store");
    server::AccountRecord account{1, server::kDefaultAccountName, server::kDefaultAccountPassword};
    expect(store.upsert_account(account), "seed stack account");
    server::CharacterRecord hero;
    hero.id = hero.account_id = 1;
    hero.name = "Stacks";
    hero.money = 100;
    hero.map_x = hero.map_y = config.spawn_x;
    auto seller = hero;
    seller.id = 2;
    seller.name = "StackSeller";
    expect(store.save_character(hero) && store.save_character(seller) &&
               store.grant_starter_items(hero, {}),
           "seed stack characters");
    for (int n = 0; n < 73; ++n) {
        server::ItemRecord row;
        row.iid = 100 + n;
        row.character_id = 1;
        row.index = n < 7 || n == 72 ? 23000 : 23001;
        row.count = n == 0 ? 5 : n == 1 ? 7 : n == 6 ? std::numeric_limits<int>::max() : 1;
        row.prefix = n == 2 ? 1 : 0;
        row.account_bound = n == 3;
        row.character_bound = n == 4;
        row.random_stat_type = n == 5 ? 1 : 0;
        row.random_stat_value = n == 5 ? 2 : 0;
        expect(store.insert_item(row),
               "seed full bag with incompatible and expired-page instances");
    }
    for (int n = 0; n < 2; ++n) {
        server::ItemRecord row;
        row.iid = 300 + n;
        row.character_id = 1;
        row.index = 23002;
        row.count = 4 + n * 2;
        expect(store.insert_item(row), "seed two quest stacks");
    }
    server::ItemRecord delivery;
    delivery.iid = 400;
    delivery.character_id = 2;
    delivery.index = 23000;
    delivery.count = 3;
    expect(store.insert_item(delivery) && store.trading_post_sell(delivery, 3, 1, {}).committed,
           "seed a delivery that fits an existing full-bag stack");
    protocol::TradingPostRequest market;
    auto page = store.trading_post_list(hero.id, market);
    expect(page && !page->entries.empty() &&
               store.trading_post_buy(hero, page->entries.front().id, 3, 1).committed,
           "buy full-bag stack delivery before login");
    store.close();
    server::ServerHost host;
    expect(host.start(config), "start stack server");
    for (int n = 0; n < 200 && !std::filesystem::exists(config.unix_path); ++n) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    auto session = server::connect_unix(config.unix_path);
    expect(session != nullptr, "connect stack client");
    if (!session) {
        return 1;
    }
    std::vector<std::uint8_t> rx, payload;
    protocol::S2CCodeFields code;
    expect(session->send_all(protocol::Handshake::build_connect(0)) &&
               wait_frame(*session, rx, protocol::S2C::Code, payload) &&
               protocol::Handshake::parse_code(payload.data(), payload.size(), code) &&
               session->send_all(protocol::Handshake::build_answer(code.protocol_test_value)) &&
               wait_frame(*session, rx, protocol::S2C::CreateNpc, payload),
           "enter stack world");
    protocol::CreateNpcInfo npc;
    expect(protocol::Quest::parse_create_npc(payload.data(), payload.size(), npc),
           "read collector");
    const auto inventory = [&] {
        expect(store.open(config.save_dir), "inspect stack persistence");
        const auto rows = store.load_items(1);
        store.close();
        return rows;
    };
    const auto count = [&](int iid) {
        const auto rows = inventory();
        const auto found =
            std::find_if(rows.begin(), rows.end(), [&](const auto& row) { return row.iid == iid; });
        return found == rows.end() ? 0 : found->count;
    };
    market = {};
    market.operation = 1;
    market.command = protocol::TradingPostCommand::CollectAll;
    market.view = protocol::TradingPostView::Collection;
    market.npc_id = npc.id;
    protocol::TradingPostState state;
    expect(session->send_all(protocol::frame_trading_post_request(market)) &&
               wait_frame(*session, rx, protocol::S2C::TradingPost, payload) &&
               protocol::parse_trading_post_state(payload.data(), payload.size(), state) &&
               state.entries.empty() && count(100) == 8 && count(400) == 0,
           "full bag accepts compatible delivery into the existing IID");
    protocol::PacketWriter malformed;
    malformed.write_u8(0);
    malformed.write_u8(255);
    expect(session->send_all(
               malformed.frame(static_cast<std::uint8_t>(protocol::C2S::SortInventory))) &&
               session->send_all(protocol::GameExit::frame_client_request()) &&
               wait_frame(*session, rx, protocol::S2C::AnsGameExit, payload) && count(100) == 8 &&
               count(101) == 7,
           "sort request rejects trailing client data without mutating any stack");
    sqlite3* faults = nullptr;
    expect(sqlite3_open((config.save_dir + "/" + server::kSqliteFileName).c_str(), &faults) ==
               SQLITE_OK,
           "open stack fault injection connection");
    expect(sqlite3_exec(faults,
                        "CREATE TRIGGER fail_stack_sort BEFORE DELETE ON items WHEN OLD.iid=101 "
                        "BEGIN SELECT RAISE(ABORT,'owned merge failure'); END",
                        nullptr,
                        nullptr,
                        nullptr) == SQLITE_OK,
           "inject a failure after sorting begins");
    const auto sort = [&](bool quest, bool committed) {
        protocol::PacketWriter request;
        request.write_u8(quest ? 1 : 0);
        return session->send_all(
                   request.frame(static_cast<std::uint8_t>(protocol::C2S::SortInventory))) &&
               wait_frame(*session, rx, protocol::S2C::SortInventory, payload) &&
               payload == std::vector<std::uint8_t>{static_cast<std::uint8_t>(quest),
                                                    static_cast<std::uint8_t>(committed)};
    };
    expect(sort(false, false) && count(100) == 8 && count(101) == 7,
           "failed consolidation rolls back every stack and reports failure");
    expect(sqlite3_exec(faults, "DROP TRIGGER fail_stack_sort", nullptr, nullptr, nullptr) ==
               SQLITE_OK,
           "remove stack failure");
    sqlite3_close(faults);
    expect(sort(false, true) && count(100) == 15 && count(101) == 0,
           "successful consolidation removes the source and persists the exact sum");
    expect(
        count(102) == 1 && count(103) == 1 && count(104) == 1 && count(105) == 1 &&
            count(106) == std::numeric_limits<int>::max() && count(107) == 1 && count(108) == 1 &&
            count(172) == 1 && count(300) == 4 && count(301) == 6,
        "sorting preserves differing attributes, both bindings, nonstackable items, overflow, expired cells and other sections");
    expect(sort(false, true) && count(100) == 15 && count(172) == 1,
           "repeated sort is idempotent and leaves expired-page items alone");
    expect(sort(true, true) && count(300) == 10 && count(301) == 0,
           "quest sorting consolidates only the requested section");
    session->close();
    host.stop();
    expect(count(100) == 15 && count(101) == 0 && count(300) == 10 && count(301) == 0,
           "disconnect autosave never resurrects consumed source IIDs");
    std::filesystem::remove_all(root);
    return test::g_failures == 0 ? 0 : 1;
}
