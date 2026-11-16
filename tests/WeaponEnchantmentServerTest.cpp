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
#include "openkal/server/ServerHost.h"
#include "openkal/server/WeaponEnchantment.h"
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
    const auto root = test::temp_dir(("weapon_enchantment_" + std::to_string(getpid())).c_str());
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
    expect(test::write_open_area(config.maps_dir + "/n_000_000.akarea"), "write enchantment map");
    const auto fixture = [](const std::string& path, const std::string& json) {
        std::ofstream out(path);
        out << json;
        expect(out.good(), "write project-owned enchantment fixture");
    };
    fixture(config.monsters_dir + "/1.json",
            R"({"index":1,"name":"Unused","stats":{"level":1,"hp":1},"spawns":[]})");
    fixture(
        config.items_dir + "/23022.json",
        R"({"index":23022,"name":"Talisman of Enchantment","class":["general","etc"],"stackable":1})");
    for (int tier = 2; tier <= 5; ++tier) {
        const int index = server::kEnchantmentTalismanIndex + tier - 1;
        fixture(config.items_dir + "/" + std::to_string(index) + ".json",
                "{\"index\":" + std::to_string(index) +
                    ",\"name\":\"Enchantment "
                    "Talisman\",\"class\":[\"general\",\"etc\"],\"stackable\":1}");
    }
    const auto weapon = [&](int index, int grade, const char* kind, bool imperial) {
        fixture(config.items_dir + "/" + std::to_string(index) + ".json",
                "{\"index\":" + std::to_string(index) + ",\"name\":\"" +
                    (imperial ? "Imperial Training Weapon" : "Training Weapon") +
                    "\",\"class\":[\"weapon\",\"" + kind +
                    "\"],\"wear\":1,\"level\":" + std::to_string(grade) + ",\"endurance\":30}");
    };
    weapon(9000, 37, "sword", false);
    weapon(9001, 40, "sword", false);
    weapon(9002, 50, "sword", false);
    weapon(9003, 53, "sword", false);
    weapon(9010, 40, "sword", true);
    weapon(9011, 50, "sword", true);
    weapon(9012, 53, "sword", true);
    for (const auto& pair : std::vector<std::pair<int, const char*>>{
             {9020, "wand"}, {9030, "bow"}, {9040, "dagger"}, {9050, "sword2h"}}) {
        weapon(pair.first, 40, pair.second, false);
        weapon(pair.first + 1, pair.first == 9020 ? 43 : 53, pair.second, false);
    }
    const int high_grades[] = {59, 60, 69, 70, 79, 80, 95};
    for (int i = 0; i < 7; ++i) {
        weapon(9060 + i, high_grades[i], "sword", false);
    }
    weapon(9091, 0, "sword", false);
    weapon(9094, 39, "sword", false);
    fixture(config.items_dir + "/9090.json",
            R"({"index":9090,"class":["defense","upperarmor"],"wear":1,"level":40})");
    fixture(config.items_dir + "/9092.json", R"({"index":9092,"class":["general","etc"]})");
    fixture(config.items_dir + "/9093.json",
            R"({"index":9093,"class":["weapon","sword"],"stackable":1,"level":40})");
    auto store = server::create_data_store(server::StoreBackend::Sqlite);
    expect(store && store->open(config.save_dir), "open enchantment persistence");
    server::CharacterRecord hero;
    hero.name = "Enchanter";
    hero.level = 60;
    hero.hp = hero.mp = 100;
    hero.map_x = hero.map_y = config.spawn_x;
    expect(store->ensure_default_player("enchanter", "enchanter", hero),
           "create enchantment player");
    const auto account = store->find_account("enchanter");
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
    const int indices[] = {
        23022, 9001, 9020, 9011, 9030, 9040, 9050, 9090, 9091, 9093, 9001, 9092, 9000, 9094};
    for (int i = 0; i < 14; ++i) {
        server::ItemRecord item;
        item.character_id = config.character_id;
        item.iid = 100 + i;
        item.index = indices[i];
        item.count = i == 0 ? 6 : (i == 9 ? 2 : 1);
        if ((i > 0 && i <= 6) || i >= 12) {
            item.x_attack = item.x_magic = item.x_hit = item.upgrade_level = 20;
            item.upgrade_rate = 9;
            item.cur_end = 13;
            item.max_end = 30;
            item.prefix = 7;
            item.mix_type = 17;
            item.qigong_perforated = 1;
            item.qigong_effect = 2;
            item.dragon_effect = 3;
            item.account_bound = true;
            item.enchant_protected = true;
        }
        if (i == 10) {
            item.slot = server::persist_slot_of(server::WearKind::Weapon);
        }
        expect(store->insert_item(item), "seed talisman and target weapon instances");
    }
    for (int i = 0; i < 11; ++i) {
        server::ItemRecord item;
        item.character_id = config.character_id;
        item.iid = 114 + i;
        item.index = i < 4 ? server::kEnchantmentTalismanIndex + i + 1 : 9060 + i - 4;
        item.count = i == 0 ? 2 : 1;
        if (i >= 4) {
            item.x_attack = item.x_magic = item.x_hit = item.upgrade_level = 20;
            item.upgrade_rate = 9;
            item.enchant_protected = true;
        }
        expect(store->insert_item(item), "seed higher talisman grades and boundary weapon targets");
    }
    store->close();
    server::ServerHost host;
    expect(host.start(config), "start enchantment server");
    for (int attempt = 0; attempt < 200 && !std::filesystem::exists(config.unix_path); ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    auto session = server::connect_unix(config.unix_path);
    if (!session) {
        expect(false, "connect enchantment client");
        return 1;
    }
    std::vector<std::uint8_t> rx, payload;
    const auto login = [&] {
        expect(session->send_all(protocol::Handshake::build_connect(0)) &&
                   wait_frame(*session, rx, protocol::S2C::Code, payload),
               "enchantment handshake");
        protocol::S2CCodeFields code;
        expect(protocol::Handshake::parse_code(payload.data(), payload.size(), code),
               "decode handshake");
        expect(session->send_all(protocol::Handshake::build_answer(code.protocol_test_value)) &&
                   wait_frame(*session, rx, protocol::S2C::CreatePlayer, payload),
               "enter enchantment world");
    };
    const auto barrier = [&] {
        return session->send_all(protocol::GameExit::frame_client_request()) &&
               wait_frame(*session, rx, protocol::S2C::AnsGameExit, payload);
    };
    const auto inspect = [&](int iid) -> std::optional<server::ItemRecord> {
        expect(store->open(config.save_dir), "inspect enchantment persistence");
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
    login();
    expect(barrier(), "initial enchantment barrier");
    for (int target : {100, 107, 108, 109, 110, 111, 112, 113, 999}) {
        expect(apply(100, target), "send invalid enchantment request");
        expect(inspect(100) && inspect(100)->count == 6,
               "invalid targets including G37 and G39 consume no talisman");
    }
    for (int target : {112, 113}) {
        const auto item = inspect(target);
        expect(item && item->x_attack == 20 && item->x_magic == 20 && item->x_hit == 20 &&
                   item->upgrade_level == 20 && item->upgrade_rate == 9 && item->enchant_protected,
               "rejected weapons below G40 retain all enchantments and protection");
    }
    const std::vector<std::pair<int, int>> band_targets{
        {101, 1}, {103, 2}, {118, 2}, {119, 3}, {120, 3}, {121, 4}, {122, 4}, {123, 5}, {124, 5}};
    for (int tier = 1; tier <= 5; ++tier) {
        const int source = tier == 1 ? 100 : 112 + tier;
        const int count = tier == 1 ? 6 : (tier == 2 ? 2 : 1);
        for (const auto& target : band_targets) {
            if (tier == target.second) {
                continue;
            }
            const auto before = inspect(target.first);
            expect(apply(source, target.first), "send mismatched talisman grade");
            const auto after = inspect(target.first);
            expect(inspect(source) && inspect(source)->count == count && before && after &&
                       after->index == before->index && after->x_attack == before->x_attack &&
                       after->x_magic == before->x_magic && after->x_hit == before->x_hit &&
                       after->upgrade_level == before->upgrade_level &&
                       after->enchant_protected == before->enchant_protected,
                   "server rejects every mismatched talisman band without consumption or reset");
        }
    }
    for (int target = 101; target <= 106; ++target) {
        const int source = target == 103 ? 114 : 100;
        expect(session->send_all(protocol::EnchantItem::frame_c2s(source, target)) &&
                   wait_frame(*session, rx, protocol::S2C::InsertItem, payload) &&
                   wait_frame(*session, rx, protocol::S2C::InsertItem, payload),
               "accepted reroll sends source consumption followed by an absolute target update");
        protocol::PacketReader reader(payload.data(), payload.size());
        std::uint32_t packet_iid = 0, packet_info = 0, packet_count = 0;
        std::uint16_t packet_index = 0;
        std::uint8_t packet_prefix = 0;
        expect(reader.read_u32(packet_iid) && reader.read_u16(packet_index) &&
                   reader.read_u8(packet_prefix) && reader.read_u32(packet_info) &&
                   reader.read_u32(packet_count),
               "decode rerolled item identity and flags");
        const auto packet_charm =
            protocol::unpack_insert_charm(payload.size() >= 15 ? payload.data() + 15 : nullptr,
                                          payload.size() >= 15 ? payload.size() - 15 : 0);
        expect(barrier(), "enchant each supported weapon family");
        const auto item = inspect(target);
        expect(item.has_value(), "reroll keeps the target instance");
        if (!item) {
            continue;
        }
        expect(packet_iid == static_cast<std::uint32_t>(target) && packet_index == item->index &&
                   packet_prefix == item->prefix && packet_count == 1 &&
                   (packet_info & protocol::kInsertReplace) != 0 &&
                   (packet_info & protocol::kInsertEnchantProtected) == 0 &&
                   packet_charm.x_attack == item->x_attack &&
                   packet_charm.x_magic == item->x_magic && packet_charm.x_hit == item->x_hit &&
                   packet_charm.upgrade_level == item->upgrade_level &&
                   packet_charm.upgrade_rate == 0,
               "absolute packet carries authoritative grade, reset stats and consumed protection");
        const bool wand = target == 102;
        const int plus = wand ? item->x_magic : item->x_attack;
        expect(
            plus >= 1 && plus <= 16 && (wand ? item->x_attack : item->x_magic) == 0 &&
                item->x_hit >= 0 && item->x_hit <= 5 && item->upgrade_rate == 0 &&
                (item->upgrade_level == std::min(10, plus) ||
                 (plus == 16 && item->upgrade_level == 16)),
            "persisted OTP, correct charm branch and EB replace all previous enchantment values");
        expect(item->cur_end == 13 && item->max_end == 30 && item->prefix == 7 &&
                   item->mix_type == 17 && item->qigong_perforated == 1 &&
                   item->qigong_effect == 2 && item->dragon_effect == 3 && item->account_bound &&
                   !item->enchant_protected,
               "unrelated instance fields survive and the accepted attempt consumes protection");
        if (target == 101) {
            expect(item->index == 9000 || item->index == 9001 || item->index == 9002,
                   "normal sword keeps its family and moves by at most one catalog grade");
        } else if (target == 103) {
            expect(item->index >= 9010 && item->index <= 9012,
                   "Imperial sword cannot become a normal sword");
        } else {
            expect(item->index == indices[target - 100] || item->index == indices[target - 100] + 1,
                   "wand, bow, dagger and two-handed sword retain their subtype");
        }
        expect(inspect(100) && inspect(100)->count == 6 - (target - 100) + (target >= 103 ? 1 : 0),
               "each accepted application consumes exactly one talisman");
    }
    for (const auto& pair :
         std::vector<std::pair<int, int>>{{114, 118}, {115, 120}, {116, 122}, {117, 124}}) {
        expect(apply(pair.first, pair.second), "apply matching higher-grade talisman");
        const auto item = inspect(pair.second);
        expect(!inspect(pair.first) && item && item->x_attack >= 1 && item->x_attack <= 16 &&
                   item->x_magic == 0 && item->x_hit <= 5 && item->upgrade_rate == 0 &&
                   !item->enchant_protected,
               "matching grades II, III, IIII and V consume one talisman and reroll the weapon");
    }
    expect(apply(100, 102) && !inspect(100), "last talisman is deleted after a repeated reroll");
    const auto saved = inspect(102);
    expect(apply(100, 102) && inspect(102) && saved && inspect(102)->x_attack == saved->x_attack &&
               inspect(102)->x_magic == saved->x_magic && inspect(102)->x_hit == saved->x_hit &&
               inspect(102)->upgrade_level == saved->upgrade_level &&
               inspect(102)->index == saved->index,
           "replayed request cannot reroll without another talisman");
    session.reset();
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    session = server::connect_unix(config.unix_path);
    rx.clear();
    if (session) {
        login();
        expect(barrier(), "relogin enchantment barrier");
        const auto restored = inspect(102);
        expect(saved && restored && !inspect(100) && restored->index == saved->index &&
                   restored->x_attack == saved->x_attack && restored->x_magic == saved->x_magic &&
                   restored->x_hit == saved->x_hit &&
                   restored->upgrade_level == saved->upgrade_level,
               "relogin retains replacement definition and all rerolled values");
    } else {
        expect(false, "reconnect enchantment client");
    }
    session.reset();
    host.stop();
    std::filesystem::remove_all(root);
    return test::g_failures == 0 ? 0 : 1;
}
