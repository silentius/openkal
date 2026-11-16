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
#include "openkal/protocol/Attack.h"
#include "openkal/protocol/MageSkill.h"
#include "openkal/protocol/ClockSync.h"
#include "openkal/protocol/CreatePlayer.h"
#include "openkal/protocol/FrameDrain.h"
#include "openkal/protocol/GameExit.h"
#include "openkal/protocol/Handshake.h"
#include "openkal/protocol/Move.h"
#include "openkal/protocol/BuffState.h"
#include "openkal/protocol/MState.h"
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
            char pattern[] = "/tmp/openkal-mage-scheduler-XXXXXX";
            const char* result = ::mkdtemp(pattern);
            require(result != nullptr, "create owned Mage fixture directory");
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
            config.max_monsters = 2;
            config.transport = server::TransportKind::Unix;
            config.store = server::StoreBackend::Sqlite;
            config.multiplayer = false;
            require(test::write_open_area(config.maps_dir + "/n_000_000.akarea"),
                    "write owned walkable overworld area");
            write("mapinfo.json", R"({"maps":[{"index":0,"kind":0,"xy":[0, 0]}]})");
            write("portals.json", R"({"portals":[]})");
            write(
                "items/1.json",
                R"({"index":1,"name":"Owned scheduler staff","wear":1,"class":["weapon","staff"],"range":300,"specialty":{"hp":50000,"mp":50000}})");
            write(
                "monsters/62001.json",
                R"({"index":62001,"name":"Owned scheduler target","stats":{"race":1,"ai":0,"level":1,"hp":1000000,"range":256,"aspeed":1000000,"sight":[0, 0],"attack":{"min":1,"max":1},"defense":[0, 0]},"spawns":[{"map":0,"area":1,"max":1,"cycle":1000,"rect":[35, 32, 35, 32]}]})");
            write(
                "monsters/62002.json",
                R"({"index":62002,"name":"Owned secondary Mage target","stats":{"race":1,"ai":0,"level":1,"hp":1000000,"range":256,"aspeed":1000000,"sight":[0, 0],"attack":{"min":1,"max":1},"defense":[0, 0]},"spawns":[{"map":0,"area":1,"max":1,"cycle":1000,"rect":[36, 32, 36, 32]}]})");
            for (const int skill : {4, 9, 23, 31, 43, 66, 67}) {
                write(
                    "skills/" + std::to_string(skill) + ".json",
                    "{\"class\":\"mage\",\"index\":" + std::to_string(skill) +
                        ",\"maxLevel\":1,\"mp\":1,\"levelLimit\":90,\"grade\":1,\"type\":[\"use\"],\"manualLearn\":true,\"delay\":[0, 200]}");
            }
            auto store = server::create_data_store(config.store);
            require(store && store->open(config.save_dir), "open owned SQLite fixture");
            server::CharacterRecord draft;
            draft.name = "Owned Mage scheduler";
            draft.clazz = 1;
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
                    "create owned local account and Mage");
            const auto account = store->find_account(server::kDefaultAccountName);
            require(account.has_value(), "load owned local account");
            auto characters = store->list_characters(account->id);
            require(characters.size() == 1, "one owned persisted character");
            draft.id = characters.front().id;
            draft.account_id = account->id;
            require(store->save_character(draft), "persist complete owned Mage seed");
            character_id = draft.id;
            config.character_id = character_id;
            for (const auto& item : store->load_items(character_id)) {
                require(store->delete_item(item.iid), "remove any implicit starter item");
            }
            server::ItemRecord staff;
            staff.iid = 1001;
            staff.character_id = character_id;
            staff.account_id = account->id;
            staff.index = 1;
            staff.count = 1;
            staff.slot = 1;
            require(store->insert_item(staff), "persist an ordinary equipped staff");
            original_items = store->load_items(character_id);
            require(original_items.size() == 1, "owned staff is the whole initial inventory");
            for (const int index : {4, 9, 23, 31, 43, 66, 67}) {
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
            require(skills.size() == 7, "all seven learned skill rows remain");
            for (const int index : {4, 9, 23, 31, 43, 66, 67}) {
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

    struct MonsterStateUpdate {
        std::uint32_t id = 0;
        std::uint64_t value = 0;
        std::uint64_t sequence = 0;
    };

    struct MonsterSpeedUpdate {
        std::uint32_t id = 0;
        std::int16_t value = 0;
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
        std::vector<MonsterStateUpdate> mstates;
        std::vector<MonsterSpeedUpdate> speeds;
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
                } else if (opcode == protocol::S2C::MState) {
                    std::uint32_t id = 0;
                    std::uint64_t value = 0;
                    require(payload.size() == 12 &&
                                protocol::MState::parse(payload.data(), payload.size(), id, value),
                            "parse complete known-monster MState");
                    mstates.push_back({id, value, sequence});
                } else if (opcode == protocol::S2C::UpdateMProperty) {
                    std::uint32_t id = 0;
                    std::uint8_t property = 0;
                    std::int16_t value = 0;
                    require(payload.size() == 7 &&
                                protocol::Property::parse_update_mproperty(
                                    payload.data(), payload.size(), id, property, value),
                            "parse complete known-monster speed property");
                    if (property == protocol::kPropertyMSpeed) {
                        speeds.push_back({id, value, sequence});
                    }
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
            if (self.id == 0 || clocks == 0 || (self.class_byte & 0x7f) != 1 || self.wear[0] != 1 ||
                max_mp <= 480 || mp != max_mp || hp <= 0) {
                return false;
            }
            for (const int index : {4, 9, 23, 31, 43, 66, 67}) {
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
            require(Clock::now() < until, "Mage scheduler watchdog: " + label);
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
                                      const std::array<std::uint32_t, 2>& targets) {
        wait_all(
            peers,
            [&] {
                const auto snapshot = host.monster_snapshot();
                for (std::size_t index = 0; index < targets.size(); ++index) {
                    std::vector<protocol::S2CSkillFields> first, second;
                    for (std::size_t reader = 0; reader < peers.size(); ++reader) {
                        auto& output = reader == 0 ? first : second;
                        for (const auto& value : peers[reader].skills) {
                            if (value.value.has_damage && value.value.caster == peers[0].self.id &&
                                value.value.target == targets[index]) {
                                output.push_back(value.value);
                            }
                        }
                    }
                    if (first.size() != second.size()) {
                        return false;
                    }
                    std::int64_t total = 0;
                    for (std::size_t i = 0; i < first.size(); ++i) {
                        if (skill_fields(first[i]) != skill_fields(second[i]) ||
                            first[i].damage <= 0) {
                            return false;
                        }
                        total += first[i].damage;
                    }
                    const auto found =
                        std::find_if(snapshot.begin(), snapshot.end(), [&](const auto& value) {
                            return value.id == targets[index];
                        });
                    if (found == snapshot.end() || found->hp <= 0 || found->max_hp != kTargetHp ||
                        found->x != kTargetX + static_cast<int>(index) * 32 || found->y != kY ||
                        found->hp != kTargetHp - total) {
                        return false;
                    }
                }
                return true;
            },
            "both full eight-field damage streams conserve both actual living monster HP values");
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

    void saved_barrier(std::array<Peer, 2>& peers, std::size_t index) {
        auto& peer = peers[index];
        peer.exiting = true;
        ++peer.expected_exit_acks;
        peer.send(protocol::GameExit::frame_client_request());
        wait_all(
            peers,
            [&] { return peer.exit_acks == peer.expected_exit_acks; },
            "new parsed saved=true FIFO barrier");
        require(peer.connected, "ordinary save barrier leaves the gameplay connection healthy");
    }

    void cast(std::array<Peer, 2>& peers, int skill, const Bytes& request, int cost) {
        const auto before = peers[0].sequence;
        const int mp = peers[0].mp;
        peers[0].send(request);
        wait_all(
            peers,
            [&] {
                const bool spent = std::any_of(
                    peers[0].mp_updates.begin(), peers[0].mp_updates.end(), [&](const auto& value) {
                        return value.sequence > before && value.value == mp - cost;
                    });
                const bool accepted = std::any_of(
                    peers[0].skills.begin(), peers[0].skills.end(), [&](const auto& value) {
                        return value.sequence > before && value.value.skill == skill &&
                               value.value.caster == peers[0].self.id && !value.value.has_damage;
                    });
                return spent && accepted;
            },
            "ordinary learned Mage cast and literal MP reconciliation");
    }

    void wait_hits(std::array<Peer, 2>& peers, int skill, std::uint32_t target, std::size_t count) {
        wait_all(
            peers,
            [&] {
                return damage_hits(peers[0], skill, peers[0].self.id, target).size() >= count &&
                       damage_hits(peers[1], skill, peers[0].self.id, target).size() >= count;
            },
            "real scheduled Mage target damage reaches both readers",
            std::chrono::seconds(12));
    }

    std::uint32_t storm_controller(std::array<Peer, 2>& peers) {
        wait_all(
            peers,
            [&] { return peers[0].created(228) && peers[1].created(228); },
            "both readers receive the actual Thunder Storm Magic controller");
        const auto id = peers[0].created(228)->id;
        require(id != 0 && peers[1].created(228)->id == id,
                "both readers agree on the real storm controller identity");
        return id;
    }

    std::array<std::uint64_t, 2> actor_movement(std::array<Peer, 2>& peers) {
        const auto before = peers[0].sequence;
        move_confirmed(peers, 0, 1);
        std::array<std::uint64_t, 2> result{};
        for (std::size_t frame = static_cast<std::size_t>(before); frame < peers[0].wire.size();
             ++frame) {
            const auto& [type, payload] = peers[0].wire[frame];
            if (type != static_cast<std::uint8_t>(protocol::S2C::MovePlayerEnd)) {
                continue;
            }
            std::uint32_t id = 0;
            std::int8_t dx = 0, dy = 0, dz = 0;
            require(protocol::Move::parse_player(payload.data(), payload.size(), id, dx, dy, dz),
                    "decode exact ordinary actor movement boundary");
            if (id == peers[0].self.id && dx == 1 && dy == 0 && dz == 0) {
                result[0] = frame + 1;
                break;
            }
        }
        require(result[0] > before, "the actor receives its exact authoritative movement echo");
        // Local SQLite mode does not broadcast remote player movement. Submit a fresh
        // observer FIFO barrier only after receiving the actor's accepted movement echo.
        saved_barrier(peers, 1);
        result[1] = peers[1].sequence;
        return result;
    }

    void trigger_pair(std::array<Peer, 2>& peers,
                      std::uint32_t target,
                      std::uint32_t second,
                      int first_skill,
                      int next_skill,
                      std::size_t expected_splashes) {
        const auto first_before =
            damage_hits(peers[0], first_skill, peers[0].self.id, target).size();
        const auto next_before = damage_hits(peers[0], next_skill, peers[0].self.id, target).size();
        Bytes batch = protocol::Skill::frame_client_target(
            static_cast<std::uint8_t>(first_skill), protocol::kCkMonster, target);
        const auto duplicate = batch;
        batch.insert(batch.end(), duplicate.begin(), duplicate.end());
        const auto next = protocol::Skill::frame_client_target(
            static_cast<std::uint8_t>(next_skill), protocol::kCkMonster, target);
        batch.insert(batch.end(), next.begin(), next.end());
        const auto began = Clock::now();
        peers[0].send(batch);
        wait_hits(peers, first_skill, target, first_before + 1);
        wait_hits(peers, next_skill, target, next_before + 1);
        const auto last = damage_hits(peers[0], next_skill, peers[0].self.id, target).back();
        require(
            last.received >= began && last.received - began < std::chrono::milliseconds(499),
            "live lockout precondition: accepted trigger pair fits 499ms with integer-clock quantization margin");
        wait_hits(peers, 67, target, expected_splashes + 1);
        wait_hits(peers, 67, second, expected_splashes);
        saved_barrier(peers, 0);
        saved_barrier(peers, 1);
        for (const auto& peer : peers) {
            require(damage_hits(peer, first_skill, peers[0].self.id, target).size() ==
                            first_before + 1 &&
                        damage_hits(peer, next_skill, peers[0].self.id, target).size() ==
                            next_before + 1,
                    "both distinct casts damage the marked victim once despite the repeat");
            require(damage_hits(peer, 67, peers[0].self.id, target).size() ==
                            expected_splashes + 1 &&
                        damage_hits(peer, 67, peers[0].self.id, second).size() == expected_splashes,
                    "one splash per accepted trigger pair preserves the shared mark lockout");
        }
        std::cout
            << "mark-trigger-pair=" << first_skill << ',' << next_skill << " accepted-response-us="
            << std::chrono::duration_cast<std::chrono::microseconds>(last.received - began).count()
            << " splashes=" << expected_splashes << '\n';
    }

    void exercise(int skill) {
        Fixture fixture;
        server::ServerHost host;
        require(host.start(fixture.config), "start real owned SQLite/Unix Mage scheduler fixture");
        std::array<Peer, 2> peers;
        for (auto& peer : peers) {
            const auto connection_deadline = Clock::now() + std::chrono::seconds(8);
            while (!peer.session && Clock::now() < connection_deadline) {
                peer.session = server::connect_unix(fixture.config.unix_path);
                if (!peer.session) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                }
            }
            require(peer.session != nullptr, "connect ordinary independent Mage reader");
            peer.connected = true;
            peer.send(protocol::Handshake::build_connect(0));
        }
        wait_all(
            peers,
            [&] {
                return peers[0].ready() && peers[1].ready() && peers[0].created(62001) &&
                       peers[1].created(62001) && peers[0].created(62002) &&
                       peers[1].created(62002);
            },
            "both handshakes, full resources, staff, seven learned ranks and both actual targets");
        require(peers[0].self.id != peers[1].self.id,
                "local SQLite sessions have distinct runtime actors");
        const std::array<std::uint32_t, 2> targets{peers[0].created(62001)->id,
                                                   peers[0].created(62002)->id};
        require(targets[0] != 0 && targets[1] != 0 && targets[0] != targets[1],
                "ordinary owned targets have distinct real runtime identities");
        for (std::size_t index = 0; index < targets.size(); ++index) {
            for (const auto& peer : peers) {
                const auto created = *peer.created(62001 + static_cast<int>(index));
                require(created.id == targets[index] && created.x == kTargetX + index * 32 &&
                            created.y == kY && created.hp == kTargetHp &&
                            created.max_hp == kTargetHp,
                        "both targets have their authored pose and normal revised full HP");
            }
        }
        while (peers[1].self.x < kObserverX) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            move_confirmed(peers, 1, std::min(3, kObserverX - peers[1].self.x));
        }
        require(peers[0].self.x == kActorX && peers[1].self.x == kObserverX &&
                    peers[1].self.x - (kTargetX + 32) > 300 &&
                    coords::in_sight(peers[1].self.x, peers[1].self.y, kTargetX, kY) &&
                    coords::in_sight(
                        peers[1].self.x, peers[1].self.y, peers[0].self.x, peers[0].self.y),
                "healthy observer stays in sight beyond all damaging fields and owner radii");
        std::uint32_t controller = 0;
        if (skill == 43) {
            cast(peers, 43, protocol::Skill::frame_client_ground(43, kTargetX, kY), 352);
            controller = storm_controller(peers);
            for (const auto target : targets) {
                wait_hits(peers, 43, target, 2);
            }
            require_shared_damage_and_hp(host, peers, targets);
            wait_all(
                peers,
                [&] {
                    return peers[0].removed.count(controller) && peers[1].removed.count(controller);
                },
                "complete ordinary eight-pulse storm and exact controller cleanup",
                std::chrono::seconds(20));
            for (const auto& peer : peers) {
                for (const auto target : targets) {
                    const auto hits = damage_hits(peer, 43, peers[0].self.id, target);
                    require(
                        hits.size() == 8 && std::all_of(hits.begin(),
                                                        hits.end(),
                                                        [&](const auto& hit) {
                                                            return hit.sequence <
                                                                   peer.removed.at(controller);
                                                        }),
                        "both readers see eight positive storm pulses before exact controller removal");
                }
            }
            require_shared_damage_and_hp(host, peers, targets);
        } else if (skill == 66) {
            cast(peers, 66, protocol::Skill::frame_client_self(66), 480);
            wait_hits(peers, 66, targets[0], 2);
            for (const auto& peer : peers) {
                const auto state =
                    std::find_if(peer.mstates.begin(), peer.mstates.end(), [&](const auto& value) {
                        return value.id == targets[0] &&
                               (value.value & protocol::kCmsMoveSpeed) != 0;
                    });
                require(state != peer.mstates.end() &&
                            std::any_of(peer.speeds.begin(),
                                        peer.speeds.end(),
                                        [&](const auto& value) {
                                            return value.id == targets[0] && value.value == -100 &&
                                                   value.sequence > state->sequence;
                                        }),
                        "actual 66 damage applies known-monster MState and signed -100 speed");
                require(damage_hits(peer, 66, peers[0].self.id, targets[1]).empty(),
                        "self-area 66 respects the three-tile authored boundary");
            }
            cast(peers, 43, protocol::Skill::frame_client_ground(43, kTargetX, kY), 352);
            controller = storm_controller(peers);
            const auto moved = actor_movement(peers);
            std::array<std::size_t, 2> before{};
            for (std::size_t index = 0; index < peers.size(); ++index) {
                const auto hits = damage_hits(peers[index], 66, peers[0].self.id, targets[0]);
                before[index] = std::count_if(hits.begin(), hits.end(), [&](const auto& hit) {
                    return hit.sequence < moved[index];
                });
                require(before[index] >= 2 && before[index] < 4 && hits.size() == before[index],
                        "acknowledged movement interrupts 66 while multiple natural pulses remain");
            }
            wait_all(
                peers,
                [&] {
                    for (std::size_t index = 0; index < peers.size(); ++index) {
                        for (const auto target : targets) {
                            const auto hits =
                                damage_hits(peers[index], 43, peers[0].self.id, target);
                            if (std::count_if(hits.begin(), hits.end(), [&](const auto& hit) {
                                    return hit.sequence > moved[index];
                                }) < 2) {
                                return false;
                            }
                        }
                    }
                    return true;
                },
                "two actual post-movement storm pulses on both readers and targets");
            saved_barrier(peers, 0);
            saved_barrier(peers, 1);
            for (std::size_t index = 0; index < peers.size(); ++index) {
                require(
                    damage_hits(peers[index], 66, peers[0].self.id, targets[0]).size() ==
                        before[index],
                    "independent later 43 scheduler pulses progress without any further cancelled 66 damage");
            }
            require_shared_damage_and_hp(host, peers, targets);
        } else {
            cast(peers,
                 67,
                 protocol::Skill::frame_client_target(67, protocol::kCkMonster, targets[0]),
                 480);
            wait_hits(peers, 67, targets[0], 1);
            saved_barrier(peers, 0);
            saved_barrier(peers, 1);
            for (const auto& peer : peers) {
                require(damage_hits(peer, 67, peers[0].self.id, targets[0]).size() == 1 &&
                            damage_hits(peer, 67, peers[0].self.id, targets[1]).empty(),
                        "initial scheduled 67 is direct-only and leaves the second victim intact");
            }
            trigger_pair(peers, targets[0], targets[1], 4, 9, 1);
            require_shared_damage_and_hp(host, peers, targets);
            actor_movement(peers);
            cast(peers, 43, protocol::Skill::frame_client_ground(43, kTargetX, kY), 352);
            controller = storm_controller(peers);
            for (const auto target : targets) {
                wait_hits(peers, 43, target, 1);
            }
            // A newly created storm's first real pulse is due after1000ms: the mark's
            //500ms shared lockout has elapsed without an artificial sleep or clock seam.
            trigger_pair(peers, targets[0], targets[1], 23, 31, 2);
            require_shared_damage_and_hp(host, peers, targets);
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
            // Shared SQLite row: close the observer before two fresh actor barriers.
            const int saves = index == 0 ? 2 : 1;
            for (int save = 0; save < saves; ++save) {
                saved_barrier(peers, index);
            }
            if (index == 0) {
                auto audit = server::create_data_store(fixture.config.store);
                require(audit && audit->open(fixture.config.save_dir),
                        "reopen SQLite after actual saved acknowledgement");
                const auto saved = audit->load_character(fixture.character_id);
                require(
                    saved && saved->hp == peers[0].hp_at_exit && saved->mp == peers[0].mp_at_exit &&
                        saved->map_x == peers[0].self.x && saved->map_y == kY &&
                        saved->clazz == 1 && saved->level == 90,
                    "successful save contains exact preceding authoritative Mage resources and pose");
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
        std::cout << "ordinary-mage-scheduler skill=" << skill << " actor=" << peers[0].self.id
                  << " target=" << targets[0] << " second=" << targets[1]
                  << " controller=" << controller << " actor_hp_saved=" << peers[0].hp_at_exit
                  << " actor_mp_saved=" << peers[0].mp_at_exit << '\n';
    }
}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 1) {
            for (const int skill : {43, 66, 67}) {
                exercise(skill);
            }
        } else {
            require(argc == 3 && std::string(argv[1]) == "--skill",
                    "optional bringup argument is --skill 43, 66 or 67");
            const std::string value = argv[2];
            require(value == "43" || value == "66" || value == "67",
                    "finite ordinary Mage skill selection");
            exercise(std::stoi(value));
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Mage scheduler witness failed: " << error.what() << '\n';
        return 1;
    }
}
