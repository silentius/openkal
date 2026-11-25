/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/world/StarterKit.h"

#include "fixture_util.h"
#include "openkal/map/Constants.h"
#include "openkal/protocol/Chatting.h"
#include "openkal/protocol/DropItem.h"
#include "openkal/protocol/FrameParser.h"
#include "openkal/protocol/GameExit.h"
#include "openkal/protocol/Handshake.h"
#include "openkal/protocol/Honor.h"
#include "openkal/protocol/InsertItem.h"
#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketReader.h"
#include "openkal/protocol/Quest.h"
#include "openkal/protocol/Shop.h"
#include "openkal/protocol/Storage.h"
#include "openkal/protocol/UseItem.h"
#include "openkal/server/ServerHost.h"
#include "openkal/server/net/Transport.h"
#include "test_util.h"

#include <algorithm>
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
}  // namespace

int main() {
    using namespace openkal;
    const auto root = test::temp_dir(("starter_kit_" + std::to_string(getpid())).c_str());
    server::ServerConfig config;
    config.maps_dir = root + "/maps";
    config.monsters_dir = root + "/monsters";
    config.items_dir = root + "/items";
    config.starters_dir = root + "/starters";
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
                            config.items_dir,
                            config.monsters_dir,
                            config.starters_dir,
                            config.npcs_dir,
                            config.shops_dir}) {
        test::mkdir_p(dir);
    }
    expect(test::write_open_area(config.maps_dir + "/n_000_000.akarea"), "write starter map");
    std::ofstream(config.monsters_dir + "/1.json")
        << R"({"index":1,"name":"Unused","stats":{"level":1,"hp":1},"spawns":[]})";
    std::ofstream(config.items_dir + "/9000.json")
        << R"({"index":9000,"name":"Starter writ","class":["general","etc"],"stackable":1,"use":1,"buy":1,"sell":1,"specialty":{"buff":[100, 86400, 0]}})";
    std::ofstream(config.items_dir + "/9001.json")
        << R"({"index":9001,"name":"Starter sword","class":["weapon","sword"],"wear":1})";
    std::ofstream(config.items_dir + "/9002.json")
        << R"({"index":9002,"name":"Honor reward","class":["general","etc"],"stackable":1})";
    std::ofstream(config.items_dir + "/23032.json")
        << R"({"index":23032,"name":"Honor Coupon","class":["general","etc"],"stackable":1,"use":1,"specialty":{"honor_reward_points":1000}})";
    std::ofstream(config.starters_dir + "/common.json")
        << "{\"spawn\": [" << config.spawn_x << ", " << config.spawn_y
        << ", 0], \"items\": [{\"item_id\":31,\"count\":100}, {\"item_id\":9000,\"count\":2}]}";
    for (const auto* name : {"knight", "mage", "archer", "thief"}) {
        std::ofstream(config.starters_dir + "/" + name + ".json")
            << R"({"items":[{"item_id":9001,"count":1,"equipped":true}]})";
    }

    std::ofstream(config.npcs_dir + "/1.json")
        << "{\"index\":1,\"kind\":1,\"shape\":1,\"storage_keeper\":true,\"xy\":[" << config.spawn_x
        << ", " << config.spawn_y << ", 0]}";
    std::ofstream(config.shops_dir + "/1.json")
        << R"({"schema":"openkal.shop/v1","npcIndex":1,"items":[9000]})";
    std::ofstream(config.npcs_dir + "/2.json")
        << "{\"index\":2,\"kind\":1,\"shape\":1,\"html\":990340,\"xy\":[" << config.spawn_x + 30
        << ", " << config.spawn_y << ", 0]}";
    std::ofstream(config.shops_dir + "/2.json")
        << R"({"schema":"openkal.shop/v1","npcIndex":2,"honorOffers":[{"itemIndex":9002,"rewardPoints":400,"minRank":4}]})";

    auto store = server::create_data_store(server::StoreBackend::Sqlite);
    expect(store && store->open(config.save_dir), "open starter persistence");
    server::CharacterRecord hero;
    hero.name = "Starter";
    hero.map_x = hero.map_y = config.spawn_x;
    expect(store->ensure_default_player("starter", "starter", hero), "create starter character");
    const auto account = store->find_account("starter");
    if (!account) {
        return 1;
    }
    hero = store->list_characters(account->id).front();
    config.character_id = hero.id;
    server::StarterKit kit;
    expect(kit.load_dir(config.starters_dir), "load owned starter config");
    std::unordered_map<int, server::ItemCombatDef> defs;
    expect(kit.grant_if_empty(*store, hero, defs), "grant complete kit once");
    auto rows = store->load_items(hero.id);
    expect(rows.size() == 2 && hero.money == 100, "kit includes equipment, consumables and money");
    std::uint32_t consumable = 0;
    for (const auto& item : rows) {
        expect(item.character_bound, "every starter instance is character-bound");
        if (item.index == 9000) {
            consumable = static_cast<std::uint32_t>(item.iid);
        }
    }
    expect(!kit.grant_if_empty(*store, hero, defs), "reject second grant with inventory present");

    server::CharacterRecord retry;
    retry.account_id = account->id;
    retry.name = "Retry";
    expect(store->save_character(retry), "create rollback character");
    retry = store->list_characters(account->id).back();
    const auto before = retry;
    // Collide with an existing IID after one successful insert inside the transaction.
    std::atomic<std::uint32_t> colliding{0};
    const auto first = rows.front();
    expect(store->delete_item(first.iid), "free first starter iid for rollback test");
    expect(!kit.grant_if_empty(*store, retry, defs, &colliding),
           "rollback kit on later iid collision");
    expect(store->load_items(retry.id).empty() && retry.money == before.money &&
               store->load_character(retry.id)->money == before.money,
           "failed grant leaves no partial inventory, money or caller mutation");
    expect(store->insert_item(first), "restore first starter row");
    expect(kit.grant_if_empty(*store, retry, defs), "failed transaction can retry the whole kit");
    store->close();

    server::ServerHost host;
    expect(host.start(config), "start starter server");
    std::unique_ptr<server::ISession> session;
    std::vector<std::uint8_t> rx, payload;
    const auto connect = [&] {
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
        bool saved = false;
        return session->send_all(protocol::GameExit::frame_client_request()) &&
               wait_frame(*session, rx, protocol::S2C::AnsGameExit, payload) &&
               protocol::GameExit::parse_server_answer(payload.data(), payload.size(), saved) &&
               saved;
    };
    const auto inspect = [&] {
        expect(store->open(config.save_dir), "inspect persisted starter inventory");
        const auto result = store->load_items(hero.id);
        store->close();
        return result;
    };
    expect(connect(), "enter starter world");
    if (!session) {
        host.stop();
        return 1;
    }
    protocol::HonorStatus honor;
    const auto receive_honor = [&] {
        return wait_frame(*session, rx, protocol::S2C::HonorStatus, payload) &&
               protocol::parse_honor_status(payload.data(), payload.size(), honor);
    };
    expect(receive_honor() && honor.honor_points == 0 && honor.reward_points == 0 &&
               honor.rank == protocol::HonorRank::Unranked,
           "new characters enter the local world with an empty Honor snapshot");
    for (int n = 0; n < 2;) {
        expect(wait_frame(*session, rx, protocol::S2C::InsertItem, payload),
               "receive starter instance");
        protocol::PacketReader reader(payload);
        std::uint32_t iid = 0, info = 0;
        std::uint16_t index = 0;
        std::uint8_t prefix = 0;
        expect(reader.read_u32(iid) && reader.read_u16(index) && reader.read_u8(prefix) &&
                   reader.read_u32(info),
               "parse login instance flags");
        if (index == server::kMoneyItemIndex) {
            continue;
        }
        ++n;
        expect(protocol::insert_character_bound(info),
               "login transmits character binding for worn items and consumables");
    }
    protocol::CreateNpcInfo npc;
    std::uint32_t honor_npc = 0;
    for (int i = 0; i < 2; ++i) {
        protocol::CreateNpcInfo nearby;
        expect(wait_frame(*session, rx, protocol::S2C::CreateNpc, payload) &&
                   protocol::Quest::parse_create_npc(payload.data(), payload.size(), nearby),
               "receive the local shop NPC");
        if (nearby.index == 1) {
            npc = nearby;
        } else if (nearby.index == 2) {
            honor_npc = nearby.id;
        }
    }
    expect(npc.id != 0 && honor_npc != 0, "find the ordinary and Honor merchants");
    expect(session->send_all(protocol::Storage::frame_info_c2s(npc.id)) &&
               wait_frame(*session, rx, protocol::S2C::StorageInfo, payload),
           "open shared account storage");
    expect(session->send_all(protocol::Storage::frame_transfer_c2s(true, npc.id, consumable, 1)) &&
               barrier(),
           "attempt to deposit part of a bound starter stack");
    expect(store->open(config.save_dir), "inspect character-bound deposit rejection");
    expect(store->load_storage_items(account->id).empty() &&
               store->load_items(hero.id).size() == 2 &&
               store->load_character(hero.id)->money == 100,
           "bound deposit leaves inventory, account storage and transfer fee unchanged");
    store->close();
    expect(session->send_all(protocol::Shop::frame_item_list_c2s(npc.id)) &&
               wait_frame(*session, rx, protocol::S2C::ItemList, payload),
           "open merchant");
    expect(session->send_all(protocol::Shop::frame_sell_c2s(npc.id, consumable, 1)) && barrier(),
           "attempt to sell a bound starter item");
    rows = inspect();
    expect(rows.size() == 2 && rows.front().count == 2,
           "merchant rejects character-bound starter items");
    expect(session->send_all(protocol::DropItem::frame_c2s(consumable, 1)) && barrier() &&
               inspect().size() == 2,
           "character-bound starters cannot be dropped");
    expect(session->send_all(protocol::UseItem::frame_c2s(consumable)) && barrier(),
           "consume starter writ");
    rows = inspect();
    const auto used = std::find_if(rows.begin(), rows.end(), [=](const auto& item) {
        return item.iid == static_cast<std::int32_t>(consumable);
    });
    expect(used != rows.end() && used->count == 1 && used->character_bound,
           "starter consumption persists the lower count and binding");
    expect(session->send_all(protocol::Shop::frame_buy_c2s(npc.id, 9000, 3)) && barrier(),
           "buy ordinary versions of the starter item");
    expect(inspect().size() == 3, "ordinary rewards do not merge into bound starter stacks");
    rows = inspect();
    const auto bought = std::find_if(rows.begin(), rows.end(), [](const auto& item) {
        return item.index == 9000 && !item.character_bound && !item.account_bound;
    });
    const std::int32_t bought_iid = bought == rows.end() ? 0 : bought->iid;
    expect(bought != rows.end() && bought->count == 3,
           "ordinary purchase creates one separate unbound stack");
    const auto balance = [&] {
        expect(store->open(config.save_dir), "inspect merchant balance");
        const auto saved = store->load_character(hero.id);
        store->close();
        expect(saved.has_value(), "merchant character remains persisted");
        return saved ? saved->money : -1;
    };
    expect(balance() == 97, "ordinary purchase charges all three one-Geon units");
    if (bought_iid > 0) {
        expect(session->send_all(protocol::Shop::frame_sell_c2s(npc.id, bought_iid, 1)) &&
                   barrier(),
               "sell part of the newly purchased ordinary stack");
        rows = inspect();
        const auto remainder =
            std::find_if(rows.begin(), rows.end(), [bought_iid](const auto& item) {
                return item.iid == bought_iid;
            });
        expect(remainder != rows.end() && remainder->count == 2 && !remainder->character_bound &&
                   balance() == 98,
               "partial ordinary sale retains its IID and pays exactly one Geon");
        expect(session->send_all(protocol::Shop::frame_sell_c2s(npc.id, bought_iid, 2)) &&
                   barrier(),
               "sell the complete ordinary remainder");
        rows = inspect();
        expect(std::none_of(rows.begin(),
                            rows.end(),
                            [bought_iid](const auto& item) { return item.iid == bought_iid; }) &&
                   rows.size() == 2 && balance() == 100,
               "full ordinary sale removes only the unbound row and restores its "
               "three-Geon value");
    }
    protocol::ShopItemList honor_list;
    expect(session->send_all(protocol::Quest::frame_ask_npc(honor_npc)) &&
               wait_frame(*session, rx, protocol::S2C::SendHtml, payload) &&
               session->send_all(protocol::Shop::frame_item_list_c2s(honor_npc)) &&
               wait_frame(*session, rx, protocol::S2C::ItemList, payload) &&
               protocol::Shop::parse_item_list_s2c(payload.data(), payload.size(), honor_list) &&
               honor_list.honor && honor_list.offers.size() == 1 &&
               honor_list.offers.front().minimum_rank == protocol::HonorRank::Corporal &&
               honor_list.offers.front().buy == 400,
           "Honor merchant advertises Reward Points and the required rank");
    expect(session->send_all(protocol::Chatting::frame_c2s("/getitemid 23032 2")) && barrier(),
           "give the single-player test character two coupons");
    rows = inspect();
    const auto coupon = std::find_if(
        rows.begin(), rows.end(), [](const auto& item) { return item.index == 23032; });
    const auto coupon_id = coupon == rows.end() ? 0 : static_cast<std::uint32_t>(coupon->iid);
    expect(coupon_id && session->send_all(protocol::UseItem::frame_c2s(coupon_id)) &&
               receive_honor() && honor.honor_points == 0 && honor.weekly_honor == 0 &&
               honor.reward_points == 1000 && honor.rank == protocol::HonorRank::Unranked,
           "redeeming a coupon increases only Reward Points");
    rows = inspect();
    expect(std::any_of(rows.begin(),
                       rows.end(),
                       [&](const auto& item) {
                           return item.iid == static_cast<int>(coupon_id) && item.count == 1;
                       }),
           "one coupon remains on the same stack");
    const auto purchase = protocol::Shop::frame_buy_c2s(honor_npc, 9002, 2);
    expect(session->send_all(purchase) && barrier() && inspect().size() == 3,
           "the server rejects a purchase below the minimum rank despite sufficient points");
    expect(session->send_all(protocol::Chatting::frame_c2s("/honor +=12090")) && receive_honor() &&
               honor.honor_points == 12090 && honor.weekly_honor == 12090 &&
               honor.reward_points == 1000 && honor.rank == protocol::HonorRank::Corporal,
           "single-player Honor addition immediately updates the snapshot");
    expect(session->send_all(purchase) && receive_honor() && honor.honor_points == 12090 &&
               honor.weekly_honor == 12090 && honor.reward_points == 200,
           "a qualifying bulk purchase costs exactly 800 Reward Points");
    expect(session->send_all(purchase) && barrier(),
           "try the same purchase with insufficient points");
    rows = inspect();
    expect(std::any_of(rows.begin(),
                       rows.end(),
                       [](const auto& item) { return item.index == 9002 && item.count == 2; }),
           "insufficient points leave the awarded item count unchanged");
    expect(session->send_all(protocol::Chatting::frame_c2s("/honor -=100")) && receive_honor() &&
               honor.honor_points == 11990 && honor.rank == protocol::HonorRank::Specialist,
           "single-player Honor subtraction immediately updates the snapshot");
    expect(session->send_all(protocol::UseItem::frame_c2s(coupon_id)) && receive_honor() &&
               honor.reward_points == 1200 && honor.honor_points == 11990,
           "the last coupon credits the same wallet");
    expect(session->send_all(protocol::UseItem::frame_c2s(coupon_id)) && barrier(),
           "replay the use packet for the exhausted coupon stack");
    session->close();
    host.stop();
    expect(host.start(config) && connect() && receive_honor() && barrier(),
           "reconnect after starter consumption");
    expect(honor.honor_points == 11990 && honor.reward_points == 1200,
           "Honor and Reward Points survive restart without a duplicate coupon credit");
    rows = inspect();
    const auto remaining = std::find_if(rows.begin(), rows.end(), [=](const auto& item) {
        return item.iid == static_cast<std::int32_t>(consumable);
    });
    expect(remaining != rows.end() && remaining->count == 1,
           "relogin does not replenish consumed starter quantities");
    expect(rows.size() == 3 && balance() == 100 &&
               std::none_of(rows.begin(),
                            rows.end(),
                            [bought_iid](const auto& item) {
                                return item.iid == bought_iid || item.index == 23032;
                            }),
           "ordinary sales, money and the final coupon removal survive reconnect");
    session->close();
    host.stop();

    expect(store->open(config.save_dir), "empty inventory between sessions");
    for (const auto& item : store->load_items(hero.id)) {
        expect(store->delete_item(item.iid), "delete remaining starter inventory");
    }
    hero = *store->load_character(hero.id);
    hero.map_x += 20;
    const auto money = hero.money;
    expect(store->save_character(hero), "save changed spawn after the initial kit");
    store->close();
    expect(host.start(config) && connect() && barrier(),
           "reconnect with an entirely empty inventory");
    expect(inspect().empty(), "empty inventory does not grant starters again");
    session->close();
    host.stop();
    expect(store->open(config.save_dir), "inspect once-only state after restart");
    hero = *store->load_character(hero.id);
    expect(hero.money == money && hero.map_x == config.spawn_x + 20,
           "relogin does not repeat money or reset the spawn");
    expect(!kit.grant_if_empty(*store, hero, defs), "durable marker blocks a direct repeat grant");
    store->close();

    sqlite3* legacy = nullptr;
    expect(sqlite3_open((config.save_dir + "/" + server::kSqliteFileName).c_str(), &legacy) ==
               SQLITE_OK,
           "open legacy schema fixture");
    expect(sqlite3_exec(legacy,
                        "ALTER TABLE characters DROP COLUMN starter_granted; "
                        "ALTER TABLE items DROP COLUMN character_bound; "
                        "DELETE FROM schema_version WHERE version=27",
                        nullptr,
                        nullptr,
                        nullptr) == SQLITE_OK,
           "restore pre-fix schema with an empty existing character");
    sqlite3_close(legacy);
    expect(store->open(config.save_dir), "migrate the legacy database");
    expect(!kit.grant_if_empty(*store, hero, defs) && store->load_items(hero.id).empty(),
           "legacy characters with empty bags are never treated as new characters");
    for (const auto& item : store->load_items(retry.id)) {
        expect(!item.character_bound, "migration does not bind unrelated legacy inventory");
    }
    server::CharacterRecord fresh;
    fresh.name = "AfterMigration";
    fresh.account_id = account->id;
    expect(store->save_character(fresh), "create character after migration");
    fresh = store->list_characters(account->id).back();
    expect(kit.grant_if_empty(*store, fresh, defs),
           "new characters remain eligible after migration");
    store->close();
    std::filesystem::remove_all(root);
    return test::g_failures == 0 ? 0 : 1;
}
