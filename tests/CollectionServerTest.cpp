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
#include "openkal/protocol/EnchantItem.h"
#include "openkal/protocol/FrameParser.h"
#include "openkal/protocol/GameExit.h"
#include "openkal/protocol/Handshake.h"
#include "openkal/protocol/InsertItem.h"
#include "openkal/protocol/ItemWear.h"
#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketReader.h"
#include "openkal/protocol/Property.h"
#include "openkal/protocol/Skill.h"
#include "openkal/protocol/TrashItem.h"
#include "openkal/server/CollectionEquipment.h"
#include "openkal/server/ServerHost.h"
#include "openkal/server/net/Transport.h"
#include "openkal/server/store/DataStore.h"
#include "test_util.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>
#include <unistd.h>
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
    const auto root =
        test::temp_dir(("collection_server_" + std::to_string(getpid()) + "_" +
                        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))
                           .c_str());
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
    expect(test::write_open_area(config.maps_dir + "/n_000_000.akarea"),
           "write collection server map");
    for (const auto& file : std::filesystem::directory_iterator(std::string(OPENKAL_REPO_ROOT) +
                                                                "/config/items/collection")) {
        std::filesystem::copy_file(
            file.path(), std::filesystem::path(config.items_dir) / file.path().filename());
    }
    for (const auto& table :
         {std::pair{"trigram", 15}, std::pair{"hanin", 2}, std::pair{"yeoiju", 20}}) {
        std::ofstream out(root + "/collection/" + table.first + ".json");
        out << "{\"success_percent\":[";
        for (int i = 0; i < table.second; ++i) {
            out << (i ? ",100" : "100");
        }
        out << "]}";
    }
    {
        std::ofstream file(config.monsters_dir + "/1.json");
        file
            << R"({"index":1,"name":"Unused audit monster","stats":{"level":1,"hp":1},"spawns":[]})";
    }
    auto store = server::create_data_store(server::StoreBackend::Sqlite);
    expect(store && store->open(config.save_dir), "open isolated collection persistence");
    server::CharacterRecord hero;
    hero.name = "Collector";
    hero.level = 90;
    hero.hp = 100;
    hero.mp = 100;
    hero.map_x = hero.map_y = config.spawn_x;
    expect(store->ensure_default_player("collector", "collector", hero),
           "create collection test character");
    const auto account = store->find_account("collector");
    if (!account) {
        return 1;
    }
    const auto characters = store->list_characters(account->id);
    if (characters.empty()) {
        return 1;
    }
    config.character_id = characters.front().id;
    const auto seed =
        [&](std::uint32_t iid, int index, int level = 0, int option = 0, int value = 0) {
            server::ItemRecord item;
            item.iid = iid;
            item.character_id = config.character_id;
            item.index = index;
            item.count = 1;
            item.upgrade_level = level;
            item.random_stat_type = option;
            item.random_stat_value = value;
            expect(store->insert_item(item), "seed collection instance");
        };
    seed(100, 24056);
    seed(101, 24059, 0, 106, 12);
    seed(102, 24059, 0, 112, 30);
    seed(103, 24059, 0, 112, 20);
    seed(104, 24059, 0, 106, 12);
    seed(200, 24003);
    seed(201, 24003);
    seed(202, 24004);
    seed(203, 24003);
    seed(210, 24057, 0, 100, 4);
    seed(211, 24057, 0, 100, 5);
    seed(220, server::kYeoijuAttack);
    seed(221, server::kYeoijuAttack);
    seed(222, server::kYeoijuFortification);
    for (int part = 0; part < 8; ++part) {
        seed(300 + part, 24027 + part, 5);
    }
    seed(310, 24051, 0, 107, 4);
    seed(311, 24000);
    server::ItemRecord weapon;
    weapon.iid = 400;
    weapon.character_id = config.character_id;
    weapon.index = 9000;
    weapon.count = 1;
    weapon.slot = 1;
    expect(store->insert_item(weapon), "seed PvP weapon");
    server::SkillRecord skill;
    skill.character_id = config.character_id;
    skill.skill_index = 3;
    skill.level = 1;
    expect(store->upsert_skill(skill), "learn PvP audit skill");
    {
        std::ofstream file(config.items_dir + "/9000.json");
        file
            << R"({"index":9000,"name":"Audit sword","class":["weapon","sword"],"wear":1,"range":100,"specialty":{"attack":[1,1]}})";
    }
    config.skills_dir = root + "/skills";
    test::mkdir_p(config.skills_dir);
    {
        std::ofstream file(config.skills_dir + "/3.json");
        file
            << R"({"class":0,"index":3,"name":"Lightning Slash","maxLevel":10,"mp":10,"type":["active","use"],"damageType":1,"property":[1,1,0,0,0,0,5,0],"action":3})";
    }
    const int initial_items = static_cast<int>(store->load_items(config.character_id).size());
    for (int i = initial_items; i < 72; ++i) {
        seed(1000 + i - initial_items, 9000);
    }
    expect(store->load_items(config.character_id).size() == 72, "start with a full base inventory");
    store->close();
    server::ServerHost host;
    expect(host.start(config), "start collection server");
    for (int attempt = 0; attempt < 200 && !std::filesystem::exists(config.unix_path); ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    auto session = server::connect_unix(config.unix_path);
    expect(session != nullptr, "connect collection client");
    if (!session) {
        return 1;
    }
    std::vector<std::uint8_t> rx, payload;
    expect(session->send_all(protocol::Handshake::build_connect(0)) &&
               wait_frame(*session, rx, protocol::S2C::Code, payload),
           "collection handshake");
    protocol::S2CCodeFields code;
    expect(protocol::Handshake::parse_code(payload.data(), payload.size(), code),
           "parse collection handshake");
    expect(session->send_all(protocol::Handshake::build_answer(code.protocol_test_value)) &&
               wait_frame(*session, rx, protocol::S2C::CreatePlayer, payload),
           "enter collection world");
    const auto read_stat = [&](server::ISession& peer,
                               std::vector<std::uint8_t>& buffer,
                               protocol::PropertyType property) {
        std::vector<std::uint8_t> data;
        for (int i = 0; i < 40; ++i) {
            if (!wait_frame(peer, buffer, protocol::S2C::UpdateProperty, data)) {
                return -1;
            }
            protocol::UpdatePropertyFields value;
            if (protocol::Property::parse_update_any(data.data(), data.size(), value) &&
                value.property == static_cast<std::uint8_t>(property)) {
                return property == protocol::PropertyType::Hp ? value.vital_max : value.word_value;
            }
        }
        return -1;
    };
    const auto read_speed = [&](server::ISession& peer, std::vector<std::uint8_t>& buffer) {
        std::vector<std::uint8_t> data;
        if (!wait_frame(peer, buffer, protocol::S2C::UpdateMProperty, data)) {
            return -1;
        }
        std::uint32_t entity = 0;
        std::uint8_t property = 0;
        std::int16_t value = 0;
        return protocol::Property::parse_update_mproperty(
                   data.data(), data.size(), entity, property, value) &&
                       property == protocol::kPropertyMSpeed
                   ? static_cast<int>(value)
                   : -1;
    };
    const int baseline_hp = read_stat(*session, rx, protocol::PropertyType::Hp);
    const int baseline_dodge = read_stat(*session, rx, protocol::PropertyType::Dodge);
    const int baseline_attack = read_stat(*session, rx, protocol::PropertyType::MinAttack);
    expect(baseline_hp > 0 && baseline_dodge >= 0 && baseline_attack > 0,
           "read baseline vitals and combat without collection equipment");
    const auto wear = [&](std::uint32_t iid, bool equip) {
        if (!session->send_all(equip ? protocol::ItemWear::frame_client_put_on(iid)
                                     : protocol::ItemWear::frame_client_put_off(iid)) ||
            !wait_frame(*session,
                        rx,
                        equip ? protocol::S2C::PutOnItem : protocol::S2C::PutOffItem,
                        payload)) {
            return false;
        }
        protocol::PacketReader r(payload.data(), payload.size());
        std::uint32_t player = 0, actual = 0;
        return r.read_u32(player) && r.read_u32(actual) && actual == iid;
    };
    const auto inspect = [&](std::uint32_t iid) -> std::optional<server::ItemRecord> {
        if (!store->open(config.save_dir)) {
            return {};
        }
        const auto items = store->load_items(config.character_id);
        store->close();
        for (const auto& item : items) {
            if (item.iid == static_cast<std::int32_t>(iid)) {
                return item;
            }
        }
        return {};
    };
    const auto barrier = [&] {
        // Saving acknowledges all preceding packets without an equipment cooldown or disconnect.
        return session->send_all(protocol::GameExit::frame_client_request()) &&
               wait_frame(*session, rx, protocol::S2C::AnsGameExit, payload);
    };
    expect(wear(100, true) && wear(101, true) && wear(102, true),
           "equip Hanin mirror, evasion and speed essences");
    expect(session->send_all(protocol::Chatting::frame_c2s("/getitemid 9000 77 4")) && barrier(),
           "fill the cells released by collection and separately stored ordinary equipment");
    expect(store->open(config.save_dir), "inspect collection capacity");
    const auto full_items = store->load_items(config.character_id);
    store->close();
    expect(full_items.size() == 76 && std::count_if(full_items.begin(),
                                                    full_items.end(),
                                                    [](const auto& item) {
                                                        return item.index == 9000 &&
                                                               item.prefix == 77;
                                                    }) == 4,
           "72 inventory items coexist with ordinary and Collection equipment");
    expect(session->send_all(protocol::ItemWear::frame_client_put_off(101)) && barrier(),
           "request collection removal with no inventory room");
    expect(inspect(101) && inspect(101)->slot != 0 && inspect(101)->random_stat_value == 12,
           "full inventory rejects removal without changing the collection instance");
    expect(session->send_all(protocol::TrashItem::frame_c2s(1000)) && barrier() && !inspect(1000),
           "free one normal inventory slot");
    expect(wear(101, false) && inspect(101) && inspect(101)->slot == 0 &&
               inspect(101)->random_stat_type == 106 && inspect(101)->random_stat_value == 12,
           "return the same essence and its option to the available inventory slot");
    std::this_thread::sleep_for(std::chrono::milliseconds(1050));
    expect(wear(101, true), "move the returned essence back into collection storage");
    expect(session->send_all(protocol::ItemWear::frame_client_put_on(103)) && barrier(),
           "reject duplicate speed before ordered acknowledgement");
    expect(inspect(103) && inspect(103)->slot == 0,
           "rejected duplicate stays unequipped in persistence");
    expect(session->send_all(protocol::ItemWear::frame_client_put_off(100)) && barrier(),
           "reject mirror removal before ordered acknowledgement");
    expect(inspect(100) && inspect(100)->slot != 0, "occupied mirror remains equipped");
    expect(session->send_all(protocol::EnchantItem::frame_c2s(202, 200)) && barrier(),
           "reject mismatched trigram material");
    expect(inspect(202) && inspect(200)->index == 24003, "rejected upgrade consumes nothing");
    expect(session->send_all(protocol::EnchantItem::frame_c2s(201, 200)) && barrier(),
           "upgrade raw trigram with configured guaranteed stage");
    expect(!inspect(201) && inspect(200) && inspect(200)->index == 24027 &&
               inspect(200)->upgrade_level == 1,
           "upgrade consumes exact source and persists target conversion");
    expect(session->send_all(protocol::EnchantItem::frame_c2s(211, 210)) && barrier(),
           "upgrade low essence");
    expect(!inspect(211) && inspect(210) && inspect(210)->index == 24058 &&
               inspect(210)->random_stat_type >= 100,
           "essence upgrade rerolls and persists its new tier");
    expect(session->send_all(protocol::EnchantItem::frame_c2s(221, 220)) && barrier(),
           "upgrade Yeoiju with matching spare and fortification");
    expect(!inspect(221) && !inspect(222) && inspect(220) && inspect(220)->upgrade_level == 1,
           "Yeoiju consumes both required materials");
    for (int part = 0; part < 8; ++part) {
        expect(wear(300 + part, true), "equip eight distinct trigrams");
    }
    expect(session->send_all(protocol::EnchantItem::frame_c2s(310, 310)) && barrier(),
           "absorb into explicitly addressed Taegeuk");
    const auto tae = inspect(310);
    expect(tae && tae->mix_type >= 1 && tae->mix_type <= 8 && inspect(300) && inspect(307),
           "absorption persists a row without consuming worn trigrams");
    expect(wear(311, true) && wear(310, true), "equip completed mirror and Taegeuk");
    expect(inspect(311) && inspect(311)->pet_expire_at > 0 && inspect(311)->account_bound,
           "mirror duration starts at equip and is persisted");
    expect(session->send_all(protocol::ItemWear::frame_client_put_off(300)) && barrier(),
           "reject removal from active trigram set");
    expect(inspect(300) && inspect(300)->slot != 0, "set dependency is enforced by server");
    expect(store->open(config.save_dir), "inspect inventory before reconnect");
    const auto before_login = store->load_items(config.character_id);
    store->close();
    const int worn_collection = static_cast<int>(
        std::count_if(before_login.begin(), before_login.end(), [](const auto& item) {
            return item.slot != 0 &&
                   server::collection_def(item.index).kind != server::CollectionKind::None;
        }));
    const int worn_ordinary = static_cast<int>(
        std::count_if(before_login.begin(), before_login.end(), [](const auto& item) {
            return item.slot != 0 &&
                   server::collection_def(item.index).kind == server::CollectionKind::None;
        }));
    const int normal_items =
        static_cast<int>(before_login.size()) - worn_collection - worn_ordinary;
    expect(worn_collection == 13 && normal_items < 72,
           "count separate collection and bag contents");
    expect(session->send_all(protocol::Chatting::frame_c2s("/getitemid 9000 78 " +
                                                           std::to_string(72 - normal_items))) &&
               barrier(),
           "fill all normal cells before restoring collection equipment");
    // The embedded host permits independent live sessions of the configured test character.
    auto opponent = server::connect_unix(config.unix_path);
    expect(opponent != nullptr, "connect second combat session");
    if (opponent) {
        std::vector<std::uint8_t> peer_rx;
        expect(opponent->send_all(protocol::Handshake::build_connect(0)) &&
                   wait_frame(*opponent, peer_rx, protocol::S2C::Code, payload),
               "handshake second combat session");
        protocol::S2CCodeFields peer_code;
        expect(protocol::Handshake::parse_code(payload.data(), payload.size(), peer_code),
               "read second session code");
        expect(
            opponent->send_all(protocol::Handshake::build_answer(peer_code.protocol_test_value)) &&
                wait_frame(*opponent, peer_rx, protocol::S2C::CreatePlayer, payload),
            "enter second combat session");
        protocol::CreatePlayerFields player;
        expect(protocol::CreatePlayer::parse(payload.data(), payload.size(), player),
               "read independent player entity id");
        const int pve_hp = read_stat(*opponent, peer_rx, protocol::PropertyType::Hp);
        const int pve_dodge = read_stat(*opponent, peer_rx, protocol::PropertyType::Dodge);
        const int pve_attack = read_stat(*opponent, peer_rx, protocol::PropertyType::MinAttack);
        expect(pve_hp > baseline_hp && pve_attack > baseline_attack,
               "restored trigrams increase PvE HP and attack");
        expect(pve_dodge > baseline_dodge, "restored collection increases PvE evasion");
        std::unordered_set<std::uint32_t> restored_iids;
        int collection_packets = 0;
        for (int i = 0; i < 72 + worn_collection + worn_ordinary; ++i) {
            if (!wait_frame(*opponent, peer_rx, protocol::S2C::InsertItem, payload)) {
                expect(false, "receive every inventory and collection instance on login");
                break;
            }
            protocol::PacketReader reader(payload.data(), payload.size());
            std::uint32_t iid = 0, info = 0, count = 0;
            std::uint16_t index = 0;
            std::uint8_t prefix = 0;
            expect(reader.read_u32(iid) && reader.read_u16(index) && reader.read_u8(prefix) &&
                       reader.read_u32(info) && reader.read_u32(count) && count == 1,
                   "decode restored item location");
            expect(restored_iids.insert(iid).second, "login sends each instance exactly once");
            if (server::collection_def(index).kind != server::CollectionKind::None &&
                protocol::insert_put_on(info)) {
                ++collection_packets;
            }
        }
        expect(restored_iids.size() ==
                       static_cast<std::size_t>(72 + worn_collection + worn_ordinary) &&
                   collection_packets == worn_collection && restored_iids.count(102) &&
                   restored_iids.count(310),
               "login restores full inventory plus all collection instances without truncation");
        expect(opponent->send_all(protocol::GameExit::frame_client_request()) &&
                   wait_frame(*opponent, peer_rx, protocol::S2C::AnsGameExit, payload) && barrier(),
               "drain initial combat snapshots");
        expect(session->send_all(
                   protocol::Skill::frame_client_target(3, protocol::kCkPlayer, player.id)),
               "cast offensive skill at other player");
        expect(read_speed(*session, rx) == 36, "attacker retains amplified movement speed in PvP");
        expect(read_stat(*session, rx, protocol::PropertyType::Hp) == baseline_hp,
               "attacker collection HP removed before PvP");
        expect(read_stat(*session, rx, protocol::PropertyType::Dodge) == baseline_dodge,
               "attacker collection evasion removed on PvP contact");
        expect(read_stat(*session, rx, protocol::PropertyType::MinAttack) == baseline_attack,
               "attacker collection damage removed before PvP");
        expect(read_speed(*opponent, peer_rx) == 36,
               "defender retains amplified movement speed in PvP");
        expect(read_stat(*opponent, peer_rx, protocol::PropertyType::Hp) == baseline_hp,
               "defender collection HP removed before PvP");
        expect(read_stat(*opponent, peer_rx, protocol::PropertyType::Dodge) == baseline_dodge,
               "defender collection evasion removed before PvP resolution");
        opponent->close();
    }
    session->close();
    host.stop();
    expect(store->open(config.save_dir), "reopen collection inventory after shutdown");
    const auto restored = store->load_items(config.character_id);
    store->close();
    bool absorption_restored = false, speed_restored = false;
    for (const auto& item : restored) {
        if (item.iid == 310) {
            absorption_restored = tae && item.mix_type == tae->mix_type && item.slot != 0;
        }
        if (item.iid == 102) {
            speed_restored =
                item.random_stat_type == 112 && item.random_stat_value == 30 && item.slot != 0;
        }
    }
    expect(absorption_restored && speed_restored,
           "absorption, essence values and equipped state survive restart");
    return test::g_failures ? 1 : 0;
}
