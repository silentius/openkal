/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "TradingPostContract.h"
#include "fixture_util.h"
#include "openkal/protocol/Chatting.h"
#include "openkal/protocol/CreatePlayer.h"
#include "openkal/protocol/DropItem.h"
#include "openkal/protocol/FrameParser.h"
#include "openkal/protocol/GameExit.h"
#include "openkal/protocol/Guild.h"
#include "openkal/protocol/Handshake.h"
#include "openkal/protocol/Mail.h"
#include "openkal/protocol/Move.h"
#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/Party.h"
#include "openkal/protocol/PlayerOptions.h"
#include "openkal/protocol/Trade.h"
#include "openkal/protocol/Teleport.h"
#include "openkal/server/AccountPassword.h"
#include "openkal/server/LobbyClient.h"
#include "openkal/server/LobbyService.h"
#include "openkal/server/ManagementApi.h"
#include "openkal/server/ServerHost.h"
#include "openkal/server/store/PostgresStore.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <libpq-fe.h>
#include <stdexcept>
#include <thread>
#include <unistd.h>

namespace {
    void require(bool condition, const char* message) {
        if (!condition) {
            throw std::runtime_error(message);
        }
        std::cout << "PASS " << message << std::endl;
    }
    void contract(openkal::server::IDataStore& store) {
        using namespace openkal::server;
        AccountRecord account;
        account.name = "contract";
        account.password = "local contract password";
        require(store.upsert_account(account), "insert account");
        const auto a = store.find_account(account.name);
        require(a.has_value(), "find account");
        const auto clean = store.load_account_discipline(a->id);
        require(clean && clean->points == 0 && clean->kicks == 0 && !clean->banned,
                "new account starts without protocol sanctions");
        const auto decayed = store.update_account_discipline(a->id, AccountDisciplineChange::Decay);
        require(decayed && decayed->points == 0, "account score stays at zero after decay");
        CharacterRecord c;
        c.account_id = a->id;
        c.name = "ContractHero";
        c.money = 5000000000LL;
        c.exp = 6000000000LL;
        c.map_x = 1024;
        c.map_y = 1024;
        c.talent_mask = 3;
        c.beacon_ready_at = 1800000000123LL;
        require(store.save_character(c), "insert character with 64-bit state");
        auto rows = store.list_characters(a->id);
        require(rows.size() == 1, "list owned character");
        c = rows[0];
        require(c.money == 5000000000LL && c.exp == 6000000000LL &&
                    c.beacon_ready_at == 1800000000123LL,
                "preserve wide integers");
        ItemRecord starter;
        starter.iid = 99;
        starter.character_id = c.id;
        starter.index = 1;
        starter.count = 1;
        starter.character_bound = true;
        require(store.grant_starter_items(c, {starter}), "atomically grant a starter kit");
        require(store.delete_item(99) && !store.grant_starter_items(c, {starter}) &&
                    store.load_items(c.id).empty(),
                "starter grant remains once-only after its final item is consumed");
        ItemRecord item;
        item.iid = 100;
        item.character_id = c.id;
        item.index = 1;
        item.count = 2;
        item.enchant_protected = true;
        item.account_bound = true;
        item.character_bound = true;
        item.ride_expire_at = 1800000000;
        require(store.insert_item(item), "insert inventory");
        auto items = store.load_items(c.id);
        require(items.size() == 1 && items[0].enchant_protected && items[0].account_bound &&
                    items[0].character_bound,
                "reload item flags");
        item.count = 3;
        require(store.update_item(item), "update item");
        item.count = 3;
        ItemRecord reward = item;
        reward.iid = 101;
        reward.count = 1;
        require(store.consume_item_with_rewards(item, {reward}), "consume and reward transaction");
        require(!store.consume_item_with_rewards(item, {reward}),
                "reject stale consumption and duplicate reward");
        auto current_box = item;
        current_box.count = 2;
        require(!store.consume_item_with_rewards(current_box, {reward}),
                "rollback consumption when a later reward already exists");
        const auto unchanged = store.load_items(c.id);
        const auto box_after = std::find_if(
            unchanged.begin(), unchanged.end(), [](const auto& row) { return row.iid == 100; });
        require(box_after != unchanged.end() && box_after->count == 2,
                "failed reward transaction leaves the box count unchanged");
        ItemRecord stored = item;
        stored.iid = 102;
        stored.character_id = 0;
        stored.account_id = a->id;
        stored.character_bound = false;
        stored.slot = 2;
        require(store.insert_item(stored), "store account item");
        require(store.load_storage_items(a->id).size() == 1, "reload storage");
        SkillRecord skill{c.id, 3, 2};
        require(store.upsert_skill(skill) && store.load_skills(c.id).size() == 1, "persist skills");
        ShortcutRecord shortcuts;
        shortcuts.character_id = c.id;
        shortcuts.slots[19] = 3;
        require(store.upsert_shortcuts(shortcuts), "persist all twenty shortcuts");
        require(store.load_shortcuts(c.id)->slots[19] == 3, "reload last shortcut");
        QuestFlagRecord quest;
        quest.character_id = c.id;
        quest.quest = 1;
        quest.flag = 2;
        require(store.upsert_quest_flag(quest) && store.load_quest_flags(c.id).size() == 1,
                "persist quests");
        BuffRemainRecord buff{c.id, 101, 300, 0, 0, 23000};
        require(store.replace_buff_remains(c.id, {buff}) &&
                    store.load_buff_remains(c.id).size() == 1,
                "persist buff remains");
        InventoryUnlockRecord unlock{c.id, 1, 86400};
        require(store.replace_inventory_unlocks(c.id, {unlock}) &&
                    store.load_inventory_unlocks(c.id).size() == 1,
                "persist inventory unlocks");
        require(store.save_server_event({"contract", 4, 10, 1800000000}) &&
                    store.load_server_event("contract")->contribution_count == 4,
                "persist server event");
        BattleMountRecord mount;
        mount.mount_iid = 100;
        mount.satiety = 77;
        mount.abilities[0] = 2;
        require(store.upsert_battle_mount(mount) && store.load_battle_mount(100)->satiety == 77,
                "persist battle mount");
        require(store.delete_battle_mount(100), "delete battle mount");
        SystemMailRecord mail;
        mail.character_id = c.id;
        mail.source_key = "contract";
        mail.subject = "Reward";
        mail.body = "Synthetic test reward";
        mail.item_index = 1;
        mail.item_count = 1;
        mail.created_at = 100;
        require(store.ensure_system_mail(mail) && store.ensure_system_mail(mail),
                "idempotent system mail");
        const auto mails = store.list_system_mail(c.id);
        require(mails.size() == 1, "list mail");
        ItemRecord attachment;
        attachment.iid = 103;
        require(store.claim_system_mail(c.id, mails[0].id, attachment, 101) ==
                    SystemMailClaimResult::Claimed,
                "atomically claim mail");
        require(store.claim_system_mail(c.id, mails[0].id, attachment, 102) ==
                    SystemMailClaimResult::AlreadyClaimed,
                "reject repeated mail claim");
        require(store.update_system_mail(c.id, mails[0].id, false, 103) &&
                    store.update_system_mail(c.id, mails[0].id, true, 104),
                "read and delete claimed mail");
        SystemMailRecord note;
        note.character_id = c.id;
        note.source_key = "owned-text-message";
        note.subject = "Message only";
        note.body = "No attachment";
        note.created_at = 105;
        require(store.ensure_system_mail(note), "persist a text-only system message");
        const auto notes = store.list_system_mail(c.id);
        const auto note_row = std::find_if(notes.begin(), notes.end(), [](const auto& row) {
            return row.source_key == "owned-text-message";
        });
        require(note_row != notes.end(), "text-only message appears in inbox");
        attachment.iid = 104;
        require(store.claim_system_mail(c.id, note_row->id, attachment, 106) !=
                    SystemMailClaimResult::Claimed,
                "text-only messages cannot create empty inventory items");
        require(store.update_system_mail(c.id, note_row->id, false, 107) &&
                    store.update_system_mail(c.id, note_row->id, true, 108),
                "text-only mail can be read and deleted without claiming");
        require(store.ensure_system_mail(note) && store.list_system_mail(c.id).empty(),
                "deleted text-only mail retains its idempotency key");
        require(store.schedule_character_deletion(c.id, a->id, 100) &&
                    store.list_characters(a->id).empty(),
                "schedule character deletion");
        require(store.restore_character(c.id, a->id, 101) && store.load_character(c.id).has_value(),
                "restore owned character");
        require(store.schedule_character_deletion(c.id, a->id, 100), "schedule final purge");
        CharacterPurgeResult result = CharacterPurgeResult::WorkRemaining;
        for (int i = 0; i < 30 && result != CharacterPurgeResult::CharacterPurged; ++i) {
            result = store.process_character_deletions(100 + kCharacterDeletionGraceSeconds, 2);
        }
        require(result == CharacterPurgeResult::CharacterPurged && !store.load_character(c.id),
                "bounded cascade purge");
        require(store.max_item_iid() >= 102, "global item seed");
        openkal::test::trading_post_contract(store);
        require(openkal::test::g_failures == 0, "market transaction contract");
    }
    struct Peer {
        std::unique_ptr<openkal::server::ISession> session;
        std::vector<std::uint8_t> rx;
        std::vector<std::pair<std::uint8_t, std::vector<std::uint8_t>>> history;
        template <class Predicate> bool wait(Predicate predicate) {
            const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            while (std::chrono::steady_clock::now() < end) {
                for (const auto& frame : history) {
                    if (predicate(frame.first, frame.second)) {
                        return true;
                    }
                }
                if (!session || !session->recv_into(rx)) {
                    return false;
                }
                for (;;) {
                    std::size_t n = 0;
                    std::uint8_t type = 0;
                    std::vector<std::uint8_t> payload;
                    auto state =
                        openkal::protocol::FrameParser::pop(rx.data(), rx.size(), n, type, payload);
                    if (state == openkal::protocol::FrameParseStatus::Invalid) {
                        return false;
                    }
                    if (state == openkal::protocol::FrameParseStatus::Incomplete) {
                        break;
                    }
                    history.emplace_back(type, std::move(payload));
                    rx.erase(rx.begin(), rx.begin() + static_cast<std::ptrdiff_t>(n));
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            return false;
        }
        std::uint32_t enter() {
            using namespace openkal::protocol;
            require(session->send_all(Handshake::build_connect(0)), "send world connect");
            S2CCodeFields challenge{};
            require(wait([&](auto type, const auto& bytes) {
                        return type == static_cast<std::uint8_t>(S2C::Code) &&
                               Handshake::parse_code(bytes.data(), bytes.size(), challenge);
                    }),
                    "receive challenge");
            require(session->send_all(Handshake::build_answer(challenge.protocol_test_value)),
                    "answer world challenge");
            std::uint32_t id = 0;
            require(wait([&](auto type, const auto& bytes) {
                        CreatePlayerFields c;
                        if (type != static_cast<std::uint8_t>(S2C::CreatePlayer) ||
                            !CreatePlayer::parse(bytes.data(), bytes.size(), c) ||
                            (c.class_byte & 0x80) == 0) {
                            return false;
                        }
                        id = c.id;
                        return true;
                    }),
                    "receive authoritative self");
            return id;
        }
    };

    void test_protocol_discipline(const openkal::server::ServerConfig& cfg,
                                  openkal::server::PostgresStore& pg) {
        using namespace openkal;
        const std::string username = "protocol_test";
        const std::string password = "Owned protocol fixture password";
        server::LobbyClient lobby;
        require(lobby.connect(cfg.tcp_host, cfg.tcp_port) &&
                    lobby.register_account(username, "protocol@example.test", password) &&
                    lobby.login(username, password),
                "sign in the independent protocol-enforcement fixture");
        const auto account = lobby.account_id();
        server::CharacterRecord character;
        character.name = "ProtocolHero";
        character.clazz = 0;
        character.str = 23;
        character.hth = 16;
        character.intl = 8;
        character.wis = 8;
        character.dex = 10;
        require(lobby.save_character(character), "create the protocol test character");
        const auto characters = lobby.list_characters(account);
        require(characters.size() == 1, "load the protocol test character");
        const auto character_id = characters.front().id;
        const auto expect_state = [&](int points, int kicks, bool banned) {
            const auto state = pg.load_account_discipline(account);
            require(state && state->points == points && state->kicks == kicks &&
                        state->banned == banned,
                    "persist exact account points, kicks and ban status");
        };
        const auto disconnected = [](Peer& peer) {
            (void) peer.wait([](auto, const auto&) { return false; });
            require(!peer.session->alive(), "server closed the account connection");
        };

        require(lobby.select(character_id).has_value(), "select without entering the world");
        Peer in_lobby{lobby.release_session(), {}, {}};
        require(in_lobby.session->send_all({1, 0}), "send invalid framing after authentication");
        disconnected(in_lobby);
        expect_state(1, 0, false);

        for (int kick = 1; kick <= 3; ++kick) {
            require(lobby.connect(cfg.tcp_host, cfg.tcp_port) && lobby.login(username, password) &&
                        lobby.select(character_id),
                    "reconnect and select the character before a threshold disconnect");
            Peer peer{lobby.release_session(), {}, {}};
            (void) peer.enter();

            protocol::PacketWriter invalid_attack;
            invalid_attack.write_u8(255);
            invalid_attack.write_u32(1);
            const auto invalid_kind =
                invalid_attack.frame(static_cast<std::uint8_t>(protocol::C2S::Attack));
            std::vector<std::uint8_t> burst;
            for (int n = 0; n < (kick == 1 ? 8 : 9); ++n) {
                burst.insert(burst.end(), invalid_kind.begin(), invalid_kind.end());
            }
            const auto late_teleport = protocol::Teleport::frame_client_ack(1, 0);
            burst.insert(burst.end(), late_teleport.begin(), late_teleport.end());
            const auto barrier = protocol::GameExit::frame_client_request();
            burst.insert(burst.end(), barrier.begin(), barrier.end());
            require(peer.session->send_all(burst) && peer.wait([](auto type, const auto&) {
                return type == static_cast<std::uint8_t>(protocol::S2C::AnsGameExit);
            }),
                    "nine points and a late teleport acknowledgment leave the account connected");
            expect_state(9, kick - 1, false);
            require(peer.session->send_all(protocol::Handshake::build_connect(0)),
                    "submit an out-of-phase request at the threshold");
            disconnected(peer);
            expect_state(0, kick, kick == 3);
        }
        require(lobby.connect(cfg.tcp_host, cfg.tcp_port) && !lobby.login(username, password) &&
                    lobby.error() == "This account is banned.",
                "the third threshold disconnect blocks a valid future login");
        lobby.close();
    }
}  // namespace
int main(int argc, char** argv) {
    using namespace openkal;
    const char* base = std::getenv("OPENKAL_TEST_POSTGRES_DSN");
    if (!base || argc != 2) {
        return 77;
    }
    const std::string schema = "openkal_test_" + std::to_string(getpid());
    using Connection = std::unique_ptr<PGconn, decltype(&PQfinish)>;
    using Result = std::unique_ptr<PGresult, decltype(&PQclear)>;
    Connection admin(PQconnectdb(base), PQfinish);
    if (!admin || PQstatus(admin.get()) != CONNECTION_OK) {
        return 1;
    }
    const auto sql = [&](const std::string& text) {
        Result result(PQexec(admin.get(), text.c_str()), PQclear);
        return result && PQresultStatus(result.get()) == PGRES_COMMAND_OK;
    };
    if (!sql("CREATE SCHEMA " + schema)) {
        return 1;
    }
    const std::string dsn = std::string(base) + " options='-csearch_path=" + schema + "'";
    const auto cleanup = [&] { (void) sql("DROP SCHEMA " + schema + " CASCADE"); };
    try {
        server::SqliteStore sqlite;
        require(sqlite.open(":memory:"), "open unchanged SQLite store");
        contract(sqlite);
        server::PostgresStore pg;
        require(pg.open(dsn), "open isolated PostgreSQL schema");
        contract(pg);
        require(pg.execute("ALTER TABLE characters DROP COLUMN role; "
                           "ALTER TABLE accounts DROP COLUMN protocol_points, "
                           "DROP COLUMN protocol_kicks, DROP COLUMN banned; "
                           "UPDATE openkal_schema_version SET version=4"),
                "prepare legacy PostgreSQL schema");
        pg.close();
        require(pg.open(dsn) && pg.load_character(900)->role == server::UserRole::Player,
                "migrate existing PostgreSQL characters to player roles");
        const auto migrated_account = pg.find_account("contract");
        require(migrated_account.has_value(), "preserve account during PostgreSQL migration");
        const auto migrated = pg.load_account_discipline(migrated_account->id);
        require(migrated && migrated->points == 0 && migrated->kicks == 0 && !migrated->banned,
                "migrate existing PostgreSQL accounts without sanctions");
        const auto root = test::temp_dir("multiplayer");
        for (const char* dir : {"maps", "monsters", "items", "starters"}) {
            std::filesystem::create_directories(root + "/" + dir);
        }
        require(test::write_open_area(root + "/maps/n_000_000.akarea"), "write owned map fixture");
        std::ofstream(root + "/monsters/1.json")
            << R"({"index":1,"name":"TestMob","stats":{"race":0,"level":1,"hp":10,"attack":{"min":1,"max":1}},"spawns":[]})";
        std::ofstream(root + "/items/1.json")
            << R"({"index":1,"name":"Test Sword","class":["weapon","sword"]})";
        std::ofstream(root + "/items/2.json")
            << R"({"index":2,"name":"Loot Fixture","class":["general","etc"],"stackable":1})";
        std::ofstream(root + "/starters/common.json")
            << R"({"spawn":[1024, 1024, 100],"items":[{"item_id":1,"count":1,"equipped":true},{"item_id":2,"count":1,"equipped":false}]})";
        server::ServerConfig cfg;
        cfg.maps_dir = root + "/maps";
        cfg.monsters_dir = root + "/monsters";
        cfg.items_dir = root + "/items";
        cfg.starters_dir = root + "/starters";
        cfg.save_dir = root + "/save";
        cfg.store = server::StoreBackend::Postgres;
        cfg.database_dsn = dsn;
        std::ofstream(root + "/guilds.json")
            << R"({"schema":"openkal.guilds/v1","creation":{"minimum_level":1,"minimum_members":2,"require_party":true,"fee_zamogeon":100},"membership":{"application_fee_zamogeon":17},"grades":{"minimum_members_by_grade":[1,2,2,2,26,40,54,70,90,110]},"daily_reward":{"minimum_membership_seconds":0}})";
        cfg.guild_rules_path = root + "/guilds.json";
        std::ofstream(root + "/user_roles.json")
            << R"({"schema":"openkal.user_roles/v1","roles":{"1":{"commands":[]},"2":{"commands":[]},"3":{"commands":["getitemid"]}}})";
        cfg.user_roles_path = root + "/user_roles.json";
        cfg.management_socket = root + "/management.sock";
        cfg.management_token = std::string(32, 'm');
        cfg.multiplayer = true;
        cfg.allow_client_registration = true;
        cfg.transport = server::TransportKind::Tcp;
        cfg.tcp_host = "127.0.0.1";
        cfg.tcp_port = static_cast<std::uint16_t>(std::stoi(argv[1]));
        cfg.spawn_x = 1024;
        cfg.spawn_y = 1024;
        cfg.spawn_z = 100;
        server::ServerHost host;
        require(host.start(cfg), "start TLS multiplayer server");
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        server::ServerHost duplicate_server;
        require(!duplicate_server.start(cfg),
                "reject a second authoritative server for the same database");
        test_protocol_discipline(cfg, pg);
        server::LobbyClient alice, bob;
        require(alice.connect(cfg.tcp_host, cfg.tcp_port) && alice.registration_allowed(),
                "read registration capability");
        require(alice.register_account("Alice", "alice@example.test", "A long test password 1"),
                "register Alice through TLS");
        require(!alice.register_account("Alice", "other@example.test", "A long test password 1"),
                "reject duplicate username");
        require(!alice.register_account("Other", "ALICE@example.test", "A long test password 1"),
                "reject duplicate normalized email");
        require(!alice.login("Alice", "A wrong test password"), "reject wrong password");
        require(alice.login("ALICE", "A long test password 1"), "authenticate normalized username");
        const auto account = pg.find_account("alice");
        require(account && account->password.find("A long test") == std::string::npos &&
                    server::verify_account_password("A long test password 1", account->password),
                "store only salted password hash");
        server::LobbyClient duplicate;
        require(duplicate.connect(cfg.tcp_host, cfg.tcp_port) &&
                    !duplicate.login("alice", "A long test password 1"),
                "reject simultaneous account login");
        duplicate.close();
        require(bob.connect(cfg.tcp_host, cfg.tcp_port) &&
                    bob.register_account("Bob", "bob@example.test", "A long test password 2") &&
                    bob.login("bob", "A long test password 2"),
                "register and sign in independent second client");
        server::LobbyClient race_a, race_b;
        require(race_a.connect(cfg.tcp_host, cfg.tcp_port) &&
                    race_b.connect(cfg.tcp_host, cfg.tcp_port),
                "connect concurrent registration clients");
        auto first_registration = std::async(std::launch::async, [&] {
            return race_a.register_account(
                "RaceUser", "racea@example.test", "Concurrent test password");
        });
        auto second_registration = std::async(std::launch::async, [&] {
            return race_b.register_account(
                "RaceUser", "raceb@example.test", "Concurrent test password");
        });
        require(first_registration.get() != second_registration.get(),
                "concurrent duplicate username has exactly one successful registration");
        race_a.close();
        race_b.close();
        server::CharacterRecord draft;
        draft.name = "AliceHero";
        draft.clazz = 0;
        draft.str = 23;
        draft.hth = 16;
        draft.intl = 8;
        draft.wis = 8;
        draft.dex = 10;
        auto invalid_draft = draft;
        invalid_draft.str = 2000000000;
        require(!alice.save_character(invalid_draft) &&
                    alice.list_characters(alice.account_id()).empty(),
                "reject forged creation stats without partial character rows");
        draft.role = server::UserRole::Admin;
        require(alice.save_character(draft), "create character and starter kit on server");
        auto alice_chars = alice.list_characters(alice.account_id());
        require(alice_chars.size() == 1, "receive remote character list");
        draft.name = "BobHero";
        draft.money = 999999;
        draft.map_x = 999999;
        require(bob.save_character(draft), "create second character with authoritative defaults");
        auto bob_chars = bob.list_characters(bob.account_id());
        require(bob_chars.size() == 1 && bob_chars[0].money == 0 && bob_chars[0].map_x == 1024,
                "ignore client-supplied money and spawn");
        const int aid = alice_chars[0].id, bid = bob_chars[0].id;
        require(pg.load_character(aid)->role == server::UserRole::Player &&
                    pg.load_character(bid)->role == server::UserRole::Player,
                "client-supplied role cannot create privileged characters");
        require(alice.schedule_character_deletion(aid, alice.account_id(), 0),
                "delete owned character through lobby");
        require(alice.list_characters(alice.account_id()).empty() &&
                    alice.list_deleted_characters(alice.account_id(), 0).size() == 1,
                "list remotely recoverable character");
        require(alice.restore_character(aid, alice.account_id(), 0),
                "restore owned character through lobby");
        for (int i = 0; i < 7; ++i) {
            auto extra = draft;
            extra.name = "ExtraHero" + std::to_string(i);
            require(alice.save_character(extra), "create remaining allowed character slots");
        }
        auto excess = draft;
        excess.name = "ExcessHero";
        require(alice.list_characters(alice.account_id()).size() == 8 &&
                    !alice.save_character(excess),
                "enforce eight character limit server-side");
        require(!alice.select(bid), "reject foreign character selection");
        require(!alice.schedule_character_deletion(bid, alice.account_id(), 0),
                "reject foreign deletion");
        auto funded = pg.load_character(aid);
        funded->money = 1000;
        require(pg.save_character(*funded), "fund owned guild test founder");
        require(alice.select(aid).has_value() && bob.select(bid).has_value(),
                "select independent owned characters");
        const auto ai = alice.load_items(aid), bi = bob.load_items(bid);
        require(ai.size() == 1 && bi.size() == 1 && ai[0].iid != bi[0].iid,
                "allocate unique starter item IDs");
        // Explicitly unbind the owned fixture stacks before the sessions load them.
        for (const auto owner : {aid, bid}) {
            for (auto item : pg.load_items(owner)) {
                if (item.slot == 0) {
                    item.account_bound = item.character_bound = false;
                    item.count = 5;
                    item.prefix = 4;
                    item.x_attack = 7;
                    require(pg.update_item(item), "prepare tradeable owned fixture stack");
                }
            }
        }
        Peer pa{alice.release_session(), {}, {}}, pb{bob.release_session(), {}, {}};
        const auto ap = pa.enter(), bp = pb.enter();
        require(ap != bp, "allocate distinct player entities");
        const auto sees = [&](Peer& peer, std::uint32_t target) {
            return peer.wait([&](auto type, const auto& bytes) {
                protocol::CreatePlayerFields c;
                return type == static_cast<std::uint8_t>(protocol::S2C::CreatePlayer) &&
                       protocol::CreatePlayer::parse(bytes.data(), bytes.size(), c) &&
                       c.id == target && (c.class_byte & 0x80) == 0;
            });
        };
        require(sees(pa, bp) && sees(pb, ap), "both clients see each other");
        protocol::PlayerOptionsRequest player_action;
        player_action.target = bp;
        player_action.request_id = 1;
        protocol::PlayerOptionsState offered;
        require(pa.session->send_all(protocol::frame_player_options_request(player_action)),
                "query selected-player options before party formation");
        require(pa.wait([&](auto type, const auto& bytes) {
            return type == static_cast<std::uint8_t>(protocol::S2C::PlayerOptions) &&
                   protocol::parse_player_options_state(bytes.data(), bytes.size(), offered) &&
                   offered.request_id == 1 &&
                   offered.actions ==
                       std::vector<protocol::PlayerAction>{protocol::PlayerAction::InviteParty};
        }),
                "guildless ungrouped target offers only the possible party action");
        player_action.action = protocol::PlayerAction::InviteParty;
        player_action.request_id = 2;
        require(pa.session->send_all(protocol::frame_player_options_request(player_action)),
                "invite founding party member");
        protocol::PartyState party;
        require(pb.wait([&](auto type, const auto& bytes) {
            return type == static_cast<std::uint8_t>(protocol::S2C::PartyInfo) &&
                   protocol::parse_party_state(bytes.data(), bytes.size(), party) &&
                   party.invitation != 0;
        }),
                "receive founding party invitation");
        protocol::PartyRequest accept;
        accept.command = protocol::PartyCommand::Reply;
        accept.invitation = party.invitation;
        accept.value = 1;
        require(pb.session->send_all(protocol::frame_party_request(accept)),
                "accept founding party");
        require(pa.wait([&](auto type, const auto& bytes) {
            protocol::PartyState state;
            return type == static_cast<std::uint8_t>(protocol::S2C::PartyInfo) &&
                   protocol::parse_party_state(bytes.data(), bytes.size(), state) &&
                   state.members.size() == 2;
        }),
                "two-member party exists");
        protocol::GuildRequest create;
        create.command = protocol::GuildCommand::Create;
        create.request_id = 100;
        platform::Json before_creation;
        require(pg.load_guilds(before_creation), "read creation revision");
        create.revision = static_cast<std::uint64_t>(before_creation.at("revision").number());
        create.expected_fee = 100;
        create.text = "TestGuild";
        auto rejected = create;
        rejected.request_id = 99;
        rejected.expected_fee = 99;
        require(pa.session->send_all(protocol::frame_guild_request(rejected)),
                "send rejected creation while the guild window may be closed");
        require(pa.wait([&](auto type, const auto& bytes) {
            std::string sender, text;
            return type == static_cast<std::uint8_t>(protocol::S2C::Chatting) &&
                   protocol::Chatting::parse_s2c(bytes.data(), bytes.size(), sender, text) &&
                   sender == "Server" &&
                   text == "Creation price changed. Refresh the guild window.";
        }),
                "guild rejection is visible in system chat independently of the guild window");
        require(pa.wait([&](auto type, const auto& bytes) {
            protocol::GuildResponse response;
            return type == static_cast<std::uint8_t>(protocol::S2C::Guild) &&
                   protocol::parse_guild_response(bytes.data(), bytes.size(), response) &&
                   response.request_id == 99 && !response.data.at("success").boolean() &&
                   response.data.at("status").as_string() ==
                       "Creation price changed. Refresh the guild window.";
        }),
                "guild rejection also retains its structured window feedback");
        require(pg.load_character(aid)->money == 1000, "rejected creation never debits money");
        require(pg.load_guilds(before_creation), "refresh revision after rejected creation");
        create.revision = static_cast<std::uint64_t>(before_creation.at("revision").number());
        require(pa.session->send_all(protocol::frame_guild_request(create)),
                "request configurable guild creation");
        require(pb.wait([&](auto type, const auto& bytes) {
            protocol::GuildResponse response;
            return type == static_cast<std::uint8_t>(protocol::S2C::Guild) &&
                   protocol::parse_guild_response(bytes.data(), bytes.size(), response) &&
                   response.kind == protocol::GuildMessage::View &&
                   response.data.at("formation").as_int() == aid;
        }),
                "founding consent reaches the second authenticated client");
        protocol::GuildRequest consent;
        consent.command = protocol::GuildCommand::FormationReply;
        consent.request_id = 101;
        consent.target = aid;
        consent.value = 1;
        require(pb.session->send_all(protocol::frame_guild_request(consent)),
                "accept guild founding");
        protocol::GuildResponse guild_view;
        require(pb.wait([&](auto type, const auto& bytes) {
            return type == static_cast<std::uint8_t>(protocol::S2C::Guild) &&
                   protocol::parse_guild_response(bytes.data(), bytes.size(), guild_view) &&
                   guild_view.kind == protocol::GuildMessage::View &&
                   guild_view.data.at("own").at("name").as_string() == "TestGuild";
        }),
                "both founding members join the same persistent guild");
        require(guild_view.data.at("own").at("grade").as_int() == 4,
                "two members directly reach configured grade 4 over TLS");
        require(pg.load_character(aid)->money == 900, "guild cost is exactly one configured debit");
        require(!pg.schedule_character_deletion(aid, alice.account_id(), 100),
                "guild member deletion is guarded in PostgreSQL");
        require(pa.session->send_all(protocol::frame_guild_request(create)),
                "replay founding request");
        require(pa.wait([&](auto type, const auto& bytes) {
            protocol::GuildResponse response;
            return type == static_cast<std::uint8_t>(protocol::S2C::Guild) &&
                   protocol::parse_guild_response(bytes.data(), bytes.size(), response) &&
                   response.request_id == 100;
        }),
                "replay receives an authoritative response");
        require(pg.load_character(aid)->money == 900,
                "replayed guild creation never charges twice");
        require(pb.wait([&](auto type, const auto& bytes) {
            protocol::GuildResponse response;
            return type == static_cast<std::uint8_t>(protocol::S2C::Guild) &&
                   protocol::parse_guild_response(bytes.data(), bytes.size(), response) &&
                   response.kind == protocol::GuildMessage::Display &&
                   response.data.at("player").as_int() == static_cast<int>(ap) &&
                   response.data.at("visible").boolean() &&
                   response.data.at("name").as_string() == "TestGuild";
        }),
                "observer receives public guild-name eligibility");

        server::LobbyClient applicant;
        require(applicant.connect(cfg.tcp_host, cfg.tcp_port) &&
                    applicant.register_account(
                        "Applicant", "applicant@example.test", "A long test password 4") &&
                    applicant.login("applicant", "A long test password 4"),
                "create independent guild applicant");
        auto applicant_draft = draft;
        applicant_draft.name = "ApplicantHero";
        require(applicant.save_character(applicant_draft), "create applicant character");
        const auto applicant_id = applicant.list_characters(applicant.account_id()).front().id;
        auto applicant_wallet = pg.load_character(applicant_id);
        applicant_wallet->money = 100;
        require(pg.save_character(*applicant_wallet), "fund applicant for the quoted fee");
        require(applicant.select(applicant_id).has_value(), "select guildless applicant");
        Peer pc{applicant.release_session(), {}, {}};
        const auto cp = pc.enter();
        require(sees(pc, bp), "applicant sees an ordinary guild member");
        protocol::GuildRequest ranking_request;
        ranking_request.command = protocol::GuildCommand::Ranking;
        ranking_request.request_id = 200;
        require(pc.session->send_all(protocol::frame_guild_request(ranking_request)),
                "guildless player opens ranking");
        require(pc.wait([&](auto type, const auto& bytes) {
            protocol::GuildResponse response;
            return type == static_cast<std::uint8_t>(protocol::S2C::Guild) &&
                   protocol::parse_guild_response(bytes.data(), bytes.size(), response) &&
                   response.request_id == 200 &&
                   response.data.at("own").at("guild").as_int() == 0 &&
                   response.data.at("rows").array().size() == 1;
        }),
                "new guild is immediately discoverable by an outsider");
        player_action = {};
        player_action.target = bp;
        player_action.request_id = 201;
        require(pc.session->send_all(protocol::frame_player_options_request(player_action)),
                "query options on ordinary guild member");
        require(pc.wait([&](auto type, const auto& bytes) {
            return type == static_cast<std::uint8_t>(protocol::S2C::PlayerOptions) &&
                   protocol::parse_player_options_state(bytes.data(), bytes.size(), offered) &&
                   offered.request_id == 201 && offered.guild_name == "TestGuild" &&
                   offered.actions ==
                       std::vector<protocol::PlayerAction>{protocol::PlayerAction::ApplyGuild};
        }),
                "ordinary member offers guild application without leader privilege");
        player_action.action = protocol::PlayerAction::ApplyGuild;
        player_action.guild = offered.guild;
        player_action.expected_fee = offered.application_fee;
        player_action.request_id = 202;
        player_action.expected_fee = 16;
        require(pc.session->send_all(protocol::frame_player_options_request(player_action)),
                "submit stale application price");
        require(pc.wait([&](auto type, const auto& bytes) {
            protocol::PlayerOptionsState response;
            return type == static_cast<std::uint8_t>(protocol::S2C::PlayerOptions) &&
                   protocol::parse_player_options_state(bytes.data(), bytes.size(), response) &&
                   response.request_id == 202 &&
                   response.status.find("Application price changed") == 0;
        }) && pg.load_character(applicant_id)->money == 100,
                "stale quoted price is rejected without a debit");
        player_action.expected_fee = 17;
        require(pc.session->send_all(protocol::frame_player_options_request(player_action)),
                "apply through selected guild member");
        require(pc.wait([&](auto type, const auto& bytes) {
            return type == static_cast<std::uint8_t>(protocol::S2C::PlayerOptions) &&
                   protocol::parse_player_options_state(bytes.data(), bytes.size(), offered) &&
                   offered.request_id == 202 && offered.actions.empty() &&
                   offered.status.find("Application sent to TestGuild") == 0;
        }),
                "application is confirmed and duplicate action disappears");
        platform::Json application_state;
        require(pg.load_guilds(application_state) &&
                    application_state.at("applications").array().size() == 1 &&
                    application_state.at("applications").array()[0].at("character").as_int() ==
                        applicant_id,
                "context action creates a durable guild application");
        require(pc.session->send_all(protocol::frame_player_options_request(player_action)),
                "replay contextual application");
        require(pc.wait([&](auto type, const auto& bytes) {
            protocol::PlayerOptionsState response;
            return type == static_cast<std::uint8_t>(protocol::S2C::PlayerOptions) &&
                   protocol::parse_player_options_state(bytes.data(), bytes.size(), response) &&
                   response.request_id == 202;
        }),
                "replayed context application receives feedback");
        require(pg.load_guilds(application_state) &&
                    application_state.at("applications").array().size() == 1,
                "replay never creates duplicate applications");
        require(pg.load_character(applicant_id)->money == 83,
                "context application charges the displayed fee exactly once");
        player_action.action = protocol::PlayerAction::Query;
        player_action.target = cp;
        player_action.request_id = 203;
        require(pc.session->send_all(protocol::frame_player_options_request(player_action)),
                "query forged self context");
        require(pc.wait([&](auto type, const auto& bytes) {
            protocol::PlayerOptionsState response;
            return type == static_cast<std::uint8_t>(protocol::S2C::PlayerOptions) &&
                   protocol::parse_player_options_state(bytes.data(), bytes.size(), response) &&
                   response.request_id == 203 && response.actions.empty() &&
                   !response.status.empty();
        }),
                "self context is rejected without side effects");
        pc.session->close();
        require(pa.session->send_all(protocol::Move::frame_client_end(1, 0, 0)),
                "send first player movement");
        require(pb.wait([&](auto type, const auto& bytes) {
            std::uint32_t id = 0;
            std::int8_t x = 0, y = 0, z = 0;
            return type == static_cast<std::uint8_t>(protocol::S2C::MovePlayerEnd) &&
                   protocol::Move::parse_player(bytes.data(), bytes.size(), id, x, y, z) &&
                   id == ap && x == 1;
        }),
                "replicate movement to second client");
        require(pa.session->send_all(protocol::Chatting::frame_c2s("hello multiplayer")),
                "send multiplayer chat");
        require(pb.wait([](auto type, const auto& bytes) {
            return type == static_cast<std::uint8_t>(protocol::S2C::Chatting) &&
                   std::string(bytes.begin(), bytes.end()).find("hello multiplayer") !=
                       std::string::npos;
        }),
                "deliver chat to second client");

        std::this_thread::sleep_for(std::chrono::milliseconds(1100));
        require(pa.session->send_all(protocol::Chatting::frame_c2s("$guild integration")),
                "send guild channel text");
        require(pb.wait([](auto type, const auto& bytes) {
            std::string speaker, text;
            return type == static_cast<std::uint8_t>(protocol::S2C::Chatting) &&
                   protocol::Chatting::parse_s2c(bytes.data(), bytes.size(), speaker, text) &&
                   text == "$guild integration";
        }),
                "guild chat reaches the other connected member");
        {
            using protocol::TradeCommand;
            protocol::TradeState ta, tb;
            const auto snapshot = [&](Peer& peer, protocol::TradeState& state, auto predicate) {
                return peer.wait([&](auto type, const auto& bytes) {
                    protocol::TradeState next;
                    if (type != static_cast<std::uint8_t>(protocol::S2C::OpenTrade) ||
                        !protocol::parse_trade_state(bytes.data(), bytes.size(), next) ||
                        !predicate(next)) {
                        return false;
                    }
                    state = std::move(next);
                    return true;
                });
            };
            const auto send = [&](Peer& peer,
                                  TradeCommand command,
                                  const protocol::TradeState& state,
                                  std::vector<protocol::TradeAmount> items = {},
                                  int money = 0) {
                protocol::TradeRequest request;
                request.command = command;
                request.target = &peer == &pa ? bp : ap;
                request.token = state.token;
                request.revision = state.revision;
                request.items = std::move(items);
                request.money = money;
                require(peer.session->send_all(protocol::frame_trade_request(request)),
                        "send real TLS trade command");
            };
            const auto a_before = pg.load_character(aid)->money,
                       b_before = pg.load_character(bid)->money;
            send(pa, TradeCommand::Invite, {});
            require(
                snapshot(pb,
                         tb,
                         [](const auto& v) { return v.phase == protocol::TradePhase::Invitation; }),
                "real recipient receives trade invitation");
            require(
                snapshot(pa,
                         ta,
                         [](const auto& v) { return v.phase == protocol::TradePhase::Invitation; }),
                "inviter is locked too");
            // These forged packets bypass the normal client traffic gate.
            require(pa.session->send_all(protocol::Move::frame_client_end(5, 0, 0)) &&
                        pa.session->send_all(protocol::Chatting::frame_c2s("trade-forbidden-chat")),
                    "inject non-trade packets while invited");
            send(pb, TradeCommand::Accept, tb);
            require(snapshot(pa,
                             ta,
                             [](const auto& v) { return v.phase == protocol::TradePhase::Open; }),
                    "accept opens both trade editors");
            require(snapshot(pb,
                             tb,
                             [](const auto& v) { return v.phase == protocol::TradePhase::Open; }),
                    "recipient receives complete offer snapshot");
            require(!ta.available.empty() && !tb.available.empty(),
                    "both participants have owned trade items");
            const auto a_item = ta.available.front().iid, b_item = tb.available.front().iid;
            send(pa, TradeCommand::Offer, ta, {{a_item, 2, 11}}, 5);
            require(snapshot(pa, ta, [](const auto& v) { return v.revision == 2; }),
                    "publish first offer");
            require(snapshot(pb, tb, [](const auto& v) { return v.revision == 2; }),
                    "peer sees same revision");
            send(pa, TradeCommand::Confirm, ta);
            require(snapshot(pb,
                             tb,
                             [](const auto& v) { return v.revision == 2 && v.partner_confirmed; }),
                    "one confirmation cannot commit");
            send(pb, TradeCommand::Offer, tb, {{b_item, 6, 0}});
            require(snapshot(pb,
                             tb,
                             [](const auto& v) {
                                 return v.status == "That item or quantity cannot be traded.";
                             }),
                    "server rejects a forged quantity above the owned stack");
            require(tb.revision == 2 && tb.partner_confirmed,
                    "rejected quantity changes no offer or existing acceptance");
            send(pb, TradeCommand::Offer, tb, {{b_item, 3, 0}});
            require(snapshot(pa, ta, [](const auto& v) { return v.revision == 3; }),
                    "changing peer offer invalidates earlier confirmation");
            require(!ta.confirmed && !ta.partner_confirmed, "both confirmations reset");
            send(pa, TradeCommand::Confirm, ta);
            send(pb, TradeCommand::Confirm, tb);
            // tb still refers to revision 2: its stale confirmation must not complete anything.
            require(snapshot(pb, tb, [](const auto& v) { return v.revision == 3; }),
                    "stale confirm returns current revision");
            require(pg.load_character(aid)->money == a_before &&
                        pg.load_character(bid)->money == b_before,
                    "stale confirmation moves no money");
            send(pb, TradeCommand::Confirm, tb);
            require(snapshot(pa, ta, [](const auto& v) { return !v.locked(); }) &&
                        snapshot(pb, tb, [](const auto& v) { return !v.locked(); }),
                    "both matching confirmations close the trade");
            require(ta.phase == protocol::TradePhase::Completed &&
                        tb.phase == protocol::TradePhase::Completed && ta.other.size() == 1 &&
                        tb.other.size() == 1 && ta.other.front().iid == b_item &&
                        ta.other.front().count == 3 && ta.partner_money == 0 &&
                        tb.other.front().iid == a_item && tb.other.front().count == 2 &&
                        tb.partner_money == 5,
                    "both committed receipts contain only that participant's incoming offer");
            require(pg.load_character(aid)->money == a_before - 5 &&
                        pg.load_character(bid)->money == b_before + 5,
                    "TLS trade persists conserved money");
            const auto a_items = pg.load_items(aid), b_items = pg.load_items(bid);
            require(std::any_of(a_items.begin(),
                                a_items.end(),
                                [&](const auto& item) {
                                    return item.iid == static_cast<int>(a_item) && item.count == 3;
                                }) &&
                        std::any_of(b_items.begin(),
                                    b_items.end(),
                                    [&](const auto& item) {
                                        return item.iid == static_cast<int>(b_item) &&
                                               item.count == 2;
                                    }),
                    "partial trade retains both source stacks and their remaining quantities");
            require(std::any_of(a_items.begin(),
                                a_items.end(),
                                [&](const auto& item) {
                                    return item.iid != static_cast<int>(a_item) &&
                                           item.count == 3 && item.prefix == 4 &&
                                           item.x_attack == 7;
                                }) &&
                        std::any_of(b_items.begin(),
                                    b_items.end(),
                                    [&](const auto& item) {
                                        return item.iid != static_cast<int>(b_item) &&
                                               item.count == 2 && item.prefix == 4 &&
                                               item.x_attack == 7;
                                    }),
                    "TLS trade splits new instances without losing modifiers or quantities");
            require(pg.load_character(aid)->map_x == 1025, "trade gate rejects forged movement");
            require(std::none_of(pb.history.begin(),
                                 pb.history.end(),
                                 [](const auto& frame) {
                                     std::string speaker, text;
                                     return frame.first == static_cast<std::uint8_t>(
                                                               protocol::S2C::Chatting) &&
                                            protocol::Chatting::parse_s2c(frame.second.data(),
                                                                          frame.second.size(),
                                                                          speaker,
                                                                          text) &&
                                            text == "trade-forbidden-chat";
                                 }),
                    "server gate drops chat during invitation");
            pa.history.clear();
            pb.history.clear();
            send(pb, TradeCommand::Invite, {});
            require(
                snapshot(pa,
                         ta,
                         [](const auto& v) { return v.phase == protocol::TradePhase::Invitation; }),
                "either player can initiate a fresh trade");
            send(pa, TradeCommand::Cancel, ta);
            require(snapshot(pa, ta, [](const auto& v) { return !v.locked(); }) &&
                        snapshot(pb, tb, [](const auto& v) { return !v.locked(); }),
                    "declining invitation unlocks both participants");
            require(ta.phase == protocol::TradePhase::Closed &&
                        tb.phase == protocol::TradePhase::Closed && ta.other.empty() &&
                        tb.other.empty() && ta.partner_money == 0 && tb.partner_money == 0,
                    "cancellation sends no committed receipt");
            require(pg.load_character(aid)->money == a_before - 5 &&
                        pg.load_character(bid)->money == b_before + 5,
                    "cancelled trade cannot change either balance");
        }
        const auto inventory = pg.load_items(aid);
        const auto loot = std::find_if(
            inventory.begin(), inventory.end(), [](const auto& item) { return item.index == 2; });
        require(loot != inventory.end(), "server grants synthetic bag loot");
        const auto world_item = static_cast<std::uint32_t>(loot->iid);
        require(pa.session->send_all(protocol::DropItem::frame_c2s(world_item, 1)),
                "drop an owned item into the shared world");
        const auto sees_item = [&](Peer& peer) {
            return peer.wait([&](auto type, const auto& bytes) {
                protocol::PacketReader reader(bytes);
                std::uint16_t index = 0;
                std::uint32_t id = 0;
                return type == static_cast<std::uint8_t>(protocol::S2C::CreateItem) &&
                       reader.read_u16(index) && reader.read_u32(id) && id == world_item;
            });
        };
        require(sees_item(pa) && sees_item(pb), "both clients receive shared ground item");
        protocol::PacketWriter pickup;
        pickup.write_u32(world_item);
        pickup.write_u32(0);
        pickup.write_u32(0);
        const auto pickup_frame =
            pickup.frame(static_cast<std::uint8_t>(protocol::C2S::PickupItem));
        require(pa.session->send_all(pickup_frame) && pb.session->send_all(pickup_frame),
                "compete for the same ground item");
        require(pa.wait([&](auto type, const auto& bytes) {
            protocol::PacketReader reader(bytes);
            std::uint32_t id = 0;
            return type == static_cast<std::uint8_t>(protocol::S2C::RemoveItem) &&
                   reader.read_u32(id) && id == world_item;
        }),
                "ground item is removed after one pickup");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        int persisted_loot = 0;
        for (const auto id : {aid, bid}) {
            for (const auto& item : pg.load_items(id)) {
                if (item.index == 2) {
                    persisted_loot += item.count;
                }
            }
        }
        require(persisted_loot == 10,
                "competing pickup preserves the fixture quantity without duplication");
        {
            const auto seller_items = pg.load_items(bid);
            const auto item =
                std::find_if(seller_items.begin(), seller_items.end(), [](const auto& row) {
                    return row.index == 2 && row.count > 0 && row.slot == 0;
                });
            require(item != seller_items.end(), "find owned tradeable market fixture");
            pa.history.clear();
            pb.history.clear();
            const auto market_state = [&](Peer& peer,
                                          protocol::TradingPostView view,
                                          protocol::TradingPostState& out) {
                return peer.wait([&](auto type, const auto& bytes) {
                    return type == static_cast<std::uint8_t>(protocol::S2C::TradingPost) &&
                           protocol::parse_trading_post_state(bytes.data(), bytes.size(), out) &&
                           out.acknowledged == 1 && out.view == view;
                });
            };
            protocol::TradingPostRequest sale;
            sale.operation = 1;
            sale.command = protocol::TradingPostCommand::Sell;
            sale.view = protocol::TradingPostView::Own;
            sale.iid = static_cast<std::uint32_t>(item->iid);
            sale.unit_price = 1;
            protocol::TradingPostState offers;
            require(pb.session->send_all(protocol::frame_trading_post_request(sale)) &&
                        market_state(pb, protocol::TradingPostView::Own, offers) &&
                        offers.entries.size() == 1,
                    "online seller lists one owned item");
            protocol::TradingPostRequest purchase;
            purchase.operation = 1;
            purchase.command = protocol::TradingPostCommand::Buy;
            purchase.listing = offers.entries.front().id;
            purchase.unit_price = 1;
            protocol::TradingPostState bought;
            require(pa.session->send_all(protocol::frame_trading_post_request(purchase)) &&
                        market_state(pa, protocol::TradingPostView::Buy, bought) &&
                        bought.purchased == 1,
                    "online buyer purchases the live offer");
            const auto notified = [](auto type, const auto& bytes) {
                bool pending = false;
                return type == static_cast<std::uint8_t>(protocol::S2C::TradingPostCollection) &&
                       protocol::parse_trading_post_collection(
                           bytes.data(), bytes.size(), pending) &&
                       pending;
            };
            require(
                pa.wait(notified) && pb.wait(notified),
                "buyer and online seller receive collection indicators without opening Collection");
        }
        pa.session->close();
        require(pb.wait([&](auto type, const auto& bytes) {
            std::uint32_t id = 0;
            return type == static_cast<std::uint8_t>(protocol::S2C::RemovePlayer) &&
                   protocol::CreatePlayer::parse_remove(bytes.data(), bytes.size(), id) && id == ap;
        }),
                "remove disconnected player from other client");
        pb.session->close();
        host.stop();
        platform::Json guild_state;
        require(pg.load_guilds(guild_state) && guild_state.at("guilds").array().size() == 1 &&
                    guild_state.at("guilds").array()[0].at("members").array().size() == 2,
                "disconnect preserves both guild memberships");
        auto saved = pg.load_character(aid);
        require(saved && saved->map_x == 1025 && !pg.load_items(aid).empty(),
                "persist logout position and inventory");
        cfg.allow_client_registration = false;
        require(host.start(cfg), "restart same persistent server with registration disabled");
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        server::LobbyClient rejoin;
        require(rejoin.connect(cfg.tcp_host, cfg.tcp_port) && !rejoin.registration_allowed(),
                "advertise disabled registration");
        require(!rejoin.register_account("Carol", "carol@example.test", "A long test password 3"),
                "server rejects registration even when directly requested");
        require(rejoin.login("alice", "A long test password 1") &&
                    rejoin.select(aid)->map_x == 1025,
                "existing account rejoins persisted character after restart");
        Peer returning{rejoin.release_session(), {}, {}};
        returning.enter();
        require(returning.wait([](auto type, const auto& bytes) {
            bool pending = false;
            return type == static_cast<std::uint8_t>(protocol::S2C::TradingPostCollection) &&
                   protocol::parse_trading_post_collection(bytes.data(), bytes.size(), pending) &&
                   pending;
        }),
                "purchased collection restores its indicator after multiplayer reconnect");
        require(returning.wait([](auto type, const auto& bytes) {
            protocol::GuildResponse response;
            return type == static_cast<std::uint8_t>(protocol::S2C::Guild) &&
                   protocol::parse_guild_response(bytes.data(), bytes.size(), response) &&
                   response.kind == protocol::GuildMessage::View &&
                   response.data.at("own").at("name").as_string() == "TestGuild";
        }),
                "guild identity is restored after server restart");
        const auto inventory_total = [&] {
            int total = 0;
            for (const auto& item : pg.load_items(aid)) {
                if (item.index == 2) {
                    total += item.count;
                }
            }
            return total;
        };
        const auto command_barrier = [&] {
            require(returning.session->send_all(protocol::GameExit::frame_client_request()) &&
                        returning.wait([](auto type, const auto&) {
                            return type == static_cast<std::uint8_t>(protocol::S2C::AnsGameExit);
                        }),
                    "wait for role command and persistence barrier");
        };
        const auto denied_command = [&](const std::string& command) {
            returning.history.clear();
            require(returning.session->send_all(protocol::Chatting::frame_c2s(command)),
                    "send privileged command");
            require(returning.wait([](auto type, const auto& bytes) {
                std::string speaker, text;
                return type == static_cast<std::uint8_t>(protocol::S2C::Chatting) &&
                       protocol::Chatting::parse_s2c(bytes.data(), bytes.size(), speaker, text) &&
                       text == "You do not have permission to use this command.";
            }),
                    "ungranted multiplayer command is denied");
            command_barrier();
        };
        for (int role = 0; role < 3; ++role) {
            require(pg.execute("UPDATE characters SET role=" + std::to_string(role) +
                               " WHERE id=" + std::to_string(aid)),
                    "set unprivileged or ungranted persisted role");
            const int before = inventory_total();
            denied_command("/getitemid 2 0 3");
            require(inventory_total() == before, "roles 0, 1 and 2 cannot grant items");
        }
        require(pg.execute("UPDATE characters SET role=3 WHERE id=" + std::to_string(aid)),
                "promote persisted character to admin");
        returning.history.clear();
        const int before_admin_grant = inventory_total();
        require(returning.session->send_all(protocol::Chatting::frame_c2s("/GETITEMID 2 0 3")),
                "admin sends case-insensitive item grant over multiplayer");
        command_barrier();
        require(inventory_total() == before_admin_grant + 3 &&
                    pg.load_character(aid)->role == server::UserRole::Admin,
                "admin item grant persists inventory and ordinary save preserves role");
        const int before_level = pg.load_character(aid)->level;
        denied_command("/level 50");
        require(pg.load_character(aid)->level == before_level,
                "admin receives no implicit grant for other commands");
        require(pg.execute("UPDATE characters SET role=0 WHERE id=" + std::to_string(aid)),
                "revoke admin role while online");
        denied_command("/getitemid 2 0 3");
        require(inventory_total() == before_admin_grant + 3,
                "online revocation takes effect on the next command");
        const auto management = [&](platform::Json command) {
            return server::management_request(
                cfg.management_socket, cfg.management_token, std::move(command));
        };
        const auto connection = management(platform::Json::parse(R"({"op":"database"})"));
        require(connection.at("error").is_null() &&
                    connection.at("database_dsn").as_string() == cfg.database_dsn,
                "authenticated management console receives the server database connection");
        const auto rejected_connection =
            server::management_request(cfg.management_socket,
                                       std::string(32, 'x'),
                                       platform::Json::parse(R"({"op":"database"})"));
        require(!rejected_connection.at("error").as_string().empty() &&
                    rejected_connection.at("database_dsn").is_null(),
                "invalid management credentials never receive database settings");
        auto status = management(platform::Json::parse(R"({"op":"status","after":0})"));
        bool alice_online = false, bob_online = false;
        for (const auto& value : status.at("online").array()) {
            alice_online = alice_online || value.as_int() == aid;
            bob_online = bob_online || value.as_int() == bid;
        }
        require(alice_online && !bob_online,
                "local API reports live authoritative character presence");
        require(!server::management_request(cfg.management_socket,
                                            std::string(32, 'x'),
                                            platform::Json::parse(R"({"op":"status"})"))
                     .at("error")
                     .as_string()
                     .empty(),
                "management endpoint rejects a wrong secret independently of game login");
        auto pm = platform::Json::parse(R"({"op":"pm","body":"Owned support PM"})");
        pm.object()["character"].set_number(aid);
        returning.history.clear();
        require(management(pm).at("error").is_null(), "management sends a PM to an online player");
        require(returning.wait([](auto type, const auto& bytes) {
            std::string speaker, text;
            return type == static_cast<std::uint8_t>(protocol::S2C::Chatting) &&
                   protocol::Chatting::parse_s2c(bytes.data(), bytes.size(), speaker, text) &&
                   speaker == server::kManagementSpeaker &&
                   text.find("Owned support PM") != std::string::npos;
        }),
                "player receives management message through the ordinary whisper channel");
        returning.history.clear();
        require(returning.session->send_all(
                    protocol::Chatting::frame_c2s("@!Support Owned player reply")) &&
                    returning.wait([](auto type, const auto& bytes) {
                        std::string speaker, text;
                        return type == static_cast<std::uint8_t>(protocol::S2C::Chatting) &&
                               protocol::Chatting::parse_s2c(
                                   bytes.data(), bytes.size(), speaker, text) &&
                               text.find("Owned player reply") != std::string::npos;
                    }),
                "player replies to the reserved support whisper identity");
        status = management(platform::Json::parse(R"({"op":"status","after":0})"));
        require(status.at("events").array().size() == 2 &&
                    status.at("events").array()[0].at("outgoing").boolean() &&
                    !status.at("events").array()[1].at("outgoing").boolean(),
                "console receives both directions of the support conversation");
        pm.object()["character"].set_number(bid);
        require(!management(pm).at("error").as_string().empty(),
                "PM to an offline player is rejected rather than reported delivered");
        auto mail = platform::Json::parse(
            R"({"op":"mail","subject":"Support message","body":"Message without an attachment","request_id":"management-message-fixture-0001","item_index":0,"item_count":0})");
        mail.object()["character"].set_number(aid);
        returning.history.clear();
        require(management(mail).at("error").is_null(),
                "management persists text-only in-game mail");
        std::vector<protocol::MailEntry> inbox;
        require(returning.wait([&](auto type, const auto& bytes) {
            return type == static_cast<std::uint8_t>(protocol::S2C::Mail) &&
                   protocol::Mail::parse_list_s2c(bytes.data(), bytes.size(), inbox) &&
                   std::any_of(inbox.begin(), inbox.end(), [](const auto& entry) {
                       return entry.subject == "Support message" && entry.item_index == 0 &&
                              entry.item_count == 0;
                   });
        }),
                "online player receives a valid message-only inbox snapshot immediately");
        const auto message_count = pg.list_system_mail(aid).size();
        require(management(mail).at("error").is_null() &&
                    pg.list_system_mail(aid).size() == message_count,
                "repeating a mail request cannot duplicate its durable message");
        const auto text_mail = std::find_if(inbox.begin(), inbox.end(), [](const auto& entry) {
            return entry.subject == "Support message";
        });
        const auto text_id = text_mail->id;
        returning.history.clear();
        require(returning.session->send_all(
                    protocol::Mail::frame_action_c2s(protocol::MailCommand::Delete, text_id)) &&
                    returning.wait([&](auto type, const auto& bytes) {
                        std::vector<protocol::MailEntry> entries;
                        return type == static_cast<std::uint8_t>(protocol::S2C::Mail) &&
                               protocol::Mail::parse_list_s2c(
                                   bytes.data(), bytes.size(), entries) &&
                               std::none_of(entries.begin(), entries.end(), [&](const auto& entry) {
                                   return entry.id == text_id;
                               });
                    }),
                "message-only mail can be deleted without claiming an item");
        mail.object()["character"].set_number(bid);
        mail.object()["request_id"].string() = "management-offline-fixture-0001";
        require(management(mail).at("error").is_null(),
                "management accepts mail for offline characters");
        const auto offline_mail = pg.list_system_mail(bid);
        require(std::any_of(offline_mail.begin(),
                            offline_mail.end(),
                            [](const auto& entry) { return entry.subject == "Support message"; }),
                "offline mail remains durable until the recipient reconnects");
        server::LobbyClient offline_return;
        require(offline_return.connect(cfg.tcp_host, cfg.tcp_port) &&
                    offline_return.login("bob", "A long test password 2") &&
                    offline_return.select(bid).has_value(),
                "offline mail recipient reconnects through the regular lobby");
        Peer mail_recipient{offline_return.release_session(), {}, {}};
        mail_recipient.enter();
        require(mail_recipient.wait([](auto type, const auto& bytes) {
            std::vector<protocol::MailEntry> entries;
            return type == static_cast<std::uint8_t>(protocol::S2C::Mail) &&
                   protocol::Mail::parse_list_s2c(bytes.data(), bytes.size(), entries) &&
                   std::any_of(entries.begin(), entries.end(), [](const auto& entry) {
                       return entry.subject == "Support message" && entry.item_index == 0;
                   });
        }),
                "offline management mail is delivered on the recipient's next world entry");
        mail_recipient.session->close();
        mail.object()["character"].set_number(aid);
        mail.object()["request_id"].string() = "management-attachment-fixture-0001";
        mail.object()["item_index"].set_number(2);
        mail.object()["item_count"].set_number(2);
        returning.history.clear();
        require(management(mail).at("error").is_null(), "management queues an item attachment");
        const auto attached = pg.list_system_mail(aid);
        const auto found_attachment =
            std::find_if(attached.begin(), attached.end(), [](const auto& entry) {
                return entry.source_key == "management:management-attachment-fixture-0001";
            });
        require(found_attachment != attached.end(),
                "item attachment belongs to the selected character");
        const int before_attachment = inventory_total();
        require(returning.session->send_all(protocol::Mail::frame_claim_c2s(
                    static_cast<std::uint32_t>(found_attachment->id))) &&
                    returning.wait([&](auto type, const auto& bytes) {
                        protocol::MailClaimResult result;
                        return type == static_cast<std::uint8_t>(protocol::S2C::Mail) &&
                               protocol::Mail::parse_claim_result_s2c(
                                   bytes.data(), bytes.size(), result) &&
                               result.id == found_attachment->id && result.claimed;
                    }),
                "player claims a management item through the normal inventory transaction");
        require(inventory_total() == before_attachment + 2,
                "management attachment adds its exact quantity once");
        returning.session->close();
        rejoin.close();
        Result terminated(PQexec(admin.get(),
                                 ("SELECT pg_terminate_backend(pid) FROM pg_stat_activity WHERE "
                                  "application_name='openkal-world:" +
                                  schema + "'")
                                     .c_str()),
                          PQclear);
        require(terminated && PQresultStatus(terminated.get()) == PGRES_TUPLES_OK &&
                    PQntuples(terminated.get()) == 1,
                "interrupt only this test world's database connection");
        const auto stop_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(4);
        while (host.running() && std::chrono::steady_clock::now() < stop_deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        require(!host.running(),
                "database loss closes the authoritative world without a local fallback");
        host.stop();
        require(pg.ping() && pg.load_character(aid).has_value(),
                "separate inspector connection and saved data survive world shutdown");
        require(pg.execute("UPDATE openkal_schema_version SET version=99"),
                "prepare unsupported future test schema");
        server::PostgresStore future;
        require(!future.open(dsn), "reject unsupported future schema without migration");
        pg.close();
        cleanup();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << std::endl;
        cleanup();
        return 1;
    }
}
