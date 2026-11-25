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
#include "openkal/protocol/CreatePlayer.h"
#include "openkal/protocol/FrameParser.h"
#include "openkal/protocol/GameExit.h"
#include "openkal/protocol/Handshake.h"
#include "openkal/protocol/ItemWear.h"
#include "openkal/protocol/Move.h"
#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketReader.h"
#include "openkal/protocol/PacketWriter.h"
#include "openkal/protocol/PlayerOptions.h"
#include "openkal/protocol/Progression.h"
#include "openkal/protocol/Property.h"
#include "openkal/protocol/Pvp.h"
#include "openkal/protocol/Quest.h"
#include "openkal/protocol/Shop.h"
#include "openkal/protocol/Skill.h"
#include "openkal/protocol/Teleport.h"
#include "openkal/protocol/UseItem.h"
#include "openkal/server/AccountPassword.h"
#include "openkal/server/GuildService.h"
#include "openkal/server/LobbyClient.h"
#include "openkal/server/ServerHost.h"
#include "openkal/server/TestControl.h"
#include "openkal/server/store/PostgresStore.h"

#include <chrono>
#include <cctype>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <libpq-fe.h>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <unistd.h>

namespace {
    using namespace openkal;
    using Clock = std::chrono::steady_clock;
    using namespace std::chrono_literals;
    constexpr std::uint16_t kGuard = 62100;
    constexpr std::uint16_t kMaster = 62101;
    constexpr std::uint16_t kQuest = 64990;
    constexpr int kOutside = 1024;
    constexpr int kLeaderStartX = kOutside + 16;
    constexpr int kPartnerStartY = kOutside + 16;
    constexpr int kCamp = 1250;
    constexpr int kFlagX = 1252;
    constexpr int kFlagIndex = 62104;
    constexpr int kSword = 62105;
    constexpr int kBlockerIndex = 62106;
    constexpr auto kPassword = "Owned castle test password";
    constexpr auto kTestToken = "OwnedCastleTestControl2026";

    void require(bool condition, const char* message) {
        if (!condition) {
            throw std::runtime_error(message);
        }
        std::cout << "PASS " << message << std::endl;
    }

    void write(const std::filesystem::path& path, const std::string& text) {
        std::ofstream output(path);
        output << text;
        output.close();
        if (!output.good()) {
            throw std::runtime_error("could not write synthetic fixture " + path.string());
        }
    }

    class Peer {
      public:
        explicit Peer(std::unique_ptr<server::ISession> session) : m_session(std::move(session)) {}

        void send(const std::vector<std::uint8_t>& bytes, const char* message) {
            require(m_session && m_session->send_all(bytes), message);
        }

        template <typename Predicate>
        bool receive(protocol::S2C opcode, Predicate predicate, Clock::duration timeout = 3s) {
            const auto end = Clock::now() + timeout;
            do {
                for (;;) {
                    std::size_t size = 0;
                    std::uint8_t type = 0;
                    std::vector<std::uint8_t> payload;
                    const auto parsed = protocol::FrameParser::pop(
                        m_buffer.data(), m_buffer.size(), size, type, payload);
                    if (parsed == protocol::FrameParseStatus::Invalid) {
                        throw std::runtime_error("invalid frame from Castle War host");
                    }
                    if (parsed == protocol::FrameParseStatus::Incomplete) {
                        break;
                    }
                    m_buffer.erase(m_buffer.begin(), m_buffer.begin() + size);
                    if (type == static_cast<std::uint8_t>(opcode) && predicate(payload)) {
                        return true;
                    }
                }
                std::this_thread::sleep_for(5ms);
                if (!m_session->recv_into(m_buffer)) {
                    return false;
                }
            } while (Clock::now() < end);
            return false;
        }

        void enter(std::uint32_t& guard, std::uint32_t& master, bool expect_blocker) {
            using namespace protocol;
            send(Handshake::build_connect(0), "send authenticated world connection");
            S2CCodeFields code{};
            require(receive(S2C::Code,
                            [&](const auto& payload) {
                                return Handshake::parse_code(payload.data(), payload.size(), code);
                            }),
                    "receive world challenge");
            send(Handshake::build_answer(code.protocol_test_value), "answer world challenge");
            require(receive(S2C::CreatePlayer,
                            [&](const auto& payload) {
                                CreatePlayerFields self;
                                if (!CreatePlayer::parse(payload.data(), payload.size(), self) ||
                                    !(self.class_byte & 0x80)) {
                                    return false;
                                }
                                m_id = self.id;
                                return true;
                            }),
                    "receive world character");
            if (expect_blocker) {
                (void) monster_at(kBlockerIndex,
                                  1248,
                                  1248,
                                  "receive a stationary monster occupying the flag cell");
            }
            require(receive(S2C::CreateNpc,
                            [&](const auto& payload) {
                                CreateNpcInfo npc;
                                if (!Quest::parse_create_npc(payload.data(), payload.size(), npc)) {
                                    return false;
                                }
                                if (npc.index == kGuard) {
                                    guard = npc.id;
                                } else if (npc.index == kMaster) {
                                    master = npc.id;
                                }
                                return guard != 0 && master != 0;
                            }),
                    "receive configured guard and master runtime identities");
        }

        void talk(std::uint32_t npc, int dialog) {
            using namespace protocol;
            send(Quest::frame_ask_npc(npc), "talk to configured NPC");
            require(receive(S2C::SendHtml,
                            [&](const auto& payload) {
                                std::int32_t html = 0;
                                std::uint32_t source = 0;
                                return Quest::parse_send_html(
                                           payload.data(), payload.size(), html, &source) &&
                                       html == dialog && source == npc;
                            }),
                    "receive Castle War dialog for the selected NPC");
        }

        protocol::AssassinList read_assassin_list(std::uint32_t npc) {
            using namespace protocol;
            send(Quest::frame_ask_npc(npc), "open the Assassin List board");
            AssassinList list;
            require(receive(S2C::AssassinList,
                            [&](const auto& payload) {
                                return parse_assassin_list(payload.data(), payload.size(), list) &&
                                       list.npc_id == npc;
                            }),
                    "receive the board snapshot for the selected NPC");
            return list;
        }

        bool response(std::string_view expected) {
            return receive(protocol::S2C::Chatting, [&](const auto& payload) {
                std::string speaker, message;
                return protocol::Chatting::parse_s2c(
                           payload.data(), payload.size(), speaker, message) &&
                       message.find(expected) != std::string::npos;
            });
        }

        protocol::PlayerOptionsState
        player_options(const Peer& target,
                       protocol::PlayerAction action = protocol::PlayerAction::Query) {
            using namespace protocol;
            PlayerOptionsRequest request;
            request.action = action;
            request.request_id = ++m_options_request;
            request.target = target.id();
            send(frame_player_options_request(request), "use selected player options");
            PlayerOptionsState state;
            require(receive(S2C::PlayerOptions,
                            [&](const auto& payload) {
                                return parse_player_options_state(
                                           payload.data(), payload.size(), state) &&
                                       state.request_id == request.request_id &&
                                       state.target == request.target;
                            }),
                    "receive selected player options and result");
            return state;
        }

        void teleport(int x, int y) {
            using namespace protocol;
            int z = 0;
            require(receive(
                        S2C::Teleport,
                        [&](const auto& payload) {
                            std::uint8_t map = 0, mode = 0;
                            int actual_x = 0, actual_y = 0;
                            return Teleport::parse_server(payload.data(),
                                                          payload.size(),
                                                          map,
                                                          actual_x,
                                                          actual_y,
                                                          z,
                                                          mode) &&
                                   map == 0 && actual_x == x && actual_y == y;
                        },
                        15s),
                    "receive authoritative Castle War transfer");
            send(Teleport::frame_client_ack(1, z), "acknowledge Castle War transfer");
        }

        std::uint32_t monster_at(int definition, int x, int y, const char* message) {
            using namespace protocol;
            std::uint32_t id = 0;
            require(receive(
                        S2C::CreateMonster,
                        [&](const auto& payload) {
                            PacketReader reader(payload);
                            std::uint16_t index = 0;
                            std::uint32_t actual_x = 0, actual_y = 0;
                            return reader.read_u16(index) && reader.read_u32(id) &&
                                   reader.read_u32(actual_x) && reader.read_u32(actual_y) &&
                                   index == definition &&
                                   actual_x == static_cast<std::uint32_t>(x) &&
                                   actual_y == static_cast<std::uint32_t>(y);
                        },
                        12s),
                    message);
            return id;
        }

        std::uint32_t standard(int x, int y) {
            return monster_at(kFlagIndex, x, y, "receive the objective at its configured position");
        }

        void capture(std::uint32_t standard) {
            using namespace protocol;
            bool hit = false;
            for (int attempt = 0; attempt < 10 && !hit; ++attempt) {
                send(Attack::frame_client(kCkMonster, standard), "attack an objective");
                hit = receive(
                    S2C::Attack,
                    [standard](const auto& payload) {
                        std::uint32_t source = 0, target = 0;
                        std::int32_t damage = 0, extra = 0;
                        std::uint8_t flags = 0;
                        return Attack::parse_server(payload.data(),
                                                    payload.size(),
                                                    source,
                                                    target,
                                                    damage,
                                                    extra,
                                                    flags) &&
                               target == standard && damage > 0;
                    },
                    300ms);
                if (!hit) {
                    std::this_thread::sleep_for(200ms);
                }
            }
            require(hit, "hit the objective through regular combat");
        }

        std::uint32_t id() const {
            return m_id;
        }

        void close() {
            m_session->close();
        }

        bool pvp_status(std::uint32_t id,
                        protocol::PvpRelation relation,
                        bool attackable,
                        bool masked = false,
                        bool duel_defeat = false) {
            return receive(protocol::S2C::PvpStatus, [&](const auto& payload) {
                protocol::PvpStatus state;
                return protocol::parse_pvp_status(payload.data(), payload.size(), state) &&
                       state.player_id == id && state.relation == relation &&
                       state.attackable == attackable && state.masked == masked &&
                       (state.dueling && state.defeated) == duel_defeat;
            });
        }

        bool sees_assassin(std::uint32_t id) {
            return receive(protocol::S2C::CreatePlayer, [&](const auto& payload) {
                protocol::CreatePlayerFields player;
                return protocol::CreatePlayer::parse(payload.data(), payload.size(), player) &&
                       player.id == id && player.name == protocol::kAssassinName;
            });
        }

        bool player_attack(std::uint32_t id, Clock::duration timeout, int minimum_damage = 0) {
            return receive(
                protocol::S2C::Attack,
                [&](const auto& payload) {
                    std::uint32_t attacker = 0, target = 0;
                    std::int32_t damage = 0, extra = 0;
                    std::uint8_t flags = 0;
                    return protocol::Attack::parse_server(payload.data(),
                                                          payload.size(),
                                                          attacker,
                                                          target,
                                                          damage,
                                                          extra,
                                                          flags) &&
                           attacker == m_id && target == id && damage >= minimum_damage;
                },
                timeout);
        }

      private:
        std::uint32_t m_id = 0;
        std::uint64_t m_options_request = 0;
        std::unique_ptr<server::ISession> m_session;
        std::vector<std::uint8_t> m_buffer;
    };

    Peer enter_world(std::uint16_t port,
                     const std::string& account,
                     int character,
                     std::uint32_t& guard,
                     std::uint32_t& master,
                     bool expect_blocker = false) {
        server::LobbyClient lobby;
        require(lobby.connect("127.0.0.1", port) && lobby.login(account, kPassword) &&
                    lobby.select(character).has_value(),
                "authenticate an ordinary guild leader over TLS");
        Peer peer(lobby.release_session());
        peer.enter(guard, master, expect_blocker);
        return peer;
    }

    void seed_leader(server::PostgresStore& store,
                     server::GuildService& guilds,
                     int id,
                     std::string account,
                     std::string name,
                     std::string guild,
                     int x = kOutside,
                     int y = kOutside) {
        using namespace server;
        const auto hash = hash_account_password(kPassword);
        require(!hash.empty() && store.upsert_account({id, std::move(account), hash}),
                "seed an ordinary test account");
        CharacterRecord character;
        character.id = id;
        character.account_id = id;
        character.name = std::move(name);
        character.map_x = x;
        character.map_y = y;
        character.map_z = 100;
        character.level = 70;
        character.str = character.dex = 100;
        character.weapon_index = kSword;
        ItemRecord sword;
        sword.iid = id;
        sword.character_id = id;
        sword.index = kSword;
        sword.count = 1;
        sword.slot = 1;
        require(store.save_character(character) && store.insert_item(sword),
                "seed a guild leader and equipped weapon");
        protocol::GuildRequest request;
        request.command = protocol::GuildCommand::Create;
        request.request_id = static_cast<std::uint64_t>(id);
        request.revision = guilds.revision();
        request.text = std::move(guild);
        GuildContext context;
        context.actor = character;
        const auto result = guilds.execute(request, context, std::time(nullptr));
        if (!result.error.empty()) {
            throw std::runtime_error("could not create guild " + request.text + ": " +
                                     result.error);
        }
    }

    platform::Json castle_command(server::TestControlClient& control, const char* action) {
        auto command = platform::Json::parse(R"({"op":"castle_war"})");
        command.object()["action"].set_type(platform::Json::Type::String);
        command.object()["action"].string() = action;
        auto result = control.request(command);
        if (!result.at("error").is_null()) {
            throw std::runtime_error(result.at("error").as_string());
        }
        return result;
    }

    template <typename Predicate>
    bool wait_until(Predicate predicate, Clock::duration timeout = 8s) {
        const auto end = Clock::now() + timeout;
        do {
            if (predicate()) {
                return true;
            }
            std::this_thread::sleep_for(70ms);
        } while (Clock::now() < end);
        return false;
    }

    std::string phase(const platform::Json& status) {
        auto value = status.at("phase").as_string();
        for (auto& c : value) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return value;
    }

    void exercise_guild_catalog(server::TestControlClient& control,
                                server::TestControlClient& denied) {
        using platform::Json;
        auto request = Json::parse(R"({"op":"castle_war_guilds","after":0,"limit":2})");
        const auto rejected = denied.request(request);
        require(rejected.at("error").as_string() ==
                        "Test control is disabled or authentication failed." &&
                    rejected.at("guilds").is_null(),
                "require the test-control token to read the Castle War guild catalog");

        const auto pool = control.request(Json::parse(R"({"op":"guilds","count":1})"));
        const auto& pool_guilds = pool.at("guilds").array();
        require(pool.at("error").is_null() && pool_guilds.size() == 1 &&
                    pool_guilds.front().at("name").as_string() == "TestGuild0001",
                "create a managed test guild alongside the three ordinary guilds");

        const auto matches_guild = [](const Json& row, const char* name, bool managed) {
            return row.at("id").is_number() && row.at("id").number() > 0 &&
                   row.at("name").as_string() == name &&
                   row.at("managed").type() == Json::Type::Bool &&
                   row.at("managed").boolean() == managed;
        };
        const auto first_page = control.request(request);
        const auto& first = first_page.at("guilds").array();
        require(first_page.at("error").is_null() && first.size() == 2 &&
                    matches_guild(first[0], "SiegeFounders", false) &&
                    matches_guild(first[1], "SiegeLeaders", false) &&
                    first[0].at("id").number() < first[1].at("id").number() &&
                    first_page.at("next").is_number() &&
                    first_page.at("next").number() == first[1].at("id").number(),
                "list ordinary guilds by ID with a cursor for the next page");

        request.object()["after"].set_number(first_page.at("next").number());
        const auto second_page = control.request(request);
        const auto& second = second_page.at("guilds").array();
        require(second_page.at("error").is_null() && second.size() == 2 &&
                    matches_guild(second[0], "SiegePartners", false) &&
                    matches_guild(second[1], "TestGuild0001", true) &&
                    first[1].at("id").number() < second[0].at("id").number() &&
                    second[0].at("id").number() < second[1].at("id").number() &&
                    second_page.at("next").is_number() && second_page.at("next").number() == 0,
                "include managed guilds and finish catalog pagination after the last guild");
    }

    void exercise_simulation(std::uint16_t port, server::PostgresStore& store, Peer& defender) {
        using namespace server;
        using platform::Json;
        const auto start = Json::parse(R"({
            "op": "castle_war",
            "action": "start",
            "attack": [["SiegeLeaders", "SiegePartners"]],
            "defense": ["SiegeFounders"],
            "declaration_seconds": 2,
            "preparation_seconds": 2,
            "admission_seconds": 10,
            "battle_seconds": 30,
            "capture_unlock_seconds": 0,
            "hold_seconds": 2,
            "flag_hit_points": 1
        })");
        TestControlClient denied;
        const auto trust = tls_config_from_environment();
        require(denied.connect("127.0.0.1", port, "wrong", trust) &&
                    !denied.request(start).at("error").is_null(),
                "require the dedicated test-control token to override Castle War");
        TestControlClient control;
        require(control.connect("127.0.0.1", port, kTestToken, trust),
                "connect the authenticated test controller");
        exercise_guild_catalog(control, denied);
        require(wait_until([&] {
                    const auto status = castle_command(control, "status");
                    return status.at("available").boolean() && !status.at("simulation").boolean() &&
                           phase(status) == "idle" && status.at("start_blocked").is_string() &&
                           status.at("start_blocked").as_string().empty() &&
                           status.at("owner").as_string() == "SiegeFounders";
                }),
                "read the live castle owner before enabling simulation");
        Json before;
        require(wait_until([&] {
                    return store.load_castle_war(before) && before.at("players").is_array() &&
                           before.at("players").array().empty();
                }),
                "wait for the scheduled siege to persist the defender's return");
        std::uint32_t guard = 0, master = 0;
        auto leader = enter_world(port, "castlechallenger", 2, guard, master);
        guard = master = 0;
        auto partner = enter_world(port, "castlepartner", 3, guard, master);

        auto conflicting = start;
        conflicting.object()["defense"] = Json::parse(R"(["SiegePartners"])");
        require(!control.request(conflicting).at("error").is_null() &&
                    !castle_command(control, "status").at("simulation").boolean(),
                "reject a guild on both sides without starting the simulation");
        const auto started = control.request(start);
        if (!started.at("error").is_null()) {
            throw std::runtime_error(started.at("error").as_string());
        }
        require(started.at("simulation").boolean() &&
                    !started.at("start_blocked").as_string().empty(),
                "configure a leading attacking guild, its partner, and the defenders");
        require(wait_until([&] {
                    const auto status = castle_command(control, "status");
                    return status.at("simulation").boolean() && phase(status) == "admission" &&
                           status.at("seconds_remaining").as_int() <= 10;
                }),
                "reach the shortened admission window within seconds");
        const auto admitted = castle_command(control, "admit");
        require(admitted.at("admitted").as_int(-1) == 3,
                "admit the three ordinary connected guild leaders");
        leader.teleport(kCamp, kCamp);
        partner.teleport(kCamp, kCamp);
        defender.teleport(1450, 1450);

        partner.capture(partner.standard(kFlagX, kCamp));
        leader.capture(leader.standard(1280, kCamp));
        const auto score = castle_command(control, "status");
        require(phase(score) == "battle" && score.at("attack_score").as_int() == 80 &&
                    score.at("defense_score").as_int() == 60,
                "use live flag results to score the simulated factions");
        const auto finished = castle_command(control, "finish");
        require(finished.at("simulation").boolean() &&
                    finished.at("owner").as_string() == "SiegeLeaders",
                "finish on the current score and award ownership to the attacking alliance leader");
        Json during;
        require(store.load_castle_war(during) && Json::stringify(during) == Json::stringify(before),
                "simulated ownership and Honor leave the saved castle state untouched");
        (void) castle_command(control, "restore");
        leader.teleport(kLeaderStartX, kOutside);
        partner.teleport(kOutside, kPartnerStartY);
        defender.teleport(kOutside, kOutside);
        require(wait_until([&] {
                    const auto restored = castle_command(control, "status");
                    return !restored.at("simulation").boolean() &&
                           restored.at("owner").as_string() == "SiegeFounders";
                }),
                "restore the live schedule and castle owner after returning every participant");
        Json after;
        require(store.load_castle_war(after) && Json::stringify(after) == Json::stringify(before),
                "restoring the schedule leaves the saved castle state untouched");

        const auto automatic = control.request(Json::parse(R"({
            "op": "castle_war",
            "action": "start",
            "attack": [["SiegeLeaders"]],
            "defense": [],
            "declaration_seconds": 0,
            "preparation_seconds": 0,
            "admission_seconds": 6,
            "battle_seconds": 10,
            "capture_unlock_seconds": 0,
            "hold_seconds": 2,
            "flag_hit_points": 1,
            "auto_admit": true
        })"));
        require(automatic.at("simulation").boolean() && phase(automatic) == "admission",
                "start a second simulation directly in admission");
        leader.teleport(kCamp, kCamp);
        (void) castle_command(control, "restore");
        leader.teleport(kLeaderStartX, kOutside);
        require(wait_until([&] {
                    const auto restored = castle_command(control, "status");
                    return !restored.at("simulation").boolean() &&
                           restored.at("owner").as_string() == "SiegeFounders";
                }),
                "automatically admit the online leader and cancel during admission");
    }

    std::string create_rules(std::time_t battle) {
        const std::tm calendar = *std::gmtime(&battle);
        auto doc = platform::Json::parse(R"({
          "schema": "openkal.castle_war/v1",
          "enabled": true,
          "minimum_level": 1,
          "schedule": {
            "utc_offset_minutes": 0,
            "declaration_seconds": 60,
            "preparation_seconds": 10,
            "admission_seconds": 8,
            "battle_seconds": 12
          },
          "combat": {
            "capture_unlock_seconds": 0,
            "hold_seconds": 2,
            "flag_regen_points": 0
          },
          "layout": {
            "bounds": [1200, 1200, 1500, 1500],
            "guard_npc_index": 62100,
            "master_npc_index": 62101,
            "camp_npc_index": 62102,
            "manager_npc_index": 62103,
            "attack_camp": [0, 1250, 1250, 100],
            "defense_camp": [0, 1450, 1450, 100],
            "east_gate": [0, 1300, 1250, 100],
            "west_gate": [0, 1250, 1300, 100],
            "exit": [0, 1024, 1024, 100],
            "flags": [
              {"position": [0, 1252, 1250, 100], "monster_index": 62104, "hit_points": 1},
              {"position": [0, 1280, 1250, 100], "monster_index": 62104, "hit_points": 1},
              {"position": [0, 1260, 1270, 100], "monster_index": 62104, "hit_points": 1},
              {"position": [0, 1280, 1270, 100], "monster_index": 62104, "hit_points": 1}
            ]
          }
        })");
        auto& schedule = doc.object()["schedule"].object();
        schedule["battle_weekday"].set_number(calendar.tm_wday);
        schedule["battle_hour"].set_number(calendar.tm_hour);
        schedule["battle_minute"].set_number(calendar.tm_min);
        schedule["battle_second"].set_number(calendar.tm_sec);
        return platform::Json::stringify(doc);
    }

    void
    exercise_pvp(server::ServerConfig config, server::PostgresStore& store, std::uint16_t port) {
        using namespace protocol;
        using namespace server;
        constexpr int mask_index = 1036;
        constexpr int archer_mask_index = 1037;
        constexpr int mask_iid = 70001;
        constexpr int reserve_iid = 70002;
        constexpr int scroll_iid = 70003;
        constexpr int stack_iid = 70004;
        constexpr int wrong_class_iid = 70005;
        constexpr int below_level_iid = 70006;
        constexpr int scroll_index = 62001;
        constexpr int stack_index = 62002;
        constexpr int level = 70;
        const auto level_exp = need_exp_for_level(level);
        const auto start_exp = level_exp / 2;
        config.guild_rules_path.clear();
        config.castle_war_path.clear();
        config.assassin_safe_areas = {
            {0, kOutside - 16, kOutside - 16, kOutside + 20, kOutside + 24}};
        const auto items = std::filesystem::path(config.items_dir);
        const auto shops = items.parent_path() / "shops";
        std::filesystem::create_directories(shops);
        config.shops_dir = shops.string();
        write(shops / "merchant.json", R"({"schema":"openkal.shop/v1",
            "npcIndex":62100,"items":[1036]})");
        write(items / "sword.json", R"({"index":62105,"name":"Owned Sword",
            "class":["weapon","sword"],"wear":1,"range":36,
            "specialty":{"hit":10000,"attack":[2000, 2000]}})");
        write(items / "practice_sword.json", R"({"index":62107,"name":"Owned Practice Sword",
            "class":["weapon","sword"],"wear":1,"range":36,
            "specialty":{"hit":10000,"attack":[1, 1]}})");
        write(items / "mask.json", R"({"index":1036,"name":"Owned Assassin Mask",
            "class":["defense","mask"],"limit":{"class":"knight","level":30},
            "wear":1,"assassin_mask":true,"stackable":0,"buy":100000})");
        write(items / "archer_mask.json", R"({"index":1037,"name":"Owned Archer Mask",
            "class":["defense","mask"],"limit":{"class":"archer","level":30},
            "wear":1,"assassin_mask":true,"stackable":0,"buy":100000})");
        write(items / "teleport.json", R"({"index":62001,"name":"Owned Scroll",
            "class":["general","etc"],"use":1,"specialty":{"teleport":{"loc":0}}})");
        write(items / "stack.json", R"({"index":62002,"name":"Owned Stack",
            "class":["general","etc"],"stackable":1})");
        for (int id : {1, 2, 3}) {
            auto character = store.load_character(id).value();
            character.exp = start_exp;
            character.hp = 100;
            character.money = 100000;
            require(store.save_character(character), "seed EXP, HP and Geon for PvP");
        }
        auto low_level = store.load_character(4).value();
        low_level.level = 29;
        low_level.str = 1;
        require(store.save_character(low_level), "seed a player below the mask level limit");
        auto practice_sword = store.load_items(4).front();
        practice_sword.index = 62107;
        require(store.update_item(practice_sword), "equip a non-lethal practice weapon");
        const auto seed = [&](int character, int iid, int index, int count, bool bound) {
            ItemRecord record;
            record.iid = iid;
            record.character_id = character;
            record.index = index;
            record.count = count;
            record.account_bound = bound;
            require(store.insert_item(record), "seed an owned PvP inventory instance");
        };
        seed(1, mask_iid, mask_index, 1, false);
        seed(1, reserve_iid, 23031, 1, true);
        seed(1, scroll_iid, scroll_index, 1, true);
        seed(1, stack_iid, stack_index, 12, false);
        seed(1, wrong_class_iid, archer_mask_index, 1, true);
        seed(4, below_level_iid, mask_index, 1, true);
        const auto item_is = [&](int character, int iid, int slot) {
            const auto owned = store.load_items(character);
            return std::any_of(owned.begin(), owned.end(), [&](const auto& entry) {
                return entry.iid == iid && entry.slot == slot;
            });
        };

        ServerHost host;
        require(host.start(config), "start PvP without guild or Castle War services");
        std::this_thread::sleep_for(150ms);
        std::uint32_t guard = 0, master = 0;
        auto a = enter_world(port, "castlefounder", 1, guard, master);
        guard = master = 0;
        auto b = enter_world(port, "castlechallenger", 2, guard, master);
        guard = master = 0;
        auto c = enter_world(port, "castlepartner", 3, guard, master);
        guard = master = 0;
        auto witness = enter_world(port, "pvpwitness", 4, guard, master);
        const auto migrated = store.load_items(1);
        require(std::any_of(migrated.begin(),
                            migrated.end(),
                            [&](const auto& entry) {
                                return entry.iid == reserve_iid && entry.index == mask_index;
                            }),
                "a previously purchased prototype mask becomes the matching class mask");
        a.send(ItemWear::frame_client_put_on(wrong_class_iid), "try another class's mask");
        require(a.response("do not meet the Assassin Mask requirements"),
                "the server rejects a mask for another class");
        witness.send(ItemWear::frame_client_put_on(below_level_iid), "try to equip at level 29");
        require(witness.response("do not meet the Assassin Mask requirements"),
                "the server rejects equipping below level 30");
        b.send(Shop::frame_item_list_c2s(guard), "open shop configured for Assassin Masks");
        require(
            b.receive(S2C::ItemList,
                      [](const auto& payload) {
                          ShopItemList list;
                          return Shop::parse_item_list_s2c(payload.data(), payload.size(), list) &&
                                 list.offers.size() == 1 && list.offers.front().index == mask_index;
                      }),
            "the regular shop lists the Assassin Mask");
        b.send(Shop::frame_buy_c2s(guard, mask_index, 1), "buy the Assassin Mask");
        require(wait_until([&] {
                    const auto bag = store.load_items(2);
                    return std::any_of(bag.begin(), bag.end(), [](const auto& item) {
                        return item.index == mask_index && item.count == 1;
                    });
                }),
                "regular NPC purchase grants the mask");
        const auto strike = [](Peer& source, const Peer& target) {
            source.send(Attack::frame_client(kCkPlayer, target.id()), "attack another player");
        };
        const auto defeat = [&](Peer& source, const Peer& target, const auto& defeated) {
            for (int attempt = 0; attempt < 8 && !defeated(); ++attempt) {
                strike(source, target);
                std::this_thread::sleep_for(850ms);
            }
            require(defeated(), "resolve the defeat through normal combat");
        };

        strike(a, b);
        require(!a.player_attack(b.id(), 200ms), "ordinary players cannot attack each other");
        const auto offers = [](const PlayerOptionsState& state, PlayerAction action) {
            return std::find(state.actions.begin(), state.actions.end(), action) !=
                   state.actions.end();
        };
        require(offers(a.player_options(b), PlayerAction::ChallengeDuel),
                "Player Options offers a challenge in safety");
        require(a.player_options(b, PlayerAction::ChallengeDuel).status ==
                    "Duel invitation sent to CastleLeader.",
                "challenge from Player Options");
        require(b.response("challenged you to a duel"), "receive the duel invitation");
        require(b.player_options(c, PlayerAction::AcceptDuel).status ==
                    "That player did not invite you to a duel.",
                "accepting with another player selected does not consume the invitation");
        const auto pending = b.player_options(a);
        require(offers(pending, PlayerAction::AcceptDuel) &&
                    offers(pending, PlayerAction::DeclineDuel) &&
                    !offers(pending, PlayerAction::ChallengeDuel),
                "Player Options offers responses for the actual inviter");
        require(b.player_options(a, PlayerAction::DeclineDuel).status ==
                        "Duel invitation declined." &&
                    a.response("declined your duel invitation"),
                "declining from Player Options notifies the inviter");
        require(b.player_options(a, PlayerAction::ChallengeDuel).status ==
                    "Duel invitation sent to CastleFounder.",
                "reversed challenge creates a fresh invitation");
        require(a.player_options(b, PlayerAction::AcceptDuel).status ==
                    "Duel started with CastleLeader.",
                "accept from Player Options in safety");
        require(a.pvp_status(b.id(), PvpRelation::Enemy, true),
                "duel opponent is hostile in the starter safe area");
        require(b.pvp_status(b.id(), PvpRelation::Ally, false),
                "duel participant sees their own side as allied");
        const auto has_hp = [](bool alive) {
            return [alive](const auto& payload) {
                UpdatePropertyFields update;
                return Property::parse_update_any(payload.data(), payload.size(), update) &&
                       update.property == static_cast<std::uint8_t>(PropertyType::CurHp) &&
                       (update.word_value > 0) == alive;
            };
        };
        bool collapsed = false;
        for (int attempt = 0; attempt < 8 && !collapsed; ++attempt) {
            strike(a, b);
            collapsed = b.receive(S2C::UpdateProperty, has_hp(false), 850ms);
        }
        require(collapsed, "a lethal duel hit sends zero HP to the defeated player");
        require(a.pvp_status(b.id(), PvpRelation::Enemy, false, false, true),
                "the opponent sees the duel defeat before automatic recovery");
        require(b.pvp_status(b.id(), PvpRelation::Ally, false, false, true),
                "the defeated player sees the same duel defeat");
        b.send(Skill::frame_client_revival(), "attempt village revival during the collapse");
        require(!b.receive(S2C::UpdateProperty, has_hp(true), 250ms),
                "the collapse window is not skipped by an early revival request");
        require(a.response("was defeated in the duel"), "the duel ends after the collapse");
        require(b.receive(S2C::UpdateProperty, has_hp(true)),
                "the defeated player recovers automatically");
        require(a.pvp_status(b.id(), PvpRelation::Neutral, false),
                "recovery clears the duel relationship");
        require(store.load_character(2)->exp == start_exp, "duel leaves EXP intact");
        strike(a, b);
        require(!a.player_attack(b.id(), 200ms), "attacks stop after the duel ends");
        c.send(Chatting::frame_c2s("/duel CastleLeader"), "challenge a player through chat");
        require(b.response("challenged you to a duel") &&
                    b.player_options(c, PlayerAction::DeclineDuel).status ==
                        "Duel invitation declined." &&
                    c.response("declined your duel invitation"),
                "chat invitations use the same responses as Player Options");

        a.send(ItemWear::frame_client_put_on(mask_iid), "put on the Assassin Mask");
        require(a.response("You are now an Assassin"), "mask activates");
        require(b.sees_assassin(a.id()), "other players receive the alias");
        require(b.pvp_status(a.id(), PvpRelation::Neutral, false, true),
                "the mask stays visible without granting attacks in the starter area");
        require(b.response(kAssassinWarning), "nearby players receive the warning");
        strike(a, b);
        require(!a.player_attack(b.id(), 200ms), "the starter area rejects Assassin attacks");
        a.send(ItemWear::frame_client_put_off(mask_iid), "attempt to remove the mask");
        require(a.response("cannot take off"), "the server blocks mask removal");
        a.send(UseItem::frame_c2s(scroll_iid),
               "attempt to use a city teleport scroll while masked");
        require(a.response("cannot teleport"), "the server rejects teleporting while masked");
        require(item_is(1, scroll_iid, 0) && item_is(1, mask_iid, 7),
                "the mask is durable and a blocked teleport does not consume the scroll");

        a.close();
        std::this_thread::sleep_for(300ms);
        guard = master = 0;
        a = enter_world(port, "castlefounder", 1, guard, master);
        require(b.sees_assassin(a.id()), "the disguise survives reconnect");
        a.send(Move::frame_client_end(56, 0, 0), "walk the Assassin outside the starter area");
        require(!b.pvp_status(a.id(), PvpRelation::Enemy, true, true),
                "the player remaining inside safety stays protected");
        b.send(Move::frame_client_end(56, 0, 0), "walk the victim out of safety");
        c.send(Move::frame_client_end(56, 0, 0), "walk the third player out of safety");
        witness.send(Move::frame_client_end(56, 0, 0), "walk the witness out of safety");
        require(b.pvp_status(a.id(), PvpRelation::Enemy, true, true),
                "an unmasked player is allowed to fight back outside safety");

        defeat(a, b, [&] {
            const auto saved = store.load_character(2);
            return saved && saved->hp == 0 && saved->pvp_death_exp_percent == 5;
        });
        require(store.load_character(2)->exp == start_exp,
                "victim EXP is unchanged until village revival");
        const auto revive = [](Peer& player) {
            PacketWriter writer;
            player.send(writer.frame(static_cast<std::uint8_t>(C2S::Revival)),
                        "start from the village");
        };
        revive(b);
        require(wait_until([&] {
                    const auto saved = store.load_character(2);
                    return saved && saved->hp > 0 && saved->pvp_death_exp_percent == 0 &&
                           saved->exp == start_exp - level_exp * 5 / 100;
                }),
                "village revival charges exactly five percent once to the victim");

        require(c.read_assassin_list(master).entries.empty(),
                "a duel and an unmasked defeat leave the board empty");
        strike(witness, a);
        require(witness.player_attack(a.id(), 3s, 1),
                "a different player damages the Assassin first");
        defeat(c, a, [&] {
            const auto saved = store.load_character(1);
            return saved && saved->hp == 0 && saved->pvp_death_exp_percent == 15;
        });
        require(!item_is(1, mask_iid, 7) && item_is(1, reserve_iid, 0) && item_is(1, scroll_iid, 0),
                "defeat destroys the worn mask and preserves bound inventory");
        if (!item_is(1, stack_iid, 0)) {
            require(witness.receive(S2C::CreateItem,
                                    [](const auto& payload) {
                                        PacketReader reader(payload);
                                        std::uint16_t index = 0;
                                        std::uint32_t iid = 0;
                                        return reader.read_u16(index) && reader.read_u32(iid) &&
                                               index == stack_index && iid == stack_iid;
                                    }),
                    "the whole rolled stack appears on the ground");
            PacketWriter pickup;
            pickup.write_u32(stack_iid);
            pickup.write_u32(0);
            pickup.write_u32(0);
            witness.send(pickup.frame(static_cast<std::uint8_t>(C2S::PickupItem)),
                         "a nearby player who did not land the kill claims the stack");
            require(wait_until([&] { return item_is(4, stack_iid, 0); }),
                    "there is no killer priority and the pickup restores the original instance");
            c.send(pickup.frame(static_cast<std::uint8_t>(C2S::PickupItem)),
                   "the killer requests the already picked-up stack");
            const auto inventory = store.load_items(4);
            const auto found = std::find_if(inventory.begin(),
                                            inventory.end(),
                                            [](const auto& item) { return item.iid == stack_iid; });
            require(found != inventory.end() && found->count == 12, "pickup awards the full stack");
        }
        for (auto* reader : {&c, &witness}) {
            const auto list = reader->read_assassin_list(master);
            require(list.entries.size() == 1 && list.entries.front().assassin == "CastleFounder" &&
                        list.entries.front().last_hitter == "CastlePartner",
                    "both clients see the real Assassin and the player who landed the fatal hit");
        }
        revive(a);
        require(wait_until([&] {
                    const auto saved = store.load_character(1);
                    return saved && saved->hp > 0 && saved->pvp_death_exp_percent == 0 &&
                           saved->exp == start_exp - level_exp * 15 / 100;
                }),
                "village revival charges exactly fifteen percent to the defeated Assassin");
        host.stop();

        PostgresStore reopened;
        require(reopened.open(config.database_dsn), "reopen the board database");
        const auto persisted = reopened.load_assassin_list();
        require(persisted && persisted->size() == 1 &&
                    persisted->front().assassin == "CastleFounder",
                "the PvP entry survives a server restart");
        auto record = reopened.load_character(1).value();
        record.hp = 0;
        ItemRecord worn_mask;
        worn_mask.character_id = record.id;
        worn_mask.index = mask_index;
        worn_mask.count = 1;
        worn_mask.slot = kAssassinMaskSlot;
        for (std::size_t index = 0; index <= kAssassinListLimit; ++index) {
            worn_mask.iid = 71000 + static_cast<int>(index);
            require(reopened.insert_item(worn_mask) &&
                        reopened.commit_assassin_death(
                            record, {worn_mask}, "Hitter" + std::to_string(index)),
                    "add a defeat to the capped board");
        }
        require(!reopened.commit_assassin_death(record, {worn_mask}, "Duplicate"),
                "a repeated death commit cannot add another entry");
        const std::unique_ptr<PGconn, decltype(&PQfinish)> db(
            PQconnectdb(config.database_dsn.c_str()), PQfinish);
        const std::unique_ptr<PGresult, decltype(&PQclear)> rows(
            PQexec(db.get(), "SELECT last_hitter FROM assassin_list ORDER BY id DESC"), PQclear);
        require(rows && PQresultStatus(rows.get()) == PGRES_TUPLES_OK &&
                    PQntuples(rows.get()) == static_cast<int>(kAssassinListLimit) &&
                    std::string_view(PQgetvalue(rows.get(), 0, 0)) == "Hitter50" &&
                    std::string_view(PQgetvalue(rows.get(), 49, 0)) == "Hitter1",
                "the table retains exactly the 50 newest entries and prunes older rows");
    }

    void exercise(const std::filesystem::path& root,
                  const std::string& dsn,
                  std::uint16_t port,
                  bool pvp) {
        using namespace protocol;
        using namespace server;
        for (const auto* child : {"maps", "monsters", "items", "npcs", "save"}) {
            std::filesystem::create_directories(root / child);
        }
        require(test::write_open_area((root / "maps/n_000_000.akarea").string()),
                "write synthetic siege map");
        write(
            root / "monsters/standard.json",
            R"({"index":62104,"name":"Owned Standard","stats":{"race":2,"level":1,"hp":1},"spawns":[]})");
        write(root / "monsters/blocker.json", R"({
            "index": 62106,
            "name": "Owned Blocker",
            "stats": {"race":3, "level":1, "hp":100},
            "spawns": [{"map":0, "rect":[39, 39, 39, 39], "max":1}]
        })");
        write(
            root / "items/sword.json",
            R"({"index":62105,"name":"Owned Sword","class":["weapon","sword"],"wear":1,"range":36})");
        for (const auto index : {62100, 62101, 62102, 62103}) {
            const int x = index == 62102 ? kCamp : kOutside;
            write(root / "npcs" / (std::to_string(index) + ".json"),
                  "{\"index\":" + std::to_string(index) +
                      ",\"name\":\"Owned NPC\",\"map\":0,\"html\":" +
                      std::to_string(pvp && index == kMaster ? kAssassinListDialog : 0) +
                      ",\"xy\":[" + std::to_string(x) + ", " + std::to_string(x + index - kGuard) +
                      ", 100]}");
        }
        const std::string guild_policy = R"({"schema":"openkal.guilds/v1","creation":{
            "minimum_level":1,"fee_zamogeon":0}})";
        write(root / "guilds.json", guild_policy);
        PostgresStore store;
        require(store.open(dsn), "open isolated Castle War schema");
        GuildRules guild_rules;
        std::string error;
        require(guild_rules.parse(platform::Json::parse(guild_policy), error),
                "load owned guild policy");
        GuildService guilds(guild_rules, store, store);
        require(guilds.restore(), "load guild state");
        seed_leader(store, guilds, 1, "castlefounder", "CastleFounder", "SiegeFounders");
        seed_leader(store,
                    guilds,
                    2,
                    "castlechallenger",
                    "CastleLeader",
                    "SiegeLeaders",
                    kLeaderStartX,
                    kOutside);
        seed_leader(store,
                    guilds,
                    3,
                    "castlepartner",
                    "CastlePartner",
                    "SiegePartners",
                    kOutside,
                    kPartnerStartY);

        ServerConfig config;
        config.maps_dir = (root / "maps").string();
        config.monsters_dir = (root / "monsters").string();
        config.items_dir = (root / "items").string();
        config.npcs_dir = (root / "npcs").string();
        config.save_dir = (root / "save").string();
        config.guild_rules_path = (root / "guilds.json").string();
        config.castle_war_path = (root / "castle.json").string();
        config.store = StoreBackend::Postgres;
        config.database_dsn = dsn;
        config.multiplayer = true;
        config.transport = TransportKind::Tcp;
        config.tcp_host = "127.0.0.1";
        config.tcp_port = port;
        config.test_control_token = kTestToken;
        config.spawn_x = config.spawn_y = kOutside;
        config.spawn_z = 100;
        config.map_radius_cells = 0;
        if (pvp) {
            seed_leader(store, guilds, 4, "pvpwitness", "PvpWitness", "PvpWitnesses");
            exercise_pvp(config, store, port);
            return;
        }
        {
            write(config.castle_war_path, R"({"schema":"openkal.castle_war/v1","enabled":true})");
            ServerHost invalid;
            require(!invalid.start(config), "reject enabled event with a missing siege layout");
        }
        const auto battle = std::time(nullptr) + 19;
        write(config.castle_war_path, create_rules(battle));
        ServerHost host;
        require(host.start(config), "start a dedicated TLS host with private Castle War settings");
        std::this_thread::sleep_for(150ms);
        std::uint32_t guard = 0, master = 0;
        auto peer = enter_world(port, "castlefounder", 1, guard, master, true);

        peer.send(Chatting::frame_c2s("/honor +=100"), "try the local Honor command online");
        require(peer.response("permission") || peer.response("available in single player"),
                "the multiplayer server rejects a local Honor adjustment");
        peer.talk(master, 990331);
        peer.send(Quest::frame_quest(Quest::make_key(kQuest, 2)),
                  "try admission before the window");
        require(peer.response("admission window is closed"), "early admission is denied");
        peer.talk(guard, 990330);
        peer.send(Quest::frame_quest(Quest::make_key(kQuest, 1)), "register attacking guild");
        require(peer.response("Registered for Castle War as attackers"),
                "guard accepts declaration");

        using WallClock = std::chrono::system_clock;
        std::this_thread::sleep_until(WallClock::from_time_t(battle - 7));
        peer.talk(master, 990331);
        peer.send(Quest::frame_quest(Quest::make_key(kQuest, 2)),
                  "request admission in the window");
        peer.teleport(kCamp, kCamp);
        peer.send(GameExit::frame_client_request(), "request ordinary character selection");
        require(peer.receive(S2C::AnsGameExit,
                             [](const auto& payload) {
                                 bool saved = true;
                                 return GameExit::parse_server_answer(
                                            payload.data(), payload.size(), saved) &&
                                        !saved;
                             }),
                "enrolled participant cannot leave via character selection");

        peer.capture(peer.standard(kFlagX, kCamp));
        peer.send(Chatting::frame_c2s("/castle status"), "read authoritative score");
        require(peer.response("Attack 50 / Defense 0"),
                "depleting flag health credits the attacking faction");
        peer.teleport(kOutside, kOutside);
        peer.send(Chatting::frame_c2s("/castle status"), "read finalized castle status");
        require(peer.response("owner: SiegeFounders"),
                "the winning guild owns the castle after the scheduled end");
        exercise_simulation(port, store, peer);
        host.stop();
        CastleWarRules rules;
        require(rules.load(config.castle_war_path, error), "reload the private Castle War rules");
        CastleWarService restored(std::move(rules), store, guilds);
        require(restored.restore() && restored.owner() == guilds.membership(1) &&
                    restored.honor(1) > 0,
                "PostgreSQL retains the winning guild and the founder's reward");
        const auto honor = store.load_honor(1, honor_week_start(battle));
        require(honor && honor->honor_points == restored.honor(1) && honor->reward_points == 0,
                "the shared Honor balance contains the regular round award only");
    }
}  // namespace

int main(int argc, char** argv) {
    const auto* base = std::getenv("OPENKAL_TEST_POSTGRES_DSN");
    if (!base || (argc != 2 && argc != 3)) {
        return 77;
    }
    const std::string schema = "openkal_castle_test_" + std::to_string(::getpid());
    using Connection = std::unique_ptr<PGconn, decltype(&PQfinish)>;
    using Result = std::unique_ptr<PGresult, decltype(&PQclear)>;
    Connection admin(PQconnectdb(base), PQfinish);
    if (!admin || PQstatus(admin.get()) != CONNECTION_OK) {
        return 1;
    }
    PQsetNoticeProcessor(admin.get(), [](void*, const char*) {}, nullptr);
    const auto sql = [&](const std::string& statement) {
        Result result(PQexec(admin.get(), statement.c_str()), PQclear);
        return result && PQresultStatus(result.get()) == PGRES_COMMAND_OK;
    };
    if (!sql("CREATE SCHEMA " + schema)) {
        return 1;
    }
    const auto root = std::filesystem::temp_directory_path() / schema;
    int status = 0;
    try {
        exercise(root,
                 std::string(base) + " options='-csearch_path=" + schema + "'",
                 static_cast<std::uint16_t>(std::stoi(argv[1])),
                 argc == 3 && std::string_view(argv[2]) == "pvp");
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << '\n';
        status = 1;
    }
    std::error_code error;
    std::filesystem::remove_all(root, error);
    return sql("DROP SCHEMA " + schema + " CASCADE") && !error ? status : 1;
}
