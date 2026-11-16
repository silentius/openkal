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
#include "openkal/protocol/Attack.h"
#include "openkal/protocol/Chatting.h"
#include "openkal/protocol/ClockSync.h"
#include "openkal/protocol/CreatePlayer.h"
#include "openkal/protocol/DropItem.h"
#include "openkal/protocol/FrameDrain.h"
#include "openkal/protocol/GameExit.h"
#include "openkal/protocol/Handshake.h"
#include "openkal/protocol/Move.h"
#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketReader.h"
#include "openkal/protocol/PacketWriter.h"
#include "openkal/protocol/Skill.h"
#include "openkal/protocol/TalentTree.h"
#include "openkal/protocol/Teleport.h"
#include "openkal/server/ServerHost.h"
#include "openkal/server/TestControl.h"
#include "openkal/server/store/DataStore.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/resource.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

#if defined(OPENKAL_LOAD_POSTGRES)
#include <libpq-fe.h>
#endif

namespace {
    using namespace openkal;
    using Bytes = std::vector<std::uint8_t>;
    using Clock = std::chrono::steady_clock;
    using Json = platform::Json;

    void require(bool condition, const std::string& message) {
        if (!condition) {
            throw std::runtime_error(message);
        }
    }

    struct Options {
        bool postgres = false;
        int clients = 4;
        int rounds = 3;
        int warmup = 1;
        std::uint16_t port = 0;
        std::string report;
    };

    Options options(int argc, char** argv) {
        Options out;
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--sqlite") {
                out.postgres = false;
            } else if (arg == "--postgres") {
                out.postgres = true;
            } else {
                require(i + 1 < argc, "missing value for " + arg);
                const std::string value = argv[++i];
                if (arg == "--clients") {
                    out.clients = std::stoi(value);
                } else if (arg == "--rounds") {
                    out.rounds = std::stoi(value);
                } else if (arg == "--warmup") {
                    out.warmup = std::stoi(value);
                } else if (arg == "--port") {
                    const int port = std::stoi(value);
                    require(port > 0 && port <= 65535, "invalid port");
                    out.port = static_cast<std::uint16_t>(port);
                } else if (arg == "--report") {
                    out.report = value;
                } else {
                    throw std::runtime_error("unknown argument " + arg);
                }
            }
        }
        require(out.clients >= 1 && out.clients <= 64, "clients must be 1 through 64");
        require(out.rounds >= 1 && out.rounds <= 1000 && out.warmup >= 0 && out.warmup <= 100,
                "invalid bounded round count");
        return out;
    }

    struct Fixture {
        std::string root;
        server::ServerConfig config;
        std::unique_ptr<server::IDataStore> store;
#if defined(OPENKAL_LOAD_POSTGRES)
        std::unique_ptr<PGconn, decltype(&PQfinish)> admin{nullptr, PQfinish};
        std::string schema;
#endif

        explicit Fixture(const Options& opts) {
            char pattern[] = "/tmp/openkal-load-XXXXXX";
            const char* path = ::mkdtemp(pattern);
            require(path != nullptr, "create unique owned fixture directory");
            root = path;
            for (const auto* dir : {"maps",
                                    "monsters",
                                    "items",
                                    "save",
                                    "groups",
                                    "itemgroups",
                                    "starters",
                                    "npcs",
                                    "skills"}) {
                std::filesystem::create_directories(root + "/" + dir);
            }
            config.maps_dir = root + "/maps";
            config.monsters_dir = root + "/monsters";
            config.items_dir = root + "/items";
            config.save_dir = root + "/save";
            config.groups_dir = root + "/groups";
            config.itemgroups_dir = root + "/itemgroups";
            config.starters_dir = root + "/starters";
            config.npcs_dir = root + "/npcs";
            config.skills_dir = root + "/skills";
            config.map_catalog_config = root + "/mapinfo.json";
            config.mapinfo_path = config.map_catalog_config;
            config.portals_path = root + "/portals.json";
            config.unix_path = root + "/game.sock";
            config.spawn_x = config.spawn_y = 1024;
            config.spawn_z = 0;
            config.map_radius_cells = 0;
            config.max_monsters = 1;
            config.tick_hz = 50;
            config.store =
                opts.postgres ? server::StoreBackend::Postgres : server::StoreBackend::Sqlite;
            config.transport =
                opts.postgres ? server::TransportKind::Tcp : server::TransportKind::Unix;
            config.multiplayer = opts.postgres;
            config.tcp_host = "127.0.0.1";
            config.tcp_port = opts.port;
            config.test_control_token = opts.postgres ? "OwnedLoadControlSecret2026" : "";
            write(
                "mapinfo.json",
                R"({"maps":[{"index":0,"kind":1,"xy":[0, 0]}, {"index":9,"kind":1,"xy":[0, 0]}]})");
            write(
                "portals.json",
                R"({"portals":[{"id":"owned_load_gate","fromMap":0,"fromRect":[36, 32, 37, 33],"toMap":9,"to":[1536, 1536, 0]}]})");
            require(test::write_open_area(config.maps_dir + "/n_000_000.akarea"),
                    "write owned walkable map cell");
            write(
                "monsters/1.json",
                R"({"index":1,"name":"Owned Load Target","stats":{"race":0,"level":1,"hp":1,"exp":100,"ai":0,"dodge":0,"mspeed":[1000000, 1000000],"itemGroups":[[1, 1], [2, 1]]},"spawns":[{"map":0,"max":1,"cycle":3600,"rect":[32, 32, 33, 33]}]})");
            write(
                "items/1.json",
                R"({"index":1,"name":"Owned Load Sword","class":["weapon","sword"],"wear":1,"specialty":{"attack":[500, 500],"hit":500}})");
            write(
                "items/2.json",
                R"({"index":2,"name":"Owned Load Token","class":["general","etc"],"stackable":1})");
            write("groups/1.json", R"({"index":1,"entries":[{"item":2,"chance":100}]})");
            write("groups/2.json",
                  R"({"index":2,"entries":[{"type":"money","amount":17,"chance":100}]})");
            write("itemgroups/1.json", R"({"index":1,"entries":[{"group":1,"chance":100}]})");
            write("itemgroups/2.json", R"({"index":2,"entries":[{"group":2,"chance":100}]})");
            write("starters/common.json", R"({"spawn":[1024, 1024, 0],"items":[]})");
            write(
                "npcs/1.json",
                R"({"index":1,"name":"Owned Load Witness","kind":0,"shape":1,"map":0,"xy":[1088, 1088, 0]})");
            write("skills/0.json",
                  R"({"class":0,"index":0,"type":["use"],"level":1,"mp":0,"maxLevel":1})");
            // All catalogs and identities are generated here; no external game data is read.
            if (opts.postgres) {
#if defined(OPENKAL_LOAD_POSTGRES)
                const char* base = std::getenv("OPENKAL_TEST_POSTGRES_DSN");
                require(base != nullptr, "PostgreSQL DSN is required");
                admin.reset(PQconnectdb(base));
                require(admin && PQstatus(admin.get()) == CONNECTION_OK, "connect PostgreSQL");
                PQsetNoticeProcessor(admin.get(), [](void*, const char*) {}, nullptr);
                schema = "openkal_load_" + root.substr(root.find_last_of('-') + 1);
                sql("CREATE SCHEMA " + schema);
                config.database_dsn = std::string(base) + " options='-csearch_path=" + schema + "'";
#endif
            }
            store = server::create_data_store(config.store);
            require(store && store->open(opts.postgres ? config.database_dsn : config.save_dir),
                    "initialize isolated persistence schema");
        }

        ~Fixture() {
            if (store) {
                store->close();
            }
#if defined(OPENKAL_LOAD_POSTGRES)
            if (admin && !schema.empty()) {
                const auto query = "DROP SCHEMA " + schema + " CASCADE";
                auto* result = PQexec(admin.get(), query.c_str());
                PQclear(result);
            }
#endif
            if (!root.empty()) {
                std::filesystem::remove_all(root);
            }
        }

        void write(const std::string& relative, const std::string& text) {
            std::ofstream file(root + "/" + relative);
            file << text;
            require(static_cast<bool>(file), "write " + relative);
        }

#if defined(OPENKAL_LOAD_POSTGRES)
        void sql(const std::string& query) {
            std::unique_ptr<PGresult, decltype(&PQclear)> result(PQexec(admin.get(), query.c_str()),
                                                                 PQclear);
            require(result && PQresultStatus(result.get()) == PGRES_COMMAND_OK,
                    "execute owned schema SQL");
        }
#endif
    };

    struct Peer {
        std::unique_ptr<server::ISession> session;
        Bytes rx;
        protocol::CreatePlayerFields self;
        bool ready = false;
        bool closed = false;
        bool saved = false;
        int character_id = 0;
        int x = 1024;
        int y = 1024;
        int map = 0;
        std::string barrier;
        bool chatted = false;
        std::int64_t money = 0;
        std::uint64_t own_moves = 0;
        std::uint64_t talent_replies = 0;
        std::uint64_t progression_frames = 0;
        protocol::TalentState talents;
        std::set<std::uint32_t> ground;
        std::map<std::uint32_t, std::pair<int, int>> ground_positions;
        std::map<std::uint32_t, std::pair<int, int>> inventory;
        std::set<std::uint32_t> removed;
        std::set<std::uint32_t> dead_entities;
        std::uint64_t teleports = 0;
        std::uint64_t position_snapshots = 0;
    };

    void send(Peer& peer, const Bytes& frame) {
        require(peer.session && peer.session->send_all(frame), "send bounded ordinary frame");
    }

    void receive(Peer& peer) {
        if (!peer.session || peer.closed) {
            return;
        }
        const bool connected = peer.session->recv_into(peer.rx);
        protocol::FrameDrain drain(peer.rx);
        Bytes payload;
        std::uint8_t type = 0;
        for (;;) {
            const auto status = drain.pop(type, payload);
            require(status != protocol::FrameParseStatus::Invalid, "valid server frame stream");
            if (status == protocol::FrameParseStatus::Incomplete) {
                break;
            }
            const auto opcode = static_cast<protocol::S2C>(type);
            if (opcode == protocol::S2C::Code) {
                protocol::S2CCodeFields code;
                require(protocol::Handshake::parse_code(payload.data(), payload.size(), code),
                        "parse handshake challenge");
                send(peer, protocol::Handshake::build_answer(code.protocol_test_value));
            } else if (opcode == protocol::S2C::CreatePlayer) {
                protocol::CreatePlayerFields player;
                require(protocol::CreatePlayer::parse(payload.data(), payload.size(), player),
                        "parse player presentation");
                if (player.class_byte & 0x80) {
                    peer.x = player.x;
                    peer.y = player.y;
                    peer.self = std::move(player);
                }
            } else if (opcode == protocol::S2C::ClockSync) {
                protocol::ClockSyncFields clock;
                peer.ready = protocol::ClockSync::parse(payload.data(), payload.size(), clock) &&
                             clock.unix_seconds > 0 && peer.self.id != 0;
            } else if (opcode == protocol::S2C::Chatting) {
                std::string speaker, text;
                require(
                    protocol::Chatting::parse_s2c(payload.data(), payload.size(), speaker, text),
                    "parse chat barrier");
                if (text == peer.barrier && speaker == peer.self.name) {
                    peer.chatted = true;
                }
            } else if (opcode == protocol::S2C::MovePlayerEnd) {
                std::uint32_t id = 0;
                std::int8_t dx = 0, dy = 0, dz = 0;
                require(
                    protocol::Move::parse_player(payload.data(), payload.size(), id, dx, dy, dz),
                    "parse movement echo");
                if (id == peer.self.id) {
                    ++peer.own_moves;
                    peer.x += dx;
                    peer.y += dy;
                }
            } else if (opcode == protocol::S2C::TalentTree) {
                require(protocol::parse_talent_state(payload.data(), payload.size(), peer.talents),
                        "parse talent response");
                ++peer.talent_replies;
            } else if (opcode == protocol::S2C::Property ||
                       opcode == protocol::S2C::UpdateProperty) {
                ++peer.progression_frames;
            } else if (opcode == protocol::S2C::CreateItem) {
                protocol::PacketReader reader(payload);
                std::uint16_t index = 0;
                std::uint32_t id = 0, x = 0, y = 0, count = 0;
                require(reader.read_u16(index) && reader.read_u32(id) && reader.read_u32(x) &&
                            reader.read_u32(y) && reader.read_u32(count),
                        "parse ground item identity and authoritative position");
                require(x <= 524287 && y <= 524287 && count > 0,
                        "owned ground item has bounded coordinates and positive quantity");
                peer.ground.insert(id);
                peer.ground_positions[id] = {static_cast<int>(x), static_cast<int>(y)};
            } else if (opcode == protocol::S2C::RemoveItem) {
                protocol::PacketReader reader(payload);
                std::uint32_t id = 0;
                require(reader.read_u32(id), "parse removed ground item");
                peer.ground.erase(id);
                peer.ground_positions.erase(id);
                peer.removed.insert(id);
            } else if (opcode == protocol::S2C::InsertItem) {
                protocol::PacketReader reader(payload);
                std::uint32_t id = 0, info = 0, count = 0;
                std::uint16_t index = 0;
                std::uint8_t prefix = 0;
                require(reader.read_u32(id) && reader.read_u16(index) && reader.read_u8(prefix) &&
                            reader.read_u32(info) && reader.read_u32(count),
                        "parse inventory identity and quantity");
                if (id == 0) {
                    peer.money = count;
                } else {
                    peer.inventory[id] = {index, static_cast<int>(count)};
                }
            } else if (opcode == protocol::S2C::Action) {
                std::uint32_t id = 0;
                std::uint8_t action = 0, argument = 0;
                require(protocol::Skill::parse_action(
                            payload.data(), payload.size(), id, action, argument),
                        "parse entity action");
                if (action == protocol::kAtDie) {
                    peer.dead_entities.insert(id);
                }
            } else if (opcode == protocol::S2C::Teleport) {
                std::uint8_t map = 0, cheat = 0;
                int z = 0;
                require(protocol::Teleport::parse_server(
                            payload.data(), payload.size(), map, peer.x, peer.y, z, cheat),
                        "parse portal destination");
                peer.map = map;
                ++peer.teleports;
                send(peer, protocol::Teleport::frame_client_ack(1, z));
            } else if (opcode == protocol::S2C::MoveBefore) {
                std::uint32_t id = 0;
                int x = 0, y = 0, z = 0;
                std::uint16_t direction = 0;
                require(protocol::Move::parse_before(
                            payload.data(), payload.size(), id, x, y, z, direction),
                        "parse acknowledged position");
                if (id == peer.self.id) {
                    peer.x = x;
                    peer.y = y;
                    ++peer.position_snapshots;
                }
            } else if (opcode == protocol::S2C::AnsGameExit) {
                require(protocol::GameExit::parse_server_answer(
                            payload.data(), payload.size(), peer.saved),
                        "parse durable exit acknowledgment");
            }
        }
        peer.closed = !connected;
    }

    template <typename Predicate>
    void advance(std::vector<Peer>& peers,
                 std::size_t readable,
                 Predicate done,
                 const std::string& label,
                 std::chrono::milliseconds timeout = std::chrono::seconds(8)) {
        const auto deadline = Clock::now() + timeout;
        do {
            // Every healthy peer gets one full available-byte drain on every turn.
            for (std::size_t i = 0; i < readable; ++i) {
                receive(peers[i]);
            }
            if (done()) {
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        } while (Clock::now() < deadline);
        throw std::runtime_error("watchdog expired: " + label);
    }

    template <typename Predicate> void wait_host(Predicate done, const std::string& label) {
        const auto deadline = Clock::now() + std::chrono::seconds(8);
        while (!done()) {
            require(Clock::now() < deadline, "watchdog expired: " + label);
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }

    std::unique_ptr<server::ISession>
    connect(Fixture& fixture, bool postgres, int character_id, bool slow_reader = false) {
        if (!postgres) {
            return server::connect_unix(fixture.config.unix_path);
        }
        server::TestControlClient login;
        require(login.connect("127.0.0.1",
                              fixture.config.tcp_port,
                              fixture.config.test_control_token,
                              server::tls_config_from_environment(),
                              slow_reader ? 4096 : 0),
                "connect TLS identity session");
        const auto request =
            Json::parse("{\"op\":\"login\",\"id\":" + std::to_string(character_id) + "}");
        const auto response = login.request(request);
        require(response.at("error").as_string().empty(),
                "authenticate owned independent identity: " + response.at("error").as_string());
        auto session = login.release_session();
        if (slow_reader) {
            tcp_info info{};
            socklen_t size = sizeof(info);
            require(::getsockopt(session->fd(), IPPROTO_TCP, TCP_INFO, &info, &size) == 0 &&
                        info.tcpi_rcv_wscale == 0,
                    "slow reader negotiates a bounded unscaled TCP receive window");
        }
        return session;
    }

    void seed(Fixture& fixture, int id) {
        auto hero = fixture.store->load_character(id);
        require(hero.has_value(), "load owned character");
        hero->level = 40;
        hero->str = hero->dex = 100;
        hero->hth = hero->wis = 50;
        hero->hp = hero->max_hp = 1000;
        hero->mp = hero->max_mp = 500;
        hero->money = 100;
        hero->map = 0;
        hero->map_x = hero->map_y = 1024;
        hero->weapon_index = 1;
        require(fixture.store->save_character(*hero), "seed bounded owned combat attributes");
        server::ItemRecord sword;
        sword.character_id = id;
        sword.iid = id * 100 + 1;
        sword.index = 1;
        sword.count = 1;
        sword.slot = 1;
        require(fixture.store->insert_item(sword), "seed owned weapon IID");
        auto token = sword;
        token.iid++;
        token.index = 2;
        token.slot = 0;
        require(fixture.store->insert_item(token), "seed owned bag IID");
    }

    void start_peers(Fixture& fixture,
                     server::ServerHost& host,
                     const Options& opts,
                     std::vector<Peer>& peers) {
        std::vector<int> identities;
        if (opts.postgres) {
            server::TestControlClient control;
            wait_host(
                [&] {
                    return control.connect("127.0.0.1",
                                           opts.port,
                                           fixture.config.test_control_token,
                                           server::tls_config_from_environment());
                },
                "TLS listener ready");
            for (int i = 1; i <= opts.clients; ++i) {
                const auto request =
                    Json::parse("{\"op\":\"ensure\",\"ordinal\":" + std::to_string(i) + "}");
                const auto response = control.request(request);
                const int id = response.at("id").as_int();
                require(id > 0 && response.at("error").as_string().empty(),
                        "provision owned account and character");
                identities.push_back(id);
                seed(fixture, id);
            }
            // The 64-slot limit includes admin and retiring sockets. Release and await cleanup.
            auto admin_session = control.release_session();
            admin_session->close();
            admin_session.reset();
            wait_host(
                [&] {
                    const auto m = host.runtime_metrics_snapshot();
                    return m.completed_loops && m.active_connections == 0 &&
                           m.retiring_connections == 0;
                },
                "admin slot released");
        } else {
            identities.assign(static_cast<std::size_t>(opts.clients), fixture.config.character_id);
            wait_host([&] { return std::filesystem::exists(fixture.config.unix_path); },
                      "Unix listener ready");
        }
        peers.reserve(static_cast<std::size_t>(opts.clients));
        for (const int id : identities) {
            Peer peer;
            peer.character_id = id;
            peer.session = connect(fixture,
                                   opts.postgres,
                                   id,
                                   identities.size() > 1 && peers.size() + 1 == identities.size());
            require(peer.session != nullptr, "connect gameplay session");
            send(peer, protocol::Handshake::build_connect(0));
            peers.push_back(std::move(peer));
            // Drain earlier peers during each handshake, including the future slow reader.
            advance(peers, peers.size(), [&] { return peers.back().ready; }, "gameplay handshake");
        }
        std::set<std::uint32_t> entities;
        for (const auto& peer : peers) {
            entities.insert(peer.self.id);
        }
        require(entities.size() == peers.size(), "all runtime entity IDs are independent");
        if (opts.postgres) {
            const std::set<int> ids(identities.begin(), identities.end());
            require(ids.size() == peers.size(),
                    "PostgreSQL profiles use distinct character records");
        }
        wait_host(
            [&] { return host.runtime_metrics_snapshot().active_connections == peers.size(); },
            "all gameplay connections accepted");
    }

    double round(std::vector<Peer>& peers, std::size_t healthy, int ordinal, bool chat = false) {
        std::vector<std::uint64_t> moves, talents;
        const auto started = Clock::now();
        for (std::size_t i = 0; i < healthy; ++i) {
            auto& peer = peers[i];
            moves.push_back(peer.own_moves);
            talents.push_back(peer.talent_replies);
            peer.barrier = "owned-load-" + std::to_string(ordinal) + "-" + std::to_string(i);
            peer.chatted = false;
            Bytes batch;
            const auto append = [&](const Bytes& frame) {
                batch.insert(batch.end(), frame.begin(), frame.end());
            };
            append(protocol::Move::frame_client_end(ordinal % 2 ? -1 : 1, 0, 0));
            append(protocol::frame_talent_request(protocol::TalentCommand::Snapshot));
            if (chat) {
                append(protocol::Chatting::frame_c2s(peer.barrier));
            }
            send(peer, batch);
        }
        advance(
            peers,
            healthy,
            [&] {
                for (std::size_t i = 0; i < healthy; ++i) {
                    require(!peers[i].closed, "healthy peer stays connected");
                    if ((chat && !peers[i].chatted) || peers[i].own_moves <= moves[i] ||
                        peers[i].talent_replies <= talents[i]) {
                        return false;
                    }
                }
                return true;
            },
            "every healthy peer movement/talent FIFO barrier and optional first chat");
        return std::chrono::duration<double, std::milli>(Clock::now() - started).count();
    }

    Bytes pickup(std::uint32_t id) {
        protocol::PacketWriter writer;
        writer.write_u32(id);
        writer.write_u32(0);
        writer.write_u32(0);
        return writer.frame(static_cast<std::uint8_t>(protocol::C2S::PickupItem));
    }

    void duplicate_pickup(Fixture& fixture, std::vector<Peer>& peers) {
        if (peers.size() < 2) {
            return;
        }
        const auto items = fixture.store->load_items(peers[0].character_id);
        const auto found = std::find_if(items.begin(), items.end(), [](const auto& item) {
            return item.index == 2 && item.slot == 0;
        });
        require(found != items.end(), "owned race token exists");
        const auto id = static_cast<std::uint32_t>(found->iid);
        send(peers[0], protocol::DropItem::frame_c2s(id, 1));
        advance(
            peers,
            peers.size(),
            [&] { return peers[0].ground.count(id) && peers[1].ground.count(id); },
            "both independent players see dropped token");
        send(peers[0], pickup(id));
        send(peers[1], pickup(id));
        advance(
            peers,
            peers.size(),
            [&] { return peers[0].removed.count(id) && peers[1].removed.count(id); },
            "racing pickup removes one world identity");
        (void) round(peers, peers.size(), 8000);
        int tokens = 0;
        std::set<int> iids;
        for (const auto& peer : peers) {
            for (const auto& item : fixture.store->load_items(peer.character_id)) {
                require(iids.insert(item.iid).second,
                        "persistent inventory IIDs stay globally unique");
                if (item.index == 2) {
                    tokens += item.count;
                }
            }
        }
        require(tokens == static_cast<int>(peers.size()),
                "duplicate pickup conserves total quantity");
    }

    void pressure(server::ServerHost& host, std::vector<Peer>& peers) {
        if (peers.size() == 1) {
            return;
        }
        auto& slow = peers.back();
        int receive_buffer = 4096;
        require(::setsockopt(slow.session->fd(),
                             SOL_SOCKET,
                             SO_RCVBUF,
                             &receive_buffer,
                             sizeof(receive_buffer)) == 0,
                "bound slow-reader receive buffer");
        const auto request = protocol::frame_talent_request(protocol::TalentCommand::Snapshot);
        std::size_t burst_requests = 128;
        constexpr std::size_t kMaxBurstRequests = 2048;
        require(request.size() * kMaxBurstRequests < 32768,
                "pressure burst stays below the server receive bound");
        const auto ticks_before = host.runtime_metrics_snapshot().scheduled_ticks;
        // Filling TCP/TLS buffers depends on the host. Once output stays queued,
        // the one-second server deadline still has the separate three-second watchdog below.
        const auto fill_deadline = Clock::now() + std::chrono::seconds(20);
        bool witnessed = false;
        bool filling = true;
        std::uint64_t candidate_loop = 0;
        std::uint64_t candidate_tick = 0;
        Clock::time_point blocked_since{};
        int barriers = 0;
        int transient_backlogs = 0;
        int fill_bursts = 0;
        const auto diagnostics = [&] {
            const auto state = host.runtime_metrics_snapshot();
            return " (healthy_rounds=" + std::to_string(barriers) +
                   ", fill_bursts=" + std::to_string(fill_bursts) +
                   ", transient_backlogs=" + std::to_string(transient_backlogs) +
                   ", pending_bytes=" + std::to_string(state.observed_pending_output_bytes) +
                   ", active_connections=" + std::to_string(state.active_connections) + ")";
        };
        while (Clock::now() < fill_deadline || candidate_loop != 0) {
            const auto state = host.runtime_metrics_snapshot();
            if (state.active_connections < peers.size()) {
                break;
            }
            // Stop feeding once a real queue is sampled; distinguish a transient flush backlog
            // from output which remains queued while every reading peer completes another turn.
            if (filling) {
                Bytes batch;
                batch.reserve(request.size() * burst_requests);
                for (std::size_t i = 0; i < burst_requests; ++i) {
                    batch.insert(batch.end(), request.begin(), request.end());
                }
                if (!slow.session->send_all(batch)) {
                    break;
                }
                ++fill_bursts;
            }
            (void) round(peers, peers.size() - 1, 9000 + barriers++);
            const auto quiet_since = Clock::now();
            const auto quiet_loop = host.runtime_metrics_snapshot().completed_loops;
            // A busy population's newly queued healthy replies are not slow-reader pressure.
            // Drain them without sending again before inspecting aggregate queue observations.
            advance(
                peers,
                peers.size() - 1,
                [&] {
                    return host.runtime_metrics_snapshot().completed_loops >= quiet_loop + 2 &&
                           Clock::now() - quiet_since >= std::chrono::milliseconds(25);
                },
                "quiet drain separates healthy replies from slow-reader pressure");
            const auto sampled = host.runtime_metrics_snapshot();
            if (sampled.active_connections < peers.size()) {
                break;
            }
            if (sampled.observed_pending_output_bytes == 0) {
                if (candidate_loop) {
                    ++transient_backlogs;
                }
                burst_requests = std::min(kMaxBurstRequests, burst_requests * 2);
                filling = true;
                witnessed = false;
                candidate_loop = 0;
                blocked_since = {};
            } else if (candidate_loop && sampled.completed_loops > candidate_loop) {
                witnessed =
                    witnessed || (Clock::now() - blocked_since >= std::chrono::milliseconds(100) &&
                                  sampled.scheduled_ticks > candidate_tick);
                require(Clock::now() - blocked_since < std::chrono::seconds(3),
                        "one-second output deadline disconnects within coarse watchdog slack" +
                            diagnostics());
            } else if (sampled.observed_pending_output_bytes > 0) {
                filling = false;
                candidate_loop = sampled.completed_loops;
                candidate_tick = sampled.scheduled_ticks;
                blocked_since = Clock::now();
            }
        }
        require(witnessed,
                "establish sustained queued output before checking slow-peer disconnection" +
                    diagnostics());
        advance(
            peers,
            peers.size() - 1,
            [&] { return host.runtime_metrics_snapshot().active_connections == peers.size() - 1; },
            "slow peer disconnects after bounded queue pressure" + diagnostics(),
            std::chrono::seconds(3));
        const auto state = host.runtime_metrics_snapshot();
        require(state.scheduled_ticks > ticks_before && barriers > 1,
                "world ticks and every healthy client advance during pressure");
        require(state.peak_observed_peer_pending_output_bytes <= 1024 * 1024,
                "sampled per-peer pending bytes respect the queue bound");
        std::cerr << "pressure sustained_without_slow_input_ms="
                  << std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() -
                                                                           blocked_since)
                         .count()
                  << " healthy_rounds=" << barriers << " fill_bursts=" << fill_bursts
                  << " largest_burst_requests=" << burst_requests
                  << " drained_transient_backlogs=" << transient_backlogs
                  << " tick_delta=" << state.scheduled_ticks - ticks_before
                  << " observed_peer_peak_lower_bound="
                  << state.peak_observed_peer_pending_output_bytes << '\n';
        slow.session->close();
        slow.closed = true;
        (void) round(peers, peers.size() - 1, 9999);
    }

    void close_background(server::ServerHost& host, std::vector<Peer>& peers) {
        for (std::size_t i = 1; i < peers.size(); ++i) {
            if (!peers[i].closed) {
                send(peers[i], protocol::GameExit::frame_client_request());
            }
        }
        advance(
            peers,
            peers.size(),
            [&] {
                return std::all_of(peers.begin() + 1, peers.end(), [](const auto& peer) {
                    return peer.closed || peer.saved;
                });
            },
            "background durable exits");
        for (std::size_t i = 1; i < peers.size(); ++i) {
            peers[i].session->close();
            peers[i].closed = true;
        }
        wait_host(
            [&] {
                const auto state = host.runtime_metrics_snapshot();
                return state.active_connections == 1 && state.retiring_connections == 0;
            },
            "background saves finish before final SQLite actor save");
    }

    struct ActorBaseline {
        server::CharacterRecord character;
        int tokens = 0;
    };

    int token_count(const std::vector<server::ItemRecord>& rows) {
        int count = 0;
        for (const auto& item : rows) {
            if (item.index == 2) {
                count += item.count;
            }
        }
        return count;
    }

    void move_actor_to(std::vector<Peer>& peers, int x, int y) {
        auto& actor = peers.front();
        int steps = 0;
        while (actor.x != x || actor.y != y) {
            require(++steps <= 100, "owned point can be approached with bounded ordinary movement");
            const auto next_step = Clock::now() + std::chrono::milliseconds(100);
            const int next_x = actor.x + std::clamp(x - actor.x, -4, 4);
            const int next_y = actor.y + std::clamp(y - actor.y, -4, 4);
            const auto previous_moves = actor.own_moves;
            send(actor,
                 protocol::Move::frame_client_end(static_cast<std::int8_t>(next_x - actor.x),
                                                  static_cast<std::int8_t>(next_y - actor.y),
                                                  0));
            advance(
                peers,
                peers.size(),
                [&] {
                    return actor.own_moves > previous_moves && actor.x == next_x &&
                           actor.y == next_y;
                },
                "actor approaches an owned point while every peer drains");
            std::this_thread::sleep_until(next_step);
        }
    }

    std::vector<std::array<std::int64_t, 30>> item_rows(Fixture& fixture, int owner) {
        std::vector<std::array<std::int64_t, 30>> rows;
        for (const auto& item : fixture.store->load_items(owner)) {
            rows.push_back({item.iid,
                            item.character_id,
                            item.account_id,
                            item.index,
                            item.prefix,
                            item.count,
                            item.slot,
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
                            item.character_bound});
        }
        std::sort(rows.begin(), rows.end());
        return rows;
    }

    ActorBaseline pve(Fixture& fixture, server::ServerHost& host, std::vector<Peer>& peers) {
        auto& actor = peers.front();
        const auto monsters = host.monster_snapshot();
        require(!monsters.empty(), "owned PvE target spawned");
        const auto target = monsters.front().id;
        const auto before = fixture.store->load_character(actor.character_id);
        require(before.has_value(), "load actor before PvE transaction");
        const ActorBaseline baseline{*before,
                                     token_count(fixture.store->load_items(actor.character_id))};
        const auto deadline = Clock::now() + std::chrono::seconds(10);
        while (Clock::now() < deadline && actor.dead_entities.count(target) == 0) {
            send(actor, protocol::Attack::frame_client(protocol::kCkMonster, target));
            const auto until = Clock::now() + std::chrono::milliseconds(200);
            do {
                for (auto& peer : peers) {
                    receive(peer);
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            } while (Clock::now() < until && actor.dead_entities.count(target) == 0);
        }
        require(actor.dead_entities.count(target) == 1,
                "ordinary attack produces witnessed monster death");
        advance(
            peers,
            peers.size(),
            [&] { return actor.ground.size() >= 2; },
            "owned item and money drops");
        (void) round(peers, peers.size(), 11000);
        const auto drops = actor.ground;
        const auto negative_id = *drops.begin();
        const auto negative_position = actor.ground_positions.at(negative_id);
        // This owned area is walkable and lies left of the field portal at x=1152.
        move_actor_to(peers, negative_position.first - 96, negative_position.second);
        const auto rejected_money = fixture.store->load_character(actor.character_id);
        require(rejected_money.has_value(), "load money before rejected pickup");
        const auto rejected_rows = item_rows(fixture, actor.character_id);
        const auto rejected_wire_money = actor.money;
        const auto rejected_wire_items = actor.inventory;
        send(actor, pickup(negative_id));
        (void) round(peers, peers.size(), 11003);
        const auto after_rejection = fixture.store->load_character(actor.character_id);
        require(
            actor.ground.count(negative_id) == 1 && !actor.removed.count(negative_id) &&
                after_rejection && after_rejection->money == rejected_money->money &&
                item_rows(fixture, actor.character_id) == rejected_rows &&
                actor.money == rejected_wire_money && actor.inventory == rejected_wire_items,
            "96-pixel pickup rejection preserves exact world ID, money and all owned item fields");
        std::cerr << "pickup rejected_at_distance=96 id=" << negative_id
                  << " unchanged_persistent_rows=" << rejected_rows.size() << '\n';
        for (const auto id : drops) {
            const auto position = actor.ground_positions.at(id);
            const auto dx = static_cast<long long>(actor.x) - position.first;
            const auto dy = static_cast<long long>(actor.y) - position.second;
            std::cerr << "pickup id=" << id << " actor=(" << actor.x << ',' << actor.y << ") drop=("
                      << position.first << ',' << position.second
                      << ") original_distance_squared=" << dx * dx + dy * dy << '\n';
            // Loot scatter can put a valid observed drop outside the 64-pixel pickup radius.
            // Approach the wire position instead of assuming the corpse remained in range.
            move_actor_to(peers, position.first, position.second);
            send(actor, pickup(id));
            advance(
                peers,
                peers.size(),
                [&] { return actor.removed.count(id) == 1 && actor.ground.count(id) == 0; },
                "ordinary pickup removes the exact approached world item");
        }
        advance(
            peers,
            peers.size(),
            [&] { return actor.ground.empty(); },
            "ordinary pickup removes all owned drops");
        (void) round(peers, peers.size(), 11001);
        const auto replies = actor.talent_replies;
        const auto progression_before = actor.progression_frames;
        send(actor, protocol::frame_talent_request(protocol::TalentCommand::Apply, 0, 1));
        advance(
            peers,
            peers.size(),
            [&] { return actor.talent_replies > replies && actor.talents.mask == 1; },
            "valid talent purchase returns authoritative state");
        require(actor.progression_frames > progression_before,
                "talent purchase produces a fresh progression snapshot");
        const auto stale_replies = actor.talent_replies;
        send(actor, protocol::frame_talent_request(protocol::TalentCommand::Apply, 0, 1));
        advance(
            peers,
            peers.size(),
            [&] { return actor.talent_replies > stale_replies; },
            "stale request response");
        require(actor.talents.status == protocol::TalentStatus::Stale && actor.talents.mask == 1,
                "stale purchase does not change committed talents");
        // A well-framed truncated request must not change state or desynchronize the next request.
        protocol::PacketWriter malformed;
        malformed.write_u8(static_cast<std::uint8_t>(protocol::TalentCommand::Apply));
        send(actor, malformed.frame(static_cast<std::uint8_t>(protocol::C2S::TalentTree)));
        const auto after_malformed = actor.talent_replies;
        send(actor, protocol::frame_talent_request(protocol::TalentCommand::Snapshot));
        advance(
            peers,
            peers.size(),
            [&] { return actor.talent_replies > after_malformed && actor.talents.mask == 1; },
            "valid request after truncated payload");
        (void) round(peers, peers.size(), 11002);
        return baseline;
    }

    void portal_and_reconnect(Fixture& fixture,
                              server::ServerHost& host,
                              std::vector<Peer>& peers,
                              const ActorBaseline& baseline) {
        auto& actor = peers.front();
        // Move to a one-tile field portal; no administrative teleport is used.
        int delta_x = 36 * 32 - actor.x;
        int delta_y = 32 * 32 - actor.y;
        while (delta_x != 0 || delta_y != 0) {
            const auto next_step = Clock::now() + std::chrono::milliseconds(100);
            const int step_x = std::clamp(delta_x, -4, 4);
            const int step_y = std::clamp(delta_y, -4, 4);
            send(actor,
                 protocol::Move::frame_client_end(
                     static_cast<std::int8_t>(step_x), static_cast<std::int8_t>(step_y), 0));
            delta_x -= step_x;
            delta_y -= step_y;
            std::this_thread::sleep_until(next_step);
        }
        advance(
            peers,
            1,
            [&] {
                return actor.teleports > 0 && actor.map == 9 && actor.x == 1536 && actor.y == 1536;
            },
            "field portal and client acknowledgment");
        (void) round(peers, 1, 10000);
        send(actor, protocol::GameExit::frame_client_request());
        advance(peers, 1, [&] { return actor.saved; }, "actor durable exit barrier");
        actor.session->close();
        wait_host(
            [&] {
                const auto m = host.runtime_metrics_snapshot();
                return m.active_connections == 0 && m.retiring_connections == 0;
            },
            "all session persistence and retiring output complete");
        const auto record = fixture.store->load_character(actor.character_id);
        require(record && record->map == 9 && record->map_x == actor.x &&
                    record->map_y == actor.y && record->talent_mask == 1 &&
                    record->money == baseline.character.money + 17,
                "durable barrier preserves pose, money and talent state");
        const auto items = fixture.store->load_items(actor.character_id);
        require(token_count(items) == baseline.tokens + 1,
                "durable inventory receives exactly one generated PvE token");
        std::map<std::uint32_t, std::pair<int, int>> saved_items;
        for (const auto& item : items) {
            require(saved_items.emplace(item.iid, std::make_pair(item.index, item.count)).second,
                    "durable IID rows are unique");
        }
        Peer returning;
        returning.character_id = actor.character_id;
        returning.session = connect(fixture, fixture.config.multiplayer, actor.character_id);
        require(returning.session != nullptr, "reconnect after graceful durable exit");
        send(returning, protocol::Handshake::build_connect(0));
        peers.clear();
        peers.push_back(std::move(returning));
        advance(peers, 1, [&] { return peers[0].ready; }, "reconnected gameplay handshake");
        (void) round(peers, 1, 10002, true);
        require(peers[0].talents.mask == 1, "reconnect restores authoritative committed talents");
        require(peers[0].money == record->money, "reconnect wire money matches durable balance");
        require(peers[0].self.x == record->map_x && peers[0].self.y == record->map_y,
                "reconnect self presentation matches durable pose");
        require(peers[0].inventory == saved_items,
                "reconnect wire inventory preserves every IID and count");
        send(peers[0], protocol::GameExit::frame_client_request());
        advance(peers, 1, [&] { return peers[0].saved; }, "reconnected durable exit");
        peers[0].session->close();
    }

    double percentile(std::vector<double> samples, double fraction) {
        if (samples.empty()) {
            return 0;
        }
        std::sort(samples.begin(), samples.end());
        return samples[static_cast<std::size_t>(std::ceil(fraction * samples.size())) - 1];
    }

    void report(const Options& opts,
                const server::HostRuntimeMetricsSnapshot& metrics,
                const std::vector<double>& barriers) {
        std::vector<double> ticks, lateness, dispatch;
        for (const auto& tick : metrics.ticks) {
            ticks.push_back(static_cast<double>(tick.duration_ns) / 1e6);
            lateness.push_back(static_cast<double>(tick.lateness_ns) / 1e6);
        }
        for (const auto value : metrics.dispatch_work_ns) {
            dispatch.push_back(static_cast<double>(value) / 1e6);
        }
        rusage usage{};
        require(::getrusage(RUSAGE_SELF, &usage) == 0, "sample combined process RSS");
        std::ostringstream output;
        output << "{\"backend\":\""
               << (opts.postgres ? "postgres_tls_unique_identities" : "sqlite_shared_character")
               << "\",\"clients_including_slow\":" << opts.clients
               << ",\"healthy_during_pressure\":" << (opts.clients == 1 ? 1 : opts.clients - 1)
               << ",\"barrier_scope\":\"all_peer_round_completion_not_individual_latency\""
               << ",\"host_sample_scope\":\"whole_scenario_latest_bounded_window\""
               << ",\"warmup_scope\":\"barrier_rounds_only\""
               << ",\"warmup_rounds\":" << opts.warmup << ",\"barrier_samples\":" << barriers.size()
               << ",\"barrier_ms\":{\"p50\":" << percentile(barriers, .50)
               << ",\"p95\":" << percentile(barriers, .95)
               << ",\"p99\":" << percentile(barriers, .99)
               << "},\"tick_ms\":{\"p50\":" << percentile(ticks, .50)
               << ",\"p95\":" << percentile(ticks, .95) << ",\"p99\":" << percentile(ticks, .99)
               << "},\"dispatch_ms\":{\"p50\":" << percentile(dispatch, .50)
               << ",\"p95\":" << percentile(dispatch, .95)
               << ",\"p99\":" << percentile(dispatch, .99)
               << "},\"tick_lateness_ms_p99\":" << percentile(lateness, .99)
               << ",\"peak_connections\":" << metrics.peak_connections
               << ",\"completed_loops\":" << metrics.completed_loops
               << ",\"scheduled_ticks\":" << metrics.scheduled_ticks
               << ",\"tick_overruns\":" << metrics.tick_overruns
               << ",\"retained_tick_samples\":" << metrics.ticks.size()
               << ",\"retained_dispatch_samples\":" << metrics.dispatch_work_ns.size()
               << ",\"observed_queue_peak_bytes_lower_bound\":"
               << metrics.peak_observed_pending_output_bytes
               << ",\"observed_peer_queue_peak_bytes_lower_bound\":"
               << metrics.peak_observed_peer_pending_output_bytes
               << ",\"combined_process_peak_rss_kib\":" << usage.ru_maxrss << "}";
        std::cout << output.str() << '\n';
        if (!opts.report.empty()) {
            std::ofstream file(opts.report);
            file << output.str() << '\n';
            require(static_cast<bool>(file), "write measurement report");
        }
    }
}  // namespace

int main(int argc, char** argv) {
    // Independent final safety net for unexpected blocking teardown or client transport calls.
    ::alarm(150);
    try {
        const auto opts = options(argc, argv);
        if (opts.postgres) {
#if !defined(OPENKAL_LOAD_POSTGRES)
            return 77;
#else
            if (!std::getenv("OPENKAL_TEST_POSTGRES_DSN") || !opts.port ||
                !std::getenv("OPENKAL_TLS_CERT") || !std::getenv("OPENKAL_TLS_KEY") ||
                !std::getenv("OPENKAL_TLS_CA")) {
                return 77;
            }
#endif
        }
        Fixture fixture(opts);
        if (!opts.postgres) {
            server::CharacterRecord hero;
            hero.name = "OwnedLoadActor";
            hero.map_x = hero.map_y = 1024;
            require(fixture.store->ensure_default_player("owned-load", "owned-load", hero),
                    "create one shared SQLite smoke character");
            const auto account = fixture.store->find_account("owned-load");
            require(account.has_value(), "load SQLite fixture account");
            const auto characters = fixture.store->list_characters(account->id);
            require(characters.size() == 1, "one SQLite fixture character");
            fixture.config.character_id = characters.front().id;
            seed(fixture, characters.front().id);
        }
        server::ServerHost host;
        require(!host.runtime_metrics_snapshot().enabled, "metrics disabled by default");
        require(host.set_runtime_metrics_enabled(true), "enable metrics before start");
        require(host.start(fixture.config), "start owned load-test host");
        require(!host.set_runtime_metrics_enabled(false), "reject live metrics reconfiguration");
        std::vector<Peer> peers;
        start_peers(fixture, host, opts, peers);
        std::vector<double> barriers;
        for (int i = 0; i < opts.warmup + opts.rounds; ++i) {
            const auto latency = round(peers, peers.size(), i, i == 0);
            if (i >= opts.warmup) {
                barriers.push_back(latency);
            }
        }
        if (opts.postgres) {
            duplicate_pickup(fixture, peers);
        }
        const auto baseline = pve(fixture, host, peers);
        pressure(host, peers);
        close_background(host, peers);
        portal_and_reconnect(fixture, host, peers, baseline);
        const auto stopping = Clock::now();
        host.stop();
        require(Clock::now() - stopping < std::chrono::seconds(5), "bounded clean host shutdown");
        const auto metrics = host.runtime_metrics_snapshot();
        require(metrics.enabled && metrics.scheduled_ticks > 0 && metrics.completed_loops > 0,
                "completed metrics survive host shutdown");
        require(metrics.peak_connections <= 64, "accepted plus retiring connections stay bounded");
        report(opts, metrics, barriers);
        require(host.set_runtime_metrics_enabled(true) &&
                    host.runtime_metrics_snapshot().completed_loops == 0,
                "re-enabling stopped recorder resets measurements");
        require(host.set_runtime_metrics_enabled(false) && !host.runtime_metrics_snapshot().enabled,
                "disable stopped recorder");
        ::alarm(0);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Multiplayer load test failed: " << error.what() << '\n';
        return 1;
    }
}
