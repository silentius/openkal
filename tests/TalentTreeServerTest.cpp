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
#include "openkal/protocol/Chatting.h"
#include "openkal/protocol/DropItem.h"
#include "openkal/protocol/FrameParser.h"
#include "openkal/protocol/GameExit.h"
#include "openkal/protocol/Handshake.h"
#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/Property.h"
#include "openkal/protocol/Skill.h"
#include "openkal/protocol/TalentTree.h"
#include "openkal/server/ServerHost.h"
#include "openkal/server/net/Transport.h"
#include "openkal/server/store/DataStore.h"
#include "test_util.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sqlite3.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>

namespace {
    using namespace openkal;
    using test::expect;
    bool wait_frame(
        server::ISession& session,
        std::vector<std::uint8_t>& rx,
        protocol::S2C expected,
        std::vector<std::uint8_t>& payload,
        const std::function<void(std::uint8_t, const std::vector<std::uint8_t>&)>& observe = {}) {
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
                if (observe) {
                    observe(type, payload);
                }
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
    const auto root = test::temp_dir(("talent_server_" + std::to_string(getpid())).c_str());
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
    expect(test::write_open_area(config.maps_dir + "/n_000_000.akarea"), "write talent server map");
    {
        std::ofstream f(config.monsters_dir + "/1.json");
        f << R"({"index":1,"name":"Unused","stats":{"level":1,"hp":1},"spawns":[]})";
    }
    {
        std::ofstream f(config.items_dir + "/9000.json");
        f << R"({"index":9000,"name":"Owned test token","class":["general","etc"],"stackable":1})";
    }
    auto store = server::create_data_store(server::StoreBackend::Sqlite);
    expect(store->open(config.save_dir), "open talent server store");
    server::CharacterRecord hero;
    hero.name = "Talent Hero";
    hero.level = 110;
    hero.hp = 100;
    hero.mp = 1;
    hero.map_x = hero.map_y = config.spawn_x;
    expect(store->ensure_default_player("talents", "talents", hero),
           "create high-level talent tester");
    const auto account = store->find_account("talents");
    if (!account) {
        return 1;
    }
    const auto characters = store->list_characters(account->id);
    if (characters.empty()) {
        return 1;
    }
    config.character_id = characters.front().id;
    for (int i = 0; i < 8; ++i) {
        server::ItemRecord item;
        item.iid = 100 + i;
        item.character_id = config.character_id;
        item.index = 9000;
        item.count = 1;
        expect(store->insert_item(item), "seed owned auto-pick test item");
    }
    store->close();
    server::ServerHost host;
    expect(host.start(config), "start talent server");
    for (int i = 0; i < 200 && !std::filesystem::exists(config.unix_path); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    auto session = server::connect_unix(config.unix_path);
    if (!session) {
        host.stop();
        return 1;
    }
    std::vector<std::uint8_t> rx, payload;
    const auto login = [&] {
        expect(session->send_all(protocol::Handshake::build_connect(0)) &&
                   wait_frame(*session, rx, protocol::S2C::Code, payload),
               "talent handshake");
        protocol::S2CCodeFields code;
        expect(protocol::Handshake::parse_code(payload.data(), payload.size(), code),
               "decode talent challenge");
        expect(session->send_all(protocol::Handshake::build_answer(code.protocol_test_value)) &&
                   wait_frame(*session, rx, protocol::S2C::CreatePlayer, payload),
               "enter talent world");
    };
    protocol::TalentState state;
    std::unordered_map<int, int> properties;
    std::unordered_map<int, int> passive_ranks;
    const auto observe = [&](std::uint8_t type, const std::vector<std::uint8_t>& bytes) {
        if (type == static_cast<int>(protocol::S2C::UpdateProperty)) {
            protocol::UpdatePropertyFields fields;
            if (protocol::Property::parse_update_any(bytes.data(), bytes.size(), fields)) {
                properties[fields.property] = fields.kind == protocol::UpdatePropertyKind::Vital
                                                  ? fields.vital_max
                                                  : fields.word_value;
            }
        }
        if (type == static_cast<int>(protocol::S2C::SkillInfo)) {
            std::vector<protocol::SkillRankEntry> entries;
            if (protocol::Skill::parse_skill_info(bytes.data(), bytes.size(), entries)) {
                passive_ranks.clear();
                for (const auto& entry : entries) {
                    passive_ranks[entry.index] = entry.level;
                }
            }
        }
    };
    const auto receive = [&] {
        const bool ok = wait_frame(*session, rx, protocol::S2C::TalentTree, payload, observe) &&
                        protocol::parse_talent_state(payload.data(), payload.size(), state);
        expect(ok, "receive authoritative talent state");
        return ok;
    };
    const auto request =
        [&](protocol::TalentCommand cmd, std::uint64_t expected = 0, std::uint64_t desired = 0) {
            expect(session->send_all(protocol::frame_talent_request(cmd, expected, desired)),
                   "send talent request");
            return receive();
        };
    login();
    receive();
    expect(state.level == 110 && state.mask == 0,
           "existing level 110 receives all 110 talent points");
    request(protocol::TalentCommand::Apply, 0, protocol::talent_bit(1));
    expect(state.status == protocol::TalentStatus::Invalid && state.mask == 0,
           "server rejects skipped prerequisite");
    request(protocol::TalentCommand::Apply, 0, std::uint64_t{1} << 60);
    expect(state.status == protocol::TalentStatus::Invalid && state.mask == 0,
           "server rejects forged nodes");
    const auto base_attack = properties[static_cast<int>(protocol::PropertyType::MinAttack)];
    const auto base_defense = properties[static_cast<int>(protocol::PropertyType::Defense)];
    const auto base_mana = properties[static_cast<int>(protocol::PropertyType::Mp)];
    request(protocol::TalentCommand::Apply, 0, 3);
    expect(state.status == protocol::TalentStatus::Ok && state.mask == 3,
           "server commits a multi-node draft");
    expect(properties[static_cast<int>(protocol::PropertyType::MinAttack)] == base_attack + 15 &&
               properties[static_cast<int>(protocol::PropertyType::Defense)] ==
                   std::max(0, base_defense - 6),
           "committed physical bonuses and defense penalty reach combat properties");
    expect(passive_ranks[protocol::kTalentSkillFirst] == 2,
           "SkillInfo includes the committed passive branch rank");
    sqlite3* failure_db = nullptr;
    expect(sqlite3_open((config.save_dir + "/" + server::kSqliteFileName).c_str(), &failure_db) ==
               SQLITE_OK,
           "open failed-save fixture");
    expect(sqlite3_exec(
               failure_db,
               "CREATE TRIGGER reject_talent_apply BEFORE UPDATE OF talent_mask ON characters WHEN "
               "NEW.talent_mask=7 BEGIN SELECT RAISE(ABORT, 'owned failed-save fixture'); END",
               nullptr,
               nullptr,
               nullptr) == SQLITE_OK,
           "install isolated persistence failure");
    request(protocol::TalentCommand::Apply, 3, 7);
    expect(state.status == protocol::TalentStatus::SaveFailed && state.mask == 3,
           "failed durable save rolls back the entire draft");
    expect(
        sqlite3_exec(failure_db, "DROP TRIGGER reject_talent_apply", nullptr, nullptr, nullptr) ==
            SQLITE_OK,
        "remove isolated save failure");
    sqlite3_close(failure_db);
    request(protocol::TalentCommand::Apply, 0, 7);
    expect(state.status == protocol::TalentStatus::Stale && state.mask == 3,
           "outdated Apply cannot overwrite the build");
    request(protocol::TalentCommand::Apply, 3, 0);
    expect(state.status == protocol::TalentStatus::Invalid && state.mask == 3,
           "server rejects refunds");
    request(protocol::TalentCommand::Beacon);
    expect(state.status == protocol::TalentStatus::Unavailable, "unlearned Beacon cannot be cast");
    const auto oversized = (std::uint64_t{1} << 20) - 1;
    request(protocol::TalentCommand::Apply, 3, oversized);
    expect(state.status == protocol::TalentStatus::Invalid && state.mask == 3,
           "server enforces total budget");
    const auto with_magic = std::uint64_t{3} | protocol::talent_bit(10);
    request(protocol::TalentCommand::Apply, 3, with_magic);
    expect(state.mask == with_magic &&
               properties[static_cast<int>(protocol::PropertyType::Mp)] == base_mana * 98 / 100,
           "magical mana penalty reaches authoritative maximum MP");
    auto with_pick = with_magic;
    for (int i = 25; i < 31; ++i) {
        with_pick |= protocol::talent_bit(i);
    }
    request(protocol::TalentCommand::Apply, with_magic, with_pick);
    expect(state.mask == with_pick && passive_ranks[protocol::kTalentSkillFirst + 3] == 6,
           "auto-pick nodes are committed as passive skills");
    for (int i = 0; i < 8; ++i) {
        expect(session->send_all(protocol::DropItem::frame_c2s(100 + i, 1)),
               "drop one nearby test item");
    }
    expect(session->send_all(protocol::GameExit::frame_client_request()) &&
               wait_frame(*session, rx, protocol::S2C::AnsGameExit, payload),
           "finish drops before auto-pick pulse");
    std::this_thread::sleep_for(std::chrono::milliseconds(5400));
    expect(store->open(config.save_dir), "inspect automatic pickup");
    const auto picked = store->load_items(config.character_id);
    int owned = 0;
    for (const auto& item : picked) {
        if (item.index == 9000) {
            owned += item.count;
        }
    }
    expect(owned == 6, "Auto Pick collects six items in the first five-second pulse");
    store->close();
    // Respec the isolated fixture offline to exercise cooldown and the independent two-second
    // pulses.
    session.reset();
    host.stop();
    expect(store->open(config.save_dir), "inspect durable server selection");
    auto saved = store->load_character(config.character_id);
    expect(saved && saved->talent_mask == with_pick, "Apply persisted before the client response");
    if (!saved) {
        return 1;
    }
    std::uint64_t build = 0;
    for (int i = 20; i < 29; ++i) {
        build |= protocol::talent_bit(i);
    }
    for (int i = 31; i < 37; ++i) {
        build |= protocol::talent_bit(i);
    }
    expect(protocol::talent_cost(build) == 105, "regen and Beacon fixture costs 105 points");
    saved->talent_mask = build;
    saved->hp = 1;
    saved->mp = 1;
    expect(store->save_character(*saved), "seed owned regeneration fixture");
    store->close();
    expect(host.start(config), "restart talent server");
    for (int i = 0; i < 200 && !std::filesystem::exists(config.unix_path); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    session = server::connect_unix(config.unix_path);
    rx.clear();
    if (!session) {
        host.stop();
        return 1;
    }
    login();
    receive();
    expect(state.mask == build, "relogin restores all selected nodes");
    request(protocol::TalentCommand::Beacon);
    expect(state.status == protocol::TalentStatus::Ok && state.beacon_cooldown_ms > 119000 &&
               state.beacon_active_ms > 29000,
           "Beacon starts the 120-second cooldown and 30-second activity");
    expect(passive_ranks[protocol::kBeaconSkill] == 1 &&
               passive_ranks[protocol::kTalentSkillFirst + 4] == 5,
           "Beacon unlock is synchronized as an equippable rank-one self buff");
    request(protocol::TalentCommand::Beacon);
    expect(state.status == protocol::TalentStatus::Unavailable && state.beacon_cooldown_ms > 118000,
           "Beacon cannot bypass cooldown");
    // The base standing pulse is 6/12 seconds, so at 2.3 seconds HP and MP must include talents.
    std::this_thread::sleep_for(std::chrono::milliseconds(2300));
    expect(session->send_all(protocol::GameExit::frame_client_request()) &&
               wait_frame(*session, rx, protocol::S2C::AnsGameExit, payload),
           "save pulse outcome");
    expect(store->open(config.save_dir), "read talent vitals");
    saved = store->load_character(config.character_id);
    expect(saved && saved->hp == 1 + std::min(40, saved->max_hp / 100) + 20 &&
               saved->mp == 1 + std::min(20, saved->max_mp / 100) + 20,
           "Auto HP and MP apply independent two-second pulses");
    expect(saved && saved->beacon_ready_at > 0, "Beacon cooldown is persisted immediately");
    store->close();
    session.reset();
    host.stop();
    expect(host.start(config), "restart with active cooldown");
    for (int i = 0; i < 200 && !std::filesystem::exists(config.unix_path); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    session = server::connect_unix(config.unix_path);
    rx.clear();
    if (!session) {
        host.stop();
        return 1;
    }
    login();
    receive();
    expect(state.beacon_cooldown_ms > 110000 && state.beacon_active_ms == 0,
           "reconnect retains cooldown and cancels activity");
    request(protocol::TalentCommand::Beacon);
    expect(state.status == protocol::TalentStatus::Unavailable,
           "reconnect cannot reset Beacon cooldown");
    session.reset();
    host.stop();
    std::filesystem::remove_all(root);
    return test::g_failures == 0 ? 0 : 1;
}
