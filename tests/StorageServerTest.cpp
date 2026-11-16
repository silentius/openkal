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
#include "openkal/platform/Json.h"
#include "openkal/protocol/ClockSync.h"
#include "openkal/protocol/CreatePlayer.h"
#include "openkal/protocol/FrameDrain.h"
#include "openkal/protocol/GameExit.h"
#include "openkal/protocol/Handshake.h"
#include "openkal/protocol/InsertItem.h"
#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketReader.h"
#include "openkal/protocol/PacketWriter.h"
#include "openkal/protocol/Quest.h"
#include "openkal/protocol/Storage.h"
#include "openkal/protocol/TalentTree.h"
#include "openkal/server/ServerHost.h"
#include "openkal/server/TestControl.h"
#include "openkal/server/store/DataStore.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <unistd.h>

#if defined(OPENKAL_STORAGE_POSTGRES)
#include <libpq-fe.h>
#endif

namespace {
    using namespace openkal;
    using Bytes = std::vector<std::uint8_t>;
    using Clock = std::chrono::steady_clock;

    void require(bool condition, const std::string& message) {
        if (!condition) {
            throw std::runtime_error(message);
        }
    }

    struct Options {
        bool postgres = false;
        std::uint16_t port = 0;
    };

    Options options(int argc, char** argv) {
        Options result;
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--postgres") {
                result.postgres = true;
            } else if (arg == "--port" && i + 1 < argc) {
                const int port = std::stoi(argv[++i]);
                require(port > 0 && port <= 65535, "valid TLS port");
                result.port = static_cast<std::uint16_t>(port);
            } else {
                throw std::runtime_error("unknown storage-test argument");
            }
        }
        require(!result.postgres || result.port != 0, "PostgreSQL requires a TLS port");
        return result;
    }

    struct TemporaryDirectory {
        std::string path;

        TemporaryDirectory() {
            char pattern[] = "/tmp/openkal-storage-XXXXXX";
            const char* created = ::mkdtemp(pattern);
            require(created != nullptr, "create owned storage fixture directory");
            path = created;
        }

        ~TemporaryDirectory() {
            std::error_code error;
            std::filesystem::remove_all(path, error);
        }
    };

#if defined(OPENKAL_STORAGE_POSTGRES)
    struct OwnedSchema {
        std::unique_ptr<PGconn, decltype(&PQfinish)> connection{nullptr, PQfinish};
        std::string name;

        ~OwnedSchema() {
            if (connection && !name.empty()) {
                std::unique_ptr<PGresult, decltype(&PQclear)> result(
                    PQexec(connection.get(), ("DROP SCHEMA " + name + " CASCADE").c_str()),
                    PQclear);
            }
        }

        std::string create(const std::string& suffix) {
            const char* base = std::getenv("OPENKAL_TEST_POSTGRES_DSN");
            require(base != nullptr, "PostgreSQL DSN is required");
            connection.reset(PQconnectdb(base));
            require(connection && PQstatus(connection.get()) == CONNECTION_OK,
                    "connect owned PostgreSQL database");
            PQsetNoticeProcessor(connection.get(), [](void*, const char*) {}, nullptr);
            std::string candidate = "openkal_storage_" + suffix;
            std::transform(candidate.begin(),
                           candidate.end(),
                           candidate.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            std::unique_ptr<PGresult, decltype(&PQclear)> result(
                PQexec(connection.get(), ("CREATE SCHEMA " + candidate).c_str()), PQclear);
            require(result && PQresultStatus(result.get()) == PGRES_COMMAND_OK,
                    "create isolated owned PostgreSQL schema");
            name = candidate;
            return std::string(base) + " options='-csearch_path=" + name + "'";
        }
    };
#endif

    struct Fixture {
        TemporaryDirectory directory;
#if defined(OPENKAL_STORAGE_POSTGRES)
        OwnedSchema schema;
#endif
        server::ServerConfig config;
        std::unique_ptr<server::IDataStore> store;
        std::array<server::CharacterRecord, 2> characters{};
        server::ItemRecord stack;
        server::ItemRecord gear;
        server::ItemRecord foreign;

        void write(const std::string& relative, const std::string& text) const {
            std::ofstream stream(directory.path + "/" + relative);
            stream << text;
            require(static_cast<bool>(stream), "write owned " + relative);
        }

        explicit Fixture(const Options& opts) {
            for (const char* part : {"maps", "monsters", "items", "npcs", "starters", "quests"}) {
                std::filesystem::create_directories(directory.path + "/" + part);
            }
            config.maps_dir = directory.path + "/maps";
            config.monsters_dir = directory.path + "/monsters";
            config.items_dir = directory.path + "/items";
            config.npcs_dir = directory.path + "/npcs";
            config.starters_dir = directory.path + "/starters";
            config.quests_dir = directory.path + "/quests";
            config.save_dir = directory.path + "/save";
            config.unix_path = directory.path + "/server.sock";
            config.map_catalog_config = directory.path + "/mapinfo.json";
            config.mapinfo_path = config.map_catalog_config;
            config.portals_path = directory.path + "/portals.json";
            config.spawn_x = config.spawn_y = 1024;
            config.spawn_z = 0;
            config.max_monsters = 1;
            config.map_radius_cells = 0;
            config.transport =
                opts.postgres ? server::TransportKind::Tcp : server::TransportKind::Unix;
            config.store =
                opts.postgres ? server::StoreBackend::Postgres : server::StoreBackend::Sqlite;
            config.multiplayer = opts.postgres;
            config.tcp_host = "127.0.0.1";
            config.tcp_port = opts.port;
            config.test_control_token = opts.postgres ? "OwnedStorageControlSecret2026" : "";
            require(test::write_open_area(config.maps_dir + "/n_000_000.akarea"),
                    "write synthetic walkable area");
            // All catalogs and accounts belong to this test; no external game data is read.
            write("mapinfo.json", R"({"maps":[{"index":0,"kind":1,"xy":[0, 0]}]})");
            write("portals.json", R"({"portals":[]})");
            write("monsters/1.json", R"({"index":1,"stats":{"level":1,"hp":1},"spawns":[]})");
            write(
                "items/9000.json",
                R"({"index":9000,"name":"Owned Storage Stack","class":["general","etc"],"stackable":1})");
            write(
                "items/9001.json",
                R"({"index":9001,"name":"Owned Storage Sword","class":["weapon","sword"],"wear":1,"endurance":100})");
            write(
                "npcs/1.json",
                R"({"index":60001,"name":"Owned Keeper","map":0,"xy":[1024, 1024, 0],"storage_keeper":true})");
            write("npcs/2.json",
                  R"({"index":60002,"name":"Owned Nonkeeper","map":0,"xy":[1056, 1024, 0]})");
            write("starters/common.json", R"({"spawn":[1024, 1024, 0],"items":[]})");
            if (opts.postgres) {
#if defined(OPENKAL_STORAGE_POSTGRES)
                config.database_dsn =
                    schema.create(directory.path.substr(directory.path.find_last_of('-') + 1));
#else
                throw std::runtime_error("PostgreSQL support is not compiled");
#endif
            }
            store = server::create_data_store(config.store);
            require(store && store->open(opts.postgres ? config.database_dsn : config.save_dir),
                    "open owned storage backend");
            for (int ordinal = 1; ordinal <= 2; ++ordinal) {
                server::CharacterRecord draft;
                draft.name = server::test_identity("TestPlayer", ordinal);
                draft.map_x = draft.map_y = 1024;
                draft.map_z = 0;
                draft.money = ordinal == 1 ? 120 : 180;
                const auto account_name = server::test_identity("test", ordinal);
                require(store->ensure_default_player(
                            account_name, server::test_identity("TestPassword", ordinal), draft),
                        "create legitimate owned account and character");
                const auto account = store->find_account(account_name);
                require(account.has_value(), "load owned account");
                const auto roster = store->list_characters(account->id);
                require(roster.size() == 1, "one character per owned account");
                characters[static_cast<std::size_t>(ordinal - 1)] = roster.front();
                for (const auto& row : store->load_items(roster.front().id)) {
                    require(store->delete_item(row.iid), "clear any default inventory");
                }
            }
            config.character_id = characters[0].id;
            stack.iid = 1001;
            stack.character_id = characters[0].id;
            stack.account_id = characters[0].account_id;
            stack.index = 9000;
            stack.prefix = 7;
            stack.count = 10;
            stack.account_bound = true;
            stack.enchant_protected = true;
            require(store->insert_item(stack), "seed account-bound stack");
            gear = stack;
            gear.iid = 1002;
            gear.index = 9001;
            gear.count = 1;
            gear.cur_end = 73;
            gear.max_end = 100;
            gear.x_attack = 3;
            gear.x_magic = 4;
            gear.x_hit = 5;
            gear.x_defense = 6;
            gear.x_dodge = 7;
            gear.upgrade_level = 2;
            gear.upgrade_rate = 8;
            gear.mix_type = 9;
            gear.bof = 1;
            gear.qigong_perforated = 1;
            gear.qigong_effect = 11;
            gear.dragon_effect = 12;
            gear.random_stat_type = 1;
            gear.random_stat_value = 3;
            gear.transform_level = 4;
            gear.transform_exp = 123;
            gear.pet_expire_at = 4102444800;
            gear.ride_expire_at = 4102444801;
            require(store->insert_item(gear), "seed complete instance metadata");
            auto other = stack;
            other.iid = 1003;
            other.character_id = characters[1].id;
            other.account_id = characters[1].account_id;
            other.count = 4;
            require(store->insert_item(other), "seed second account inventory");
            foreign = other;
            foreign.iid = 2001;
            foreign.character_id = 0;
            foreign.count = 9;
            require(store->insert_item(foreign), "seed foreign account storage");
        }
    };

    struct Packet {
        std::uint8_t type = 0;
        Bytes payload;
    };

    struct BagItem {
        std::uint32_t iid = 0;
        std::uint16_t index = 0;
        std::uint8_t prefix = 0;
        std::uint32_t info = 0;
        std::int32_t count = 0;
    };

    BagItem bag_item(const Packet& packet) {
        BagItem item;
        protocol::PacketReader reader(packet.payload);
        require(reader.read_u32(item.iid) && reader.read_u16(item.index) &&
                    reader.read_u8(item.prefix) && reader.read_u32(item.info) &&
                    reader.read_i32(item.count),
                "parse absolute bag update");
        return item;
    }

    struct Peer {
        std::unique_ptr<server::ISession> session;
        Bytes rx;
        std::vector<Packet> packets;
        std::map<int, std::uint32_t> npcs;
        std::map<std::uint32_t, BagItem> bag;
        int money = 0;
        int money_updates = 0;
        std::uint32_t self = 0;
        int clocks = 0;
        int talents = 0;
        int storage_updates = 0;
        int exit_acks = 0;
        protocol::StorageInfo storage;

        void send(const Bytes& frame) {
            require(session && session->send_all(frame), "send ordinary storage-test request");
        }

        void receive() {
            const bool connected = session->recv_into(rx);
            {
                protocol::FrameDrain drain(rx);
                std::uint8_t type = 0;
                Bytes payload;
                for (;;) {
                    const auto status = drain.pop(type, payload);
                    require(status != protocol::FrameParseStatus::Invalid,
                            "valid framed server stream");
                    if (status == protocol::FrameParseStatus::Incomplete) {
                        break;
                    }
                    packets.push_back({type, payload});
                    const auto opcode = static_cast<protocol::S2C>(type);
                    if (opcode == protocol::S2C::Code) {
                        protocol::S2CCodeFields code;
                        require(
                            protocol::Handshake::parse_code(payload.data(), payload.size(), code),
                            "parse world challenge");
                        send(protocol::Handshake::build_answer(code.protocol_test_value));
                    } else if (opcode == protocol::S2C::CreatePlayer) {
                        protocol::CreatePlayerFields player;
                        require(
                            protocol::CreatePlayer::parse(payload.data(), payload.size(), player),
                            "parse player presentation");
                        if (player.class_byte & 0x80) {
                            self = player.id;
                        }
                    } else if (opcode == protocol::S2C::CreateNpc) {
                        protocol::CreateNpcInfo npc;
                        require(
                            protocol::Quest::parse_create_npc(payload.data(), payload.size(), npc),
                            "parse live keeper identity");
                        npcs[npc.index] = npc.id;
                    } else if (opcode == protocol::S2C::ClockSync) {
                        protocol::ClockSyncFields clock;
                        require(protocol::ClockSync::parse(payload.data(), payload.size(), clock),
                                "parse world-ready clock");
                        ++clocks;
                    } else if (opcode == protocol::S2C::TalentTree) {
                        protocol::TalentState state;
                        require(protocol::parse_talent_state(payload.data(), payload.size(), state),
                                "parse FIFO request barrier");
                        ++talents;
                    } else if (opcode == protocol::S2C::StorageInfo) {
                        require(protocol::Storage::parse_info_s2c(
                                    payload.data(), payload.size(), storage),
                                "parse complete storage snapshot");
                        ++storage_updates;
                    } else if (opcode == protocol::S2C::InsertItem) {
                        const auto item = bag_item(packets.back());
                        if (item.iid == 0) {
                            money = item.count;
                            ++money_updates;
                        } else if (item.count <= 0) {
                            bag.erase(item.iid);
                        } else if ((item.info & protocol::kInsertReplace) || !bag.count(item.iid)) {
                            bag[item.iid] = item;
                        } else {
                            bag[item.iid].count += item.count;
                        }
                    } else if (opcode == protocol::S2C::AnsGameExit) {
                        bool saved = false;
                        require(protocol::GameExit::parse_server_answer(
                                    payload.data(), payload.size(), saved) &&
                                    saved,
                                "game-exit response confirms successful durable save");
                        ++exit_acks;
                    }
                }
            }
            require(connected, "gameplay connection remains alive");
        }

        template <typename Predicate> void wait(Predicate done, const std::string& label) {
            const auto deadline = Clock::now() + std::chrono::seconds(8);
            while (!done()) {
                receive();
                require(Clock::now() < deadline, "storage watchdog: " + label);
                if (!done()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                }
            }
        }

        void barrier() {
            const int before = talents;
            send(protocol::frame_talent_request(protocol::TalentCommand::Snapshot));
            wait([&] { return talents > before; }, "FIFO talent barrier");
        }

        void open(std::uint32_t keeper) {
            const int before = storage_updates;
            send(protocol::Storage::frame_info_c2s(keeper));
            wait([&] { return storage_updates > before; }, "storage list");
            require(storage.npc_id == keeper, "storage snapshot carries selected runtime keeper");
        }

        void transfer(bool deposit, std::uint32_t keeper, std::uint32_t iid, std::uint32_t count) {
            const int before = storage_updates;
            packets.clear();
            send(protocol::Storage::frame_transfer_c2s(deposit, keeper, iid, count));
            wait([&] { return storage_updates > before; }, "successful storage transfer");
        }

        std::vector<Packet> storage_packets() const {
            std::vector<Packet> result;
            for (const auto& packet : packets) {
                if (packet.type == static_cast<std::uint8_t>(protocol::S2C::InsertItem) ||
                    packet.type == static_cast<std::uint8_t>(protocol::S2C::StorageInfo)) {
                    result.push_back(packet);
                }
            }
            return result;
        }
    };

    void enter(Peer& peer, Fixture& fixture, const Options& opts, int identity) {
        if (opts.postgres) {
            server::TestControlClient login;
            const auto deadline = Clock::now() + std::chrono::seconds(8);
            bool connected = false;
            while (!connected && Clock::now() < deadline) {
                connected = login.connect("127.0.0.1",
                                          opts.port,
                                          fixture.config.test_control_token,
                                          server::tls_config_from_environment());
                if (!connected) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                }
            }
            require(connected, "connect trusted owned TLS session");
            const auto response = login.request(platform::Json::parse(
                "{\"op\":\"login\",\"id\":" + std::to_string(identity) + "}"));
            require(response.at("error").as_string().empty(), "select legitimate managed identity");
            peer.session = login.release_session();
        } else {
            const auto deadline = Clock::now() + std::chrono::seconds(8);
            while (!peer.session && Clock::now() < deadline) {
                peer.session = server::connect_unix(fixture.config.unix_path);
                if (!peer.session) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                }
            }
        }
        require(peer.session != nullptr, "connect storage gameplay client");
        peer.send(protocol::Handshake::build_connect(0));
        peer.wait(
            [&] {
                return peer.self && peer.clocks && peer.npcs.count(60001) && peer.npcs.count(60002);
            },
            "complete normal gameplay handshake and NPC presentation");
        peer.barrier();
        peer.packets.clear();
    }

    void same_metadata(const server::ItemRecord& actual, const server::ItemRecord& expected) {
        const auto fields = [](const server::ItemRecord& item) {
            return std::tie(item.index,
                            item.prefix,
                            item.cur_end,
                            item.max_end,
                            item.x_attack,
                            item.x_magic,
                            item.x_hit,
                            item.x_defense,
                            item.x_dodge,
                            item.upgrade_level,
                            item.upgrade_rate,
                            item.mix_type,
                            item.bof,
                            item.qigong_perforated,
                            item.qigong_effect,
                            item.dragon_effect,
                            item.random_stat_type,
                            item.random_stat_value,
                            item.transform_level,
                            item.transform_exp,
                            item.pet_expire_at,
                            item.ride_expire_at,
                            item.account_bound,
                            item.enchant_protected,
                            item.character_bound);
        };
        require(fields(actual) == fields(expected),
                "all durable instance metadata survives transfer");
    }

    server::ItemRecord row(const std::vector<server::ItemRecord>& rows, std::int32_t iid) {
        const auto found = std::find_if(
            rows.begin(), rows.end(), [iid](const auto& item) { return item.iid == iid; });
        require(found != rows.end(), "expected owned item row exists");
        return *found;
    }

    void wire_metadata(const protocol::StorageItem& item, const server::ItemRecord& expected) {
        require(expected.transform_exp >= 0, "owned transform EXP is nonnegative");
        require(item.index == expected.index && item.prefix == expected.prefix &&
                    item.info == protocol::pack_insert_info(true,
                                                            expected.cur_end,
                                                            expected.max_end,
                                                            false,
                                                            expected.account_bound,
                                                            expected.enchant_protected,
                                                            expected.character_bound) &&
                    item.transform_level == expected.transform_level &&
                    item.transform_exp == static_cast<std::uint32_t>(expected.transform_exp),
                "storage wire preserves identity, flags, endurance and progression");
        std::array<std::uint8_t, protocol::kInsertCharmBytes> actual{};
        protocol::pack_insert_charm(actual.data(), item.charm);
        const std::array<std::uint8_t, protocol::kInsertCharmBytes> wanted = {
            static_cast<std::uint8_t>(expected.x_attack),
            static_cast<std::uint8_t>(expected.x_magic),
            static_cast<std::uint8_t>(expected.x_hit),
            static_cast<std::uint8_t>(expected.x_defense),
            static_cast<std::uint8_t>(expected.x_dodge),
            static_cast<std::uint8_t>(expected.upgrade_level),
            static_cast<std::uint8_t>(expected.upgrade_rate),
            static_cast<std::uint8_t>(expected.mix_type),
            static_cast<std::uint8_t>(expected.bof),
            static_cast<std::uint8_t>(expected.qigong_perforated),
            static_cast<std::uint8_t>(expected.qigong_effect),
            static_cast<std::uint8_t>(expected.dragon_effect),
            static_cast<std::uint8_t>(expected.random_stat_type),
            static_cast<std::uint8_t>(expected.random_stat_value)};
        require(actual == wanted, "storage wire preserves all fourteen charm bytes");
    }

    void transfer_order(const Peer& peer, std::uint32_t iid, int count, int money = -1) {
        const auto packets = peer.storage_packets();
        require(packets.size() == static_cast<std::size_t>(money >= 0 ? 3 : 2),
                "exact number of bag/money/storage transfer frames");
        require(packets.front().type == static_cast<std::uint8_t>(protocol::S2C::InsertItem) &&
                    packets.back().type == static_cast<std::uint8_t>(protocol::S2C::StorageInfo),
                "bag update precedes storage snapshot");
        const auto item = bag_item(packets.front());
        require(item.iid == iid && item.count == count && (item.info & protocol::kInsertReplace),
                "absolute bag update uses the correct instance and count");
        if (money >= 0) {
            require(packets[1].type == static_cast<std::uint8_t>(protocol::S2C::InsertItem),
                    "money follows bag update before storage snapshot");
            const auto balance = bag_item(packets[1]);
            require(balance.iid == 0 && balance.count == money,
                    "deposit charges exactly sixty Geon");
        }
    }

    void rejected(Peer& peer, const Bytes& request) {
        const int before = peer.storage_updates;
        peer.packets.clear();
        peer.send(request);
        peer.barrier();
        require(peer.storage_updates == before && peer.storage_packets().empty(),
                "rejected request produces no transfer or storage snapshot before FIFO barrier");
    }

    void exercise(const Options& opts) {
        Fixture fixture(opts);
        server::ServerHost host;
        require(host.start(fixture.config), "start unchanged authoritative storage host");
        Peer a, b;
        enter(a, fixture, opts, fixture.characters[0].id);
        if (opts.postgres) {
            enter(b, fixture, opts, fixture.characters[1].id);
            require(a.self != b.self &&
                        fixture.characters[0].account_id != fixture.characters[1].account_id,
                    "TLS peers have distinct real accounts, characters and runtime entities");
            b.open(b.npcs.at(60001));
            require(b.storage.items.size() == 1 && b.storage.items.front().iid == 2001,
                    "second account sees only its own preexisting storage");
        }
        const auto keeper = a.npcs.at(60001);
        rejected(a, protocol::Storage::frame_transfer_c2s(true, keeper, 1001, 3));
        a.open(keeper);
        require(a.storage.items.empty(), "first account does not list foreign storage");
        protocol::PacketWriter trailing_info;
        trailing_info.write_u32(a.npcs.at(60002));
        trailing_info.write_u8(1);
        rejected(a, trailing_info.frame(static_cast<std::uint8_t>(protocol::C2S::StorageInfo)));
        rejected(a, protocol::Storage::frame_transfer_c2s(false, keeper, 2001, 1));
        require(
            row(fixture.store->load_storage_items(fixture.characters[1].account_id), 2001).count ==
                9,
            "foreign storage IID cannot be withdrawn by first account");

        a.transfer(true, keeper, 1001, 3);
        transfer_order(a, 1001, 7, 60);
        require(a.storage.items.size() == 1 && a.storage.items.front().count == 3 &&
                    a.storage.items.front().slot == 0,
                "partial deposit chooses first storage cell");
        const auto split = a.storage.items.front().iid;
        require(split > 2001 && split != 1001, "partial deposit allocates a fresh global IID");
        wire_metadata(a.storage.items.front(), fixture.stack);
        const auto stored_split =
            row(fixture.store->load_storage_items(fixture.characters[0].account_id),
                static_cast<int>(split));
        require(stored_split.character_id == 0 &&
                    stored_split.account_id == fixture.characters[0].account_id &&
                    stored_split.count == 3 && stored_split.slot == 0,
                "partial deposit persists account custody and quantity");
        same_metadata(stored_split, fixture.stack);
        require(row(fixture.store->load_items(fixture.characters[0].id), 1001).count == 7,
                "partial deposit persists stack remainder");
        if (opts.postgres) {
            rejected(b, protocol::Storage::frame_transfer_c2s(false, b.npcs.at(60001), split, 1));
            b.open(b.npcs.at(60001));
            require(b.storage.items.size() == 1 && b.storage.items.front().iid == 2001,
                    "second live account cannot list or withdraw first account split");
            b.transfer(true, b.npcs.at(60001), 1003, 4);
            transfer_order(b, 1003, 0, 120);
            require(b.storage.items.size() == 2 && b.storage.items[1].iid == 1003 &&
                        b.storage.items[1].slot == 1,
                    "interleaved second-account deposit uses its own first free cell");
        }

        a.transfer(true, keeper, 1002, 1);
        transfer_order(a, 1002, 0, 0);
        require(a.storage.items.size() == 2 && a.storage.items[1].iid == 1002 &&
                    a.storage.items[1].slot == 1,
                "whole deposit keeps IID after existing split");
        wire_metadata(a.storage.items[1], fixture.gear);
        same_metadata(
            row(fixture.store->load_storage_items(fixture.characters[0].account_id), 1002),
            fixture.gear);
        const auto after_deposits = fixture.store->load_character(fixture.characters[0].id);
        require(after_deposits && after_deposits->money == 0,
                "two successful deposits durably spend exactly120 Geon");
        rejected(a, protocol::Storage::frame_transfer_c2s(true, keeper, 1001, 1));
        require(row(fixture.store->load_items(fixture.characters[0].id), 1001).count == 7,
                "insufficient deposit fee leaves source unchanged");

        const int before_invalid = a.storage_updates;
        a.send(protocol::Storage::frame_info_c2s(a.npcs.at(60002)));
        a.barrier();
        require(a.storage_updates == before_invalid, "nonkeeper does not return a storage list");
        rejected(a, protocol::Storage::frame_transfer_c2s(false, keeper, 1002, 1));
        require(
            row(fixture.store->load_storage_items(fixture.characters[0].account_id), 1002).count ==
                1,
            "invalid keeper clears authorization for an otherwise valid owned withdrawal");
        a.open(keeper);

        a.transfer(false, keeper, split, 2);
        const auto first_withdrawal = bag_item(a.storage_packets().front()).iid;
        require(first_withdrawal > split && first_withdrawal != 1001,
                "partial withdrawal uses a new IID without merging");
        transfer_order(a, first_withdrawal, 2);
        require(a.storage.items.size() == 2 && a.storage.items.front().iid == split &&
                    a.storage.items.front().count == 1,
                "partial withdrawal retains stored identity and one-unit remainder");
        const auto partial_row = row(fixture.store->load_items(fixture.characters[0].id),
                                     static_cast<int>(first_withdrawal));
        require(partial_row.count == 2 && partial_row.slot == 0 &&
                    partial_row.account_id == fixture.characters[0].account_id,
                "withdrawn split belongs to the selected character and account");
        same_metadata(partial_row, fixture.stack);
        a.transfer(false, keeper, split, 1);
        transfer_order(a, split, 1);
        require(a.storage.items.size() == 1 && a.storage.items.front().iid == 1002,
                "whole remainder withdrawal keeps IID and leaves unrelated gear");
        a.transfer(false, keeper, 1002, 1);
        transfer_order(a, 1002, 1);
        require(a.storage.items.empty(),
                "all first-account items returned without a withdrawal fee");
        same_metadata(row(fixture.store->load_items(fixture.characters[0].id), 1002), fixture.gear);
        const auto after_withdrawals = fixture.store->load_character(fixture.characters[0].id);
        require(after_withdrawals && after_withdrawals->money == 0,
                "withdrawals are free at zero money");
        rejected(a, protocol::Storage::frame_transfer_c2s(false, keeper, split, 1));

        const int saves = a.exit_acks;
        a.send(protocol::GameExit::frame_client_request());
        a.wait([&] { return a.exit_acks > saves; }, "durable game-exit barrier");
        if (opts.postgres) {
            const int other_saves = b.exit_acks;
            b.send(protocol::GameExit::frame_client_request());
            b.wait([&] { return b.exit_acks > other_saves; }, "second-account durable exit");
            b.session->close();
        }
        a.session->close();
        host.stop();
        fixture.store->close();
        require(fixture.store->open(opts.postgres ? fixture.config.database_dsn
                                                  : fixture.config.save_dir),
                "reopen backend after authoritative shutdown");
        const auto inventory = fixture.store->load_items(fixture.characters[0].id);
        require(inventory.size() == 4 && row(inventory, 1001).count == 7 &&
                    row(inventory, static_cast<int>(first_withdrawal)).count == 2 &&
                    row(inventory, static_cast<int>(split)).count == 1 &&
                    row(inventory, 1002).count == 1,
                "durable bag preserves split identities and total quantities");
        same_metadata(row(inventory, 1002), fixture.gear);
        require(fixture.store->load_storage_items(fixture.characters[0].account_id).empty(),
                "empty first-account storage survives shutdown");
        const auto other_storage =
            fixture.store->load_storage_items(fixture.characters[1].account_id);
        require(other_storage.size() == static_cast<std::size_t>(opts.postgres ? 2 : 1) &&
                    row(other_storage, 2001).count == 9,
                "foreign account storage remains isolated and durable");

        require(host.start(fixture.config), "restart authoritative host from persisted storage");
        Peer restored, restored_other;
        enter(restored, fixture, opts, fixture.characters[0].id);
        restored.open(restored.npcs.at(60001));
        const auto restored_character = fixture.store->load_character(fixture.characters[0].id);
        require(
            restored_character && restored_character->money == 0 &&
                restored.storage.items.empty() && restored.money == 0 &&
                restored.money_updates == 0 && restored.bag.size() == 4 &&
                restored.bag.at(1001).count == 7 && restored.bag.at(first_withdrawal).count == 2 &&
                restored.bag.at(split).count == 1 && restored.bag.at(1002).count == 1,
            "ordinary reconnect restores bag identities and empty storage while zero money remains implicit");
        require(restored.bag.at(1002).prefix == fixture.gear.prefix &&
                    protocol::insert_cur_end(restored.bag.at(1002).info) == fixture.gear.cur_end &&
                    protocol::insert_max_end(restored.bag.at(1002).info) == fixture.gear.max_end &&
                    protocol::insert_account_bound(restored.bag.at(1002).info),
                "ordinary reconnect presents persisted instance flags and endurance");
        if (opts.postgres) {
            enter(restored_other, fixture, opts, fixture.characters[1].id);
            restored_other.open(restored_other.npcs.at(60001));
            require(restored_other.money == 120 && restored_other.bag.empty() &&
                        restored_other.storage.items.size() == 2 &&
                        restored_other.storage.items[0].iid == 2001 &&
                        restored_other.storage.items[1].iid == 1003,
                    "second real TLS account reconnects to its own persisted money and storage");
            restored_other.session->close();
        }
        restored.session->close();
        host.stop();
        std::cout << (opts.postgres ? "PostgreSQL/TLS distinct-account"
                                    : "SQLite/Unix configured-account")
                  << " storage routing baseline passed\n";
    }
}  // namespace

int main(int argc, char** argv) {
    try {
        exercise(options(argc, argv));
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "StorageServerTest: " << error.what() << '\n';
        return 1;
    }
}
