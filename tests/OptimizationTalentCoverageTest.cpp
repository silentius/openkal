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
#include "openkal/protocol/CreatePlayer.h"
#include "openkal/protocol/FrameParser.h"
#include "openkal/protocol/GameExit.h"
#include "openkal/protocol/Handshake.h"
#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketWriter.h"
#include "openkal/protocol/Skill.h"
#include "openkal/protocol/TalentTree.h"
#include "openkal/protocol/Teleport.h"
#include "openkal/server/ServerHost.h"
#include "openkal/server/TestControl.h"
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
    bool next_frame(server::ISession& session,
                    std::vector<std::uint8_t>& rx,
                    std::uint8_t& type,
                    std::vector<std::uint8_t>& payload) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        do {
            if (!session.recv_into(rx)) {
                return false;
            }
            std::size_t consumed = 0;
            const auto status =
                protocol::FrameParser::pop(rx.data(), rx.size(), consumed, type, payload);
            if (status == protocol::FrameParseStatus::Invalid) {
                return false;
            }
            if (status == protocol::FrameParseStatus::Complete) {
                rx.erase(rx.begin(), rx.begin() + static_cast<std::ptrdiff_t>(consumed));
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        } while (std::chrono::steady_clock::now() < deadline);
        return false;
    }
    bool wait_frame(server::ISession& session,
                    std::vector<std::uint8_t>& rx,
                    protocol::S2C expected,
                    std::vector<std::uint8_t>& payload) {
        std::uint8_t type = 0;
        for (int i = 0; i < 1000 && next_frame(session, rx, type, payload); ++i) {
            if (type == static_cast<std::uint8_t>(expected)) {
                return true;
            }
        }
        return false;
    }
    void test_control_guard(const std::string& root) {
        server::ServerConfig config;
        config.maps_dir = root + "/maps";
        config.monsters_dir = root + "/monsters";
        config.items_dir = root + "/items";
        config.skills_dir = root + "/skills";
        config.save_dir = root + "/save";
        config.unix_path = root + "/game.sock";
        config.store = server::StoreBackend::Sqlite;
        config.transport = server::TransportKind::Unix;
        config.spawn_x = config.spawn_y = 512;
        config.spawn_z = 0;
        config.max_monsters = 0;
        config.map_radius_cells = 0;
        for (const auto& dir :
             {config.maps_dir, config.monsters_dir, config.items_dir, config.skills_dir}) {
            test::mkdir_p(dir);
        }
        expect(test::write_open_area(config.maps_dir + "/n_000_000.akarea"),
               "write owned Earth Boom control area");
        std::ofstream(config.monsters_dir + "/1.json")
            << R"({"index":1,"name":"Owned control definition","stats":{"level":1,"hp":1},"spawns":[]})";
        std::ofstream(config.items_dir + "/2.json")
            << R"({"index":2,"name":"Owned one-hand control sword","class":["weapon","sword"],"wear":1})";
        std::ofstream(config.skills_dir + "/91.json")
            << R"({"index":91,"class":0,"maxLevel":1,"autoLearnLevels":[1],"type":["use","once"],"mp":1,"delay":[0, 1000]})";
        auto store = server::create_data_store(server::StoreBackend::Sqlite);
        const bool opened = store->open(config.save_dir);
        expect(opened, "open disposable control-guard character store");
        if (!opened) {
            return;
        }
        server::CharacterRecord hero;
        hero.name = "Owned Control Knight";
        hero.level = 110;
        hero.hp = hero.max_hp = 100;
        hero.mp = hero.max_mp = 100000;
        hero.map_x = hero.map_y = 512;
        hero.weapon_index = 2;
        for (int node = 20; node < 29; ++node) {
            hero.talent_mask |= protocol::talent_bit(node);
        }
        for (int node = 31; node < 37; ++node) {
            hero.talent_mask |= protocol::talent_bit(node);
        }
        const bool seeded = store->ensure_default_player("control_guard", "control_guard", hero);
        expect(seeded, "seed an owned knight with an unlocked ready Beacon");
        const auto account = store->find_account("control_guard");
        if (!seeded || !account) {
            return;
        }
        const auto records = store->list_characters(account->id);
        if (records.empty()) {
            expect(false, "find owned control-guard character");
            return;
        }
        config.character_id = records.front().id;
        server::ItemRecord sword;
        sword.iid = 1002;
        sword.character_id = config.character_id;
        sword.index = 2;
        sword.count = 1;
        sword.slot = 1;
        expect(store->insert_item(sword), "seed equipped one-hand weapon for Earth Boom");
        store->close();
        server::ServerHost host;
        expect(host.start(config), "start owned Earth Boom control server");
        for (int i = 0; i < 200 && !std::filesystem::exists(config.unix_path); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        auto victim = server::connect_unix(config.unix_path);
        auto caster = server::connect_unix(config.unix_path);
        expect(victim && caster, "connect two owned local runtime identities for control testing");
        if (!victim || !caster) {
            host.stop();
            return;
        }
        std::vector<std::uint8_t> victim_rx, caster_rx, payload;
        const auto enter = [&](server::ISession& peer,
                               std::vector<std::uint8_t>& rx,
                               protocol::CreatePlayerFields& self) {
            protocol::S2CCodeFields code;
            protocol::TalentState state;
            return peer.send_all(protocol::Handshake::build_connect(0)) &&
                   wait_frame(peer, rx, protocol::S2C::Code, payload) &&
                   protocol::Handshake::parse_code(payload.data(), payload.size(), code) &&
                   peer.send_all(protocol::Handshake::build_answer(code.protocol_test_value)) &&
                   wait_frame(peer, rx, protocol::S2C::CreatePlayer, payload) &&
                   protocol::CreatePlayer::parse(payload.data(), payload.size(), self) &&
                   wait_frame(peer, rx, protocol::S2C::TalentTree, payload) &&
                   protocol::parse_talent_state(payload.data(), payload.size(), state) &&
                   state.mask == hero.talent_mask && state.beacon_cooldown_ms == 0 &&
                   state.beacon_active_ms == 0;
        };
        protocol::CreatePlayerFields victim_self, caster_self;
        const bool entered =
            enter(*victim, victim_rx, victim_self) && enter(*caster, caster_rx, caster_self);
        expect(entered && victim_self.id != caster_self.id,
               "distinct runtime identities start with ready Beacon and known owned skills");
        if (!entered) {
            host.stop();
            return;
        }
        // Local mode uses the configured disposable record for both connections.
        // This fixture asserts runtime control; persistence has a separate fixture.
        expect(caster->send_all(protocol::Skill::frame_client_self(91)) &&
                   caster->send_all(protocol::GameExit::frame_client_request()) &&
                   wait_frame(*caster, caster_rx, protocol::S2C::AnsGameExit, payload),
               "execute one real Earth Boom before a same-connection save barrier");
        bool observed_control = false;
        const auto observation_deadline =
            std::chrono::steady_clock::now() + std::chrono::seconds(3);
        std::uint8_t type = 0;
        while (std::chrono::steady_clock::now() < observation_deadline &&
               next_frame(*victim, victim_rx, type, payload)) {
            if (type != static_cast<std::uint8_t>(protocol::S2C::Skill)) {
                continue;
            }
            protocol::S2CSkillFields skill;
            if (protocol::Skill::parse_server(payload.data(), payload.size(), skill) &&
                skill.skill == 91 && skill.caster == caster_self.id &&
                skill.target == victim_self.id && skill.has_damage && skill.damage == 0) {
                observed_control = true;
                break;
            }
        }
        expect(observed_control,
               "real Earth Boom reports its zero-damage control hit on the victim");
        const auto request_beacon = [&](protocol::TalentState& state) {
            return victim->send_all(
                       protocol::frame_talent_request(protocol::TalentCommand::Beacon)) &&
                   wait_frame(*victim, victim_rx, protocol::S2C::TalentTree, payload) &&
                   protocol::parse_talent_state(payload.data(), payload.size(), state);
        };
        protocol::TalentState blocked;
        expect(observed_control && request_beacon(blocked) &&
                   blocked.status == protocol::TalentStatus::Unavailable &&
                   blocked.mask == hero.talent_mask && blocked.beacon_cooldown_ms == 0 &&
                   blocked.beacon_active_ms == 0,
               "controlled alive player cannot activate Beacon or consume its ready cooldown");
        // Rank-one Earth Boom controls players for two seconds. Passing that
        // deadline also verifies that the rejection is temporary.
        std::this_thread::sleep_for(std::chrono::milliseconds(2100));
        protocol::TalentState recovered;
        expect(request_beacon(recovered) && recovered.status == protocol::TalentStatus::Ok &&
                   recovered.mask == hero.talent_mask && recovered.beacon_cooldown_ms > 119000 &&
                   recovered.beacon_active_ms > 29000,
               "Beacon succeeds after control expires without cooldown from the rejected attempt");
        caster->close();
        victim->close();
        host.stop();
    }
}  // namespace

int main() {
    const auto root = test::temp_dir(("optimization_talent_" + std::to_string(getpid())).c_str());
    server::ServerConfig config;
    config.maps_dir = root + "/maps";
    config.monsters_dir = root + "/monsters";
    config.items_dir = root + "/items";
    config.save_dir = root + "/save";
    config.unix_path = root + "/game.sock";
    config.store = server::StoreBackend::Sqlite;
    config.transport = server::TransportKind::Unix;
    config.spawn_x = config.spawn_y = 512;
    config.spawn_z = 0;
    config.max_monsters = 0;
    config.map_radius_cells = 0;
    config.test_control_token = "owned-talent-control-token";
    for (const auto& dir : {config.maps_dir, config.monsters_dir, config.items_dir}) {
        test::mkdir_p(dir);
    }
    expect(test::write_open_area(config.maps_dir + "/n_000_000.akarea"),
           "write owned talent-guard area");
    std::ofstream(config.monsters_dir + "/1.json")
        << R"({"index":1,"name":"Owned definition","stats":{"level":1,"hp":1},"spawns":[]})";
    std::ofstream(config.items_dir + "/1.json")
        << R"({"index":1,"name":"Owned item","class":["general","etc"],"stackable":1})";
    auto store = server::create_data_store(server::StoreBackend::Sqlite);
    expect(store->open(config.save_dir), "open owned talent guard store");
    server::CharacterRecord hero;
    hero.name = server::test_identity("TestPlayer", 1);
    hero.level = 110;
    hero.hp = hero.mp = 100;
    hero.map_x = hero.map_y = 512;
    for (int node = 20; node < 29; ++node) {
        hero.talent_mask |= protocol::talent_bit(node);
    }
    for (int node = 31; node < 37; ++node) {
        hero.talent_mask |= protocol::talent_bit(node);
    }
    const auto username = server::test_identity("test", 1);
    expect(store->ensure_default_player(username, "guard", hero),
           "seed owned unlocked Beacon character");
    const auto account = store->find_account(username);
    if (!account) {
        return 1;
    }
    const auto records = store->list_characters(account->id);
    if (records.empty()) {
        return 1;
    }
    config.character_id = records.front().id;
    store->close();
    server::ServerHost host;
    expect(host.start(config), "start talent guard server");
    for (int i = 0; i < 200 && !std::filesystem::exists(config.unix_path); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    auto session = server::connect_unix(config.unix_path);
    if (!session) {
        host.stop();
        return 1;
    }
    std::vector<std::uint8_t> rx, payload;
    auto premature = protocol::frame_talent_request(protocol::TalentCommand::Beacon);
    const auto premature_apply =
        protocol::frame_talent_request(protocol::TalentCommand::Apply, 0, 1);
    premature.insert(premature.end(), premature_apply.begin(), premature_apply.end());
    const auto connect = protocol::Handshake::build_connect(0);
    premature.insert(premature.end(), connect.begin(), connect.end());
    std::uint8_t first_type = 0;
    expect(session->send_all(premature) && next_frame(*session, rx, first_type, payload) &&
               first_type == static_cast<std::uint8_t>(protocol::S2C::Code),
           "talent commands before Connect emit no reply before the handshake challenge");
    protocol::S2CCodeFields challenge;
    expect(protocol::Handshake::parse_code(payload.data(), payload.size(), challenge),
           "decode talent guard challenge");
    premature = protocol::frame_talent_request(protocol::TalentCommand::Snapshot);
    const auto answer = protocol::Handshake::build_answer(challenge.protocol_test_value);
    const auto entry_barrier = protocol::GameExit::frame_client_request();
    premature.insert(premature.end(), premature_apply.begin(), premature_apply.end());
    premature.insert(premature.end(), answer.begin(), answer.end());
    premature.insert(premature.end(), entry_barrier.begin(), entry_barrier.end());
    expect(session->send_all(premature), "send pre-answer talents and owned entry barrier");
    int entry_states = 0;
    bool entry_player = false, entry_saved = false;
    while (next_frame(*session, rx, first_type, payload)) {
        if (first_type == static_cast<std::uint8_t>(protocol::S2C::CreatePlayer)) {
            entry_player = true;
        }
        if (first_type == static_cast<std::uint8_t>(protocol::S2C::TalentTree)) {
            ++entry_states;
        }
        if (first_type == static_cast<std::uint8_t>(protocol::S2C::AnsGameExit)) {
            expect(protocol::GameExit::parse_server_answer(
                       payload.data(), payload.size(), entry_saved),
                   "decode owned entry persistence barrier");
            break;
        }
    }
    expect(
        entry_player && entry_saved && entry_states == 1,
        "talent requests before Answer emit no state or mutation before the single login snapshot");
    protocol::TalentState state;
    const auto request = [&](protocol::TalentCommand command) {
        return session->send_all(protocol::frame_talent_request(command)) &&
               wait_frame(*session, rx, protocol::S2C::TalentTree, payload) &&
               protocol::parse_talent_state(payload.data(), payload.size(), state);
    };
    protocol::PacketWriter malformed;
    malformed.write_u8(255);
    auto batch = malformed.frame(static_cast<std::uint8_t>(protocol::C2S::TalentTree));
    const auto snapshot = protocol::frame_talent_request(protocol::TalentCommand::Snapshot);
    const auto barrier = protocol::GameExit::frame_client_request();
    batch.insert(batch.end(), snapshot.begin(), snapshot.end());
    batch.insert(batch.end(), barrier.begin(), barrier.end());
    expect(session->send_all(batch),
           "send malformed talent request followed by valid snapshot and barrier");
    int states = 0;
    bool reached_barrier = false;
    std::uint8_t type = 0;
    while (next_frame(*session, rx, type, payload)) {
        if (type == static_cast<std::uint8_t>(protocol::S2C::TalentTree)) {
            ++states;
            expect(protocol::parse_talent_state(payload.data(), payload.size(), state) &&
                       state.mask == hero.talent_mask,
                   "valid snapshot after malformed talent request preserves committed mask");
        }
        if (type == static_cast<std::uint8_t>(protocol::S2C::AnsGameExit)) {
            reached_barrier = true;
            break;
        }
    }
    expect(reached_barrier && states == 1,
           "malformed talent request emits no state or extra response");
    sqlite3* failure_db = nullptr;
    expect(sqlite3_open((config.save_dir + "/" + server::kSqliteFileName).c_str(), &failure_db) ==
               SQLITE_OK,
           "open owned Beacon persistence fault fixture");
    expect(sqlite3_exec(failure_db,
                        "CREATE TRIGGER reject_beacon BEFORE UPDATE OF beacon_ready_at ON "
                        "characters WHEN NEW.beacon_ready_at>OLD.beacon_ready_at BEGIN SELECT "
                        "RAISE(ABORT, 'owned Beacon fault'); END",
                        nullptr,
                        nullptr,
                        nullptr) == SQLITE_OK,
           "install narrowly scoped Beacon save failure");
    expect(request(protocol::TalentCommand::Beacon) &&
               state.status == protocol::TalentStatus::SaveFailed &&
               state.beacon_cooldown_ms == 0 && state.beacon_active_ms == 0 &&
               state.mask == hero.talent_mask,
           "failed Beacon save restores cooldown and never activates its effect");
    expect(sqlite3_exec(failure_db, "DROP TRIGGER reject_beacon", nullptr, nullptr, nullptr) ==
               SQLITE_OK,
           "remove owned Beacon save failure");
    sqlite3_close(failure_db);
    expect(session->send_all(protocol::Skill::frame_rest(1)) &&
               request(protocol::TalentCommand::Beacon) &&
               state.status == protocol::TalentStatus::Unavailable && state.beacon_cooldown_ms == 0,
           "resting player cannot activate Beacon or consume its cooldown");
    expect(session->send_all(protocol::Skill::frame_rest(0)), "stand after Beacon rest guard");
    const auto teleport_command =
        platform::Json::parse("{\"op\":\"teleport\",\"id\":" + std::to_string(config.character_id) +
                              ",\"map\":0,\"x\":544,\"y\":512,\"z\":0}");
    expect(session->send_all(
               server::frame_test_request(config.test_control_token, teleport_command)) &&
               wait_frame(*session, rx, protocol::S2C::Teleport, payload),
           "start owned pending teleport");
    expect(request(protocol::TalentCommand::Beacon) &&
               state.status == protocol::TalentStatus::Unavailable && state.beacon_cooldown_ms == 0,
           "pending teleport cannot activate Beacon or consume cooldown");
    expect(session->send_all(protocol::frame_talent_request(
               protocol::TalentCommand::Apply, hero.talent_mask, hero.talent_mask)) &&
               wait_frame(*session, rx, protocol::S2C::TalentTree, payload) &&
               protocol::parse_talent_state(payload.data(), payload.size(), state) &&
               state.status == protocol::TalentStatus::Invalid && state.mask == hero.talent_mask,
           "pending teleport cannot commit even an otherwise valid talent draft");
    expect(session->send_all(protocol::Teleport::frame_client_ack(1, 0)) &&
               request(protocol::TalentCommand::Beacon) &&
               state.status == protocol::TalentStatus::Ok && state.beacon_cooldown_ms > 119000 &&
               state.beacon_active_ms > 29000,
           "clearing teleport guard allows Beacon after earlier rollback and rejected attempts");
    session->close();
    host.stop();
    expect(store->open(config.save_dir), "inspect durable Beacon after rollback and later success");
    auto saved = store->load_character(config.character_id);
    expect(saved && saved->beacon_ready_at > 0 && saved->talent_mask == hero.talent_mask,
           "successful retry persists only the final Beacon cooldown and unchanged talents");
    if (saved) {
        saved->hp = 0;
        saved->beacon_ready_at = 0;
        expect(store->save_character(*saved), "seed owned dead-character guard fixture offline");
    }
    store->close();
    session.reset();
    expect(host.start(config), "restart talent guard server with a dead character");
    for (int i = 0; i < 200 && !std::filesystem::exists(config.unix_path); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    session = server::connect_unix(config.unix_path);
    rx.clear();
    if (!session) {
        host.stop();
        return 1;
    }
    expect(
        session->send_all(protocol::Handshake::build_connect(0)) &&
            wait_frame(*session, rx, protocol::S2C::Code, payload) &&
            protocol::Handshake::parse_code(payload.data(), payload.size(), challenge) &&
            session->send_all(protocol::Handshake::build_answer(challenge.protocol_test_value)) &&
            wait_frame(*session, rx, protocol::S2C::CreatePlayer, payload) &&
            wait_frame(*session, rx, protocol::S2C::TalentTree, payload),
        "dead character enters world without implicit revival");
    expect(request(protocol::TalentCommand::Beacon) &&
               state.status == protocol::TalentStatus::Unavailable && state.beacon_cooldown_ms == 0,
           "dead player cannot activate Beacon or consume cooldown");
    expect(session->send_all(protocol::frame_talent_request(
               protocol::TalentCommand::Apply, hero.talent_mask, hero.talent_mask)) &&
               wait_frame(*session, rx, protocol::S2C::TalentTree, payload) &&
               protocol::parse_talent_state(payload.data(), payload.size(), state) &&
               state.status == protocol::TalentStatus::Invalid && state.mask == hero.talent_mask,
           "dead player cannot apply an otherwise valid talent draft");
    session->close();
    host.stop();
    test_control_guard(root + "/control");
    std::filesystem::remove_all(root);
    return test::g_failures ? 1 : 0;
}
