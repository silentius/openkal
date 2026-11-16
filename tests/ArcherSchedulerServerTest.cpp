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
#include "openkal/map/Coords.h"
#include "openkal/protocol/ArcherSkill.h"
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
#include <optional>
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
    constexpr int kActorX = 1024;
    constexpr int kY = 1024;
    constexpr int kTargetX = 1120;
    constexpr int kObserverX = 1472;
    constexpr int kTargetDefinition = 62001;
    constexpr int kTargetHp = 1000132;

    void require(bool condition, const std::string& message) {
        if (!condition) {
            throw std::runtime_error(message);
        }
    }

    struct TemporaryDirectory {
        std::string path;
        TemporaryDirectory() {
            char pattern[] = "/tmp/openkal-archer-scheduler-XXXXXX";
            const char* result = ::mkdtemp(pattern);
            require(result != nullptr, "create owned Archer fixture directory");
            path = result;
        }
        ~TemporaryDirectory() {
            std::error_code error;
            std::filesystem::remove_all(path, error);
        }
    };

    auto item_fields(const server::ItemRecord& v) {
        return std::make_tuple(v.iid,
                               v.character_id,
                               v.account_id,
                               v.index,
                               v.prefix,
                               v.count,
                               v.slot,
                               v.cur_end,
                               v.max_end,
                               v.x_attack,
                               v.x_magic,
                               v.x_hit,
                               v.x_defense,
                               v.x_dodge,
                               v.upgrade_level,
                               v.upgrade_rate,
                               v.mix_type,
                               v.bof,
                               v.qigong_perforated,
                               v.qigong_effect,
                               v.dragon_effect,
                               v.random_stat_type,
                               v.random_stat_value,
                               v.transform_level,
                               v.transform_exp,
                               v.pet_expire_at,
                               v.ride_expire_at,
                               v.account_bound,
                               v.enchant_protected,
                               v.character_bound);
    }

    struct Fixture {
        TemporaryDirectory directory;
        server::ServerConfig config;
        std::vector<server::ItemRecord> original_items;
        std::int32_t character_id = 0;

        void write(const std::string& relative, const std::string& value) const {
            std::ofstream stream(directory.path + "/" + relative);
            stream << value;
            require(static_cast<bool>(stream), "write owned " + relative);
        }

        Fixture() {
            for (const char* child :
                 {"maps", "items", "monsters", "skills", "npcs", "quests", "starters"}) {
                std::filesystem::create_directories(directory.path + "/" + child);
            }
            config.maps_dir = directory.path + "/maps";
            config.items_dir = directory.path + "/items";
            config.monsters_dir = directory.path + "/monsters";
            config.skills_dir = directory.path + "/skills";
            config.npcs_dir = directory.path + "/npcs";
            config.quests_dir = directory.path + "/quests";
            config.starters_dir = directory.path + "/starters";
            config.save_dir = directory.path + "/save";
            config.unix_path = directory.path + "/server.sock";
            config.map_catalog_config = directory.path + "/mapinfo.json";
            config.mapinfo_path = config.map_catalog_config;
            config.portals_path = directory.path + "/portals.json";
            config.spawn_x = kActorX;
            config.spawn_y = kY;
            config.spawn_z = 0;
            config.map_radius_cells = 0;
            config.max_monsters = 1;
            config.transport = server::TransportKind::Unix;
            config.store = server::StoreBackend::Sqlite;
            config.multiplayer = false;
            require(test::write_open_area(config.maps_dir + "/n_000_000.akarea"),
                    "write owned walkable overworld area");
            write("mapinfo.json", R"({"maps":[{"index":0,"kind":0,"xy":[0, 0]}]})");
            write("portals.json", R"({"portals":[]})");
            write(
                "items/1.json",
                R"({"index":1,"name":"Owned scheduler bow","wear":1,"class":["weapon","bow"],"range":300,"specialty":{"hp":50000,"mp":50000}})");
            write(
                "monsters/62001.json",
                R"({"index":62001,"name":"Owned scheduler target","stats":{"race":1,"ai":0,"level":1,"hp":1000000,"range":256,"aspeed":1000000,"sight":[0, 0],"attack":{"min":1,"max":1},"defense":[0, 0]},"spawns":[{"map":0,"area":1,"max":1,"cycle":1000,"rect":[35, 32, 35, 32]}]})");
            for (const int skill : {42, 47, 49}) {
                write(
                    "skills/" + std::to_string(skill) + ".json",
                    "{\"class\":\"archer\",\"index\":" + std::to_string(skill) +
                        ",\"maxLevel\":1,\"mp\":1,\"levelLimit\":90,\"grade\":1,\"type\":[\"use\"],\"manualLearn\":true,\"delay\":[0, 200]}");
            }
            auto store = server::create_data_store(config.store);
            require(store && store->open(config.save_dir), "open owned SQLite fixture");
            server::CharacterRecord draft;
            draft.name = "Owned Archer scheduler";
            draft.clazz = 2;
            draft.level = 90;
            draft.specialty = 23;
            draft.map = 0;
            draft.map_x = kActorX;
            draft.map_y = kY;
            draft.map_z = 0;
            draft.hp = draft.mp = 1000000;
            draft.str = draft.hth = draft.intl = draft.wis = draft.dex = 100;
            draft.weapon_index = 1;
            require(store->ensure_default_player(
                        server::kDefaultAccountName, server::kDefaultAccountPassword, draft),
                    "create owned local account and Archer");
            const auto account = store->find_account(server::kDefaultAccountName);
            require(account.has_value(), "load owned local account");
            auto characters = store->list_characters(account->id);
            require(characters.size() == 1, "one owned persisted character");
            draft.id = characters.front().id;
            draft.account_id = account->id;
            require(store->save_character(draft), "persist complete owned Archer seed");
            character_id = draft.id;
            config.character_id = character_id;
            for (const auto& item : store->load_items(character_id)) {
                require(store->delete_item(item.iid), "remove any implicit starter item");
            }
            server::ItemRecord bow;
            bow.iid = 1001;
            bow.character_id = character_id;
            bow.account_id = account->id;
            bow.index = 1;
            bow.count = 1;
            bow.slot = 1;
            require(store->insert_item(bow), "persist an ordinary equipped bow");
            original_items = store->load_items(character_id);
            require(original_items.size() == 1, "owned bow is the whole initial inventory");
            for (const int index : {42, 47, 49}) {
                server::SkillRecord skill;
                skill.character_id = character_id;
                skill.skill_index = index;
                skill.level = 1;
                require(store->upsert_skill(skill), "persist ordinary learned skill rank");
            }
        }

        void require_inventory(server::IDataStore& store) const {
            auto items = store.load_items(character_id);
            require(items.size() == original_items.size(),
                    "scheduler casts preserve inventory row count");
            const auto by_iid = [](const auto& a, const auto& b) { return a.iid < b.iid; };
            auto expected = original_items;
            std::sort(items.begin(), items.end(), by_iid);
            std::sort(expected.begin(), expected.end(), by_iid);
            for (std::size_t i = 0; i < items.size(); ++i) {
                require(item_fields(items[i]) == item_fields(expected[i]),
                        "scheduler casts preserve every persisted inventory metadata field");
            }
            const auto skills = store.load_skills(character_id);
            require(skills.size() == 3, "all three learned skill rows remain");
            for (const int index : {42, 47, 49}) {
                require(std::count_if(skills.begin(),
                                      skills.end(),
                                      [&](const auto& value) {
                                          return value.skill_index == index && value.level == 1;
                                      }) == 1,
                        "ordinary casts do not alter learned ranks");
            }
        }
    };

    struct MonsterPresentation {
        std::uint32_t id = 0;
        std::uint16_t index = 0;
        std::uint32_t x = 0;
        std::uint32_t y = 0;
        std::int32_t hp = 0;
        std::int32_t max_hp = 0;
        std::uint64_t sequence = 0;
        Clock::time_point received{};
    };

    MonsterPresentation parse_monster(const Bytes& payload, std::uint64_t sequence) {
        protocol::PacketReader reader(payload);
        MonsterPresentation value;
        std::uint16_t direction = 0;
        std::uint32_t gstate = 0, mstate = 0, guild_id = 0;
        std::uint64_t guild = 0;
        std::uint8_t race = 0;
        std::string guild_name, name;
        require(
            reader.read_u16(value.index) && reader.read_u32(value.id) && reader.read_u32(value.x) &&
                reader.read_u32(value.y) && reader.read_u16(direction) &&
                reader.read_i32(value.hp) && reader.read_i32(value.max_hp) &&
                reader.read_u32(gstate) && reader.read_u32(mstate) && reader.read_u64(guild) &&
                reader.read_string(guild_name) && reader.read_u8(race) &&
                reader.read_u32(guild_id) && reader.read_string(name) && reader.remaining() == 0,
            "decode the complete real monster presentation");
        value.sequence = sequence;
        value.received = Clock::now();
        return value;
    }

    struct ObservedSkill {
        protocol::S2CSkillFields value;
        std::uint64_t sequence = 0;
        Clock::time_point received{};
    };

    struct ResourceUpdate {
        std::int32_t value = 0;
        std::uint64_t sequence = 0;
    };

    struct Peer {
        std::unique_ptr<server::ISession> session;
        Bytes rx;
        protocol::CreatePlayerFields self{};
        std::map<std::uint32_t, MonsterPresentation> monsters;
        std::vector<ObservedSkill> skills;
        std::vector<protocol::SkillRankEntry> ranks;
        std::map<std::uint32_t, std::uint64_t> removed;
        std::vector<ResourceUpdate> mp_updates;
        std::vector<std::pair<std::uint8_t, Bytes>> wire;
        std::vector<Bytes> sent;
        std::uint64_t sequence = 0;
        int clocks = 0;
        int hp = 0, max_hp = 0, mp = 0, max_mp = 0;
        int hp_at_exit = 0, mp_at_exit = 0;
        int exit_acks = 0;
        int expected_exit_acks = 0;
        bool connected = false;
        bool exiting = false;

        void send(const Bytes& frame) {
            require(connected && session && session->send_all(frame), "send ordinary wire request");
            sent.push_back(frame);
        }

        void receive() {
            if (!connected) {
                return;
            }
            const bool live = session->recv_into(rx);
            protocol::FrameDrain frames(rx);
            Bytes payload;
            std::uint8_t type = 0;
            for (;;) {
                const auto status = frames.pop(type, payload);
                require(status != protocol::FrameParseStatus::Invalid, "valid server framing");
                if (status == protocol::FrameParseStatus::Incomplete) {
                    break;
                }
                ++sequence;
                wire.emplace_back(type, payload);
                const auto opcode = static_cast<protocol::S2C>(type);
                if (opcode == protocol::S2C::Code) {
                    protocol::S2CCodeFields code;
                    require(protocol::Handshake::parse_code(payload.data(), payload.size(), code),
                            "parse world challenge");
                    send(protocol::Handshake::build_answer(code.protocol_test_value));
                } else if (opcode == protocol::S2C::CreatePlayer) {
                    protocol::CreatePlayerFields value;
                    require(protocol::CreatePlayer::parse(payload.data(), payload.size(), value),
                            "parse player presentation");
                    if ((value.class_byte & 0x80) != 0) {
                        self = value;
                    }
                } else if (opcode == protocol::S2C::ClockSync) {
                    protocol::ClockSyncFields value;
                    require(protocol::ClockSync::parse(payload.data(), payload.size(), value),
                            "parse world-ready clock");
                    ++clocks;
                } else if (opcode == protocol::S2C::SkillInfo) {
                    require(
                        protocol::Skill::parse_skill_info(payload.data(), payload.size(), ranks),
                        "parse ordinary learned ranks");
                } else if (opcode == protocol::S2C::CreateMonster) {
                    const auto value = parse_monster(payload, sequence);
                    monsters[value.id] = value;
                } else if (opcode == protocol::S2C::RemoveMonster) {
                    protocol::PacketReader reader(payload);
                    std::uint32_t id = 0;
                    require(reader.read_u32(id) && reader.remaining() == 0,
                            "parse exact controller removal");
                    removed[id] = sequence;
                } else if (opcode == protocol::S2C::Skill) {
                    protocol::S2CSkillFields value;
                    require(protocol::Skill::parse_server(payload.data(), payload.size(), value),
                            "parse complete ordinary skill result");
                    skills.push_back({value, sequence, Clock::now()});
                } else if (opcode == protocol::S2C::UpdateProperty) {
                    protocol::UpdatePropertyFields value;
                    require(
                        protocol::Property::parse_update_any(payload.data(), payload.size(), value),
                        "parse authoritative resource property");
                    if (value.kind == protocol::UpdatePropertyKind::Vital) {
                        if (value.property ==
                            static_cast<std::uint8_t>(protocol::PropertyType::Hp)) {
                            hp = value.vital_cur;
                            max_hp = value.vital_max;
                        } else if (value.property ==
                                   static_cast<std::uint8_t>(protocol::PropertyType::Mp)) {
                            mp = value.vital_cur;
                            max_mp = value.vital_max;
                        }
                    } else if (value.kind == protocol::UpdatePropertyKind::Word) {
                        if (value.property ==
                            static_cast<std::uint8_t>(protocol::PropertyType::CurHp)) {
                            hp = value.word_value;
                        } else if (value.property ==
                                   static_cast<std::uint8_t>(protocol::PropertyType::CurMp)) {
                            mp = value.word_value;
                            mp_updates.push_back({mp, sequence});
                        }
                    }
                } else if (opcode == protocol::S2C::MovePlayerOn ||
                           opcode == protocol::S2C::MovePlayerEnd) {
                    std::uint32_t id = 0;
                    std::int8_t dx = 0, dy = 0, dz = 0;
                    require(protocol::Move::parse_player(
                                payload.data(), payload.size(), id, dx, dy, dz),
                            "parse acknowledged ordinary movement");
                    if (id == self.id) {
                        self.x += dx;
                        self.y += dy;
                        self.z += dz;
                    }
                } else if (opcode == protocol::S2C::AnsGameExit) {
                    bool saved = false;
                    require(exiting && exit_acks < expected_exit_acks &&
                                protocol::GameExit::parse_server_answer(
                                    payload.data(), payload.size(), saved) &&
                                saved,
                            "fresh GameExit response confirms a complete successful save");
                    ++exit_acks;
                    hp_at_exit = hp;
                    mp_at_exit = mp;
                }
            }
            connected = live;
            require(live || (exiting && exit_acks == expected_exit_acks),
                    "reader remains healthy until the complete successful exit acknowledgement");
            require(wire.size() < 10000 && rx.size() < 1024 * 1024, "bounded diagnostic capture");
        }

        bool ready() const {
            if (self.id == 0 || clocks == 0 || (self.class_byte & 0x7f) != 2 || self.wear[0] != 1 ||
                max_mp <= 380 || mp != max_mp || hp <= 0) {
                return false;
            }
            for (const int index : {42, 47, 49}) {
                if (std::count_if(ranks.begin(), ranks.end(), [&](const auto& value) {
                        return value.index == index && value.level == 1;
                    }) != 1) {
                    return false;
                }
            }
            return true;
        }

        std::optional<MonsterPresentation> created(int index) const {
            for (const auto& [id, value] : monsters) {
                (void) id;
                if (value.index == index) {
                    return value;
                }
            }
            return std::nullopt;
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
            require(Clock::now() < until, "Archer scheduler watchdog: " + label);
            if (!done()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        }
    }

    void move_confirmed(std::array<Peer, 2>& peers, std::size_t index, int delta) {
        const int expected = peers[index].self.x + delta;
        require(delta != 0 && delta >= -64 && delta <= 64,
                "ordinary movement respects the real displacement gate");
        peers[index].send(protocol::Move::frame_client_end(static_cast<std::int8_t>(delta), 0, 0));
        wait_all(
            peers, [&] { return peers[index].self.x == expected; }, "authoritative movement echo");
    }

    auto skill_fields(const protocol::S2CSkillFields& value) {
        return std::make_tuple(value.skill,
                               value.caster,
                               value.target,
                               value.has_damage,
                               value.level,
                               value.damage,
                               value.palsy,
                               value.flags);
    }

    std::vector<ObservedSkill>
    damage_hits(const Peer& peer, int skill, std::uint32_t actor, std::uint32_t target) {
        std::vector<ObservedSkill> result;
        for (const auto& value : peer.skills) {
            if (value.value.skill == skill && value.value.caster == actor &&
                value.value.target == target && value.value.has_damage) {
                result.push_back(value);
            }
        }
        return result;
    }

    void require_shared_damage_and_hp(server::ServerHost& host,
                                      std::array<Peer, 2>& peers,
                                      int skill,
                                      std::uint32_t target) {
        wait_all(
            peers,
            [&] {
                const auto a = damage_hits(peers[0], skill, peers[0].self.id, target);
                const auto b = damage_hits(peers[1], skill, peers[0].self.id, target);
                if (a.empty() || a.size() != b.size()) {
                    return false;
                }
                std::int64_t total = 0;
                for (std::size_t i = 0; i < a.size(); ++i) {
                    if (skill_fields(a[i].value) != skill_fields(b[i].value) ||
                        a[i].value.damage <= 0) {
                        return false;
                    }
                    total += a[i].value.damage;
                }
                const auto snapshot = host.monster_snapshot();
                const auto found =
                    std::find_if(snapshot.begin(), snapshot.end(), [&](const auto& value) {
                        return value.id == target;
                    });
                return found != snapshot.end() && found->hp > 0 && found->max_hp == kTargetHp &&
                       found->x == kTargetX && found->y == kY && found->hp == kTargetHp - total;
            },
            "both complete damage streams conserve the actual living monster HP");
    }

    void print_wire(const std::array<Peer, 2>& peers, int skill) {
        constexpr char hex[] = "0123456789abcdef";
        for (std::size_t i = 0; i < peers.size(); ++i) {
            for (const auto& frame : peers[i].sent) {
                std::cout << "skill=" << skill << " reader=" << i << " client_frame=";
                for (const auto value : frame) {
                    std::cout << hex[value >> 4] << hex[value & 15];
                }
                std::cout << '\n';
            }
            for (const auto& [type, payload] : peers[i].wire) {
                std::cout << "skill=" << skill << " reader=" << i
                          << " opcode=" << static_cast<int>(type) << " payload=";
                for (const auto value : payload) {
                    std::cout << hex[value >> 4] << hex[value & 15];
                }
                std::cout << '\n';
            }
        }
    }

    void exercise(int skill) {
        Fixture fixture;
        server::ServerHost host;
        require(host.start(fixture.config), "start real owned SQLite/Unix scheduler fixture");
        std::array<Peer, 2> peers;
        for (auto& peer : peers) {
            const auto connection_deadline = Clock::now() + std::chrono::seconds(8);
            while (!peer.session && Clock::now() < connection_deadline) {
                peer.session = server::connect_unix(fixture.config.unix_path);
                if (!peer.session) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                }
            }
            require(peer.session != nullptr, "connect ordinary independent reader");
            peer.connected = true;
            peer.send(protocol::Handshake::build_connect(0));
        }
        wait_all(
            peers,
            [&] {
                return peers[0].ready() && peers[1].ready() &&
                       peers[0].created(kTargetDefinition) && peers[1].created(kTargetDefinition);
            },
            "both handshakes, full resources, equipped bow, learned ranks and actual target");
        require(peers[0].self.id != peers[1].self.id,
                "SQLite local sessions have distinct runtime actors");
        const auto target = peers[0].created(kTargetDefinition)->id;
        require(target != 0 && peers[1].created(kTargetDefinition)->id == target,
                "both readers receive the same real target");
        for (const auto& peer : peers) {
            const auto created = *peer.created(kTargetDefinition);
            require(created.x == kTargetX && created.y == kY && created.hp == kTargetHp &&
                        created.max_hp == kTargetHp,
                    "owned non-Maguni target is presented at its fixed authored pose and HP");
        }
        while (peers[1].self.x < kObserverX) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            move_confirmed(peers, 1, std::min(3, kObserverX - peers[1].self.x));
        }
        require(peers[0].self.x == kActorX && peers[1].self.x == kObserverX &&
                    peers[1].self.x - peers[0].self.x > 300 && peers[1].self.x - kTargetX > 300 &&
                    coords::in_sight(peers[1].self.x, peers[1].self.y, kTargetX, kY) &&
                    coords::in_sight(
                        peers[1].self.x, peers[1].self.y, peers[0].self.x, peers[0].self.y),
                "healthy observer remains in sight outside every damaging field and owner radius");
        const auto before_sequence = peers[0].sequence;
        const int full_mp = peers[0].mp;
        const int cost = skill == 42 ? 116 : 380;
        const auto request_time = Clock::now();
        peers[0].send(skill == 49
                          ? protocol::Skill::frame_client_target(49, protocol::kCkMonster, target)
                          : protocol::Skill::frame_client_ground(
                                static_cast<std::uint8_t>(skill), kTargetX, kY));
        wait_all(
            peers,
            [&] {
                const bool spent = std::any_of(
                    peers[0].mp_updates.begin(), peers[0].mp_updates.end(), [&](const auto& value) {
                        return value.sequence > before_sequence && value.value == full_mp - cost;
                    });
                const bool accepted = std::any_of(
                    peers[0].skills.begin(), peers[0].skills.end(), [&](const auto& value) {
                        return value.sequence > before_sequence && value.value.skill == skill &&
                               value.value.caster == peers[0].self.id;
                    });
                return spent && accepted;
            },
            "ordinary learned cast is accepted and reconciles its literal resource cost");
        const int controller_definition = skill == 42 ? protocol::kArcherPoisonCloudControllerDef
                                                      : protocol::kArcherArrowRainControllerDef;
        std::uint32_t controller = 0;
        if (skill != 49) {
            wait_all(
                peers,
                [&] {
                    return peers[0].created(controller_definition) &&
                           peers[1].created(controller_definition);
                },
                "both real Magic controller presentations");
            controller = peers[0].created(controller_definition)->id;
            require(controller != target &&
                        controller == peers[1].created(controller_definition)->id,
                    "both readers agree on the independent scheduled controller identity");
        }
        wait_all(
            peers,
            [&] {
                return damage_hits(peers[0], skill, peers[0].self.id, target).size() >= 2 &&
                       damage_hits(peers[1], skill, peers[0].self.id, target).size() >= 2;
            },
            "successive actual scheduled damage reaches both healthy readers",
            std::chrono::seconds(10));
        require_shared_damage_and_hp(host, peers, skill, target);
        if (skill == 47) {
            std::array<std::uint64_t, 2> before_movement{};
            for (std::size_t i = 0; i < peers.size(); ++i) {
                const auto hits = damage_hits(peers[i], skill, peers[0].self.id, target);
                require(!peers[i].removed.count(controller) && hits.size() < 5,
                        "Rain is still active with pulses remaining before ordinary movement");
                before_movement[i] = peers[i].sequence;
            }
            move_confirmed(peers, 0, 1);
            wait_all(
                peers,
                [&] {
                    return peers[0].removed.count(controller) && peers[1].removed.count(controller);
                },
                "movement cancels and removes the exact Arrow Rain controller");
            require(Clock::now() - request_time <
                        std::chrono::milliseconds(protocol::kArcherArrowRainDurationMs),
                    "movement removal is observed before the ordinary ten-second field expiry");
            // Five uninterrupted pulses start immediately and then repeat every two seconds.
            // Reject a delayed setup rather than mistake natural exhaustion for cancellation.
            constexpr int earliest_exhaustion_ms =
                (protocol::kArcherArrowRainTicks - 1) * protocol::kArcherArrowRainTickMs;
            require(Clock::now() - request_time <
                        std::chrono::milliseconds(earliest_exhaustion_ms - 1),
                    "invalid Rain causality precondition: removal must precede the earliest "
                    "uninterrupted fifth pulse, allowing one millisecond of clock quantization");
            for (std::size_t i = 0; i < peers.size(); ++i) {
                const auto& peer = peers[i];
                require(peer.removed.at(controller) > before_movement[i],
                        "both readers receive exact Rain removal after the movement request");
                const auto hits = damage_hits(peer, skill, peers[0].self.id, target);
                require(hits.size() < 5, "Rain is removed while uninterrupted pulses still remain");
                // Local SQLite mode echoes movement only to its sender, while controller
                // removal and damage are delivered to both readers.
                std::uint64_t acknowledged_movement = 0;
                for (std::size_t frame = static_cast<std::size_t>(before_movement[i]);
                     frame < peer.wire.size();
                     ++frame) {
                    const auto& [type, payload] = peer.wire[frame];
                    if (type != static_cast<std::uint8_t>(protocol::S2C::MovePlayerEnd)) {
                        continue;
                    }
                    std::uint32_t id = 0;
                    std::int8_t dx = 0, dy = 0, dz = 0;
                    require(protocol::Move::parse_player(
                                payload.data(), payload.size(), id, dx, dy, dz),
                            "decode complete movement in the causal Rain interval");
                    if (id == peers[0].self.id && dx == 1 && dy == 0 && dz == 0) {
                        acknowledged_movement = frame + 1;
                        break;
                    }
                }
                if (i == 0) {
                    require(acknowledged_movement > before_movement[i] &&
                                acknowledged_movement < peer.removed.at(controller),
                            "the actor receives its ordinary movement echo before Rain removal");
                    require(std::all_of(hits.begin(),
                                        hits.end(),
                                        [&](const auto& hit) {
                                            return hit.sequence < acknowledged_movement;
                                        }),
                            "the actor receives no Rain damage after its accepted movement echo");
                }
                for (const auto& hit : hits) {
                    require(hit.sequence < peer.removed.at(controller),
                            "all drained Rain damage precedes its exact controller removal");
                }
            }
            require_shared_damage_and_hp(host, peers, skill, target);
        } else if (skill == 42) {
            move_confirmed(peers, 1, 1);
            wait_all(
                peers,
                [&] {
                    return peers[0].removed.count(controller) && peers[1].removed.count(controller);
                },
                "ordinary twelve-second Cloud controller expiry",
                std::chrono::seconds(16));
            wait_all(
                peers,
                [&] {
                    for (const auto& peer : peers) {
                        const auto hits = damage_hits(peer, skill, peers[0].self.id, target);
                        if (std::none_of(hits.begin(), hits.end(), [&](const auto& hit) {
                                return hit.sequence > peer.removed.at(controller);
                            })) {
                            return false;
                        }
                    }
                    return true;
                },
                "retained Cloud status damage continues after the separate controller expiry phase");
            require_shared_damage_and_hp(host, peers, skill, target);
        } else {
            for (const auto& peer : peers) {
                const auto hits = damage_hits(peer, skill, peers[0].self.id, target);
                require(
                    hits.size() == 2 && hits[0].sequence < hits[1].sequence &&
                        hits[0].value.damage > 0 && hits[1].value.damage > 0,
                    "the surviving fused target receives a separate direct hit and delayed blast");
            }
        }
        for (const auto& peer : peers) {
            require(std::none_of(peer.skills.begin(),
                                 peer.skills.end(),
                                 [&](const auto& value) {
                                     return value.value.has_damage &&
                                            value.value.target == peers[1].self.id;
                                 }),
                    "ordinary observer placement avoids incidental PvP damage");
        }
        for (const std::size_t index : {std::size_t{1}, std::size_t{0}}) {
            peers[index].exiting = true;
            // SQLite peers share one character: the first actor barrier lets the observer
            // retirement persist finish; the second saves the actor after that stale writer.
            const int saves = index == 0 ? 2 : 1;
            for (int save = 0; save < saves; ++save) {
                ++peers[index].expected_exit_acks;
                peers[index].send(protocol::GameExit::frame_client_request());
                wait_all(
                    peers,
                    [&] { return peers[index].exit_acks == peers[index].expected_exit_acks; },
                    "fresh saved=true exit barrier");
            }
            if (index == 0) {
                auto audit = server::create_data_store(fixture.config.store);
                require(audit && audit->open(fixture.config.save_dir),
                        "reopen SQLite after actual saved acknowledgement");
                const auto saved = audit->load_character(fixture.character_id);
                require(
                    saved && saved->hp == peers[0].hp_at_exit && saved->mp == peers[0].mp_at_exit &&
                        saved->map_x == peers[0].self.x && saved->map_y == kY &&
                        saved->clazz == 2 && saved->level == 90,
                    "the ordinary successful save contains the exact preceding authoritative resource and pose state");
                fixture.require_inventory(*audit);
            }
            peers[index].session->close();
            peers[index].connected = false;
        }
        host.stop();
        auto final_store = server::create_data_store(fixture.config.store);
        require(final_store && final_store->open(fixture.config.save_dir),
                "reopen final owned inventory persistence");
        fixture.require_inventory(*final_store);
        print_wire(peers, skill);
        std::cout << "ordinary-scheduler skill=" << skill << " actor=" << peers[0].self.id
                  << " target=" << target << " controller=" << controller
                  << " actor_hp_saved=" << peers[0].hp_at_exit
                  << " actor_mp_saved=" << peers[0].mp_at_exit << '\n';
    }
}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 1) {
            for (const int skill : {42, 47, 49}) {
                exercise(skill);
            }
        } else {
            require(argc == 3 && std::string(argv[1]) == "--skill",
                    "optional bringup argument is --skill 42, 47 or 49");
            const std::string value = argv[2];
            require(value == "42" || value == "47" || value == "49",
                    "finite ordinary scheduler skill selection");
            exercise(std::stoi(value));
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Archer scheduler witness failed: " << error.what() << '\n';
        return 1;
    }
}
