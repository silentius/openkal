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
#include "openkal/protocol/Chatting.h"
#include "openkal/protocol/CreatePlayer.h"
#include "openkal/protocol/FrameParser.h"
#include "openkal/protocol/GameExit.h"
#include "openkal/protocol/Handshake.h"
#include "openkal/protocol/ItemWear.h"
#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/Property.h"
#include "openkal/protocol/TrashItem.h"
#include "openkal/server/ServerHost.h"
#include "openkal/server/net/Transport.h"
#include "openkal/server/store/DataStore.h"
#include "test_util.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <thread>
#include <unistd.h>

namespace {
    using namespace openkal;
    using test::expect;

    struct Peer {
        std::unique_ptr<server::ISession> session;
        std::vector<std::uint8_t> rx;
        std::map<int, int> stats;
        std::vector<std::uint8_t> payload;

        bool wait(protocol::S2C wanted) {
            const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(3);
            do {
                if (!session->recv_into(rx)) {
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
                    if (type == static_cast<std::uint8_t>(protocol::S2C::UpdateProperty)) {
                        protocol::UpdatePropertyFields value;
                        if (protocol::Property::parse_update_any(
                                payload.data(), payload.size(), value)) {
                            stats[value.property] =
                                value.property == static_cast<int>(protocol::PropertyType::Hp)
                                    ? value.vital_max
                                    : value.word_value;
                        }
                    }
                    if (type == static_cast<std::uint8_t>(wanted)) {
                        return true;
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            } while (std::chrono::steady_clock::now() < until);
            return false;
        }

        bool barrier() {
            return session->send_all(protocol::GameExit::frame_client_request()) &&
                   wait(protocol::S2C::AnsGameExit);
        }
    };
}  // namespace

int main() {
    using namespace openkal;
    const auto root = test::temp_dir(("skin_server_" + std::to_string(getpid())).c_str());
    server::ServerConfig config;
    config.maps_dir = root + "/maps";
    config.monsters_dir = root + "/monsters";
    config.items_dir = root + "/items";
    config.save_dir = root + "/save";
    config.unix_path = root + "/skin.sock";
    config.store = server::StoreBackend::Sqlite;
    config.transport = server::TransportKind::Unix;
    config.spawn_x = config.spawn_y = 15 * map::kTileSize;
    config.map_radius_cells = 1;
    config.max_monsters = 0;
    for (const auto& dir : {config.maps_dir, config.monsters_dir, config.items_dir}) {
        test::mkdir_p(dir);
    }
    expect(test::write_open_area(config.maps_dir + "/n_000_000.akarea"), "create skin server area");
    std::ofstream(config.monsters_dir + "/1.json")
        << R"({"index":1,"name":"Synthetic","stats":{"level":1,"hp":1},"spawns":[]})";
    const char* definitions[] = {
        R"({"index":9100,"class":["skin","helmet"],"wear":1,"specialty":{"str":9000}})",
        R"({"index":9101,"class":["skin","boots"],"wear":1})",
        R"({"index":9102,"class":["skin","body"],"wear":1,"specialty":{"hp":50000,"str":9000}})",
        R"({"index":9103,"class":["skin","upper"],"wear":1})",
        R"({"index":9104,"class":["skin","weapon"],"skin":{"weapon_type":"sword"},"wear":1})",
        R"({"index":9105,"class":["weapon","sword"],"wear":1})",
        R"({"index":9106,"class":["skin","body"],"wear":1,"limit":{"class":"mage"}})",
        R"({"index":9107,"class":["skin","face"],"wear":1})",
        R"({"index":9108,"class":["general","etc"],"stackable":0})",
        R"({"index":9109,"class":["weapon","sword2h"],"wear":1})",
        R"({"index":9110,"class":["skin","weapon"],"skin":{"weapon_type":"sword2h"},"wear":1})",
        R"({"index":9111,"class":["skin","weapon"],"wear":1})"};
    for (int i = 0; i < 12; ++i) {
        std::ofstream(config.items_dir + "/" + std::to_string(i) + ".json") << definitions[i];
    }
    auto store = server::create_data_store(config.store);
    expect(store && store->open(config.save_dir), "open skin persistence");
    server::CharacterRecord hero;
    hero.name = "Skins";
    hero.level = 90;
    hero.map_x = config.spawn_x;
    hero.map_y = config.spawn_y;
    expect(store->ensure_default_player("skin", "skin", hero), "create cosmetic character");
    const auto account = store->find_account("skin");
    if (!account) {
        return 1;
    }
    const auto characters = store->list_characters(account->id);
    if (characters.empty()) {
        return 1;
    }
    config.character_id = characters.front().id;
    for (int i = 0; i < 72; ++i) {
        server::ItemRecord row;
        row.character_id = config.character_id;
        row.iid = 1000 + i;
        row.index = 9100 + std::min(i, 8);
        if (i >= 10 && i <= 13) {
            row.index = i == 10 || i == 13 ? 9109 : (i == 11 ? 9110 : 9111);
        }
        row.count = 1;
        expect(store->insert_item(row), "seed full cosmetic test inventory");
    }
    store->close();
    server::ServerHost host;
    const auto connect = [&]() -> std::unique_ptr<Peer> {
        expect(host.start(config), "start cosmetic server");
        for (int attempt = 0; attempt < 200 && !std::filesystem::exists(config.unix_path);
             ++attempt) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        auto peer = std::make_unique<Peer>();
        peer->session = server::connect_unix(config.unix_path);
        if (!peer->session) {
            return {};
        }
        protocol::S2CCodeFields code;
        if (!peer->session->send_all(protocol::Handshake::build_connect(0)) ||
            !peer->wait(protocol::S2C::Code) ||
            !protocol::Handshake::parse_code(peer->payload.data(), peer->payload.size(), code) ||
            !peer->session->send_all(protocol::Handshake::build_answer(code.protocol_test_value)) ||
            !peer->wait(protocol::S2C::CreatePlayer)) {
            return {};
        }
        return peer;
    };
    auto peer = connect();
    if (!peer) {
        host.stop();
        return 1;
    }
    expect(peer->barrier(), "drain initial cosmetic inventory");
    const auto items = [&]() {
        expect(store->open(config.save_dir), "inspect cosmetic persistence");
        auto result = store->load_items(config.character_id);
        store->close();
        return result;
    };
    const auto slot = [&](int iid) {
        const auto rows = items();
        for (const auto& row : rows) {
            if (row.iid == iid) {
                return row.slot;
            }
        }
        return -1;
    };
    const auto equip = [&](int iid) {
        return peer->session->send_all(protocol::ItemWear::frame_client_put_on(iid)) &&
               peer->barrier();
    };
    expect(equip(1004) && slot(1004) == 0, "weapon skin requires an actual weapon");
    expect(equip(1005) && slot(1005) == 1, "equip actual weapon independently");
    const auto baseline = peer->stats;
    for (int iid : {1000, 1001, 1004, 1007}) {
        expect(equip(iid) && slot(iid) == 6, "equip individual cosmetic into independent storage");
    }
    for (auto property : {protocol::PropertyType::Str,
                          protocol::PropertyType::Hp,
                          protocol::PropertyType::MinAttack,
                          protocol::PropertyType::MaxAttack}) {
        expect(peer->stats[static_cast<int>(property)] == baseline.at(static_cast<int>(property)),
               "skin bonuses do not enter combat calculations");
    }
    expect(peer->session->send_all(protocol::Chatting::frame_c2s("/getitemid 9108 0 5")) &&
               peer->barrier(),
           "fill cells freed by cosmetic equipment");
    const auto before = items().size();
    expect(equip(1002) && slot(1002) == 0 && slot(1000) == 6 && slot(1001) == 6 &&
               slot(1007) == 6 && items().size() == before,
           "whole-body switch with insufficient return space leaves every instance unchanged");
    for (int iid : {1008, 1009}) {
        expect(peer->session->send_all(protocol::TrashItem::frame_c2s(iid)) && peer->barrier(),
               "free return space for displaced skins");
    }
    expect(equip(1002) && slot(1002) == 6 && slot(1000) == 0 && slot(1001) == 0 &&
               slot(1007) == 0 && slot(1004) == 6 && slot(1005) == 1,
           "whole-body switch atomically returns body skins and preserves real weapon and weapon "
           "skin");
    expect(equip(1003) && slot(1003) == 6 && slot(1002) == 0,
           "individual body skin automatically returns the whole-body skin");
    expect(equip(1006) && slot(1006) == 0, "server rejects a skin for another class");
    expect(equip(1003) && slot(1003) == 6, "replayed cosmetic equip is idempotent");
    peer->session->close();
    host.stop();
    peer = connect();
    expect(static_cast<bool>(peer), "reconnect cosmetic client");
    if (peer) {
        protocol::CreatePlayerFields appearance;
        expect(
            protocol::CreatePlayer::parse(peer->payload.data(), peer->payload.size(), appearance) &&
                appearance.skins[0] == 9104 && appearance.skins[3] == 9103,
            "login appearance snapshot restores cosmetic slots");
        expect(peer->barrier() && slot(1003) == 6 && slot(1004) == 6 && slot(1005) == 1,
               "relogin preserves cosmetic and actual equipment identities");
        expect(peer->session->send_all(protocol::ItemWear::frame_client_put_off(1003)) &&
                   peer->barrier() && slot(1003) == 6,
               "removing a skin into a full inventory is refused without item loss");
        peer->session->close();
    }
    host.stop();
    expect(store->open(config.save_dir), "open failed cosmetic transaction fixture");
    auto records = store->load_items(config.character_id);
    auto first = records.front();
    const int original_slot = first.slot;
    first.slot = original_slot == 6 ? 0 : 6;
    auto missing = first;
    missing.iid = 99999999;
    expect(!store->update_equipment_items({first, missing}), "failed multi-skin save rolls back");
    records = store->load_items(config.character_id);
    const auto restored = std::find_if(
        records.begin(), records.end(), [&](const auto& row) { return row.iid == first.iid; });
    expect(restored != records.end() && restored->slot == original_slot,
           "failed save preserves prior cosmetic location");
    store->close();
    peer = connect();
    expect(static_cast<bool>(peer), "connect weapon-type restriction client");
    if (peer) {
        expect(peer->barrier(), "drain weapon-type inventory");
        expect(equip(1010) && slot(1010) == 0 && slot(1005) == 1 && slot(1004) == 6,
               "server rejects changing to two-hand while one-hand skin is equipped");
        expect(equip(1011) && slot(1011) == 0 && slot(1004) == 6,
               "server rejects two-hand skin on an actual one-hand sword");
        expect(equip(1012) && slot(1012) == 0,
               "unclassified weapon skin is refused without displacing the existing skin");
        expect(peer->session->send_all(protocol::TrashItem::frame_c2s(1014)) && peer->barrier(),
               "free space to remove the one-hand skin");
        expect(peer->session->send_all(protocol::ItemWear::frame_client_put_off(1004)) &&
                   peer->barrier() && slot(1004) == 0,
               "remove one-hand skin before changing type");
        expect(equip(1010) && slot(1010) == 1 && slot(1005) == 0,
               "weapon type change succeeds after removing the incompatible skin");
        expect(equip(1011) && slot(1011) == 6,
               "matching two-hand skin equips over a two-hand sword");
        expect(equip(1005) && slot(1005) == 0 && slot(1011) == 6,
               "server rejects the reverse weapon type change with the skin equipped");
        expect(equip(1013) && slot(1013) == 1 && slot(1010) == 0 && slot(1011) == 6,
               "compatible actual weapon swaps preserve the matching cosmetic");
        peer->session->close();
    }
    host.stop();
    std::filesystem::remove_all(root);
    return test::g_failures == 0 ? 0 : 1;
}
