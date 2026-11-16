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
#include "openkal/protocol/Attack.h"
#include "openkal/protocol/ClockSync.h"
#include "openkal/protocol/CreatePlayer.h"
#include "openkal/protocol/FrameDrain.h"
#include "openkal/protocol/GameExit.h"
#include "openkal/protocol/Handshake.h"
#include "openkal/protocol/Move.h"
#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketReader.h"
#include "openkal/protocol/Property.h"
#include "openkal/protocol/Skill.h"
#include "openkal/protocol/TalentTree.h"
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
#include <sstream>
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
    enum class Scenario { Basic, LethalMagic, Counter };
    constexpr int kMonsterIndex = 61000;
    constexpr int kStart = 3600;

    void require(bool value, const std::string& message) {
        if (!value) {
            throw std::runtime_error(message);
        }
    }

    struct TemporaryDirectory {
        std::string path;
        TemporaryDirectory() {
            char pattern[] = "/tmp/openkal-world-events-XXXXXX";
            const auto* created = ::mkdtemp(pattern);
            require(created != nullptr, "create owned H9 directory");
            path = created;
        }
        ~TemporaryDirectory() {
            std::error_code error;
            std::filesystem::remove_all(path, error);
        }
    };

    std::string item_rows(std::vector<server::ItemRecord> rows) {
        std::sort(
            rows.begin(), rows.end(), [](const auto& a, const auto& b) { return a.iid < b.iid; });
        std::ostringstream out;
        for (const auto& r : rows) {
            out << r.iid << ',' << r.character_id << ',' << r.account_id << ',' << r.index << ','
                << r.prefix << ',' << r.count << ',' << r.slot << ',' << r.cur_end << ','
                << r.max_end << ',' << r.x_attack << ',' << r.x_magic << ',' << r.x_hit << ','
                << r.x_defense << ',' << r.x_dodge << ',' << r.upgrade_level << ','
                << r.upgrade_rate << ',' << r.mix_type << ',' << r.bof << ',' << r.qigong_perforated
                << ',' << r.qigong_effect << ',' << r.dragon_effect << ',' << r.random_stat_type
                << ',' << r.random_stat_value << ',' << r.transform_level << ',' << r.transform_exp
                << ',' << r.pet_expire_at << ',' << r.ride_expire_at << ',' << r.account_bound
                << ',' << r.enchant_protected << ',' << r.character_bound << '\n';
        }
        return out.str();
    }

    struct Fixture {
        TemporaryDirectory directory;
        server::ServerConfig config;
        std::string initial_items;
        explicit Fixture(Scenario scenario) {
            for (const auto* child :
                 {"maps", "monsters", "items", "skills", "quests", "starters", "npcs"}) {
                std::filesystem::create_directories(directory.path + '/' + child);
            }
            config.maps_dir = directory.path + "/maps";
            config.monsters_dir = directory.path + "/monsters";
            config.items_dir = directory.path + "/items";
            config.skills_dir = directory.path + "/skills";
            config.npcs_dir = directory.path + "/npcs";
            config.portals_path = directory.path + "/portals.json";
            config.mapinfo_path = directory.path + "/mapinfo.json";
            config.map_catalog_config = config.mapinfo_path;
            config.quests_dir = directory.path + "/quests";
            config.starters_dir = directory.path + "/starters";
            config.save_dir = directory.path + "/save";
            config.unix_path = directory.path + "/server.sock";
            config.spawn_x = config.spawn_y = kStart;
            config.spawn_z = 0;
            config.map_radius_cells = 0;
            config.spawn_load_radius_px = 8192;
            config.max_monsters = 1;
            config.multiplayer = false;
            config.store = server::StoreBackend::Sqlite;
            config.transport = server::TransportKind::Unix;
            require(test::write_open_area(config.maps_dir + "/n_000_000.akarea"),
                    "owned open non-village geometry");
            const auto write = [&](const std::string& relative, const std::string& text) {
                std::ofstream stream(directory.path + '/' + relative);
                stream << text;
                require(static_cast<bool>(stream), "write owned " + relative);
            };
            write("portals.json", R"({"portals":[]})");
            write("mapinfo.json", R"({"maps":[{"index":0,"kind":1,"xy":[0, 0]}]})");
            write(
                "items/1.json",
                R"({"index":1,"name":"Owned H9 marker","class":["general","etc"],"stackable":1})");
            const bool magic = scenario == Scenario::LethalMagic;
            const bool counter = scenario == Scenario::Counter;
            write(
                "monsters/61000.json",
                std::string(
                    R"({"index":61000,"name":"Owned H9 source","stats":{"race":1,"level":1,"ai":)") +
                    (magic ? "4" : "2") + ",\"hp\":" + (counter ? "0" : "100000") +
                    ",\"exp\":0,\"str\":0,\"int\":" + (magic ? "1000" : "0") +
                    R"(,"hit":10000,"range":24,"aspeed":2000,"attack":[0, 20, 20])" +
                    (magic ? R"(,"skill":[[29, 1]])" : "") +
                    R"(},"spawns":[{"map":0,"max":1,"cycle":3600,"rect":[128, 128, 129, 129]}]})");
            // A natural generic monster cast uses its ordinary pre-cast clock.
            write("skills/monster29.json",
                  R"({"class":-2,"index":29,"maxLevel":1,"delay":[100, 2000],"type":["use"]})");
            write(
                "skills/thief20.json",
                R"({"class":3,"index":20,"maxLevel":1,"mp":0,"delay":[0, 100],"type":["use"],"grade":1})");
            auto store = server::create_data_store(config.store);
            require(store && store->open(config.save_dir), "open owned H9 SQLite store");
            server::CharacterRecord character;
            character.name = "Owned H9 actor";
            character.clazz = counter ? 3 : 0;
            character.map = 0;
            character.map_x = character.map_y = kStart;
            character.map_z = 0;
            character.level = 1;
            character.str = counter ? 1000000 : 1;
            character.hth = counter ? 300 : magic ? 1 : 100;
            character.intl = character.wis = counter ? 1000 : 1;
            character.dex = 1;
            character.hp = counter ? 10000 : magic ? 1 : 1000;
            character.mp = counter ? 1000 : 50;
            require(store->ensure_default_player(
                        server::kDefaultAccountName, server::kDefaultAccountPassword, character),
                    "create owned local identity");
            const auto account = store->find_account(server::kDefaultAccountName);
            require(account.has_value(), "load owned account");
            const auto saved = store->list_characters(account->id);
            require(saved.size() == 1, "one owned shared SQLite character");
            character.id = saved.front().id;
            character.account_id = account->id;
            require(store->save_character(character), "seed explicit persisted stats and pose");
            config.character_id = character.id;
            server::ItemRecord item;
            item.iid = 1001;
            item.character_id = character.id;
            item.account_id = account->id;
            item.index = 1;
            item.count = 1;
            require(store->insert_item(item), "keep ordinary login from granting a starter kit");
            if (counter) {
                require(store->upsert_skill({character.id, 20, 1}), "learn owned Counter Attack");
            }
            initial_items = item_rows(store->load_items(character.id));
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

    enum class Kind { Basic, Prepare, Skill, Hp, Mp, Die, SelfBuff };
    struct Event {
        Kind kind = Kind::Basic;
        std::uint64_t sequence = 0;
        std::uint32_t source = 0;
        std::uint32_t target = 0;
        int damage = 0;
        int hp = -1;
        int mp = -1;
        int previous_mp = -1;
        int palsy = 0;
        std::uint8_t skill = 0;
        std::uint8_t level = 0;
        std::uint8_t flags = 0;
    };
    struct Peer {
        std::unique_ptr<server::ISession> session;
        Bytes rx;
        protocol::CreatePlayerFields self;
        std::map<std::uint32_t, MonsterCreate> monsters;
        std::vector<Event> events;
        std::uint64_t sequence = 0;
        int clocks = 0;
        int snapshots = 0;
        int move_acks = 0;
        int hp = -1;
        int max_hp = -1;
        int mp = -1;
        int x = kStart;
        int y = kStart;
        int exit_acks = 0;
        bool connected = false;
        bool exiting = false;

        void send(const Bytes& bytes) {
            require(connected && session && session->send_all(bytes), "send ordinary H9 request");
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
                require(status != protocol::FrameParseStatus::Invalid, "valid H9 framing");
                if (status == protocol::FrameParseStatus::Incomplete) {
                    break;
                }
                ++sequence;
                const auto opcode = static_cast<protocol::S2C>(type);
                if (opcode == protocol::S2C::Code) {
                    protocol::S2CCodeFields code;
                    require(protocol::Handshake::parse_code(payload.data(), payload.size(), code),
                            "parse ordinary challenge");
                    send(protocol::Handshake::build_answer(code.protocol_test_value));
                } else if (opcode == protocol::S2C::CreatePlayer) {
                    protocol::CreatePlayerFields value;
                    require(protocol::CreatePlayer::parse(payload.data(), payload.size(), value),
                            "parse ordinary player");
                    if (value.class_byte & 0x80) {
                        self = value;
                        x = value.x;
                        y = value.y;
                    }
                } else if (opcode == protocol::S2C::CreateMonster) {
                    const auto value = parse_monster(payload, sequence);
                    if (value.index == kMonsterIndex) {
                        monsters[value.id] = value;
                    }
                } else if (opcode == protocol::S2C::ClockSync) {
                    protocol::ClockSyncFields value;
                    require(protocol::ClockSync::parse(payload.data(), payload.size(), value),
                            "parse world readiness clock");
                    ++clocks;
                } else if (opcode == protocol::S2C::TalentTree) {
                    protocol::TalentState value;
                    require(protocol::parse_talent_state(payload.data(), payload.size(), value),
                            "parse real FIFO snapshot");
                    ++snapshots;
                } else if (opcode == protocol::S2C::MovePlayerEnd) {
                    std::uint32_t id = 0;
                    std::int8_t dx = 0, dy = 0, dz = 0;
                    require(protocol::Move::parse_player(
                                payload.data(), payload.size(), id, dx, dy, dz),
                            "parse ordinary movement echo");
                    if (id == self.id) {
                        x += dx;
                        y += dy;
                        ++move_acks;
                    }
                } else if (opcode == protocol::S2C::Attack) {
                    Event event;
                    event.kind = Kind::Basic;
                    event.sequence = sequence;
                    require(protocol::Attack::parse_server(payload.data(),
                                                           payload.size(),
                                                           event.source,
                                                           event.target,
                                                           event.damage,
                                                           event.palsy,
                                                           event.flags),
                            "parse full incoming attack");
                    events.push_back(event);
                } else if (opcode == protocol::S2C::Skill) {
                    protocol::S2CSkillFields skill;
                    require(protocol::Skill::parse_server(payload.data(), payload.size(), skill),
                            "parse full skill result");
                    Event event;
                    event.kind = skill.has_damage ? Kind::Skill : Kind::SelfBuff;
                    event.sequence = sequence;
                    event.source = skill.caster;
                    event.target = skill.target;
                    event.skill = skill.skill;
                    event.level = skill.level;
                    event.damage = skill.damage;
                    event.palsy = skill.palsy;
                    event.flags = skill.flags;
                    events.push_back(event);
                } else if (opcode == protocol::S2C::Action) {
                    Event event;
                    event.sequence = sequence;
                    if (protocol::Skill::parse_action_skill(payload.data(),
                                                            payload.size(),
                                                            event.source,
                                                            event.skill,
                                                            event.target)) {
                        event.kind = Kind::Prepare;
                        events.push_back(event);
                    } else {
                        std::uint8_t action = 0, argument = 0;
                        require(protocol::Skill::parse_action(
                                    payload.data(), payload.size(), event.source, action, argument),
                                "parse ordinary action");
                        if (action == protocol::kAtDie) {
                            event.kind = Kind::Die;
                            events.push_back(event);
                        }
                    }
                } else if (opcode == protocol::S2C::UpdateProperty) {
                    protocol::UpdatePropertyFields value;
                    require(
                        protocol::Property::parse_update_any(payload.data(), payload.size(), value),
                        "parse full authoritative property");
                    if (value.property == static_cast<std::uint8_t>(protocol::PropertyType::Hp) &&
                        value.kind == protocol::UpdatePropertyKind::Vital) {
                        hp = value.vital_cur;
                        max_hp = value.vital_max;
                        Event event;
                        event.kind = Kind::Hp;
                        event.sequence = sequence;
                        event.source = self.id;
                        event.hp = hp;
                        events.push_back(event);
                    } else if (value.property ==
                                   static_cast<std::uint8_t>(protocol::PropertyType::Mp) &&
                               value.kind == protocol::UpdatePropertyKind::Vital) {
                        mp = value.vital_cur;
                    } else if (value.property ==
                               static_cast<std::uint8_t>(protocol::PropertyType::CurMp)) {
                        Event event;
                        event.kind = Kind::Mp;
                        event.sequence = sequence;
                        event.source = self.id;
                        event.previous_mp = mp;
                        event.mp = value.word_value;
                        mp = value.word_value;
                        events.push_back(event);
                    } else if (value.property ==
                               static_cast<std::uint8_t>(protocol::PropertyType::CurHp)) {
                        hp = value.word_value;
                        Event event;
                        event.kind = Kind::Hp;
                        event.sequence = sequence;
                        event.source = self.id;
                        event.hp = hp;
                        events.push_back(event);
                    }
                } else if (opcode == protocol::S2C::AnsGameExit) {
                    bool saved = false;
                    require(exiting &&
                                protocol::GameExit::parse_server_answer(
                                    payload.data(), payload.size(), saved) &&
                                saved,
                            "GameExit returns a parsed saved=true acknowledgement");
                    ++exit_acks;
                }
            }
            connected = live;
            require(live || (exiting && exit_acks > 0), "H9 connection outlives its saved ACK");
            require(events.size() < 20000, "bounded H9 typed event storage");
        }
        bool ready() const {
            return self.id != 0 && clocks > 0 && hp >= 0 && max_hp > 0 && mp >= 0;
        }
    };

    template <typename Predicate>
    void wait_all(std::array<Peer, 2>& peers,
                  Predicate complete,
                  const std::string& label,
                  std::chrono::seconds timeout = std::chrono::seconds(10)) {
        const auto deadline = Clock::now() + timeout;
        while (!complete()) {
            for (auto& peer : peers) {
                peer.receive();
            }
            require(Clock::now() < deadline, "H9 watchdog: " + label);
            if (!complete()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        }
    }

    void fifo(std::array<Peer, 2>& peers) {
        const std::array<int, 2> before = {peers[0].snapshots, peers[1].snapshots};
        for (auto& peer : peers) {
            peer.send(protocol::frame_talent_request(protocol::TalentCommand::Snapshot));
        }
        wait_all(
            peers,
            [&] { return peers[0].snapshots > before[0] && peers[1].snapshots > before[1]; },
            "fresh read-only FIFO reply from both clients");
    }

    void move_peers(std::array<Peer, 2>& peers,
                    int x,
                    int y,
                    bool actor_only = false,
                    bool stop_on_actor_death = false) {
        for (int step = 0; peers[0].x != x || peers[0].y != y ||
                           (!actor_only && (peers[1].x != x || peers[1].y != y));
             ++step) {
            if (stop_on_actor_death && peers[0].hp == 0) {
                return;
            }
            require(step < 512, "finite ordinary approach path");
            const auto next_step = Clock::now() + std::chrono::milliseconds(100);
            std::array<int, 2> expected = {peers[0].move_acks, peers[1].move_acks};
            for (std::size_t i = 0; i < peers.size(); ++i) {
                if (actor_only && i != 0) {
                    continue;
                }
                const int dx = std::clamp(x - peers[i].x, -4, 4);
                const int dy = std::clamp(y - peers[i].y, -4, 4);
                if (dx != 0 || dy != 0) {
                    peers[i].send(protocol::Move::frame_client_end(
                        static_cast<std::int8_t>(dx), static_cast<std::int8_t>(dy), 0));
                    ++expected[i];
                }
            }
            wait_all(
                peers,
                [&] {
                    return (stop_on_actor_death && peers[0].hp == 0) ||
                           (peers[0].move_acks >= expected[0] && peers[1].move_acks >= expected[1]);
                },
                "acknowledged bounded movement while both peers drain");
            std::this_thread::sleep_until(next_step);
        }
    }

    const Event* find_event(const Peer& peer,
                            Kind kind,
                            std::uint32_t source,
                            std::uint32_t target,
                            bool positive = false) {
        const auto found =
            std::find_if(peer.events.begin(), peer.events.end(), [&](const Event& event) {
                return event.kind == kind && event.source == source && event.target == target &&
                       (!positive || event.damage > 0);
            });
        return found == peer.events.end() ? nullptr : &*found;
    }

    bool ordered_death(const Peer& peer, std::uint32_t source, std::uint32_t target, bool counter) {
        for (const auto& event : peer.events) {
            if (event.kind != Kind::Skill || event.damage <= 0 ||
                event.source != (counter ? target : source) ||
                event.target != (counter ? source : target) || event.skill != (counter ? 20 : 29)) {
                continue;
            }
            const auto dying = counter ? source : target;
            const auto die =
                std::find_if(peer.events.begin(), peer.events.end(), [&](const Event& action) {
                    return action.kind == Kind::Die && action.source == dying &&
                           action.sequence > event.sequence;
                });
            if (die != peer.events.end()) {
                return true;
            }
        }
        return false;
    }

    void await_saved(std::array<Peer, 2>& peers, std::size_t index) {
        const int previous = peers[index].exit_acks;
        peers[index].exiting = true;
        peers[index].send(protocol::GameExit::frame_client_request());
        wait_all(
            peers,
            [&] { return peers[index].exit_acks == previous + 1; },
            "fresh parsed saved acknowledgement");
    }

    void exit_saved(std::array<Peer, 2>& peers, std::size_t index) {
        await_saved(peers, index);
        peers[index].session->close();
        peers[index].connected = false;
    }

    const Event*
    first_after(const Peer& peer, Kind kind, std::uint32_t source, std::uint64_t sequence) {
        const auto found =
            std::find_if(peer.events.begin(), peer.events.end(), [&](const Event& event) {
                return event.kind == kind && event.source == source && event.sequence > sequence;
            });
        return found == peer.events.end() ? nullptr : &*found;
    }

    const Event* last_before(const Peer& peer,
                             Kind kind,
                             std::uint32_t source,
                             std::uint32_t target,
                             std::uint64_t sequence) {
        const Event* found = nullptr;
        for (const auto& event : peer.events) {
            if (event.kind == kind && event.source == source && event.target == target &&
                event.sequence < sequence) {
                found = &event;
            }
        }
        return found;
    }

    auto payload_key(const Event& event) {
        return std::make_tuple(event.kind,
                               event.source,
                               event.target,
                               event.damage,
                               event.palsy,
                               event.skill,
                               event.level,
                               event.flags);
    }

    void verify_damage_hp(const Peer& actor, const Event& damage) {
        const auto* before = last_before(actor, Kind::Hp, actor.self.id, 0, damage.sequence);
        const auto* after = first_after(actor, Kind::Hp, actor.self.id, damage.sequence);
        require(before && after && damage.damage > 0 &&
                    after->hp == std::max(0, before->hp - damage.damage),
                "incoming typed damage precedes its exact authoritative HP subtraction");
    }

    void cast_counter(std::array<Peer, 2>& peers) {
        const std::array<std::uint64_t, 2> before = {peers[0].sequence, peers[1].sequence};
        require(peers[0].hp > 0 && peers[0].mp >= 75,
                "Counter Attack has an alive actor and its literal 75 MP resource cost");
        peers[0].send(protocol::Skill::frame_client_self(20));
        const auto acknowledged = [&](const Peer& peer, std::uint64_t previous) {
            return std::any_of(peer.events.begin(), peer.events.end(), [&](const Event& event) {
                return event.kind == Kind::SelfBuff && event.source == peers[0].self.id &&
                       event.target == peers[0].self.id && event.skill == 20 && event.level == 1 &&
                       event.sequence > previous;
            });
        };
        wait_all(
            peers,
            [&] { return acknowledged(peers[0], before[0]) && acknowledged(peers[1], before[1]); },
            "fresh ordinary learned Counter Attack cast reaches both readers");
        const auto cast =
            std::find_if(peers[0].events.begin(), peers[0].events.end(), [&](const Event& event) {
                return event.kind == Kind::SelfBuff && event.source == peers[0].self.id &&
                       event.target == peers[0].self.id && event.skill == 20 && event.level == 1 &&
                       event.sequence > before[0];
            });
        require(cast != peers[0].events.end(), "actor has a fresh typed Counter acknowledgement");
        const auto* paid = last_before(peers[0], Kind::Mp, peers[0].self.id, 0, cast->sequence);
        require(paid && paid->sequence > before[0] && paid->previous_mp >= 75 &&
                    paid->mp == paid->previous_mp - 75,
                "accepted Counter Attack pays exactly 75 MP before its Skill acknowledgement");
        // This internal buff has no BState wire bit; the later positive counter is its active proof.
    }

    server::CharacterRecord verify_saved(const Fixture& fixture,
                                         const Peer& actor,
                                         Scenario scenario,
                                         const std::string& stage) {
        auto store = server::create_data_store(fixture.config.store);
        require(store && store->open(fixture.config.save_dir), "reopen durable rows " + stage);
        const auto persisted = store->load_character(fixture.config.character_id);
        require(persisted && persisted->map == 0 && persisted->map_x == actor.x &&
                    persisted->map_y == actor.y && persisted->hp >= 0 &&
                    persisted->hp <= persisted->max_hp,
                "saved actor retains acknowledged pose and bounded authoritative HP " + stage);
        if (scenario == Scenario::LethalMagic) {
            require(persisted->hp == 0, "lethal incoming skill is durably saved as HP 0 " + stage);
        } else {
            require(persisted->hp > 0,
                    "surviving incoming/counter actor is durably alive " + stage);
        }
        require(item_rows(store->load_items(fixture.config.character_id)) == fixture.initial_items,
                "all 30 persisted marker-item fields are conserved " + stage);
        return *persisted;
    }

    void exercise(Scenario scenario) {
        Fixture fixture(scenario);
        server::ServerHost host;
        require(host.start(fixture.config), "start ordinary H9 SQLite/Unix Host");
        std::array<Peer, 2> peers;
        for (auto& peer : peers) {
            const auto until = Clock::now() + std::chrono::seconds(8);
            while (!peer.session && Clock::now() < until) {
                peer.session = server::connect_unix(fixture.config.unix_path);
                if (!peer.session) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                }
            }
            require(peer.session != nullptr, "connect independent ordinary H9 reader");
            peer.connected = true;
            peer.send(protocol::Handshake::build_connect(0));
        }
        wait_all(
            peers,
            [&] { return peers[0].ready() && peers[1].ready(); },
            "both ordinary handshakes and full HP presentation");
        require(peers[0].self.id != peers[1].self.id, "two distinct runtime actor identities");
        if (scenario == Scenario::LethalMagic) {
            require(peers[0].max_hp < 150,
                    "owned level 1 HP stays below the literal skill29 minimum");
        }
        int counter_casts = 0;
        if (scenario == Scenario::Counter) {
            cast_counter(peers);
            ++counter_casts;
        }
        fifo(peers);
        // Both readers start in sight. Only the actor enters aggro range.
        wait_all(
            peers,
            [&] { return peers[0].monsters.size() == 1 && peers[1].monsters.size() == 1; },
            "both readers know the same ordinary monster before combat");
        const auto created = peers[0].monsters.begin()->second;
        require(peers[1].monsters.count(created.id) == 1 && created.index != 213,
                "generic non-Lord attacker has one shared identity");
        if (scenario == Scenario::Counter) {
            require(created.hp == 132 && created.max_hp == 132,
                    "owned HP 0 HTH 0 level 1 base revises to 132 before counter death");
        }
        fifo(peers);
        move_peers(peers,
                   static_cast<int>(created.x),
                   static_cast<int>(created.y),
                   true,
                   scenario == Scenario::LethalMagic);
        const auto actor = peers[0].self.id;
        if (scenario == Scenario::Basic) {
            wait_all(
                peers,
                [&] {
                    const auto* a = find_event(peers[0], Kind::Basic, created.id, actor, true);
                    const auto* b = find_event(peers[1], Kind::Basic, created.id, actor, true);
                    return a && b && first_after(peers[0], Kind::Hp, actor, a->sequence);
                },
                "natural positive basic damage and HP delivery");
            const auto* a = find_event(peers[0], Kind::Basic, created.id, actor, true);
            const auto* b = find_event(peers[1], Kind::Basic, created.id, actor, true);
            require(payload_key(*a) == payload_key(*b) && a->palsy == 0 &&
                        (a->flags & protocol::kAtkHit),
                    "both readers receive identical positive basic results");
            verify_damage_hp(peers[0], *a);
            require(peers[0].hp > 0, "weak basic attacker leaves the actor alive");
            // Ordinary acknowledged movement leaves the enemy's sight before saving.
            move_peers(peers, peers[0].x - 1024, peers[0].y, true);
        } else if (scenario == Scenario::LethalMagic) {
            wait_all(
                peers,
                [&] {
                    return ordered_death(peers[0], created.id, actor, false) &&
                           ordered_death(peers[1], created.id, actor, false);
                },
                "generic skill prepare/fire and spatial actor death");
            const auto* fire0 = find_event(peers[0], Kind::Skill, created.id, actor, true);
            const auto* fire1 = find_event(peers[1], Kind::Skill, created.id, actor, true);
            require(fire0 && fire1 && fire0->skill == 29 && fire0->level == 1 &&
                        fire0->damage >= 150 && fire0->damage <= 300 &&
                        (fire0->flags & protocol::kAtkHit) && fire0->palsy == 0 &&
                        payload_key(*fire0) == payload_key(*fire1),
                    "both readers receive the literal bounded monster magic 29 result");
            for (const auto& peer : peers) {
                const auto* prepare = find_event(peer, Kind::Prepare, created.id, actor);
                const auto* fire = find_event(peer, Kind::Skill, created.id, actor, true);
                const auto* die = first_after(peer, Kind::Die, actor, fire->sequence);
                require(prepare && prepare->skill == 29 && prepare->sequence < fire->sequence &&
                            die,
                        "both readers see prepare before damaging fire before player death");
            }
            verify_damage_hp(peers[0], *fire0);
            const auto* hp = first_after(peers[0], Kind::Hp, actor, fire0->sequence);
            const auto* die = first_after(peers[0], Kind::Die, actor, fire0->sequence);
            require(hp && hp->hp == 0 && die && hp->sequence < die->sequence,
                    "lethal skill sends exact CurHp 0 before player Die");
            require(std::count_if(peers[0].events.begin(),
                                  peers[0].events.end(),
                                  [&](const Event& event) {
                                      return event.kind == Kind::Die && event.source == actor;
                                  }) == 1,
                    "one lethal incoming event publishes one actor death");
        } else {
            // This exercises the real 35.1% counter gate, without seeding shared
            // production RNG.
            auto refresh = Clock::now() + std::chrono::seconds(20);
            const auto deadline = Clock::now() + std::chrono::seconds(120);
            const auto complete = [&] {
                return ordered_death(peers[0], created.id, actor, true) &&
                       ordered_death(peers[1], created.id, actor, true);
            };
            while (!complete()) {
                for (auto& peer : peers) {
                    peer.receive();
                }
                require(Clock::now() < deadline,
                        "H9 watchdog: natural counter gate and authoritative monster death");
                if (!complete() && Clock::now() >= refresh) {
                    cast_counter(peers);
                    ++counter_casts;
                    refresh = Clock::now() + std::chrono::seconds(20);
                }
                if (!complete()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                }
            }
            std::array<Event, 2> killing_counters;
            for (std::size_t i = 0; i < peers.size(); ++i) {
                const auto* die = first_after(peers[i], Kind::Die, created.id, 0);
                require(die != nullptr, "counter kill has a typed non-Maguni death");
                const auto* counter =
                    last_before(peers[i], Kind::Skill, actor, created.id, die->sequence);
                require(counter && counter->skill == 20 && counter->level == 1 &&
                            counter->damage > 0 && counter->palsy == 0 && counter->flags == 0,
                        "killing Counter Attack retains the complete ordinary wire result");
                const auto* primary =
                    last_before(peers[i], Kind::Basic, created.id, actor, counter->sequence);
                require(primary && primary->damage > 0 && primary->sequence < counter->sequence,
                        "primary basic result precedes the counter result and monster Die");
                killing_counters[i] = *counter;
                if (i == 0) {
                    verify_damage_hp(peers[i], *primary);
                    const auto* hp = first_after(peers[i], Kind::Hp, actor, primary->sequence);
                    require(hp && die->sequence < hp->sequence,
                            "counter result and monster death precede the victim HP update");
                }
            }
            require(payload_key(killing_counters[0]) == payload_key(killing_counters[1]),
                    "both readers agree on the killing counter fields");
            const auto snapshot = host.monster_snapshot();
            const auto entity =
                std::find_if(snapshot.begin(), snapshot.end(), [&](const auto& monster) {
                    return monster.id == created.id;
                });
            require(entity == snapshot.end() || entity->hp == 0,
                    "counter death corresponds to authoritative dead-or-removed world "
                    "state");
        }
        fifo(peers);
        // Let observer EOF retirement finish between two fresh actor save replies.
        exit_saved(peers, 1);
        await_saved(peers, 0);
        await_saved(peers, 0);
        require(peers[0].connected && peers[0].session->alive() && peers[0].exit_acks == 2,
                "actor stays connected through both fresh saved acknowledgements");
        (void) verify_saved(fixture, peers[0], scenario, "before actor close and Host stop");
        peers[0].session->close();
        peers[0].connected = false;
        host.stop();
        const auto persisted = verify_saved(fixture, peers[0], scenario, "after Host stop");
        std::cout << "H9 ordinary scenario=" << static_cast<int>(scenario)
                  << " monster=" << created.id << " actor=" << actor
                  << " observer=" << peers[1].self.id << " final_hp=" << persisted.hp
                  << " actor_pose=" << persisted.map_x << ',' << persisted.map_y
                  << " counter_casts=" << counter_casts << " saved_acks=" << peers[0].exit_acks
                  << ',' << peers[1].exit_acks << '\n';
    }
}  // namespace

int main() {
    try {
        exercise(Scenario::Basic);
        exercise(Scenario::LethalMagic);
        exercise(Scenario::Counter);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "H9 live witness failed: " << error.what() << '\n';
        return 1;
    }
}
