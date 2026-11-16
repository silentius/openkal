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
#include "openkal/protocol/Attack.h"
#include "openkal/protocol/Chatting.h"
#include "openkal/protocol/CreatePlayer.h"
#include "openkal/protocol/DropItem.h"
#include "openkal/protocol/EnchantItem.h"
#include "openkal/protocol/FrameParser.h"
#include "openkal/protocol/GameExit.h"
#include "openkal/protocol/Handshake.h"
#include "openkal/protocol/InsertItem.h"
#include "openkal/protocol/ItemWear.h"
#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketReader.h"
#include "openkal/protocol/Property.h"
#include "openkal/protocol/Quest.h"
#include "openkal/protocol/Skill.h"
#include "openkal/protocol/TrashItem.h"
#include "openkal/protocol/UseItem.h"
#include "openkal/server/CollectionEquipment.h"
#include "openkal/server/ServerHost.h"
#include "openkal/server/net/Transport.h"
#include "openkal/server/store/DataStore.h"
#include "test_util.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>

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
    const auto root = test::temp_dir(("inventory_server_" + std::to_string(getpid())).c_str());
    server::ServerConfig config;
    config.maps_dir = root + "/maps";
    config.monsters_dir = root + "/monsters";
    config.items_dir = root + "/items";
    config.quests_dir = root + "/quests";
    config.save_dir = root + "/save";
    config.unix_path = root + "/game.sock";
    config.store = server::StoreBackend::Sqlite;
    config.transport = server::TransportKind::Unix;
    config.spawn_x = config.spawn_y = 15 * map::kTileSize;
    config.map_radius_cells = 1;
    config.max_monsters = 0;
    for (const auto& dir :
         {config.maps_dir, config.items_dir, config.monsters_dir, config.quests_dir}) {
        test::mkdir_p(dir);
    }
    expect(test::write_open_area(config.maps_dir + "/n_000_000.akarea"),
           "create inventory server map");
    {
        std::ofstream file(config.monsters_dir + "/1.json");
        file
            << R"({"index":1,"name":"Unused test monster","stats":{"level":1,"hp":1},"spawns":[]})";
    }
    const char* definitions[] = {
        R"({"index":9000,"name":"General fixture","class":["general","etc"],"stackable":0})",
        R"({"index":9001,"name":"Quest fixture","class":["quest","etc"],"stackable":0})",
        R"({"index":9002,"name":"Task fixture","class":["quest","etc"],"stackable":0})"};
    for (int i = 0; i < 3; ++i) {
        std::ofstream file(config.items_dir + "/" + std::to_string(9000 + i) + ".json");
        file << definitions[i];
    }
    {
        std::ofstream out(config.items_dir + "/9006.json");
        out << R"({"index":9006,"name":"Unlock fixture","class":["general","etc"],"stackable":0,"use":1,"specialty":{"buff":[100, 86400, 0]}})";
    }
    {
        std::ofstream out(config.quests_dir + "/11.json");
        out << R"({"index":11,"flag":1,"cases":[{"then":[{"op":"item","in":[[9002, 0, 1], [9006, 0, 1]]}]}]})";
    }
    auto store = server::create_data_store(server::StoreBackend::Sqlite);
    expect(store && store->open(config.save_dir), "open inventory persistence");
    server::CharacterRecord hero;
    hero.name = "Inventory";
    hero.level = 90;
    hero.hp = hero.mp = 100;
    hero.map_x = hero.map_y = config.spawn_x;
    expect(store->ensure_default_player("inventory", "inventory", hero),
           "create inventory character");
    const auto account = store->find_account("inventory");
    if (!account) {
        return 1;
    }
    const auto characters = store->list_characters(account->id);
    if (characters.empty()) {
        return 1;
    }
    config.character_id = characters.front().id;
    for (int section = 0; section < 3; ++section) {
        const int count = section == 0 ? 72 : 36;
        for (int i = 0; i < count; ++i) {
            server::ItemRecord item;
            item.character_id = config.character_id;
            item.iid = 1000 + section * 1000 + i;
            item.index = section == 0 && i < 5 ? 9006 : 9000 + section;
            item.count = 1;
            expect(store->insert_item(item), "seed independent full inventory section");
        }
    }
    store->close();
    server::ServerHost host;
    expect(host.start(config), "start inventory server");
    for (int attempt = 0; attempt < 200 && !std::filesystem::exists(config.unix_path); ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    auto session = server::connect_unix(config.unix_path);
    if (!session) {
        expect(false, "connect inventory client");
        host.stop();
        return 1;
    }
    std::vector<std::uint8_t> rx, payload;
    expect(session->send_all(protocol::Handshake::build_connect(0)) &&
               wait_frame(*session, rx, protocol::S2C::Code, payload),
           "inventory handshake");
    protocol::S2CCodeFields code;
    expect(protocol::Handshake::parse_code(payload.data(), payload.size(), code),
           "parse inventory handshake");
    expect(session->send_all(protocol::Handshake::build_answer(code.protocol_test_value)) &&
               wait_frame(*session, rx, protocol::S2C::CreatePlayer, payload),
           "enter inventory world");
    std::unordered_set<std::uint32_t> received;
    for (int i = 0; i < 144; ++i) {
        if (!wait_frame(*session, rx, protocol::S2C::InsertItem, payload)) {
            expect(false, "receive every General and Other instance at login");
            break;
        }
        protocol::PacketReader reader(payload.data(), payload.size());
        std::uint32_t iid = 0;
        expect(reader.read_u32(iid) && received.insert(iid).second,
               "login preserves unique instance identities");
    }
    expect(received.size() == 144 && received.count(1071) && received.count(2035) &&
               received.count(3035),
           "72 General plus 36 Quest plus 36 protected Task Items coexist and restore completely");
    const auto barrier = [&] {
        return session->send_all(protocol::GameExit::frame_client_request()) &&
               wait_frame(*session, rx, protocol::S2C::AnsGameExit, payload);
    };
    const auto items = [&] {
        expect(store->open(config.save_dir), "inspect inventory persistence");
        auto result = store->load_items(config.character_id);
        store->close();
        return result;
    };
    const auto grant = [&](int index, int count) {
        return session->send_all(protocol::Chatting::frame_c2s(
                   "/getitemid " + std::to_string(index) + " 77 " + std::to_string(count))) &&
               barrier();
    };
    for (int index : {9000, 9001, 9002}) {
        expect(grant(index, 1) && items().size() == 144,
               "a full category refuses an extra stack independently");
    }
    for (int expansion = 0; expansion < 4; ++expansion) {
        expect(session->send_all(protocol::UseItem::frame_c2s(1000 + expansion)) && barrier(),
               "consume one existing writ for the next General block");
        expect(grant(9000, 37), "fill the newly activated block and the consumed writ cell");
        expect(items().size() == static_cast<std::size_t>(144 + (expansion + 1) * 36),
               "each writ adds exactly 36 General places and consumes only its own instance");
        expect(grant(9000, 1) &&
                   items().size() == static_cast<std::size_t>(144 + (expansion + 1) * 36),
               "General refuses deposits beyond the current active block limit");
    }
    expect(store->open(config.save_dir), "inspect active expansion persistence");
    expect(store->load_inventory_unlocks(config.character_id).size() == 4,
           "all four timed expansions persist");
    store->close();
    expect(session->send_all(protocol::UseItem::frame_c2s(1004)) && barrier() &&
               items().size() == 288,
           "fully expanded General cannot consume a fifth writ");
    expect(grant(9002, 1) && items().size() == 288,
           "all 216 General and independent Quest/Task cells can be occupied together");
    expect(session->send_all(protocol::TrashItem::frame_c2s(3000)) && barrier() &&
               items().size() == 288,
           "server refuses deletion of protected Task Items");
    expect(session->send_all(protocol::DropItem::frame_c2s(3000, 1)) && barrier() &&
               items().size() == 288,
           "server refuses dropping protected Task Items");
    expect(session->send_all(protocol::TrashItem::frame_c2s(1005)) && barrier(),
           "free one General cell");
    expect(grant(9006, 1) && items().size() == 288,
           "normal reward consumables use free General cells instead of Other");
    session->close();
    host.stop();
    expect(store->open(config.save_dir) &&
               store->replace_inventory_unlocks(config.character_id, {}),
           "expire all General expansions between sessions");
    for (int i = 0; i < 36; ++i) {
        server::ItemRecord legacy;
        legacy.iid = 4000 + i;
        legacy.character_id = config.character_id;
        legacy.index = 9006;
        legacy.prefix = 45;
        legacy.count = 1;
        legacy.enchant_protected = true;
        expect(store->insert_item(legacy),
               "seed legacy consumable overflow outside General capacity");
    }
    store->close();
    expect(host.start(config), "restart with expired General expansions");
    for (int attempt = 0; attempt < 200 && !std::filesystem::exists(config.unix_path); ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    session = server::connect_unix(config.unix_path);
    if (session) {
        rx.clear();
        expect(session->send_all(protocol::Handshake::build_connect(0)) &&
                   wait_frame(*session, rx, protocol::S2C::Code, payload) &&
                   protocol::Handshake::parse_code(payload.data(), payload.size(), code) &&
                   session->send_all(protocol::Handshake::build_answer(code.protocol_test_value)) &&
                   wait_frame(*session, rx, protocol::S2C::CreatePlayer, payload),
               "reconnect after expiry");
        received.clear();
        for (int i = 0; i < 288; ++i) {
            if (!wait_frame(*session, rx, protocol::S2C::InsertItem, payload)) {
                break;
            }
            protocol::PacketReader reader(payload.data(), payload.size());
            std::uint32_t iid = 0;
            if (reader.read_u32(iid)) {
                received.insert(iid);
            }
        }
        expect(received.size() == 288,
               "login still sends every item from expired blocks and Other");
        expect(store->open(config.save_dir), "inspect migrated legacy overflow");
        const auto stored_overflow = store->load_storage_items(account->id);
        auto preserved = store->load_items(config.character_id);
        preserved.insert(preserved.end(), stored_overflow.begin(), stored_overflow.end());
        expect(stored_overflow.size() == 36 && preserved.size() == 324,
               "legacy General overflow moves to account storage without losing instances");
        for (int i = 0; i < 36; ++i) {
            const auto found = std::find_if(preserved.begin(),
                                            preserved.end(),
                                            [i](const auto& item) { return item.iid == 4000 + i; });
            expect(found != preserved.end() && found->prefix == 45 && found->count == 1 &&
                       found->enchant_protected,
                   "overflow migration preserves exact instance identity and metadata");
        }
        store->close();
        expect(grant(9000, 1) && items().size() == 288,
               "expired overflow cannot grant new General deposits");
        session->close();
    } else {
        expect(false, "reconnect inventory client");
    }
    host.stop();
    // A separate full-inventory scenario exercises ordinary equipment and persistence.
    const char* equipment_defs[] = {
        R"({"index":9003,"name":"Sword fixture","class":["weapon","sword"],"wear":1,"endurance":50})",
        R"({"index":9004,"name":"Armor fixture","class":["defense","upperarmor"],"wear":1,"endurance":60})",
        R"({"index":9005,"name":"Ring fixture","class":["ornament","ring"],"wear":1})"};
    for (int i = 0; i < 3; ++i) {
        std::ofstream file(config.items_dir + "/" + std::to_string(9003 + i) + ".json");
        file << equipment_defs[i];
    }
    expect(store->open(config.save_dir), "prepare equipment inventory scenario");
    for (const auto& item : store->load_items(config.character_id)) {
        expect(store->delete_item(item.iid), "clear previous capacity fixture");
    }
    for (int i = 0; i < 73; ++i) {
        server::ItemRecord item;
        item.character_id = config.character_id;
        item.iid = 5000 + i;
        item.index = i == 0 || i == 72 ? 9003 : i == 1 ? 9004 : i == 2 ? 9005 : 9000;
        item.count = 1;
        item.slot = i == 72 ? 1 : 0;
        item.prefix = i == 0 ? 7 : 11;
        item.cur_end = 23;
        item.max_end = 50;
        item.x_attack = i == 0 ? 12 : 3;
        item.upgrade_level = 2;
        item.random_stat_type = 3;
        item.random_stat_value = 4;
        item.enchant_protected = true;
        expect(store->insert_item(item), "seed full General plus worn weapon");
    }
    store->close();
    const auto connect_equipment = [&](int expected) {
        expect(host.start(config), "start equipment inventory server");
        for (int attempt = 0; attempt < 200 && !std::filesystem::exists(config.unix_path);
             ++attempt) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        session = server::connect_unix(config.unix_path);
        if (!session) {
            return false;
        }
        rx.clear();
        if (!session->send_all(protocol::Handshake::build_connect(0)) ||
            !wait_frame(*session, rx, protocol::S2C::Code, payload) ||
            !protocol::Handshake::parse_code(payload.data(), payload.size(), code) ||
            !session->send_all(protocol::Handshake::build_answer(code.protocol_test_value)) ||
            !wait_frame(*session, rx, protocol::S2C::CreatePlayer, payload)) {
            return false;
        }
        received.clear();
        for (int i = 0; i < expected; ++i) {
            if (!wait_frame(*session, rx, protocol::S2C::InsertItem, payload)) {
                return false;
            }
            protocol::PacketReader reader(payload.data(), payload.size());
            std::uint32_t iid = 0;
            if (reader.read_u32(iid)) {
                received.insert(iid);
            }
        }
        return received.size() == static_cast<std::size_t>(expected);
    };
    const auto row = [&](int iid) {
        for (const auto& item : items()) {
            if (item.iid == iid) {
                return item;
            }
        }
        return server::ItemRecord{};
    };
    if (connect_equipment(73)) {
        expect(received.count(5072) && grant(9000, 1) && items().size() == 73,
               "login restores full General and independent worn equipment without extra inventory "
               "capacity");
        expect(session->send_all(protocol::ItemWear::frame_client_put_on(5000)) && barrier() &&
                   row(5000).slot == 1 && row(5072).slot == 0 && items().size() == 73,
               "full General permits exact-instance weapon replacement using the incoming cell");
        expect(
            session->send_all(protocol::ItemWear::frame_client_put_on(5000)) &&
                wait_frame(*session, rx, protocol::S2C::PutOnItem, payload) && barrier() &&
                row(5000).slot == 1 && row(5072).slot == 0 && items().size() == 73,
            "repeating PutOn for the worn instance acknowledges without moving or duplicating it");
        expect(row(5000).prefix == 7 && row(5000).x_attack == 12 && row(5072).prefix == 11 &&
                   row(5000).cur_end == 23 && row(5000).max_end == 50 &&
                   row(5000).random_stat_value == 4 && row(5000).enchant_protected,
               "weapon swap retains instance enchantments, endurance, prefix and protection");
        expect(session->send_all(protocol::ItemWear::frame_client_put_off(5000)) && barrier() &&
                   row(5000).slot == 1,
               "full General refuses ordinary unequip and leaves the instance worn");
        expect(session->send_all(protocol::ItemWear::frame_client_put_on(5001)) && barrier() &&
                   row(5001).slot == 2 && grant(9000, 1) && items().size() == 74,
               "armor equips outside General and releases one writable cell");
        expect(grant(9000, 1) && items().size() == 74,
               "equipment storage cannot be used as additional ordinary inventory cells");
        expect(session->send_all(protocol::ItemWear::frame_client_put_on(5002)) && barrier() &&
                   row(5002).slot == 2 && grant(9000, 1) && items().size() == 75,
               "ornaments also release their General cell when equipped");
        expect(session->send_all(protocol::TrashItem::frame_c2s(5003)) && barrier() &&
                   session->send_all(protocol::ItemWear::frame_client_put_off(5000)) && barrier() &&
                   row(5000).slot == 0 && grant(9000, 1) && items().size() == 74,
               "unequip uses the freed General cell without granting extra capacity");
        // A rejected pair must roll back the first saved update as well.
        expect(store->open(config.save_dir), "open equipment transaction verification");
        auto saved = store->load_items(config.character_id);
        auto first = saved.front();
        const int original_slot = first.slot;
        first.slot = original_slot == 0 ? 1 : 0;
        auto missing = first;
        missing.iid = 99999999;
        expect(!store->update_equipment_items({first, missing}),
               "failed equipment pair rejects the transaction");
        saved = store->load_items(config.character_id);
        const auto unchanged = std::find_if(
            saved.begin(), saved.end(), [&](const auto& item) { return item.iid == first.iid; });
        expect(unchanged != saved.end() && unchanged->slot == original_slot,
               "failed equipment pair preserves the first instance location");
        store->close();
        session->close();
        host.stop();
        if (connect_equipment(74)) {
            expect(
                received.count(5000) && received.count(5001) && received.count(5002) &&
                    row(5000).slot == 0 && row(5001).slot == 2 && row(5002).slot == 2,
                "relogin restores exact equipment and inventory identities when General is full");
            expect(grant(9000, 1) && items().size() == 74,
                   "relogin does not count worn armor or ornaments as General cells");
            session->close();
        } else {
            expect(false, "reconnect ordinary equipment client");
        }
        host.stop();
    } else {
        expect(false, "connect ordinary equipment client");
        host.stop();
    }
    // Owned reward fixtures exercise legacy resolution and atomic rejection over the wire.
    config.npcs_dir = root + "/npcs";
    test::mkdir_p(config.npcs_dir);
    for (int index : {48, 23000, 23001}) {
        std::ofstream out(config.items_dir + "/" + std::to_string(index) + ".json");
        out << "{\"index\":" << index
            << R"(,"name":"Owned reward","class":["general","etc"],"stackable":1})";
    }
    for (int index : {12, 13, 14}) {
        std::ofstream npc(config.npcs_dir + "/" + std::to_string(index) + ".json");
        npc << "{\"index\":" << index << ",\"kind\":3,\"quest\":[" << index << ", 1],\"xy\":["
            << config.spawn_x << ", " << config.spawn_y << ", 0]}";
        std::ofstream quest(config.quests_dir + "/" + std::to_string(index) + ".json");
        quest
            << "{\"index\":" << index
            << R"(,"flag":1,"cases":[{"if":[{"op":"notclear","quest":12}],"then":[{"op":"item","out":[[9001, 1]],"in":[[31, 0, 137], )"
            << (index == 12   ? "[47, 0, 2], [11047, 0, 3], [48, 0, 1]"
                : index == 13 ? "[29999, 0, 1]"
                              : "[2055, 0, 1]")
            << R"(]},{"op":"clear","quest":12,"flag":1},{"op":"html","id":1234}]}]})";
    }
    expect(store->open(config.save_dir), "prepare quest reward inventory");
    for (const auto& item : store->load_items(config.character_id)) {
        expect(store->delete_item(item.iid), "clear equipment fixture");
    }
    const auto beforeQuest = store->load_character(config.character_id);
    for (int i = 0; i < 72; ++i) {
        server::ItemRecord item;
        item.character_id = config.character_id;
        item.iid = 6000 + i;
        item.index = i == 71 ? 9001 : 9000;
        item.count = 1;
        expect(store->insert_item(item), "seed delivery item and 71 General cells");
    }
    store->close();
    if (connect_equipment(72)) {
        std::unordered_map<int, std::uint32_t> npcs;
        for (int i = 0; i < 3; ++i) {
            protocol::CreateNpcInfo npc;
            if (wait_frame(*session, rx, protocol::S2C::CreateNpc, payload) &&
                protocol::Quest::parse_create_npc(payload.data(), payload.size(), npc)) {
                npcs[npc.index] = npc.id;
            }
        }
        expect(npcs.size() == 3, "receive the reward fixture NPCs");
        const auto rejected = [&](int index, const std::string& reason) {
            std::string speaker, message;
            expect(npcs.count(index) &&
                       session->send_all(protocol::Quest::frame_ask_npc(npcs.at(index))) &&
                       wait_frame(*session, rx, protocol::S2C::Chatting, payload) &&
                       protocol::Chatting::parse_s2c(
                           payload.data(), payload.size(), speaker, message) &&
                       speaker == "Server" && message == reason && barrier(),
                   "rejected quest sends the precise reason before the protocol barrier");
            expect(row(6071).count == 1 && items().size() == 72,
                   "rejected exchange preserves the parcel and grants no partial items");
            expect(store->open(config.save_dir), "inspect rejected quest currency");
            const auto character = store->load_character(config.character_id);
            expect(character && beforeQuest && character->money == beforeQuest->money,
                   "rejected exchange preserves currency");
            store->close();
        };
        rejected(13, "Quest reward unavailable: item 29999.");
        rejected(14, "Quest reward unavailable: item 2055.");
        rejected(12, "Make room in your inventory before continuing this quest.");
        expect(session->send_all(protocol::TrashItem::frame_c2s(6000)) && barrier(),
               "free the second reward cell");
        protocol::QuestFlagEntry flag;
        expect(npcs.count(12) && session->send_all(protocol::Quest::frame_ask_npc(npcs.at(12))) &&
                   wait_frame(*session, rx, protocol::S2C::SaveQuest, payload) &&
                   protocol::Quest::parse_save_quest(payload.data(), payload.size(), flag) &&
                   flag.quest == 12 && flag.clear != 0,
               "retry succeeds and clears the delivery after freeing reward space");
        std::int32_t html = 0;
        std::uint32_t source = 0;
        expect(
            wait_frame(*session, rx, protocol::S2C::SendHtml, payload) &&
                protocol::Quest::parse_send_html(payload.data(), payload.size(), html, &source) &&
                html == 1234 && source == npcs[12] && barrier(),
            "successful delivery opens its completion dialog for the source NPC");
        const auto rewards = items();
        const auto count = [&](int index) {
            int result = 0;
            for (const auto& item : rewards) {
                if (item.index == index) {
                    result += item.count;
                }
            }
            return result;
        };
        expect(count(23000) == 5 && count(47) == 0 && count(11047) == 0,
               "both historical medicine IDs resolve and stack under the current definition");
        expect(count(48) == 1 && count(23001) == 0,
               "an explicitly loaded definition takes precedence over migration");
        expect(count(9001) == 0 && count(31) == 0,
               "completed delivery consumes the parcel without creating a currency item");
        expect(store->open(config.save_dir), "inspect completed quest currency");
        const auto character = store->load_character(config.character_id);
        expect(character && beforeQuest && character->money == beforeQuest->money + 137,
               "failed attempts followed by success grant currency exactly once");
        store->close();
        session->close();
    } else {
        expect(false, "connect quest reward inventory client");
    }
    host.stop();
    std::filesystem::remove_all(root);
    return test::g_failures == 0 ? 0 : 1;
}
