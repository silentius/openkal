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
#include "openkal/protocol/ClockSync.h"
#include "openkal/protocol/CreatePlayer.h"
#include "openkal/protocol/FrameDrain.h"
#include "openkal/protocol/GameExit.h"
#include "openkal/protocol/Handshake.h"
#include "openkal/protocol/MapState.h"
#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketReader.h"
#include "openkal/protocol/Quest.h"
#include "openkal/protocol/Skill.h"
#include "openkal/server/ServerHost.h"
#include "openkal/server/net/Transport.h"
#include "openkal/server/store/DataStore.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

namespace {
    using namespace openkal;
    using Bytes = std::vector<std::uint8_t>;
    using Clock = std::chrono::steady_clock;
    constexpr int kMap = 3;
    constexpr int kX = 2048;
    constexpr int kY = 2048;
    constexpr int kMonumentIndex = 62000;
    constexpr int kLordIndex = 213;
    // The loaded level-1, HTH-0 definition adds 132 to its authored HP base.
    constexpr int kExpectedLordMaxHp = 1132;

    void require(bool condition, const std::string& message) {
        if (!condition) {
            throw std::runtime_error(message);
        }
    }

    struct TemporaryDirectory {
        std::string path;

        TemporaryDirectory() {
            char pattern[] = "/tmp/openkal-doggebi-lord-XXXXXX";
            const char* created = ::mkdtemp(pattern);
            require(created != nullptr, "create owned Doggebi fixture directory");
            path = created;
        }

        ~TemporaryDirectory() {
            std::error_code error;
            std::filesystem::remove_all(path, error);
        }
    };

    struct Fixture {
        TemporaryDirectory directory;
        server::ServerConfig config;
        int monuments = 8;

        void write(const std::string& relative, const std::string& value) const {
            std::ofstream stream(directory.path + "/" + relative);
            stream << value;
            require(static_cast<bool>(stream), "write owned " + relative);
        }

        explicit Fixture(bool incomplete_layout) : monuments(incomplete_layout ? 7 : 8) {
            for (const char* child : {"maps", "monsters", "items", "npcs", "quests", "starters"}) {
                std::filesystem::create_directories(directory.path + "/" + child);
            }
            config.maps_dir = directory.path + "/maps";
            config.monsters_dir = directory.path + "/monsters";
            config.items_dir = directory.path + "/items";
            config.npcs_dir = directory.path + "/npcs";
            config.quests_dir = directory.path + "/quests";
            config.starters_dir = directory.path + "/starters";
            config.save_dir = directory.path + "/save";
            config.unix_path = directory.path + "/server.sock";
            config.map_catalog_config = directory.path + "/mapinfo.json";
            config.mapinfo_path = config.map_catalog_config;
            config.portals_path = directory.path + "/portals.json";
            config.spawn_x = kX;
            config.spawn_y = kY;
            config.spawn_z = 0;
            config.map_radius_cells = 0;
            config.max_monsters = 0;
            config.transport = server::TransportKind::Unix;
            config.store = server::StoreBackend::Sqlite;
            config.multiplayer = false;
            require(test::write_open_area(config.maps_dir + "/n_000_000.akarea"),
                    "write owned non-village walkable area");
            write("mapinfo.json", R"({"maps":[{"index":3,"kind":1,"xy":[0, 0]}]})");
            write("portals.json", R"({"portals":[]})");
            write(
                "items/1.json",
                R"({"index":1,"name":"Owned H5 scenario item","class":["general","etc"],"stackable":1})");
            write(
                "monsters/213.json",
                R"({"index":213,"name":"Owned H5 Lord","stats":{"race":0,"level":1,"hp":1000,"str":4,"int":8,"attack":[0, 1, 1],"magic":[3, 3]},"spawns":[]})");
            write(
                "monsters/232.json",
                R"({"index":232,"name":"Owned H5 spirit","stats":{"race":0,"level":1,"hp":20},"spawns":[]})");
            for (int slot = 0; slot < monuments; ++slot) {
                const int x = kX + (slot - 4) * 16;
                write("npcs/" + std::to_string(kMonumentIndex + slot) + ".json",
                      "{\"index\":" + std::to_string(kMonumentIndex + slot) +
                          ",\"name\":\"Owned H5 monument " + std::to_string(slot) +
                          "\",\"map\":3,\"xy\":[" + std::to_string(x) +
                          ", 2080, 0],\"scenario\":[" + std::to_string(slot) + ", 1],\"shape\":7}");
            }
            auto store = server::create_data_store(config.store);
            require(store && store->open(config.save_dir), "open owned SQLite fixture");
            server::CharacterRecord draft;
            draft.name = "Owned H5 actor";
            draft.map = kMap;
            draft.map_x = kX;
            draft.map_y = kY;
            draft.map_z = 0;
            require(store->ensure_default_player(
                        server::kDefaultAccountName, server::kDefaultAccountPassword, draft),
                    "create owned local account and character");
            const auto account = store->find_account(server::kDefaultAccountName);
            require(account.has_value(), "load owned local account");
            auto characters = store->list_characters(account->id);
            require(characters.size() == 1, "one owned persisted local character");
            auto character = characters.front();
            character.map = kMap;
            character.map_x = kX;
            character.map_y = kY;
            character.map_z = 0;
            require(store->save_character(character), "persist positive-map walkable pose");
            config.character_id = character.id;
            for (const auto& item : store->load_items(character.id)) {
                require(store->delete_item(item.iid), "clear any implicit starter inventory");
            }
            server::ItemRecord item;
            item.iid = 1001;
            item.character_id = character.id;
            item.account_id = account->id;
            item.index = 1;
            item.count = 1;
            require(store->insert_item(item), "seed an owned item to preserve the saved pose");
        }
    };

    struct MonsterCreate {
        std::uint32_t id = 0;
        std::uint16_t index = 0;
        std::uint32_t x = 0;
        std::uint32_t y = 0;
        std::uint16_t direction = 0;
        std::int32_t hp = 0;
        std::int32_t max_hp = 0;
        std::uint32_t gstate = 0;
        std::uint32_t mstate = 0;
        std::uint64_t guild = 0;
        std::string guild_name;
        std::uint8_t race = 0;
        std::uint32_t guild_id = 0;
        std::string name;
        std::uint64_t sequence = 0;
    };

    MonsterCreate parse_monster(const Bytes& payload, std::uint64_t sequence) {
        protocol::PacketReader reader(payload);
        MonsterCreate value;
        value.sequence = sequence;
        require(reader.read_u16(value.index) && reader.read_u32(value.id) &&
                    reader.read_u32(value.x) && reader.read_u32(value.y) &&
                    reader.read_u16(value.direction) && reader.read_i32(value.hp) &&
                    reader.read_i32(value.max_hp) && reader.read_u32(value.gstate) &&
                    reader.read_u32(value.mstate) && reader.read_u64(value.guild) &&
                    reader.read_string(value.guild_name) && reader.read_u8(value.race) &&
                    reader.read_u32(value.guild_id) && reader.read_string(value.name) &&
                    reader.remaining() == 0,
                "parse all authored CreateMonster fields");
        return value;
    }

    struct CastObservation {
        std::uint32_t caster = 0;
        std::uint8_t skill = 0;
        std::uint32_t target = 0;
        std::uint64_t sequence = 0;
        bool fire = false;
        bool has_damage = false;
        std::uint8_t level = 0;
    };

    struct Peer {
        std::unique_ptr<server::ISession> session;
        Bytes rx;
        protocol::CreatePlayerFields self;
        std::map<int, protocol::CreateNpcInfo> npcs;
        std::vector<MonsterCreate> lords;
        std::vector<CastObservation> casts;
        std::vector<protocol::MapStateFields> map_states;
        std::vector<std::pair<std::string, std::string>> chats;
        std::uint64_t sequence = 0;
        int clocks = 0;
        int exit_acks = 0;
        bool connected = false;
        bool exiting = false;

        void send(const Bytes& frame) {
            require(session && connected && session->send_all(frame), "send ordinary request");
        }

        void receive() {
            if (!connected) {
                return;
            }
            const bool live = session->recv_into(rx);
            protocol::FrameDrain drain(rx);
            Bytes payload;
            std::uint8_t type = 0;
            for (;;) {
                const auto status = drain.pop(type, payload);
                require(status != protocol::FrameParseStatus::Invalid, "valid server framing");
                if (status == protocol::FrameParseStatus::Incomplete) {
                    break;
                }
                ++sequence;
                const auto opcode = static_cast<protocol::S2C>(type);
                if (opcode == protocol::S2C::Code) {
                    protocol::S2CCodeFields code;
                    require(protocol::Handshake::parse_code(payload.data(), payload.size(), code),
                            "parse world challenge");
                    send(protocol::Handshake::build_answer(code.protocol_test_value));
                } else if (opcode == protocol::S2C::CreatePlayer) {
                    protocol::CreatePlayerFields player;
                    require(protocol::CreatePlayer::parse(payload.data(), payload.size(), player),
                            "parse player presentation");
                    if (player.class_byte & 0x80) {
                        self = player;
                    }
                } else if (opcode == protocol::S2C::CreateNpc) {
                    protocol::CreateNpcInfo npc;
                    require(protocol::Quest::parse_create_npc(payload.data(), payload.size(), npc),
                            "parse monument presentation");
                    if (npc.index >= kMonumentIndex && npc.index < kMonumentIndex + 8) {
                        npcs[npc.index] = npc;
                    }
                } else if (opcode == protocol::S2C::CreateMonster) {
                    const auto monster = parse_monster(payload, sequence);
                    if (monster.index == kLordIndex) {
                        lords.push_back(monster);
                    }
                } else if (opcode == protocol::S2C::ClockSync) {
                    protocol::ClockSyncFields clock;
                    require(protocol::ClockSync::parse(payload.data(), payload.size(), clock),
                            "parse world-ready clock");
                    ++clocks;
                } else if (opcode == protocol::S2C::Chatting) {
                    std::string speaker, message;
                    require(protocol::Chatting::parse_s2c(
                                payload.data(), payload.size(), speaker, message),
                            "parse command reply");
                    chats.emplace_back(std::move(speaker), std::move(message));
                } else if (opcode == protocol::S2C::Action) {
                    CastObservation cast;
                    if (protocol::Skill::parse_action_skill(
                            payload.data(), payload.size(), cast.caster, cast.skill, cast.target)) {
                        cast.sequence = sequence;
                        casts.push_back(cast);
                    }
                } else if (opcode == protocol::S2C::Skill) {
                    protocol::S2CSkillFields skill;
                    require(protocol::Skill::parse_server(payload.data(), payload.size(), skill),
                            "parse skill fire");
                    casts.push_back({skill.caster,
                                     skill.skill,
                                     skill.target,
                                     sequence,
                                     true,
                                     skill.has_damage,
                                     skill.level});
                } else if (opcode == protocol::S2C::MapState) {
                    protocol::MapStateFields state;
                    require(protocol::MapState::parse(payload.data(), payload.size(), state),
                            "parse authoritative scenario state");
                    map_states.push_back(state);
                } else if (opcode == protocol::S2C::AnsGameExit) {
                    bool saved = false;
                    require(exiting &&
                                protocol::GameExit::parse_server_answer(
                                    payload.data(), payload.size(), saved) &&
                                saved,
                            "ordinary exit confirms durable save");
                    ++exit_acks;
                }
            }
            connected = live;
            require(live || (exiting && exit_acks == 1),
                    "connection survives until its complete saved exit acknowledgement");
            require(casts.size() < 2000 && map_states.size() < 2000 && lords.size() < 10,
                    "bounded live witness observations");
        }

        bool server_chat(const std::string& message) const {
            return std::find(chats.begin(),
                             chats.end(),
                             std::make_pair(std::string("Server"), message)) != chats.end();
        }

        bool ready(int monuments) const {
            return self.id != 0 && clocks > 0 && static_cast<int>(npcs.size()) == monuments;
        }

        bool lord_countdown() const {
            return std::any_of(map_states.begin(), map_states.end(), [](const auto& state) {
                return state.map == kMap &&
                       state.phase == static_cast<std::uint8_t>(server::DoggebiRitualPhase::Idle) &&
                       state.active_mask == 0 && state.timer == protocol::MapStateTimer::Lord &&
                       state.timer_seconds >= 1 && state.timer_seconds <= 3600;
            });
        }
    };

    template <typename Predicate>
    void wait_all(std::array<Peer, 2>& peers,
                  Predicate done,
                  const std::string& label,
                  std::chrono::seconds duration = std::chrono::seconds(8)) {
        const auto until = Clock::now() + duration;
        while (!done()) {
            for (auto& peer : peers) {
                peer.receive();
            }
            require(Clock::now() < until, "Doggebi witness watchdog: " + label);
            if (!done()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        }
    }

    using CastKey = std::tuple<std::uint32_t, std::uint8_t, std::uint32_t>;

    std::set<CastKey>
    completed_casts(const Peer& peer, std::uint32_t lord, const std::set<std::uint32_t>& actors) {
        std::map<CastKey, std::uint64_t> prepared;
        std::set<CastKey> result;
        for (const auto& cast : peer.casts) {
            if (cast.caster != lord || !actors.count(cast.target)) {
                continue;
            }
            const CastKey key{cast.caster, cast.skill, cast.target};
            if (!cast.fire) {
                prepared[key] = cast.sequence;
            } else if (prepared.count(key) && prepared.at(key) < cast.sequence &&
                       !cast.has_damage && cast.level == 1) {
                result.insert(key);
            }
        }
        return result;
    }

    bool shared_completed_cast(const std::array<Peer, 2>& peers,
                               std::uint32_t lord,
                               const std::set<std::uint32_t>& actors) {
        const auto first = completed_casts(peers[0], lord, actors);
        const auto second = completed_casts(peers[1], lord, actors);
        return std::any_of(
            first.begin(), first.end(), [&](const auto& key) { return second.count(key) != 0; });
    }

    void exercise(bool incomplete_layout) {
        Fixture fixture(incomplete_layout);
        server::ServerHost host;
        require(host.start(fixture.config), "start ordinary SQLite/Unix Host");
        std::array<Peer, 2> peers;
        for (auto& peer : peers) {
            const auto deadline = Clock::now() + std::chrono::seconds(8);
            while (!peer.session && Clock::now() < deadline) {
                peer.session = server::connect_unix(fixture.config.unix_path);
                if (!peer.session) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                }
            }
            require(peer.session != nullptr, "connect independent Unix reader");
            peer.connected = true;
            peer.send(protocol::Handshake::build_connect(0));
        }
        wait_all(
            peers,
            [&] { return peers[0].ready(fixture.monuments) && peers[1].ready(fixture.monuments); },
            "both ordinary handshakes and all authored monuments");
        require(peers[0].self.id != peers[1].self.id, "independent runtime actors");
        for (const auto& peer : peers) {
            require(peer.self.x == kX && peer.self.y == kY && peer.self.z == 0,
                    "ordinary login preserves the owned positive-map pose");
            std::set<std::uint32_t> npc_ids;
            for (int slot = 0; slot < fixture.monuments; ++slot) {
                const auto& npc = peer.npcs.at(kMonumentIndex + slot);
                require(npc.id != 0 && npc_ids.insert(npc.id).second,
                        "distinct runtime monument identities");
                require(npc.shape == 7 && npc.x == kX + (slot - 4) * 16 && npc.y == 2080 &&
                            npc.z == 0,
                        "each monument presents its independently authored location and shape");
            }
        }
        for (int slot = 0; slot < fixture.monuments; ++slot) {
            require(peers[0].npcs.at(kMonumentIndex + slot).id ==
                        peers[1].npcs.at(kMonumentIndex + slot).id,
                    "both readers observe the same runtime monument identities");
        }
        for (auto& peer : peers) {
            peer.send(protocol::Chatting::frame_c2s("/godmode"));
        }
        wait_all(
            peers,
            [&] {
                return peers[0].server_chat("God mode on.") && peers[1].server_chat("God mode on.");
            },
            "ordinary local god-mode command replies");
        peers[0].send(protocol::Chatting::frame_c2s("/spawn monster 213"));
        wait_all(
            peers,
            [&] {
                return peers[0].server_chat("Spawned Owned H5 Lord (213).") &&
                       !peers[0].lords.empty() && !peers[1].lords.empty();
            },
            "authored Lord creation on both readers");
        const std::uint32_t lord = peers[0].lords.front().id;
        require(lord != 0 && peers[1].lords.front().id == lord,
                "both readers receive the same runtime Lord");
        for (const auto& peer : peers) {
            const auto& created = peer.lords.front();
            require(created.index == kLordIndex && created.hp == kExpectedLordMaxHp &&
                        created.max_hp == kExpectedLordMaxHp && created.name == "Owned H5 Lord",
                    "real Lord presentation contains the authored identity and resources");
        }
        const std::set<std::uint32_t> actors{peers[0].self.id, peers[1].self.id};
        if (incomplete_layout) {
            const auto until = Clock::now() + std::chrono::seconds(3);
            while (Clock::now() < until) {
                for (auto& peer : peers) {
                    peer.receive();
                    require(!peer.lord_countdown(),
                            "incomplete layout cannot broadcast Lord timer");
                    require(completed_casts(peer, lord, actors).empty(),
                            "incomplete layout cannot satisfy scheduled Lord cast witness");
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        } else {
            wait_all(
                peers,
                [&] {
                    return peers[0].lord_countdown() && peers[1].lord_countdown() &&
                           shared_completed_cast(peers, lord, actors);
                },
                "shared ordered Lord prepare/fire and countdown",
                std::chrono::seconds(20));
        }
        for (std::size_t i = 0; i < peers.size(); ++i) {
            const auto& peer = peers[i];
            std::cout << "reader=" << i << " actor=" << peer.self.id << " lord=" << lord
                      << " monuments=" << peer.npcs.size()
                      << " lord_timer=" << peer.lord_countdown()
                      << " completed_casts=" << completed_casts(peer, lord, actors).size() << '\n';
            const auto& created = peer.lords.front();
            std::cout << "create reader=" << i << " index=" << created.index << " id=" << created.id
                      << " xy=" << created.x << ',' << created.y
                      << " direction=" << created.direction << " hp=" << created.hp << '/'
                      << created.max_hp << " gstate=" << created.gstate
                      << " mstate=" << created.mstate << " guild=" << created.guild
                      << " guild_name=" << created.guild_name
                      << " race=" << static_cast<int>(created.race)
                      << " guild_id=" << created.guild_id << " name=" << created.name << '\n';
            for (const auto& [index, npc] : peer.npcs) {
                std::cout << "monument reader=" << i << " index=" << index << " id=" << npc.id
                          << '\n';
            }
            for (const auto& cast : peer.casts) {
                if (cast.caster == lord) {
                    std::cout << "cast reader=" << i << " sequence=" << cast.sequence
                              << " caster=" << cast.caster
                              << " skill=" << static_cast<int>(cast.skill)
                              << " target=" << cast.target << " fire=" << cast.fire << '\n';
                }
            }
            for (const auto& state : peer.map_states) {
                if (state.timer == protocol::MapStateTimer::Lord) {
                    std::cout << "countdown reader=" << i << " map=" << static_cast<int>(state.map)
                              << " phase=" << static_cast<int>(state.phase)
                              << " mask=" << static_cast<int>(state.active_mask)
                              << " timer=" << static_cast<int>(state.timer)
                              << " seconds=" << state.timer_seconds << '\n';
                    break;
                }
            }
        }
        for (const std::size_t i : {std::size_t{1}, std::size_t{0}}) {
            peers[i].exiting = true;
            peers[i].send(protocol::GameExit::frame_client_request());
            wait_all(
                peers, [&] { return peers[i].exit_acks == 1; }, "complete durable exit response");
            peers[i].session->close();
            peers[i].connected = false;
            std::cout << "exit reader=" << i << " saved=true\n";
        }
        host.stop();
    }
}  // namespace

int main(int argc, char** argv) {
    try {
        require(argc == 1 || (argc == 2 && std::string(argv[1]) == "--incomplete-layout"),
                "only the optional incomplete-layout negative control is supported");
        exercise(argc == 2);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Doggebi Lord scheduler witness failed: " << error.what() << '\n';
        return 1;
    }
}
