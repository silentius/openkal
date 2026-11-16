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
#include "openkal/platform/File.h"
#include "openkal/platform/Json.h"
#include "openkal/protocol/FrameParser.h"
#include "openkal/protocol/GameExit.h"
#include "openkal/protocol/Handshake.h"
#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/Quest.h"
#include "openkal/protocol/Shop.h"
#include "openkal/server/ServerHost.h"
#include "openkal/server/net/Transport.h"
#include "openkal/server/store/DataStore.h"
#include "test_util.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sqlite3.h>
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

    void fixture(const std::string& path, const std::string& contents) {
        std::ofstream out(path);
        out << contents;
        expect(out.good(), "write synthetic crafting fixture");
    }

    void run_case(const std::string& root, bool full, bool fail_save, bool frees_slot = false) {
        server::ServerConfig config;
        config.maps_dir = root + "/maps";
        config.monsters_dir = root + "/monsters";
        config.items_dir = root + "/items";
        config.npcs_dir = root + "/npcs";
        config.shops_dir = root + "/shops";
        config.save_dir = root + "/save";
        config.unix_path = root + "/game.sock";
        config.store = server::StoreBackend::Sqlite;
        config.transport = server::TransportKind::Unix;
        config.spawn_x = config.spawn_y = 15 * map::kTileSize;
        config.map_radius_cells = 1;
        config.max_monsters = 0;
        for (const auto& dir : {config.maps_dir,
                                config.monsters_dir,
                                config.items_dir,
                                config.npcs_dir,
                                config.shops_dir}) {
            test::mkdir_p(dir);
        }
        expect(test::write_open_area(config.maps_dir + "/n_000_000.akarea"),
               "write synthetic shop map");
        fixture(config.monsters_dir + "/1.json",
                R"({"index":1,"stats":{"level":1,"hp":1},"spawns":[]})");
        const auto origin = std::to_string(config.spawn_x);
        fixture(config.npcs_dir + "/1.json",
                "{\"index\":60002,\"html\":990310,\"xy\":[" + origin + "," + origin + ",0]}");
        fixture(config.npcs_dir + "/2.json",
                "{\"index\":2,\"xy\":[" + std::to_string(config.spawn_x + 200) + "," + origin +
                    ",0]}");
        fixture(config.items_dir + "/23030.json",
                R"({"index":23030,"name":"Skin Ticket","class":["general","etc"],"stackable":1})");
        fixture(config.items_dir + "/9000.json",
                R"({"index":9000,"class":["general","etc"],"stackable":1})");
        fixture(
            config.items_dir + "/25000.json",
            R"({"index":25000,"name":"Synthetic full skin","class":["skin","body"],"wear":1,"action":[1,1],"limit":{"class":"knight"}})");
        fixture(
            config.items_dir + "/25001.json",
            R"({"index":25001,"name":"Synthetic weapon skin","class":["skin","weapon"],"wear":1,"action":[21,1],"skin":{"weapon_type":"sword"},"limit":{"class":"knight"}})");
        fixture(
            config.items_dir + "/25145.json",
            R"({"index":25145,"name":"Other class","class":["skin","body"],"wear":1,"action":[1,1],"limit":{"class":"magician"}})");
        fixture(
            config.items_dir + "/25002.json",
            R"({"index":25002,"name":"Excluded skin","class":["skin","weapon"],"wear":1,"action":[21,2],"skin":{"weapon_type":"sword"},"limit":{"class":"knight"}})");
        fixture(
            config.shops_dir + "/1.json",
            R"({"schema":"openkal.shop/v1","npcIndex":60002,"currencyItem":23030,"skinCatalog":true,"fullBodyPrice":4,"itemPrice":2,"excludeItems":[25002]})");
        auto store = server::create_data_store(server::StoreBackend::Sqlite);
        expect(store && store->open(config.save_dir), "open exchange store");
        server::CharacterRecord hero;
        hero.name = "SkinBuyer";
        hero.hp = hero.mp = 100;
        hero.map_x = hero.map_y = config.spawn_x;
        hero.map_z = 0;
        hero.money = 0;
        expect(store->ensure_default_player("buyer", "buyer", hero), "create buyer");
        const auto account = store->find_account("buyer");
        if (!account) {
            return;
        }
        const auto chars = store->list_characters(account->id);
        if (chars.empty()) {
            return;
        }
        config.character_id = chars.front().id;
        for (const auto& item : store->load_items(config.character_id)) {
            expect(store->delete_item(item.iid), "clear starter items");
        }
        auto seed = [&](int iid, int index, int count) {
            server::ItemRecord r;
            r.character_id = config.character_id;
            r.iid = iid;
            r.index = index;
            r.count = count;
            expect(store->insert_item(r), "seed shop bag");
        };
        const int tickets = frees_slot ? 4 : (full ? 10 : 8);
        const int first_stack = full && !frees_slot ? 5 : 1;
        seed(100, 23030, first_stack);
        seed(101, 23030, tickets - first_stack);
        if (full) {
            for (int i = 0; i < 70; ++i) {
                seed(200 + i, 9000, 1);
            }
        }
        if (fail_save) {
            sqlite3* db = nullptr;
            expect(sqlite3_open((config.save_dir + "/" + server::kSqliteFileName).c_str(), &db) ==
                       SQLITE_OK,
                   "open failure injection");
            expect(sqlite3_exec(
                       db,
                       "CREATE TRIGGER reject_skin BEFORE INSERT ON items WHEN "
                       "NEW.item_index=25000 BEGIN SELECT RAISE(ABORT,'synthetic failure'); END;",
                       nullptr,
                       nullptr,
                       nullptr) == SQLITE_OK,
                   "inject failed skin save");
            sqlite3_close(db);
        }
        store->close();
        server::ServerHost host;
        expect(host.start(config), "start ticket shop server");
        for (int i = 0; i < 200 && !std::filesystem::exists(config.unix_path); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        auto session = server::connect_unix(config.unix_path);
        if (!session) {
            expect(false, "connect buyer");
            return;
        }
        std::vector<std::uint8_t> rx, payload;
        expect(session->send_all(protocol::Handshake::build_connect(0)) &&
                   wait_frame(*session, rx, protocol::S2C::Code, payload),
               "buyer handshake");
        protocol::S2CCodeFields code;
        expect(protocol::Handshake::parse_code(payload.data(), payload.size(), code),
               "parse handshake");
        expect(session->send_all(protocol::Handshake::build_answer(code.protocol_test_value)) &&
                   wait_frame(*session, rx, protocol::S2C::CreatePlayer, payload),
               "enter buyer world");
        std::uint32_t artisan = 0, distant = 0;
        for (int i = 0; i < 2; ++i) {
            expect(wait_frame(*session, rx, protocol::S2C::CreateNpc, payload), "receive NPC");
            protocol::CreateNpcInfo n;
            if (protocol::Quest::parse_create_npc(payload.data(), payload.size(), n)) {
                if (n.index == 60002) {
                    artisan = n.id;
                } else {
                    distant = n.id;
                }
            }
        }
        expect(artisan && distant, "discover NPC ids");
        const auto barrier = [&] {
            expect(session->send_all(protocol::GameExit::frame_client_request()) &&
                       wait_frame(*session, rx, protocol::S2C::AnsGameExit, payload),
                   "synchronize shop");
        };
        const auto count = [&](int index) {
            expect(store->open(config.save_dir), "inspect shop bag");
            auto rows = store->load_items(config.character_id);
            store->close();
            int total = 0;
            for (const auto& r : rows) {
                if (r.index == index) {
                    total += r.count;
                    if (index == 25000 || index == 25001) {
                        expect(!r.account_bound && !r.character_bound, "skins remain tradeable");
                    }
                }
            }
            return total;
        };
        const auto buy = [&](int index, int qty = 1) {
            expect(session->send_all(protocol::Shop::frame_buy_c2s(artisan, index, qty)),
                   "send redemption");
            barrier();
        };
        buy(25000);
        expect(count(23030) == tickets && count(25000) == 0,
               "buy without shop authorization rejected");
        expect(session->send_all(protocol::Shop::frame_item_list_c2s(artisan)) &&
                   wait_frame(*session, rx, protocol::S2C::ItemList, payload),
               "request ticket catalog");
        protocol::ShopItemList list;
        expect(protocol::Shop::parse_item_list_s2c(payload.data(), payload.size(), list) &&
                   list.currency_index == 23030 && list.offers.size() == 2,
               "only own class offers use ticket currency");
        expect(list.offers.size() == 2 && list.offers[0].buy == 4 && list.offers[1].buy == 2,
               "full and weapon costs are authoritative");
        buy(25145);
        buy(25002);
        buy(9000);
        buy(25000, 2);
        expect(count(23030) == tickets && count(25000) == 0 && count(25145) == 0 &&
                   count(25002) == 0,
               "excluded skins, foreign class, non-skin, and bulk requests cannot spend tickets");
        expect(session->send_all(protocol::Shop::frame_item_list_c2s(distant)),
               "distant shop request");
        barrier();
        buy(25000);
        expect(count(23030) == tickets && count(25000) == 0,
               "invalid NPC clears shop authorization");
        expect(session->send_all(protocol::Shop::frame_item_list_c2s(artisan)) &&
                   wait_frame(*session, rx, protocol::S2C::ItemList, payload),
               "resume ticket shop");
        buy(25000);
        if ((full && !frees_slot) || fail_save) {
            expect(count(23030) == tickets && count(25000) == 0,
                   "full bag or failed database transaction retains every ticket");
            buy(25000);
            expect(count(23030) == tickets && count(25000) == 0,
                   "retry after rollback cannot duplicate skins");
        } else {
            expect(count(23030) == tickets - 4 && count(25000) == 1,
                   "four split-stack tickets buy one full skin without gold");
            if (!full) {
                buy(25001);
                buy(25001);
                buy(25001);
                expect(count(23030) == 0 && count(25001) == 2,
                       "two tickets per weapon and no purchase without enough tickets");
            }
        }
        session->close();
        host.stop();
    }
}  // namespace
int main() {
    const auto root = openkal::test::temp_dir(("skin_shop_" + std::to_string(getpid())).c_str());
    run_case(root + "/success", false, false);
    run_case(root + "/full", true, false);
    run_case(root + "/failure", false, true);
    run_case(root + "/freed_slot", true, false, true);
    std::filesystem::remove_all(root);
    return openkal::test::g_failures ? 1 : 0;
}
