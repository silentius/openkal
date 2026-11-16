/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "TestPlayerBackend.h"
#include "fixture_util.h"
#include "openkal/protocol/CreatePlayer.h"
#include "openkal/protocol/FrameParser.h"
#include "openkal/protocol/Handshake.h"
#include "openkal/protocol/Move.h"
#include "openkal/protocol/PacketReader.h"
#include "openkal/server/LobbyClient.h"
#include "openkal/server/ServerHost.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <libpq-fe.h>
#include <set>
#include <stdexcept>
#include <thread>
#include <unistd.h>

using namespace openkal;
using J = platform::Json;
namespace {
    void require(bool condition, const char* text) {
        if (!condition) {
            throw std::runtime_error(text);
        }
        std::cout << "PASS " << text << '\n';
    }
    bool wait(const std::function<bool()>& done, int seconds = 30) {
        const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
        while (std::chrono::steady_clock::now() < end) {
            QCoreApplication::processEvents();
            if (done()) {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return false;
    }
    struct Viewer {
        std::unique_ptr<server::ISession> connection;
        std::vector<std::uint8_t> rx;
        std::set<std::uint32_t> monsters;
        int removed_monsters = 0;
        template <class Predicate> bool poll(Predicate predicate) {
            if (!connection->recv_into(rx)) {
                throw std::runtime_error("Viewer disconnected.");
            }
            for (;;) {
                std::size_t consumed = 0;
                std::uint8_t type = 0;
                std::vector<std::uint8_t> payload;
                const auto status =
                    protocol::FrameParser::pop(rx.data(), rx.size(), consumed, type, payload);
                if (status == protocol::FrameParseStatus::Incomplete) {
                    return false;
                }
                if (status == protocol::FrameParseStatus::Invalid) {
                    throw std::runtime_error("Malformed viewer packet.");
                }
                rx.erase(rx.begin(), rx.begin() + static_cast<std::ptrdiff_t>(consumed));
                protocol::PacketReader reader(payload);
                std::uint32_t id = 0;
                if (type == static_cast<std::uint8_t>(protocol::S2C::CreateMonster)) {
                    std::uint16_t definition = 0;
                    if (!reader.read_u16(definition) || !reader.read_u32(id)) {
                        throw std::runtime_error("Malformed monster create packet.");
                    }
                    monsters.insert(id);
                } else if (type == static_cast<std::uint8_t>(protocol::S2C::RemoveMonster)) {
                    if (!reader.read_u32(id)) {
                        throw std::runtime_error("Malformed monster remove packet.");
                    }
                    monsters.erase(id);
                    ++removed_monsters;
                }
                if (predicate(type, payload)) {
                    return true;
                }
            }
        }
        template <class Predicate> bool receive(Predicate predicate) {
            return wait([&] { return poll(predicate); }, 15);
        }
        std::uint32_t enter(const std::string& endpoint, int id) {
            server::LobbyClient lobby;
            require(lobby.connect_local(endpoint) && lobby.login("managed", "managed"),
                    "attach local graphical lobby");
            const auto list = lobby.list_characters(lobby.account_id());
            require(list.size() == 1 && list.front().id == id && lobby.select(id).has_value(),
                    "bridge exposes the selected live character");
            connection = lobby.release_session();
            require(connection->send_all(protocol::Handshake::build_connect(0)),
                    "request live session snapshot");
            protocol::S2CCodeFields code{};
            require(receive([&](auto type, const auto& bytes) {
                        return type == static_cast<std::uint8_t>(protocol::S2C::Code) &&
                               protocol::Handshake::parse_code(bytes.data(), bytes.size(), code);
                    }),
                    "receive attachment challenge");
            require(
                connection->send_all(protocol::Handshake::build_answer(code.protocol_test_value)),
                "answer attachment challenge");
            std::uint32_t entity = 0;
            require(receive([&](auto type, const auto& bytes) {
                        protocol::CreatePlayerFields player;
                        if (type != static_cast<std::uint8_t>(protocol::S2C::CreatePlayer) ||
                            !protocol::CreatePlayer::parse(bytes.data(), bytes.size(), player) ||
                            !(player.class_byte & 0x80)) {
                            return false;
                        }
                        entity = player.id;
                        return true;
                    }),
                    "receive live self snapshot");
            return entity;
        }
    };
}  // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    std::cout << std::unitbuf;
    const char* base = std::getenv("OPENKAL_TEST_POSTGRES_DSN");
    if (!base || argc != 2) {
        return 77;
    }
    using Connection = std::unique_ptr<PGconn, decltype(&PQfinish)>;
    using Result = std::unique_ptr<PGresult, decltype(&PQclear)>;
    Connection admin(PQconnectdb(base), PQfinish);
    if (!admin || PQstatus(admin.get()) != CONNECTION_OK) {
        return 1;
    }
    PQsetNoticeProcessor(admin.get(), [](void*, const char*) {}, nullptr);
    const auto sql = [&](const std::string& text) {
        Result result(PQexec(admin.get(), text.c_str()), PQclear);
        return result && PQresultStatus(result.get()) == PGRES_COMMAND_OK;
    };
    const auto schema = "openkal_population_test_" + std::to_string(getpid());
    if (!sql("CREATE SCHEMA " + schema)) {
        return 1;
    }
    const auto root = test::temp_dir(("test-population-" + std::to_string(getpid())).c_str());
    int result = 0;
    try {
        for (const auto* dir : {"maps", "monsters", "items", "save"}) {
            std::filesystem::create_directories(root + "/" + dir);
        }
        require(test::write_open_area(root + "/maps/n_000_000.akarea"),
                "write owned walkable area");
        std::ofstream(root + "/monsters/1.json")
            << R"({"index":1,"name":"Owned Test Mob","stats":{"race":6,"level":1,"hp":10},"spawns":[
                {"map":0,"area":1,"max":2,"cycle":1,"rect":[30, 30, 34, 34]},
                {"map":0,"area":2,"max":2,"cycle":1,"rect":[123, 123, 127, 127]}
            ]})";
        std::ofstream(root + "/items/1.json")
            << R"({"index":1,"name":"Owned Test Sword","class":["weapon","sword"],"wear":1,"endurance":50})";
        std::ofstream(root + "/items/2.json")
            << R"({"index":2,"name":"Owned Test Stack","class":["general","etc"],"stackable":1})";
        std::ofstream(root + "/guilds.json") << R"({"schema":"openkal.guilds/v1"})";
        server::ServerConfig cfg;
        cfg.maps_dir = root + "/maps";
        cfg.monsters_dir = root + "/monsters";
        cfg.items_dir = root + "/items";
        cfg.save_dir = root + "/save";
        cfg.database_dsn = std::string(base) + " options='-csearch_path=" + schema + "'";
        cfg.store = server::StoreBackend::Postgres;
        cfg.multiplayer = true;
        cfg.guild_rules_path = root + "/guilds.json";
        cfg.transport = server::TransportKind::Tcp;
        cfg.tcp_host = "127.0.0.1";
        cfg.tcp_port = static_cast<std::uint16_t>(std::stoi(argv[1]));
        cfg.spawn_x = cfg.spawn_y = 1024;
        cfg.spawn_z = 100;
        cfg.max_monsters = 2;
        cfg.test_control_token = "OwnedTestControlSecret2026";
        server::ServerHost host;
        require(host.start(cfg), "start isolated PostgreSQL test server");
        server::TestControlClient denied;
        require(wait(
                    [&] {
                        return denied.connect("127.0.0.1",
                                              cfg.tcp_port,
                                              "wrong",
                                              server::tls_config_from_environment());
                    },
                    10),
                "connect unauthorized test peer");
        require(!denied.request(J::parse(R"({"op":"hello"})")).at("error").is_null(),
                "reject wrong test-control token");
        server::TestControlClient control;
        require(control.connect("127.0.0.1",
                                cfg.tcp_port,
                                cfg.test_control_token,
                                server::tls_config_from_environment()),
                "connect administrative test peer");
        auto call = [&](const J& command) {
            auto reply = control.request(command);
            if (!reply.at("error").as_string().empty()) {
                throw std::runtime_error(reply.at("error").as_string());
            }
            return reply;
        };
        const auto castle_status = call(J::parse(R"({"op":"castle_war","action":"status"})"));
        require(castle_status.at("available").type() == J::Type::Bool &&
                    !castle_status.at("available").boolean() &&
                    castle_status.at("start_blocked").as_string().find("castle_war_file") !=
                        std::string::npos,
                "report the missing Castle War configuration to test players");
        TestPlayerBackend backend;
        QJsonObject state;
        const auto guiThread = std::this_thread::get_id();
        bool wrongEventThread = false;
        int completed = 0;
        QString endpoint, lastMessage;
        QObject::connect(
            &backend, &TestPlayerBackend::snapshot, &app, [&](const QJsonObject& value) {
                wrongEventThread = wrongEventThread || std::this_thread::get_id() != guiThread;
                if (value.contains("players")) {
                    state = value;
                }
            });
        QObject::connect(&backend, &TestPlayerBackend::busyChanged, &app, [&](bool busy) {
            if (!busy) {
                ++completed;
            }
        });
        QObject::connect(&backend, &TestPlayerBackend::openClient, &app, [&](const QString& value) {
            endpoint = value;
        });
        QObject::connect(&backend, &TestPlayerBackend::message, &app, [&](const QString& value) {
            lastMessage = value;
            std::cout << "BACKEND " << value.toStdString() << '\n';
        });
        auto task = [&](QJsonObject command) {
            const int expected = completed + 1;
            backend.submit(std::move(command));
            if (!wait([&] { return completed >= expected; }, 90)) {
                throw std::runtime_error("Backend operation timed out: " +
                                         lastMessage.toStdString());
            }
        };
        task({{"op", "connect"},
              {"host", "127.0.0.1"},
              {"port", cfg.tcp_port},
              {"token", QString::fromStdString(cfg.test_control_token)},
              {"ca", QString::fromUtf8(std::getenv("OPENKAL_TLS_CA"))}});
        constexpr int kPlayerCount = 51;
        task({{"op", "provision"}, {"players", kPlayerCount}, {"guilds", 8}});
        if (state["players"].toArray().size() != kPlayerCount) {
            throw std::runtime_error("Provisioning failed: " + lastMessage.toStdString());
        }
        require(state["players"].toArray().size() == kPlayerCount &&
                    state["guilds"].toArray().size() == 8,
                "Qt backend creates fifty-one characters and eight guilds across roster pages");
        const auto players = state["players"].toArray();
        QJsonArray ids;
        for (const auto& player : players) {
            ids.append(player.toObject()["id"]);
        }
        const int first = ids.first().toInt();
        task({{"op", "provision"}, {"players", kPlayerCount}, {"guilds", 8}});
        require(state["players"].toArray().first().toObject()["id"].toInt() == first,
                "repeated provisioning reuses accounts and characters");
        task({{"op", "login"}, {"ids", ids}});
        require(wait(
                    [&] {
                        int online = 0;
                        for (const auto& player : state["players"].toArray()) {
                            if (player.toObject()["status"].toString() == "Online") {
                                ++online;
                            }
                        }
                        return online == kPlayerCount;
                    },
                    30),
                "fifty-one independent TLS clients enter and stay online");
        auto inspect = J::parse(R"({"op":"inspect"})");
        inspect.object()["id"].set_number(first);
        const int entity = call(inspect).at("entity").as_int();
        require(entity > 0, "inspect persistent world entity");
        auto ensureWeapon = inspect;
        ensureWeapon.object()["op"].string() = "ensure_weapon";
        require(!control.request(ensureWeapon).at("error").as_string().empty() &&
                    call(inspect).at("items").array().empty(),
                "missing starter configuration reports an error without inventing a weapon");
        for (const auto* operation : {"autonomous_city_travel", "autonomous_area_travel"}) {
            task({{"op", operation},
                  {"id", first},
                  {"city", 0},
                  {"area", 1},
                  {"map", 0},
                  {"x", 4000},
                  {"y", 4000},
                  {"z", 100}});
            const auto unchanged =
                server::read_test_character(call(inspect).at("record").as_string());
            require(unchanged && unchanged->map_x == 1024 && unchanged->map_y == 1024,
                    "unsolicited autonomous travel commands cannot move a player without a "
                    "matching pending request");
        }
        auto assignment = J::parse(R"({"op":"membership","rank":1,"guild":8})");
        assignment.object()["id"].set_number(first);
        require(call(assignment).at("error").is_null(),
                "move leader to later guild in a real PostgreSQL transaction");
        assignment.object()["guild"].set_number(1);
        require(call(assignment).at("error").is_null(),
                "move leader back to earlier guild without unique-key conflict");
        task({{"op", "give"}, {"ids", QJsonArray{first}}, {"item", 2}, {"count", 7}});
        require(call(inspect).at("items").array().size() == 1,
                "grant inventory on live authoritative session");
        task({{"op", "give"}, {"ids", QJsonArray{first}}, {"item", 1}, {"count", 1}});
        int sword = 0;
        const auto inventory = call(inspect);
        for (const auto& item : inventory.at("items").array()) {
            if (item.at("index").as_int() == 1) {
                sword = item.at("iid").as_int();
            }
        }
        require(sword > 0, "grant owned test weapon");
        task({{"op", "equip"}, {"ids", QJsonArray{first}}, {"iid", sword}});
        require(wait([&] {
                    const auto updatedInventory = call(inspect);
                    for (const auto& item : updatedInventory.at("items").array()) {
                        if (item.at("iid").as_int() == sword) {
                            return item.at("slot").as_int() == 1;
                        }
                    }
                    return false;
                }),
                "equip weapon through normal gameplay rules");
        task({{"op", "unequip"}, {"ids", QJsonArray{first}}, {"iid", sword}});
        require(wait([&] {
                    const auto updatedInventory = call(inspect);
                    for (const auto& item : updatedInventory.at("items").array()) {
                        if (item.at("iid").as_int() == sword) {
                            return item.at("slot").as_int() == 0;
                        }
                    }
                    return false;
                }),
                "unequip weapon through normal gameplay rules");
        task({{"op", "remove"}, {"ids", QJsonArray{first}}, {"iid", sword}});
        require(call(inspect).at("items").array().size() == 1,
                "remove unequipped inventory instance");

        task({{"op", "view"}, {"ids", QJsonArray{first}}});
        require(!endpoint.isEmpty(), "create owner-only local spectator endpoint");
        Viewer viewer;
        require(viewer.enter(endpoint.toStdString(), first) == static_cast<std::uint32_t>(entity),
                "opening the spectator view retains the previous world entity");
        require(viewer.connection->send_all(protocol::Move::frame_client_end(8, 0, 0)),
                "attempt movement through the read-only bridge");
        require(viewer.connection->send_all(protocol::Handshake::build_connect(0)),
                "request a fresh snapshot after the prohibited input");
        require(viewer.receive([&](auto type, const auto& bytes) {
            if (type == static_cast<std::uint8_t>(protocol::S2C::Code)) {
                protocol::S2CCodeFields code{};
                require(protocol::Handshake::parse_code(bytes.data(), bytes.size(), code) &&
                            viewer.connection->send_all(
                                protocol::Handshake::build_answer(code.protocol_test_value)),
                        "answer the repeated snapshot challenge");
                return false;
            }
            protocol::CreatePlayerFields player;
            return type == static_cast<std::uint8_t>(protocol::S2C::CreatePlayer) &&
                   protocol::CreatePlayer::parse(bytes.data(), bytes.size(), player) &&
                   player.id == static_cast<std::uint32_t>(entity) && player.x == 1024;
        }) && server::read_test_character(call(inspect).at("record").as_string())->map_x == 1024,
                "spectator movement never reaches the authoritative server");
        viewer.connection.reset();
        require(wait([&] { return call(inspect).at("entity").as_int() == entity; }),
                "closing control keeps the same character online");
        task({{"op", "view_closed"}, {"endpoint", endpoint}});
        endpoint.clear();
        task({{"op", "view"}, {"ids", QJsonArray{first}}});
        Viewer reopened;
        require(reopened.enter(endpoint.toStdString(), first) == static_cast<std::uint32_t>(entity),
                "reopen graphical control without logging out");
        const int second = ids.at(1).toInt();
        inspect.object()["id"].set_number(second);
        const auto secondEntity = static_cast<std::uint32_t>(call(inspect).at("entity").as_int());
        require(reopened.receive([&](auto type, const auto& bytes) {
            protocol::CreatePlayerFields player;
            return type == static_cast<std::uint8_t>(protocol::S2C::CreatePlayer) &&
                   protocol::CreatePlayer::parse(bytes.data(), bytes.size(), player) &&
                   player.id == secondEntity;
        }),
                "observer already sees the other player before teleport");
        task({{"op", "teleport"},
              {"ids", QJsonArray{second}},
              {"map", 0},
              {"x", 1100},
              {"y", 1100},
              {"z", 100},
              {"radius", 0},
              {"spacing", 0},
              {"seed", 3}});
        require(reopened.receive([&](auto type, const auto& bytes) {
            std::uint32_t id = 0;
            int x = 0, y = 0, z = 0;
            std::uint16_t direction = 0;
            return type == static_cast<std::uint8_t>(protocol::S2C::MoveBefore) &&
                   protocol::Move::parse_before(
                       bytes.data(), bytes.size(), id, x, y, z, direction) &&
                   id == secondEntity && x == 1100 && y == 1100 && z == 100;
        }),
                "same-area teleport reaches an existing observer without relog");
        auto moveSecond = [&](int x, int y) {
            task({{"op", "teleport"},
                  {"ids", QJsonArray{second}},
                  {"map", 0},
                  {"x", x},
                  {"y", y},
                  {"z", 100},
                  {"radius", 0},
                  {"spacing", 0}});
        };
        require(reopened.receive([&](auto, const auto&) { return reopened.monsters.size() == 2; }),
                "near observer receives its resident monster population");
        const auto nearMonsters = reopened.monsters;
        const auto nearRemovals = reopened.removed_monsters;
        moveSecond(4000, 4000);
        require(reopened.receive([&](auto type, const auto& bytes) {
            std::uint32_t id = 0;
            return type == static_cast<std::uint8_t>(protocol::S2C::RemovePlayer) &&
                   protocol::CreatePlayer::parse_remove(bytes.data(), bytes.size(), id) &&
                   id == secondEntity;
        }),
                "teleport out of sight immediately removes the remote player");
        const auto nearEndpoint = endpoint;
        task({{"op", "view"}, {"ids", QJsonArray{second}}});
        Viewer remote;
        require(remote.enter(endpoint.toStdString(), second) == secondEntity,
                "attach a simultaneous observer in the distant hunting ground");
        require(remote.receive([&](auto, const auto&) { return remote.monsters.size() == 2; }),
                "distant observer receives a separate resident monster population");
        const auto farMonsters = remote.monsters;
        const auto farRemovals = remote.removed_monsters;
        require(std::none_of(farMonsters.begin(),
                             farMonsters.end(),
                             [&](auto id) { return nearMonsters.count(id) != 0; }),
                "separated viewers receive only monsters in their own sight");
        const auto refillEnd = std::chrono::steady_clock::now() + std::chrono::milliseconds(2500);
        require(wait(
                    [&] {
                        const auto ignore = [](auto, const auto&) { return false; };
                        (void) reopened.poll(ignore);
                        (void) remote.poll(ignore);
                        if (reopened.monsters != nearMonsters || remote.monsters != farMonsters ||
                            reopened.removed_monsters != nearRemovals ||
                            remote.removed_monsters != farRemovals) {
                            throw std::runtime_error(
                                "Distant refills changed a visible monster population.");
                        }
                        return std::chrono::steady_clock::now() >= refillEnd;
                    },
                    5),
                "both observers retain identical monster entities across multiple refill cycles");
        remote.connection.reset();
        task({{"op", "view_closed"}, {"endpoint", endpoint}});
        endpoint = nearEndpoint;
        moveSecond(1100, 1100);
        require(reopened.receive([&](auto type, const auto& bytes) {
            protocol::CreatePlayerFields player;
            return type == static_cast<std::uint8_t>(protocol::S2C::CreatePlayer) &&
                   protocol::CreatePlayer::parse(bytes.data(), bytes.size(), player) &&
                   player.id == secondEntity && player.x == 1100 && player.y == 1100;
        }),
                "teleport back into sight recreates the remote player at its current pose");
        reopened.connection.reset();
        task({{"op", "view_closed"}, {"endpoint", endpoint}});
        task({{"op", "teleport"},
              {"ids", ids},
              {"map", 0},
              {"x", 1200},
              {"y", 1200},
              {"z", 100},
              {"radius", 160},
              {"spacing", 24},
              {"seed", 3}});
        std::vector<std::pair<int, int>> poses;
        for (const auto& id : ids) {
            inspect.object()["id"].set_number(id.toInt());
            const auto row = call(inspect);
            const auto character = server::read_test_character(row.at("record").as_string());
            require(character && row.at("online").as_int() == 1,
                    "teleported player remains online");
            for (const auto& p : poses) {
                const int dx = p.first - character->map_x, dy = p.second - character->map_y;
                if (dx * dx + dy * dy < 24 * 24) {
                    throw std::runtime_error("Scatter violated minimum spacing.");
                }
            }
            poses.emplace_back(character->map_x, character->map_y);
        }
        // Real traffic continues while the tool is otherwise idle.
        int soakSeconds = 3;
        if (const char* soak = std::getenv("OPENKAL_TEST_PLAYER_SOAK_SECONDS")) {
            soakSeconds = std::clamp(std::stoi(soak), 1, 3600);
        }
        const auto idleEnd = std::chrono::steady_clock::now() + std::chrono::seconds(soakSeconds);
        while (std::chrono::steady_clock::now() < idleEnd) {
            QCoreApplication::processEvents();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        task({{"op", "refresh"}});
        int onlineAfterIdle = 0;
        for (const auto& player : state["players"].toArray()) {
            if (player.toObject()["status"].toString() == "Online") {
                ++onlineAfterIdle;
            }
        }
        require(onlineAfterIdle == kPlayerCount,
                "all fifty-one sessions remain online after the idle interval");
        task({{"op", "provision"}, {"players", 12}, {"guilds", 3}});
        require(
            state["players"].toArray().size() == 12 && state["guilds"].toArray().size() == 3,
            "reduce active population and archive extra guilds without deleting character records");
        task({{"op", "provision"}, {"players", kPlayerCount}, {"guilds", 8}});
        require(state["players"].toArray().size() == kPlayerCount &&
                    state["players"].toArray().first().toObject()["id"].toInt() == first,
                "restore population and reuse existing identities");
        const auto previousEndpoint = endpoint;
        task({{"op", "logout"}, {"ids", QJsonArray{first}}});
        task({{"op", "login"}, {"ids", QJsonArray{first}}});
        endpoint.clear();
        task({{"op", "view"}, {"ids", QJsonArray{first}}});
        require(!endpoint.isEmpty() && endpoint != previousEndpoint,
                "relogin creates a new worker and spectator endpoint generation");
        task({{"op", "view_closed"}, {"endpoint", previousEndpoint}});
        Viewer replacement;
        require(replacement.enter(endpoint.toStdString(), first) > 0,
                "a delayed viewer-close event cannot close the replacement session");
        replacement.connection.reset();
        task({{"op", "view_closed"}, {"endpoint", endpoint}});
        require(!wrongEventThread, "backend roster events are delivered on the GUI thread");
        task({{"op", "logout"}, {"ids", ids}});
        host.stop();
        std::filesystem::create_directories(root + "/starters");
        std::ofstream(root + "/starters/common.json")
            << R"({"spawn":[1360, 1376, 100],"items":[]})";
        const char* classes[] = {"knight", "mage", "archer", "thief"};
        const char* weapons[] = {"sword", "wand", "bow", "dagger"};
        for (int clazz = 0; clazz < 4; ++clazz) {
            const int item = clazz == 0 ? 1 : clazz + 2;
            std::ofstream(root + "/items/" + std::to_string(item) + ".json")
                << "{\"index\":" << item << ",\"class\":[\"weapon\",\"" << weapons[clazz]
                << "\"],\"wear\":1,\"endurance\":50,\"limit\":{\"class\":\"" << classes[clazz]
                << "\"}}";
            std::ofstream(root + "/starters/" + classes[clazz] + ".json")
                << "{\"items\":[{\"item_id\":" << item << ",\"count\":1,\"equipped\":true}]}";
        }
        cfg.starters_dir = root + "/starters";
        require(host.start(cfg), "restart with an owned normal starter kit");
        server::TestControlClient starterControl;
        require(wait(
                    [&] {
                        return starterControl.connect("127.0.0.1",
                                                      cfg.tcp_port,
                                                      cfg.test_control_token,
                                                      server::tls_config_from_environment());
                    },
                    10),
                "connect starter provisioning test");
        const auto created = starterControl.request(J::parse(R"({"op":"ensure","ordinal":52})"));
        require(created.at("error").is_null() && created.at("id").as_int() > 0,
                "provision through normal character creation");
        const auto inspectStarter = J::parse(
            "{\"op\":\"inspect\",\"id\":" + std::to_string(created.at("id").as_int()) + "}");
        const auto starter = starterControl.request(inspectStarter);
        const auto starterCharacter = server::read_test_character(starter.at("record").as_string());
        require(starterCharacter && starterCharacter->weapon_index == 5 &&
                    starterCharacter->map_x == 1360 && starterCharacter->map_y == 1376 &&
                    starter.at("items").array().size() == 1 &&
                    starter.at("items").array()[0].at("slot").as_int() != 0,
                "test character receives the normal equipped starter weapon and authored spawn");
        const auto reused = starterControl.request(J::parse(R"({"op":"ensure","ordinal":52})"));
        const auto afterReuse = starterControl.request(inspectStarter);
        require(reused.at("id").as_int() == created.at("id").as_int() &&
                    afterReuse.at("items").array().size() == 1 &&
                    afterReuse.at("items").array()[0].at("iid").as_int() ==
                        starter.at("items").array()[0].at("iid").as_int(),
                "reprovisioning preserves the character without duplicating its starter kit");
        inspect.object()["id"].set_number(first);
        const auto beforeRepair = starterControl.request(inspect);
        const auto beforeRecord =
            server::read_test_character(beforeRepair.at("record").as_string());
        const auto repaired = starterControl.request(ensureWeapon);
        const int repairedId = repaired.at("iid").as_int();
        require(repaired.at("error").is_null() && repairedId > 0,
                "restore only the missing starter weapon of an existing offline test player");
        const auto afterRepair = starterControl.request(inspect);
        const auto afterRecord = server::read_test_character(afterRepair.at("record").as_string());
        require(beforeRecord && afterRecord && beforeRecord->money == afterRecord->money &&
                    beforeRecord->map_x == afterRecord->map_x &&
                    beforeRecord->map_y == afterRecord->map_y &&
                    afterRepair.at("items").array().size() ==
                        beforeRepair.at("items").array().size() + 1,
                "weapon recovery preserves existing items, currency and position");
        require(starterControl.request(ensureWeapon).at("iid").as_int(-1) == 0 &&
                    starterControl.request(inspect).at("items").array().size() ==
                        afterRepair.at("items").array().size(),
                "an unequipped suitable weapon prevents duplicate recovery grants");
        Result bound(PQexec(admin.get(),
                            ("SELECT character_bound FROM " + schema +
                             ".items WHERE iid=" + std::to_string(repairedId))
                                .c_str()),
                     PQclear);
        require(bound && PQresultStatus(bound.get()) == PGRES_TUPLES_OK &&
                    PQntuples(bound.get()) == 1 &&
                    std::string(PQgetvalue(bound.get(), 0, 0)) == "1",
                "the replacement starter weapon is durably character-bound");

        task({{"op", "connect"},
              {"host", "127.0.0.1"},
              {"port", cfg.tcp_port},
              {"token", QString::fromStdString(cfg.test_control_token)},
              {"ca", QString::fromUtf8(std::getenv("OPENKAL_TLS_CA"))}});
        task({{"op", "login"}, {"ids", QJsonArray{second}}});
        auto liveInspect = inspect;
        liveInspect.object()["id"].set_number(second);
        require(
            wait([&] { return starterControl.request(liveInspect).at("online").as_int() != 0; }),
            "enter an existing unarmed test player before recovering its weapon");
        auto liveWeapon = ensureWeapon;
        liveWeapon.object()["id"].set_number(second);
        const auto liveRepair = starterControl.request(liveWeapon);
        const int liveWeaponId = liveRepair.at("iid").as_int();
        require(liveRepair.at("error").is_null() && liveWeaponId > 0,
                "recover a missing starter weapon while the player is online");
        const auto mageInventory = starterControl.request(liveInspect);
        require(std::any_of(mageInventory.at("items").array().begin(),
                            mageInventory.at("items").array().end(),
                            [&](const auto& item) {
                                return item.at("iid").as_int() == liveWeaponId &&
                                       item.at("index").as_int() == 3;
                            }),
                "weapon recovery selects the configured weapon for the player's class");
        task({{"op", "equip"}, {"ids", QJsonArray{second}}, {"iid", liveWeaponId}});
        require(wait([&] {
                    const auto inventory = starterControl.request(liveInspect);
                    for (const auto& item : inventory.at("items").array()) {
                        if (item.at("iid").as_int() == liveWeaponId) {
                            return item.at("slot").as_int() == 1;
                        }
                    }
                    return false;
                }),
                "the ordinary equipment request can wear the live replacement weapon");
        require(starterControl.request(liveWeapon).at("iid").as_int(-1) == 0,
                "online recovery leaves an equipped weapon unchanged");
        task({{"op", "logout"}, {"ids", QJsonArray{second}}});
        host.stop();
        cfg.test_control_token.clear();
        require(host.start(cfg), "restart with test control disabled by default");
        server::TestControlClient disabled;
        require(wait(
                    [&] {
                        return disabled.connect("127.0.0.1",
                                                cfg.tcp_port,
                                                "OwnedTestControlSecret2026",
                                                server::tls_config_from_environment());
                    },
                    10),
                "connect after test mode is disabled");
        require(!disabled.request(J::parse(R"({"op":"hello"})")).at("error").is_null(),
                "reject test commands when server token is absent");
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << '\n';
        result = 1;
    }
    if (!sql("DROP SCHEMA " + schema + " CASCADE")) {
        result = 1;
    }
    std::filesystem::remove_all(root);
    return result;
}
