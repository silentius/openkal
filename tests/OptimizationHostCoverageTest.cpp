/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

// Compile the implementation here to characterize its internal helpers.
// The static server archive supplies dependencies without pulling a second Host object.
#include "../libs/server/src/Host.cpp"
#include "OptimizationGameWorldPeer.h"
#include "fixture_util.h"
#include "openkal/server/GuildStore.h"
#include "test_util.h"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sqlite3.h>
#include <sstream>
#include <string_view>
#include <tuple>
#include <unistd.h>

namespace {
    using namespace openkal;
    using namespace openkal::server;
    using test::expect;

    void test_gameplay_limits() {
        MovementBudget movement;
        expect(movement.spend(1.0, 1000) && movement.spend(0.5, 1000),
               "movement accepts a bounded burst");
        expect(!movement.spend(0.1, 1000) && movement.spend(0.1, 1100),
               "movement refills with server time and rejects excess travel");
        expect(!combat_repeat_ready(1000, 1200, 200) && combat_repeat_ready(1100, 1200, 200),
               "weapon and skill cooldowns reject immediate repeats and allow bounded jitter");
        combat::Combatant mage;
        mage.level = 10;
        mage.min_magic = mage.max_magic = 5;
        const auto immediate = combat::resolve_magic(mage, mage, 2, 1, 18, 0, 0, 1200);
        const auto charged = combat::resolve_magic(mage, mage, 2, 1, 18, 0, 1200, 1200);
        expect(immediate.damage == 0 && immediate.palsy == 0 && charged.damage > 0,
               "zero elapsed cast time cannot grant damage or palsy");
    }

    void test_protocol_point_decay() {
        ProtocolPointDecay decay;
        decay.restart(SessionPhase::WaitConnect, 3, 0);
        expect(!decay.due(SessionPhase::WaitConnect, 600000) &&
                   !decay.due(SessionPhase::Playing, 600000),
               "character selection does not start a decay interval");
        decay.restart(SessionPhase::Playing, 3, 600000);
        expect(!decay.due(SessionPhase::Playing, 899999) &&
                   decay.due(SessionPhase::Playing, 900000),
               "the first point decays after 300 seconds in the world");
        decay.advance(2);
        expect(!decay.due(SessionPhase::Playing, 1199999) &&
                   decay.due(SessionPhase::Playing, 1200000),
               "another point requires a complete interval");
        decay.restart(SessionPhase::Playing, 3, 1190000);
        expect(!decay.due(SessionPhase::Playing, 1200000) &&
                   decay.due(SessionPhase::Playing, 1490000),
               "a violation restarts the full interval");
        decay.restart(SessionPhase::WaitConnect, 3, 1490000);
        decay.restart(SessionPhase::Playing, 3, 1800000);
        expect(!decay.due(SessionPhase::Playing, 2099999) &&
                   decay.due(SessionPhase::Playing, 2100000),
               "world re-entry discards earlier partial progress");
        decay.advance(0);
        expect(!decay.due(SessionPhase::Playing, 2400000), "decay stops at zero points");
    }

#if defined(OPENKAL_UPKEEP_BASELINE_BRIDGE)
#include "HostUpkeepBaseline.inc"
#endif

#if defined(OPENKAL_ROUTING_BASELINE_BRIDGE)
#include "HostRoutingBaseline.inc"
#endif

#if defined(OPENKAL_STORAGE_BASELINE_BRIDGE)
#include "HostStorageBaseline.inc"
#endif

#if defined(OPENKAL_SHOP_BASELINE_BRIDGE)
#include "HostShopBaseline.inc"
#endif

#if defined(OPENKAL_DOGGEBI_BASELINE_BRIDGE)
#include "HostDoggebiBaseline.inc"
#endif

#if defined(OPENKAL_ARCHER_BASELINE_BRIDGE)
#include "HostArcherBaseline.inc"
#endif

#if defined(OPENKAL_MAGE_BASELINE_BRIDGE)
#include "HostMageBaseline.inc"
#endif

#if defined(OPENKAL_WORLDSTEP_BASELINE_BRIDGE)
#include "HostWorldStepBaseline.inc"
#endif

#if defined(OPENKAL_WORLDEVENTS_BASELINE_BRIDGE)
#include "HostWorldEventsBaseline.inc"
#endif

    class RecordingSession final : public ISession {
      public:
        int fd() const override {
            return -1;
        }
        bool alive() const override {
            return m_alive;
        }
        void close() override {
            m_alive = false;
        }
        bool send_all(const std::uint8_t* bytes, std::size_t size) override {
            packets.emplace_back(bytes, bytes + size);
            return successful;
        }
        SendResult try_send(const std::uint8_t* bytes, std::size_t size) override {
            packets.emplace_back(bytes, bytes + size);
            return successful ? SendResult{SendStatus::Progress, size, SendReadiness::Write}
                              : SendResult{};
        }
        bool recv_into(std::vector<std::uint8_t>&) override {
            return m_alive;
        }
        std::vector<std::vector<std::uint8_t>> packets;
        bool successful = true;

      private:
        bool m_alive = true;
    };

    class OwnedGuildStore final : public IGuildStore {
      public:
        platform::Json state = platform::Json::parse(
            R"({"revision":0,"next_id":2,"guilds":[{"id":1,"grade":2,"members":[{"id":1,"level":110,"rank":1},{"id":2,"level":110,"rank":5}]}],"characters":[],"applications":[],"receipts":[],"ranking":[]})");
        bool load_guilds(platform::Json& out) override {
            out = state;
            return true;
        }
        bool commit_guilds(std::uint64_t previous,
                           const platform::Json& next,
                           const std::vector<CharacterRecord>&) override {
            if (static_cast<std::uint64_t>(state.at("revision").number()) != previous) {
                return false;
            }
            state = next;
            return true;
        }
    };

    auto combat_values(const combat::Combatant& a) {
        return std::make_tuple(a.level,
                               a.min_attack,
                               a.max_attack,
                               a.min_magic,
                               a.max_magic,
                               a.armor_broken,
                               a.magic_intensification_rank,
                               a.hit,
                               a.dodge,
                               a.defense,
                               a.defense_range,
                               a.absorb,
                               a.fatal_rate,
                               a.fatal_pt,
                               a.critical_damage_percent,
                               a.explosive_damage_percent,
                               a.talent_explosive_damage_percent,
                               a.perfect_evasion_base,
                               a.perfect_evasion_advanced,
                               a.block_percent,
                               a.wearing_shield,
                               a.wearing_two_hand,
                               a.has_parry,
                               a.has_second_job_attrib,
                               a.res_fire,
                               a.res_ice,
                               a.res_lightning,
                               a.res_curse,
                               a.res_palsy,
                               a.attack_range,
                               a.attack_speed_ms,
                               a.is_player,
                               a.clazz,
                               a.attack_type,
                               a.upgrade_level,
                               a.char_attack,
                               a.char_magic,
                               a.weapon_min_attack,
                               a.weapon_max_attack,
                               a.weapon_min_magic,
                               a.weapon_max_magic,
                               a.bstate);
    }

    void apply_buff(ClientSession& c, SkillBuffId id, int value1, int value2 = 0) {
        std::int32_t remain = 0;
        std::uint64_t state = 0;
        expect(c.skill_buffs.apply(id, c.clazz, 1, 60, value1, value2, 100, remain, state),
               "apply owned characterization buff");
    }

    void test_interest() {
        GameWorld world;
        NpcRegistry npcs;
        ClientSession c;
        c.map = 0;
        c.px = c.py = 64 * map::kTileSize;
        auto sync_all = [&] {
            sync_monster_interest(c, world);
            sync_item_interest(c, world);
            sync_npc_interest(c, npcs);
            if (c.sock) {
                (void) c.sock->flush(POLLIN | POLLOUT);
            }
        };
        c.known_monsters.insert(900);
        c.known_items.insert(901);
        c.known_npcs.insert(902);
        sync_all();
        expect(c.known_monsters.count(900) && c.known_items.count(901) && c.known_npcs.count(902),
               "interest leaves disconnected viewer knowledge untouched");
        auto recording = std::make_unique<RecordingSession>();
        auto* output = recording.get();
        c.sock = std::make_unique<HostConnection>(std::move(recording));
        sync_all();
        c.phase = SessionPhase::Playing;
        sync_all();
        expect(output->packets.empty(), "waiting phase and zero player id suppress all interest");
        c.player_id = 1;
        sync_all();
        expect(output->packets.size() == 3 && c.known_monsters.empty() && c.known_items.empty() &&
                   c.known_npcs.empty(),
               "empty world removes stale reconnect knowledge");
        output->packets.clear();
        auto add_monster = [&](std::uint32_t id,
                               int dx,
                               int dy,
                               int logical_map,
                               int hp,
                               std::uint32_t gstate,
                               float knee,
                               float dying) {
            MonsterEntity m;
            m.id = id;
            m.x = c.px + dx;
            m.y = c.py + dy;
            m.map = logical_map;
            m.hp = hp;
            m.gstate = gstate;
            m.knee_left = knee;
            m.die_remove_left = dying;
            world.monsters().push_back(m);
        };
        add_monster(10, 0, 0, 0, 1, 0, 0, 0);
        add_monster(11, 25 * 32 + 31, 0, 0, 1, 0, 0, 0);
        add_monster(12, 26 * 32, 0, 0, 1, 0, 0, 0);
        add_monster(13, -25 * 32, 0, 0, 1, 0, 0, 0);
        add_monster(14, -25 * 32 - 1, 0, 0, 1, 0, 0, 0);
        add_monster(15, 0, 0, 1, 1, 0, 0, 0);
        add_monster(16, 0, 0, 0, 0, 0, 0, 0);
        add_monster(17, 0, 0, 0, 0, protocol::kCgsKnee, 0, 0);
        add_monster(18, 0, 0, 0, -1, 0, 1, 0);
        add_monster(19, 0, 0, 0, 0, 0, 0, 1);
        add_monster(20, 0, 26 * 32, 0, 1, 0, 0, 0);
        add_monster(21, 0, -25 * 32 - 1, 0, 1, 0, 0, 0);
        std::vector<std::vector<std::uint8_t>> expected;
        for (const auto& m : world.monsters()) {
            if (m.id == 10 || m.id == 11 || m.id == 13 || m.id == 17 || m.id == 18 || m.id == 19) {
                expected.push_back(frame_create_monster(m));
            }
            GroundItem g;
            g.id = m.id + 100;
            g.x = m.x;
            g.y = m.y;
            g.map = m.map;
            g.item_index = 1;
            world.ground_items().push_back(g);
        }
        sync_monster_interest(c, world);
        (void) c.sock->flush(POLLIN | POLLOUT);
        expect(output->packets == expected,
               "monster creates preserve world order and corpse/sight rules");
        output->packets.clear();
        for (const auto& g : world.ground_items()) {
            if (g.map == 0 && g.id != 112 && g.id != 114 && g.id != 120 && g.id != 121) {
                output->packets.push_back(frame_create_item(g));
            }
        }
        expected = output->packets;
        output->packets.clear();
        sync_item_interest(c, world);
        (void) c.sock->flush(POLLIN | POLLOUT);
        expect(output->packets == expected,
               "loot creates preserve world order and exact tile boundaries");

        const auto root = test::temp_dir(("optimization_npcs_" + std::to_string(getpid())).c_str());
        for (int i = 0; i < 5; ++i) {
            const int dx = i == 1 ? 25 * 32 + 31 : i == 2 ? 26 * 32 : 0;
            const int dy = i == 4 ? -25 * 32 - 1 : 0;
            std::ofstream(root + "/" + std::to_string(i + 1) + ".json")
                << "{\"index\":" << i + 1 << ",\"name\":\"Owned NPC\",\"map\":" << (i == 3 ? 1 : 0)
                << ",\"xy\":[" << c.px + dx << ", " << c.py + dy << "]}";
        }
        expect(npcs.load_dir(root), "load owned NPC visibility fixtures");
        std::uint32_t next = 200;
        npcs.spawn_for_maps(next, 0, 1);
        output->packets.clear();
        expected.clear();
        for (const auto& n : npcs.npcs()) {
            if (n.map == 0 && n.x < c.px + 26 * 32 && n.y == c.py) {
                expected.push_back(
                    protocol::Quest::frame_create_npc(NpcRegistry::to_create_info(n)));
            }
        }
        sync_npc_interest(c, npcs);
        (void) c.sock->flush(POLLIN | POLLOUT);
        expect(output->packets == expected && expected.size() == 2,
               "NPC map and sight boundaries preserve create order");
        output->packets.clear();
        sync_all();
        expect(output->packets.empty(), "unchanged ticks send no duplicate creates");
        world.monsters().front().x += 26 * 32;
        world.ground_items().erase(world.ground_items().begin());
        sync_all();
        expect(output->packets == std::vector<std::vector<std::uint8_t>>{frame_remove_monster(10),
                                                                         frame_remove_item(110)},
               "stationary viewer sees movement and pickup removal exactly once");
        output->packets.clear();
        world.monsters()[7].gstate = 0;
        world.monsters()[8].knee_left = 0;
        world.monsters()[9].die_remove_left = 0;
        sync_monster_interest(c, world);
        (void) c.sock->flush(POLLIN | POLLOUT);
        expect(!c.known_monsters.count(17) && !c.known_monsters.count(18) &&
                   !c.known_monsters.count(19) && output->packets.size() == 3,
               "completed corpse lifetimes emit exactly one removal each");
        c.px += 100000;
        sync_all();
        expect(c.known_monsters.empty() && c.known_items.empty() && c.known_npcs.empty(),
               "viewer teleport evicts all previous map interest");
        c.px -= 100000;
        output->successful = false;
        sync_all();
        expect(!c.known_monsters.empty() && !c.known_items.empty() && !c.known_npcs.empty(),
               "characterize current failed-send behavior: interest knowledge advances");
        std::filesystem::remove_all(root);
    }

    template <typename Entity, typename Visible, typename Create, typename Remove>
    void legacy_interest(ClientSession& c,
                         const std::vector<Entity>& entities,
                         std::unordered_set<std::uint32_t>& known,
                         Visible visible,
                         Create create,
                         Remove remove) {
        if (!c.sock || c.phase != SessionPhase::Playing || c.player_id == 0) {
            return;
        }
        // Independent reference for the full traversal and per-call hash set.
        std::unordered_set<std::uint32_t> should_see;
        should_see.reserve(entities.size());
        for (const auto& entity : entities) {
            if (!visible(entity)) {
                continue;
            }
            should_see.insert(entity.id);
            if (!known.count(entity.id)) {
                c.sock->enqueue(create(entity));
                known.insert(entity.id);
            }
        }
        for (auto it = known.begin(); it != known.end();) {
            if (!should_see.count(*it)) {
                c.sock->enqueue(remove(*it));
                it = known.erase(it);
            } else {
                ++it;
            }
        }
    }

    bool visible_monster(const ClientSession& c, const MonsterEntity& m) {
        const bool corpse = m.hp <= 0 && ((m.gstate & protocol::kCgsKnee) != 0 || m.knee_left > 0 ||
                                          m.die_remove_left > 0);
        return (m.hp > 0 || corpse) && m.map == c.map && coords::in_sight(c.px, c.py, m.x, m.y);
    }

    void legacy_monster_interest(ClientSession& c, const GameWorld& world) {
        legacy_interest(
            c,
            world.monsters(),
            c.known_monsters,
            [&](const auto& m) { return visible_monster(c, m); },
            [](const auto& m) { return frame_create_monster(m); },
            [](auto id) { return frame_remove_monster(id); });
    }

    struct InterestViewer {
        ClientSession session;
        RecordingSession* output = nullptr;
        InterestViewer() {
            auto recording = std::make_unique<RecordingSession>();
            output = recording.get();
            session.sock = std::make_unique<HostConnection>(std::move(recording));
            session.phase = SessionPhase::Playing;
            session.player_id = 1;
            session.px = session.py = 2048;
        }
    };

    auto normalized_interest_packets(const RecordingSession& output) {
        std::pair<std::vector<std::vector<std::uint8_t>>, std::vector<std::vector<std::uint8_t>>>
            result;
        for (const auto& packet : output.packets) {
            std::size_t consumed = 0;
            std::uint8_t type = 0;
            std::vector<std::uint8_t> payload;
            expect(
                protocol::FrameParser::pop(packet.data(), packet.size(), consumed, type, payload) ==
                        protocol::FrameParseStatus::Complete &&
                    consumed == packet.size(),
                "interest differential output consists of complete protocol frames");
            const bool removal = type == static_cast<std::uint8_t>(protocol::S2C::RemoveMonster) ||
                                 type == static_cast<std::uint8_t>(protocol::S2C::RemoveItem) ||
                                 type == static_cast<std::uint8_t>(protocol::S2C::RemoveNpc);
            (removal ? result.second : result.first).push_back(packet);
        }
        // Create order is contractual source traversal order. Removal membership is
        // compared independently of the hash container's unspecified iteration order.
        std::sort(result.second.begin(), result.second.end());
        return result;
    }

    void test_indexed_interest() {
        GameWorld world;
        NpcRegistry npcs;
        WorldInterestIndex index;
        InterestViewer oracle, fallback, indexed;
        const auto each = [&](const auto& action) {
            action(oracle.session);
            action(fallback.session);
            action(indexed.session);
        };
        each([](auto& c) {
            c.known_monsters.insert(90000);
            c.known_items.insert(90001);
            c.known_npcs.insert(90002);
        });
        const auto root = test::temp_dir(("indexed_interest_" + std::to_string(getpid())).c_str());
        const auto write_npcs = [&](int revision) {
            std::filesystem::remove_all(root);
            test::mkdir_p(root);
            for (int i = 0; i < 8; ++i) {
                const int x = 2048 + (i == 1 ? 831 : i == 2 ? 832 : i == 3 ? -801 : 0);
                std::ofstream(root + "/" + std::to_string(i + 1) + ".json")
                    << "{\"index\":" << i + 1
                    << ",\"name\":\"Owned indexed NPC\",\"map\":" << (i % 3 == 0 ? 2 : 0)
                    << ",\"xy\":[" << x + revision << ", 2048]}";
            }
            expect(npcs.load_dir(root), "reload owned NPC definitions for indexed interest");
            std::uint32_t next = static_cast<std::uint32_t>(5000 + revision);
            npcs.spawn_for_maps(next, 0, 2);
        };
        write_npcs(0);
        for (int i = 0; i < 32; ++i) {
            MonsterEntity m;
            m.id = static_cast<std::uint32_t>(100 + i);
            m.map = i % 5 == 0 ? 2 : 0;
            m.x = 2048 + (i == 1 ? 831 : i == 2 ? 832 : i == 3 ? -800 : i == 4 ? -801 : 0);
            m.y = 2048 + (i == 6 ? 831 : i == 7 ? 832 : i == 8 ? -800 : i == 9 ? -801 : 0);
            m.hp = i % 4 == 0 ? 0 : 100;
            m.gstate = i == 12 ? protocol::kCgsKnee : 0;
            m.knee_left = i == 16 ? 1.0f : 0;
            m.die_remove_left = i == 24 ? 1.0f : 0;
            world.monsters().push_back(m);
            GroundItem g;
            g.id = static_cast<std::uint32_t>(1000 + i);
            g.item_index = 1;
            g.map = m.map;
            g.x = m.x;
            g.y = m.y;
            world.ground_items().push_back(g);
        }
        const auto compare = [&](const char* stage, bool exact_packet_order = false) {
            index.rebuild(world, npcs);
            for (auto* viewer : {&oracle, &fallback, &indexed}) {
                viewer->output->packets.clear();
            }
            auto& c = oracle.session;
            legacy_monster_interest(c, world);
            legacy_interest(
                c,
                world.ground_items(),
                c.known_items,
                [&](const auto& g) {
                    return g.map == c.map && coords::in_sight(c.px, c.py, g.x, g.y);
                },
                [](const auto& g) { return frame_create_item(g); },
                [](auto id) { return frame_remove_item(id); });
            legacy_interest(
                c,
                npcs.npcs(),
                c.known_npcs,
                [&](const auto& n) {
                    return n.map == c.map && coords::in_sight(c.px, c.py, n.x, n.y);
                },
                [](const auto& n) {
                    return protocol::Quest::frame_create_npc(NpcRegistry::to_create_info(n));
                },
                [](auto id) { return protocol::Quest::frame_remove_npc(id); });
            sync_monster_interest(fallback.session, world);
            sync_item_interest(fallback.session, world);
            sync_npc_interest(fallback.session, npcs);
            sync_monster_interest(indexed.session, world, &index);
            sync_item_interest(indexed.session, world, &index);
            sync_npc_interest(indexed.session, npcs, &index);
            for (auto* viewer : {&oracle, &fallback, &indexed}) {
                while (viewer->session.sock->pending()) {
                    (void) viewer->session.sock->flush(POLLIN | POLLOUT);
                }
            }
            const auto expected = normalized_interest_packets(*oracle.output);
            expect(normalized_interest_packets(*fallback.output) == expected &&
                       normalized_interest_packets(*indexed.output) == expected &&
                       fallback.session.known_monsters == c.known_monsters &&
                       indexed.session.known_monsters == c.known_monsters &&
                       fallback.session.known_items == c.known_items &&
                       indexed.session.known_items == c.known_items &&
                       fallback.session.known_npcs == c.known_npcs &&
                       indexed.session.known_npcs == c.known_npcs,
                   stage);
            if (exact_packet_order) {
                expect(oracle.output->packets == fallback.output->packets &&
                           oracle.output->packets == indexed.output->packets,
                       "fixed initial fixture retains exact create and removal frame order");
            }
        };
        compare(
            "indexed and fallback interest match the old oracle at all sight/map/corpse boundaries",
            true);
        const auto scratch_capacity = indexed.session.interest_scratch.capacity();
        const auto monster_capacity = index.monsters(0).capacity();
        const auto item_capacity = index.items(0).capacity();
        const auto npc_capacity = index.npcs(0).capacity();
        const auto* monster_storage = index.monsters(0).data();
        for (int repeat = 0; repeat < 8; ++repeat) {
            compare("stable indexed batches produce no duplicate creates or removals");
        }
        expect(indexed.output->packets.empty() &&
                   indexed.session.interest_scratch.capacity() == scratch_capacity &&
                   index.monsters(0).capacity() == monster_capacity &&
                   index.items(0).capacity() == item_capacity &&
                   index.npcs(0).capacity() == npc_capacity &&
                   index.monsters(0).data() == monster_storage,
               "stable batches retain scratch and per-map candidate storage capacities");

        world.monsters().front().map = 0;
        world.monsters().front().hp = 100;
        world.monsters()[1].x += 1;
        world.monsters()[2].x -= 1;
        world.monsters()[12].gstate = 0;
        world.monsters()[16].knee_left = 0;
        world.monsters()[24].die_remove_left = 0;
        world.ground_items().erase(world.ground_items().begin() + 1);
        world.ground_items().pop_back();
        compare("stationary viewers observe spawn, sight crossings, corpse completion, pickup and "
                "expiry");
        world.monsters().erase(world.monsters().begin() + 2);
        auto spawned = world.monsters().front();
        spawned.id = 999;
        world.monsters().push_back(spawned);
        write_npcs(1);
        compare(
            "source vector mutation and NPC reload rebuild every borrowed candidate before reuse");
        each([](auto& c) { c.map = 2; });
        compare(
            "map teleport removes prior interest and creates destination entities in source order");
        each([](auto& c) {
            c.known_monsters.clear();
            c.known_items.clear();
            c.known_npcs.clear();
        });
        compare("reconnect knowledge rebuild matches the complete legacy snapshot");
        each([](auto& c) { c.map = 99; });
        compare("an absent map has no candidates and evicts all previous interest");
        expect(index.monsters(99).empty() && index.items(99).empty() && index.npcs(99).empty() &&
                   indexed.session.interest_scratch.empty() &&
                   indexed.session.interest_scratch.capacity() >= scratch_capacity,
               "empty-map passes clear scratch without discarding its retained capacity");
        world.monsters().clear();
        world.ground_items().clear();
        MonsterEntity duplicate_monster;
        duplicate_monster.id = 7777;
        duplicate_monster.map = 0;
        duplicate_monster.x = duplicate_monster.y = 2048;
        duplicate_monster.hp = 100;
        world.monsters().push_back(duplicate_monster);
        world.monsters().push_back(duplicate_monster);
        GroundItem duplicate_item;
        duplicate_item.id = 8888;
        duplicate_item.map = 0;
        duplicate_item.x = duplicate_item.y = 2048;
        duplicate_item.item_index = 1;
        world.ground_items().push_back(duplicate_item);
        world.ground_items().push_back(duplicate_item);
        each([](auto& c) {
            c.map = 0;
            c.known_monsters = {7777, 7778};
            c.known_items = {8888, 8889};
        });
        compare("duplicate visible IDs cannot hide a stale known ID when raw counts coincide");
        expect(indexed.session.known_monsters == std::unordered_set<std::uint32_t>{7777} &&
                   indexed.session.known_items == std::unordered_set<std::uint32_t>{8888},
               "the equal-size fast path compares distinct visible IDs before retaining knowledge");
        world.monsters().clear();
        world.ground_items().clear();
        compare("empty world rebuild clears old candidate buckets without leaving stale pointers");
        expect(index.monsters(0).empty() && index.items(0).empty() &&
                   index.monsters(0).capacity() >= monster_capacity &&
                   index.items(0).capacity() >= item_capacity,
               "empty rebuild retains prior per-map capacities for future spawns");
        std::filesystem::remove_all(root);
    }

    void benchmark_interest() {
        using Clock = std::chrono::steady_clock;
        constexpr int samples = 25;
        std::cout << "# CPU-only stable-known monster interest batches; indexed timings include "
                     "one rebuild.\n"
                  << "# Indexed is forced even for one viewer; selected uses the production "
                     "one-viewer fallback.\n"
                  << "viewers,local_monsters,unrelated_maps,visible_fraction,samples,"
                     "legacy_p50_us,legacy_p95_us,legacy_p99_us,indexed_p50_us,indexed_p95_us,"
                     "indexed_p99_us,selected_p50_us,selected_p95_us,selected_p99_us,"
                     "legacy_candidate_checks,indexed_candidate_checks,rebuild_entities,scratch_"
                     "capacity,index_capacity\n";
        for (int viewers : {1, 16, 64}) {
            for (int count : {256, 1024, 4096}) {
                for (int unrelated_maps : {0, 3}) {
                    for (int density : {0, 1, 2}) {
                        GameWorld world;
                        NpcRegistry npcs;
                        WorldInterestIndex index;
                        for (int map_id = 0; map_id <= unrelated_maps; ++map_id) {
                            for (int i = 0; i < count; ++i) {
                                MonsterEntity m;
                                m.id = static_cast<std::uint32_t>(map_id * count + i + 1);
                                m.map = map_id;
                                m.hp = 100;
                                m.x = 2048 +
                                      (density == 2 || (density == 1 && i % 8 == 0) ? 0 : 5000);
                                m.y = 2048;
                                world.monsters().push_back(m);
                            }
                        }
                        std::vector<std::unique_ptr<InterestViewer>> old_viewers, new_viewers;
                        for (int i = 0; i < viewers; ++i) {
                            old_viewers.push_back(std::make_unique<InterestViewer>());
                            new_viewers.push_back(std::make_unique<InterestViewer>());
                            for (const auto& m : world.monsters()) {
                                if (visible_monster(old_viewers.back()->session, m)) {
                                    old_viewers.back()->session.known_monsters.insert(m.id);
                                    new_viewers.back()->session.known_monsters.insert(m.id);
                                }
                            }
                        }
                        index.rebuild(world, npcs);
                        for (auto& viewer : new_viewers) {
                            sync_monster_interest(viewer->session, world, &index);
                        }
                        std::array<std::vector<double>, 3> timings;
                        const int variants = viewers == 1 ? 3 : 2;
                        for (int sample = 0; sample < samples; ++sample) {
                            for (int pass = 0; pass < variants; ++pass) {
                                const int variant = (sample + pass) % variants;
                                const auto start = Clock::now();
                                if (variant == 0) {
                                    for (auto& viewer : old_viewers) {
                                        legacy_monster_interest(viewer->session, world);
                                    }
                                } else if (variant == 1) {
                                    index.rebuild(world, npcs);
                                    for (auto& viewer : new_viewers) {
                                        sync_monster_interest(viewer->session, world, &index);
                                    }
                                } else {
                                    for (auto& viewer : new_viewers) {
                                        sync_monster_interest(viewer->session, world);
                                    }
                                }
                                timings[variant].push_back(
                                    std::chrono::duration<double, std::micro>(Clock::now() - start)
                                        .count());
                            }
                        }
                        bool stable = true;
                        for (int i = 0; i < viewers; ++i) {
                            stable = stable &&
                                     old_viewers[i]->session.known_monsters ==
                                         new_viewers[i]->session.known_monsters &&
                                     !old_viewers[i]->session.sock->pending() &&
                                     !new_viewers[i]->session.sock->pending();
                        }
                        expect(stable,
                               "interest benchmark preserves known IDs and enqueues no warm-state "
                               "output");
                        for (auto& timing : timings) {
                            std::sort(timing.begin(), timing.end());
                        }
                        const auto percentile = [&](int variant, std::size_t percentage) {
                            const auto& values = timings[variant];
                            return values[(values.size() * percentage + 99) / 100 - 1];
                        };
                        std::cout << viewers << ',' << count << ',' << unrelated_maps << ','
                                  << (density == 0   ? "0"
                                      : density == 1 ? "0.125"
                                                     : "1")
                                  << ',' << samples;
                        for (int variant : {0, 1, viewers == 1 ? 2 : 1}) {
                            std::cout << ',' << std::fixed << std::setprecision(3)
                                      << percentile(variant, 50) << ',' << percentile(variant, 95)
                                      << ',' << percentile(variant, 99);
                        }
                        std::cout << ',' << viewers * world.monsters().size() << ','
                                  << viewers * index.monsters(0).size() << ','
                                  << world.monsters().size() << ','
                                  << new_viewers.front()->session.interest_scratch.capacity() << ','
                                  << index.monsters(0).capacity() << '\n';
                    }
                }
            }
        }
    }

    bool receive_hello(ISession& session, std::vector<std::uint8_t>& rx, int expected) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        int received = 0;
        while (std::chrono::steady_clock::now() < deadline) {
            if (!session.recv_into(rx)) {
                return false;
            }
            for (;;) {
                std::size_t consumed = 0;
                std::uint8_t type = 0;
                std::vector<std::uint8_t> payload;
                const auto status =
                    protocol::FrameParser::pop(rx.data(), rx.size(), consumed, type, payload);
                if (status == protocol::FrameParseStatus::Incomplete) {
                    break;
                }
                if (status != protocol::FrameParseStatus::Complete || type != kTestResponse) {
                    return false;
                }
                rx.erase(rx.begin(), rx.begin() + static_cast<std::ptrdiff_t>(consumed));
                protocol::PacketReader reader(payload);
                std::string response;
                if (!reader.read_string(response) || reader.remaining() ||
                    platform::Json::parse(response).at("server").as_string() !=
                        "OpenKal test control v1") {
                    return false;
                }
                ++received;
            }
            if (received >= expected) {
                return received == expected && rx.empty();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        return false;
    }

    void test_receive_buffer() {
        const auto root = test::temp_dir(("optimization_host_" + std::to_string(getpid())).c_str());
        ServerConfig config;
        config.maps_dir = root + "/maps";
        config.monsters_dir = root + "/monsters";
        config.items_dir = root + "/items";
        config.save_dir = root + "/save";
        config.unix_path = root + "/game.sock";
        config.store = StoreBackend::Sqlite;
        config.transport = TransportKind::Unix;
        config.spawn_x = config.spawn_y = 512;
        config.spawn_z = 0;
        config.max_monsters = 0;
        config.map_radius_cells = 0;
        config.test_control_token = "owned-coverage-token";
        for (const auto& dir : {config.maps_dir, config.monsters_dir, config.items_dir}) {
            test::mkdir_p(dir);
        }
        expect(test::write_open_area(config.maps_dir + "/n_000_000.akarea"),
               "write owned frame-test area");
        std::ofstream(config.monsters_dir + "/1.json")
            << R"({"index":1,"name":"Owned frame-test definition","stats":{"level":1,"hp":1},"spawns":[]})";
        std::ofstream(config.items_dir + "/1.json")
            << R"({"index":1,"name":"Owned frame-test item","class":["general","etc"],"stackable":1})";
        ServerHost host;
        const bool started = host.start(config);
        expect(started, "start server for actual receive-loop characterization");
        if (!started) {
            return;
        }
        for (int attempt = 0; attempt < 200 && !std::filesystem::exists(config.unix_path);
             ++attempt) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        auto session = connect_unix(config.unix_path);
        expect(session != nullptr, "connect receive-loop test peer");
        if (!session) {
            host.stop();
            std::filesystem::remove_all(root);
            return;
        }
        std::vector<std::uint8_t> rx;
        const auto hello = frame_test_request(config.test_control_token,
                                              platform::Json::parse(R"({"op":"hello"})"));
        bool splits_ok = true;
        for (std::size_t split = 1; split < hello.size(); ++split) {
            auto batch = hello;
            batch.insert(
                batch.end(), hello.begin(), hello.begin() + static_cast<std::ptrdiff_t>(split));
            splits_ok = splits_ok && session->send_all(batch) && receive_hello(*session, rx, 1);
            // Receipt of the first command is a barrier for the coalesced prefix.
            // The suffix is completed by a separate write on the same connection.
            splits_ok = splits_ok &&
                        session->send_all(hello.data() + split, hello.size() - split) &&
                        receive_hello(*session, rx, 1);
            if (!splits_ok) {
                break;
            }
        }
        expect(splits_ok,
               "every header/payload split after a valid prefix dispatches exactly once");
        std::vector<std::uint8_t> burst;
        protocol::PacketWriter unknown;
        burst = unknown.frame(241);
        for (int i = 0; i < 256; ++i) {
            burst.insert(burst.end(), hello.begin(), hello.end());
        }
        expect(session->send_all(burst) && receive_hello(*session, rx, 256),
               "unknown opcode and 256 coalesced frames preserve exact dispatch count");
        auto malformed = hello;
        malformed.insert(malformed.end(), {0, 0});
        expect(session->send_all(malformed), "send valid prefix followed by invalid frame length");
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        bool closed = false;
        while (std::chrono::steady_clock::now() < deadline) {
            if (!session->recv_into(rx)) {
                closed = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        expect(closed, "invalid length after a valid prefix closes the established receive buffer");
        std::size_t consumed = 0;
        std::uint8_t response_type = 0;
        std::vector<std::uint8_t> response_payload;
        const auto response_status = protocol::FrameParser::pop(
            rx.data(), rx.size(), consumed, response_type, response_payload);
        protocol::PacketReader response_reader(response_payload);
        std::string response_text;
        expect(response_status == protocol::FrameParseStatus::Complete &&
                   response_type == kTestResponse && consumed == rx.size() &&
                   response_reader.read_string(response_text) && !response_reader.remaining() &&
                   platform::Json::parse(response_text).at("server").as_string() ==
                       "OpenKal test control v1",
               "valid prefix response drains before an invalid trailing frame closes the peer");
        session = connect_unix(config.unix_path);
        rx.clear();
        expect(session && session->send_all(hello) && receive_hello(*session, rx, 1),
               "reconnect receives no stale frames from the invalid predecessor");
        if (session) {
            session->close();
        }
        host.stop();
        std::filesystem::remove_all(root);
    }

    // Stable numeric output covers the complete calculator write set and untouched control inputs.
    std::string combat_session_state(const ClientSession& c) {
        std::ostringstream out;
        out << std::setprecision(17) << '{';
        const char* separator = "";
        const auto field = [&](const char* name, auto value) {
            out << separator << '"' << name << "\":" << value;
            separator = ",";
        };
#define COMBAT_FIELD(name) field(#name, c.name)
        COMBAT_FIELD(str);
        COMBAT_FIELD(hth);
        COMBAT_FIELD(intl);
        COMBAT_FIELD(wis);
        COMBAT_FIELD(dex);
        COMBAT_FIELD(stats_initialized);
        COMBAT_FIELD(prefix_bonus.str);
        COMBAT_FIELD(prefix_bonus.hth);
        COMBAT_FIELD(prefix_bonus.intl);
        COMBAT_FIELD(prefix_bonus.wis);
        COMBAT_FIELD(prefix_bonus.dex);
        COMBAT_FIELD(min_attack);
        COMBAT_FIELD(max_attack);
        COMBAT_FIELD(min_magic);
        COMBAT_FIELD(max_magic);
        COMBAT_FIELD(hit);
        COMBAT_FIELD(dodge);
        COMBAT_FIELD(defense);
        COMBAT_FIELD(defense_range);
        COMBAT_FIELD(absorb);
        COMBAT_FIELD(fatal_rate);
        COMBAT_FIELD(fatal_pt);
        COMBAT_FIELD(perfect_evasion_base);
        COMBAT_FIELD(perfect_evasion_advanced);
        COMBAT_FIELD(block_percent);
        COMBAT_FIELD(wearing_shield);
        COMBAT_FIELD(wearing_two_hand);
        COMBAT_FIELD(upgrade_level);
        COMBAT_FIELD(char_attack);
        COMBAT_FIELD(char_magic);
        COMBAT_FIELD(weapon_min_attack);
        COMBAT_FIELD(weapon_max_attack);
        COMBAT_FIELD(weapon_min_magic);
        COMBAT_FIELD(weapon_max_magic);
        COMBAT_FIELD(res_fire);
        COMBAT_FIELD(res_ice);
        COMBAT_FIELD(res_lightning);
        COMBAT_FIELD(res_curse);
        COMBAT_FIELD(res_palsy);
        COMBAT_FIELD(attack_range);
        COMBAT_FIELD(attack_speed_ms);
        COMBAT_FIELD(cur_hp);
        COMBAT_FIELD(max_hp);
        COMBAT_FIELD(cur_mp);
        COMBAT_FIELD(max_mp);
        COMBAT_FIELD(transform_mspeed_percent);
        COMBAT_FIELD(gstate);
        COMBAT_FIELD(collection_pve);
        COMBAT_FIELD(collection_pvp_until_ms);
        COMBAT_FIELD(collection_bonus.attack);
        COMBAT_FIELD(collection_bonus.hp);
        COMBAT_FIELD(collection_bonus.mp);
        COMBAT_FIELD(clazz);
        COMBAT_FIELD(level);
        COMBAT_FIELD(specialty);
        COMBAT_FIELD(talent_mask);
        COMBAT_FIELD(weapon_index);
        COMBAT_FIELD(armor_index);
        COMBAT_FIELD(weapon_prefix);
        COMBAT_FIELD(armor_prefix);
        COMBAT_FIELD(weapon_iid);
        COMBAT_FIELD(armor_iid);
        COMBAT_FIELD(transform.index);
        COMBAT_FIELD(transform.level);
        COMBAT_FIELD(transform.exp);
        COMBAT_FIELD(transform.put_on_ticks);
        COMBAT_FIELD(transform_stage);
        COMBAT_FIELD(mounted);
        COMBAT_FIELD(next_action_ms);
        COMBAT_FIELD(control_until_ms);
        COMBAT_FIELD(next_dec_rage_ms);
        COMBAT_FIELD(next_transform_tick_ms);
        COMBAT_FIELD(next_stand_hp_ms);
        COMBAT_FIELD(next_stand_mp_ms);
        COMBAT_FIELD(next_sit_regen_ms);
        COMBAT_FIELD(next_meditation_ms);
        COMBAT_FIELD(next_knight_recovery_ms);
        COMBAT_FIELD(next_pet_heal_ms);
        COMBAT_FIELD(next_archer_mp_drain_ms);
        COMBAT_FIELD(last_autosave_ms);
#undef COMBAT_FIELD
        field("collection_world_bound", c.collection_world != nullptr);
        field("skill_bstate", c.skill_buffs.bstate());
        field("pay_bstate", c.pay_buffs.bstate());
        field("queued_bytes", c.sock ? c.sock->pending_bytes() : 0);
        out << ",\"wear\":[";
        separator = "";
        for (auto value : c.wear) {
            out << separator << value;
            separator = ",";
        }
        out << "],\"collection_ability\":[";
        separator = "";
        for (auto value : c.collection_bonus.ability) {
            out << separator << value;
            separator = ",";
        }
        return out.str() + "]}";
    }

    std::string combat_inventory_state(const ClientSession& c) {
        std::ostringstream out;
        const auto slot = [&](const ClientSession::BagSlot& item) {
            std::apply([&](auto... values) { ((out << values << ','), ...); },
                       std::tie(item.iid,
                                item.index,
                                item.prefix,
                                item.count,
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
                                item.enchant_protected,
                                item.random_stat_type,
                                item.random_stat_value,
                                item.transform_level,
                                item.transform_exp,
                                item.pet_expire_at,
                                item.ride_expire_at,
                                item.account_bound,
                                item.character_bound,
                                item.equipped));
        };
        for (const auto& item : c.bag) {
            slot(item);
        }
        for (const auto& item : c.collection) {
            slot(item);
        }
        for (const auto& [id, rank] :
             std::map<int, int>(c.learned_skills.begin(), c.learned_skills.end())) {
            out << id << ':' << rank << ';';
        }
        for (const auto& [iid, mount] : c.battle_mounts) {
            out << iid << ':' << mount.mount_iid << ':' << static_cast<int>(mount.grade) << ':'
                << mount.satiety << ':' << mount.max_satiety << ':' << mount.revision << ':';
            for (auto ability : mount.abilities) {
                out << ability << ',';
            }
            for (auto equipment : mount.equipment_iids) {
                out << equipment << ',';
            }
        }
        return out.str();
    }

    void combat_fixture_world(GameWorld& world) {
        for (int clazz = 0; clazz < 4; ++clazz) {
            ItemCombatDef weapon;
            weapon.index = 101 + clazz;
            weapon.class_big = "weapon";
            weapon.class_small =
                std::array<const char*, 4>{"sword2h", "wand", "bow", "dagger"}[clazz];
            weapon.is_weapon = true;
            weapon.min_attack = 101 + clazz;
            weapon.max_attack = 207 + clazz;
            weapon.min_magic = 53 + clazz;
            weapon.max_magic = 89 + clazz;
            weapon.hit = 7;
            weapon.range = 49;
            weapon.attack_speed_ms = 753;
            GameWorldCoveragePeer::add_item(world, weapon);
        }
        ItemCombatDef armor;
        armor.index = 201;
        armor.is_defense = true;
        armor.class_big = "defense";
        armor.class_small = "shield";
        armor.grade = 5;
        armor.defense_melee = 47;
        armor.defense_range = 73;
        armor.hp = 31;
        armor.mp = 29;
        armor.res_fire = 13;
        armor.res_ice = 17;
        armor.res_lightning = 19;
        armor.res_curse = 23;
        armor.res_palsy = 29;
        GameWorldCoveragePeer::add_item(world, armor);
        armor.index = 202;
        armor.class_small = "helmet";
        armor.defense_melee = 3;
        armor.defense_range = 5;
        armor.hp = 7;
        armor.mp = 11;
        GameWorldCoveragePeer::add_item(world, armor);
        PrefixCombatDef prefix;
        prefix.index = 11;
        prefix.str = 2;
        prefix.hth = 3;
        prefix.intl = 5;
        prefix.wis = 7;
        prefix.dex = 11;
        prefix.hp = 13;
        prefix.mp = 17;
        prefix.defense = 19;
        prefix.hit = 23;
        prefix.min_attack = 29;
        GameWorldCoveragePeer::add_prefix(world, prefix);
        RidingDef riding;
        riding.kind = protocol::RidingKind::Battle;
        GameWorldCoveragePeer::add_riding(world, 301, riding);
        ItemCombatDef mount;
        mount.index = 301;
        GameWorldCoveragePeer::add_item(world, mount);
        ItemCombatDef egg;
        egg.index = 401;
        egg.is_transform = true;
        egg.transform_race = 8;
        GameWorldCoveragePeer::add_item(world, egg);
        const auto directory =
            test::temp_dir(("combat_snapshot_" + std::to_string(getpid())).c_str());
        std::ofstream(directory + "/bof.json")
            << R"({"minGrade":5,"halfCount":3,"fullCount":5,"halfDefensePercent":7,"fullHpMpPercent":11,"fullResistPercent":13,"armorParts":["helmet"],"fxLevels":[5],"tiers":[{"grade":5,"piece":2,"halfSet":3,"fullSet":4}]})";
        expect(world.load_bof_json(directory + "/bof.json"),
               "load owned combat snapshot Bof policy");
        std::filesystem::remove_all(directory);
    }

    ClientSession combat_fixture(int clazz, bool initialized, int loadout) {
        ClientSession c;
        c.clazz = clazz;
        c.level = 110;
        c.stats_initialized = initialized;
        c.str = 101;
        c.hth = 97;
        c.intl = 89;
        c.wis = 83;
        c.dex = 79;
        c.specialty = combat::kSecondJobAttrib1;
        c.next_action_ms = 111;
        c.control_until_ms = 222;
        c.next_dec_rage_ms = 333;
        c.next_transform_tick_ms = 444;
        c.next_stand_hp_ms = 555;
        c.next_stand_mp_ms = 666;
        c.next_sit_regen_ms = 777;
        c.next_meditation_ms = 888;
        c.next_knight_recovery_ms = 999;
        c.next_pet_heal_ms = 123;
        c.next_archer_mp_drain_ms = 234;
        c.last_autosave_ms = 345;
        c.collection_pvp_until_ms = 456;
        c.gstate = 0x10208040u;
        for (int i = 0; i < 7; ++i) {
            c.wear[i] = 900 + i;
        }
        const std::array<int, 6> hp{0, -7, 1000000, 7, 1, 19};
        const std::array<int, 6> mp{0, -11, 1000000, 13, 1, 23};
        c.cur_hp = hp[loadout];
        c.cur_mp = mp[loadout];
        if (loadout == 0) {
            return c;
        }
        c.weapon_index = 101 + clazz;
        c.armor_index = 201;
        c.weapon_prefix = c.armor_prefix = 11;
        if (loadout == 1) {
            return c;
        }
        c.weapon_iid = 11;
        c.armor_iid = 12;
        for (int i = 0; i < 2; ++i) {
            auto& item = c.bag[i];
            item.iid = 11 + i;
            item.index = i == 0 ? c.weapon_index : c.armor_index;
            item.count = 1;
            item.equipped = true;
            item.prefix = 11;
            item.x_attack = 3;
            item.x_magic = 5;
            item.x_hit = 7;
            item.x_defense = 11;
            item.x_dodge = 13;
            item.upgrade_level = 4;
            item.upgrade_rate = 17;
            item.cur_end = 19;
            item.max_end = 23;
            item.mix_type = i == 0 ? 10 | (1 << 4) : 8 | (2 << 4);
            item.qigong_effect = i == 0 ? 31 : 27;
            item.dragon_effect = i == 0 ? 85 : 111;
            item.random_stat_type = i == 0 ? 5 : 13;
            item.random_stat_value = 3;
        }
        for (int skill : {2, 5, 6, 8, 12, 17, 20, 30, 32, 36, 37, 39, 48, 67, 68, 70, 71}) {
            c.learned_skills[skill] = 3;
        }
        apply_buff(c, SkillBuffId::RefiningWeapon, 7);
        apply_buff(c, SkillBuffId::StoneShield, 11, 3);
        apply_buff(c, SkillBuffId::MageMagicIntensification, 2);
        apply_buff(c, SkillBuffId::ArcherCurseStrength, -11);
        apply_buff(c, SkillBuffId::ArcherCurseWisdom, -13);
        apply_buff(c, SkillBuffId::ArcherCurseDefense, -17, -19);
        apply_buff(c, SkillBuffId::ArcherMeshSpeed, -7);
        for (int node = 0; node < protocol::kTalentNodeCount; ++node) {
            const auto mask = c.talent_mask | protocol::talent_bit(node);
            if (protocol::valid_talents(mask, c.level)) {
                c.talent_mask = mask;
            }
        }
        if (loadout == 2) {
            return c;
        }
        for (int i = 0; i < 5; ++i) {
            auto& item = c.bag[2 + i];
            item.iid = 20 + i;
            item.index = 202;
            item.count = 1;
            item.equipped = true;
            item.bof = 1;
        }
        c.mounted = true;
        c.bag[7].iid = 30;
        c.bag[7].index = 301;
        c.bag[7].count = 1;
        c.bag[7].equipped = true;
        auto& mount = c.battle_mounts[30];
        mount.satiety = 37;
        mount.max_satiety = 100;
        for (int i = 0; i < static_cast<int>(mount.equipment_iids.size()); ++i) {
            c.bag[8 + i].iid = 40 + i;
            c.bag[8 + i].index = 30218 + i + (i == 4 && loadout % 2 ? 1 : 0);
            c.bag[8 + i].count = 1;
            c.bag[8 + i].upgrade_level = 3;
            mount.equipment_iids[i] = 40 + i;
        }
        c.transform.index = 401;
        c.transform.level = 37;
        c.transform_stage = 3;
        for (int i = 0; i < 9; ++i) {
            auto& item = c.collection[i];
            item.iid = 100 + i;
            item.index = i == 0 ? kYinYangMirrorFirst + 1 : kTrigramFirst + 8 + i - 1;
            item.count = 1;
            item.upgrade_level = 7;
            item.equipped = true;
        }
        if (loadout >= 4) {
            c.bag[0].count = 0;
            c.weapon_iid = loadout == 4 ? 11 : 0;
        }
        return c;
    }

    void write_combatant(std::ostream& out, const combat::Combatant& value) {
        const char* separator = "";
        out << '[' << std::setprecision(17);
        std::apply([&](auto... fields) { ((out << separator << fields, separator = ","), ...); },
                   combat_values(value));
        out << ']';
    }

    void write_combat_damage(std::ostream& out,
                             const combat::Combatant& attacker,
                             bool player,
                             unsigned seed) {
        combat::Combatant target;
        target.level = 109;
        target.clazz = combat::kPcKnight;
        target.is_player = player;
        target.defense = 57;
        target.defense_range = 91;
        target.absorb = 7;
        target.dodge = 31;
        target.block_percent = player ? 17 : 0;
        target.wearing_shield = player;
        target.wearing_two_hand = player;
        target.has_parry = player;
        target.has_second_job_attrib = player;
        std::srand(seed);
        out << "{\"seed\":" << seed << ",\"rolls\":[";
        for (int roll = 0; roll < 8; ++roll) {
            if (roll) {
                out << ',';
            }
            const auto result = combat::resolve_physical(
                attacker,
                target,
                roll * 7 - 14,
                roll * 3,
                std::array<int, 8>{-1, -1, 0, 1, 101, 257, 999, 1500}[roll],
                std::array<int, 4>{-1, 0, 375, 753}[roll % 4],
                std::array<int, 4>{0, 700, 753, 1200}[roll % 4],
                roll % 4 == 0);
            out << "{\"hit\":" << result.hit << ",\"fatal\":" << result.fatal
                << ",\"explosive\":" << result.explosive << ",\"blocked\":" << result.blocked
                << ",\"damage\":" << result.damage << ",\"palsy\":" << result.palsy
                << ",\"ice_slow_duration_sec\":" << result.ice_slow_duration_sec
                << ",\"ice_slow_percent\":" << result.ice_slow_percent << '}';
        }
        out << "],\"next_rand\":" << std::rand() << '}';
    }

    void combat_snapshot() {
        std::ostringstream records;
        GameWorld world;
        combat_fixture_world(world);
        for (int clazz = 0; clazz < 4; ++clazz) {
            for (bool initialized : {false, true}) {
                for (int loadout = 0; loadout < 6; ++loadout) {
                    for (bool pve : {false, true}) {
                        auto c = combat_fixture(clazz, initialized, loadout);
                        c.collection_pve = pve;
                        recompute_combat(c, world);
                        const auto state = combat_session_state(c);
                        const auto inventory = combat_inventory_state(c);
                        const auto seed = static_cast<unsigned>(
                            1000 + clazz * 100 + int(initialized) * 40 + loadout * 4 + int(pve));
                        std::srand(seed);
                        const auto preview = session_combatant(c, false, false);
                        const int preview_next_rand = std::rand();
                        expect(combat_session_state(c) == state &&
                                   combat_inventory_state(c) == inventory,
                               "preview preserves an already computed profile and its inventory");
                        const auto combatant = session_combatant(c, pve, false);
                        records << "{\"class\":" << clazz
                                << ",\"initialized_input\":" << int(initialized)
                                << ",\"loadout\":" << loadout << ",\"pve\":" << int(pve)
                                << ",\"state\":" << state
                                << ",\"preview_next_rand\":" << preview_next_rand
                                << ",\"preview\":";
                        write_combatant(records, preview);
                        records << ",\"combatant\":";
                        write_combatant(records, combatant);
                        records << ",\"damage_player\":";
                        write_combat_damage(records, preview, true, seed);
                        records << ",\"damage_monster\":";
                        write_combat_damage(records, combatant, false, seed + 1);
                        records << "}\n";
                    }
                }
            }
        }
        std::cout << records.str();
    }

    void test_preview_purity() {
        GameWorld world;
        combat_fixture_world(world);
        for (int clazz = 0; clazz < 4; ++clazz) {
            for (bool initialized : {false, true}) {
                for (int loadout = 0; loadout < 6; ++loadout) {
                    auto reference = combat_fixture(clazz, initialized, loadout);
                    reference.collection_pve = false;
                    recompute_combat(reference, world);
                    const auto expected = combat_values(session_combatant(reference, false, false));
                    auto c = combat_fixture(clazz, initialized, loadout);
                    c.collection_world = &world;
                    c.min_attack = -31;
                    c.max_attack = 47;
                    c.hit = 59;
                    c.prefix_bonus = {2, 3, 5, 7, 11};
                    c.collection_bonus.attack = 13;
                    c.collection_bonus.ability[13] = 1.25;
                    c.transform_mspeed_percent = 17;
                    auto recording = std::make_unique<RecordingSession>();
                    const auto* output = recording.get();
                    c.sock = std::make_unique<HostConnection>(std::move(recording));
                    const auto before = combat_session_state(c);
                    const auto inventory = combat_inventory_state(c);
                    for (int repeat = 0; repeat < 3; ++repeat) {
                        expect(combat_values(session_combatant(c, false, false)) == expected,
                               "alternate preview matches an independent authoritative PvP "
                               "calculation");
                    }
                    expect(combat_session_state(c) == before &&
                               combat_inventory_state(c) == inventory &&
                               c.collection_world == &world && output->packets.empty() &&
                               !c.sock->pending(),
                           "repeated preview leaves uninitialized/stale stats, resources, wear, "
                           "bits, inputs and deadlines unchanged");
                }
            }
        }
    }

    void combat_work_probe(bool preview) {
        GameWorld world;
        combat_fixture_world(world);
        auto c = combat_fixture(2, true, 3);
        recompute_combat(c, world);
        if (preview) {
            (void) session_combatant(c, false, false);
        }
        std::cout << "combat_work_probe preview=" << preview << '\n';
    }

    void benchmark_combat_preview() {
        GameWorld world;
        combat_fixture_world(world);
        std::cout
            << "combat preview benchmark: 25 samples,256 previews per sample; no timing threshold\n"
               "class,samples,previews_per_sample,median_us,p95_us\n";
        std::uint64_t checksum = 0;
        for (int clazz = 0; clazz < 4; ++clazz) {
            auto c = combat_fixture(clazz, true, 3);
            recompute_combat(c, world);
            std::vector<double> samples;
            for (int sample = 0; sample < 25; ++sample) {
                const auto start = std::chrono::steady_clock::now();
                for (int call = 0; call < 256; ++call) {
                    checksum +=
                        static_cast<std::uint64_t>(session_combatant(c, false, false).max_attack);
                }
                samples.push_back(std::chrono::duration<double, std::micro>(
                                      std::chrono::steady_clock::now() - start)
                                      .count());
            }
            std::sort(samples.begin(), samples.end());
            std::cout << clazz << ",25,256," << std::fixed << std::setprecision(3) << samples[12]
                      << ',' << samples[23] << '\n';
        }
        std::cout << "checksum=" << checksum << '\n';
    }

    struct RequestClockTrace {
        std::string reads;
        std::vector<std::int64_t> unix_values{2000000, 2000005};
        std::vector<std::uint64_t> steady_values{1000, 1010, 1020};
        std::size_t unix_index = 0, steady_index = 0;
        std::function<void(std::size_t)> observe_steady;
    };
    RequestClockTrace request_clock;

    std::int64_t request_unix_ms() {
        request_clock.reads += 'U';
        const auto index = request_clock.unix_index++;
        expect(index < request_clock.unix_values.size(), "bounded request Unix clock reads");
        return request_clock.unix_values[std::min(index, request_clock.unix_values.size() - 1)];
    }

    std::uint64_t request_steady_ms() {
        request_clock.reads += 'S';
        const auto index = request_clock.steady_index++;
        expect(index < request_clock.steady_values.size(), "bounded request steady clock reads");
        if (request_clock.observe_steady) {
            request_clock.observe_steady(index);
        }
        return request_clock.steady_values[std::min(index, request_clock.steady_values.size() - 1)];
    }

    struct RequestPeer {
        ClientSession c;
        RecordingSession* output = nullptr;
        RequestPeer() {
            auto recording = std::make_unique<RecordingSession>();
            output = recording.get();
            c.sock = std::make_unique<HostConnection>(std::move(recording));
            c.phase = SessionPhase::Playing;
            c.player_id = 700;
            c.level = 110;
            c.stats_initialized = true;
            c.cur_hp = 100;
            c.cur_mp = 50;
            c.aggro_immune_left = 5.0f;
        }
        std::vector<std::vector<std::uint8_t>> drain() {
            (void) c.sock->flush(POLLIN | POLLOUT);
            auto result = std::move(output->packets);
            output->packets.clear();
            return result;
        }
    };

    std::vector<std::uint8_t> talent_payload(protocol::TalentCommand command,
                                             std::uint64_t expected = 0,
                                             std::uint64_t desired = 0) {
        const auto frame = protocol::frame_talent_request(command, expected, desired);
        std::vector<std::uint8_t> payload;
        std::size_t consumed = 0;
        std::uint8_t type = 0;
        expect(protocol::FrameParser::pop(frame.data(), frame.size(), consumed, type, payload) ==
                       protocol::FrameParseStatus::Complete &&
                   consumed == frame.size(),
               "decode complete owned talent request frame");
        return payload;
    }

    std::uint64_t beacon_mask() {
        std::uint64_t result = 0;
        for (int node = 20; node < 29; ++node) {
            result |= protocol::talent_bit(node);
        }
        for (int node = 31; node < 37; ++node) {
            result |= protocol::talent_bit(node);
        }
        return result;
    }

    void expect_talent_reply(RequestPeer& peer,
                             protocol::TalentStatus status,
                             std::uint32_t cooldown,
                             std::uint32_t active,
                             const char* reason) {
        protocol::TalentState expected;
        expected.mask = peer.c.talent_mask;
        expected.level = peer.c.level;
        expected.status = status;
        expected.beacon_cooldown_ms = cooldown;
        expected.beacon_active_ms = active;
        expect(peer.drain() ==
                   std::vector<std::vector<std::uint8_t>>{protocol::frame_talent_state(expected)},
               reason);
    }

    void test_talent_request_replay() {
        GameWorld world;
        const TalentRequestClock clock{request_unix_ms, request_steady_ms};
        ClientSession disconnected;
        request_clock = {};
        send_talent_state(disconnected, protocol::TalentStatus::Ok, clock);
        expect(!disconnected.sock && request_clock.reads.empty(),
               "talent state without a transport reads neither request clock");
        const auto invoke =
            [&](RequestPeer& peer, const auto& payload, IDataStore* store = nullptr) {
                request_clock = {};
                return handle_talent_request(peer.c, payload, world, store, clock);
            };
        RequestPeer malformed;
        const auto complete = talent_payload(protocol::TalentCommand::Apply, 0, 1);
        const auto before = combat_session_state(malformed.c);
        for (std::size_t length = 0; length < complete.size(); ++length) {
            const std::vector<std::uint8_t> truncated(complete.begin(), complete.begin() + length);
            expect(invoke(malformed, truncated) == HostRequestResult::Malformed &&
                       request_clock.reads.empty() && !malformed.c.sock->pending() &&
                       combat_session_state(malformed.c) == before,
                   "every truncated Apply has no clock, state or packet effect");
        }
        for (auto command : {protocol::TalentCommand::Snapshot,
                             protocol::TalentCommand::Apply,
                             protocol::TalentCommand::Beacon}) {
            auto trailing = talent_payload(command);
            trailing.push_back(0xa5);
            expect(invoke(malformed, trailing) == HostRequestResult::Malformed &&
                       request_clock.reads.empty() && !malformed.c.sock->pending(),
                   "talent request trailing bytes remain malformed");
        }
        expect(invoke(malformed, std::vector<std::uint8_t>{255}) == HostRequestResult::Malformed,
               "unknown talent command remains malformed");
        expect(invoke(malformed, talent_payload(protocol::TalentCommand::Snapshot)) ==
                       HostRequestResult::Handled &&
                   request_clock.reads == "SU",
               "valid request after malformed input reads only ordered reply clocks");
        expect_talent_reply(malformed, protocol::TalentStatus::Ok, 0, 0, "exact snapshot response");

        for (int guard = 0; guard < 6; ++guard) {
            RequestPeer peer;
            peer.c.talent_mask = beacon_mask();
            if (guard == 0) {
                peer.c.talent_mask = 0;
            }
            if (guard == 1) {
                peer.c.cur_hp = 0;
            }
            if (guard == 2) {
                peer.c.resting = true;
            }
            if (guard == 3) {
                peer.c.tele_pending = true;
            }
            if (guard == 4) {
                peer.c.beacon_ready_at = 2000001;
            }
            if (guard == 5) {
                peer.c.control_until_ms = 1001;
            }
            const auto ready = peer.c.beacon_ready_at;
            expect(invoke(peer, talent_payload(protocol::TalentCommand::Beacon)) ==
                           HostRequestResult::Handled &&
                       request_clock.reads == (guard == 5 ? "USSU" : "USU") &&
                       peer.c.beacon_ready_at == ready && peer.c.beacon_active_until_ms == 0 &&
                       peer.c.aggro_immune_left == 5.0f,
                   "Beacon guards preserve state and conditional clock evaluation order");
            expect_talent_reply(peer,
                                protocol::TalentStatus::Unavailable,
                                0,
                                0,
                                "exact unavailable Beacon response");
        }
        RequestPeer boundary;
        boundary.c.talent_mask = beacon_mask();
        boundary.c.beacon_ready_at = 2000000;
        boundary.c.control_until_ms = 1000;
        expect(invoke(boundary, talent_payload(protocol::TalentCommand::Beacon)) ==
                       HostRequestResult::Handled &&
                   request_clock.reads == "USSSU" && boundary.c.beacon_ready_at == 2120000 &&
                   boundary.c.beacon_active_until_ms == 31010 &&
                   boundary.c.aggro_immune_left == 0.0f,
               "exact cooldown and control deadlines permit Beacon with a fresh activation clock");
        expect_talent_reply(boundary,
                            protocol::TalentStatus::Ok,
                            119995,
                            29990,
                            "exact successful Beacon response");
        request_clock = {};
        request_clock.unix_values = {2000010, 2000015};
        request_clock.steady_values = {1030};
        expect(handle_talent_request(boundary.c,
                                     talent_payload(protocol::TalentCommand::Beacon),
                                     world,
                                     nullptr,
                                     clock) == HostRequestResult::Handled &&
                   request_clock.reads == "USU" && boundary.c.beacon_active_until_ms == 31010,
               "duplicate Beacon cannot extend its committed effect");
        expect_talent_reply(boundary,
                            protocol::TalentStatus::Unavailable,
                            119985,
                            29980,
                            "duplicate Beacon reports existing deadlines");

        const auto root = test::temp_dir(("request_replay_" + std::to_string(getpid())).c_str());
        auto store = create_data_store(StoreBackend::Sqlite);
        expect(store->open(root), "open owned request replay store");
        CharacterRecord initial;
        initial.name = "Owned request replay";
        initial.level = 110;
        initial.hp = 100;
        initial.mp = 50;
        expect(store->ensure_default_player("request_replay", "owned", initial),
               "seed owned request replay character");
        const auto account = store->find_account("request_replay");
        const auto characters =
            account ? store->list_characters(account->id) : std::vector<CharacterRecord>{};
        if (characters.empty()) {
            expect(false, "find owned request replay character");
            store->close();
            std::filesystem::remove_all(root);
            return;
        }
        sqlite3* raw = nullptr;
        expect(sqlite3_open((root + "/" + kSqliteFileName).c_str(), &raw) == SQLITE_OK,
               "open owned request persistence audit connection");
        std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db(raw, sqlite3_close);
        const auto sql = [&](const char* statement) {
            expect(sqlite3_exec(db.get(), statement, nullptr, nullptr, nullptr) == SQLITE_OK,
                   "execute owned request persistence audit policy");
        };
        sql("CREATE TABLE request_audit(seq INTEGER PRIMARY KEY,mask INTEGER,beacon INTEGER);"
            "CREATE TRIGGER audit_requests AFTER UPDATE ON characters BEGIN INSERT INTO "
            "request_audit(mask,beacon) VALUES(NEW.talent_mask,NEW.beacon_ready_at); END;");
        const auto audit = [&] {
            std::vector<std::pair<std::int64_t, std::int64_t>> rows;
            sqlite3_stmt* statement = nullptr;
            expect(sqlite3_prepare_v2(db.get(),
                                      "SELECT mask,beacon FROM request_audit ORDER BY seq",
                                      -1,
                                      &statement,
                                      nullptr) == SQLITE_OK,
                   "read ordered request persistence records");
            if (statement) {
                while (sqlite3_step(statement) == SQLITE_ROW) {
                    rows.emplace_back(sqlite3_column_int64(statement, 0),
                                      sqlite3_column_int64(statement, 1));
                }
                sqlite3_finalize(statement);
            }
            return rows;
        };
        RequestPeer apply;
        apply.c.character_id = characters.front().id;
        apply.c.account_id = account->id;
        apply.c.name = initial.name;
        sql("CREATE TRIGGER reject_request BEFORE UPDATE ON characters BEGIN SELECT "
            "RAISE(ABORT,'owned request failure'); END;");
        expect(invoke(apply, complete, store.get()) == HostRequestResult::Handled &&
                   apply.c.talent_mask == 0 && audit().empty() && !apply.c.collection_world &&
                   request_clock.reads == "SU",
               "failed talent save rolls back mask without recalculation or progression");
        expect_talent_reply(
            apply, protocol::TalentStatus::SaveFailed, 0, 0, "exact failed Apply response");
        sql("DROP TRIGGER reject_request;");
        expect(invoke(apply, complete, store.get()) == HostRequestResult::Handled &&
                   apply.c.talent_mask == 1 && request_clock.reads == "SU" &&
                   audit() == std::vector<std::pair<std::int64_t, std::int64_t>>{{1, 0}},
               "successful Apply commits exactly one ordered character record");
        RequestPeer expected;
        expected.c.talent_mask = 1;
        recompute_combat(expected.c, world);
        send_progression_snapshot(expected.c, false);
        send_skill_info(expected.c);
        protocol::TalentState reply;
        reply.mask = 1;
        reply.level = 110;
        expected.c.sock->enqueue(protocol::frame_talent_state(reply));
        expect(apply.drain() == expected.drain(),
               "Apply emits progression, skill info and final talent state in exact wire order");
        expect(invoke(apply, complete, store.get()) == HostRequestResult::Handled &&
                   audit().size() == 1 && apply.c.talent_mask == 1,
               "duplicate stale draft cannot create an extra persistence record");
        expect_talent_reply(
            apply, protocol::TalentStatus::Stale, 0, 0, "exact stale Apply response");
        for (int guard = 0; guard < 3; ++guard) {
            apply.c.cur_hp = guard == 0 ? 0 : 100;
            apply.c.tele_pending = guard == 1;
            const auto desired = guard == 2 ? 0 : 1;
            expect(invoke(apply,
                          talent_payload(protocol::TalentCommand::Apply, 1, desired),
                          store.get()) == HostRequestResult::Handled &&
                       apply.c.talent_mask == 1 && audit().size() == 1,
                   "dead, teleporting and invalid-removal Apply preserve committed state");
            expect_talent_reply(
                apply, protocol::TalentStatus::Invalid, 0, 0, "exact invalid Apply response");
        }
        apply.c.cur_hp = 100;
        apply.c.tele_pending = false;
        apply.c.talent_mask = beacon_mask();
        apply.c.beacon_ready_at = 7;
        sql("CREATE TRIGGER reject_request BEFORE UPDATE ON characters BEGIN SELECT "
            "RAISE(ABORT,'owned request failure'); END;");
        expect(invoke(apply, talent_payload(protocol::TalentCommand::Beacon), store.get()) ==
                       HostRequestResult::Handled &&
                   request_clock.reads == "USSU" && apply.c.beacon_ready_at == 7 &&
                   apply.c.beacon_active_until_ms == 0 && apply.c.aggro_immune_left == 5.0f &&
                   audit().size() == 1,
               "failed Beacon save restores cooldown and never reads activation time");
        expect_talent_reply(
            apply, protocol::TalentStatus::SaveFailed, 0, 0, "exact failed Beacon response");
        sql("DROP TRIGGER reject_request;");
        request_clock = {};
        request_clock.observe_steady = [&](std::size_t index) {
            if (index == 1) {
                const auto saved = store->load_character(apply.c.character_id);
                expect(saved && saved->beacon_ready_at == 2120000,
                       "Beacon cooldown is durable before activation time is sampled");
            }
        };
        expect(handle_talent_request(apply.c,
                                     talent_payload(protocol::TalentCommand::Beacon),
                                     world,
                                     store.get(),
                                     clock) == HostRequestResult::Handled &&
                   request_clock.reads == "USSSU" && audit().size() == 2 &&
                   audit().back() == std::make_pair(static_cast<std::int64_t>(beacon_mask()),
                                                    std::int64_t{2120000}),
               "successful Beacon persists once before its activation and response");
        expect_talent_reply(
            apply, protocol::TalentStatus::Ok, 119995, 29990, "exact saved Beacon response");
        RequestPeer independent;
        expect(invoke(independent, talent_payload(protocol::TalentCommand::Snapshot)) ==
                       HostRequestResult::Handled &&
                   independent.c.talent_mask == 0 && independent.c.beacon_ready_at == 0 &&
                   apply.c.beacon_ready_at == 2120000,
               "interleaved client snapshot cannot inherit another client's state or clocks");
        expect_talent_reply(
            independent, protocol::TalentStatus::Ok, 0, 0, "independent client exact response");
        store->close();
        db.reset();
        std::filesystem::remove_all(root);
    }

    using ScheduledPackets = std::array<std::vector<std::vector<std::uint8_t>>, 3>;
    bool scheduled_snapshot = false;
    std::int64_t scheduled_unix_value = 1000;
    int scheduled_unix_reads = 0;

    std::int64_t scheduled_unix_seconds() {
        ++scheduled_unix_reads;
        return scheduled_unix_value;
    }

    struct ScheduledPlayers {
        GameWorld world;
        std::vector<ClientSession> clients{3};
        std::array<RecordingSession*, 3> outputs{};

        ScheduledPlayers() {
            for (std::size_t i = 0; i < clients.size(); ++i) {
                auto recording = std::make_unique<RecordingSession>();
                outputs[i] = recording.get();
                auto& c = clients[i];
                c.sock = std::make_unique<HostConnection>(std::move(recording));
                c.phase = SessionPhase::Playing;
                c.player_id = 700 + static_cast<std::uint32_t>(i);
                c.map = i == 2 ? 1 : 0;
                c.px = 2048 + static_cast<int>(i) * 16;
                c.py = 2048;
                c.level = 30;
                c.stats_initialized = true;
                c.str = c.hth = c.intl = c.wis = c.dex = 30;
                c.next_stand_hp_ms = c.next_stand_mp_ms = 1000000;
                c.next_talent_regen_ms = 1000000;
                c.next_transform_tick_ms = 1000000;
            }
        }

        ClientSession& owner() {
            return clients.front();
        }

        void tick(std::uint64_t now_ms, std::int64_t unix_seconds, IDataStore* store = nullptr) {
            scheduled_unix_value = unix_seconds;
            scheduled_unix_reads = 0;
            tick_player_regen_and_transform(
                owner(), clients, world, store, now_ms, scheduled_unix_seconds);
            expect(scheduled_unix_reads == (owner().phase == SessionPhase::Playing ? 1 : 0),
                   "scheduled player reads Unix seconds once only after the Playing guard");
        }

        ScheduledPackets drain() {
            ScheduledPackets packets;
            for (std::size_t i = 0; i < clients.size(); ++i) {
                if (clients[i].sock) {
                    for (int round = 0; clients[i].sock->pending_bytes() > 0 && round < 128;
                         ++round) {
                        const auto before = clients[i].sock->pending_bytes();
                        expect(clients[i].sock->flush(POLLIN | POLLOUT),
                               "scheduled-player recorder flush succeeds without dropping output");
                        const auto after = clients[i].sock->pending_bytes();
                        expect(after < before, "scheduled-player recorder makes flush progress");
                        if (after >= before) {
                            break;
                        }
                    }
                    expect(clients[i].sock->pending_bytes() == 0,
                           "scheduled-player recorder captures every queued frame");
                    packets[i] = std::move(outputs[i]->packets);
                    outputs[i]->packets.clear();
                }
            }
            return packets;
        }
    };

    std::string scheduled_state(const ClientSession& c, std::uint64_t now_ms) {
        std::ostringstream out;
        out << combat_session_state(c) << '\n'
            << combat_inventory_state(c) << '\n'
            << static_cast<int>(c.phase) << ',' << c.rage << ',' << c.tflag << ','
            << c.pending_transform_stage << ',' << c.resting << ',' << c.rest_point << ','
            << c.next_talent_regen_ms << ',' << c.visual_shield << ',' << c.rest_food.mstate();
        for (auto wear : c.skin_wear) {
            out << ',' << wear;
        }
        // Probe a value copy to expose food's private next-pulse/expiry behavior without changing it.
        auto food = c.rest_food;
        for (const auto at : {now_ms, now_ms + 1, now_ms + 999, now_ms + 1000, now_ms + 100000}) {
            const auto pulse = food.tick(at);
            out << ';' << at << ':' << pulse.hp_add << ',' << pulse.mp_add << ','
                << pulse.hp_expired << ',' << pulse.mp_expired << ',' << food.mstate();
        }
        return out.str();
    }

    void observe_scheduled(std::string_view name,
                           ScheduledPlayers& players,
                           const ScheduledPackets& packets,
                           std::uint64_t now_ms,
                           const std::vector<std::string>& persistence = {},
                           int next_random = -1) {
        if (!scheduled_snapshot) {
            return;
        }
        // The comparison is made on the same C++ runtime; assertions derive RNG values portably.
        std::cout << "scheduled " << name << " clock=" << scheduled_unix_reads
                  << " next_random=" << next_random << '\n';
        for (std::size_t i = 0; i < players.clients.size(); ++i) {
            std::cout << "peer " << i << '\n'
                      << scheduled_state(players.clients[i], now_ms) << '\n';
            for (const auto& packet : packets[i]) {
                for (auto byte : packet) {
                    std::cout << std::hex << std::setw(2) << std::setfill('0')
                              << static_cast<int>(byte);
                }
                std::cout << std::dec << std::setfill(' ') << '\n';
            }
        }
        for (const auto& row : persistence) {
            std::cout << "store " << row << '\n';
        }
    }

    struct ScheduledStore {
        std::string root = test::temp_dir(("scheduled_player_" + std::to_string(getpid())).c_str());
        std::unique_ptr<IDataStore> store = create_data_store(StoreBackend::Sqlite);
        std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db{nullptr, sqlite3_close};
        std::int32_t character_id = 0;

        ScheduledStore() {
            expect(store->open(root), "open owned scheduled-player persistence");
            CharacterRecord character;
            character.name = "Owned scheduled player";
            expect(store->ensure_default_player("scheduled", "owned", character),
                   "seed owned scheduled-player character");
            const auto account = store->find_account("scheduled");
            const auto characters =
                account ? store->list_characters(account->id) : std::vector<CharacterRecord>{};
            expect(!characters.empty(), "find owned scheduled-player character");
            if (!characters.empty()) {
                character_id = characters.front().id;
            }
            sqlite3* raw = nullptr;
            expect(sqlite3_open((root + "/" + kSqliteFileName).c_str(), &raw) == SQLITE_OK,
                   "open scheduled-player SQLite audit connection");
            db.reset(raw);
            sql("CREATE TABLE scheduled_audit(seq INTEGER PRIMARY KEY,kind TEXT,iid INTEGER,"
                "level INTEGER,exp INTEGER);"
                "CREATE TRIGGER scheduled_delete AFTER DELETE ON items BEGIN INSERT INTO "
                "scheduled_audit(kind,iid,level,exp) VALUES('D',OLD.iid,OLD.transform_level,"
                "OLD.transform_exp); END;"
                "CREATE TRIGGER scheduled_update AFTER UPDATE ON items BEGIN INSERT INTO "
                "scheduled_audit(kind,iid,level,exp) VALUES('U',NEW.iid,NEW.transform_level,"
                "NEW.transform_exp); END;");
        }

        ~ScheduledStore() {
            db.reset();
            store->close();
            std::filesystem::remove_all(root);
        }

        void sql(const char* statement) {
            expect(sqlite3_exec(db.get(), statement, nullptr, nullptr, nullptr) == SQLITE_OK,
                   "execute scheduled-player persistence policy");
        }

        void seed(ClientSession& c) {
            c.character_id = character_id;
            for (const auto& slot : c.bag) {
                if (slot.iid != 0 && slot.count > 0) {
                    ItemRecord item;
                    fill_item_record_from_slot(item, c, slot, 3);
                    expect(store->insert_item(item), "persist owned scheduled-player item");
                }
            }
        }

        std::vector<std::string> audit() const {
            std::vector<std::string> rows;
            sqlite3_stmt* statement = nullptr;
            expect(sqlite3_prepare_v2(db.get(),
                                      "SELECT kind,iid,level,exp FROM scheduled_audit ORDER BY seq",
                                      -1,
                                      &statement,
                                      nullptr) == SQLITE_OK,
                   "read ordered scheduled-player persistence operations");
            if (statement) {
                while (sqlite3_step(statement) == SQLITE_ROW) {
                    rows.push_back(std::string(reinterpret_cast<const char*>(
                                       sqlite3_column_text(statement, 0))) +
                                   ',' + std::to_string(sqlite3_column_int(statement, 1)) + ',' +
                                   std::to_string(sqlite3_column_int(statement, 2)) + ',' +
                                   std::to_string(sqlite3_column_int(statement, 3)));
                }
                sqlite3_finalize(statement);
            }
            return rows;
        }
    };

    std::vector<std::string> scheduled_store_snapshot(const ScheduledStore& persistence) {
        auto rows = persistence.audit();
        auto items = persistence.store->load_items(persistence.character_id);
        std::sort(
            items.begin(), items.end(), [](const auto& a, const auto& b) { return a.iid < b.iid; });
        for (const auto& item : items) {
            rows.push_back(
                "item," + std::to_string(item.iid) + ',' + std::to_string(item.index) + ',' +
                std::to_string(item.count) + ',' + std::to_string(item.transform_level) + ',' +
                std::to_string(item.transform_exp) + ',' + std::to_string(item.pet_expire_at) +
                ',' + std::to_string(item.ride_expire_at));
        }
        return rows;
    }

    void
    scheduled_pet(ScheduledPlayers& players, int slot, int index, int heal, std::int64_t expiry) {
        GameWorldCoveragePeer::add_pet(players.world, PetDef{index, 0, heal, false});
        auto& item = players.owner().bag[static_cast<std::size_t>(slot)];
        item.iid = 2000 + static_cast<std::uint32_t>(slot);
        item.index = index;
        item.count = 1;
        item.equipped = true;
        item.pet_expire_at = expiry;
    }

    void test_scheduled_expiry() {
        for (const auto phase : {SessionPhase::WaitConnect, SessionPhase::WaitAnsCode}) {
            ScheduledPlayers players;
            auto& c = players.owner();
            c.phase = phase;
            c.cur_hp = 0;
            c.rage = 7;
            c.resting = true;
            scheduled_pet(players, 1, 301, 7, 1000);
            const auto before = scheduled_state(c, 10000);
            std::srand(41);
            const int expected_random = std::rand();
            std::srand(41);
            players.tick(10000, 1000);
            const int next_random = std::rand();
            const auto packets = players.drain();
            expect(scheduled_state(c, 10000) == before && packets == ScheduledPackets{} &&
                       next_random == expected_random,
                   "non-Playing scheduled players retain state, equipment, packets and RNG");
            observe_scheduled("phase_guard", players, packets, 10000, {}, next_random);
        }
        for (const auto unix_seconds : {999, 1000, 1001}) {
            ScheduledPlayers players;
            auto& c = players.owner();
            c.cur_hp = 0;
            c.mounted = true;
            c.next_pet_heal_ms = 123;
            c.next_sit_regen_ms = 456;
            c.wear[6] = 99;
            c.skin_wear[0] = 88;
            scheduled_pet(players, 1, 301, 7, 1000);
            scheduled_pet(players, 2, 302, 11, 1000);
            for (int slot : {0, 3}) {
                auto& ride = c.bag[static_cast<std::size_t>(slot)];
                ride.iid = 2000 + static_cast<std::uint32_t>(slot);
                ride.index = 310 + slot;
                ride.count = 1;
                ride.equipped = true;
                ride.prefix = slot == 0 ? -3 : 300;
                ride.ride_expire_at = 1000;
                GameWorldCoveragePeer::add_riding(players.world, ride.index, RidingDef{});
            }
            ScheduledStore persistence;
            persistence.seed(c);
            std::srand(43);
            const int expected_random = std::rand();
            std::srand(43);
            players.tick(10000, unix_seconds, persistence.store.get());
            const int next_random = std::rand();
            const auto packets = players.drain();
            expect(!c.mounted && c.cur_hp == 0 && c.next_sit_regen_ms == 456 &&
                       c.next_talent_regen_ms == 1000000 && next_random == expected_random,
                   "dead players dismount without regeneration, deadline changes or RNG");
            if (unix_seconds < 1000) {
                expect(c.bag[0].count == 1 && c.bag[1].count == 1 && c.next_pet_heal_ms == 123 &&
                           persistence.audit().empty(),
                       "pet and riding equipment survive one second before their expiry");
                expect(packets[0].size() == 2 && packets[1].size() == 1 && packets[2].empty(),
                       "dead-player dismount broadcasts only the registered ride in sight");
            } else {
                expect(c.bag[0].iid == 0 && c.bag[1].iid == 0 && c.bag[2].iid == 0 &&
                           c.bag[3].iid == 0 && c.next_pet_heal_ms == 0 && c.wear[6] == 0 &&
                           c.skin_wear[0] == 0 &&
                           persistence.audit() ==
                               std::vector<std::string>{
                                   "D,2001,1,0", "D,2002,1,0", "D,2000,1,0", "D,2003,1,0"},
                       "expiry at or after Unix deadline clears pets before rides in bag order");
                protocol::RidingState registered;
                registered.player_id = c.player_id;
                registered.iid = 2000;
                registered.item_index = 310;
                registered.expire_at = 1000;
                protocol::RidingState empty;
                empty.player_id = c.player_id;
                std::vector<std::vector<std::uint8_t>> expected;
                const auto removed = [&](int slot, int index, int prefix) {
                    const auto iid = 2000 + static_cast<std::uint32_t>(slot);
                    expected.push_back(protocol::ItemWear::frame_server_put_off(
                        c.player_id, iid, static_cast<std::uint16_t>(index)));
                    expected.push_back(frame_insert_item(iid,
                                                         static_cast<std::uint16_t>(index),
                                                         static_cast<std::uint8_t>(prefix),
                                                         protocol::pack_insert_info(true, 0, 0),
                                                         0));
                };
                removed(1, 301, 0);
                removed(2, 302, 0);
                expected.push_back(protocol::Property::frame_update_mproperty(
                    c.player_id, protocol::kPropertyMSpeed, 0));
                expected.push_back(protocol::Riding::frame_server_state(registered));
                removed(0, 310, 0);
                removed(3, 313, 255);
                expected.push_back(protocol::Riding::frame_server_state(empty));
                expect(packets[0] == expected &&
                           packets[1] ==
                               std::vector<std::vector<std::uint8_t>>{
                                   protocol::Riding::frame_server_state(registered),
                                   protocol::Riding::frame_server_state(empty)} &&
                           packets[2].empty() &&
                           persistence.store->load_items(c.character_id).empty(),
                       "expiry preserves exact owner/nearby packet order and persisted deletions");
            }
            observe_scheduled("expiry_" + std::to_string(unix_seconds),
                              players,
                              packets,
                              10000,
                              scheduled_store_snapshot(persistence),
                              next_random);
        }
        for (int policy = 0; policy < 3; ++policy) {
            ScheduledPlayers players;
            auto& c = players.owner();
            c.cur_hp = 0;
            scheduled_pet(players, 0, 301, 7, 1000);
            ScheduledStore persistence;
            persistence.seed(c);
            IDataStore* store = persistence.store.get();
            if (policy == 0) {
                persistence.sql("CREATE TRIGGER reject_delete BEFORE DELETE ON items BEGIN "
                                "SELECT RAISE(ABORT,'owned scheduled failure'); END;");
            } else if (policy == 1) {
                store = nullptr;
            } else {
                c.character_id = 0;
                c.sock.reset();
            }
            players.tick(10000, 1000, store);
            const auto packets = players.drain();
            expect(c.bag[0].iid == 0 && c.cur_hp == 0 && persistence.audit().empty() &&
                       persistence.store->load_items(persistence.character_id).size() == 1 &&
                       packets[0].size() == (policy == 2 ? 0u : 2u),
                   "failed or gated persistence never prevents expired RAM equipment removal");
            observe_scheduled("expiry_policy_" + std::to_string(policy),
                              players,
                              packets,
                              10000,
                              scheduled_store_snapshot(persistence));
        }
        ScheduledPlayers excluded;
        auto& c = excluded.owner();
        c.cur_hp = 0;
        for (int slot = 0; slot < 5; ++slot) {
            scheduled_pet(excluded, slot, 301, 7, slot == 4 ? 1001 : 1000);
        }
        c.bag[0].equipped = false;
        c.bag[1].count = 0;
        c.bag[2].index = 999;
        c.bag[3].pet_expire_at = 0;
        GameWorldCoveragePeer::add_riding(excluded.world, 310, RidingDef{});
        for (int slot = 5; slot < 10; ++slot) {
            auto& ride = c.bag[static_cast<std::size_t>(slot)];
            ride.iid = 2000 + static_cast<std::uint32_t>(slot);
            ride.index = slot == 7 ? 999 : 310;
            ride.count = slot == 6 ? 0 : 1;
            ride.equipped = slot != 5;
            ride.ride_expire_at = slot == 8 ? 0 : slot == 9 ? 1001 : 1000;
        }
        const auto before = combat_inventory_state(c);
        excluded.tick(10000, 1000);
        const auto packets = excluded.drain();
        expect(combat_inventory_state(c) == before && packets == ScheduledPackets{},
               "unequipped, empty, unknown, permanent and future pet/ride slots do not expire");
        observe_scheduled("expiry_exclusions", excluded, packets, 10000);
    }

    std::vector<std::uint8_t> scheduled_property(protocol::PropertyType type, int value) {
        return protocol::Property::frame_update(static_cast<std::uint8_t>(type), value);
    }

    void test_scheduled_resources() {
        constexpr std::uint64_t now = 10000;
        ScheduledPlayers players;
        auto& c = players.owner();
        c.cur_hp = 100;
        c.max_hp = 1000;
        c.cur_mp = 50;
        c.max_mp = 500;
        c.wis = 45;
        c.talent_mask = protocol::talent_bit(23) | protocol::talent_bit(25);
        c.resting = true;
        c.rest_point = 2;
        c.next_talent_regen_ms = c.next_stand_hp_ms = c.next_stand_mp_ms = now;
        c.next_pet_heal_ms = c.next_meditation_ms = c.next_knight_recovery_ms = now;
        c.next_sit_regen_ms = now;
        scheduled_pet(players, 0, 301, 7, 0);
        scheduled_pet(players, 1, 302, 11, 0);
        apply_buff(c, SkillBuffId::Meditation, 2);
        apply_buff(c, SkillBuffId::KnightHealingChant, 19);
        expect(c.rest_food.apply_hp(13, 1, now - 1000) && c.rest_food.apply_mp(17, 1, now - 1000),
               "arm simultaneous owned food pulses and expiration");
        std::srand(47);
        const int meditation = combat::irand(0, 3);
        const int hp_roll = combat::irand(0, 4);
        const int mp_roll = combat::irand(0, 4);
        const int expected_random = std::rand();
        std::srand(47);
        players.tick(now, 1000);
        const int next_random = std::rand();
        const auto packets = players.drain();
        const auto hp = protocol::PropertyType::CurHp, mp = protocol::PropertyType::CurMp;
        const auto mstate = protocol::MState::frame(c.player_id, 0);
        expect(packets[0] ==
                       std::vector<std::vector<std::uint8_t>>{
                           scheduled_property(hp, 110),
                           scheduled_property(mp, 55),
                           scheduled_property(hp, 121),
                           scheduled_property(hp, 131),
                           scheduled_property(mp, 60),
                           scheduled_property(mp, 66 + meditation),
                           scheduled_property(hp, 150),
                           scheduled_property(hp, 189 + hp_roll),
                           scheduled_property(mp, 100 + meditation + mp_roll),
                           scheduled_property(hp, 202 + hp_roll),
                           scheduled_property(mp, 117 + meditation + mp_roll),
                           mstate} &&
                   packets[1] == std::vector<std::vector<std::uint8_t>>{mstate} &&
                   packets[2].empty(),
               "simultaneous sources preserve talent/pet/stand/meditation/recovery/sit/food order");
        expect(c.cur_hp == 202 + hp_roll && c.cur_mp == 117 + meditation + mp_roll &&
                   c.next_talent_regen_ms == 12000 && c.next_pet_heal_ms == 12000 &&
                   c.next_stand_hp_ms == 16000 && c.next_stand_mp_ms == 22000 &&
                   c.next_meditation_ms == 14000 && c.next_knight_recovery_ms == 13000 &&
                   c.next_sit_regen_ms == 14000 && !c.rest_food.active() &&
                   next_random == expected_random,
               "each due source advances once and meditation precedes the two sitting RNG draws");
        observe_scheduled("all_resources_due", players, packets, now, {}, next_random);
        const auto before = scheduled_state(c, now);
        std::srand(53);
        const int repeat_random = std::rand();
        std::srand(53);
        players.tick(now, 1000);
        const int repeat_next = std::rand();
        const auto repeat_packets = players.drain();
        expect(scheduled_state(c, now) == before && repeat_packets == ScheduledPackets{} &&
                   repeat_next == repeat_random,
               "repeating the same time cannot repeat a pulse or consume RNG");
        observe_scheduled("repeat_time", players, repeat_packets, now, {}, repeat_next);
        const int before_hp = c.cur_hp, before_mp = c.cur_mp;
        std::srand(59);
        const int late_meditation = combat::irand(0, 3);
        const int late_hp = combat::irand(0, 4), late_mp = combat::irand(0, 4);
        const int late_random = std::rand();
        std::srand(59);
        players.tick(100000, 1000);
        const int late_next = std::rand();
        const auto late_packets = players.drain();
        expect(c.cur_hp == before_hp + 89 + late_hp &&
                   c.cur_mp == before_mp + 50 + late_meditation + late_mp &&
                   c.next_talent_regen_ms == 102000 && c.next_pet_heal_ms == 102000 &&
                   c.next_stand_hp_ms == 106000 && c.next_stand_mp_ms == 112000 &&
                   c.next_meditation_ms == 104000 && c.next_knight_recovery_ms == 103000 &&
                   c.next_sit_regen_ms == 104000 && late_packets[0].size() == 9 &&
                   late_packets[1].empty() && late_packets[2].empty() && late_next == late_random,
               "a late tick applies one pulse per source and anchors all next deadlines to now");
        observe_scheduled("late_resources", players, late_packets, 100000, {}, late_next);

        for (const auto& [rest_point, missing] :
             {std::pair<int, bool>{0, false}, {2, false}, {2, true}}) {
            ScheduledPlayers full;
            auto& owner = full.owner();
            owner.cur_hp = owner.max_hp = 1000;
            owner.cur_mp = owner.max_mp = 500;
            if (missing) {
                --owner.cur_hp;
                --owner.cur_mp;
                owner.talent_mask = protocol::talent_bit(23) | protocol::talent_bit(25);
            }
            owner.resting = true;
            owner.rest_point = rest_point;
            owner.next_talent_regen_ms = owner.next_pet_heal_ms = now;
            owner.next_stand_hp_ms = owner.next_stand_mp_ms = now;
            owner.next_meditation_ms = owner.next_knight_recovery_ms = now;
            owner.next_sit_regen_ms = now;
            scheduled_pet(full, 0, 301, 7, 0);
            apply_buff(owner, SkillBuffId::Meditation, 2);
            apply_buff(owner, SkillBuffId::KnightHealingChant, 19);
            std::srand(61);
            if (rest_point > 0) {
                (void) std::rand();
                (void) std::rand();
            }
            const int next = std::rand();
            std::srand(61);
            full.tick(now, 1000);
            const int actual = std::rand();
            const auto full_packets = full.drain();
            ScheduledPackets expected_packets;
            if (missing) {
                expected_packets[0] = {scheduled_property(hp, 1000), scheduled_property(mp, 500)};
            }
            expect(full_packets == expected_packets && owner.next_stand_hp_ms == now &&
                       owner.next_stand_mp_ms == now && owner.next_talent_regen_ms == 12000 &&
                       owner.next_pet_heal_ms == 12000 && owner.next_meditation_ms == 14000 &&
                       owner.next_knight_recovery_ms == 13000 && owner.next_sit_regen_ms == 14000 &&
                       actual == next,
                   "full or talent-filled resources suppress later pulses/meditation RNG but "
                   "retain sitting RNG rules");
            observe_scheduled("full_" + std::to_string(rest_point) +
                                  (missing ? "_talent_fill" : ""),
                              full,
                              full_packets,
                              now,
                              {},
                              actual);
        }
        ScheduledPlayers initialized;
        auto& init = initialized.owner();
        scheduled_pet(initialized, 0, 301, 7, 0);
        apply_buff(init, SkillBuffId::Meditation, 2);
        apply_buff(init, SkillBuffId::KnightHealingChant, 19);
        init.next_talent_regen_ms = 0;
        initialized.tick(now, 1000);
        const auto init_packets = initialized.drain();
        expect(init_packets == ScheduledPackets{} && init.next_talent_regen_ms == 12000 &&
                   init.next_pet_heal_ms == 12000 && init.next_meditation_ms == 14000 &&
                   init.next_knight_recovery_ms == 13000,
               "newly enabled periodic sources arm deadlines without an immediate pulse");
        observe_scheduled("initialize_sources", initialized, init_packets, now);
        init.bag[0] = {};
        (void) init.skill_buffs.remove(SkillBuffId::Meditation);
        (void) init.skill_buffs.remove(SkillBuffId::KnightHealingChant);
        initialized.tick(now + 1, 1000);
        const auto disabled = initialized.drain();
        expect(init.next_pet_heal_ms == 0 && init.next_meditation_ms == 0 &&
                   init.next_knight_recovery_ms == 0 && disabled == ScheduledPackets{},
               "removing pet and skill sources clears their existing deadlines");
        observe_scheduled("disable_sources", initialized, disabled, now + 1);

        ScheduledPlayers capped;
        auto& cap = capped.owner();
        cap.cur_hp = cap.cur_mp = 100;
        cap.max_hp = cap.max_mp = 10000;
        cap.next_stand_hp_ms = cap.next_stand_mp_ms = now;
        capped.tick(now, 1000);
        const auto cap_packets = capped.drain();
        expect(cap_packets[0] ==
                   std::vector<std::vector<std::uint8_t>>{scheduled_property(hp, 140),
                                                          scheduled_property(mp, 120)},
               "standing restoration retains its 40 HP and 20 MP amount caps");
        observe_scheduled("standing_caps", capped, cap_packets, now);

        ScheduledPlayers food;
        auto& fed = food.owner();
        fed.cur_hp = 100;
        fed.max_hp = 1000;
        fed.cur_mp = 50;
        fed.max_mp = 500;
        expect(fed.rest_food.apply_hp(13, 4, 9000) && fed.rest_food.apply_mp(17, 4, 9000),
               "arm multi-tick food channels");
        int step = 0;
        for (const auto at : {9999u, 10000u, 10000u, 13000u}) {
            food.tick(at, 1000);
            const auto food_packets = food.drain();
            if (step == 0 || step == 2) {
                expect(food_packets == ScheduledPackets{},
                       "food does not pulse early or repeat at the same timestamp");
            } else {
                std::vector<std::vector<std::uint8_t>> expected{
                    scheduled_property(hp, at == 13000 ? 126 : 113),
                    scheduled_property(mp, at == 13000 ? 84 : 67)};
                if (at == 13000) {
                    expected.push_back(protocol::MState::frame(fed.player_id, 0));
                }
                expect(
                    food_packets[0] == expected &&
                        food_packets[1] ==
                            (at == 13000
                                 ? std::vector<std::vector<std::uint8_t>>{protocol::MState::frame(
                                       fed.player_id, 0)}
                                 : std::vector<std::vector<std::uint8_t>>{}) &&
                        food_packets[2].empty(),
                    "food pulses once when late and sends HP/MP before its expiry broadcast");
            }
            observe_scheduled("food_" + std::to_string(step), food, food_packets, at);
            ++step;
        }
    }

    void test_scheduled_transforms() {
        for (int mode = 0; mode < 7; ++mode) {
            ScheduledPlayers players;
            auto& c = players.owner();
            ItemCombatDef egg;
            egg.index = 401;
            egg.is_transform = true;
            egg.transform_race = 2;
            GameWorldCoveragePeer::add_item(players.world, egg);
            auto& slot = c.bag[0];
            slot.iid = 4001;
            slot.index = 401;
            slot.count = 1;
            slot.equipped = true;
            c.transform.index = 401;
            c.transform.level = mode == 3 ? 100 : 1;
            c.transform.exp = mode == 1 ? 0 : protocol::need_transform_exp_for_level(1) - 10;
            c.transform.put_on_ticks = 179;
            c.level = mode == 2 ? 1 : 30;
            c.next_transform_tick_ms = mode == 4 ? 10001 : 10000;
            slot.transform_level = c.transform.level;
            slot.transform_exp = c.transform.exp;
            if (mode == 0 || mode == 5 || mode == 6) {
                c.transform_stage = 1;
                c.pending_transform_stage = 2;
                c.tflag = protocol::transform_equip_tflag(2) | 1;
                c.gstate |= protocol::kCgsOnTransform;
                c.rage = protocol::dec_rage_for_stage(1);
                c.next_dec_rage_ms = 10000;
                apply_buff(c, SkillBuffId::ThiefHiding, 5);
            }
            ScheduledStore persistence;
            persistence.seed(c);
            if (mode == 5) {
                persistence.sql("CREATE TRIGGER reject_update BEFORE UPDATE ON items BEGIN "
                                "SELECT RAISE(ABORT,'owned scheduled failure'); END;");
            }
            if (mode == 6) {
                c.character_id = 0;
                c.sock.reset();
            }
            players.tick(10000, 1000, mode == 1 ? nullptr : persistence.store.get());
            const auto packets = players.drain();
            const bool leveled = mode == 0 || mode == 5 || mode == 6;
            expect(c.next_transform_tick_ms == (mode == 4 ? 10001u : 11000u) &&
                       c.transform.put_on_ticks == (mode == 4 ? 179 : 0),
                   "transform due/max/player-level gates consume exactly the scheduled wear tick");
            if (leveled) {
                expect(c.transform.level == 2 && c.transform.exp == 0 &&
                           slot.transform_level == 2 && slot.transform_exp == 0 && c.rage == 0 &&
                           c.transform_stage == 0 && c.pending_transform_stage == 0 &&
                           c.next_dec_rage_ms == 0 && (c.gstate & protocol::kCgsOnTransform) == 0 &&
                           c.tflag == protocol::transform_equip_tflag(2) &&
                           !c.skill_buffs.find(SkillBuffId::ThiefHiding),
                       "level-up completes before same-tick rage exhaustion releases form/hiding");
                const auto form = protocol::Transform::frame_action_form(
                    c.player_id, static_cast<std::uint8_t>(protocol::transform_equip_tflag(2)));
                expect(packets[1] == std::vector<std::vector<std::uint8_t>>{form} &&
                           packets[2] == std::vector<std::vector<std::uint8_t>>{form},
                       "form release preserves its global playing-peer broadcast across maps");
                if (c.sock) {
                    protocol::UpdateItemInfoFields update;
                    update.iid = 4001;
                    update.index = 401;
                    update.level = 2;
                    update.added = 10;
                    const auto rage = protocol::Property::frame_update(protocol::kPropertyRage, 0);
                    const auto item = protocol::ItemInfo::frame_update(update);
                    const auto rage_at = std::find(packets[0].begin(), packets[0].end(), rage);
                    const auto form_at = std::find(packets[0].begin(), packets[0].end(), form);
                    expect(
                        !packets[0].empty() && packets[0].front() == item &&
                            rage_at != packets[0].end() && form_at != packets[0].end() &&
                            rage_at > packets[0].begin() + 1 && rage_at < form_at,
                        "animal item update and progression precede rage and form-release packets");
                }
                expect(persistence.audit() == (mode == 0 ? std::vector<std::string>{"U,4001,2,0"}
                                                         : std::vector<std::string>{}),
                       "level-up persists once while ignored failure/character gates preserve RAM");
            } else {
                expect(
                    persistence.audit().empty() && packets[1].empty() && packets[2].empty() &&
                        packets[0].size() == (mode == 1 ? 1u : 0u),
                    "non-leveling, blocked and not-due transforms avoid persistence/progression");
                expect(c.transform.level == (mode == 3 ? 100 : 1) &&
                           c.transform.exp ==
                               (mode == 1 ? 10 : protocol::need_transform_exp_for_level(1) - 10),
                       "transform level cap and player-level rejection retain their EXP behavior");
            }
            const auto persisted = persistence.store->load_items(persistence.character_id);
            expect(persisted.size() == 1 && persisted.front().transform_level == (mode == 0   ? 2
                                                                                  : mode == 3 ? 100
                                                                                              : 1),
                   "stored animal state changes only after a successful level-up write");
            observe_scheduled("transform_" + std::to_string(mode),
                              players,
                              packets,
                              10000,
                              scheduled_store_snapshot(persistence));
        }
        for (int mode = 0; mode < 3; ++mode) {
            ScheduledPlayers players;
            auto& c = players.owner();
            c.transform_stage = mode == 2 ? 3 : 99;
            c.gstate = mode == 2 ? 0 : protocol::kCgsOnTransform;
            c.rage = protocol::max_rage();
            c.next_dec_rage_ms = mode == 0 ? 0 : 10000;
            players.tick(10000, 1000);
            const auto packets = players.drain();
            expect(c.next_dec_rage_ms == (mode == 2 ? 10000u : 15000u) &&
                       c.rage == (mode == 1 ? protocol::max_rage() - protocol::dec_rage_for_stage(3)
                                            : protocol::max_rage()) &&
                       packets[0].size() == (mode == 1 ? 1u : 0u) && packets[1].empty() &&
                       packets[2].empty(),
                   "rage initialization, clamped stage and absent OnTransform bit retain timing");
            observe_scheduled("rage_" + std::to_string(mode), players, packets, 10000);
        }
    }

    void test_scheduled_player_phases() {
        test_scheduled_expiry();
        test_scheduled_resources();
        test_scheduled_transforms();
    }

    bool h1_snapshot = false;
    std::vector<std::int64_t> h1_unix_values;
    std::size_t h1_unix_reads = 0;

    std::int64_t h1_unix_seconds() {
        expect(h1_unix_reads < h1_unix_values.size(),
               "H1 reads only the supplied lazy Unix values");
        return h1_unix_reads < h1_unix_values.size() ? h1_unix_values[h1_unix_reads++] : 0;
    }

    struct H1Players : ScheduledPlayers {
        PartyService parties;
        std::mutex world_mutex;
        std::atomic<std::uint32_t> next_iid{4000};

        H1Players() {
            for (auto& c : clients) {
                c.name = "Owned H1 player";
                c.cur_hp = c.max_hp = 100;
                c.cur_mp = c.max_mp = 50;
            }
            ItemCombatDef item;
            item.index = 901;
            GameWorldCoveragePeer::add_item(world, item);
        }

        void collection(std::uint64_t now_ms,
                        std::initializer_list<std::int64_t> reads,
                        IDataStore* store = nullptr) {
            h1_unix_values = reads;
            h1_unix_reads = 0;
#if defined(OPENKAL_UPKEEP_BASELINE_BRIDGE)
            h1_baseline::collection(owner(), world, store, now_ms, h1_unix_seconds);
#else
            tick_collection_expiry_and_profile(owner(), world, store, now_ms, h1_unix_seconds);
#endif
            expect(h1_unix_reads == h1_unix_values.size(), "H1 consumes the exact lazy Unix trace");
        }

        void pickup(std::uint64_t now_ms, IDataStore* store = nullptr, std::size_t actor = 0) {
#if defined(OPENKAL_UPKEEP_BASELINE_BRIDGE)
            h1_baseline::autopick(
                clients[actor], clients, world, store, next_iid, parties, world_mutex, now_ms);
#else
            tick_talent_autopick(
                clients[actor], clients, world, store, next_iid, parties, world_mutex, now_ms);
#endif
        }

        void autosave(std::uint64_t now_ms, IDataStore* store) {
#if defined(OPENKAL_UPKEEP_BASELINE_BRIDGE)
            h1_baseline::autosave(owner(), store, now_ms);
#else
            autosave_player_if_due(owner(), store, now_ms);
#endif
        }

        void ground(std::uint32_t id, int index = kMoneyItemIndex, int count = 7) {
            GroundItem item;
            item.id = id;
            item.item_index = index;
            item.count = count;
            item.x = owner().px;
            item.y = owner().py;
            world.ground_items().push_back(item);
            for (auto& c : clients) {
                c.known_items.insert(id);
            }
        }
    };

    void observe_h1(std::string_view name,
                    H1Players& players,
                    const ScheduledPackets& packets,
                    const ScheduledStore* persistence = nullptr) {
        if (!h1_snapshot) {
            return;
        }
        std::cout << "h1 " << name << " next_iid=" << players.next_iid.load()
                  << " unix_reads=" << h1_unix_reads << '\n';
        for (std::size_t i = 0; i < players.clients.size(); ++i) {
            const auto& c = players.clients[i];
            std::cout << "peer " << i << ' ' << c.money << ',' << c.next_talent_pickup_ms << ','
                      << c.party_id << ',' << static_cast<int>(c.party_loot) << '\n'
                      << scheduled_state(c, 10000) << '\n';
            for (auto id : std::set<std::uint32_t>(c.known_items.begin(), c.known_items.end())) {
                std::cout << "known " << id << '\n';
            }
            for (const auto& packet : packets[i]) {
                for (auto byte : packet) {
                    std::cout << std::hex << std::setw(2) << std::setfill('0')
                              << static_cast<int>(byte);
                }
                std::cout << std::dec << std::setfill(' ') << '\n';
            }
        }
        for (const auto& item : players.world.ground_items()) {
            std::cout << "ground " << item.id << ',' << item.item_index << ',' << item.prefix << ','
                      << item.random_stat_type << ',' << item.random_stat_value << ',' << item.count
                      << ',' << item.x << ',' << item.y << ',' << item.map << ','
                      << item.owner_player_id << ',' << item.owner_party_id << ','
                      << item.party_recipient_id << ',' << item.server_owned << ','
                      << item.ownership_left << ',' << item.despawn_left << '\n';
        }
        if (persistence) {
            for (const auto& row : scheduled_store_snapshot(*persistence)) {
                std::cout << "store " << row << '\n';
            }
        }
    }

    ClientSession::BagSlot h1_mirror(std::uint32_t iid, int count, std::int64_t expiry) {
        ClientSession::BagSlot item;
        item.iid = iid;
        item.index = kYinYangMirrorFirst;
        item.count = count;
        item.pet_expire_at = expiry;
        return item;
    }

    void test_h1_collection() {
        {
            H1Players players;
            auto& c = players.owner();
            c.bag[0] = h1_mirror(10, 0, 1000);
            c.bag[1] = h1_mirror(11, 1, 0);
            c.bag[2] = h1_mirror(12, 1, 1000);
            c.bag[2].index = kTrigramFirst;
            c.bag[3] = h1_mirror(13, 1, 1000);
            c.bag[3].index = 901;
            const auto before = scheduled_state(c, 10000);
            players.collection(10000, {});
            const auto packets = players.drain();
            expect(scheduled_state(c, 10000) == before && packets == ScheduledPackets{},
                   "ineligible collection slots read no clock and retain every state field");
            observe_h1("collection_ineligible", players, packets);
        }
        for (const auto unix_seconds : {999, 1000, 1001}) {
            H1Players players;
            auto& c = players.owner();
            c.bag[0] = h1_mirror(20, 2, 1000);
            c.collection[0] = h1_mirror(21, 1, 1000);
            players.collection(10000, {unix_seconds, unix_seconds});
            const auto packets = players.drain();
            expect(c.bag[0].count == (unix_seconds < 1000 ? 2 : 1) &&
                       c.collection[0].count == (unix_seconds < 1000 ? 1 : 0) &&
                       packets[1].empty() && packets[2].empty(),
                   "Mirror expiry uses equality and consumes one unit in each eligible domain");
            expect((unix_seconds < 1000 && packets[0].empty()) ||
                       (unix_seconds >= 1000 && packets[0].size() > 2 &&
                        packets[0][0] == frame_insert_item(20,
                                                           kYinYangMirrorFirst,
                                                           0,
                                                           protocol::pack_insert_info(true, 0, 0),
                                                           1,
                                                           {},
                                                           1000) &&
                        packets[0][1] == frame_insert_item(21,
                                                           kYinYangMirrorFirst,
                                                           0,
                                                           protocol::pack_insert_info(true, 0, 0),
                                                           0)),
                   "bag and collection consumption packets precede recomputation in source order");
            observe_h1("collection_edge_" + std::to_string(unix_seconds), players, packets);
            players.collection(10000,
                               unix_seconds < 1000
                                   ? std::initializer_list<std::int64_t>{unix_seconds, unix_seconds}
                                   : std::initializer_list<std::int64_t>{unix_seconds});
            const auto repeated = players.drain();
            expect(c.bag[0].count == (unix_seconds < 1000 ? 2 : 0),
                   "an expired Mirror stack consumes one more unit only on the repeated tick");
            observe_h1("collection_repeat_" + std::to_string(unix_seconds), players, repeated);
        }
        for (int mode = 0; mode < 3; ++mode) {
            const bool fail_delete = mode == 1;
            ScheduledStore persistence;
            H1Players players;
            auto& c = players.owner();
            c.bag[0] = h1_mirror(30, 1, 1000);
            c.bag[1] = h1_mirror(31, mode == 2 ? 2 : 1, 1000);
            c.collection[0] = h1_mirror(32, 1, 1000);
            persistence.seed(c);
            ItemRecord collection_item;
            fill_item_record_from_slot(collection_item, c, c.collection[0], 20);
            expect(persistence.store->insert_item(collection_item), "seed expired collection item");
            if (fail_delete) {
                persistence.sql("CREATE TRIGGER h1_reject_delete BEFORE DELETE ON items BEGIN "
                                "SELECT RAISE(ABORT,'owned H1 delete failure'); END;");
            }
            c.cur_hp = 0;
            players.collection(10000, {999, 1000, 1001}, persistence.store.get());
            const auto packets = players.drain();
            expect(c.bag[0].iid == 30 && c.bag[1].count == (mode == 2 ? 1 : 0) &&
                       c.collection[0].iid == 0 && c.cur_hp == 0,
                   "distinct per-slot Unix reads preserve future slots and expire dead actors");
            expect(persistence.audit() ==
                           (fail_delete
                                ? std::vector<std::string>{}
                                : std::vector<std::string>{mode == 2 ? "U,31,1,0" : "D,31,1,0",
                                                           "D,32,1,0"}) &&
                       persistence.store->load_items(c.character_id).size() == (fail_delete ? 3u
                                                                                : mode == 2 ? 2u
                                                                                            : 1u),
                   "collection consumes locally even when persisted deletion fails");
            observe_h1(fail_delete ? "collection_delete_failure"
                       : mode == 2 ? "collection_stack_update"
                                   : "collection_delete_order",
                       players,
                       packets,
                       &persistence);
        }
        {
            H1Players players;
            auto& c = players.owner();
            c.bag[0] = h1_mirror(33, 1, 1000);
            c.sock.reset();
            players.collection(10000, {1000});
            const auto packets = players.drain();
            expect(c.bag[0].iid == 0 && c.collection_world == &players.world &&
                       packets == ScheduledPackets{},
                   "collection expiry and recomputation also complete without an output socket");
            observe_h1("collection_no_socket", players, packets);
        }
        for (const std::uint64_t now_ms : {99u, 100u, 101u}) {
            H1Players players;
            auto& c = players.owner();
            recompute_combat(c, players.world);
            c.collection_pve = false;
            c.collection_pvp_until_ms = 100;
            c.cur_hp = 0;
            players.collection(now_ms, {});
            const auto packets = players.drain();
            expect(c.collection_pve == (now_ms >= 100) && c.cur_hp == 0 &&
                       ((now_ms < 100) == packets[0].empty()),
                   "explicit profile timeout gate preserves the dead player's zero resources");
            observe_h1("profile_outer_edge_" + std::to_string(now_ms), players, packets);
        }
        {
            H1Players players;
            auto& c = players.owner();
            recompute_combat(c, players.world);
            c.collection_pve = false;
            c.collection_pvp_until_ms = steady_ms_now() + 60000;
            players.collection(c.collection_pvp_until_ms, {});
            const auto packets = players.drain();
            expect(!c.collection_pve && packets == ScheduledPackets{},
                   "profile selection retains its independent real steady-clock guard");
            // The real-clock deadline is intentionally not included in deterministic snapshots.
        }
    }

    void test_h1_autopick() {
        h1_unix_reads = 0;
        for (int gate = 0; gate < 3; ++gate) {
            H1Players players;
            auto& c = players.owner();
            c.talent_mask = gate == 0 ? 0 : protocol::talent_bit(29);
            c.cur_hp = gate == 1 ? 0 : 100;
            c.tele_pending = gate == 2;
            c.next_talent_pickup_ms = 17;
            players.ground(50);
            players.pickup(10000);
            const auto packets = players.drain();
            expect(c.next_talent_pickup_ms == 17 && players.world.ground_items().size() == 1 &&
                       c.money == 0 && packets == ScheduledPackets{},
                   "autopick talent, alive and teleport gates preserve timer and ground state");
            observe_h1("pickup_gate_" + std::to_string(gate), players, packets);
        }
        for (const auto now_ms : {9999u, 10000u, 10001u, 1000000u}) {
            H1Players players;
            auto& c = players.owner();
            c.talent_mask = protocol::talent_bit(29);
            c.next_talent_pickup_ms = 10000;
            players.ground(60);
            players.pickup(now_ms);
            const auto packets = players.drain();
            const bool due = now_ms >= 10000;
            expect(c.next_talent_pickup_ms == (due ? now_ms + 5000 : 10000) &&
                       c.money == (due ? 7 : 0) &&
                       players.world.ground_items().size() == (due ? 0u : 1u),
                   "autopick equality and late ticks produce one pulse anchored to current time");
            if (due) {
                expect(
                    packets[0].size() == 2 && packets[0][0] == frame_remove_item(60) &&
                        packets[1] ==
                            std::vector<std::vector<std::uint8_t>>{frame_remove_item(60)} &&
                        packets[2] == packets[1],
                    "known-item removals precede the owner award and retain observer predicates");
            }
            observe_h1("pickup_edge_" + std::to_string(now_ms), players, packets);
            players.pickup(now_ms);
            expect(players.drain() == ScheduledPackets{} && c.money == (due ? 7 : 0),
                   "a repeated same-time tick cannot pick or award twice");
        }
        {
            H1Players players;
            auto& c = players.owner();
            c.talent_mask = protocol::talent_bit(29);
            players.ground(61);
            players.pickup(10000);
            const auto packets = players.drain();
            expect(c.next_talent_pickup_ms == 15000 && c.money == 0 &&
                       packets == ScheduledPackets{},
                   "autopick initializes its five-second deadline without an immediate pulse");
            observe_h1("pickup_initial", players, packets);
        }
        for (int eligibility = 0; eligibility < 11; ++eligibility) {
            H1Players players;
            auto& c = players.owner();
            c.talent_mask = protocol::talent_bit(29);
            c.next_talent_pickup_ms = 10000;
            players.ground(70);
            auto& item = players.world.ground_items().back();
            item.ownership_left = 10;
            if (eligibility == 0) {
                item.map = 1;
            }
            if (eligibility == 1) {
                item.x += 65;
            }
            if (eligibility == 2) {
                item.x += 64;
            }
            if (eligibility == 3) {
                item.server_owned = true;
            }
            if (eligibility == 4) {
                item.server_owned = true;
                item.ownership_left = 0;
            }
            if (eligibility == 5) {
                item.owner_player_id = 999;
            }
            if (eligibility == 6) {
                item.owner_player_id = c.player_id;
            }
            if (eligibility == 7) {
                item.owner_party_id = c.party_id = 17;
            }
            if (eligibility == 8) {
                item.owner_party_id = 18;
                c.party_id = 17;
            }
            if (eligibility == 9) {
                item.owner_player_id = 999;
                item.ownership_left = 0;
            }
            std::srand(61);
            const int expected_random = std::rand();
            std::srand(61);
            players.pickup(10000);
            const int next_random = std::rand();
            const bool allowed = eligibility == 2 || eligibility == 4 || eligibility == 6 ||
                                 eligibility == 7 || eligibility == 9 || eligibility == 10;
            const auto packets = players.drain();
            expect(c.money == (allowed ? 7 : 0) &&
                       players.world.ground_items().size() == (allowed ? 0u : 1u) &&
                       next_random == expected_random,
                   "autopick preserves map/radius/ownership eligibility without an RNG draw");
            observe_h1("pickup_eligibility_" + std::to_string(eligibility), players, packets);
        }
        {
            H1Players players;
            auto& c = players.owner();
            c.talent_mask = protocol::talent_bit(29);
            c.next_talent_pickup_ms = 10000;
            for (std::size_t i = 0; i < c.bag.size(); ++i) {
                c.bag[i].iid = 5000 + static_cast<std::uint32_t>(i);
                c.bag[i].index = 901;
                c.bag[i].count = 1;
            }
            players.ground(80, 901);
            players.ground(81, kMoneyItemIndex, 7);
            players.ground(82, kMoneyItemIndex, 11);
            players.ground(83, kMoneyItemIndex, 19);
            players.pickup(10000);
            const auto packets = players.drain();
            expect(c.money == 18 && players.world.ground_items().size() == 2 &&
                       players.world.find_ground_item(80) && players.world.find_ground_item(83) &&
                       players.next_iid == 4000 && packets[0].size() == 6 &&
                       packets[0][2] == frame_remove_item(81) &&
                       packets[0][4] == frame_remove_item(82),
                   "failed insertion does not consume pickup quota or reorder later successes");
            observe_h1("pickup_success_quota", players, packets);
        }
        {
            H1Players players;
            auto& c = players.owner();
            c.talent_mask = protocol::talent_bit(29);
            c.next_talent_pickup_ms = 10000;
            c.party_id = players.clients[1].party_id = 17;
            players.ground(90, kMoneyItemIndex, 17);
            players.world.ground_items().back().owner_party_id = 17;
            players.pickup(10000);
            const auto packets = players.drain();
            expect(c.money == 9 && players.clients[1].money == 8 && players.clients[2].money == 0 &&
                       players.world.ground_items().empty(),
                   "party money divides in client traversal order with remainder awarded first");
            observe_h1("pickup_party_money", players, packets);
        }
        {
            H1Players players;
            for (std::size_t i = 0; i < 2; ++i) {
                players.clients[i].talent_mask = protocol::talent_bit(29);
                players.clients[i].next_talent_pickup_ms = 10000;
            }
            players.ground(91, 901, 1);
            players.pickup(10000);
            players.pickup(10000, nullptr, 1);
            const auto packets = players.drain();
            expect(players.owner().bag[0].iid == 91 && players.clients[1].bag[0].iid == 0 &&
                       players.world.ground_items().empty() && players.next_iid == 4000,
                   "two scheduled clients cannot receive the same persistent ground identity");
            observe_h1("pickup_interleaved", players, packets);
        }
    }

    void test_h1_autosave() {
        for (const std::uint64_t last : {std::uint64_t{0},
                                         std::uint64_t{10001},
                                         std::uint64_t{10000},
                                         std::uint64_t{10000 + kAutoSaveMs - 1},
                                         std::uint64_t{10000 + kAutoSaveMs}}) {
            H1Players players;
            auto& c = players.owner();
            c.last_autosave_ms = last;
            players.autosave(10000, nullptr);
            const auto packets = players.drain();
            expect(c.last_autosave_ms == last && packets == ScheduledPackets{},
                   "null-store autosave retains timers including unsigned future subtraction");
            observe_h1("autosave_null_" + std::to_string(last), players, packets);
        }
        for (int mode = 0; mode < 12; ++mode) {
            ScheduledStore persistence;
            H1Players players;
            auto& c = players.owner();
            c.bag[0] = h1_mirror(200, 1, 0);
            c.bag[0].index = 501;
            c.bag[0].equipped = true;
            c.transform.index = 501;
            c.transform.level = 3;
            c.transform.exp = 17;
            persistence.seed(c);
            const auto saved_character = persistence.store->load_character(c.character_id);
            expect(saved_character.has_value(), "find autosave fixture account owner");
            if (saved_character) {
                c.account_id = saved_character->account_id;
            }
            const auto id = std::to_string(c.character_id);
            persistence.sql(
                ("INSERT INTO buff_remain(character_id,buff_type,remain) VALUES(" + id +
                 ",1,50); INSERT INTO inventory_unlock(character_id,register_index,remain) VALUES(" +
                 id + ",0,50);")
                    .c_str());
            persistence.sql(
                "CREATE TRIGGER h1_character AFTER UPDATE ON characters BEGIN "
                "INSERT INTO scheduled_audit(kind,iid,level,exp) VALUES('C',NEW.id,0,0); END;"
                "CREATE TRIGGER h1_buffs AFTER DELETE ON buff_remain BEGIN "
                "INSERT INTO scheduled_audit(kind,iid,level,exp) VALUES('B',OLD.character_id,0,0); END;"
                "CREATE TRIGGER h1_unlocks AFTER DELETE ON inventory_unlock BEGIN "
                "INSERT INTO scheduled_audit(kind,iid,level,exp) VALUES('I',OLD.character_id,0,0); END;");
            c.last_autosave_ms = 10000;
            std::uint64_t now_ms = 10000 + kAutoSaveMs;
            if (mode == 1) {
                persistence.sql("CREATE TRIGGER h1_reject_character BEFORE UPDATE ON characters "
                                "BEGIN SELECT RAISE(ABORT,'owned H1 character failure'); END;");
            }
            if (mode == 2) {
                persistence.sql("CREATE TRIGGER h1_reject_buffs BEFORE DELETE ON buff_remain "
                                "BEGIN SELECT RAISE(ABORT,'owned H1 buff failure'); END;");
            }
            if (mode == 3) {
                persistence.sql(
                    "CREATE TRIGGER h1_reject_unlocks BEFORE DELETE ON inventory_unlock "
                    "BEGIN SELECT RAISE(ABORT,'owned H1 unlock failure'); END;");
            }
            if (mode == 4) {
                --now_ms;
            }
            if (mode == 5) {
                c.last_autosave_ms = now_ms + 1;
            }
            if (mode == 6) {
                c.character_id = 0;
            }
            if (mode == 7) {
                c.last_autosave_ms = 0;
            }
            if (mode == 8) {
                ++now_ms;
            }
            if (mode == 9) {
                now_ms += 10 * kAutoSaveMs;
            }
            if (mode == 10) {
                now_ms = c.last_autosave_ms;
            }
            if (mode == 11) {
                persistence.sql("CREATE TRIGGER h1_reject_transform BEFORE UPDATE ON items "
                                "BEGIN SELECT RAISE(ABORT,'owned H1 transform failure'); END;");
            }
            const auto before = steady_ms_now();
            players.autosave(now_ms, persistence.store.get());
            const auto after = steady_ms_now();
            std::vector<std::string> expected;
            if (mode != 4 && mode != 6 && mode != 10) {
                if (mode != 11) {
                    expected.push_back("U,200,3,17");
                }
                if (mode != 1) {
                    expected.push_back("C," + id + ",0,0");
                }
                if (mode != 2) {
                    expected.push_back("B," + id + ",0,0");
                }
                if (mode != 3) {
                    expected.push_back("I," + id + ",0,0");
                }
            }
            expect(persistence.audit() == expected,
                   "autosave preserves transform, character, buffs, unlocks order across failures");
            const bool saved = mode != 1 && mode != 4 && mode != 6 && mode != 10;
            expect(saved ? (c.last_autosave_ms >= before && c.last_autosave_ms <= after)
                         : c.last_autosave_ms == 10000,
                   "successful autosave uses its own fresh clock and failure retains the deadline");
            expect(persistence.store->load_buff_remains(persistence.character_id).size() ==
                           (mode == 2 || mode == 4 || mode == 6 || mode == 10 ? 1u : 0u) &&
                       persistence.store->load_inventory_unlocks(persistence.character_id).size() ==
                           (mode == 3 || mode == 4 || mode == 6 || mode == 10 ? 1u : 0u),
                   "later independent saves still execute after an earlier save fails");
            const auto packets = players.drain();
            expect(packets == ScheduledPackets{}, "autosave emits no protocol packets");
            if (!saved) {
                observe_h1("autosave_failure_or_not_due_" + std::to_string(mode),
                           players,
                           packets,
                           &persistence);
            } else if (h1_snapshot) {
                // This real-clock case has assertions, not an allegedly deterministic raw snapshot.
                std::cout << "h1 autosave_live_clock_bounds_" << mode << " verified\n";
            }
        }
    }

    void test_h1_upkeep() {
        test_h1_collection();
        test_h1_autopick();
        test_h1_autosave();
    }

    using StorageValues = std::array<std::int64_t, 30>;
    bool h2_snapshot = false;

    StorageValues storage_values(const ItemRecord& item) {
        return {item.iid,
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
                item.character_bound};
    }

    std::string storage_character_state(const CharacterRecord& c) {
        std::ostringstream out;
        out << std::quoted(c.name) << ',' << static_cast<int>(c.role) << ','
            << static_cast<int>(c.deletion_state) << ',';
        std::apply([&](const auto&... value) { ((out << value << ','), ...); },
                   std::tie(c.id,
                            c.account_id,
                            c.clazz,
                            c.face,
                            c.hair,
                            c.map,
                            c.map_x,
                            c.map_y,
                            c.map_z,
                            c.revival_point,
                            c.level,
                            c.exp,
                            c.hp,
                            c.max_hp,
                            c.mp,
                            c.max_mp,
                            c.pu_point,
                            c.su_point,
                            c.contribution,
                            c.rage,
                            c.str,
                            c.hth,
                            c.intl,
                            c.wis,
                            c.dex,
                            c.money,
                            c.next_iid,
                            c.weapon_index,
                            c.armor_index,
                            c.transform_index,
                            c.transform_level,
                            c.transform_exp,
                            c.transform_put_on_ticks,
                            c.talent_mask,
                            c.beacon_ready_at,
                            c.specialty,
                            c.delete_after));
        return out.str();
    }

    std::vector<std::uint8_t> storage_payload(const std::vector<std::uint8_t>& frame) {
        expect(frame.size() >= 3, "owned storage request has a complete frame header");
        return frame.size() >= 3 ? std::vector<std::uint8_t>(frame.begin() + 3, frame.end())
                                 : std::vector<std::uint8_t>{};
    }

    struct StoragePlayers : ScheduledPlayers {
        ScheduledStore persistence;
        NpcRegistry npcs;
        std::unique_ptr<IDataStore> null_store;
        std::atomic<std::uint32_t> next_iid{90000};
        std::map<int, std::uint32_t> npc_ids;
        int account_a = 0;
        int account_b = 0;
        int character_b = 0;
        int sibling = 0;

        StoragePlayers() {
            auto& c = owner();
            c.character_id = persistence.character_id;
            const auto saved = persistence.store->load_character(c.character_id);
            expect(saved.has_value(), "find owned storage account A");
            if (saved) {
                auto initial = *saved;
                account_a = initial.account_id;
                c.account_id = account_a;
                c.name = initial.name;
                c.money = initial.money = 180;
                expect(persistence.store->save_character(initial), "seed account A money");
                initial.id = 0;
                initial.name = "Owned storage sibling";
                expect(persistence.store->save_character(initial),
                       "seed second account A character");
                for (const auto& row : persistence.store->list_characters(account_a)) {
                    if (row.name == initial.name) {
                        sibling = row.id;
                    }
                }
            }
            CharacterRecord foreign;
            foreign.name = "Owned storage account B";
            expect(persistence.store->ensure_default_player("storage_b", "owned", foreign),
                   "seed independent storage account B");
            const auto account = persistence.store->find_account("storage_b");
            if (account) {
                account_b = account->id;
                const auto rows = persistence.store->list_characters(account_b);
                if (!rows.empty()) {
                    character_b = rows.front().id;
                }
            }
            expect(account_a > 0 && account_b > 0 && account_a != account_b && sibling > 0,
                   "storage fixture has two actual accounts and an account A sibling");
            c.px = c.py = 1024;
            c.map = 0;
            c.last_autosave_ms = 23;
            c.next_iid = 77;
            const auto dir = persistence.root + "/storage_npcs";
            test::mkdir_p(dir);
            for (int i = 0; i < 4; ++i) {
                std::ofstream file(dir + "/" + std::to_string(60001 + i) + ".json");
                file << "{\"index\":" << 60001 + i
                     << ",\"name\":\"Owned storage NPC\",\"map\":" << (i == 3 ? 1 : 0)
                     << ",\"xy\":[" << (i == 2 ? 4096 : 1024)
                     << ", 1024, 0],\"storage_keeper\":" << (i == 1 ? "false" : "true") << '}';
            }
            expect(npcs.load_dir(dir), "load owned storage NPC definitions");
            std::uint32_t next = 800;
            npcs.spawn_for_maps(next, 0, 1);
            for (const auto& npc : npcs.npcs()) {
                npc_ids[npc.index] = npc.id;
            }
            expect(npc_ids.size() == 4, "resolve keeper IDs by owned definition identity");
            c.active_storage_npc_id = keeper();
            for (int index : {901, 902, 903, 904, 905}) {
                ItemCombatDef definition;
                definition.index = index;
                definition.class_big = index == 903 || index == 904 ? "quest" : "general";
                definition.class_small = "etc";
                definition.stackable = index == 901;
                definition.is_task = index == 904;
                definition.is_transform = index == 905;
                GameWorldCoveragePeer::add_item(world, definition);
            }
            persistence.sql(
                "CREATE TABLE storage_audit(seq INTEGER PRIMARY KEY,kind TEXT,iid INTEGER,"
                "owner INTEGER,account INTEGER,amount INTEGER,slot INTEGER);"
                "CREATE TRIGGER storage_insert AFTER INSERT ON items BEGIN INSERT INTO storage_audit"
                "(kind,iid,owner,account,amount,slot) VALUES('I',NEW.iid,coalesce(NEW.character_id,0),"
                "NEW.account_id,NEW.count,NEW.slot); END;"
                "CREATE TRIGGER storage_update AFTER UPDATE ON items BEGIN INSERT INTO storage_audit"
                "(kind,iid,owner,account,amount,slot) VALUES('U',NEW.iid,coalesce(NEW.character_id,0),"
                "NEW.account_id,NEW.count,NEW.slot); END;"
                "CREATE TRIGGER storage_delete AFTER DELETE ON items BEGIN INSERT INTO storage_audit"
                "(kind,iid,owner,account,amount,slot) VALUES('D',OLD.iid,coalesce(OLD.character_id,0),"
                "OLD.account_id,OLD.count,OLD.slot); END;"
                "CREATE TRIGGER storage_character AFTER UPDATE ON characters BEGIN INSERT INTO storage_audit"
                "(kind,iid,owner,account,amount,slot) VALUES('C',NEW.id,NEW.id,NEW.account_id,NEW.money,0); END;");
        }

        std::uint32_t keeper() const {
            const auto it = npc_ids.find(60001);
            return it == npc_ids.end() ? 0 : it->second;
        }

        ItemRecord item(int iid, int count = 10, int index = 901) const {
            ItemRecord row;
            row.iid = iid;
            row.character_id = persistence.character_id;
            row.account_id = account_a;
            row.index = index;
            row.count = count;
            row.prefix = 7;
            row.cur_end = 13;
            row.max_end = 29;
            row.x_attack = 11;
            row.x_magic = 12;
            row.x_hit = 13;
            row.x_defense = 14;
            row.x_dodge = 15;
            row.upgrade_level = 3;
            row.upgrade_rate = 4;
            row.mix_type = 5;
            row.bof = 1;
            row.qigong_perforated = 1;
            row.qigong_effect = 21;
            row.dragon_effect = 31;
            row.random_stat_type = 106;
            row.random_stat_value = 8;
            row.transform_level = 5;
            row.transform_exp = 234;
            row.pet_expire_at = 4102444800;
            row.ride_expire_at = 4102444900;
            row.account_bound = true;
            row.enchant_protected = true;
            return row;
        }

        void bag(ItemRecord row, int position = 0) {
            expect(persistence.store->insert_item(row), "seed owned storage source bag row");
            owner().bag[static_cast<std::size_t>(position)] = bag_slot_of(row);
        }

        void stored(ItemRecord row, int slot = 0) {
            row.character_id = 0;
            row.slot = slot;
            expect(persistence.store->insert_item(row), "seed owned account-storage row");
        }

        void clear_audit() {
            persistence.sql("DELETE FROM storage_audit; DELETE FROM scheduled_audit;");
        }

        bool info(const std::vector<std::uint8_t>& payload, bool no_store = false) {
#if defined(OPENKAL_STORAGE_BASELINE_BRIDGE)
            return h2_baseline::info(
                owner(), payload, npcs, no_store ? null_store : persistence.store);
#else
            return handle_storage_info_request(
                owner(), payload, npcs, no_store ? nullptr : persistence.store.get());
#endif
        }

        bool
        transfer(bool deposit, const std::vector<std::uint8_t>& payload, bool no_store = false) {
#if defined(OPENKAL_STORAGE_BASELINE_BRIDGE)
            if (deposit) {
                return h2_baseline::deposit(owner(),
                                            payload,
                                            world,
                                            npcs,
                                            no_store ? null_store : persistence.store,
                                            next_iid);
            }
            return h2_baseline::withdraw(
                owner(), payload, world, npcs, no_store ? null_store : persistence.store, next_iid);
#else
            if (deposit) {
                return handle_storage_deposit_request(owner(),
                                                      payload,
                                                      world,
                                                      npcs,
                                                      no_store ? nullptr : persistence.store.get(),
                                                      next_iid);
            }
            return handle_storage_withdraw_request(owner(),
                                                   payload,
                                                   world,
                                                   npcs,
                                                   no_store ? nullptr : persistence.store.get(),
                                                   next_iid);
#endif
        }

        bool transfer(bool deposit,
                      std::uint32_t iid,
                      std::uint32_t count,
                      std::uint32_t npc = 0,
                      bool no_store = false) {
            return transfer(deposit,
                            storage_payload(protocol::Storage::frame_transfer_c2s(
                                deposit, npc == 0 ? keeper() : npc, iid, count)),
                            no_store);
        }

        std::vector<std::string> audit() const {
            std::vector<std::string> rows;
            sqlite3_stmt* statement = nullptr;
            expect(sqlite3_prepare_v2(
                       persistence.db.get(),
                       "SELECT kind,iid,owner,account,amount,slot FROM storage_audit ORDER BY seq",
                       -1,
                       &statement,
                       nullptr) == SQLITE_OK,
                   "read ordered committed storage statement effects");
            if (statement) {
                while (sqlite3_step(statement) == SQLITE_ROW) {
                    std::string row =
                        reinterpret_cast<const char*>(sqlite3_column_text(statement, 0));
                    for (int column = 1; column < 6; ++column) {
                        row += ',' + std::to_string(sqlite3_column_int64(statement, column));
                    }
                    rows.push_back(row);
                }
                sqlite3_finalize(statement);
            }
            return rows;
        }

        std::vector<StorageValues> rows() const {
            std::map<int, StorageValues> all;
            for (int id : {persistence.character_id, sibling, character_b}) {
                for (const auto& row : persistence.store->load_items(id)) {
                    all[row.iid] = storage_values(row);
                }
            }
            for (int id : {account_a, account_b}) {
                for (const auto& row : persistence.store->load_storage_items(id)) {
                    all[row.iid] = storage_values(row);
                }
            }
            std::vector<StorageValues> result;
            for (const auto& [iid, values] : all) {
                (void) iid;
                result.push_back(values);
            }
            return result;
        }
    };

    void observe_storage(std::string_view name,
                         StoragePlayers& players,
                         bool reached_end,
                         const ScheduledPackets& packets,
                         bool real_autosave = false) {
        if (!h2_snapshot) {
            return;
        }
        const auto& c = players.owner();
        std::cout << "h2 " << name << " reached_end=" << reached_end
                  << " active=" << c.active_storage_npc_id << " account=" << c.account_id
                  << " character=" << c.character_id << " money=" << c.money
                  << " session_next=" << c.next_iid << " global_next=" << players.next_iid.load()
                  << " phase=" << static_cast<int>(c.phase) << " hp=" << c.cur_hp
                  << " teleport=" << c.tele_pending << " gstate=" << c.gstate << '\n';
        if (real_autosave) {
            std::cout
                << "last_autosave=fresh_real_clock_bounds_asserted; remaining_storage_state_rows_and_wire_exact\n";
        } else {
            std::cout << "last_autosave=" << c.last_autosave_ms << '\n';
        }
        for (std::size_t peer = 0; peer < players.clients.size(); ++peer) {
            std::cout << "bag " << peer << '\n'
                      << combat_inventory_state(players.clients[peer]) << '\n';
            for (const auto& frame : packets[peer]) {
                for (auto byte : frame) {
                    std::cout << std::hex << std::setw(2) << std::setfill('0')
                              << static_cast<int>(byte);
                }
                std::cout << std::dec << std::setfill(' ') << '\n';
            }
        }
        for (const auto& row : players.rows()) {
            std::cout << "row ";
            for (auto value : row) {
                std::cout << value << ',';
            }
            std::cout << '\n';
        }
        for (const auto& row : players.audit()) {
            std::cout << "audit " << row << '\n';
        }
        for (int id : {players.persistence.character_id, players.sibling, players.character_b}) {
            const auto row = players.persistence.store->load_character(id);
            if (row) {
                std::cout << "character " << storage_character_state(*row) << '\n';
            }
        }
    }
    void test_storage_info_requests() {
        {
            StoragePlayers players;
            auto payload = storage_payload(protocol::Storage::frame_info_c2s(players.keeper()));
            for (std::size_t size = 0; size <= payload.size(); ++size) {
                auto malformed = payload;
                if (size < payload.size()) {
                    malformed.resize(size);
                } else {
                    malformed.push_back(0x7f);
                }
                players.owner().active_storage_npc_id = 123456;
                const bool end = players.info(malformed);
                const auto packets = players.drain();
                expect(!end && players.owner().active_storage_npc_id == 123456 &&
                           packets == ScheduledPackets{},
                       "malformed Info preserves the previous keeper and continues the frame loop");
                observe_storage("info_malformed_" + std::to_string(size), players, end, packets);
            }
        }
        for (int gate = 0; gate < 6; ++gate) {
            StoragePlayers p;
            auto& c = p.owner();
            std::uint32_t npc = gate == 0  ? 999999
                                : gate < 4 ? p.npc_ids.at(60001 + gate)
                                           : p.keeper();
            if (gate >= 4) {
                c.px += protocol::kNpcReplyRange + (gate == 5 ? 1 : 0);
            }
            c.active_storage_npc_id = 123456;
            const bool end = p.info(storage_payload(protocol::Storage::frame_info_c2s(npc)));
            const auto packets = p.drain();
            expect(end == (gate == 4) && c.active_storage_npc_id == (gate == 4 ? npc : 0) &&
                       packets[0].size() == (gate == 4 ? 1u : 0u) && packets[1].empty() &&
                       packets[2].empty(),
                   "Info uses exact keeper/map/radius gates and resets only invalid contexts");
            observe_storage("info_context_" + std::to_string(gate), p, end, packets);
        }
        for (int gate = 0; gate < 3; ++gate) {
            StoragePlayers p;
            if (gate == 0) {
                p.owner().account_id = 0;
            }
            if (gate == 1) {
                p.owner().sock.reset();
            }
            const bool end =
                p.info(storage_payload(protocol::Storage::frame_info_c2s(p.keeper())), gate == 2);
            const auto packets = p.drain();
            expect(end && p.owner().active_storage_npc_id == p.keeper() &&
                       packets == ScheduledPackets{},
                   "valid Info still selects the keeper without account, socket or store");
            observe_storage("info_no_reply_" + std::to_string(gate), p, end, packets);
        }
        for (int count : {215, 216}) {
            StoragePlayers p;
            for (int i = 0; i < count; ++i) {
                p.stored(p.item(1000 + i, 1), i);
            }
            auto foreign = p.item(7000, 3);
            foreign.account_id = p.account_b;
            p.stored(foreign, 0);
            p.clear_audit();
            const bool end = p.info(storage_payload(protocol::Storage::frame_info_c2s(p.keeper())));
            const auto packets = p.drain();
            expect(end && packets[0].size() == 1 && p.audit().empty(),
                   "the storage snapshot includes every occupied slot up to full capacity");
            if (packets[0].size() == 1) {
                protocol::StorageInfo info;
                const auto& frame = packets[0].front();
                expect(frame.size() == (count == 215 ? 7964u : 8000u) &&
                           protocol::Storage::parse_info_s2c(
                               frame.data() + 3, frame.size() - 3, info) &&
                           info.items.size() == static_cast<std::size_t>(count) &&
                           info.items.front().iid == 1000 &&
                           info.items.back().iid == static_cast<std::uint32_t>(999 + count) &&
                           info.items.front().charm.x_attack == 11 &&
                           info.items.front().transform_level == 5 &&
                           info.items.front().transform_exp == 234,
                       "Info preserves authored slot order and exposes only the selected account");
            }
            observe_storage("info_capacity_" + std::to_string(count), p, end, packets);
        }
    }

    void test_storage_transfer_guards() {
        for (const bool deposit : {true, false}) {
            StoragePlayers p;
            if (deposit) {
                p.bag(p.item(100));
            } else {
                p.stored(p.item(100));
            }
            p.clear_audit();
            const auto before = p.rows();
            const auto valid =
                storage_payload(protocol::Storage::frame_transfer_c2s(deposit, p.keeper(), 100, 3));
            for (std::size_t size = 0; size <= valid.size(); ++size) {
                auto malformed = valid;
                if (size < valid.size()) {
                    malformed.resize(size);
                } else {
                    malformed.push_back(0x7f);
                }
                const bool end = p.transfer(deposit, malformed);
                const auto packets = p.drain();
                expect(!end && p.rows() == before && p.audit().empty() && p.owner().money == 180 &&
                           p.owner().last_autosave_ms == 23 && p.next_iid == 90000 &&
                           p.owner().active_storage_npc_id == p.keeper() &&
                           packets == ScheduledPackets{},
                       "transfer parsers reject every truncated/trailing payload without effects");
                observe_storage(std::string(deposit ? "deposit" : "withdraw") + "_malformed_" +
                                    std::to_string(size),
                                p,
                                end,
                                packets);
            }
            expect(p.info(storage_payload(protocol::Storage::frame_info_c2s(p.keeper()))) &&
                       p.drain()[0].size() == 1,
                   "a valid storage request remains usable after malformed request replays");
        }
        for (const bool deposit : {true, false}) {
            const int limit = deposit ? 19 : 14;
            for (int gate = 0; gate < limit; ++gate) {
                StoragePlayers p;
                auto& c = p.owner();
                if (deposit) {
                    p.bag(p.item(100));
                } else {
                    p.stored(p.item(100));
                }
                std::uint32_t npc = p.keeper(), iid = 100, quantity = 3;
                if (gate == 0) {
                    c.active_storage_npc_id = 0;
                }
                if (gate == 1) {
                    npc = p.npc_ids.at(60002);
                }
                if (gate == 2) {
                    c.account_id = 0;
                }
                if (gate >= 4 && gate <= 7) {
                    npc = gate == 4 ? 999999 : p.npc_ids.at(60000 + gate - 3);
                    c.active_storage_npc_id = npc;
                }
                if (gate == 8) {
                    c.px += protocol::kNpcReplyRange + 1;
                }
                if (gate == 9) {
                    iid = 99999;
                }
                if (gate == 10) {
                    quantity = 0;
                }
                if (gate == 11) {
                    quantity = 11;
                }
                if (gate == 12) {
                    quantity = std::numeric_limits<std::uint32_t>::max();
                }
                if (gate == 13) {
                    if (deposit) {
                        c.bag[0].count = 0;
                    } else {
                        p.stored(p.item(100, 10, 902));
                    }
                }
                if (gate == 14) {
                    c.bag[0].equipped = true;
                }
                if (gate == 15) {
                    c.bag[0].index = 904;
                }
                if (gate == 16) {
                    c.bag[0].character_bound = true;
                }
                if (gate == 17) {
                    c.bag[0].index = 902;
                }
                if (gate == 18) {
                    c.money = 59;
                }
                p.clear_audit();
                const auto rows = p.rows();
                const auto before = combat_inventory_state(c);
                const auto money = c.money;
                const auto active = c.active_storage_npc_id;
                const bool end = p.transfer(deposit, iid, quantity, npc, gate == 3);
                const auto packets = p.drain();
                expect(!end && p.rows() == rows && p.audit().empty() &&
                           combat_inventory_state(c) == before && c.money == money &&
                           c.active_storage_npc_id == active && c.last_autosave_ms == 23 &&
                           p.next_iid == 90000 && packets[1].empty() && packets[2].empty(),
                       "storage rejection preserves source, ownership, fee, allocator and keeper");
                observe_storage(std::string(deposit ? "deposit_guard_" : "withdraw_guard_") +
                                    std::to_string(gate),
                                p,
                                end,
                                packets);
            }
        }
        {
            StoragePlayers p;
            p.bag(p.item(100));
            for (int slot = 0; slot < protocol::kStorageCapacity; ++slot) {
                p.stored(p.item(1000 + slot, 1), slot);
            }
            p.clear_audit();
            const auto rows = p.rows();
            const bool end = p.transfer(true, 100, 3);
            const auto packets = p.drain();
            expect(!end && p.rows() == rows && p.audit().empty() && p.owner().money == 180 &&
                       p.next_iid == 90000 &&
                       packets[0] ==
                           std::vector<std::vector<std::uint8_t>>{protocol::Chatting::frame_s2c(
                               "Server", "Your account storage is full.")},
                   "full storage rejects before split allocation and emits only its current chat");
            observe_storage("deposit_full_storage", p, end, packets);
        }
    }

    void test_storage_successes() {
        for (const bool deposit : {true, false}) {
            for (int mode = 0; mode < 9; ++mode) {
                StoragePlayers p;
                auto& c = p.owner();
                auto original = p.item(100, 10, mode == 4 ? 905 : mode == 8 ? 906 : 901);
                const std::uint32_t quantity = mode == 0 || mode == 3 || mode == 8 ? 10 : 3;
                if (mode == 4) {
                    original.count = 1;
                }
                const auto requested = mode == 4 || mode == 5 ? 1u : quantity;
                if (deposit) {
                    p.bag(original, 2);
                    p.bag(p.item(101, 1, 902), 4);
                    p.stored(p.item(200, 5), 0);
                    p.stored(p.item(201, 5), 2);
                    if (mode == 2) {
                        c.money = 60;
                    }
                } else {
                    p.stored(original, 7);
                    if (mode == 2) {
                        c.character_id = p.sibling;
                    }
                    auto existing = p.item(101, 5);
                    existing.character_id = c.character_id;
                    p.bag(existing, 0);
                    if (mode == 3) {
                        c.sock.reset();
                    }
                }
                if (mode == 6) {
                    c.sock.reset();
                }
                if (mode == 7) {
                    c.cur_hp = 0;
                    c.tele_pending = true;
                    c.gstate |= protocol::kCgsFish | protocol::kCgsCooking;
                }
                p.clear_audit();
                const bool partial = requested < static_cast<std::uint32_t>(original.count);
                const auto before_money = c.money;
                const auto before_clock = steady_ms_now();
                std::srand(82);
                const int expected_random = std::rand();
                std::srand(82);
                const bool end = p.transfer(deposit, 100, requested);
                const int next_random = std::rand();
                const auto after_clock = steady_ms_now();
                const auto packets = p.drain();
                auto moved = original;
                moved.iid = partial ? 90001 : 100;
                moved.count = static_cast<int>(requested);
                moved.character_id = deposit ? 0 : c.character_id;
                moved.slot = deposit ? 1 : 0;
                const auto values = storage_values(moved);
                const auto rows = p.rows();
                expect(
                    end && std::find(rows.begin(), rows.end(), values) != rows.end() &&
                        p.next_iid == (partial ? 90001u : 90000u) && c.next_iid == 77 &&
                        c.money == before_money - (deposit ? 60 : 0) &&
                        next_random == expected_random,
                    "successful whole/split transfer preserves all30fields with exact fee and IID rules");
                if (partial) {
                    auto remainder = original;
                    remainder.count -= static_cast<int>(requested);
                    remainder.character_id = deposit ? c.character_id : 0;
                    remainder.slot = deposit ? 0 : 7;
                    const auto expected = storage_values(remainder);
                    expect(std::find(rows.begin(), rows.end(), expected) != rows.end(),
                           "split remainder retains its owner, identity, slot and all metadata");
                }
                if (deposit) {
                    expect(
                        c.last_autosave_ms >= before_clock && c.last_autosave_ms <= after_clock &&
                            (!partial ? c.bag[0].iid == 101 : c.bag[2].iid == 100) &&
                            (mode == 6 ? packets[0].empty() : packets[0].size() == 3) &&
                            packets[1].empty() && packets[2].empty(),
                        "deposit compaction and bag/money/storage wire order follow character persistence");
                    if (packets[0].size() == 3) {
                        expect(
                            packets[0][0][2] ==
                                    static_cast<std::uint8_t>(protocol::S2C::InsertItem) &&
                                packets[0][1][2] ==
                                    static_cast<std::uint8_t>(protocol::S2C::InsertItem) &&
                                packets[0][2][2] ==
                                    static_cast<std::uint8_t>(protocol::S2C::StorageInfo),
                            "deposit sends item delta then absolute money then storage snapshot");
                    }
                } else {
                    expect(
                        c.last_autosave_ms == 23 &&
                            c.bag[1].iid == static_cast<std::uint32_t>(moved.iid) &&
                            packets[1].empty() && packets[2].empty() &&
                            (mode == 3 || mode == 6 ? packets[0].empty()
                                                    : packets[0].size() == (mode == 4 ? 3u : 2u)),
                        "withdrawal takes first empty cell without merging, charges nothing and sends transform info when needed");
                }
                if (mode == 7) {
                    expect(c.cur_hp == 0 && c.tele_pending &&
                               (c.gstate & (protocol::kCgsFish | protocol::kCgsCooking)) ==
                                   (protocol::kCgsFish | protocol::kCgsCooking),
                           "storage transfer adds no dead, teleport or activity restrictions");
                }
                observe_storage(std::string(deposit ? "deposit_success_" : "withdraw_success_") +
                                    std::to_string(mode),
                                p,
                                end,
                                packets,
                                deposit);
                if (partial && mode == 1) {
                    const auto second_before = steady_ms_now();
                    const bool repeated = p.transfer(deposit, 100, requested);
                    const auto second_after = steady_ms_now();
                    const auto repeated_packets = p.drain();
                    auto second = moved;
                    second.iid = 90002;
                    second.slot = deposit ? 3 : 0;
                    const auto expected = storage_values(second);
                    const auto repeated_rows = p.rows();
                    expect(
                        repeated && p.next_iid == 90002 && c.next_iid == 77 &&
                            c.money == (deposit ? 60 : 180) &&
                            std::find(repeated_rows.begin(), repeated_rows.end(), expected) !=
                                repeated_rows.end(),
                        "repeating a partial request transfers another split with a fresh global IID");
                    expect(
                        deposit ? c.last_autosave_ms >= second_before &&
                                      c.last_autosave_ms <= second_after
                                : c.last_autosave_ms == 23,
                        "repeated deposit saves with its own clock while repeated withdrawal stays free");
                    observe_storage(deposit ? "deposit_repeat_partial" : "withdraw_repeat_partial",
                                    p,
                                    repeated,
                                    repeated_packets,
                                    deposit);
                }
                if (!partial) {
                    const auto before_repeat = p.rows();
                    const auto repeat_clock = c.last_autosave_ms;
                    const bool repeated = p.transfer(deposit, 100, requested);
                    expect(
                        !repeated && p.rows() == before_repeat &&
                            c.last_autosave_ms == repeat_clock,
                        "a completed whole request cannot move its former source identity twice");
                    (void) p.drain();
                }
            }
        }
        for (int mode = 0; mode < 6; ++mode) {
            StoragePlayers p;
            auto& c = p.owner();
            const int index = mode == 2 || mode == 4 ? 903 : mode == 3 || mode == 5 ? 904 : 901;
            p.stored(p.item(100, 1, index));
            const int first = mode >= 2 ? ClientSession::kGeneralSlots +
                                              (index == 904 ? ClientSession::kSlotsPerRegister : 0)
                                        : 0;
            const int count = mode >= 4  ? ClientSession::kSlotsPerRegister
                              : mode < 2 ? 2 * ClientSession::kSlotsPerRegister
                                         : 0;
            for (int i = 0; i < count; ++i) {
                p.bag(p.item(1000 + i, 1, index), first + i);
            }
            if (mode == 1) {
                expect(c.inventory_unlocks.apply(3600, 4102441200),
                       "seed future General register unlock");
            }
            p.clear_audit();
            const auto before = p.rows();
            const bool end = p.transfer(false, 100, 1);
            const auto packets = p.drain();
            const bool allowed = mode == 1 || mode == 2 || mode == 3;
            expect(
                end == allowed && c.money == 180 && c.last_autosave_ms == 23 &&
                    (allowed ? c.bag[static_cast<std::size_t>(mode == 1 ? 72 : first)].iid == 100
                             : p.rows() == before),
                "withdrawal respects unlocked General and independent Quest/Task half-open ranges");
            observe_storage("withdraw_capacity_" + std::to_string(mode), p, end, packets);
        }
    }

    void test_storage_failures() {
        for (const bool deposit : {true, false}) {
            for (int fault = 0; fault < (deposit ? 6 : 4); ++fault) {
                StoragePlayers p;
                auto& c = p.owner();
                auto original = p.item(100);
                if (deposit) {
                    p.bag(original);
                } else {
                    p.stored(original, 5);
                }
                if (fault == 5) {
                    auto worn = p.item(101, 1, 905);
                    worn.slot = 3;
                    p.bag(worn, 1);
                    c.bag[1].equipped = true;
                    c.transform.index = 905;
                    c.transform.level = 7;
                    c.transform.exp = 19;
                }
                const bool whole = fault == 3 || fault >= 4;
                const auto quantity = whole ? 10u : 3u;
                if (fault == 0) {
                    p.persistence.sql(
                        "CREATE TRIGGER storage_fault BEFORE INSERT ON items WHEN NEW.iid=90001 "
                        "BEGIN SELECT RAISE(ABORT,'owned storage insert failure'); END;");
                } else if (fault == 1 || fault == 2 || fault == 3) {
                    p.persistence.sql(
                        "CREATE TRIGGER storage_fault BEFORE UPDATE ON items WHEN OLD.iid=100 "
                        "BEGIN SELECT RAISE(ABORT,'owned storage update failure'); END;");
                    if (fault == 2) {
                        p.persistence.sql(
                            "CREATE TRIGGER storage_delete_fault BEFORE DELETE ON items WHEN OLD.iid=90001 "
                            "BEGIN SELECT RAISE(ABORT,'owned storage cleanup failure'); END;");
                    }
                } else {
                    p.persistence.sql(
                        "CREATE TRIGGER storage_fault BEFORE UPDATE ON characters "
                        "BEGIN SELECT RAISE(ABORT,'owned storage character failure'); END;");
                }
                p.clear_audit();
                const auto before = p.rows();
                const bool end = p.transfer(deposit, 100, quantity);
                const auto packets = p.drain();
                const auto rows = p.rows();
                expect(
                    end == (fault >= 4) && p.next_iid == (whole ? 90000u : 90001u) &&
                        c.last_autosave_ms == 23 && c.money == (fault >= 4 ? 120 : 180) &&
                        c.next_iid == 77 && packets[1].empty() && packets[2].empty(),
                    "storage failure stages retain their old flow, allocator, fee and clock effects");
                std::string effects;
                for (const auto& row : p.audit()) {
                    effects += row.front();
                }
                expect(
                    effects == (fault == 0 || fault == 3 ? ""
                                : fault == 1             ? "ID"
                                : fault == 2             ? "I"
                                : fault == 4             ? "U"
                                                         : "UU"),
                    "AFTER triggers report committed effects including successful compensation only");
                if (fault == 0 || fault == 1 || fault == 3) {
                    expect(rows == before,
                           "failed transfer with successful cleanup retains exact rows");
                }
                if (fault == 2) {
                    auto leaked = original;
                    leaked.iid = 90001;
                    leaked.count = 3;
                    leaked.character_id = deposit ? 0 : c.character_id;
                    leaked.slot = 0;
                    const auto expected = storage_values(leaked);
                    expect(
                        rows.size() == before.size() + 1 &&
                            std::find(rows.begin(), rows.end(), expected) != rows.end(),
                        "ignored cleanup failure leaves the existing persisted split while existing count remains");
                }
                if (fault >= 4) {
                    const auto saved = p.persistence.store->load_character(c.character_id);
                    expect(
                        saved && saved->money == 180 && packets[0].size() == 3 &&
                            (fault != 5 || c.bag[0].transform_level == 7),
                        "failed character save follows successful transfer and worn flush without rollback");
                } else {
                    expect(packets[0] ==
                               std::vector<std::vector<std::uint8_t>>{protocol::Chatting::frame_s2c(
                                   "Server",
                                   deposit ? "The item could not be stored."
                                           : "The item could not be withdrawn.")},
                           "rejected transfer emits only its existing failure chat");
                }
                observe_storage(std::string(deposit ? "deposit_fault_" : "withdraw_fault_") +
                                    std::to_string(fault),
                                p,
                                end,
                                packets);
            }
        }
        {
            StoragePlayers p;
            auto foreign = p.item(100);
            foreign.account_id = p.account_b;
            p.stored(foreign);
            p.clear_audit();
            const auto before = p.rows();
            const bool rejected = p.transfer(false, 100, 3);
            expect(!rejected && p.rows() == before && p.audit().empty(),
                   "account A cannot withdraw an account B IID even when its identity is known");
            (void) p.drain();
            p.owner().account_id = p.account_b;
            p.owner().character_id = p.character_b;
            const bool end = p.transfer(false, 100, 3);
            const auto packets = p.drain();
            expect(end && p.owner().bag[0].iid == 90001 && p.next_iid == 90001,
                   "sequential independent account handoff uses the shared global split allocator");
            observe_storage("withdraw_accounts", p, end, packets);
        }
    }

    void test_storage_requests() {
        test_storage_info_requests();
        test_storage_transfer_guards();
        test_storage_successes();
        test_storage_failures();
    }

    // Draft insertion inside the existing Host coverage test's anonymous namespace.
    // This reuses StoragePlayers and does not create another Host translation unit.

    bool h4_snapshot = false;

    struct RoutingPlayers : StoragePlayers {
        ServerConfig config;
        ServerEventManager event;
        FishingCatalog fishing;
        QuestDb quest_db;
        std::unique_ptr<QuestEngine> engine;
        std::mutex world_mutex;
        bool cooking_deadline_bounded = false;
        std::array<bool, 3> fishing_deadline_bounded{};
        std::unordered_map<int, std::uint32_t> routing_ids;

        RoutingPlayers() {
            const auto dir = persistence.root + "/routing_npcs";
            const auto quests = persistence.root + "/routing_quests";
            test::mkdir_p(dir);
            test::mkdir_p(quests);
            const auto npc = [&](int index, int kind, int html, const std::string& extra) {
                std::ofstream out(dir + "/" + std::to_string(index) + ".json");
                out << "{\"index\":" << index << ",\"map\":0,\"kind\":" << kind
                    << ",\"html\":" << html << ",\"xy\":[1024, 1024, 0]" << extra << '}';
            };
            npc(61001, 0, 7001, ",\"quest\":[6200, 1]");
            npc(61002, 3, 7002, ",\"quest\":[6201, 1]");
            npc(61003, 0, 0, ",\"quest\":[6202, 1]");
            npc(61004, 5, 7004, ",\"teleports\":[[0, 1100, 1100]]");
            npc(61005, 5, 0, ",\"teleports\":[[0, 1120, 1120]]");
            npc(61006, 5, 7006, "");
            npc(61007, 5, 7007, ",\"teleports\":[[0, 1100, 1100], [0, 1200, 1200]]");
            npc(61008, 0, kSpeedMedicineDialogId, "");
            npc(61009, 0, kConsumableCraftingDialogId, "");
            // The manager intentionally also opens the speed-medicine dialog.
            npc(61010, 0, kSpeedMedicineDialogId, "");
            expect(npcs.load_dir(dir), "load owned routing NPC definitions");
            std::uint32_t next = 1600;
            npcs.spawn_for_map(next, 0);
            for (const auto& value : npcs.npcs()) {
                routing_ids[value.index] = value.id;
            }
            expect(routing_ids.size() == 10,
                   "resolve routing identities independently of load order");
            for (int q = 6200; q <= 6202; ++q) {
                std::ofstream out(quests + "/" + std::to_string(q) + ".json");
                out << "{\"index\":" << q
                    << ",\"flag\":1,\"cases\":[{\"then\":[{\"op\":\"html\",\"id\":" << q + 1000
                    << "},{\"op\":\"save\",\"quest\":" << q << ",\"flag\":2}]}]}";
            }
            {
                std::ofstream out(quests + "/6203.json");
                out << R"({"index":6203,"flag":1,"cases":[{"then":[{"op":"gstate","gstate":5,"value":1}]}]})";
            }
            expect(quest_db.load_dir(quests), "load owned routing quest steps");
            engine = std::make_unique<QuestEngine>(quest_db);
            const auto event_path = persistence.root + "/routing_event.json";
            {
                std::ofstream out(event_path);
                out << R"({"events":[{"id":"exp_event","managerNpcIndex":61010,"donateQuest":{"index":6250,"flag":1},"donateHundredQuest":{"index":6251,"flag":1},"donationItem":23016,"durationSeconds":3600,"extensionSeconds":900,"requiredItems":100,"bonusStepPercent":10,"maxBonusPercent":50}]})";
            }
            expect(event.load_file(event_path) && event.restore(*persistence.store, 1000),
                   "restore owned EXP event");
            expect(
                fishing.load_json_text(
                    R"({"schema":"openkal.fishing/v1","baseIntervalSeconds":30,"acceleratedIntervalSeconds":15,"rewards":[{"itemIndex":901,"chancePerThousand":1000}]})"),
                "load owned fishing timings");
            for (int index : {23016,
                              kDemonBloodItemIndex,
                              kSeaBreamItemIndex,
                              kBambooKegItemIndex,
                              kBloodFishItemIndex}) {
                ItemCombatDef definition;
                definition.index = index;
                definition.class_big = "general";
                definition.class_small = "etc";
                definition.stackable = true;
                GameWorldCoveragePeer::add_item(world, definition);
            }
            persistence.sql(
                "CREATE TABLE routing_audit(seq INTEGER PRIMARY KEY,kind TEXT,detail TEXT);"
                "CREATE TRIGGER routing_item_insert AFTER INSERT ON items BEGIN INSERT INTO routing_audit(kind,detail) VALUES('I',NEW.iid||','||NEW.count); END;"
                "CREATE TRIGGER routing_item_update AFTER UPDATE ON items BEGIN INSERT INTO routing_audit(kind,detail) VALUES('U',NEW.iid||','||NEW.count); END;"
                "CREATE TRIGGER routing_item_delete AFTER DELETE ON items BEGIN INSERT INTO routing_audit(kind,detail) VALUES('D',OLD.iid||','||OLD.count); END;"
                "CREATE TRIGGER routing_quest_insert AFTER INSERT ON quest_flags BEGIN INSERT INTO routing_audit(kind,detail) VALUES('Q',NEW.character_id||','||NEW.quest||','||NEW.flag||','||NEW.clear_flag); END;"
                "CREATE TRIGGER routing_quest_update AFTER UPDATE ON quest_flags BEGIN INSERT INTO routing_audit(kind,detail) VALUES('Q',NEW.character_id||','||NEW.quest||','||NEW.flag||','||NEW.clear_flag); END;"
                "CREATE TRIGGER routing_event_insert AFTER INSERT ON server_events BEGIN INSERT INTO routing_audit(kind,detail) VALUES('E',NEW.contribution_count||','||NEW.active_bonus_percent||','||NEW.active_until); END;"
                "CREATE TRIGGER routing_event_update AFTER UPDATE ON server_events BEGIN INSERT INTO routing_audit(kind,detail) VALUES('E',NEW.contribution_count||','||NEW.active_bonus_percent||','||NEW.active_until); END;"
                "CREATE TRIGGER routing_character_update AFTER UPDATE ON characters BEGIN INSERT INTO routing_audit(kind,detail) VALUES('C',NEW.id||','||NEW.money); END;");
            clients[1].character_id = character_b;
            clients[1].account_id = account_b;
            config.spawn_z = 17;
            owner().dialog_npc_id = 123;
            owner().active_speed_medicine_npc_id = 124;
            owner().active_consumable_crafting_npc_id = 125;
            owner().active_exp_event_npc_id = 126;
            owner().active_shop_npc_id = 127;
            owner().active_storage_npc_id = 128;
            owner().pending_npc_teleport_id = id(61004);
            owner().next_fishing_at_ms = 42;
            owner().cooking_complete_at_ms = 43;
            owner().cooking_order = {2, 3, 4};
            owner().money = 10000;
            clear_audit();
        }

        void clear_audit() {
            StoragePlayers::clear_audit();
            persistence.sql("DELETE FROM routing_audit;");
        }

        std::vector<std::string> audit() const {
            std::vector<std::string> rows;
            sqlite3_stmt* statement = nullptr;
            expect(sqlite3_prepare_v2(persistence.db.get(),
                                      "SELECT kind,detail FROM routing_audit ORDER BY seq",
                                      -1,
                                      &statement,
                                      nullptr) == SQLITE_OK,
                   "read ordered committed routing effects");
            if (statement) {
                while (sqlite3_step(statement) == SQLITE_ROW) {
                    rows.push_back(
                        std::string(
                            reinterpret_cast<const char*>(sqlite3_column_text(statement, 0))) +
                        ',' + reinterpret_cast<const char*>(sqlite3_column_text(statement, 1)));
                }
                sqlite3_finalize(statement);
            }
            return rows;
        }

        std::uint32_t id(int index) const {
            return routing_ids.at(index);
        }

        const NpcEntity* select(const std::vector<std::uint8_t>& payload) {
#if defined(OPENKAL_ROUTING_BASELINE_BRIDGE)
            return h4_baseline::select(owner(), payload, npcs, world_mutex, event);
#else
            return select_dialog_npc(owner(), payload, npcs, world_mutex, event);
#endif
        }

        const NpcEntity* select(int index) {
            return select(storage_payload(protocol::Quest::frame_ask_npc(id(index))));
        }

        bool portal(const NpcEntity* npc) {
#if defined(OPENKAL_ROUTING_BASELINE_BRIDGE)
            return h4_baseline::portal(owner(), npc);
#else
            return handle_npc_portal_dialog(owner(), *npc);
#endif
        }

        void process(std::uint32_t key, std::size_t peer = 0) {
            const auto before = steady_ms_now();
#if defined(OPENKAL_ROUTING_BASELINE_BRIDGE)
            h4_baseline::process(clients[peer],
                                 clients,
                                 world,
                                 config,
                                 persistence.store,
                                 next_iid,
                                 fishing,
                                 engine,
                                 key);
#else
            process_session_quest(clients[peer],
                                  clients,
                                  world,
                                  config,
                                  persistence.store.get(),
                                  next_iid,
                                  fishing,
                                  *engine,
                                  key);
#endif
            const auto after = steady_ms_now();
            if (clients[peer].next_fishing_at_ms != 0 &&
                (clients[peer].gstate & protocol::kCgsFish) != 0 && fishing.ready()) {
                const auto delay = fishing_cycle_interval_ms(fishing, clients[peer].pay_buffs);
                expect(clients[peer].next_fishing_at_ms >= before + delay &&
                           clients[peer].next_fishing_at_ms <= after + delay,
                       "quest-driven fishing retains its own bounded real scheduling clock");
                fishing_deadline_bounded[peer] = true;
            }
        }

        bool validate_donation() {
#if defined(OPENKAL_ROUTING_BASELINE_BRIDGE)
            return h4_baseline::validate_donation(owner(), npcs, event);
#else
            return validate_exp_donation_dialog(owner(), npcs, event);
#endif
        }

        auto collect(std::uint32_t key, std::int64_t now_unix = 1000) {
#if defined(OPENKAL_ROUTING_BASELINE_BRIDGE)
            return h4_baseline::collect_donation(
                owner(), key, world, persistence.store, event, now_unix);
#else
            return collect_exp_donation(
                owner(), key, world, persistence.store.get(), event, now_unix);
#endif
        }

        bool cook(const std::vector<std::uint8_t>& payload) {
            const auto before = steady_ms_now();
#if defined(OPENKAL_ROUTING_BASELINE_BRIDGE)
            const bool end = h4_baseline::begin_cooking(owner(), payload, clients, npcs);
#else
            const bool end = begin_speed_medicine_request(owner(), payload, clients, npcs);
#endif
            const auto after = steady_ms_now();
            if (end) {
                expect(owner().cooking_complete_at_ms >= before + kSpeedMedicineDurationMs &&
                           owner().cooking_complete_at_ms <= after + kSpeedMedicineDurationMs,
                       "successful cooking starts only one bounded fresh real deadline");
                cooking_deadline_bounded = true;
            }
            return end;
        }

        void cancel(const std::vector<std::uint8_t>& /* ignored_payload */) {
#if defined(OPENKAL_ROUTING_BASELINE_BRIDGE)
            h4_baseline::cancel(owner(), clients);
#else
            cancel_npc_activity(owner(), clients);
#endif
        }

        void seed_cooking(const protocol::CookingOrder& order) {
            const auto cost = speed_medicine_cost(order);
            int iid = 400;
            int position = 0;
            for (auto input : {std::pair{kDemonBloodItemIndex, cost.demon_blood},
                               std::pair{kSeaBreamItemIndex, cost.sea_bream},
                               std::pair{kBambooKegItemIndex, cost.bamboo_keg},
                               std::pair{kBloodFishItemIndex, cost.blood_fish}}) {
                if (input.second > 0) {
                    bag(item(iid++, input.second, input.first), position++);
                }
            }
            owner().money = cost.geon;
            clear_audit();
        }
    };

    void observe_routing(std::string_view label,
                         RoutingPlayers& p,
                         const ScheduledPackets& packets,
                         int outcome) {
        if (!h4_snapshot) {
            return;
        }
        std::cout << "h4 " << label << " outcome=" << outcome << " iid=" << p.next_iid.load()
                  << '\n';
        for (std::size_t i = 0; i < p.clients.size(); ++i) {
            const auto& c = p.clients[i];
            std::cout << "peer " << i << " phase=" << static_cast<int>(c.phase)
                      << " dialog=" << c.dialog_npc_id
                      << " speed=" << c.active_speed_medicine_npc_id
                      << " craft=" << c.active_consumable_crafting_npc_id
                      << " exp=" << c.active_exp_event_npc_id << " shop=" << c.active_shop_npc_id
                      << " storage=" << c.active_storage_npc_id
                      << " pending=" << c.pending_npc_teleport_id << " tele=" << c.tele_pending
                      << ',' << c.tele_map << ',' << c.tele_x << ',' << c.tele_y << ',' << c.tele_z
                      << " armed=" << c.portal_armed << " beacon=" << c.beacon_active_until_ms
                      << " money=" << c.money << " hp=" << c.cur_hp << " gstate=" << c.gstate
                      << " cooking=" << c.cooking_order.blood_fish << ','
                      << c.cooking_order.medicine_from_blood_fish << ','
                      << c.cooking_order.medicine_direct << '\n';
            // Absolute fresh cooking/fishing deadlines are independently bracketed by the wrappers.
            std::cout << "deadline_kind="
                      << (p.cooking_deadline_bounded && i == 0 && c.cooking_complete_at_ms != 0
                              ? "fresh_cooking_bounds_checked"
                              : std::to_string(c.cooking_complete_at_ms))
                      << ','
                      << (p.fishing_deadline_bounded[i] && c.next_fishing_at_ms != 0
                              ? "fresh_fishing_bounds_checked"
                              : std::to_string(c.next_fishing_at_ms))
                      << '\n';
            std::cout << combat_inventory_state(c) << '\n';
            for (const auto& frame : packets[i]) {
                for (auto byte : frame) {
                    std::cout << std::hex << std::setw(2) << std::setfill('0')
                              << static_cast<int>(byte);
                }
                std::cout << std::dec << std::setfill(' ') << '\n';
            }
            std::vector<std::uint16_t> keys;
            for (const auto& entry : c.quests) {
                keys.push_back(entry.first);
            }
            std::sort(keys.begin(), keys.end());
            for (auto key : keys) {
                const auto& entry = c.quests.at(key);
                std::cout << "quest " << key << ',' << static_cast<int>(entry.flag) << ','
                          << static_cast<int>(entry.clear) << '\n';
            }
        }
        for (const auto& row : p.rows()) {
            std::cout << "item ";
            for (auto field : row) {
                std::cout << field << ',';
            }
            std::cout << '\n';
        }
        for (const auto& row : p.audit()) {
            std::cout << "audit " << row << '\n';
        }
        for (int id : {p.persistence.character_id, p.character_b, p.sibling}) {
            for (const auto& row : p.persistence.store->load_quest_flags(id)) {
                std::cout << "savedquest " << row.character_id << ',' << row.quest << ','
                          << static_cast<int>(row.flag) << ',' << static_cast<int>(row.clear)
                          << '\n';
            }
        }
        for (int id : {p.persistence.character_id, p.character_b, p.sibling}) {
            const auto row = p.persistence.store->load_character(id);
            if (row) {
                std::cout << "character " << storage_character_state(*row) << '\n';
            }
        }
        const auto event = p.persistence.store->load_server_event("exp_event");
        if (event) {
            std::cout << "event " << event->event_id << ',' << event->contribution_count << ','
                      << event->active_bonus_percent << ',' << event->active_until << '\n';
        }
    }

    void test_routing_dialog_contexts() {
        for (int length = 0; length < 4; ++length) {
            RoutingPlayers p;
            auto payload = storage_payload(protocol::Quest::frame_ask_npc(p.id(61008)));
            payload.resize(static_cast<std::size_t>(length));
            expect(!p.select(payload) && p.owner().dialog_npc_id == 123 &&
                       p.owner().active_speed_medicine_npc_id == 124 &&
                       p.owner().active_consumable_crafting_npc_id == 125 &&
                       p.owner().active_exp_event_npc_id == 126 &&
                       p.owner().pending_npc_teleport_id == p.id(61004),
                   "malformed Ask preserves all previous contexts and pending portal state");
            observe_routing("ask_short_" + std::to_string(length), p, p.drain(), 0);
        }
        for (int mode = 0; mode < 4; ++mode) {
            RoutingPlayers p;
            auto& c = p.owner();
            if (mode == 1) {
                c.map = 1;
            }
            if (mode >= 2) {
                c.px += protocol::kNpcReplyRange + (mode == 3 ? 1 : 0);
            }
            auto payload =
                storage_payload(protocol::Quest::frame_ask_npc(mode == 0 ? 999999 : p.id(61008)));
            const auto* npc = p.select(payload);
            expect((npc != nullptr) == (mode == 2), "NPC lookup retains exact-radius boundary");
            if (!npc) {
                expect(c.dialog_npc_id == 0 && c.active_speed_medicine_npc_id == 0 &&
                           c.active_consumable_crafting_npc_id == 0 &&
                           c.active_exp_event_npc_id == 0 && c.active_shop_npc_id == 127 &&
                           c.active_storage_npc_id == 128 &&
                           c.pending_npc_teleport_id == p.id(61004),
                       "invalid Ask resets only the four dialog authorization fields");
            }
            observe_routing("ask_gate_" + std::to_string(mode), p, p.drain(), npc != nullptr);
        }
        for (int index : {61001, 61008, 61009, 61010}) {
            RoutingPlayers p;
            auto payload = storage_payload(protocol::Quest::frame_ask_npc(p.id(index)));
            payload.push_back(0xaa);
            payload.push_back(0xbb);
            const auto* npc = p.select(payload);
            expect(npc && p.owner().dialog_npc_id == p.id(index) &&
                       p.owner().active_speed_medicine_npc_id ==
                           (index == 61008 || index == 61010 ? p.id(index) : 0) &&
                       p.owner().active_consumable_crafting_npc_id ==
                           (index == 61009 ? p.id(index) : 0) &&
                       p.owner().active_exp_event_npc_id == (index == 61010 ? p.id(index) : 0),
                   "trailing Ask is accepted and specialized contexts are assigned independently");
            observe_routing("ask_valid_" + std::to_string(index), p, p.drain(), npc != nullptr);
        }
    }

    void test_routing_disabled_manager() {
        RoutingPlayers p;
        p.event = ServerEventManager{};
        expect(p.select(61010) && p.owner().active_speed_medicine_npc_id == p.id(61010) &&
                   p.owner().active_exp_event_npc_id == 0,
               "disabled EXP event does not suppress the manager's independent speed dialog");
        observe_routing("ask_disabled_event", p, p.drain(), 1);
    }

    void test_routing_portal_boundary() {
        for (int mode = 0; mode < 5; ++mode) {
            RoutingPlayers p;
            auto& c = p.owner();
            c.pending_npc_teleport_id = 0;
            const auto* npc = p.select(mode == 2 ? 61005 : 61004);
            if (mode == 1) {
                c.sock.reset();
            }
            if (mode == 3) {
                c.pending_npc_teleport_id = npc->id;
                c.beacon_active_until_ms = 9000;
            }
            if (mode == 4) {
                c.pending_npc_teleport_id = npc->id;
                c.sock.reset();
            }
            expect(p.portal(npc), "one-destination kind5 consumes the portal route");
            const auto packets = p.drain();
            expect(
                c.tele_pending == (mode >= 3) &&
                    c.pending_npc_teleport_id == (mode == 0 ? npc->id : 0),
                "portal confirmation, missing HTML and missing transport retain old state rules");
            if (mode >= 3) {
                expect(
                    c.dialog_npc_id == 0 && !c.portal_armed && c.tele_z == 0 &&
                        c.beacon_active_until_ms == 0 && c.px == 1024 && c.py == 1024,
                    "confirmed portal stages authoritative destination without moving current pose");
            }
            observe_routing("portal_" + std::to_string(mode), p, packets, 1);
        }
        for (int index : {61001, 61006, 61007}) {
            RoutingPlayers p;
            const auto* npc = p.select(index);
            expect(!p.portal(npc) && p.owner().pending_npc_teleport_id == 0,
                   "nonportal or non-single destination falls through but clears confirmation");
            observe_routing("portal_fallthrough_" + std::to_string(index), p, p.drain(), 0);
        }
    }

    void test_routing_portal_confirmation_sequence() {
        RoutingPlayers p;
        p.owner().pending_npc_teleport_id = 0;
        const auto* first = p.select(61004);
        expect(first && p.portal(first) && !p.owner().tele_pending &&
                   p.owner().pending_npc_teleport_id == first->id,
               "first portal Ask arms only confirmation");
        auto malformed = storage_payload(protocol::Quest::frame_ask_npc(first->id));
        malformed.resize(3);
        expect(!p.select(malformed) && p.owner().pending_npc_teleport_id == first->id,
               "malformed intermediate Ask preserves the first portal confirmation");
        const auto* second = p.select(61004);
        expect(second && p.portal(second) && p.owner().tele_pending &&
                   p.owner().pending_npc_teleport_id == 0,
               "second portal Ask consumes the same confirmation");
        observe_routing("portal_two_step_after_malformed", p, p.drain(), 1);
    }

    void test_routing_quest_boundary() {
        for (int mode = 0; mode < 3; ++mode) {
            RoutingPlayers p;
            p.owner().dialog_npc_id = mode == 0 ? 0 : mode == 1 ? p.id(61002) : 999999;
            if (mode == 2) {
                p.persistence.sql("CREATE TRIGGER routing_fail BEFORE INSERT ON quest_flags BEGIN "
                                  "SELECT RAISE(ABORT,'owned quest save failure'); END;");
            }
            p.process(protocol::Quest::make_key(6201, 1));
            const auto packets = p.drain();
            expect(packets[0].size() == 2 &&
                       packets[0][0][2] == static_cast<std::uint8_t>(protocol::S2C::SendHtml) &&
                       packets[0][1][2] == static_cast<std::uint8_t>(protocol::S2C::SaveQuest) &&
                       p.owner().quests.at(6201).flag == 2 && packets[1].empty() &&
                       packets[2].empty(),
                   "quest processing preserves HTML-before-save and does not invent a dialog gate");
            observe_routing("quest_" + std::to_string(mode), p, packets, 1);
        }
        for (bool ready : {false, true}) {
            RoutingPlayers p;
            if (!ready) {
                p.fishing = FishingCatalog{};
            }
            p.owner().next_fishing_at_ms = 0;
            p.process(protocol::Quest::make_key(6203, 1));
            const auto packets = p.drain();
            expect((p.owner().gstate & protocol::kCgsFish) &&
                       (p.owner().next_fishing_at_ms != 0) == ready,
                   "quest fish action schedules only a ready catalog");
            observe_routing(ready ? "fish_ready" : "fish_unready", p, packets, 1);
        }
    }

    void test_routing_interleaved_quests() {
        RoutingPlayers p;
        p.owner().dialog_npc_id = p.id(61001);
        p.clients[1].dialog_npc_id = p.id(61002);
        p.process(protocol::Quest::make_key(6200, 1));
        p.process(protocol::Quest::make_key(6201, 1), 1);
        p.process(protocol::Quest::make_key(6202, 1));
        const auto packets = p.drain();
        expect(
            packets[0].size() == 4 && packets[1].size() == 2 && packets[2].empty() &&
                p.owner().quests.size() == 2 && p.clients[1].quests.size() == 1 &&
                p.persistence.store->load_quest_flags(p.persistence.character_id).size() == 2 &&
                p.persistence.store->load_quest_flags(p.character_b).size() == 1,
            "interleaved quest owners retain independent contexts, progress, wire and real records");
        observe_routing("interleaved_quests", p, packets, 1);
    }

    void test_routing_split_and_duplicate_donation() {
        RoutingPlayers p;
        p.bag(p.item(300, 50, 23016));
        p.bag(p.item(301, 51, 23016), 1);
        p.clear_audit();
        const auto first = p.collect(p.event.donate_hundred_quest_key());
        expect(first && first->amount == 100 && first->donation.activated &&
                   count_bag_item(p.owner(), 23016) == 1 && !find_bag_slot(p.owner(), 300) &&
                   find_bag_slot(p.owner(), 301) && find_bag_slot(p.owner(), 301)->count == 1,
               "exact100 donation preserves the remainder identity across split stacks");
        const auto second = p.collect(p.event.donate_hundred_quest_key());
        expect(!second && count_bag_item(p.owner(), 23016) == 1 &&
                   p.event.active_bonus_percent(1000) == 10,
               "repeated insufficient donation cannot consume the remaining coin or extend twice");
        const auto audit = p.audit();
        expect(audit == std::vector<std::string>{"D,300,50", "U,301,1", "E,0,10,4600"},
               "committed split coin removal precedes the contribution save");
        observe_routing("split_and_duplicate_donation", p, p.drain(), 1);
    }

    void test_routing_donation_boundaries() {
        for (int mode = 0; mode < 4; ++mode) {
            RoutingPlayers p;
            p.owner().active_exp_event_npc_id = mode == 0 ? 0 : p.id(mode == 1 ? 61001 : 61010);
            if (mode == 2) {
                p.owner().map = 1;
            }
            expect(p.validate_donation() == (mode == 3),
                   "donation manager identity/map are independent guards");
            const auto packets = p.drain();
            if (mode != 3) {
                expect(p.owner().active_exp_event_npc_id == 0 && packets[0].size() == 1 &&
                           p.audit().empty(),
                       "invalid donation context resets only EXP authorization and replies");
            }
            observe_routing("donation_context_" + std::to_string(mode), p, packets, mode == 3);
        }
        for (int available : {0, 99, 100, 101}) {
            for (bool hundred : {false, true}) {
                RoutingPlayers p;
                if (available > 0) {
                    p.bag(p.item(300, available, 23016));
                }
                p.clear_audit();
                const auto receipt = p.collect(hundred ? p.event.donate_hundred_quest_key()
                                                       : p.event.donate_quest_key());
                const int expected = hundred ? (available >= 100 ? 100 : 0) : available;
                expect(receipt.has_value() == (expected > 0) &&
                           count_bag_item(p.owner(), 23016) == available - expected,
                       "donation core retains donate-all versus exact100 semantics");
                if (receipt) {
                    expect(
                        receipt->amount == expected && receipt->donation.accepted,
                        "donation result exposes the exact consumed amount without performing announcements");
                }
                observe_routing("donation_" + std::to_string(available) +
                                    (hundred ? "_100" : "_all"),
                                p,
                                p.drain(),
                                receipt.has_value());
            }
        }
        for (bool deletion : {false, true}) {
            RoutingPlayers p;
            p.bag(p.item(300, deletion ? 100 : 101, 23016));
            p.persistence.sql(
                deletion
                    ? "CREATE TRIGGER routing_fail BEFORE DELETE ON items WHEN OLD.iid=300 BEGIN SELECT RAISE(ABORT,'owned coin delete failure'); END;"
                    : "CREATE TRIGGER routing_fail BEFORE UPDATE ON items WHEN NEW.iid=300 BEGIN SELECT RAISE(ABORT,'owned coin update failure'); END;");
            p.clear_audit();
            const auto receipt = p.collect(p.event.donate_hundred_quest_key());
            expect(receipt && receipt->donation.activated &&
                       count_bag_item(p.owner(), 23016) == (deletion ? 0 : 1),
                   "ignored coin-row failures still permit live removal and contribution");
            const auto durable = p.persistence.store->load_items(p.persistence.character_id);
            expect(durable.size() == 1 && durable[0].count == (deletion ? 100 : 101),
                   "ignored coin persistence leaves the old durable quantity");
            observe_routing(
                deletion ? "donation_ignored_delete" : "donation_ignored_update", p, p.drain(), 1);
        }
        {
            RoutingPlayers p;
            p.bag(p.item(300, 100, 23016));
            p.persistence.sql("CREATE TRIGGER routing_fail BEFORE UPDATE ON server_events BEGIN "
                              "SELECT RAISE(ABORT,'owned contribution save failure'); END;");
            p.clear_audit();
            const auto receipt = p.collect(p.event.donate_quest_key());
            const auto packets = p.drain();
            expect(
                !receipt && count_bag_item(p.owner(), 23016) == 0 &&
                    p.event.progress_items() == 0 && packets[0].size() == 1 && packets[1].empty() &&
                    packets[2].empty(),
                "failed contribution retains prior removal and only sends failure chat before bag diff");
            observe_routing("donation_failed_event_save", p, packets, 0);
        }
    }

    void test_routing_mirror_removal_compatibility() {
        for (bool whole : {false, true}) {
            RoutingPlayers p;
            ItemCombatDef mirror;
            mirror.index = 645;
            mirror.class_big = "general";
            mirror.class_small = "etc";
            mirror.stackable = true;
            GameWorldCoveragePeer::add_item(p.world, mirror);
            p.bag(p.item(300, whole ? 1 : 2, 645));
            p.persistence.sql(
                whole
                    ? "CREATE TRIGGER routing_fail BEFORE DELETE ON items WHEN OLD.iid=300 BEGIN SELECT RAISE(ABORT,'owned mirror delete failure'); END;"
                    : "CREATE TRIGGER routing_fail BEFORE UPDATE ON items WHEN NEW.iid=300 BEGIN SELECT RAISE(ABORT,'owned mirror update failure'); END;");
            p.clear_audit();
            const bool removed =
                try_remove_bag_persist(p.persistence.store.get(), p.world, p.owner(), 645, 1);
            const auto durable = p.persistence.store->load_items(p.persistence.character_id);
            expect(
                removed && count_bag_item(p.owner(), 645) == (whole ? 0 : 1) &&
                    durable.size() == 1 && durable[0].count == (whole ? 1 : 2) && p.audit().empty(),
                "existing mirror-removal helper reports live success despite ignored row failure");
            // This is a component witness, not a duplicate of the retained ritual coordinator.
            // Its unchanged following activation/packet order is source-proved separately.
            observe_routing(
                whole ? "mirror_ignored_delete" : "mirror_ignored_update", p, p.drain(), removed);
        }
    }

    void test_routing_cooking_boundaries() {
        const protocol::CookingOrder order{1, 1, 1};
        for (int mode = 0; mode < 21; ++mode) {
            RoutingPlayers p;
            p.seed_cooking(order);
            auto& c = p.owner();
            c.active_speed_medicine_npc_id = mode == 8 ? 0 : p.id(61008);
            c.cooking_complete_at_ms = 43;
            auto payload =
                storage_payload(protocol::Cooking::frame_c2s(protocol::kCookingPotions, order));
            if (mode < 7) {
                payload.resize(static_cast<std::size_t>(mode));
            } else if (mode == 7) {
                payload.push_back(1);
            } else if (mode == 9) {
                c.cur_hp = 0;
            } else if (mode == 10) {
                c.gstate = protocol::kCgsFish;
            } else if (mode == 11) {
                c.gstate = protocol::kCgsCooking;
            } else if (mode == 12) {
                c.active_speed_medicine_npc_id = p.id(61001);
            } else if (mode == 13) {
                --c.money;
            } else if (mode == 14) {
                payload[0] = 2;
            } else if (mode == 15) {
                payload =
                    storage_payload(protocol::Cooking::frame_c2s(protocol::kCookingPotions, {}));
            } else if (mode == 16) {
                c.active_speed_medicine_npc_id = 999999;
                c.cur_hp = 0;
            } else if (mode == 17) {
                c.active_speed_medicine_npc_id = 999999;
            } else if (mode == 18) {
                c.map = 1;
            } else if (mode == 19) {
                c.px += protocol::kNpcReplyRange + 1;
            } else if (mode == 20) {
                --c.bag[0].count;
            }
            const auto bag = combat_inventory_state(c);
            const auto active = c.active_speed_medicine_npc_id;
            expect(!p.cook(payload) && c.cooking_complete_at_ms == 43 &&
                       combat_inventory_state(c) == bag && p.audit().empty(),
                   "rejected cooking never starts/restarts a deadline or consumes materials");
            expect(c.active_speed_medicine_npc_id ==
                       (mode == 12 || mode == 17 || mode == 18 || mode == 19 ? 0 : active),
                   "only the later NPC validation gate clears chemist authorization");
            observe_routing("cooking_reject_" + std::to_string(mode), p, p.drain(), 0);
        }
        {
            RoutingPlayers p;
            p.seed_cooking(order);
            auto& c = p.owner();
            c.active_speed_medicine_npc_id = p.id(61008);
            c.tele_pending = true;
            c.gstate = 1u << 12;
            p.clients[2].map = 1;
            const auto bag = combat_inventory_state(c);
            const auto money = c.money;
            const auto payload =
                storage_payload(protocol::Cooking::frame_c2s(protocol::kCookingPotions, order));
            expect(
                p.cook(payload) && c.cooking_order.blood_fish == 1 &&
                    c.cooking_order.medicine_from_blood_fish == 1 &&
                    c.cooking_order.medicine_direct == 1 && (c.gstate & protocol::kCgsCooking) &&
                    combat_inventory_state(c) == bag && c.money == money && p.audit().empty(),
                "cooking accepts tele_pending, retains unrelated flags, and defers all payment until completion");
            const auto deadline = c.cooking_complete_at_ms;
            expect(!p.cook(payload) && c.cooking_complete_at_ms == deadline,
                   "repeated busy request cannot restart cooking");
            const auto packets = p.drain();
            expect(packets[0].size() == 1 && packets[1].size() == 1 && packets[2].size() == 1,
                   "cooking broadcasts to all Playing peers including another map");
            observe_routing("cooking_success_and_repeat", p, packets, 1);
        }
        for (int mode = 0; mode < 4; ++mode) {
            RoutingPlayers p;
            p.owner().cur_hp = 0;
            p.owner().gstate = protocol::kCgsFish | protocol::kCgsCooking | (1u << 12);
            p.clients[2].map = 1;
            if (mode == 1) {
                p.clients[1].phase = SessionPhase::WaitConnect;
            }
            if (mode == 2) {
                p.clients[1].sock.reset();
            }
            p.cancel(std::vector<std::uint8_t>(static_cast<std::size_t>(mode), 0xff));
            const auto packets = p.drain();
            expect(
                p.owner().gstate == (1u << 12) && p.owner().next_fishing_at_ms == 0 &&
                    p.owner().cooking_complete_at_ms == 0 &&
                    p.owner().cooking_order.blood_fish == 0 &&
                    p.owner().cooking_order.medicine_from_blood_fish == 0 &&
                    p.owner().cooking_order.medicine_direct == 0 && packets[0].size() == 1 &&
                    packets[1].size() == (mode == 1 || mode == 2 ? 0u : 1u) &&
                    packets[2].size() == 1,
                "quit ignores arbitrary payload and life/context, clears both jobs and retains broadcast audience");
            observe_routing("quit_" + std::to_string(mode), p, packets, 1);
        }
    }

    void test_routing_rng_sentinels() {
        for (int operation = 0; operation < 7; ++operation) {
            RoutingPlayers p;
            p.owner().pending_npc_teleport_id = 0;
            p.owner().active_exp_event_npc_id = p.id(61010);
            const auto* npc = p.npcs.find(p.id(61001));
            std::srand(883);
            const int expected = std::rand();
            std::srand(883);
            switch (operation) {
            case 0:
                (void) p.select(61001);
                break;
            case 1:
                (void) p.portal(npc);
                break;
            case 2:
                p.process(protocol::Quest::make_key(6201, 1));
                break;
            case 3:
                (void) p.validate_donation();
                break;
            case 4:
                (void) p.collect(p.event.donate_quest_key());
                break;
            case 5:
                (void) p.cook(
                    storage_payload(protocol::Cooking::frame_c2s(protocol::kCookingPotions, {})));
                break;
            case 6:
                p.cancel({});
                break;
            }
            const int actual = std::rand();
            expect(actual == expected,
                   "selected deterministic routing actions consume no hidden std::rand draw");
            observe_routing("rng_" + std::to_string(operation), p, p.drain(), actual);
        }
    }

    void test_routing_boundaries() {
        test_routing_dialog_contexts();
        test_routing_disabled_manager();
        test_routing_portal_boundary();
        test_routing_portal_confirmation_sequence();
        test_routing_quest_boundary();
        test_routing_interleaved_quests();
        test_routing_donation_boundaries();
        test_routing_split_and_duplicate_donation();
        test_routing_cooking_boundaries();
        test_routing_mirror_removal_compatibility();
        test_routing_rng_sentinels();
    }

    bool h3_snapshot = false;

    struct ShopPlayers : StoragePlayers {
        ShopCatalog shops;
        std::string shop_dir;
        bool expected_character_save_failure = false;

        ShopPlayers() {
            shop_dir = persistence.root + "/shop_definitions";
            test::mkdir_p(shop_dir);
            const auto define = [&](int index, bool stackable, int buy, int sell) {
                ItemCombatDef definition;
                definition.index = index;
                definition.name = "Owned shop item " + std::to_string(index);
                definition.class_big = "general";
                definition.class_small = "etc";
                definition.stackable = stackable;
                definition.buy = buy;
                definition.sell = sell;
                if (index == 902 || index == 907 || index == 908) {
                    definition.class_big = "weapon";
                    definition.class_small = "sword";
                    definition.wearable = true;
                    definition.is_weapon = true;
                    definition.endurance = 100;
                }
                if (index == 908) {
                    definition.class_limit = 1;
                }
                if (index == 903) {
                    definition.class_big = "quest";
                }
                if (index == 906) {
                    definition.pay = true;
                }
                if (index >= 25000 && index <= 25003) {
                    definition.skin_slot = protocol::SkinSlot::Weapon;
                    definition.class_limit = index == 25002 ? 1 : 0;
                    definition.full_body_skin = index == 25000;
                }
                GameWorldCoveragePeer::add_item(world, std::move(definition));
            };
            define(901, true, 7, 3);
            define(902, false, 11, 5);
            define(903, true, 13, 6);
            define(906, true, 17, 7);
            define(907, true, 19, 8);
            define(908, false, 23, 9);
            define(909, true, 29, 10);
            define(910, true, 0, 2);
            define(65536, false, 31, 11);
            define(23030, true, 0, 0);
            for (int index = 25000; index <= 25003; ++index) {
                define(index, false, 0, 0);
            }
            {
                std::ofstream file(shop_dir + "/ordinary.json");
                file
                    << R"({"schema":"openkal.shop/v1","npcIndex":60001,"items":[902, 901, 908, 65536, 907, 903, 906, 909]})";
            }
            {
                std::ofstream file(shop_dir + "/skin.json");
                file
                    << R"({"schema":"openkal.shop/v1","npcIndex":60002,"currencyItem":23030,"skinCatalog":true,"fullBodyPrice":4,"itemPrice":2,"excludeItems":[25003]})";
            }
            for (int npc : {60003, 60004}) {
                std::ofstream file(shop_dir + "/" + std::to_string(npc) + ".json");
                file << "{\"schema\":\"openkal.shop/v1\",\"npcIndex\":" << npc
                     << ",\"items\":[901]}";
            }
            expect(shops.load_dir(shop_dir, world.item_defs()),
                   "load owned ordinary and skin shops");
            for (std::size_t peer = 0; peer < 2; ++peer) {
                auto& c = clients[peer];
                c.character_id = peer == 0 ? persistence.character_id : character_b;
                c.account_id = peer == 0 ? account_a : account_b;
                c.px = c.py = 1024;
                c.map = 0;
                c.clazz = 0;
                c.cur_hp = 100;
                c.money = 10000;
                c.active_shop_npc_id = ordinary();
                c.last_autosave_ms = 23;
                auto saved = persistence.store->load_character(c.character_id);
                expect(saved.has_value(), "owned shop character exists");
                if (saved) {
                    saved->money = c.money;
                    expect(persistence.store->save_character(*saved), "seed owned shop balance");
                }
            }
            clear_audit();
        }

        std::uint32_t ordinary() const {
            return npc_ids.at(60001);
        }
        std::uint32_t skin() const {
            return npc_ids.at(60002);
        }

        bool list(const std::vector<std::uint8_t>& payload, std::size_t peer = 0) {
#if defined(OPENKAL_SHOP_BASELINE_BRIDGE)
            return h3_baseline::list(clients[peer], payload, world, npcs, shops);
#else
            return handle_shop_list_request(clients[peer], payload, world, npcs, shops);
#endif
        }

        bool buy(const std::vector<std::uint8_t>& payload, std::size_t peer = 0) {
            const auto previous = clients[peer].last_autosave_ms;
            const auto before = steady_ms_now();
#if defined(OPENKAL_SHOP_BASELINE_BRIDGE)
            const bool result = h3_baseline::buy(
                clients[peer], payload, world, npcs, shops, persistence.store, next_iid);
#else
            const bool result = handle_shop_buy_request(
                clients[peer], payload, world, npcs, shops, persistence.store.get(), next_iid);
#endif
            const auto after = steady_ms_now();
            const auto stamp = clients[peer].last_autosave_ms;
            const bool expected_save = result && !expected_character_save_failure;
            expect(expected_save ? (stamp >= before && stamp <= after) : stamp == previous,
                   "purchase autosave follows the expected character-save outcome");
            return result;
        }

        bool sell(const std::vector<std::uint8_t>& payload, std::size_t peer = 0) {
            const auto previous = clients[peer].last_autosave_ms;
            const auto before = steady_ms_now();
#if defined(OPENKAL_SHOP_BASELINE_BRIDGE)
            const bool result =
                h3_baseline::sell(clients[peer], payload, world, npcs, shops, persistence.store);
#else
            const bool result = handle_shop_sell_request(
                clients[peer], payload, world, npcs, shops, persistence.store.get());
#endif
            const auto after = steady_ms_now();
            const auto stamp = clients[peer].last_autosave_ms;
            const bool expected_save = result && !expected_character_save_failure;
            expect(expected_save ? (stamp >= before && stamp <= after) : stamp == previous,
                   "sale autosave follows the expected character-save outcome");
            return result;
        }

        bool buy(int index, int count = 1, std::uint32_t npc = 0, std::size_t peer = 0) {
            return buy(
                storage_payload(protocol::Shop::frame_buy_c2s(npc ? npc : ordinary(),
                                                              static_cast<std::uint16_t>(index),
                                                              static_cast<std::uint16_t>(count))),
                peer);
        }

        bool sell(std::uint32_t iid, int count = 1, std::uint32_t npc = 0, std::size_t peer = 0) {
            return sell(storage_payload(protocol::Shop::frame_sell_c2s(
                            npc ? npc : ordinary(), iid, static_cast<std::uint16_t>(count))),
                        peer);
        }

        ItemRecord unbound(int iid, int count = 10, int index = 901) const {
            auto row = item(iid, count, index);
            row.prefix = 0;
            row.account_bound = false;
            row.character_bound = false;
            return row;
        }
    };

    void observe_shop(std::string_view name,
                      ShopPlayers& players,
                      bool reached_end,
                      const ScheduledPackets& packets,
                      int next_random) {
        if (!h3_snapshot) {
            return;
        }
        std::cout << "h3 " << name << " reached_end=" << reached_end
                  << " global_next=" << players.next_iid.load() << " next_random=" << next_random
                  << '\n';
        for (std::size_t peer = 0; peer < players.clients.size(); ++peer) {
            const auto& c = players.clients[peer];
            std::cout << "peer " << peer << " active=" << c.active_shop_npc_id
                      << " money=" << c.money << " account=" << c.account_id
                      << " character=" << c.character_id << " session_next=" << c.next_iid
                      << " phase=" << static_cast<int>(c.phase) << " hp=" << c.cur_hp
                      << " teleport=" << c.tele_pending << " gstate=" << c.gstate << " autosave="
                      << (c.last_autosave_ms == 0 || c.last_autosave_ms == 23
                              ? std::to_string(c.last_autosave_ms)
                              : "fresh_clock_bounds_checked")
                      << '\n'
                      << combat_inventory_state(c) << '\n';
            for (const auto& frame : packets[peer]) {
                for (auto byte : frame) {
                    std::cout << std::hex << std::setw(2) << std::setfill('0')
                              << static_cast<int>(byte);
                }
                std::cout << std::dec << std::setfill(' ') << '\n';
            }
        }
        for (const auto& row : players.rows()) {
            std::cout << "row ";
            for (auto value : row) {
                std::cout << value << ',';
            }
            std::cout << '\n';
        }
        for (const auto& row : players.audit()) {
            std::cout << "audit " << row << '\n';
        }
        for (int id : {players.persistence.character_id, players.character_b, players.sibling}) {
            const auto row = players.persistence.store->load_character(id);
            if (row) {
                std::cout << "character " << storage_character_state(*row) << '\n';
            }
        }
    }

    bool shop_money_last(const ScheduledPackets& packets, std::int32_t expected) {
        if (packets[0].empty()) {
            return false;
        }
        const auto& frame = packets[0].back();
        if (frame.size() < 3 || frame[2] != static_cast<std::uint8_t>(protocol::S2C::InsertItem)) {
            return false;
        }
        const auto payload = storage_payload(frame);
        protocol::PacketReader reader(payload);
        std::uint32_t iid = 1, info = 0;
        std::uint16_t index = 0;
        std::uint8_t prefix = 0;
        std::int32_t count = 0;
        return reader.read_u32(iid) && reader.read_u16(index) && reader.read_u8(prefix) &&
               reader.read_u32(info) && reader.read_i32(count) && iid == 0 &&
               index == kMoneyItemIndex && (info & protocol::kInsertReplace) && count == expected;
    }

    void test_shop_list_requests() {
        for (bool skin : {false, true}) {
            ShopPlayers p;
            const bool end = p.list(storage_payload(
                protocol::Shop::frame_item_list_c2s(skin ? p.skin() : p.ordinary())));
            const auto packets = p.drain();
            protocol::ShopItemList list;
            expect(end && packets[0].size() == 1 && packets[1].empty() && packets[2].empty(),
                   "shop list falls through and replies only to its owner");
            if (packets[0].size() == 1) {
                const auto payload = storage_payload(packets[0].front());
                expect(protocol::Shop::parse_item_list_s2c(payload.data(), payload.size(), list),
                       "parse owned shop list");
                std::vector<int> actual;
                std::vector<std::uint32_t> prices;
                for (const auto& offer : list.offers) {
                    actual.push_back(offer.index);
                    prices.push_back(offer.buy);
                }
                expect(actual == (skin ? std::vector<int>{25000, 25001}
                                       : std::vector<int>{902, 901, 908, 907, 903, 906, 909}) &&
                           list.currency_index == (skin ? 23030 : 0),
                       "ordinary configured order and skin sorted own-class order "
                       "survive wire filtering");
                expect(prices == (skin ? std::vector<std::uint32_t>{4, 2}
                                       : std::vector<std::uint32_t>{11, 7, 23, 19, 13, 17, 29}),
                       "shop prices retain configured offer order and currency semantics");
            }
            observe_shop(skin ? "list_skin" : "list_ordinary", p, end, packets, -1);
        }
        for (int mode = 0; mode < 7; ++mode) {
            ShopPlayers p;
            auto payload = storage_payload(
                protocol::Shop::frame_item_list_c2s(mode == 2   ? 999999
                                                    : mode == 3 ? p.npc_ids.at(60003)
                                                    : mode == 4 ? p.npc_ids.at(60004)
                                                                : p.ordinary()));
            if (mode == 0) {
                payload.resize(3);
            }
            if (mode == 1) {
                payload.push_back(1);
            }
            if (mode >= 5) {
                p.owner().px += protocol::kNpcReplyRange + (mode == 6 ? 1 : 0);
            }
            const bool end = p.list(payload);
            const auto packets = p.drain();
            expect(end == (mode == 5) &&
                       p.owner().active_shop_npc_id == (mode < 2 || mode == 5 ? p.ordinary() : 0),
                   "malformed list preserves activation while invalid NPC/map/range "
                   "clears it");
            expect(packets[0].size() == (mode == 5 ? 1u : 0u),
                   "only exact-radius valid shop lists reply");
            observe_shop("list_gate_" + std::to_string(mode), p, end, packets, -1);
        }
        {
            ShopPlayers p;
            auto definition = p.world.item_defs().at(901);
            definition.buy = 0;
            GameWorldCoveragePeer::add_item(p.world, definition);
            const bool end =
                p.list(storage_payload(protocol::Shop::frame_item_list_c2s(p.ordinary())));
            const auto packets = p.drain();
            expect(end && packets[0].size() == 1,
                   "live nonpositive price still produces a filtered list");
            if (packets[0].size() == 1) {
                const auto payload = storage_payload(packets[0].front());
                protocol::ShopItemList list;
                expect(protocol::Shop::parse_item_list_s2c(payload.data(), payload.size(), list) &&
                           std::none_of(list.offers.begin(),
                                        list.offers.end(),
                                        [](const auto& offer) { return offer.index == 901; }),
                       "list consults the current definition price rather than a cached offer");
            }
            observe_shop("list_live_price_changed", p, end, packets, -1);
        }
        {
            ShopPlayers p;
            p.owner().clazz = 3;
            const bool end = p.list(storage_payload(protocol::Shop::frame_item_list_c2s(p.skin())));
            const auto packets = p.drain();
            protocol::ShopItemList list;
            const auto payload = packets[0].size() == 1 ? storage_payload(packets[0].front())
                                                        : std::vector<std::uint8_t>{};
            expect(end && p.owner().active_shop_npc_id == p.skin() &&
                       protocol::Shop::parse_item_list_s2c(payload.data(), payload.size(), list) &&
                       list.offers.empty(),
                   "valid skin shop activates and replies when every offer is class-filtered");
            observe_shop("list_empty_own_class", p, end, packets, -1);
        }
        {
            ShopPlayers p;
            p.shops = ShopCatalog{};
            const bool end =
                p.list(storage_payload(protocol::Shop::frame_item_list_c2s(p.ordinary())));
            const auto packets = p.drain();
            expect(!end && p.owner().active_shop_npc_id == 0 && packets == ScheduledPackets{},
                   "live NPC with no current shop clears shop authorization");
            observe_shop("list_missing_shop", p, end, packets, -1);
        }
    }

    void test_shop_buy_requests() {
        for (int gate = 0; gate < 10; ++gate) {
            ShopPlayers p;
            if (gate == 0) {
                p.owner().active_shop_npc_id = 0;
            }
            if (gate == 4) {
                p.owner().px += protocol::kNpcReplyRange + 1;
            }
            if (gate == 5) {
                p.owner().map = 1;
            }
            if (gate == 6) {
                p.owner().money = 6;
            }
            auto payload = storage_payload(protocol::Shop::frame_buy_c2s(p.ordinary(),
                                                                         gate == 3 ? 999 : 901,
                                                                         gate == 1   ? 0
                                                                         : gate == 2 ? 10001
                                                                                     : 1));
            if (gate == 7) {
                payload.pop_back();
            }
            if (gate == 8) {
                payload.push_back(1);
            }
            if (gate == 9) {
                auto definition = p.world.item_defs().at(901);
                definition.buy = 0;
                GameWorldCoveragePeer::add_item(p.world, definition);
            }
            const auto before = combat_inventory_state(p.owner());
            const auto money = p.owner().money;
            std::srand(71);
            const int random = std::rand();
            std::srand(71);
            const bool end = p.buy(payload);
            const int after_random = std::rand();
            const auto packets = p.drain();
            expect(!end && combat_inventory_state(p.owner()) == before &&
                       p.owner().money == money && p.next_iid.load() == 90000 &&
                       p.audit().empty() && after_random == random,
                   "rejected ordinary purchase preserves bag, money, IID, RNG and "
                   "durable rows");
            expect(packets[0].size() == (gate == 6 ? 1u : 0u) && packets[1].empty() &&
                       packets[2].empty(),
                   "only insufficient money emits the existing ordinary-buy chat");
            observe_shop("buy_gate_" + std::to_string(gate), p, end, packets, after_random);
        }
        for (int mode = 0; mode < 7; ++mode) {
            ShopPlayers p;
            auto& c = p.owner();
            const int index = mode == 2 ? 902 : mode == 3 ? 907 : mode == 4 ? 908 : 901;
            if (mode <= 1) {
                auto row = p.unbound(100, 4);
                if (mode == 0) {
                    row = {};
                    row.iid = 100;
                    row.character_id = p.persistence.character_id;
                    row.account_id = p.account_a;
                    row.index = 901;
                    row.count = 4;
                }
                if (mode == 1) {
                    row.character_bound = true;
                }
                p.bag(row);
            }
            if (mode == 4) {
                c.cur_hp = 0;
                c.tele_pending = true;
                c.gstate = protocol::kCgsFish | protocol::kCgsCooking;
            }
            int quantity = mode == 2 || mode == 3 ? 2 : 3;
            if (mode == 5) {
                quantity = 10000;
                c.money = 70000;
            }
            if (mode == 6) {
                auto definition = p.world.item_defs().at(901);
                definition.buy = std::numeric_limits<int>::max();
                GameWorldCoveragePeer::add_item(p.world, definition);
                quantity = 10000;
                c.money = static_cast<std::int64_t>(std::numeric_limits<int>::max()) * quantity;
            }
            const auto money = c.money;
            p.clear_audit();
            std::srand(73);
            const bool end = p.buy(index, quantity);
            const int random = std::rand();
            const auto packets = p.drain();
            int total = 0;
            int matching = 0;
            for (const auto& row : c.bag) {
                if (row.iid && row.index == index && !row.character_bound) {
                    total += row.count;
                    ++matching;
                }
            }
            const int expected = quantity + (mode == 0 ? 4 : 0);
            expect(end && total == expected &&
                       matching == ((mode == 2 || mode == 3 || mode == 4) ? quantity : 1) &&
                       c.money ==
                           money - static_cast<std::int64_t>(p.world.item_defs().at(index).buy) *
                                       quantity,
                   "ordinary buy preserves merge, wearable, class/activity and typed "
                   "price rules");
            expect(packets[0].size() == static_cast<std::size_t>(matching + 1) &&
                       packets[1].empty() && packets[2].empty() &&
                       shop_money_last(packets, static_cast<std::int32_t>(c.money)),
                   "ordinary buy emits ordered item updates then money without "
                   "observer output");
            observe_shop("buy_success_" + std::to_string(mode), p, end, packets, random);
        }
        for (int free = 0; free <= 2; ++free) {
            ShopPlayers p;
            for (int i = 0; i < unlocked_bag_slot_count(p.owner()) - free; ++i) {
                p.bag(p.unbound(100 + i, 1, 902), i);
            }
            const auto before = combat_inventory_state(p.owner());
            p.clear_audit();
            const bool end = p.buy(902, 2);
            const auto packets = p.drain();
            expect(end == (free == 2) &&
                       (free == 2 ||
                        (before == combat_inventory_state(p.owner()) && p.audit().empty())),
                   "nonstack purchase prechecks whole capacity and never inserts a "
                   "partial in-memory basket");
            observe_shop("buy_capacity_" + std::to_string(free), p, end, packets, -1);
        }
        for (int fault = 0; fault < 3; ++fault) {
            ShopPlayers p;
            if (fault == 0) {
                p.persistence.sql(
                    "CREATE TRIGGER shop_fail BEFORE INSERT ON items WHEN NEW.iid=90002 "
                    "BEGIN SELECT RAISE(ABORT,'owned shop insert failure'); END;");
            }
            if (fault == 1) {
                ItemRecord row;
                row.iid = 100;
                row.character_id = p.persistence.character_id;
                row.account_id = p.account_a;
                row.index = 901;
                row.count = 4;
                p.bag(row);
                p.persistence.sql(
                    "CREATE TRIGGER shop_fail BEFORE INSERT ON items WHEN NEW.iid=100 "
                    "BEGIN SELECT RAISE(ABORT,'owned shop merge failure'); END;");
            }
            if (fault == 2) {
                p.expected_character_save_failure = true;
                p.persistence.sql(
                    "CREATE TRIGGER shop_fail BEFORE UPDATE ON characters BEGIN SELECT "
                    "RAISE(ABORT,'owned shop character failure'); END;");
            }
            p.clear_audit();
            std::srand(83);
            const bool end = p.buy(fault == 1 ? 901 : 902, 3);
            const int random = std::rand();
            const auto packets = p.drain();
            const auto rows = p.persistence.store->load_items(p.persistence.character_id);
            expect(end == (fault == 2) && p.owner().money == (fault == 2 ? 9967 : 10000),
                   "failed item writes roll back the entire purchase before money is charged");
            if (fault < 2) {
                expect(packets[0].size() == 1 && packets[1].empty() && packets[2].empty() &&
                           !shop_money_last(packets, 10000),
                       "rolled-back purchases emit no item or money success frames");
            } else {
                expect(shop_money_last(packets, 9967),
                       "character-save failure retains the existing charged live response");
            }
            if (fault == 0) {
                expect(rows.empty() && p.next_iid.load() == 90003 &&
                           std::all_of(p.owner().bag.begin(),
                                       p.owner().bag.end(),
                                       [](const auto& row) { return row.iid == 0; }),
                       "failed middle insert leaves no durable or live partial purchase");
            }
            if (fault == 1) {
                expect(rows.size() == 1 && rows[0].count == 4 && p.owner().bag[0].count == 4,
                       "failed merge preserves both durable and live destination counts");
            }
            if (fault == 2) {
                const auto saved = p.persistence.store->load_character(p.persistence.character_id);
                expect(saved && saved->money == 10000 && p.owner().last_autosave_ms == 23,
                       "ignored character save failure preserves old durable money and "
                       "deadline");
            }
            observe_shop("buy_ignored_failure_" + std::to_string(fault), p, end, packets, random);
        }
    }

    void test_shop_sell_requests() {
        for (int length = 0; length <= 10; ++length) {
            ShopPlayers p;
            p.bag(p.unbound(100, 10));
            auto payload = storage_payload(protocol::Shop::frame_sell_c2s(p.ordinary(), 100, 1));
            if (length < 10) {
                payload.resize(static_cast<std::size_t>(length));
            } else {
                payload.push_back(1);
            }
            const auto before = combat_inventory_state(p.owner());
            p.clear_audit();
            const bool end = p.sell(payload);
            const auto packets = p.drain();
            expect(!end && before == combat_inventory_state(p.owner()) &&
                       p.owner().money == 10000 && p.audit().empty() &&
                       packets == ScheduledPackets{},
                   "sale parser rejects every truncation and trailing byte without effects");
            observe_shop("sell_malformed_" + std::to_string(length), p, end, packets, -1);
        }
        for (int gate = 0; gate < 12; ++gate) {
            ShopPlayers p;
            auto item = p.unbound(100, 10);
            if (gate == 0) {
                p.owner().active_shop_npc_id = 0;
            }
            if (gate == 3) {
                item.character_bound = item.account_bound = true;
            }
            if (gate == 4) {
                item.account_bound = true;
            }
            if (gate == 5) {
                item.index = 903;
            }
            if (gate == 6) {
                item.index = 906;
            }
            if (gate == 7) {
                item.index = 777;
            }
            p.bag(item);
            if (gate == 8) {
                p.owner().bag[0].equipped = true;
            }
            if (gate == 9) {
                p.owner().px += protocol::kNpcReplyRange + 1;
            }
            if (gate == 10) {
                p.owner().money = std::numeric_limits<std::int64_t>::max() - 2;
            }
            auto payload =
                storage_payload(protocol::Shop::frame_sell_c2s(gate == 11 ? p.skin() : p.ordinary(),
                                                               100,
                                                               gate == 1   ? 0
                                                               : gate == 2 ? 11
                                                                           : 1));
            if (gate == 11) {
                p.owner().active_shop_npc_id = p.skin();
            }
            const auto before = combat_inventory_state(p.owner());
            const auto money = p.owner().money;
            p.clear_audit();
            std::srand(79);
            const int random = std::rand();
            std::srand(79);
            const bool end = p.sell(payload);
            const int after_random = std::rand();
            const auto packets = p.drain();
            expect(!end && before == combat_inventory_state(p.owner()) &&
                       money == p.owner().money && p.audit().empty() && after_random == random,
                   "rejected sales preserve inventory, money, persistence and RNG");
            expect(packets[0].size() == (gate == 3 || gate == 4 ? 1u : 0u),
                   "binding rejection keeps its current character-before-account "
                   "message precedence");
            observe_shop("sell_gate_" + std::to_string(gate), p, end, packets, after_random);
        }
        for (int mode = 0; mode < 7; ++mode) {
            ShopPlayers p;
            auto item = p.unbound(100, mode == 2 ? 10001 : 10, mode == 6 ? 910 : 901);
            p.bag(item);
            p.bag(p.unbound(101, 1, 902), 1);
            const int quantity = mode == 0 || mode == 3 ? 3 : item.count;
            if (mode == 3) {
                p.persistence.sql(
                    "CREATE TRIGGER shop_fail BEFORE UPDATE ON items WHEN NEW.iid=100 "
                    "BEGIN SELECT RAISE(ABORT,'owned shop partial sale failure'); END;");
            }
            if (mode == 4) {
                p.persistence.sql(
                    "CREATE TRIGGER shop_fail BEFORE DELETE ON items WHEN OLD.iid=100 "
                    "BEGIN SELECT RAISE(ABORT,'owned shop full sale failure'); END;");
            }
            if (mode == 5) {
                p.expected_character_save_failure = true;
                p.persistence.sql(
                    "CREATE TRIGGER shop_fail BEFORE UPDATE ON characters BEGIN SELECT "
                    "RAISE(ABORT,'owned shop sale character failure'); END;");
            }
            const auto before = storage_values(item);
            p.clear_audit();
            std::srand(89);
            const bool end = p.sell(100, quantity);
            const int random = std::rand();
            const auto packets = p.drain();
            expect(end &&
                       p.owner().money ==
                           10000 + static_cast<std::int64_t>(quantity) * (mode == 6 ? 2 : 3) &&
                       find_bag_slot(p.owner(), 101),
                   "ordinary sales retain later cells and award exact typed value "
                   "across ignored failures");
            const auto* remaining = find_bag_slot(p.owner(), 100);
            expect((quantity == item.count)
                       ? !remaining
                       : (remaining && remaining->count == item.count - quantity),
                   "partial/full sale mutates only the sold IID count before compaction");
            if (remaining) {
                ItemRecord row;
                fill_item_record_from_slot(row, p.owner(), *remaining, 0);
                auto actual = storage_values(row);
                actual[5] = before[5];
                expect(actual == before,
                       "partial sale preserves every other persisted instance field");
            }
            const auto durable = p.persistence.store->load_items(p.persistence.character_id);
            const auto found = std::find_if(
                durable.begin(), durable.end(), [](const auto& row) { return row.iid == 100; });
            if (mode == 3 || mode == 4) {
                expect(found != durable.end() && found->count == item.count,
                       "ignored item write failure leaves the old durable sale quantity");
            }
            expect(packets[0].size() == 2 && packets[1].empty() && packets[2].empty(),
                   "sale emits source update/removal then money only to its owner");
            expect(shop_money_last(packets, static_cast<std::int32_t>(p.owner().money)),
                   "sale wire reports the exact small fixture balance after source output");
            observe_shop(
                "sell_success_or_failure_" + std::to_string(mode), p, end, packets, random);
        }
        {
            ShopPlayers p;
            p.bag(p.unbound(100, 1));
            p.owner().money = std::numeric_limits<std::int64_t>::max() - 3;
            p.clear_audit();
            const bool end = p.sell(100);
            const auto packets = p.drain();
            expect(end && p.owner().money == std::numeric_limits<std::int64_t>::max() &&
                       shop_money_last(packets, 2000000000),
                   "sale at exact int64 addition limit succeeds");
            observe_shop("sell_exact_int64_limit", p, end, packets, -1);
        }
    }

    void test_skin_shop_requests() {
        for (int mode = 0; mode < 9; ++mode) {
            ShopPlayers p;
            auto tickets = p.unbound(100, 8, 23030);
            p.bag(tickets);
            auto& c = p.owner();
            c.active_shop_npc_id = p.skin();
            if (mode == 0) {
                c.cur_hp = 0;
            }
            if (mode == 1) {
                c.tele_pending = true;
            }
            if (mode == 2) {
                c.gstate = protocol::kCgsFish;
            }
            if (mode == 3) {
                c.gstate = protocol::kCgsCooking;
            }
            if (mode == 7) {
                p.persistence.sql("CREATE TRIGGER shop_fail BEFORE INSERT ON items WHEN "
                                  "NEW.item_index=25000 BEGIN SELECT RAISE(ABORT,'owned "
                                  "skin exchange failure'); END;");
            }
            if (mode == 8) {
                c.bag[0].character_bound = true;
                ItemRecord bound;
                fill_item_record_from_slot(bound, c, c.bag[0], 0);
                expect(p.persistence.store->update_item(bound), "persist bound exchange tickets");
            }
            p.clear_audit();
            const auto before = combat_inventory_state(c);
            const auto money = c.money;
            const bool end = p.buy(mode == 4   ? 25002
                                   : mode == 5 ? 25003
                                               : 25000,
                                   mode == 6 ? 2 : 1,
                                   p.skin());
            const auto packets = p.drain();
            expect(!end && c.money == money,
                   "every skin route preserves frame continue and never charges "
                   "ordinary money");
            if (mode < 7) {
                expect(before == combat_inventory_state(c) && p.audit().empty() &&
                           packets == ScheduledPackets{},
                       "skin-only gates reject without consuming tickets or emitting "
                       "ordinary success");
            }
            if (mode == 7) {
                expect(before == combat_inventory_state(c) && packets[0].size() == 1,
                       "failed skin transaction restores tickets and emits only existing "
                       "failure chat");
            }
            if (mode == 8) {
                int bound_outputs = 0;
                for (const auto& row : c.bag) {
                    bound_outputs += row.iid && row.index == 25000 && row.character_bound;
                }
                expect(bound_outputs == 1 && count_bag_item(c, 23030) == 4,
                       "successful skin exchange propagates consumed ticket binding");
            }
            observe_shop("skin_gate_or_exchange_" + std::to_string(mode), p, end, packets, -1);
        }
    }

    void test_shop_requests() {
        test_shop_list_requests();
        test_shop_buy_requests();
        test_shop_sell_requests();
        test_skin_shop_requests();
        ShopPlayers p;
        p.clear_audit();
        const bool first = p.buy(902, 1, 0, 0);
        const bool second = p.buy(902, 1, 0, 1);
        const bool third = p.buy(902, 1, 0, 0);
        const auto packets = p.drain();
        expect(first && second && third && p.next_iid.load() == 90003 && packets[0].size() == 4 &&
                   packets[1].size() == 2 && packets[2].empty(),
               "interleaved owners retain independent money/output and shared FIFO "
               "IID allocation");
        observe_shop("interleaved_buyers", p, third, packets, -1);
    }

    void test_equipment_calculator() {
        GameWorld world;
        ItemCombatDef weapon;
        weapon.index = 1;
        weapon.is_weapon = true;
        weapon.class_big = "weapon";
        weapon.class_small = "sword2h";
        weapon.min_attack = 100;
        weapon.max_attack = 200;
        weapon.min_magic = 50;
        weapon.max_magic = 80;
        weapon.range = 48;
        weapon.attack_speed_ms = 750;
        GameWorldCoveragePeer::add_item(world, weapon);
        ItemCombatDef armor;
        armor.index = 2;
        armor.is_defense = true;
        armor.class_big = "defense";
        armor.class_small = "shield";
        armor.defense_melee = 100;
        armor.defense_range = 200;
        armor.hp = 10;
        armor.mp = 20;
        GameWorldCoveragePeer::add_item(world, armor);
        auto make_session = [] {
            ClientSession c;
            c.clazz = combat::kPcThief;
            c.level = 55;
            c.stats_initialized = true;
            for (int slot = 0; slot < 2; ++slot) {
                auto& item = c.bag[static_cast<std::size_t>(slot)];
                item.iid = static_cast<std::uint32_t>(slot + 1);
                item.index = slot + 1;
                item.count = 1;
                item.equipped = true;
            }
            c.weapon_index = 1;
            c.armor_index = 2;
            c.weapon_iid = 1;
            return c;
        };
        bool random_stats_ok = true;
        for (int type = 0; type <= 20; ++type) {
            for (int magnitude : {-1, 0, 1, 5, 6}) {
                auto c = make_session();
                auto& modified = c.bag[type >= 7 ? 1 : 0];
                modified.random_stat_type = type;
                modified.random_stat_value = magnitude;
                recompute_combat(c, world);
                const int value = std::clamp(magnitude, 0, 5);
                combat::BaseStats stats;
                stats.str = c.str;
                stats.hth = c.hth;
                stats.intl = c.intl;
                stats.wis = c.wis;
                stats.dex = c.dex;
                if (type == 7 || type == 14) {
                    stats.str += value;
                }
                if (type == 8 || type == 15) {
                    stats.hth += value;
                }
                if (type == 9 || type == 16) {
                    stats.intl += value;
                }
                if (type == 10 || type == 17) {
                    stats.wis += value;
                }
                if (type == 11 || type == 18) {
                    stats.dex += value;
                }
                std::int32_t min_a = 0, max_a = 0, min_m = 0, max_m = 0, hit = 0, dodge = 0;
                std::int32_t fire = 0, ice = 0, lightning = 0, curse = 0, palsy = 0;
                combat::apply_stat_revise(c.level,
                                          stats,
                                          min_a,
                                          max_a,
                                          hit,
                                          dodge,
                                          min_m,
                                          max_m,
                                          fire,
                                          ice,
                                          lightning,
                                          curse,
                                          palsy);
                min_a += 100;
                max_a += 200;
                min_m += 50;
                max_m += 80;
                const auto percent = [value](int n) { return n + n * value / 100; };
                const int wmin_a = type == 1 || type == 5 ? percent(100) : 100;
                const int wmax_a = type == 3 || type == 5 ? percent(200) : 200;
                const int wmin_m = type == 2 || type == 6 ? percent(50) : 50;
                const int wmax_m = type == 4 || type == 6 ? percent(80) : 80;
                if (type == 1 || type == 5) {
                    min_a = percent(min_a);
                }
                if (type == 3 || type == 5) {
                    max_a = percent(max_a);
                }
                if (type == 2 || type == 6) {
                    min_m = percent(min_m);
                }
                if (type == 4 || type == 6) {
                    max_m = percent(max_m);
                }
                int melee = 100 + (type == 12 || type == 19 ? value : 0);
                int ranged = 200 + (type == 12 || type == 19 ? value : 0);
                if (type >= 13 && type <= 19) {
                    melee = percent(melee);
                    ranged = percent(ranged);
                }
                random_stats_ok =
                    random_stats_ok && c.min_attack == min_a && c.max_attack == max_a &&
                    c.min_magic == min_m && c.max_magic == max_m && c.hit == hit &&
                    c.dodge == dodge && c.res_fire == fire && c.res_ice == ice &&
                    c.res_lightning == lightning && c.res_curse == curse && c.res_palsy == palsy &&
                    c.weapon_min_attack == wmin_a && c.weapon_max_attack == wmax_a &&
                    c.weapon_min_magic == wmin_m && c.weapon_max_magic == wmax_m &&
                    c.defense == melee && c.defense_range == ranged && c.wearing_shield &&
                    c.wearing_two_hand && c.attack_range == 48 && c.attack_speed_ms == 750 &&
                    c.max_hp == combat::max_hp_from_stats(stats.hth, c.level, c.clazz) + 10 &&
                    c.max_mp == combat::max_mp_from_stats(stats.wis, c.level, c.clazz) + 20;
            }
        }
        expect(random_stats_ok,
               "all random equipment modifiers match independent stat/percent oracles at clamp "
               "boundaries");

        auto plain = make_session();
        recompute_combat(plain, world);
        const auto plain_values = combat_values(session_combatant(plain, true, false));
        bool skipped_slots_ok = true;
        for (int reason = 0; reason < 6; ++reason) {
            auto c = make_session();
            auto& extra = c.bag[2];
            extra.equipped = reason != 0;
            extra.count = reason == 1 ? 0 : 1;
            extra.index = reason == 2   ? 0
                          : reason == 3 ? 999
                          : reason == 4 ? kYinYangMirrorFirst
                                        : 3;
            if (reason == 4) {
                ItemCombatDef collection;
                collection.index = kYinYangMirrorFirst;
                GameWorldCoveragePeer::add_item(world, collection);
            }
            if (reason == 5) {
                ItemCombatDef skin;
                skin.index = 3;
                skin.class_big = "skin";
                skin.min_attack = 9999;
                GameWorldCoveragePeer::add_item(world, skin);
            }
            recompute_combat(c, world);
            skipped_slots_ok = skipped_slots_ok &&
                               combat_values(session_combatant(c, true, false)) == plain_values;
        }
        expect(skipped_slots_ok,
               "unequipped/empty/invalid/unknown/collection/cosmetic slots cannot contribute "
               "ordinary gear stats");

        for (int type : {0, 1, 2, 3, 8, 9, 10, 11}) {
            for (int level : {0, 1, 2, 3}) {
                auto c = make_session();
                c.bag[0].mix_type = type | (level << 4);
                recompute_combat(c, world);
                const int hp_flat = type == 1 && level == 1    ? 20
                                    : type == 1 && level == 2  ? 50
                                    : type == 11 && level != 0 ? 50
                                                               : 0;
                const int mp_flat = type == 2 && level == 1    ? 20
                                    : type == 2 && level == 2  ? 50
                                    : type == 11 && level != 0 ? 50
                                                               : 0;
                int str = plain.str, hth = plain.hth, intl = plain.intl, wis = plain.wis,
                    dex = plain.dex;
                if (type == 1 && (level == 1 || level == 2)) {
                    str += level + 1;
                    intl += level + 1;
                    dex += level + 1;
                }
                if (type == 2 && (level == 1 || level == 2)) {
                    wis += level == 1 ? 2 : 5;
                    hth += level == 1 ? 2 : 5;
                }
                if (type == 11 && level != 0) {
                    str += 6;
                    intl += 6;
                    dex += 6;
                    hth += 10;
                    wis += 10;
                }
                int hp = combat::max_hp_from_stats(hth, c.level, c.clazz) + 10 + hp_flat;
                int mp = combat::max_mp_from_stats(wis, c.level, c.clazz) + 20 + mp_flat;
                if ((type == 8 || type == 9) && (level == 1 || level == 2)) {
                    hp += hp * (type == 8 ? level * 10 : level * -5) / 100;
                    mp += mp * (type == 8 ? level * -5 : level * 10) / 100;
                }
                expect(c.max_hp == hp && c.max_mp == mp && c.prefix_bonus.str == str - c.str &&
                           c.prefix_bonus.hth == hth - c.hth &&
                           c.prefix_bonus.intl == intl - c.intl &&
                           c.prefix_bonus.wis == wis - c.wis && c.prefix_bonus.dex == dex - c.dex &&
                           c.weapon_min_attack == 100 + (type == 10 && level != 0 ? 50 : 0),
                       "mix tiers preserve exact primary, HP/MP and fantasy weapon contributions");
            }
        }
        for (int prefix : {-1, 0, 999, 1}) {
            PrefixCombatDef def;
            def.index = 1;
            def.str = 2;
            def.hth = 3;
            def.intl = 4;
            def.wis = 5;
            def.dex = 6;
            def.hp = 7;
            def.mp = 8;
            def.defense = 9;
            def.hit = 10;
            def.min_attack = 11;
            GameWorldCoveragePeer::add_prefix(world, def);
            auto c = make_session();
            c.bag[0].prefix = prefix;
            recompute_combat(c, world);
            expect(c.prefix_bonus.str == (prefix == 1 ? 2 : 0) &&
                       c.prefix_bonus.hth == (prefix == 1 ? 3 : 0) &&
                       c.prefix_bonus.intl == (prefix == 1 ? 4 : 0) &&
                       c.prefix_bonus.wis == (prefix == 1 ? 5 : 0) &&
                       c.prefix_bonus.dex == (prefix == 1 ? 6 : 0) &&
                       c.defense == 100 + (prefix == 1 ? 9 : 0) &&
                       c.defense_range == 200 + (prefix == 1 ? 9 : 0),
                   "only a resolved positive prefix contributes primary and derived equipment "
                   "bonuses");
        }
        auto fallback = make_session();
        fallback.bag = {};
        recompute_combat(fallback, world);
        expect(fallback.weapon_min_attack == 100 && fallback.defense == 100,
               "legacy weapon/armor definitions remain fallback when no ordinary bag gear is worn");
        auto index_match = make_session();
        index_match.weapon_iid = 0;
        index_match.bag[0].mix_type = 10 | (1 << 4);
        recompute_combat(index_match, world);
        expect(index_match.weapon_min_attack == 150,
               "legacy index lookup retains the equipped weapon mix");
        auto missing_fallback = make_session();
        missing_fallback.bag = {};
        missing_fallback.weapon_index = missing_fallback.armor_index = 999;
        recompute_combat(missing_fallback, world);
        expect(missing_fallback.weapon_min_attack == 0 && missing_fallback.weapon_max_attack == 0 &&
                   missing_fallback.defense == 0 && missing_fallback.defense_range == 0,
               "unresolved legacy fallback definitions cannot contribute weapon or armor stats");
        auto missing_iid = make_session();
        missing_iid.weapon_iid = 999;
        missing_iid.bag[0].mix_type = 10 | (1 << 4);
        recompute_combat(missing_iid, world);
        expect(missing_iid.weapon_min_attack == 100,
               "a missing equipped weapon instance does not borrow another instance's mix");
        auto skip_index = make_session();
        skip_index.weapon_iid = 0;
        skip_index.bag[0].equipped = false;
        skip_index.bag[0].mix_type = 10 | (1 << 4);
        skip_index.bag[2].index = 1;
        skip_index.bag[2].count = 0;
        skip_index.bag[2].equipped = true;
        skip_index.bag[3].iid = 300;
        skip_index.bag[3].index = 1;
        skip_index.bag[3].count = 1;
        skip_index.bag[3].equipped = true;
        skip_index.bag[3].mix_type = 10 | (1 << 4);
        recompute_combat(skip_index, world);
        expect(skip_index.weapon_min_attack == 150,
               "legacy mix lookup skips unequipped, unrelated, and empty instances before a valid "
               "match");
        auto defaults = make_session();
        weapon.range = weapon.attack_speed_ms = 0;
        GameWorldCoveragePeer::add_item(world, weapon);
        recompute_combat(defaults, world);
        expect(defaults.attack_range == 16 && defaults.attack_speed_ms == 700,
               "nonpositive authored weapon range and speed preserve server defaults");
        weapon.range = 48;
        weapon.attack_speed_ms = 750;
        GameWorldCoveragePeer::add_item(world, weapon);
        auto invalid_bof = make_session();
        invalid_bof.bag[1].bof = 1;
        recompute_combat(invalid_bof, world);
        expect(invalid_bof.defense == 100 && invalid_bof.defense_range == 200 &&
                   invalid_bof.prefix_bonus.str == 0,
               "a Bof marker on an unsupported shield cannot grant set bonuses");

        bool augments_ok = true;
        const auto check_augment = [&](const auto& bonus, ClientSession& c, bool on_weapon) {
            recompute_combat(c, world);
            combat::BaseStats stats;
            stats.str = c.str;
            stats.hth = c.hth;
            stats.intl = c.intl;
            stats.wis = c.wis;
            stats.dex = c.dex;
            switch (bonus.primary_kind()) {
            case 0:
                stats.str += bonus.primary_points();
                break;
            case 1:
                stats.hth += bonus.primary_points();
                break;
            case 2:
                stats.dex += bonus.primary_points();
                break;
            case 3:
                stats.wis += bonus.primary_points();
                break;
            case 4:
                stats.intl += bonus.primary_points();
                break;
            default:
                break;
            }
            std::int32_t min_a = 0, max_a = 0, min_m = 0, max_m = 0, hit = 0, dodge = 0;
            std::int32_t fire = 0, ice = 0, lightning = 0, curse = 0, palsy = 0;
            combat::apply_stat_revise(c.level,
                                      stats,
                                      min_a,
                                      max_a,
                                      hit,
                                      dodge,
                                      min_m,
                                      max_m,
                                      fire,
                                      ice,
                                      lightning,
                                      curse,
                                      palsy);
            min_a += 100;
            max_a += 200;
            min_m += 50;
            max_m += 80;
            const int physical = on_weapon ? bonus.attack_percent() : 0;
            const int magical = on_weapon ? bonus.magic_percent() : 0;
            augments_ok = augments_ok && c.min_attack == min_a + min_a * physical / 100 &&
                          c.max_attack == max_a + max_a * physical / 100 &&
                          c.min_magic == min_m + min_m * magical / 100 &&
                          c.max_magic == max_m + max_m * magical / 100 &&
                          c.hit == hit + bonus.hit() && c.dodge == dodge + bonus.dodge() &&
                          c.defense == 100 + bonus.defense() &&
                          c.defense_range == 200 + bonus.defense() &&
                          c.weapon_min_attack == 100 + 100 * physical / 100 &&
                          c.weapon_max_attack == 200 + 200 * physical / 100 &&
                          c.weapon_min_magic == 50 + 50 * magical / 100 &&
                          c.weapon_max_magic == 80 + 80 * magical / 100 &&
                          c.max_hp == combat::max_hp_from_stats(stats.hth, c.level, c.clazz) + 10 +
                                          bonus.hp() &&
                          c.max_mp == combat::max_mp_from_stats(stats.wis, c.level, c.clazz) + 20 +
                                          bonus.mp();
        };
        armor.class_small = "helmet";
        GameWorldCoveragePeer::add_item(world, armor);
        for (bool on_weapon : {false, true}) {
            for (int effect = 0; effect <= 81; ++effect) {
                auto c = make_session();
                c.bag[on_weapon ? 0 : 1].qigong_effect = effect;
                const auto bonus =
                    QigongBonus::from_effect(effect, on_weapon, on_weapon ? "sword2h" : "helmet");
                check_augment(bonus, c, on_weapon);
            }
            for (int effect = 0; effect <= 241; ++effect) {
                auto c = make_session();
                c.bag[on_weapon ? 0 : 1].dragon_effect = effect;
                const auto bonus = DragonFusingBonus::from_effect(
                    effect, on_weapon, on_weapon ? "sword2h" : "helmet");
                check_augment(bonus, c, on_weapon);
            }
        }
        expect(augments_ok,
               "every Qigong and Dragon Fusing effect composes exact primary, gear, resource and "
               "percentage outputs");

        armor.class_small = "shield";
        GameWorldCoveragePeer::add_item(world, armor);
        RidingDef ride;
        ride.kind = protocol::RidingKind::Battle;
        GameWorldCoveragePeer::add_riding(world, 4, ride);
        ItemCombatDef ride_item;
        ride_item.index = 4;
        GameWorldCoveragePeer::add_item(world, ride_item);
        bool riding_ok = true;
        constexpr int primary[] = {1, 12, 24, 35};
        constexpr int secondary[] = {1, 6, 12, 17};
        for (int grade = 0; grade <= 3; ++grade) {
            for (int type = 0; type < 6; ++type) {
                for (int satiety : {0, 50, 100}) {
                    auto c = make_session();
                    c.mounted = true;
                    c.bag[2].iid = 200;
                    c.bag[2].index = 4;
                    c.bag[2].count = 1;
                    c.bag[2].equipped = true;
                    c.bag[3].iid = 300;
                    c.bag[3].index = 30200 + grade * 6 + type;
                    c.bag[3].count = 1;
                    c.bag[3].upgrade_level = 5;
                    auto& mount = c.battle_mounts[200];
                    mount.satiety = satiety;
                    mount.max_satiety = 100;
                    mount.equipment_iids[0] = 300;
                    const int expected = (type >= 4 ? secondary[grade] : primary[grade]) + 5;
                    const int scaled = expected * satiety / 100;
                    recompute_combat(c, world);
                    riding_ok = riding_ok &&
                                c.min_attack == plain.min_attack + (type == 0 ? scaled : 0) &&
                                c.max_attack == plain.max_attack + (type == 0 ? scaled : 0) &&
                                c.min_magic == plain.min_magic + (type == 1 ? scaled : 0) &&
                                c.max_magic == plain.max_magic + (type == 1 ? scaled : 0) &&
                                c.defense == plain.defense + (type == 2 ? scaled : 0) &&
                                c.defense_range ==
                                    plain.defense_range + (type == 2 || type == 4 ? scaled : 0) &&
                                c.max_hp == plain.max_hp + (type == 3 ? scaled : 0) &&
                                c.dodge == plain.dodge + (type == 5 ? scaled : 0);
                }
            }
        }
        expect(riding_ok,
               "all six battle riding equipment kinds and four grades scale exactly with satiety");
        for (int invalid : {-1, 0, 30199, 30224}) {
            auto c = make_session();
            c.mounted = true;
            c.bag[2].iid = 200;
            c.bag[2].index = 4;
            c.bag[2].count = 1;
            c.bag[2].equipped = true;
            c.bag[3].iid = 300;
            c.bag[3].index = invalid;
            c.battle_mounts[200].equipment_iids[0] = invalid == -1 ? 999 : 300;
            recompute_combat(c, world);
            expect(combat_values(session_combatant(c, true, false)) == plain_values &&
                       c.max_hp == plain.max_hp,
                   "missing or out-of-range mounted equipment cannot contribute stats");
        }
        for (int missing_state = 0; missing_state < 4; ++missing_state) {
            auto c = make_session();
            c.mounted = true;
            c.bag[2].index = 4;
            c.bag[2].count = missing_state == 1 ? 0 : 1;
            c.bag[2].equipped = missing_state != 0;
            c.bag[2].iid = 200;
            if (missing_state == 3) {
                RidingDef common;
                common.kind = protocol::RidingKind::Common;
                GameWorldCoveragePeer::add_riding(world, 4, common);
            }
            recompute_combat(c, world);
            expect(combat_values(session_combatant(c, true, false)) == plain_values,
                   "mount flags alone cannot grant battle equipment bonuses without a valid mount "
                   "record");
        }

        bool transform_ok = true;
        for (int race : {-1, 0, 1, 2, 4, 6, 8, 10, 11, 12}) {
            ItemCombatDef egg;
            egg.index = 5;
            egg.is_transform = true;
            egg.transform_race = race;
            GameWorldCoveragePeer::add_item(world, egg);
            for (int stage : {0, 1, 2, 3, 4}) {
                auto c = make_session();
                c.transform.index = 5;
                c.transform.level = 10;
                c.transform_stage = stage;
                const auto worn = protocol::worn_transform_bonus(race, 10);
                const auto form = protocol::form_transform_bonus(race, stage);
                combat::BaseStats stats;
                stats.str = c.str + worn.str;
                stats.hth = c.hth + worn.hth;
                stats.intl = c.intl + worn.intl;
                stats.wis = c.wis + worn.wis;
                stats.dex = c.dex + worn.dex;
                std::int32_t min_a = 0, max_a = 0, min_m = 0, max_m = 0, hit = 0, dodge = 0;
                std::int32_t fire = 0, ice = 0, lightning = 0, curse = 0, palsy = 0;
                combat::apply_stat_revise(c.level,
                                          stats,
                                          min_a,
                                          max_a,
                                          hit,
                                          dodge,
                                          min_m,
                                          max_m,
                                          fire,
                                          ice,
                                          lightning,
                                          curse,
                                          palsy);
                min_a += 100;
                max_a += 200;
                min_m += 50;
                max_m += 80;
                recompute_combat(c, world);
                transform_ok = transform_ok && c.prefix_bonus.str == worn.str &&
                               c.prefix_bonus.hth == worn.hth && c.prefix_bonus.intl == worn.intl &&
                               c.prefix_bonus.wis == worn.wis && c.prefix_bonus.dex == worn.dex &&
                               c.min_attack == min_a + min_a * form.attack_percent / 100 &&
                               c.max_attack == max_a + max_a * form.attack_percent / 100 &&
                               c.min_magic == min_m + min_m * form.magic_percent / 100 &&
                               c.max_magic == max_m + max_m * form.magic_percent / 100 &&
                               c.defense == 100 + (form.defense_min + form.defense_max) / 2 &&
                               c.defense_range == 200 + (form.defense_min + form.defense_max) / 2 &&
                               c.transform_mspeed_percent == form.mspeed_percent;
            }
        }
        expect(transform_ok,
               "transform races and every stage compose exact worn and active-form bonuses");
        for (int egg_index : {0, 999, 1}) {
            auto c = make_session();
            c.transform.index = egg_index;
            c.transform_stage = 1;
            recompute_combat(c, world);
            expect(combat_values(session_combatant(c, true, false)) == plain_values,
                   "missing or non-transform egg cannot grant worn or active-form bonuses");
        }

        for (SkillBuffId id : {SkillBuffId::ArcherCurseStrength,
                               SkillBuffId::ArcherCurseHealth,
                               SkillBuffId::ArcherCurseAgility,
                               SkillBuffId::ArcherCurseIntelligence,
                               SkillBuffId::ArcherCurseWisdom}) {
            auto c = make_session();
            apply_buff(c, id, -20);
            recompute_combat(c, world);
            expect(c.prefix_bonus.str ==
                           (id == SkillBuffId::ArcherCurseStrength ? c.str * -20 / 100 : 0) &&
                       c.prefix_bonus.hth ==
                           (id == SkillBuffId::ArcherCurseHealth ? c.hth * -20 / 100 : 0) &&
                       c.prefix_bonus.dex ==
                           (id == SkillBuffId::ArcherCurseAgility ? c.dex * -20 / 100 : 0) &&
                       c.prefix_bonus.intl ==
                           (id == SkillBuffId::ArcherCurseIntelligence ? c.intl * -20 / 100 : 0) &&
                       c.prefix_bonus.wis ==
                           (id == SkillBuffId::ArcherCurseWisdom ? c.wis * -20 / 100 : 0),
                   "primary curses alter only their named stat with signed integer rounding");
        }
        auto cursed = make_session();
        cursed.learned_skills[4] = 20;
        recompute_combat(cursed, world);
        const auto uncursed_hit = cursed.hit;
        apply_buff(cursed, SkillBuffId::ArcherCurseCritical, -7);
        apply_buff(cursed, SkillBuffId::ArcherCurseDefense, -25, -50);
        apply_buff(cursed, SkillBuffId::ArcherVanishConfidence, -50);
        apply_buff(cursed, SkillBuffId::ArcherMeshSpeed, -20);
        apply_buff(cursed, SkillBuffId::ArcherCloudSight, -8);
        recompute_combat(cursed, world);
        expect(
            cursed.fatal_rate == 13 && cursed.defense == 75 && cursed.defense_range == 100 &&
                cursed.hit == uncursed_hit + uncursed_hit * -50 / 100 &&
                cursed.attack_range == 40 &&
                cursed.attack_speed_ms == combat::revised_attack_delay_ms(750, -20),
            "critical, defense, accuracy, speed and range curses preserve their separate formulas");

        for (int clazz : {0, 2}) {
            auto c = make_session();
            c.clazz = clazz;
            recompute_combat(c, world);
            const int base_min = c.min_attack, base_max = c.max_attack, base_hit = c.hit,
                      base_dodge = c.dodge;
            for (int skill : {2, 5, 6, 8, 12, 17, 20, 30, 32, 36, 37, 39, 48, 67, 68, 70, 71}) {
                c.learned_skills[skill] = 3;
            }
            recompute_combat(c, world);
            if (clazz == 0) {
                const auto mastery = combat::knight_weapon_mastery_contribution(3);
                const auto critical = combat::knight_critical_contribution(3);
                const int blessing = combat::knight_blessing_power_percent(3);
                const int min = base_min + mastery.min_attack, max = base_max + mastery.max_attack;
                const int defense = combat::knight_defense_strengthening_points(c.level, 100);
                expect(c.min_attack == min + min * blessing / 100 &&
                           c.max_attack == max + max * blessing / 100 &&
                           c.hit == base_hit + mastery.hit &&
                           c.fatal_pt == critical.fatal_pt + 20 &&
                           c.fatal_rate == critical.fatal_rate && c.defense == 100 + defense &&
                           c.defense_range == 200 + defense &&
                           c.block_percent == 3 + combat::knight_weapon_defend_block(c.level),
                       "learned Knight masteries, criticals, blessing, defense and block compose "
                       "exactly");
            } else {
                const auto mastery = combat::archer_bow_mastery_contribution(3);
                const auto critical = combat::archer_fatal_contribution(3, true);
                const auto evasion = combat::archer_evasion_contribution(3, 3);
                const int blessing = combat::archer_blessing_power_percent(3);
                const int min = base_min + mastery.min_attack, max = base_max + mastery.max_attack;
                expect(c.min_attack == min + min * blessing / 100 &&
                           c.max_attack == max + max * blessing / 100 &&
                           c.hit == base_hit + combat::archer_concentration_hit(3) &&
                           c.fatal_pt == critical.fatal_pt + critical.advanced_fatal_pt &&
                           c.fatal_rate == critical.fatal_rate &&
                           c.perfect_evasion_base == evasion.base_percent &&
                           c.perfect_evasion_advanced == evasion.advanced_percent &&
                           c.dodge == base_dodge + combat::archer_shrewdness_dodge_points(
                                                       base_dodge, c.level),
                       "learned Archer mastery, concentration, criticals, evasion and blessing "
                       "compose exactly");
            }
        }
        auto unknown_class = make_session();
        unknown_class.clazz = 4;
        unknown_class.learned_skills[2] = 10;
        unknown_class.learned_skills[30] = 10;
        recompute_combat(unknown_class, world);
        expect(unknown_class.min_attack == plain.min_attack &&
                   unknown_class.max_attack == plain.max_attack &&
                   unknown_class.max_hp ==
                       combat::max_hp_from_stats(unknown_class.hth, unknown_class.level, 4) + 10,
               "unknown persisted class retains clamped base formulas without borrowing "
               "class-specific passives");
        const auto root = test::temp_dir(("optimization_bof_" + std::to_string(getpid())).c_str());
        std::ofstream(root + "/bof.json") << R"({"minGrade":5,"halfCount":3,"fullCount":5,
          "halfDefensePercent":7,"fullHpMpPercent":11,"fullResistPercent":13,
          "armorParts":["helmet"],"fxLevels":[5],"tiers":[{"grade":5,"piece":2,"halfSet":3,"fullSet":4}]})";
        expect(world.load_bof_json(root + "/bof.json"),
               "load independently authored Bof composition catalog");
        ItemCombatDef helmet;
        helmet.index = 10;
        helmet.is_defense = true;
        helmet.class_big = "defense";
        helmet.class_small = "helmet";
        helmet.grade = 5;
        helmet.defense_melee = 1;
        helmet.defense_range = 2;
        helmet.res_fire = helmet.res_ice = helmet.res_lightning = helmet.res_curse =
            helmet.res_palsy = 2;
        GameWorldCoveragePeer::add_item(world, helmet);
        bool bof_ok = true;
        for (int clazz = 0; clazz < 4; ++clazz) {
            for (int pieces = 0; pieces <= 5; ++pieces) {
                auto c = make_session();
                c.clazz = clazz;
                for (int i = 0; i < pieces; ++i) {
                    auto& slot = c.bag[static_cast<std::size_t>(i + 2)];
                    slot.index = 10;
                    slot.iid = 100 + static_cast<std::uint32_t>(i);
                    slot.count = 1;
                    slot.equipped = true;
                    slot.bof = 1;
                }
                const int added = pieces * 2 + (pieces == 5 ? 4 : pieces >= 3 ? 3 : 0);
                const int hth = c.hth + added, wis = c.wis;
                int hp = combat::max_hp_from_stats(hth, c.level, clazz) + 10;
                int mp = combat::max_mp_from_stats(wis, c.level, clazz) + 20;
                if (pieces == 5) {
                    hp += hp * 11 / 100;
                    mp += mp * 11 / 100;
                }
                int melee = 100 + pieces, ranged = 200 + pieces * 2;
                if (pieces >= 3) {
                    melee += melee * 7 / 100;
                    ranged += ranged * 7 / 100;
                }
                recompute_combat(c, world);
                bof_ok = bof_ok && c.prefix_bonus.str == added && c.prefix_bonus.hth == added &&
                         c.prefix_bonus.intl == (clazz == 1 ? added : 0) &&
                         c.prefix_bonus.dex == (clazz == 1 ? 0 : added) && c.defense == melee &&
                         c.defense_range == ranged && c.max_hp == hp && c.max_mp == mp;
            }
        }
        expect(bof_ok,
               "Bof partial/half/full sets preserve class primaries and exact defense/resource "
               "percentages");
        OwnedGuildStore guild_store;
        auto characters = create_data_store(StoreBackend::Sqlite);
        expect(characters->open(root + "/guild_characters"),
               "open owned character boundary for guild composition");
        GuildRules rules;
        std::string error;
        expect(
            rules.parse(
                platform::Json::parse(
                    R"({"grades":{"minimum_members_by_grade":[1, 2, 3, 4, 5, 6, 7, 8, 9, 10]},"buffs":{"enabled":true,"minimum_grade":1,"primary_per_grade":2,"hp_per_grade":10,"defense_per_grade":3}})"),
                error),
            "parse independently authored guild stat policy");
        GuildService guild(rules, guild_store, *characters);
        expect(guild.restore(), "restore owned guild stat fixture");
        auto guilded = make_session();
        guilded.character_id = 1;
        const auto guild_bonus = guild.bonuses(1);
        g_guilds = &guild;
        recompute_combat(guilded, world);
        g_guilds = nullptr;
        expect(guild_bonus[0] > 0 && guilded.prefix_bonus.str == guild_bonus[0] &&
                   guilded.prefix_bonus.hth == guild_bonus[0] &&
                   guilded.prefix_bonus.intl == guild_bonus[0] &&
                   guilded.prefix_bonus.wis == guild_bonus[0] &&
                   guilded.prefix_bonus.dex == guild_bonus[0] &&
                   guilded.max_hp == combat::max_hp_from_stats(guilded.hth + guild_bonus[0],
                                                               guilded.level,
                                                               guilded.clazz) +
                                         10 + guild_bonus[1] &&
                   guilded.defense == 100 + guild_bonus[2] &&
                   guilded.defense_range == 200 + guild_bonus[2],
               "guild primaries precede base formulas while flat guild HP and defense apply once "
               "afterward");
        characters->close();
        std::filesystem::remove_all(root);
        armor.hp = armor.mp = -100000;
        GameWorldCoveragePeer::add_item(world, armor);
        auto negative = make_session();
        recompute_combat(negative, world);
        expect(negative.max_hp == 1 && negative.max_mp == 1,
               "negative authored resource bonuses retain minimum-one maxima");
        weapon.min_attack = 2000;
        weapon.max_attack = -1000;
        GameWorldCoveragePeer::add_item(world, weapon);
        auto reversed = make_session();
        recompute_combat(reversed, world);
        expect(reversed.min_attack <= reversed.max_attack,
               "reversed authored physical damage endpoints are ordered after composition");
    }

    void test_combatant() {
        GameWorld world;
        ClientSession c;
        c.level = 42;
        c.min_attack = 11;
        c.max_attack = 22;
        c.min_magic = 33;
        c.max_magic = 44;
        c.hit = 55;
        c.dodge = 66;
        c.defense = 77;
        c.defense_range = 88;
        c.absorb = 99;
        c.fatal_rate = 10;
        c.fatal_pt = 20;
        c.res_fire = 1;
        c.res_ice = 2;
        c.res_lightning = 3;
        c.res_curse = 4;
        c.res_palsy = 5;
        c.attack_range = 63;
        c.attack_speed_ms = 123;
        c.upgrade_level = 4;
        c.char_attack = 7;
        c.char_magic = 8;
        c.weapon_min_attack = 9;
        c.weapon_max_attack = 10;
        c.weapon_min_magic = 11;
        c.weapon_max_magic = 12;
        c.perfect_evasion_base = 13;
        c.perfect_evasion_advanced = 14;
        c.block_percent = 15;
        c.collection_bonus.ability[16] = 17;
        c.collection_bonus.ability[17] = 18;
        const auto base = session_combatant(c, false, false);
        expect(base.level == 42 && base.min_attack == 11 && base.max_attack == 22 &&
                   base.min_magic == 33 && base.max_magic == 44 && base.hit == 55 &&
                   base.dodge == 66 && base.defense == 77 && base.defense_range == 88 &&
                   base.absorb == 99 && base.fatal_rate == 10 && base.fatal_pt == 20 &&
                   base.res_fire == 1 && base.res_ice == 2 && base.res_lightning == 3 &&
                   base.res_curse == 4 && base.res_palsy == 5 && base.attack_range == 63 &&
                   base.attack_speed_ms == 123 && base.upgrade_level == 4 &&
                   base.char_attack == 7 && base.char_magic == 8 && base.weapon_min_attack == 9 &&
                   base.weapon_max_attack == 10 && base.weapon_min_magic == 11 &&
                   base.weapon_max_magic == 12 && base.perfect_evasion_base == 13 &&
                   base.perfect_evasion_advanced == 14 && base.block_percent == 15 &&
                   base.critical_damage_percent == 17 && base.explosive_damage_percent == 18 &&
                   base.is_player && !base.armor_broken && !base.has_parry &&
                   !base.has_second_job_attrib,
               "combatant copies every unbuffed numeric combat input");
        apply_buff(c, SkillBuffId::RefiningWeapon, 6);
        apply_buff(c, SkillBuffId::StoneShield, 3, 4);
        apply_buff(c, SkillBuffId::MageMagicIntensification, 2);
        apply_buff(c, SkillBuffId::ThiefArmorBreaker, 1);
        c.learned_skills[combat::kKnightParrySkill] = 1;
        c.specialty = combat::kSecondJobAttrib1;
        c.wearing_shield = c.wearing_two_hand = true;
        const auto buffed = session_combatant(c, true, false);
        expect(buffed.min_attack == 17 && buffed.max_attack == 28 && buffed.min_magic == 39 &&
                   buffed.max_magic == 50 && buffed.weapon_min_attack == 15 &&
                   buffed.weapon_max_attack == 16 && buffed.weapon_min_magic == 17 &&
                   buffed.weapon_max_magic == 18 && buffed.armor_broken &&
                   buffed.magic_intensification_rank == 2 && buffed.has_parry &&
                   buffed.has_second_job_attrib && buffed.wearing_shield &&
                   buffed.wearing_two_hand && buffed.bstate == c.skill_buffs.bstate(),
               "combatant includes refinement, typed buffs, equipment, and learned passives");
        c.collection_pve = false;
        expect(combat_values(session_combatant(c, false, false)) == combat_values(buffed),
               "noncontact PvP snapshot does not switch an already selected profile");
        c.collection_pve = true;
        (void) session_combatant(c, true, true);
        expect(c.collection_pve, "contact without a collection world retains selected profile");
        for (int clazz = 0; clazz <= 3; ++clazz) {
            ClientSession p;
            p.clazz = clazz;
            p.level = 55;
            p.cur_hp = 7;
            p.cur_mp = 9;
            p.learned_skills[30] = 1;
            recompute_combat(p, world);
            const auto before = combat_values(session_combatant(p, true, false));
            const auto resources = std::make_tuple(p.cur_hp,
                                                   p.cur_mp,
                                                   p.max_hp,
                                                   p.max_mp,
                                                   p.str,
                                                   p.hth,
                                                   p.intl,
                                                   p.wis,
                                                   p.dex,
                                                   p.gstate);
            const auto preview = session_combatant(p, false, false);
            expect(combat_values(preview) == before &&
                       combat_values(session_combatant(p, true, false)) == before &&
                       p.collection_pve &&
                       resources == std::make_tuple(p.cur_hp,
                                                    p.cur_mp,
                                                    p.max_hp,
                                                    p.max_mp,
                                                    p.str,
                                                    p.hth,
                                                    p.intl,
                                                    p.wis,
                                                    p.dex,
                                                    p.gstate),
                   "unarmed PvP preview restores combat state and resources for every class");

            // Complete owned collection: this must change PvE output while its
            // PvP preview matches an otherwise identical unequipped character.
            for (int slot = 0; slot <= 8; ++slot) {
                auto& item = p.collection[static_cast<std::size_t>(slot)];
                item.iid = 1000 + static_cast<std::uint32_t>(slot);
                item.index = slot == 0 ? kYinYangMirrorFirst : kTrigramFirst + slot - 1;
                item.count = 1;
                item.upgrade_level = 1;
                item.equipped = true;
            }
            recompute_combat(p, world);
            const auto collected = combat_values(session_combatant(p, true, false));
            const auto collection_state = std::make_tuple(p.cur_hp,
                                                          p.cur_mp,
                                                          p.max_hp,
                                                          p.max_mp,
                                                          p.prefix_bonus.str,
                                                          p.prefix_bonus.hth,
                                                          p.prefix_bonus.intl,
                                                          p.prefix_bonus.wis,
                                                          p.prefix_bonus.dex,
                                                          p.collection_bonus.attack,
                                                          p.collection_bonus.hp,
                                                          p.collection_bonus.mp,
                                                          p.collection_bonus.ability,
                                                          p.collection_pvp_until_ms,
                                                          p.gstate,
                                                          p.stats_initialized);
            expect(collected != before &&
                       combat_values(session_combatant(p, false, false)) == before &&
                       combat_values(session_combatant(p, true, false)) == collected &&
                       collection_state == std::make_tuple(p.cur_hp,
                                                           p.cur_mp,
                                                           p.max_hp,
                                                           p.max_mp,
                                                           p.prefix_bonus.str,
                                                           p.prefix_bonus.hth,
                                                           p.prefix_bonus.intl,
                                                           p.prefix_bonus.wis,
                                                           p.prefix_bonus.dex,
                                                           p.collection_bonus.attack,
                                                           p.collection_bonus.hp,
                                                           p.collection_bonus.mp,
                                                           p.collection_bonus.ability,
                                                           p.collection_pvp_until_ms,
                                                           p.gstate,
                                                           p.stats_initialized),
                   "collection PvP preview matches independent unarmed profile and restores every "
                   "computed output");
            (void) session_combatant(p, false, true);
            expect(!p.collection_pve && p.collection_pvp_until_ms > 0,
                   "PvP contact selects profile and starts its suppression interval");
            (void) session_combatant(p, true, true);
            expect(!p.collection_pve, "PvE contact cannot bypass active PvP suppression");
            p.collection_pvp_until_ms = 0;
            (void) session_combatant(p, true, true);
            expect(p.collection_pve, "PvE profile returns after PvP suppression expires");
            p.cur_hp = p.cur_mp = -1;
            recompute_combat(p, world);
            expect(p.cur_hp == 0 && p.cur_mp == 0,
                   "stat recomputation clamps negative resources without revival");
            p.cur_hp = p.cur_mp = 1000000;
            recompute_combat(p, world);
            expect(p.cur_hp == p.max_hp && p.cur_mp == p.max_mp,
                   "stat recomputation clamps resources to recomputed maximums");
        }
        ClientSession no_world;
        no_world.collection_pve = false;
        select_collection_profile(no_world, true);
        expect(!no_world.collection_pve && no_world.collection_world == nullptr,
               "profile selection without a world does not claim a different computed profile");
        ClientSession dead;
        recompute_combat(dead, world);
        dead.cur_hp = dead.cur_mp = 0;
        auto recording = std::make_unique<RecordingSession>();
        const auto* output = recording.get();
        dead.sock = std::make_unique<HostConnection>(std::move(recording));
        select_collection_profile(dead, false);
        (void) dead.sock->flush(POLLIN | POLLOUT);
        expect(
            !dead.collection_pve && dead.cur_hp == 0 && dead.cur_mp == 0 &&
                !output->packets.empty(),
            "dead-player profile switch retains zero resources and sends its progression snapshot");
    }
#if defined(OPENKAL_DOGGEBI_BASELINE_BRIDGE)
    using H5EncounterState = h5_baseline::EncounterState;
#else
    using H5EncounterState = DoggebiEncounterState;
#endif

    bool h5_snapshot = false;
    std::vector<std::uint64_t> h5_clock_values;
    std::size_t h5_clock_reads = 0;

    std::uint64_t h5_presentation_clock() {
        expect(h5_clock_reads < h5_clock_values.size(),
               "H5 presentation clock stays inside its authored finite script");
        const auto at = h5_clock_reads++;
        return at < h5_clock_values.size() ? h5_clock_values[at] : 10000;
    }

    template <typename... Values> void h5_fields(std::ostream& out, const Values&... values) {
        ((out << values << ','), ...);
        out << '\n';
    }

    template <typename Map>
    void h5_sorted_map(std::ostream& out, std::string_view name, const Map& source) {
        using Pair = typename Map::value_type;
        std::vector<const Pair*> values;
        for (const auto& value : source) {
            values.push_back(&value);
        }
        std::sort(values.begin(), values.end(), [](const auto* a, const auto* b) {
            return a->first < b->first;
        });
        out << name;
        for (const auto* value : values) {
            out << ',' << value->first << ':' << value->second;
        }
        out << '\n';
    }

    void h5_buffs(std::ostream& out, const SkillBuffSet& buffs) {
        for (const auto& entry : buffs.entries()) {
            h5_fields(out,
                      "buff",
                      static_cast<int>(entry.id),
                      entry.bstate,
                      entry.skill_index,
                      entry.clazz,
                      entry.value1,
                      entry.value2,
                      entry.expire_unix);
        }
    }

    void h5_monster(std::ostream& out, const MonsterEntity& m) {
        h5_fields(out,
                  "monster",
                  m.id,
                  m.def_index,
                  m.race,
                  std::quoted(m.name),
                  m.x,
                  m.y,
                  m.z,
                  m.map,
                  m.dir,
                  m.hp,
                  m.max_hp,
                  m.roam_speed,
                  m.chase_speed,
                  m.speed,
                  m.roam_x0,
                  m.roam_y0,
                  m.roam_x1,
                  m.roam_y1,
                  m.target_x,
                  m.target_y,
                  m.moving,
                  m.move_is_run,
                  m.move_busy_left,
                  m.idle_left,
                  m.dirty_create,
                  m.dirty_move,
                  m.dirty_stop,
                  static_cast<int>(m.ai),
                  m.aggro_player_id,
                  m.aggro,
                  m.beacon_pursuit,
                  m.aggro_hostility,
                  m.aggressive,
                  m.attack_range,
                  m.detect_range,
                  m.sight_range,
                  m.min_attack,
                  m.max_attack,
                  m.attack_cooldown,
                  m.skill_cooldown,
                  m.skill_precast_left,
                  m.pending_skill_slot,
                  m.next_skill_slot,
                  m.spawn_protection_left,
                  m.doggebi_absorb_left,
                  m.doggebi_resist_all_left,
                  m.doggebi_pending_reaction_damage,
                  m.doggebi_pending_reaction_physical,
                  m.attack_swinging,
                  m.detect_cooldown,
                  m.control_left,
                  m.control_break_on_damage,
                  m.open_player_id,
                  m.open_left,
                  m.open_remove_left,
                  m.gstate,
                  m.knee_left,
                  m.die_remove_left,
                  m.fire_flower_explode_left,
                  m.fire_flower_skill_fire_left,
                  m.fire_flower_remove_left,
                  m.fire_flower_exploded,
                  m.fire_flower_removed,
                  m.slow_left,
                  m.slow_percent,
                  m.dirty_slow);
        h5_sorted_map(out, "hostility", m.hostility_by_player);
        h5_sorted_map(out, "reward", m.reward_damage_by_player);
        h5_buffs(out, m.skill_buffs);
        out << "path";
        for (const auto& point : m.path_waypoints) {
            out << ',' << point.first << ':' << point.second;
        }
        out << '\n';
    }

    using H5Packets = std::array<std::vector<std::vector<std::uint8_t>>, 6>;

    struct H5Players {
        GameWorld world;
        std::vector<ClientSession> clients{6};
        std::array<RecordingSession*, 6> outputs{};
        std::mutex world_mutex;
        std::uint32_t next_entity_id = 8000;
        std::optional<DoggebiRitual> ritual;
        H5EncounterState encounter;
        int next_random = 0;
        static constexpr std::uint32_t kLord = 4000;
        static constexpr int kX = 2048;
        static constexpr int kY = 2048;

        H5Players() {
            std::vector<DoggebiRitualMonument> monuments;
            for (int slot = 0; slot < 8; ++slot) {
                monuments.push_back(
                    {static_cast<std::uint32_t>(6000 + slot), 3, slot, 24027 + slot});
            }
            const auto config = DoggebiRitualConfig::create(monuments);
            expect(config.has_value(), "H5 owns a complete eight-monument ritual layout");
            if (config) {
                ritual.emplace(*config);
            }
            for (std::size_t i = 0; i < clients.size(); ++i) {
                auto& c = clients[i];
                c.phase = i == 3 ? SessionPhase::WaitConnect : SessionPhase::Playing;
                c.player_id = 700 + static_cast<std::uint32_t>(i);
                c.map = i == 2 ? 4 : 3;
                c.px = kX + static_cast<int>(i) * 8;
                c.py = kY;
                c.pz = 42 + static_cast<int>(i);
                c.level = 1;
                c.stats_initialized = true;
                c.collection_pve = true;
                c.collection_world = nullptr;
                c.cur_hp = i == 5 ? 0 : 10000;
                c.max_hp = 10000;
                c.cur_mp = c.max_mp = 100;
                c.doggebi_state_sent = true;
                c.known_monsters.insert(kLord);
                c.dialog_npc_id = 1234;
                if (i != 4) {
                    auto recording = std::make_unique<RecordingSession>();
                    outputs[i] = recording.get();
                    c.sock = std::make_unique<HostConnection>(std::move(recording));
                }
            }
            auto lord = monster(kLord, kX, kY);
            lord.def_index = 213;
            lord.aggro = true;
            lord.aggro_player_id = clients[0].player_id;
            lord.aggro_hostility = 17;
            lord.hostility_by_player = {{clients[0].player_id, 17}};
            lord.reward_damage_by_player = {{clients[0].player_id, 23}};
            world.monsters().push_back(std::move(lord));
            encounter.lord_id = kLord;
            encounter.lord_despawn_ms = 100000000;
            encounter.last_countdown_broadcast_ms = 10000;
            future_timers();
        }

        static MonsterEntity monster(std::uint32_t id, int x, int y, int map = 3) {
            MonsterEntity result;
            result.id = id;
            result.x = result.target_x = x;
            result.y = result.target_y = y;
            result.map = map;
            result.z = 42;
            result.hp = result.max_hp = 100;
            result.name = "Owned H5 entity";
            return result;
        }

        void future_timers() {
            encounter.timers = {
                100000000, 100000000, 100000000, 7321, 100000000, 100000000, 100000000};
        }

        void prepare(DoggebiLordSkill skill, std::uint64_t at = 10000) {
            const auto target =
                skill == DoggebiLordSkill::MoonSkin || skill == DoggebiLordSkill::ProtectionOfMoon
                    ? kLord
                    : clients[0].player_id;
            encounter.cast = {skill, target, at, true};
        }

        void activate_all(std::uint64_t at) {
            for (std::uint32_t id = 6000; id < 6008; ++id) {
                expect(ritual->activate(id, at) == DoggebiRitualActivation::Accepted,
                       "H5 activates each owned monument through the public domain API");
            }
        }

        void open_cell(int x, int y) {
            map::AreaCell area;
            area.set_version(map::kVersion);
            area.assign_attrs(static_cast<std::size_t>(map::kTileCount) * map::kTileCount, 0);
            area.ensure_blocked_mask(false);
            area.ensure_not_moveable_mask(false);
            world.grid().set_cell(x, y, std::move(area));
        }

        void definitions(bool spirit) {
            static unsigned serial = 0;
            const auto root = test::temp_dir(
                ("h5_definitions_" + std::to_string(getpid()) + "_" + std::to_string(++serial))
                    .c_str());
            test::mkdir_p(root);
            std::ofstream(root + "/213.json")
                << R"({"index":213,"name":"Owned H5 lord","stats":{"race":0,"level":1,"hp":1000,"str":4,"int":8,"magic":[3, 3]},"spawns":[]})";
            if (spirit) {
                std::ofstream(root + "/232.json")
                    << R"({"index":232,"name":"Owned H5 spirit","stats":{"race":0,"level":1,"hp":20},"spawns":[]})";
            }
            expect(world.load_monsters_json_dir(root, kX, kY, 0),
                   "H5 loads only independently authored monster definitions");
            open_cell(0, 0);
            open_cell(7, 1);
        }

        H5Packets drain() {
            H5Packets packets;
            for (std::size_t i = 0; i < clients.size(); ++i) {
                if (!clients[i].sock) {
                    continue;
                }
                for (int round = 0; clients[i].sock->pending_bytes() > 0 && round < 128; ++round) {
                    const auto before = clients[i].sock->pending_bytes();
                    expect(clients[i].sock->flush(POLLIN | POLLOUT),
                           "H5 recorder flush succeeds without losing a queued packet");
                    expect(clients[i].sock->pending_bytes() < before,
                           "H5 complete drain makes bounded byte progress");
                }
                expect(clients[i].sock->pending_bytes() == 0,
                       "H5 captures every queued packet after the flush attempt budget");
                packets[i] = std::move(outputs[i]->packets);
                outputs[i]->packets.clear();
            }
            return packets;
        }

        H5Packets tick(std::uint64_t at,
                       int expected_random_draws = 0,
                       std::vector<std::uint64_t> presentation_values = {}) {
            constexpr unsigned seed = 731;
            std::srand(seed);
            for (int i = 0; i < expected_random_draws; ++i) {
                (void) std::rand();
            }
            const int expected_next = std::rand();
            std::srand(seed);
            h5_clock_reads = 0;
            h5_clock_values = presentation_values.empty() ? std::vector<std::uint64_t>(128, 10000)
                                                          : std::move(presentation_values);
#if defined(OPENKAL_DOGGEBI_BASELINE_BRIDGE)
            h5_baseline::Bridge bridge(
                encounter, world, clients, world_mutex, next_entity_id, ritual);
            bridge.tick(at, h5_presentation_clock);
#else
            if (ritual.has_value()) {
                tick_doggebi_lord_ascension(
                    encounter, world, clients, world_mutex, ritual, at, h5_presentation_clock);
                tick_doggebi_lord_combat(
                    encounter, world, clients, world_mutex, next_entity_id, at);
                tick_doggebi_ritual(encounter,
                                    *ritual,
                                    world,
                                    clients,
                                    world_mutex,
                                    next_entity_id,
                                    at,
                                    h5_presentation_clock);
            }
#endif
            next_random = std::rand();
            expect(next_random == expected_next,
                   "H5 consumes exactly the authored same-runtime rand count");
            return drain();
        }
    };

    std::string h5_state(const H5Players& p, std::uint64_t at) {
        std::ostringstream out;
        out << std::setprecision(std::numeric_limits<float>::max_digits10);
        const auto& s = p.encounter;
        h5_fields(out,
                  "controller",
                  s.lord_id,
                  s.lord_despawn_ms,
                  s.last_countdown_broadcast_ms,
                  s.spirit_max,
                  s.spawn_cursor,
                  static_cast<int>(s.cast.skill),
                  s.cast.target_id,
                  s.cast.execute_at_ms,
                  s.cast.active,
                  s.timers.ground_slam_ms,
                  s.timers.teleport_ms,
                  s.timers.spirit_absorb_ms,
                  s.timers.protection_ms,
                  s.timers.rage_or_fury_ms,
                  s.timers.overrun_ms,
                  s.timers.summon_ms,
                  s.random,
                  s.ascending,
                  s.ascension_next_pulse_ms,
                  s.ascension_pulse_count,
                  p.next_entity_id,
                  h5_clock_reads,
                  p.next_random,
                  p.world.grid().cell_count(),
                  p.world.ground_items().size());
        out << "spirit_ids";
        for (auto id : s.spirit_ids) {
            out << ',' << id;
        }
        out << '\n';
        if (p.ritual) {
            h5_fields(out,
                      "ritual",
                      p.ritual->map(),
                      static_cast<int>(p.ritual->phase()),
                      static_cast<int>(p.ritual->active_mask()),
                      p.ritual->remaining_to_spawn_ms(at));
            for (std::size_t i = 0; i < 8; ++i) {
                const auto& m = p.ritual->monument(i);
                h5_fields(out,
                          "monument",
                          m.npc_id,
                          m.map,
                          m.slot,
                          m.required_item,
                          p.ritual->can_activate(m.npc_id));
            }
            for (const auto delta : {0u, 1u, 999u, 1000u, 100000u, 140000u, 300000u, 600000u}) {
                auto probe = *p.ritual;
                const auto change = probe.tick(at + delta);
                h5_fields(out,
                          "probe",
                          delta,
                          static_cast<int>(probe.phase()),
                          static_cast<int>(probe.active_mask()),
                          probe.remaining_to_spawn_ms(at + delta),
                          static_cast<int>(change.expired_mask),
                          change.phase_changed,
                          change.spawn_requested);
            }
        } else {
            out << "ritual absent\n";
        }
        // Keep vector ordering as well as complete public MonsterEntity state.
        for (const auto& monster : p.world.monsters()) {
            h5_monster(out, monster);
        }
        for (std::size_t i = 0; i < p.clients.size(); ++i) {
            const auto& c = p.clients[i];
            h5_fields(out,
                      "peer",
                      i,
                      static_cast<int>(c.phase),
                      c.player_id,
                      c.character_id,
                      c.map,
                      c.px,
                      c.py,
                      c.pz,
                      c.control_until_ms,
                      c.tele_pending,
                      c.tele_map,
                      c.tele_x,
                      c.tele_y,
                      c.tele_z,
                      c.dialog_npc_id,
                      c.pending_npc_teleport_id,
                      c.portal_armed,
                      c.beacon_active_until_ms,
                      c.beacon_ready_at,
                      c.talent_mask,
                      c.doggebi_state_sent,
                      c.sock != nullptr,
                      c.sock ? c.sock->alive() : false,
                      c.sock ? c.sock->pending_bytes() : 0);
            out << combat_session_state(c) << '\n' << combat_inventory_state(c) << '\n';
            h5_buffs(out, c.skill_buffs);
            std::vector<std::uint32_t> known(c.known_monsters.begin(), c.known_monsters.end());
            std::sort(known.begin(), known.end());
            out << "known";
            for (auto id : known) {
                out << ',' << id;
            }
            out << '\n';
        }
        return out.str();
    }

    void h5_observe(std::string_view name,
                    const H5Players& p,
                    const H5Packets& packets,
                    std::uint64_t at) {
        if (!h5_snapshot) {
            return;
        }
        std::cout << "h5 " << name << '\n' << h5_state(p, at);
        for (std::size_t i = 0; i < packets.size(); ++i) {
            std::cout << "wire " << i << '\n';
            for (const auto& packet : packets[i]) {
                for (auto byte : packet) {
                    std::cout << std::hex << std::setw(2) << std::setfill('0')
                              << static_cast<unsigned>(byte);
                }
                std::cout << std::dec << std::setfill(' ') << '\n';
            }
        }
    }

    std::vector<std::uint8_t> h5_hp(int value) {
        return protocol::Property::frame_update(
            static_cast<std::uint8_t>(protocol::PropertyType::CurHp), value);
    }

    std::vector<std::uint8_t> h5_map(std::uint8_t mask = 0,
                                     protocol::MapStateTimer timer = protocol::MapStateTimer::None,
                                     std::uint32_t seconds = 0,
                                     DoggebiRitualPhase phase = DoggebiRitualPhase::Idle) {
        protocol::MapStateFields state;
        state.map = 3;
        state.phase = static_cast<std::uint8_t>(phase);
        state.active_mask = mask;
        state.timer = timer;
        state.timer_seconds = seconds;
        return protocol::MapState::frame(state);
    }

    void test_h5_absence_and_ascension() {
        {
            H5Players p;
            p.ritual.reset();
            const auto before = p.encounter.random;
            const auto packets = p.tick(10000);
            expect(p.encounter.random == before && p.encounter.lord_id == H5Players::kLord &&
                       p.world.find_monster(H5Players::kLord)->hp == 100 && h5_clock_reads == 0 &&
                       packets == H5Packets{},
                   "absent optional ritual skips the entire encounter without clock or output");
            h5_observe("absent", p, packets, 10000);
        }
        for (bool expired : {false, true}) {
            H5Players p;
            p.world.monsters().clear();
            p.encounter.spirit_ids = {111, 222};
            p.encounter.spirit_max = 17;
            p.encounter.spawn_cursor = 9;
            p.prepare(DoggebiLordSkill::Fury);
            p.encounter.lord_despawn_ms = expired ? 10000 : 20000;
            const auto packets = p.tick(10000);
            expect(p.encounter.lord_id == 0 && p.encounter.lord_despawn_ms == 0 &&
                       !p.encounter.cast.active && p.encounter.random == 0x213u &&
                       p.encounter.spirit_ids == std::vector<std::uint32_t>{111, 222} &&
                       p.encounter.spirit_max == 17 && p.encounter.spawn_cursor == 9 &&
                       h5_clock_reads == 3,
                   "lost lord resets only controller fields and retains stale spirit bookkeeping");
            auto expected = std::vector<std::vector<std::uint8_t>>{
                protocol::Chatting::frame_s2c("Server",
                                              expired ? "The Doggebi Lord retreats from the throne."
                                                      : "The Doggebi Lord has fallen."),
                h5_map()};
            if (expired) {
                expected.insert(
                    expected.begin(),
                    protocol::Skill::frame_action_skill(H5Players::kLord, 8, H5Players::kLord));
            }
            expect(
                packets[0] == expected && packets[2].size() == (expired ? 1u : 0u),
                "expired missing lord still prepares before lookup and uses exact reset wire order");
            h5_observe(expired ? "missing-expired" : "missing-fallen", p, packets, 10000);
        }
        {
            H5Players p;
            p.encounter.lord_despawn_ms = 10000;
            for (int pulse = 0; pulse < 13; ++pulse) {
                const auto at = 10000u + static_cast<std::uint64_t>(pulse) * 5000u;
                const auto packets = p.tick(at);
                expect(
                    p.encounter.ascending && p.encounter.ascension_pulse_count == pulse + 1 &&
                        p.encounter.ascension_next_pulse_ms == at + 5000 &&
                        p.clients[0].cur_hp == 10000 - 2 * (pulse + 1),
                    "each due ordinary ascension pulse applies literal missing-definition damage2");
                const auto* lord = p.world.find_monster(H5Players::kLord);
                expect(lord && !lord->aggro && lord->aggro_player_id == 0 &&
                           lord->hostility_by_player.empty() &&
                           lord->reward_damage_by_player.empty(),
                       "ascension clears both combat hostility and reward attribution");
                expect(
                    p.clients[2].cur_hp == 10000 && p.clients[3].cur_hp == 10000 &&
                        p.clients[4].cur_hp == 10000 - 2 * (pulse + 1) && p.clients[5].cur_hp == 0,
                    "ascension damage captures map/phase and applies even to socketless live targets");
                h5_observe("ascension-pulse-" + std::to_string(pulse + 1), p, packets, at);
                const auto repeated = p.tick(at);
                expect(repeated == H5Packets{} && p.encounter.ascension_pulse_count == pulse + 1,
                       "repeating a due timestamp does not create a second ascension pulse");
                h5_observe("ascension-repeat-" + std::to_string(pulse + 1), p, repeated, at);
            }
            const auto packets = p.tick(75000);
            const std::vector<std::vector<std::uint8_t>> expected = {
                h5_hp(9968),
                protocol::Effect::frame_damaged(700, 6),
                protocol::Skill::frame_server(8, H5Players::kLord, H5Players::kLord, 1),
                frame_remove_monster(H5Players::kLord),
                protocol::Chatting::frame_s2c("Server",
                                              "The Doggebi Lord retreats from the throne."),
                h5_map()};
            expect(
                p.clients[0].cur_hp == 9968 && packets[0] == expected &&
                    !p.world.find_monster(H5Players::kLord) && p.encounter.lord_id == 0 &&
                    !p.encounter.ascending && p.encounter.ascension_pulse_count == 0,
                "final palsy pulse6 precedes fire/removal/reset and completes after13 ordinary pulses");
            h5_observe("ascension-final", p, packets, 75000);
        }
        {
            H5Players p;
            p.encounter.lord_despawn_ms = 10000;
            const auto packets = p.tick(900000);
            expect(p.encounter.ascension_pulse_count == 1 &&
                       p.encounter.ascension_next_pulse_ms == 905000 && p.clients[0].cur_hp == 9998,
                   "very late ascension starts one pulse anchored to now without catch-up");
            h5_observe("ascension-late", p, packets, 900000);
        }
    }

    void test_h5_casts_and_reaction() {
        const std::array<DoggebiLordSkill, 9> skills = {DoggebiLordSkill::Rage,
                                                        DoggebiLordSkill::Fury,
                                                        DoggebiLordSkill::Summon,
                                                        DoggebiLordSkill::GroundSlam,
                                                        DoggebiLordSkill::Teleport,
                                                        DoggebiLordSkill::SpiritAbsorb,
                                                        DoggebiLordSkill::MoonSkin,
                                                        DoggebiLordSkill::ProtectionOfMoon,
                                                        DoggebiLordSkill::Overrun};
        for (auto skill : skills) {
            H5Players p;
            p.prepare(skill, 10001);
            p.world.find_monster(H5Players::kLord)->doggebi_pending_reaction_damage = 123;
            const auto before = p.tick(10000);
            expect(p.encounter.cast.active && before == H5Packets{} &&
                       p.world.find_monster(H5Players::kLord)->doggebi_pending_reaction_damage == 0,
                   "prepared cast clears reaction damage before due without executing");
            h5_observe("cast-before-" + std::to_string(static_cast<int>(skill)), p, before, 10000);
            const auto packets = p.tick(10001);
            expect(!p.encounter.cast.active,
                   "executing one prepared spell cannot prepare a replacement in the same tick");
            const std::uint32_t expected_target =
                skill == DoggebiLordSkill::MoonSkin || skill == DoggebiLordSkill::ProtectionOfMoon
                    ? H5Players::kLord
                    : 700;
            const auto first_fire = protocol::Skill::frame_server(
                static_cast<std::uint8_t>(skill), H5Players::kLord, expected_target, 1);
            expect(!packets[0].empty() && packets[0].front() == first_fire,
                   "spell execution broadcasts fire before damage or teleport delivery");
            if (skill == DoggebiLordSkill::Overrun) {
                const std::array<std::uint32_t, 4> target_ids = {700, 701, 704, 705};
                bool ordered = packets[0].size() >= target_ids.size();
                for (std::size_t i = 0; ordered && i < target_ids.size(); ++i) {
                    ordered = packets[0][i] ==
                              protocol::Skill::frame_server(13, H5Players::kLord, target_ids[i], 1);
                }
                expect(ordered,
                       "Overrun fires once per captured target before any health delivery");
            }
            const int expected_hp = skill == DoggebiLordSkill::Rage         ? 9997
                                    : skill == DoggebiLordSkill::Fury       ? 9996
                                    : skill == DoggebiLordSkill::GroundSlam ? 9999
                                    : skill == DoggebiLordSkill::Overrun    ? 6873
                                                                            : 10000;
            expect(p.clients[0].cur_hp == expected_hp,
                   "prepared spell uses independent literal missing-definition damage golden");
            if (skill == DoggebiLordSkill::Summon) {
                expect(
                    p.encounter.spirit_max == 6 && p.encounter.spawn_cursor == 3 &&
                        p.encounter.spirit_ids.empty() && p.next_entity_id == 8000,
                    "missing spirit definition still advances every attempt and falls back to one fire");
            }
            if (skill == DoggebiLordSkill::MoonSkin ||
                skill == DoggebiLordSkill::ProtectionOfMoon) {
                const auto* lord = p.world.find_monster(H5Players::kLord);
                expect((skill == DoggebiLordSkill::MoonSkin ? lord->doggebi_absorb_left
                                                            : lord->doggebi_resist_all_left) == 180,
                       "protection casts set exactly180 seconds on their distinct state");
            }
            if (skill == DoggebiLordSkill::Teleport) {
                expect(p.encounter.random == 2729511614u && p.clients[0].tele_pending &&
                           p.clients[0].tele_x == 59444 && p.clients[0].tele_y == 9710 &&
                           p.clients[1].tele_x == 59298 && p.clients[1].tele_y == 9784 &&
                           p.clients[4].tele_x == 59388 && p.clients[4].tele_y == 9709 &&
                           p.clients[5].tele_x == 59339 && p.clients[5].tele_y == 9749 &&
                           !p.clients[2].tele_pending && !p.clients[3].tele_pending &&
                           p.clients[0].tele_z == 42 && p.clients[0].dialog_npc_id == 0 &&
                           !p.clients[0].portal_armed,
                       "teleport retains target order and includes dead/socketless captured peers");
            }
            h5_observe(
                "cast-execute-" + std::to_string(static_cast<int>(skill)), p, packets, 10001);
        }
        for (bool physical : {false, true}) {
            for (int damage : {99, 100}) {
                for (std::uint32_t seed : {0u, 3u}) {
                    H5Players p;
                    auto* lord = p.world.find_monster(H5Players::kLord);
                    lord->doggebi_pending_reaction_damage = damage;
                    lord->doggebi_pending_reaction_physical = physical;
                    p.encounter.random = seed;
                    const auto packets = p.tick(10000);
                    const bool selected = damage == 100 && seed == 0;
                    expect(
                        p.encounter.cast.active == selected &&
                            lord->doggebi_pending_reaction_damage == (damage == 99 ? 99 : 0),
                        "reaction threshold99/100 and authored successful/failed rolls stay exact");
                    if (selected) {
                        expect(
                            p.encounter.cast.skill == (physical
                                                           ? DoggebiLordSkill::MoonSkin
                                                           : DoggebiLordSkill::ProtectionOfMoon) &&
                                p.encounter.cast.target_id == H5Players::kLord &&
                                p.encounter.cast.execute_at_ms == 11000 &&
                                lord->attack_cooldown == 1,
                            "reaction selects the physical/magic shield and delays only basic attack");
                    }
                    h5_observe("reaction-" + std::to_string(physical) + "-" +
                                   std::to_string(damage) + "-" + std::to_string(seed),
                               p,
                               packets,
                               10000);
                }
            }
        }
        for (int hp : {50, 70, 75, 76}) {
            H5Players p;
            p.world.find_monster(H5Players::kLord)->hp = hp;
            p.encounter.timers = {};
            p.encounter.timers.protection_ms = 7321;
            p.encounter.random = 0;
            const auto packets = p.tick(10000);
            const auto selected = hp <= 70   ? DoggebiLordSkill::SpiritAbsorb
                                  : hp == 75 ? DoggebiLordSkill::Summon
                                             : DoggebiLordSkill::GroundSlam;
            expect(
                p.encounter.cast.active && p.encounter.cast.skill == selected &&
                    p.encounter.timers.ground_slam_ms == 42000 &&
                    p.encounter.timers.rage_or_fury_ms == 20000 &&
                    p.encounter.timers.overrun_ms == 15000 &&
                    p.encounter.timers.protection_ms == 7321 &&
                    p.encounter.random == (hp <= 70 ? 0u : 1013904223u),
                "priority suppresses later random draws while all due cooldown assignments still happen");
            h5_observe("priority-hp-" + std::to_string(hp), p, packets, 10000);
        }
    }

    void test_h5_spirits_and_teleport_edges() {
        {
            H5Players p;
            p.prepare(DoggebiLordSkill::SpiritAbsorb);
            p.world.find_monster(H5Players::kLord)->hp = 90;
            auto corpse = H5Players::monster(4100, H5Players::kX + 32, H5Players::kY);
            corpse.hp = 0;
            p.world.monsters().push_back(corpse);
            p.world.monsters().push_back(
                H5Players::monster(4101, H5Players::kX + 64, H5Players::kY));
            p.encounter.spirit_ids = {9999, 4100, 4101};
            p.encounter.spirit_max = 3;
            for (auto& c : p.clients) {
                c.known_monsters.insert(4100);
            }
            const auto packets = p.tick(10000);
            expect(p.encounter.spirit_ids == std::vector<std::uint32_t>{4101} &&
                       p.encounter.spirit_max == 2 && !p.world.find_monster(4100) &&
                       p.world.find_monster(4101) &&
                       p.world.find_monster(H5Players::kLord)->hp == 95,
                   "missing spirits prune cap but the first present corpse remains absorbable");
            const std::vector<std::vector<std::uint8_t>> expected = {
                protocol::Skill::frame_server(5, H5Players::kLord, 700, 1),
                frame_remove_monster(4100)};
            expect(packets[0] == expected,
                   "spirit absorption fire precedes known-set removal notification");
            h5_observe("absorb-corpse", p, packets, 10000);
        }
        for (int mode = 0; mode < 4; ++mode) {
            H5Players p;
            p.definitions(true);
            p.prepare(DoggebiLordSkill::Summon);
            if (mode == 1) {
                p.world.monsters().push_back(
                    H5Players::monster(4200, H5Players::kX + 5 * map::kTileSize, H5Players::kY));
            }
            if (mode == 3) {
                p.world.monsters().push_back(
                    H5Players::monster(4400, H5Players::kX + 5 * map::kTileSize, H5Players::kY));
                p.world.monsters().push_back(H5Players::monster(
                    4401, H5Players::kX + 4 * map::kTileSize, H5Players::kY + 3 * map::kTileSize));
                p.world.monsters().push_back(H5Players::monster(
                    4402, H5Players::kX + 3 * map::kTileSize, H5Players::kY + 4 * map::kTileSize));
            }
            if (mode == 2) {
                p.encounter.spirit_max = 23;
                for (std::uint32_t id = 4300; id < 4322; ++id) {
                    p.world.monsters().push_back(H5Players::monster(id, 6000, 6000, 4));
                    p.encounter.spirit_ids.push_back(id);
                }
            }
            auto& monsters = p.world.monsters();
            while (monsters.size() < monsters.capacity()) {
                monsters.push_back(H5Players::monster(
                    5000 + static_cast<std::uint32_t>(monsters.size()), 7000, 7000, 4));
            }
            const auto capacity = monsters.capacity();
            const int successes = mode == 0 ? 3 : mode == 3 ? 0 : 2;
            const auto packets = p.tick(10000, successes);
            expect(
                (successes > 0 ? monsters.capacity() > capacity
                               : monsters.capacity() == capacity) &&
                    p.world.find_monster(H5Players::kLord) &&
                    p.next_entity_id == 8000u + static_cast<unsigned>(successes) &&
                    p.encounter.spawn_cursor == static_cast<std::size_t>(mode == 2 ? 2 : 3) &&
                    p.encounter.spirit_max == (mode == 2 ? 24 : 6),
                "successful summon relocates safely while fully blocked attempts preserve capacity and allocator");
            expect(packets[0].size() == static_cast<std::size_t>(successes == 0 ? 1 : successes),
                   "summon broadcasts exactly one fire for each successful allocated spirit");
            if (successes == 0) {
                expect(packets[0].front() ==
                           protocol::Skill::frame_server(2, H5Players::kLord, 700, 1),
                       "all blocked summon attempts fall back to the ordinary single fire packet");
            }
            for (int i = 0; i < successes; ++i) {
                expect(packets[0][static_cast<std::size_t>(i)] ==
                           protocol::Skill::frame_server(2, H5Players::kLord, 8000 + i, 1),
                       "summon fire targets retain successful allocation order");
            }
            h5_observe("summon-" + std::to_string(mode), p, packets, 10000);
        }
        {
            H5Players p;
            p.definitions(true);
            p.prepare(DoggebiLordSkill::Rage);
            const auto packets = p.tick(10000);
            expect(p.clients[0].cur_hp == 9974 && p.clients[1].cur_hp == 9974 &&
                       p.encounter.random == 1897766998u,
                   "owned level1 INT8 magic3-to4 Rage deals the independent26 damage golden");
            expect(packets[0] ==
                       std::vector<std::vector<std::uint8_t>>{
                           protocol::Skill::frame_server(0, H5Players::kLord, 700, 1),
                           h5_hp(9974),
                           protocol::Effect::frame_damaged(700, 26)},
                   "loaded-definition damage retains exact fire then HP then damaged order");
            h5_observe("loaded-definition-rage", p, packets, 10000);
        }
        {
            H5Players p;
            p.prepare(DoggebiLordSkill::GroundSlam);
            p.clients[0].godmode = true;
            apply_buff(p.clients[1], SkillBuffId::KnightPerfectDefense, 100);
            const auto packets = p.tick(10000);
            expect(p.clients[0].cur_hp == 10000 && p.clients[1].cur_hp == 10000 &&
                       p.clients[0].control_until_ms == 22000 &&
                       p.clients[1].control_until_ms == 22000 &&
                       p.clients[4].control_until_ms == 22000,
                   "godmode and perfect defense suppress HP damage but not GroundSlam control");
            const auto message = protocol::Chatting::frame_s2c(
                "Server", "The Doggebi Lord's ground slam stuns you for 12 seconds.");
            expect(packets[0].size() == 4 && packets[0][1] == h5_hp(10000) &&
                       packets[0][2] == protocol::Effect::frame_damaged(700, 0) &&
                       packets[0][3] == message,
                   "zero-damage GroundSlam still sends health, damaged0 and stun text in order");
            h5_observe("ground-slam-zero", p, packets, 10000);
        }
        for (bool ascending : {false, true}) {
            H5Players p;
            p.prepare(DoggebiLordSkill::Fury);
            p.encounter.ascending = ascending;
            p.encounter.ascension_next_pulse_ms = 20000;
            p.world.find_monster(H5Players::kLord)->aggro = ascending;
            p.world.find_monster(H5Players::kLord)->doggebi_pending_reaction_damage = 123;
            const auto packets = p.tick(10000);
            expect(
                p.encounter.cast.active && packets == H5Packets{} &&
                    p.world.find_monster(H5Players::kLord)->doggebi_pending_reaction_damage == 123,
                "nonaggro or ascending lord retains the prepared cast and reaction outside combat");
            h5_observe(
                ascending ? "cast-skipped-ascending" : "cast-skipped-nonaggro", p, packets, 10000);
        }
        {
            H5Players p;
            p.prepare(DoggebiLordSkill::Teleport);
            p.encounter.cast.target_id = 99999;
            const auto packets = p.tick(10000);
            expect(p.encounter.random == 0x213u && !p.clients[0].tele_pending &&
                       packets[0] ==
                           std::vector<std::vector<std::uint8_t>>{
                               protocol::Skill::frame_server(4, H5Players::kLord, 99999, 1)},
                   "missing teleport primary fires but consumes no destination or offset rolls");
            h5_observe("teleport-no-primary", p, packets, 10000);
        }
        {
            H5Players p;
            p.prepare(DoggebiLordSkill::Teleport);
            p.clients[0].beacon_active_until_ms = 1;
            const auto packets = p.tick(10000);
            expect(p.clients[0].beacon_active_until_ms == 0 && p.clients[0].tele_pending &&
                       packets[0].size() == 3 &&
                       packets[0][0][2] == static_cast<std::uint8_t>(protocol::S2C::Skill) &&
                       packets[0][1][2] == static_cast<std::uint8_t>(protocol::S2C::TalentTree) &&
                       packets[0][2][2] == static_cast<std::uint8_t>(protocol::S2C::Teleport),
                   "teleport ends an active Beacon and sends its state before the teleport frame");
            h5_observe("teleport-beacon", p, packets, 10000);
        }
    }

    void test_h5_ritual_and_presentation() {
        {
            H5Players p;
            p.encounter.lord_id = 0;
            expect(p.ritual->activate(6000, 10000) == DoggebiRitualActivation::Accepted &&
                       p.ritual->activate(6003, 10000) == DoggebiRitualActivation::Accepted,
                   "two incomplete monuments start with equal owned deadlines");
            const auto before = p.tick(609999);
            expect(p.ritual->active_mask() == 9 && before == H5Packets{},
                   "incomplete monuments remain active one millisecond before expiry");
            h5_observe("monuments-before", p, before, 609999);
            const auto packets = p.tick(610000);
            const std::vector<std::vector<std::uint8_t>> expected = {
                protocol::ItemMonster::frame_gstate(6000, 0),
                protocol::ItemMonster::frame_gstate(6003, 0),
                h5_map(),
                protocol::Chatting::frame_s2c(
                    "Server", "A Doggebi monument fades; the incomplete ritual weakens.")};
            expect(p.ritual->active_mask() == 0 && packets[0] == expected && h5_clock_reads == 3,
                   "simultaneous expiry broadcasts ascending slots before state and fade text");
            h5_observe("monuments-equal", p, packets, 610000);
        }
        for (int mode = 0; mode < 3; ++mode) {
            H5Players p;
            p.encounter.lord_id = 0;
            p.world.monsters().clear();
            if (mode == 1) {
                p.definitions(true);
            }
            if (mode == 2) {
                p.encounter.lord_id = H5Players::kLord;
                auto lord = H5Players::monster(H5Players::kLord, H5Players::kX, H5Players::kY);
                lord.aggro = false;
                p.world.monsters().push_back(lord);
            }
            p.activate_all(10000);
            const std::array<std::uint64_t, 4> transitions = {110000, 250000, 280000, 310000};
            for (std::size_t i = 0; i < transitions.size(); ++i) {
                const auto packets = p.tick(transitions[i], mode == 1 && i == 3 ? 1 : 0);
                if (i < 3) {
                    expect(static_cast<int>(p.ritual->phase()) == static_cast<int>(i + 2),
                           "ritual advances exactly one ordered earthquake phase at each deadline");
                } else {
                    expect(
                        p.ritual->phase() == DoggebiRitualPhase::Idle &&
                            p.ritual->active_mask() == 0,
                        "spawn success, missing definition and prior lord all reset ritual once");
                    if (mode == 1) {
                        const auto* lord = p.world.find_monster(8000);
                        expect(
                            p.encounter.lord_id == 8000 && p.next_entity_id == 8001 &&
                                p.encounter.lord_despawn_ms == 3910000 && lord &&
                                lord->aggressive && lord->spawn_protection_left == 10 &&
                                p.encounter.spirit_ids.empty() && p.encounter.spirit_max == 0 &&
                                p.encounter.spawn_cursor == 0 && p.encounter.random == 0x213u,
                            "ritual successful spawn installs exact lifetime/protection and fresh controller");
                    } else {
                        expect(p.next_entity_id == 8000 &&
                                   p.encounter.lord_id == (mode == 2 ? H5Players::kLord : 0),
                               "ritual failure or existing lord cannot allocate another boss");
                    }
                    std::size_t first_clear = packets[0].size();
                    for (std::size_t at = 0; at < packets[0].size(); ++at) {
                        if (packets[0][at] == protocol::ItemMonster::frame_gstate(6000, 0)) {
                            first_clear = at;
                            break;
                        }
                    }
                    bool ordered = first_clear + 8 <= packets[0].size();
                    for (std::size_t slot = 0; ordered && slot < 8; ++slot) {
                        ordered = packets[0][first_clear + slot] ==
                                  protocol::ItemMonster::frame_gstate(6000 + slot, 0);
                    }
                    expect(ordered,
                           "every completed/failed ritual clears all eight slots in exact order");
                }
                h5_observe("ritual-" + std::to_string(mode) + "-" + std::to_string(i),
                           p,
                           packets,
                           transitions[i]);
            }
            const auto id = p.next_entity_id;
            const auto repeated = p.tick(310000);
            expect(p.next_entity_id == id && p.ritual->phase() == DoggebiRitualPhase::Idle,
                   "repeating the final ritual timestamp cannot spawn again");
            h5_observe("ritual-repeat-" + std::to_string(mode), p, repeated, 310000);
        }
        {
            H5Players p;
            p.world.find_monster(H5Players::kLord)->aggro = false;
            p.encounter.lord_despawn_ms = 6001;
            p.encounter.last_countdown_broadcast_ms = 0;
            const auto packets = p.tick(4000, 0, {5000, 5001, 6000});
            expect(
                h5_clock_reads == 3 &&
                    packets[0] == std::vector<std::vector<std::uint8_t>>{h5_map(
                                      0, protocol::MapStateTimer::Lord, 2)} &&
                    packets[1] == std::vector<std::vector<std::uint8_t>>{h5_map(
                                      0, protocol::MapStateTimer::Lord, 1)} &&
                    packets[5] == std::vector<std::vector<std::uint8_t>>{h5_map(
                                      0, protocol::MapStateTimer::Lord, 1)} &&
                    packets[2].empty(),
                "presentation reads separately per eligible peer with exact lazy clock and ceil boundaries");
            h5_observe("clock-per-recipient", p, packets, 4000);
        }
        for (const auto last : {9001u, 9000u, 11000u}) {
            H5Players p;
            p.world.find_monster(H5Players::kLord)->aggro = false;
            p.encounter.last_countdown_broadcast_ms = last;
            const auto packets = p.tick(10000);
            expect(h5_clock_reads == (last == 9001 ? 0u : 3u) &&
                       p.encounter.last_countdown_broadcast_ms == (last == 9001 ? last : 10000u),
                   "countdown999/1000 and unsigned future-last behavior remain unchanged");
            h5_observe("countdown-" + std::to_string(last), p, packets, 10000);
        }
        {
            H5Players p;
            p.encounter.lord_id = 0;
            for (auto& c : p.clients) {
                c.doggebi_state_sent = false;
            }
            p.clients[2].doggebi_state_sent = true;
            const auto packets = p.tick(10000);
            expect(h5_clock_reads == 3 && p.clients[0].doggebi_state_sent &&
                       p.clients[1].doggebi_state_sent && p.clients[5].doggebi_state_sent &&
                       !p.clients[2].doggebi_state_sent && !p.clients[3].doggebi_state_sent &&
                       !p.clients[4].doggebi_state_sent,
                   "initial per-client state respects map/phase/socket without a new alive gate");
            h5_observe("initial-state-flags", p, packets, 10000);
        }
    }

    void test_h5_damage_and_late_boundaries() {
        for (int mode = 0; mode < 4; ++mode) {
            H5Players p;
            p.prepare(DoggebiLordSkill::Rage);
            if (mode < 2) {
                p.clients[0].res_fire = mode == 0 ? 50 : 100;
            } else {
                apply_buff(p.clients[0], SkillBuffId::KnightPerfectDefense, mode == 2 ? 50 : 100);
            }
            const auto packets = p.tick(10000);
            const int dealt = mode == 0 ? 1 : mode == 2 ? 2 : 0;
            expect(
                p.clients[0].cur_hp == 10000 - dealt && packets[0].size() == 3 &&
                    packets[0][1] == h5_hp(10000 - dealt) &&
                    packets[0][2] == protocol::Effect::frame_damaged(700, dealt),
                "fire resistance and perfect-defense reduction preserve independent integer goldens");
            h5_observe("damage-reduction-" + std::to_string(mode), p, packets, 10000);
        }
        {
            H5Players p;
            p.prepare(DoggebiLordSkill::Rage);
            p.clients[0].cur_hp = 1;
            p.clients[1].px = H5Players::kX + 7 * map::kTileSize + 31;
            p.clients[4].px = H5Players::kX + 8 * map::kTileSize;
            const auto packets = p.tick(10000);
            expect(
                p.clients[0].cur_hp == 0 && p.clients[0].village_revival_deadline_ms == 0 &&
                    p.clients[1].cur_hp == 9997 && p.clients[4].cur_hp == 10000 &&
                    p.world.find_monster(H5Players::kLord)->reward_damage_by_player.at(700) == 23,
                "tile-rectangle boundary and lethal skill preserve the absence of generic reward/revival hooks");
            h5_observe("rage-boundary-lethal", p, packets, 10000);
        }
        {
            H5Players p;
            p.encounter.lord_despawn_ms = 10000;
            p.encounter.ascending = true;
            p.encounter.ascension_next_pulse_ms = 10000;
            p.encounter.ascension_pulse_count = 13;
            p.clients[1].px = H5Players::kX + 40 * map::kTileSize + 31;
            p.clients[4].px = H5Players::kX + 41 * map::kTileSize;
            const auto packets = p.tick(10000);
            expect(p.clients[0].cur_hp == 9994 && p.clients[1].cur_hp == 9994 &&
                       p.clients[4].cur_hp == 10000 && p.encounter.lord_id == 0,
                   "final palsy captures inclusive40-tile boundary and excludes the next tile");
            h5_observe("ascension-final-radius", p, packets, 10000);
        }
        {
            H5Players p;
            p.prepare(DoggebiLordSkill::Rage, 5000);
            const auto packets = p.tick(20000);
            expect(!p.encounter.cast.active && p.clients[0].cur_hp == 9997 &&
                       p.encounter.random == 1897766998u,
                   "late prepared cast executes once without preparing again or catching up");
            h5_observe("cast-late", p, packets, 20000);
        }
        {
            H5Players p;
            p.encounter.lord_id = 0;
            p.activate_all(10000);
            const auto packets = p.tick(900000);
            expect(p.ritual->phase() == DoggebiRitualPhase::QuakeTwo &&
                       p.ritual->remaining_to_spawn_ms(900000) == 200000,
                   "late ritual advances one phase and anchors its new deadline to now");
            h5_observe("ritual-late", p, packets, 900000);
            const auto repeated = p.tick(900000);
            expect(repeated == H5Packets{} && p.ritual->phase() == DoggebiRitualPhase::QuakeTwo,
                   "same-time ritual repeat cannot catch up another phase");
            h5_observe("ritual-late-repeat", p, repeated, 900000);
        }
    }

    void test_h5_secondary_priority_and_profile() {
        for (std::uint32_t seed : {3u, 0x213u}) {
            H5Players p;
            p.encounter.random = seed;
            p.encounter.timers = {};
            p.encounter.timers.protection_ms = 7321;
            const auto packets = p.tick(10000);
            expect(
                p.encounter.cast.active &&
                    p.encounter.cast.skill ==
                        (seed == 3 ? DoggebiLordSkill::Overrun : DoggebiLordSkill::Fury) &&
                    p.encounter.random == (seed == 3 ? 3752335016u : 1899518909u) &&
                    p.encounter.timers.ground_slam_ms == 42000 &&
                    p.encounter.timers.rage_or_fury_ms == 20000 &&
                    p.encounter.timers.overrun_ms == 15000,
                "authored failed higher-priority rolls expose exact later selection and draw count");
            h5_observe("secondary-priority-" + std::to_string(seed), p, packets, 10000);
        }
        {
            H5Players p;
            p.encounter.random = 0;
            p.world.find_monster(H5Players::kLord)->hp = 50;
            p.encounter.timers.teleport_ms = 0;
            const auto packets = p.tick(10000);
            expect(
                p.encounter.cast.skill == DoggebiLordSkill::Teleport && p.encounter.cast.active &&
                    p.encounter.timers.teleport_ms == 93000 && p.encounter.random == 1013904223u,
                "due teleport can prepare below50 percent when the prior absorb slot is not due");
            h5_observe("prepare-teleport", p, packets, 10000);
        }
        {
            H5Players p;
            p.prepare(DoggebiLordSkill::Rage);
            auto& c = p.clients[0];
            c.collection_world = &p.world;
            c.collection_pve = false;
            c.collection_pvp_until_ms = 0;
            c.str = c.hth = c.intl = c.wis = c.dex = 10;
            const auto packets = p.tick(10000);
            expect(
                c.collection_pve && c.max_hp != 10000 && c.res_fire == 1 &&
                    c.cur_hp == c.max_hp - 2 && packets[0].size() > 3 &&
                    packets[0][packets[0].size() - 2] == h5_hp(c.cur_hp) &&
                    packets[0].back() == protocol::Effect::frame_damaged(700, 2),
                "contact profile recomputation precedes skill damage without rebinding its real clock");
            h5_observe("contact-profile", p, packets, 10000);
        }
    }

    void test_h5_loaded_magic_and_absorb_selection() {
        for (std::uint32_t seed : {1u, 0u}) {
            H5Players p;
            p.definitions(false);
            const auto* definition = p.world.find_def(213);
            expect(definition && definition->level == 1 && definition->intelligence == 8 &&
                       definition->min_magic == 3 && definition->max_magic == 4,
                   "owned ascension definition has independent revised magic endpoints3 and4");
            p.encounter.random = seed;
            p.encounter.lord_despawn_ms = 10000;
            const auto packets = p.tick(10000);
            const int damage = seed == 1 ? 15 : 16;
            expect(
                p.encounter.random == (seed == 1 ? 1015568748u : 1013904223u) &&
                    p.encounter.ascending && p.encounter.ascension_pulse_count == 1 &&
                    p.encounter.ascension_next_pulse_ms == 15000 &&
                    p.clients[0].cur_hp == 10000 - damage &&
                    p.clients[1].cur_hp == 10000 - damage &&
                    p.clients[4].cur_hp == 10000 - damage && p.clients[2].cur_hp == 10000 &&
                    p.clients[3].cur_hp == 10000 && p.clients[5].cur_hp == 0,
                "loaded ascension samples both inclusive magic endpoints with exact LCG and damage");
            const auto action =
                protocol::Skill::frame_action_skill(H5Players::kLord, 8, H5Players::kLord);
            const H5Packets expected = {{
                {action, h5_hp(10000 - damage), protocol::Effect::frame_damaged(700, damage)},
                {action, h5_hp(10000 - damage), protocol::Effect::frame_damaged(701, damage)},
                {action},
                {},
                {},
                {action},
            }};
            expect(packets == expected && h5_clock_reads == 0,
                   "loaded ascension keeps exact prepare then HP then effect and recipient order");
            h5_observe(seed == 1 ? "loaded-ascension-minimum" : "loaded-ascension-maximum",
                       p,
                       packets,
                       10000);
        }
        {
            H5Players p;
            p.definitions(false);
            p.prepare(DoggebiLordSkill::Fury);
            const auto packets = p.tick(10000);
            expect(p.encounter.random == 1897766998u && !p.encounter.cast.active &&
                       p.clients[0].cur_hp == 9965 && p.clients[1].cur_hp == 9965 &&
                       p.clients[4].cur_hp == 9965 && p.clients[2].cur_hp == 10000 &&
                       p.clients[3].cur_hp == 10000 && p.clients[5].cur_hp == 0,
                   "loaded level1 INT8 magic3 Fury uses the independent35 damage golden");
            const auto fire = protocol::Skill::frame_server(1, H5Players::kLord, 700, 1);
            const H5Packets expected = {{
                {fire, h5_hp(9965), protocol::Effect::frame_damaged(700, 35)},
                {fire, h5_hp(9965), protocol::Effect::frame_damaged(701, 35)},
                {fire},
                {},
                {},
                {fire},
            }};
            expect(packets == expected && h5_clock_reads == 0,
                   "loaded Fury sends fire then HP then damage to the exact eligible readers");
            h5_observe("loaded-definition-fury", p, packets, 10000);
        }
        {
            H5Players p;
            p.prepare(DoggebiLordSkill::SpiritAbsorb);
            p.world.find_monster(H5Players::kLord)->hp = 99;
            p.world.monsters().push_back(
                H5Players::monster(4200, H5Players::kX + 32, H5Players::kY, 4));
            p.world.monsters().push_back(
                H5Players::monster(4201, H5Players::kX + 6 * map::kTileSize, H5Players::kY));
            p.world.monsters().push_back(
                H5Players::monster(4202, H5Players::kX + 5 * map::kTileSize + 31, H5Players::kY));
            p.world.monsters().push_back(
                H5Players::monster(4203, H5Players::kX + 1, H5Players::kY));
            p.encounter.spirit_ids = {4200, 4201, 4202, 4203};
            p.encounter.spirit_max = 4;
            for (auto& peer : p.clients) {
                peer.known_monsters.insert(4202);
            }
            const auto monster_capacity = p.world.monsters().capacity();
            const auto tracked_capacity = p.encounter.spirit_ids.capacity();
            const auto packets = p.tick(10000);
            const auto* lord = p.world.find_monster(H5Players::kLord);
            expect(
                lord && lord->hp == 100 && lord->max_hp == 100 && !p.world.find_monster(4202) &&
                    p.world.find_monster(4200) && p.world.find_monster(4201) &&
                    p.world.find_monster(4203) &&
                    p.encounter.spirit_ids == std::vector<std::uint32_t>{4200, 4201, 4203},
                "absorption skips foreign and distant entries before the first inclusive-boundary spirit");
            expect(
                p.world.find_monster(4200)->map == 4 &&
                    p.world.find_monster(4201)->x == H5Players::kX + 6 * map::kTileSize &&
                    p.world.find_monster(4203)->hp == 100 && p.world.monsters().size() == 4 &&
                    p.world.monsters().capacity() == monster_capacity &&
                    p.encounter.spirit_ids.capacity() == tracked_capacity &&
                    p.encounter.spirit_max == 4 && p.next_entity_id == 8000 &&
                    p.encounter.random == 0x213u,
                "one capped heal and erase preserve retained spirits, capacities, cap and allocator");
            const auto fire = protocol::Skill::frame_server(5, H5Players::kLord, 700, 1);
            const auto remove = frame_remove_monster(4202);
            const H5Packets expected = {{
                {fire, remove},
                {fire, remove},
                {fire, remove},
                {},
                {},
                {fire, remove},
            }};
            expect(
                packets == expected && p.clients[0].known_monsters.count(4202) == 0 &&
                    p.clients[1].known_monsters.count(4202) == 0 &&
                    p.clients[2].known_monsters.count(4202) == 0 &&
                    p.clients[3].known_monsters.count(4202) == 1 &&
                    p.clients[4].known_monsters.count(4202) == 1 &&
                    p.clients[5].known_monsters.count(4202) == 0 && h5_clock_reads == 0,
                "ordered absorption fire and remove retain phase and socket knowledge boundaries");
            h5_observe("absorb-ordered-ineligible", p, packets, 10000);
        }
    }

    void test_h5_doggebi() {
        test_h5_absence_and_ascension();
        test_h5_casts_and_reaction();
        test_h5_spirits_and_teleport_edges();
        test_h5_ritual_and_presentation();
        test_h5_damage_and_late_boundaries();
        test_h5_secondary_priority_and_profile();
        test_h5_loaded_magic_and_absorb_selection();
    }

#if defined(OPENKAL_ARCHER_BASELINE_BRIDGE)
    using H6Hit = h6_baseline::ArcherTickHit;
#else
    using H6Hit = ArcherTickHit;
#endif
    bool h6_snapshot = false;
    using H6Packets = std::array<std::vector<std::vector<std::uint8_t>>, 6>;
    struct H6Result {
        std::vector<H6Hit> hits;
        H6Packets packets;
        int next_random = 0;
    };
    template <typename... T> void h6_fields(std::ostream& out, const T&... values) {
        ((out << values << ','), ...);
        out << '\n';
    }
    template <typename Map> void h6_map(std::ostream& out, std::string_view name, const Map& map) {
        std::vector<typename Map::key_type> keys;
        for (const auto& item : map) {
            keys.push_back(item.first);
        }
        std::sort(keys.begin(), keys.end());
        out << name;
        for (const auto& key : keys) {
            out << ',' << key << ':' << map.at(key);
        }
        out << '\n';
    }
    struct H6Players {
        GameWorld world;
        std::vector<ClientSession> clients{6};
        std::array<RecordingSession*, 6> outputs{};
        std::mutex mutex;
        DebugDummyPlayers dummies;
        std::atomic<std::uint32_t> next_iid{9000};
        std::unique_ptr<IDataStore> store;
        std::vector<ArcherTimedDamage> statuses;
        std::vector<ArcherPersistentArea> areas;
        std::vector<ArcherArrowExplosion> explosions;
        std::array<bool, 6> bracketed_deadline{};
        std::string root;
        static constexpr int kX = 2048;
        static constexpr int kY = 2048;
        H6Players() {
            for (std::size_t i = 0; i < clients.size(); ++i) {
                auto& c = clients[i];
                c.phase = i == 3 ? SessionPhase::WaitConnect : SessionPhase::Playing;
                c.player_id = 700 + static_cast<std::uint32_t>(i);
                c.map = i == 2 ? 1 : 0;
                c.px = kX + static_cast<int>(i) * 8;
                c.py = kY;
                c.level = 1;
                c.clazz = 2;
                c.stats_initialized = true;
                c.collection_pve = false;
                c.collection_world = nullptr;
                c.cur_hp = i == 5 ? 0 : 1000;
                c.max_hp = 1000;
                c.cur_mp = c.max_mp = 100;
                c.min_attack = c.max_attack = 10;
                c.hit = 1000;
                c.known_monsters = {4000, 5000};
                if (i != 4) {
                    auto recorder = std::make_unique<RecordingSession>();
                    outputs[i] = recorder.get();
                    c.sock = std::make_unique<HostConnection>(std::move(recorder));
                }
            }
            world.monsters().push_back(monster(4000, kX + 24, kY));
        }
        ~H6Players() {
            if (store) {
                store->close();
            }
            if (!root.empty()) {
                std::filesystem::remove_all(root);
            }
        }
        static MonsterEntity monster(std::uint32_t id, int x, int y, int map = 0) {
            MonsterEntity m;
            m.id = id;
            m.def_index = 1;
            m.name = "Owned H6 target";
            m.x = m.target_x = x;
            m.y = m.target_y = y;
            m.map = map;
            m.hp = m.max_hp = 1000;
            return m;
        }
        ArcherTimedDamage status(bool player = true, int skill = 17, int damage = 20) const {
            ArcherTimedDamage s;
            s.owner_id = 700;
            s.target_id = player ? 701 : 4000;
            s.skill_index = skill;
            s.skill_level = 1;
            s.target_is_player = player;
            s.damage = damage;
            s.ticks_remaining = 2;
            s.tick_ms = 1000;
            s.owner_x = clients[0].px;
            s.owner_y = clients[0].py;
            s.expire_ms = 20000;
            s.next_tick_ms = 10000;
            return s;
        }
        ArcherPersistentArea area(int skill = 42) {
            auto controller = monster(5000, kX, kY);
            controller.race = static_cast<int>(protocol::MonsterRace::Magic);
            world.monsters().push_back(controller);
            ArcherPersistentArea a;
            a.owner_id = 700;
            a.controller_id = 5000;
            a.skill_index = skill;
            a.skill_level = 1;
            a.map = 0;
            a.x = kX;
            a.y = kY;
            a.damage = 7;
            a.pvp_damage = 5;
            a.ticks_remaining = 2;
            a.radius = -999;
            a.tick_ms = 1000;
            a.owner_x = kX;
            a.owner_y = kY;
            a.expire_ms = 20000;
            a.next_tick_ms = 10000;
            return a;
        }
        H6Packets drain() {
            H6Packets packets;
            for (std::size_t i = 0; i < clients.size(); ++i) {
                if (!clients[i].sock) {
                    continue;
                }
                for (unsigned round = 0; round < 128 && clients[i].sock->pending_bytes(); ++round) {
                    const auto before = clients[i].sock->pending_bytes();
                    expect(clients[i].sock->flush(POLLIN | POLLOUT), "H6 recorder flush succeeds");
                    expect(clients[i].sock->pending_bytes() < before,
                           "H6 complete packet drain makes bounded byte progress");
                }
                expect(clients[i].sock->pending_bytes() == 0,
                       "H6 captures all deferred wire bytes");
                packets[i] = std::move(outputs[i]->packets);
                outputs[i]->packets.clear();
            }
            return packets;
        }
        H6Result tick(std::uint64_t now, int expected_draws = -1) {
            H6Result result;
            constexpr unsigned seed = 947;
            std::srand(seed);
            for (int i = 0; i < std::max(0, expected_draws); ++i) {
                (void) std::rand();
            }
            const int expected_next = std::rand();
            std::srand(seed);
            std::array<std::uint64_t, 6> previous{};
            for (std::size_t i = 0; i < clients.size(); ++i) {
                previous[i] = clients[i].collection_pvp_until_ms;
            }
            const auto before = steady_ms_now();
#if defined(OPENKAL_ARCHER_BASELINE_BRIDGE)
            h6_baseline::Bridge bridge(
                clients, world, mutex, dummies, next_iid, store, statuses, areas, explosions);
            bridge.produce(now, result.hits);
            bridge.deliver(result.hits);
#else
            tick_archer_timed_damage(clients, world, mutex, statuses, now, result.hits);
            tick_archer_areas(clients, world, mutex, areas, statuses, now, result.hits);
            tick_archer_explosions(clients, dummies, world, mutex, explosions, now, result.hits);
            deliver_archer_tick_hits(clients, world, mutex, next_iid, store.get(), result.hits);
#endif
            const auto after = steady_ms_now();
            for (std::size_t i = 0; i < clients.size(); ++i) {
                const auto deadline = clients[i].collection_pvp_until_ms;
                const bool changed = deadline != previous[i];
                expect(!changed || (deadline >= before + 30000 && deadline <= after + 30000),
                       "H6 touched PvP deadline remains inside explicit real-clock bounds");
                bracketed_deadline[i] = bracketed_deadline[i] || changed;
            }
            result.next_random = std::rand();
            expect(expected_draws < 0 || result.next_random == expected_next,
                   "H6 authored no-random paths preserve the next same-runtime draw");
            // Preserve previous producer-before-delivery packet order; never flush between them.
            result.packets = drain();
            return result;
        }
    };
    void h6_observe(std::string_view name, const H6Players& p, const H6Result& r) {
        if (!h6_snapshot) {
            return;
        }
        std::ostringstream out;
        out << std::setprecision(17);
        h6_fields(out, "h6", name, p.next_iid.load(), r.next_random);
        for (const auto& s : p.statuses) {
            h6_fields(out,
                      "status",
                      s.owner_id,
                      s.target_id,
                      s.skill_index,
                      s.skill_level,
                      s.target_is_player,
                      s.damage,
                      s.ticks_remaining,
                      s.tick_ms,
                      s.owner_x,
                      s.owner_y,
                      s.cancel_on_owner_move,
                      s.nonlethal,
                      s.expire_ms,
                      s.next_tick_ms);
        }
        for (const auto& a : p.areas) {
            h6_fields(out,
                      "area",
                      a.owner_id,
                      a.controller_id,
                      a.skill_index,
                      a.skill_level,
                      a.map,
                      a.x,
                      a.y,
                      a.damage,
                      a.pvp_damage,
                      a.ticks_remaining,
                      a.radius,
                      a.tick_ms,
                      a.owner_x,
                      a.owner_y,
                      a.cancel_on_move,
                      a.expire_ms,
                      a.next_tick_ms);
        }
        for (const auto& b : p.explosions) {
            h6_fields(out,
                      "explosion",
                      b.owner_id,
                      b.target_id,
                      b.skill_level,
                      b.map,
                      b.target_is_player,
                      b.explode_ms);
        }
        for (const auto& h : r.hits) {
            h6_fields(out,
                      "hit",
                      h.owner ? h.owner->player_id : 0,
                      h.player_target ? h.player_target->player_id : 0,
                      h.skill_index,
                      h.skill_level,
                      h.target_id,
                      h.damage,
                      h.palsy,
                      static_cast<int>(h.flags),
                      h.target_is_player,
                      h.dead,
                      h.dead_maguni,
                      h.dead_def_index,
                      h.dead_x,
                      h.dead_y);
        }
        for (std::size_t i = 0; i < p.clients.size(); ++i) {
            const auto& c = p.clients[i];
            auto combat = platform::Json::parse(combat_session_state(c));
            if (p.bracketed_deadline[i]) {
                auto& value = combat.object()["collection_pvp_until_ms"];
                value.set_type(platform::Json::Type::String);
                value.string() = "REAL_CLOCK_BRACKETED";
            }
            h6_fields(out,
                      "peer",
                      i,
                      static_cast<int>(c.phase),
                      c.player_id,
                      c.character_id,
                      c.map,
                      c.px,
                      c.py,
                      c.pz,
                      c.exp,
                      c.control_until_ms,
                      c.village_revival_deadline_ms,
                      p.bracketed_deadline[i]);
            out << platform::Json::stringify(combat, 0) << '\n'
                << combat_inventory_state(c) << '\n';
            std::vector<std::uint32_t> known(c.known_monsters.begin(), c.known_monsters.end());
            std::sort(known.begin(), known.end());
            out << "known";
            for (auto id : known) {
                out << ',' << id;
            }
            out << '\n';
        }
        for (const auto& m : p.world.monsters()) {
            h6_fields(out,
                      "monster",
                      m.id,
                      m.def_index,
                      m.race,
                      m.x,
                      m.y,
                      m.z,
                      m.map,
                      m.hp,
                      m.max_hp,
                      m.gstate,
                      m.moving,
                      m.aggro,
                      m.aggro_player_id,
                      m.aggro_hostility,
                      m.knee_left,
                      m.die_remove_left,
                      m.spawn_protection_left,
                      m.dirty_move,
                      m.dirty_stop,
                      m.attack_swinging,
                      m.control_left,
                      m.control_break_on_damage);
            h6_map(out, "hostility", m.hostility_by_player);
            h6_map(out, "reward", m.reward_damage_by_player);
        }
        for (const auto& g : p.world.ground_items()) {
            h6_fields(out,
                      "ground",
                      g.id,
                      g.item_index,
                      g.prefix,
                      g.random_stat_type,
                      g.random_stat_value,
                      g.count,
                      g.x,
                      g.y,
                      g.map,
                      g.owner_player_id,
                      g.owner_party_id,
                      g.party_recipient_id,
                      g.server_owned,
                      g.ownership_left,
                      g.despawn_left);
        }
        for (std::size_t i = 0; i < r.packets.size(); ++i) {
            out << "wire " << i << '\n';
            for (const auto& bytes : r.packets[i]) {
                for (auto byte : bytes) {
                    out << std::hex << std::setw(2) << std::setfill('0')
                        << static_cast<unsigned>(byte);
                }
                out << std::dec << std::setfill(' ') << '\n';
            }
        }
        std::cout << out.str() << std::flush;
    }
    std::vector<std::uint8_t>
    h6_damage(int skill, int amount, std::uint32_t target, int level = 1) {
        return protocol::Skill::frame_server_damage(
            skill, 700, target, level, amount, 0, protocol::kAtkHit);
    }
    void test_h6_timed_deadlines() {
        for (bool player : {false, true}) {
            H6Players p;
            p.statuses.push_back(p.status(player));
            auto before = p.tick(9999, 0);
            expect(before.hits.empty() && p.statuses.size() == 1 &&
                       p.statuses[0].next_tick_ms == 10000 && p.statuses[0].ticks_remaining == 2,
                   "H6 before deadline retains exact status timing without delivery");
            h6_observe(player ? "timed-player-before" : "timed-monster-before", p, before);
            auto due = p.tick(10000, 0);
            expect(due.hits.size() == 1 && due.hits[0].damage == 20 && p.statuses.size() == 1 &&
                       p.statuses[0].next_tick_ms == 11000 && p.statuses[0].ticks_remaining == 1 &&
                       (player ? p.clients[1].cur_hp : p.world.find_monster(4000)->hp) == 980,
                   "H6 due equality appends one literal20 hit and decrements only afterward");
            expect(due.packets[2] == std::vector<std::vector<std::uint8_t>>{h6_damage(
                                         17, 20, player ? 701 : 4000)},
                   "H6 deferred damage reaches a Playing peer on another map");
            if (player) {
                const auto hp = protocol::Property::frame_update(
                    static_cast<std::uint8_t>(protocol::PropertyType::CurHp), 980);
                expect(due.packets[1] ==
                               std::vector<std::vector<std::uint8_t>>{hp, h6_damage(17, 20, 701)} &&
                           p.clients[0].collection_pvp_until_ms > 0 &&
                           p.clients[1].collection_pvp_until_ms > 0,
                       "H6 player property precedes deferred damage and both PvP contacts occur");
            }
            h6_observe(player ? "timed-player-due" : "timed-monster-due", p, due);
            auto last = p.tick(11000, 0);
            expect(last.hits.size() == 1 && p.statuses.empty() &&
                       (player ? p.clients[1].cur_hp : p.world.find_monster(4000)->hp) == 960,
                   "H6 final bounded status tick removes the status after exactly two hits");
            h6_observe(player ? "timed-player-last" : "timed-monster-last", p, last);
        }
        for (bool nonlethal : {false, true}) {
            for (auto now : {19999u, 20000u, 20001u, 21000u, 21001u}) {
                H6Players p;
                auto s = p.status(false);
                s.nonlethal = nonlethal;
                p.statuses.push_back(s);
                auto r = p.tick(now, 0);
                const bool expired = nonlethal ? now > 21000 : now > 20000;
                expect(r.hits.size() == (expired ? 0u : 1u) &&
                           p.statuses.size() == (expired ? 0u : 1u),
                       "H6 lethal strict expiry differs from nonlethal full-interval grace");
                h6_observe(
                    "timed-expiry-" + std::to_string(nonlethal) + "-" + std::to_string(now), p, r);
            }
        }
        for (int skill : {17, 42}) {
            H6Players p;
            auto s = p.status(false, skill);
            s.ticks_remaining = 0;
            s.tick_ms = 0;
            p.statuses.push_back(s);
            auto r = p.tick(15000, 0);
            expect(
                r.hits.size() == 1 && p.statuses[0].ticks_remaining == 0 &&
                    p.statuses[0].next_tick_ms == (skill == 42 ? 16000u : 10001u),
                "H6 Cloud anchors late ticks to now while other statuses increment prior time with minimum1");
            h6_observe("timed-late-style-" + std::to_string(skill), p, r);
        }
    }
    void test_h6_timed_filters_and_damage() {
        for (int mode = 0; mode < 8; ++mode) {
            H6Players p;
            auto s = p.status();
            if (mode == 0) {
                s.owner_id = 9999;
            } else if (mode == 1) {
                p.clients[0].phase = SessionPhase::WaitConnect;
            } else if (mode == 2) {
                p.clients[0].cur_hp = 0;
            } else if (mode == 3) {
                s.cancel_on_owner_move = true;
                ++p.clients[0].px;
            } else if (mode == 4) {
                ++p.clients[0].px;
            } else if (mode == 5) {
                s.target_id = 9999;
            } else if (mode == 6) {
                p.clients[1].cur_hp = 0;
            } else {
                p.clients[1].map = 7;
            }
            p.statuses.push_back(s);
            auto r = p.tick(10000, 0);
            const bool survives = mode == 2 || mode == 4;
            expect(
                r.hits.size() == (survives ? 1u : 0u) &&
                    p.statuses.size() == (survives ? 1u : 0u) &&
                    p.clients[1].cur_hp == (mode == 6  ? 0
                                            : survives ? 980
                                                       : 1000),
                "H6 timed lookup allows a dead owner but preserves phase, exact movement and target gates");
            h6_observe("timed-filter-" + std::to_string(mode), p, r);
        }
        for (int mode = 0; mode < 6; ++mode) {
            H6Players p;
            const bool cloud = mode >= 3;
            auto s = p.status(true, cloud ? 42 : 17, mode == 0 || mode == 3 ? 0 : 20);
            if (mode == 1 || mode == 4) {
                p.clients[1].godmode = true;
            }
            if (mode == 2 || mode == 5) {
                apply_buff(p.clients[1], SkillBuffId::KnightPerfectDefense, 100);
            }
            p.statuses.push_back(s);
            auto r = p.tick(10000, 0);
            const bool skipped = mode == 3 || mode == 4;
            const int damage = mode == 5 ? 20 : 0;
            expect(
                r.hits.size() == (skipped ? 0u : 1u) &&
                    p.statuses[0].ticks_remaining == (skipped ? 2 : 1) &&
                    p.clients[1].cur_hp == 1000 - damage,
                "H6 Cloud zero/godmode skip hit and count while ordinary zero still queues and Cloud bypasses defense");
            h6_observe("timed-zero-defense-" + std::to_string(mode), p, r);
        }
        for (bool player : {false, true}) {
            H6Players p;
            auto s = p.status(player, 17, 20);
            s.nonlethal = true;
            if (player) {
                p.clients[1].cur_hp = 10;
            } else {
                p.world.find_monster(4000)->hp = 10;
            }
            p.statuses.push_back(s);
            auto r = p.tick(10000, 0);
            expect(r.hits.size() == 1 && r.hits[0].damage == 9 && !r.hits[0].dead &&
                       (player ? p.clients[1].cur_hp : p.world.find_monster(4000)->hp) == 1,
                   "H6 generic nonlethal cap leaves exactly one HP on either target kind");
            h6_observe(player ? "timed-nonlethal-player" : "timed-nonlethal-monster", p, r);
        }
        {
            H6Players p;
            p.world.find_monster(4000)->spawn_protection_left = 10;
            p.statuses.push_back(p.status(false));
            auto r = p.tick(10000, 0);
            expect(
                r.hits.size() == 1 && r.hits[0].damage == 20 &&
                    p.world.find_monster(4000)->hp == 1000 && p.statuses[0].ticks_remaining == 1,
                "H6 protected monster rejects HP loss while retaining requested packet damage and count");
            h6_observe("timed-protected-monster", p, r);
        }
        {
            H6Players p;
            auto s = p.status(true, 42, 20);
            p.clients[1].cur_hp = 1;
            p.statuses.push_back(s);
            auto r = p.tick(10000, 0);
            expect(r.hits.empty() && p.statuses[0].ticks_remaining == 2 &&
                       p.statuses[0].next_tick_ms == 11000 && p.clients[1].cur_hp == 1,
                   "H6 Cloud at one HP advances deadline but neither queues nor consumes a hit");
            h6_observe("timed-cloud-one-hp", p, r);
        }
    }

    void test_h6_monster_rejection_and_socketless_target() {
        for (int mode = 0; mode < 4; ++mode) {
            H6Players p;
            auto s = p.status(false);
            if (mode == 0) {
                s.target_id = 9999;
            } else if (mode == 1) {
                p.world.find_monster(4000)->hp = 0;
            } else if (mode == 2) {
                p.world.find_monster(4000)->map = 1;
            } else {
                p.world.find_monster(4000)->race = static_cast<int>(protocol::MonsterRace::Magic);
            }
            p.statuses.push_back(s);
            auto r = p.tick(10000, 0);
            expect(
                r.hits.size() == (mode == 3 ? 1u : 0u) &&
                    p.statuses.size() == (mode == 3 ? 1u : 0u) &&
                    (mode != 3 ||
                     (r.hits[0].damage == 20 && p.world.find_monster(4000)->hp == 1000)),
                "H6 monster lookup removes missing/dead/map targets while race rejection preserves requested hit amount");
            h6_observe("timed-monster-filter-" + std::to_string(mode), p, r);
        }
        {
            H6Players p;
            auto a = p.status();
            auto b = a;
            b.target_id = 704;
            p.statuses = {a, b};
            auto r = p.tick(10000, 0);
            const auto first = h6_damage(17, 20, 701);
            const auto second = h6_damage(17, 20, 704);
            expect(
                r.hits.size() == 2 && r.hits[0].target_id == 701 && r.hits[1].target_id == 704 &&
                    p.clients[1].cur_hp == 980 && p.clients[4].cur_hp == 980 &&
                    !p.clients[4].sock &&
                    r.packets[0] == std::vector<std::vector<std::uint8_t>>{first, second} &&
                    r.packets[1] ==
                        std::vector<std::vector<std::uint8_t>>{
                            protocol::Property::frame_update(
                                static_cast<std::uint8_t>(protocol::PropertyType::CurHp), 980),
                            first,
                            second},
                "H6 socketless Playing target remains damageable and immediate property precedes all deferred hits");
            h6_observe("timed-socketless-target-and-deferred-order", p, r);
        }
    }
    void test_h6_cloud_lifecycle_and_refresh() {
        {
            H6Players p;
            p.areas.push_back(p.area());
            auto before = p.tick(9999, 0);
            expect(before.hits.empty() && p.statuses.empty() && p.areas[0].ticks_remaining == 2,
                   "H6 Cloud before due does not scan targets or consume its area counter");
            h6_observe("cloud-before", p, before);
            auto due = p.tick(10000, 0);
            expect(
                due.hits.empty() && p.statuses.size() == 3 && p.areas[0].ticks_remaining == 1 &&
                    p.areas[0].next_tick_ms == 11000 && p.clients[1].cur_hp == 1000 &&
                    p.statuses[0].target_id == 701 && p.statuses[1].target_id == 704 &&
                    p.statuses[2].target_id == 4000,
                "H6 Cloud appends players before monsters without ticking freshly created statuses");
            bool exact = true;
            for (const auto& s : p.statuses) {
                exact = exact && s.owner_id == 700 && s.next_tick_ms == 12000 &&
                        s.expire_ms == 70000 && s.ticks_remaining == 0 && s.tick_ms == 1000 &&
                        !s.nonlethal && !s.cancel_on_owner_move &&
                        s.damage == (s.target_is_player ? 5 : 7);
            }
            expect(exact, "H6 new Cloud statuses retain first-delay/defaults and target damage");
            h6_observe("cloud-new-status-order", p, due);
            auto last = p.tick(11000, 0);
            expect(
                last.hits.empty() && p.areas.size() == 1 && p.areas[0].ticks_remaining == 0 &&
                    p.statuses.size() == 3 && p.statuses[0].next_tick_ms == 12000 &&
                    p.statuses[0].expire_ms == 71000 && p.world.find_monster(5000),
                "H6 final area pulse refreshes expiry but leaves area/controller until next cleanup");
            h6_observe("cloud-last-pulse-retained", p, last);
            auto cleanup = p.tick(11000, 0);
            expect(p.areas.empty() && p.statuses.size() == 3 && !p.world.find_monster(5000) &&
                       cleanup.packets[0] ==
                           std::vector<std::vector<std::uint8_t>>{frame_remove_monster(5000)},
                   "H6 following cleanup removes only controller while timed statuses remain");
            h6_observe("cloud-next-cleanup", p, cleanup);
        }
        {
            H6Players p;
            auto old = p.status(false, 42, 9);
            old.owner_id = 701;
            old.owner_x = p.clients[1].px;
            old.owner_y = p.clients[1].py;
            old.skill_level = 3;
            old.tick_ms = 777;
            old.next_tick_ms = 13000;
            old.nonlethal = true;
            old.cancel_on_owner_move = true;
            p.statuses.push_back(old);
            p.areas.push_back(p.area());
            auto r = p.tick(10000, 0);
            const auto& s = p.statuses.front();
            expect(
                s.owner_id == 700 && s.target_id == 4000 && s.damage == 7 && s.skill_level == 1 &&
                    s.expire_ms == 70000 && s.next_tick_ms == 13000 && s.skill_index == 42 &&
                    s.tick_ms == 777 && s.ticks_remaining == 2 && s.owner_x == old.owner_x &&
                    s.owner_y == old.owner_y && s.nonlethal && s.cancel_on_owner_move,
                "H6 Cloud reownership updates exactly four fields while retaining first status scheduling/cancel state");
            h6_observe("cloud-refresh-retains-old-fields", p, r);
        }
        {
            H6Players p;
            auto a = p.area();
            auto b = a;
            b.controller_id = 5001;
            b.owner_id = 701;
            b.damage = 13;
            b.pvp_damage = 11;
            auto controller = H6Players::monster(5001, H6Players::kX, H6Players::kY);
            controller.race = static_cast<int>(protocol::MonsterRace::Magic);
            p.world.monsters().push_back(controller);
            p.areas = {a, b};
            auto r = p.tick(10000, 0);
            auto find = [&](std::uint32_t id, bool player) -> const ArcherTimedDamage* {
                for (const auto& s : p.statuses) {
                    if (s.target_id == id && s.target_is_player == player) {
                        return &s;
                    }
                }
                return nullptr;
            };
            const auto* player = find(704, true);
            const auto* monster = find(4000, false);
            expect(
                p.statuses.size() == 4 && player && monster && player->owner_id == 701 &&
                    player->damage == 11 && monster->owner_id == 701 && monster->damage == 13 &&
                    find(701, true) && find(700, true) && find(701, true)->owner_id == 700 &&
                    find(700, true)->owner_id == 701,
                "H6 later overlapping Cloud reowns first matches in authored area order without affecting excluded self");
            h6_observe("cloud-overlap-reownership", p, r);
        }
        for (int mode = 0; mode < 8; ++mode) {
            H6Players p;
            auto a = p.area();
            if (mode == 0) {
                a.owner_id = 9999;
            } else if (mode == 1) {
                p.clients[0].map = 1;
            } else if (mode == 2) {
                a.ticks_remaining = 0;
            } else if (mode == 3 || mode == 4) {
                p.clients[0].cur_hp = 0;
                a.cancel_on_move = mode == 3;
            } else if (mode == 6 || mode == 7) {
                ++p.clients[0].px;
                a.cancel_on_move = mode == 6;
            }
            p.areas.push_back(a);
            auto r = p.tick(mode == 5 ? 20001 : 10000, 0);
            const bool remains = mode == 4 || mode == 7;
            expect(
                p.areas.size() == (remains ? 1u : 0u) &&
                    (p.world.find_monster(5000) != nullptr) == remains &&
                    (remains ? p.statuses.size() == 3 : p.statuses.empty()),
                "H6 area cleanup gates dead/moved owners only under cancellation and removes controllers precisely");
            h6_observe("area-filter-" + std::to_string(mode), p, r);
        }
    }
    void test_h6_cloud_first_match_and_due_order() {
        {
            H6Players p;
            auto a = p.status(false, 42, 9);
            a.next_tick_ms = 13000;
            auto b = a;
            b.damage = 11;
            b.skill_level = 4;
            p.statuses = {a, b};
            p.areas.push_back(p.area());
            auto r = p.tick(10000, 0);
            expect(p.statuses.size() == 4 && p.statuses[0].damage == 7 &&
                       p.statuses[0].expire_ms == 70000 && p.statuses[0].next_tick_ms == 13000 &&
                       p.statuses[1].damage == 11 && p.statuses[1].skill_level == 4 &&
                       p.statuses[1].expire_ms == 20000 && p.statuses[1].next_tick_ms == 13000,
                   "H6 duplicate matching Cloud status refresh changes the first entry only");
            h6_observe("cloud-first-matching-status-only", p, r);
        }
        {
            H6Players p;
            auto s = p.status(false, 42, 9);
            s.owner_id = 701;
            p.statuses.push_back(s);
            p.areas.push_back(p.area());
            auto r = p.tick(10000, 0);
            expect(
                r.hits.size() == 1 && r.hits[0].owner->player_id == 701 && r.hits[0].damage == 9 &&
                    p.world.find_monster(4000)->hp == 991 && p.statuses[0].owner_id == 700 &&
                    p.statuses[0].damage == 7 && p.statuses[0].ticks_remaining == 1 &&
                    p.statuses[0].next_tick_ms == 11000 && p.statuses[0].expire_ms == 70000,
                "H6 due old-owner status executes before later Cloud reownership without rescheduling it");
            h6_observe("cloud-due-old-owner-before-refresh", p, r);
        }
    }
    void test_h6_rain_and_explosion_geometry() {
        {
            H6Players p;
            auto a = p.area(47);
            a.x = H6Players::kX + 250;
            a.tick_ms = 0;
            p.clients[1].px = a.x - 32;
            p.clients[4].px = a.x + 64;
            p.world.find_monster(4000)->x = a.x - 16;
            auto magic = H6Players::monster(4001, a.x, H6Players::kY);
            magic.race = static_cast<int>(protocol::MonsterRace::Magic);
            p.world.monsters().push_back(magic);
            p.world.monsters().push_back(H6Players::monster(4002, a.x + 70, H6Players::kY));
            p.world.monsters().push_back(H6Players::monster(4003, a.x, H6Players::kY, 1));
            p.areas.push_back(a);
            auto r = p.tick(10000);
            expect(
                r.hits.size() == 2 && r.hits[0].target_id == 701 && r.hits[0].target_is_player &&
                    r.hits[1].target_id == 4000 && !r.hits[1].target_is_player &&
                    p.areas[0].next_tick_ms == 11000 && p.areas[0].ticks_remaining == 1,
                "H6 Rain scans players then world order, with independent300px range and race/map exclusions");
            h6_observe("rain-independent-range-and-order", p, r);
        }
        for (int mode = 0; mode < 8; ++mode) {
            H6Players p;
            ArcherArrowExplosion b;
            b.owner_id = 700;
            b.target_id = 4000;
            b.map = 0;
            b.explode_ms = mode == 0 ? 10001 : 10000;
            if (mode == 1) {
                b.owner_id = 9999;
            } else if (mode == 2) {
                p.clients[0].cur_hp = 0;
            } else if (mode == 3) {
                p.clients[0].map = 1;
            } else if (mode == 4) {
                b.target_id = 9999;
            } else if (mode == 5) {
                p.world.find_monster(4000)->hp = 0;
            } else if (mode == 6) {
                p.world.find_monster(4000)->map = 1;
            } else if (mode == 7) {
                p.world.find_monster(4000)->x = H6Players::kX + 301;
            }
            p.explosions.push_back(b);
            auto r = p.tick(10000, 0);
            expect(
                r.hits.empty() && p.explosions.size() == (mode == 0 ? 1u : 0u),
                "H6 explosion retains only a not-due valid owner and rejects missing/dead/map/range fuses");
            h6_observe("explosion-filter-" + std::to_string(mode), p, r);
        }
        {
            H6Players p;
            p.world.find_monster(4000)->x = H6Players::kX + 192;
            p.clients[1].px = H6Players::kX + 200;
            p.clients[4].px = H6Players::kX + 8;
            p.world.monsters().push_back(
                H6Players::monster(4001, H6Players::kX + 200, H6Players::kY));
            p.explosions.push_back({700, 4000, 1, 0, false, 10000});
            auto r = p.tick(10000);
            expect(
                r.hits.size() == 3 && r.hits[0].target_id == 701 && r.hits[1].target_id == 4000 &&
                    r.hits[2].target_id == 4001 &&
                    std::all_of(r.hits.begin(),
                                r.hits.end(),
                                [](const auto& h) { return h.skill_index == 49; }) &&
                    p.explosions.empty(),
                "H6 due explosion follows current fuse coordinates and preserves player-before-monster target order");
            h6_observe("explosion-current-fuse-center", p, r);
        }
        {
            H6Players p;
            std::uint32_t next = 10000;
            expect(p.dummies.spawn(next), "H6 owns normal public debug-dummy instances");
            const auto& dummy = p.dummies.all().front();
            p.clients[0].map = p.clients[1].map = dummy.map();
            p.clients[0].px = dummy.px() - 16;
            p.clients[0].py = dummy.py();
            p.clients[1].px = dummy.px() + 16;
            p.clients[1].py = dummy.py();
            p.world.monsters().clear();
            p.explosions.push_back({700, dummy.player_id(), 1, dummy.map(), true, 10000});
            auto r = p.tick(10000);
            expect(
                r.hits.size() == 1 && r.hits[0].target_id == 701 && p.explosions.empty(),
                "H6 a debug dummy supplies the fuse center but is never a real-client splash recipient");
            h6_observe("explosion-dummy-center-only", p, r);
        }
    }
    void h6_load_reward_catalog(H6Players& p) {
        p.root = test::temp_dir(("h6_archer_" + std::to_string(getpid())).c_str());
        const auto write = [&](const std::string& relative, const std::string& text) {
            expect(test::write_bytes(p.root + '/' + relative, {text.begin(), text.end()}),
                   "H6 writes independently owned reward inputs");
        };
        test::mkdir_p(p.root + "/maps");
        expect(test::write_open_area(p.root + "/maps/n_000_000.akarea"),
               "H6 owns a walkable non-village area for real hostility credit");
        expect(p.world.load_maps_around(p.root + "/maps", H6Players::kX, H6Players::kY, 0),
               "H6 loads its owned hostility cell");
        write(
            "monsters/1.json",
            R"({"index":1,"name":"Owned H6 reward","stats":{"race":0,"level":1,"hp":10,"exp":3,"sight":[160, 240],"itemGroups":[[1, 1]]},"spawns":[]})");
        write("items/9000.json",
              R"({"index":9000,"name":"Owned H6 token","class":["general","etc"],"stackable":1})");
        write("groups/1.json", R"({"index":1,"entries":[{"item":9000,"chance":100}]})");
        write("itemgroups/1.json", R"({"index":1,"entries":[{"group":1,"chance":100}]})");
        expect(
            p.world.load_monsters_json_dir(p.root + "/monsters", H6Players::kX, H6Players::kY, 0),
            "H6 loads its owned same-level3EXP definition without population");
        expect(p.world.load_items_json_dir(p.root + "/items") &&
                   p.world.load_drop_tables(p.root + "/groups", p.root + "/itemgroups"),
               "H6 loads one owned ordinary utility drop through public catalog APIs");
        expect(p.world.find_monster(4000) && p.world.find_def(1) && p.world.find_def(1)->exp == 3 &&
                   p.world.find_def(1)->level == 1,
               "H6 catalog replacement preserves the owned live monster and authored EXP");
    }
    void test_h6_death_and_overlap() {
        {
            H6Players p;
            p.clients[1].cur_hp = 10;
            p.statuses.push_back(p.status());
            auto r = p.tick(10000, 0);
            const auto damage = h6_damage(17, 20, 701);
            const auto die = protocol::Skill::frame_action_die(701);
            expect(r.hits.size() == 1 && r.hits[0].dead && p.statuses.empty() &&
                       p.clients[1].cur_hp == 0 &&
                       r.packets[1] ==
                           std::vector<std::vector<std::uint8_t>>{
                               protocol::Property::frame_update(
                                   static_cast<std::uint8_t>(protocol::PropertyType::CurHp), 0),
                               damage,
                               die} &&
                       r.packets[2] == std::vector<std::vector<std::uint8_t>>{damage} &&
                       r.packets[0] == std::vector<std::vector<std::uint8_t>>{damage, die} &&
                       r.packets[3].empty() && r.packets[4].empty(),
                   "H6 player death sends property then all-map damage then only in-sight action");
            expect(p.clients[1].village_revival_deadline_ms == 0 &&
                       p.clients[1].control_until_ms == 0,
                   "H6 persistent death does not introduce sibling revival/control policy");
            h6_observe("player-death-packet-filters", p, r);
        }
        for (bool maguni : {false, true}) {
            H6Players p;
            h6_load_reward_catalog(p);
            auto* target = p.world.find_monster(4000);
            if (!target) {
                continue;
            }
            target->hp = target->max_hp = 10;
            target->race = maguni ? static_cast<int>(protocol::MonsterRace::Maguni)
                                  : static_cast<int>(protocol::MonsterRace::NotMaguni);
            target->moving = true;
            target->attack_swinging = true;
            target->control_left = 3;
            target->control_break_on_damage = true;
            target->gstate = protocol::kCgsKnee;
            // Repeated producer entries must not grant a second death or reward.
            p.statuses = {p.status(false), p.status(false)};
            auto a = p.area(47);
            p.areas.push_back(a);
            p.clients[1].phase = p.clients[4].phase = SessionPhase::WaitConnect;
            p.clients[2].known_monsters.erase(4000);
            p.explosions.push_back({700, 4000, 1, 0, false, 10000});
            auto r = p.tick(10000);
            target = p.world.find_monster(4000);
            expect(
                r.hits.size() == 1 && r.hits[0].target_id == 4000 && r.hits[0].dead &&
                    r.hits[0].damage == 20 && r.hits[0].dead_maguni == maguni &&
                    p.statuses.empty() && p.explosions.empty() && target && target->hp == 0 &&
                    !target->moving && !target->aggro && target->control_left == 0 &&
                    !target->control_break_on_damage && target->dirty_slow &&
                    target->attack_swinging && target->hostility_by_player.empty() &&
                    target->reward_damage_by_player.empty(),
                "H6 timed corpse suppresses later producers and consumes hostility once without extra swing cleanup");
            if (target) {
                expect(maguni ? target->knee_left == 10 && target->die_remove_left == 0 &&
                                    (target->gstate & protocol::kCgsKnee) != 0
                              : target->knee_left == 0 && target->die_remove_left == 2.5f &&
                                    (target->gstate & protocol::kCgsKnee) == 0,
                       "H6 Maguni and ordinary corpse deadlines keep their exact authored values");
            }
            const auto& drops = p.world.ground_items();
            expect(
                p.clients[0].level == 1 && p.clients[0].exp == 3 && p.next_iid.load() == 9001 &&
                    drops.size() == 1,
                "H6 full credited death grants literal3EXP and allocates exactly one new IID below levelup");
            if (!drops.empty()) {
                const auto& g = drops.front();
                expect(
                    g.id == 9001 && g.item_index == 9000 && g.count == 1 &&
                        g.owner_player_id == 700 && g.owner_party_id == 0 && !g.server_owned &&
                        g.ownership_left == 120 && g.despawn_left == 180 &&
                        g.random_stat_type == 0 && g.random_stat_value == 0,
                    "H6 utility loot retains exact ownership, duration, count and no equipment RNG payload");
                const auto action = maguni ? protocol::Skill::frame_action_knee(4000)
                                           : protocol::Skill::frame_action_die(4000);
                expect(
                    r.packets[0] ==
                            std::vector<std::vector<std::uint8_t>>{
                                h6_damage(17, 20, 4000),
                                action,
                                protocol::Property::frame_update_exp(3, 3),
                                frame_create_item(g)} &&
                        r.packets[2] ==
                            std::vector<std::vector<std::uint8_t>>{h6_damage(17, 20, 4000)},
                    "H6 damage precedes known-corpse action, EXP and real drop presentation; unknown cross-map peer sees only damage");
            }
            h6_observe(maguni ? "maguni-overlap-one-reward" : "ordinary-overlap-one-reward", p, r);
            auto second = p.tick(10000, 0);
            expect(
                second.hits.empty() && p.clients[0].exp == 3 && p.next_iid.load() == 9001 &&
                    p.world.ground_items().size() == 1,
                "H6 repeated scheduler phase cannot duplicate consumed corpse rewards or item identities");
            h6_observe(maguni ? "maguni-overlap-second-no-reward"
                              : "ordinary-overlap-second-no-reward",
                       p,
                       second);
        }
        {
            H6Players p;
            p.clients[0].cur_hp = 10;
            auto s = p.status(true);
            s.owner_id = 701;
            s.owner_x = p.clients[1].px;
            s.owner_y = p.clients[1].py;
            s.target_id = 700;
            p.statuses.push_back(s);
            auto a = p.area();
            a.cancel_on_move = true;
            p.areas.push_back(a);
            p.explosions.push_back({700, 4000, 1, 0, false, 10000});
            auto r = p.tick(10000, 0);
            expect(
                r.hits.size() == 1 && r.hits[0].owner->player_id == 701 &&
                    r.hits[0].target_id == 700 && r.hits[0].dead && p.clients[0].cur_hp == 0 &&
                    p.areas.empty() && p.explosions.empty() && !p.world.find_monster(5000),
                "H6 earlier timed death cancels the same owner's later area and explosion in previous producer order");
            h6_observe("owner-dies-before-later-producers", p, r);
        }
    }
    void test_h6_cloud_monster_one_hp() {
        H6Players p;
        auto status = p.status(false, 42, 20);
        status.nonlethal = true;
        p.world.find_monster(4000)->hp = 1;
        p.statuses.push_back(status);
        const auto next_iid = p.next_iid.load();
        std::srand(947);
        const int expected_random = std::rand();
        const auto r = p.tick(10000, 0);
        expect(p.statuses.size() == 1,
               "H6 one-HP monster Cloud retains the status after advancing its cursor");
        if (!p.statuses.empty()) {
            const auto& retained = p.statuses.front();
            status.next_tick_ms = 11000;
            expect(
                std::tie(retained.owner_id,
                         retained.target_id,
                         retained.skill_index,
                         retained.skill_level,
                         retained.target_is_player,
                         retained.damage,
                         retained.ticks_remaining,
                         retained.tick_ms,
                         retained.owner_x,
                         retained.owner_y,
                         retained.cancel_on_owner_move,
                         retained.nonlethal,
                         retained.expire_ms,
                         retained.next_tick_ms) == std::tie(status.owner_id,
                                                            status.target_id,
                                                            status.skill_index,
                                                            status.skill_level,
                                                            status.target_is_player,
                                                            status.damage,
                                                            status.ticks_remaining,
                                                            status.tick_ms,
                                                            status.owner_x,
                                                            status.owner_y,
                                                            status.cancel_on_owner_move,
                                                            status.nonlethal,
                                                            status.expire_ms,
                                                            status.next_tick_ms),
                "H6 one-HP monster Cloud advances only its deadline and retains tick count and fields");
        }
        const auto* monster = p.world.find_monster(4000);
        expect(monster && monster->hp == 1 && monster->hostility_by_player.empty() &&
                   monster->reward_damage_by_player.empty() && r.hits.empty() &&
                   std::all_of(r.packets.begin(),
                               r.packets.end(),
                               [](const auto& packets) { return packets.empty(); }),
               "H6 one-HP monster Cloud preserves HP and emits no hit, HP or damage wire");
        expect(p.next_iid.load() == next_iid && r.next_random == expected_random &&
                   p.areas.empty() && p.explosions.empty(),
               "H6 one-HP monster Cloud preserves the IID allocator and next RNG draw");
        h6_observe("timed-cloud-monster-one-hp", p, r);
    }

    void test_h6_explosion_real_player_fuse() {
        H6Players p;
        auto monster = H6Players::monster(4001, H6Players::kX + 200, H6Players::kY);
        monster.hp = monster.max_hp = 4000;
        p.world.monsters().push_back(monster);
        p.explosions.push_back({700, 701, 1, 0, true, 10000});
        // The pending fuse stores identity, so a later pose supplies the actual center.
        p.clients[1].px = H6Players::kX + 192;
        const auto next_iid = p.next_iid.load();
        // Fixed attack10 avoids attack draws; each target draws one grade and one fatal roll.
        // Seed947 grades1001/1063 give raw1201/1263; 30% PvP then15% absorb gives306.
        const auto r = p.tick(10000, 4);
        expect(r.hits.size() == 2,
               "H6 real-player fuse creates exactly the current-center player and monster hits");
        if (r.hits.size() == 2) {
            const auto& player = r.hits[0];
            const auto& target = r.hits[1];
            expect(
                player.owner == &p.clients[0] && player.player_target == &p.clients[1] &&
                    player.target_id == 701 && player.target_is_player &&
                    player.skill_index == 49 && player.skill_level == 1 && player.damage == 306 &&
                    player.palsy == 0 && player.flags == protocol::kAtkHit && !player.dead &&
                    target.owner == &p.clients[0] && target.player_target == nullptr &&
                    target.target_id == 4001 && !target.target_is_player &&
                    target.skill_index == 49 && target.skill_level == 1 && target.damage == 1263 &&
                    target.palsy == 0 && target.flags == protocol::kAtkHit && !target.dead,
                "H6 real-player fuse preserves identity, literal damage and player-before-monster order");
        }
        const auto* outside = p.world.find_monster(4000);
        const auto* inside = p.world.find_monster(4001);
        expect(
            p.clients[1].px == H6Players::kX + 192 && p.clients[1].cur_hp == 694 && outside &&
                outside->hp == 1000 && inside && inside->hp == 2737 &&
                p.clients[0].cur_hp == 1000 && p.clients[4].cur_hp == 1000,
            "H6 current player-fuse coordinates replace its earlier pose and exclude the old-center targets");
        const auto player_damage = h6_damage(49, 306, 701);
        const auto monster_damage = h6_damage(49, 1263, 4001);
        const auto hp = protocol::Property::frame_update(
            static_cast<std::uint8_t>(protocol::PropertyType::CurHp), 694);
        const H6Packets expected = {{
            {player_damage, monster_damage},
            {hp, player_damage, monster_damage},
            {player_damage, monster_damage},
            {},
            {},
            {player_damage, monster_damage},
        }};
        expect(
            r.packets == expected,
            "H6 real-player fuse sends target HP before both ordered damage frames to exact readers");
        expect(
            p.explosions.empty() && p.statuses.empty() && p.areas.empty() &&
                p.world.ground_items().empty() && p.next_iid.load() == next_iid,
            "H6 real-player fuse is consumed once without allocating items or retaining producers");
        h6_observe("explosion-real-player-current-fuse", p, r);
    }

    void test_h6_archer_persistent() {
        expect(g_exp_event == nullptr,
               "H6 owned sublevel reward fixture starts without an unrelated EXP event controller");
        test_h6_timed_deadlines();
        test_h6_timed_filters_and_damage();
        test_h6_monster_rejection_and_socketless_target();
        test_h6_cloud_lifecycle_and_refresh();
        test_h6_cloud_first_match_and_due_order();
        test_h6_rain_and_explosion_geometry();
        test_h6_death_and_overlap();
        test_h6_cloud_monster_one_hp();
        test_h6_explosion_real_player_fuse();
    }

#if defined(OPENKAL_MAGE_BASELINE_BRIDGE)
    using H7Hit = h7_baseline::PersistentAreaHit;
#else
    using H7Hit = PersistentAreaHit;
#endif
    bool h7_snapshot = false;
    using H7Packets = std::array<std::vector<std::vector<std::uint8_t>>, 6>;
    struct H7Result {
        std::vector<H7Hit> hits;
        std::vector<MonsterMoveEvent> moves;
        H7Packets packets;
        int next_random = 0;
    };
    template <typename... T> void h7_fields(std::ostream& out, const T&... values) {
        ((out << values << ','), ...);
        out << '\n';
    }
    template <typename Map> void h7_map(std::ostream& out, std::string_view name, const Map& map) {
        std::vector<typename Map::key_type> keys;
        for (const auto& item : map) {
            keys.push_back(item.first);
        }
        std::sort(keys.begin(), keys.end());
        out << name;
        for (auto key : keys) {
            out << ',' << key << ':' << map.at(key);
        }
        out << '\n';
    }
    struct H7Players {
        GameWorld world;
        std::vector<ClientSession> clients{6};
        std::array<RecordingSession*, 6> outputs{};
        std::mutex mutex;
        std::atomic<std::uint32_t> next_iid{9000};
        std::unique_ptr<IDataStore> store;
        std::vector<MagePersistentArea> areas;
        std::vector<MageLightningMark> marks;
        std::array<bool, 6> bracketed_deadline{};
        std::array<bool, 6> bracketed_slow{};
        std::string root;
        static constexpr int kX = 2048;
        static constexpr int kY = 2048;
        H7Players() {
            for (std::size_t i = 0; i < clients.size(); ++i) {
                auto& c = clients[i];
                c.phase = i == 3 ? SessionPhase::WaitConnect : SessionPhase::Playing;
                c.player_id = 700 + static_cast<std::uint32_t>(i);
                c.map = i == 2 ? 1 : 0;
                c.px = kX + static_cast<int>(i) * 8;
                c.py = kY;
                c.level = 1;
                c.clazz = 1;
                c.stats_initialized = true;
                c.collection_pve = false;
                c.collection_world = nullptr;
                c.cur_hp = i == 5 ? 0 : 1000;
                c.max_hp = 1000;
                c.cur_mp = c.max_mp = 100;
                c.min_magic = c.max_magic = 100;
                c.min_attack = c.max_attack = 10;
                c.hit = 1000;
                c.known_monsters = {4000, 5000};
                if (i != 4) {
                    auto recorder = std::make_unique<RecordingSession>();
                    outputs[i] = recorder.get();
                    c.sock = std::make_unique<HostConnection>(std::move(recorder));
                }
            }
            world.monsters().push_back(monster(4000, kX + 24, kY));
        }
        ~H7Players() {
            if (store) {
                store->close();
            }
            if (!root.empty()) {
                std::filesystem::remove_all(root);
            }
        }
        static MonsterEntity monster(std::uint32_t id, int x, int y, int map = 0) {
            MonsterEntity m;
            m.id = id;
            m.def_index = 1;
            m.name = "Owned H7 target";
            m.x = m.target_x = x;
            m.y = m.target_y = y;
            m.map = map;
            m.hp = m.max_hp = 1000;
            m.race = static_cast<int>(protocol::MonsterRace::NotMaguni);
            return m;
        }
        MagePersistentArea storm(int skill = 43) {
            auto m = monster(5000, kX, kY);
            m.race = static_cast<int>(protocol::MonsterRace::Magic);
            world.monsters().push_back(m);
            MagePersistentArea a;
            a.owner_id = 700;
            a.controller_id = 5000;
            a.skill_index = skill;
            a.skill_level = 1;
            a.map = 0;
            a.x = kX;
            a.y = kY;
            a.magic = 100;
            a.pvp_magic = 60;
            a.ticks_remaining = 2;
            a.expire_ms = 20000;
            a.next_tick_ms = 10000;
            return a;
        }
        MagePersistentArea
        channel(int skill = 65, std::uint32_t target = 0, bool player = false) const {
            MagePersistentArea a;
            a.owner_id = 700;
            a.skill_index = skill;
            a.skill_level = 3;
            a.map = 0;
            a.x = kX;
            a.y = kY;
            a.hook_attack = true;
            a.single_target_id = target;
            a.single_target_is_player = player;
            a.trigger_magic_min = a.trigger_magic_max = 100;
            a.trigger_pvp_magic_min = a.trigger_pvp_magic_max = 60;
            a.ticks_remaining = skill == 67 ? 1 : 2;
            a.expire_ms = 20000;
            a.next_tick_ms = 10000;
            return a;
        }
        H7Packets drain() {
            H7Packets packets;
            for (std::size_t i = 0; i < clients.size(); ++i) {
                if (!clients[i].sock) {
                    continue;
                }
                for (unsigned round = 0; round < 128 && clients[i].sock->pending_bytes(); ++round) {
                    const auto before = clients[i].sock->pending_bytes();
                    expect(clients[i].sock->flush(POLLIN | POLLOUT), "H7 recorder flush succeeds");
                    expect(clients[i].sock->pending_bytes() < before,
                           "H7 drain makes bounded byte progress");
                }
                expect(clients[i].sock->pending_bytes() == 0,
                       "H7 captures all staged and immediate wire output");
                packets[i] = std::move(outputs[i]->packets);
                outputs[i]->packets.clear();
            }
            return packets;
        }
        H7Result tick(std::uint64_t now, unsigned seed = 947, int expected_draws = -1) {
            H7Result r;
            std::srand(seed);
            for (int i = 0; i < std::max(0, expected_draws); ++i) {
                (void) std::rand();
            }
            const int expected_next = std::rand();
            std::srand(seed);
            std::array<std::uint64_t, 6> old_deadline{};
            std::array<std::int64_t, 6> old_slow{};
            for (std::size_t i = 0; i < clients.size(); ++i) {
                old_deadline[i] = clients[i].collection_pvp_until_ms;
                const auto* e = clients[i].skill_buffs.find(SkillBuffId::MageIcicleSlow);
                old_slow[i] = e ? e->expire_unix : -1;
            }
            const auto before = steady_ms_now();
            const auto unix_before = unix_now_sec();
#if defined(OPENKAL_MAGE_BASELINE_BRIDGE)
            h7_baseline::Bridge bridge(clients, world, mutex, next_iid, store, areas, marks);
            bridge.prune(now);
            bridge.produce(now, r.hits, r.moves);
            bridge.deliver(r.hits, r.moves);
#else
            prune_mage_lightning_marks(clients, world, mutex, marks, now);
            tick_mage_areas(clients, world, mutex, store.get(), areas, now, r.hits, r.moves);
            deliver_mage_area_effects(
                clients, world, mutex, next_iid, store.get(), r.hits, r.moves);
#endif
            const auto unix_after = unix_now_sec();
            const auto after = steady_ms_now();
            for (std::size_t i = 0; i < clients.size(); ++i) {
                const auto deadline = clients[i].collection_pvp_until_ms;
                const bool changed = deadline != old_deadline[i];
                expect(!changed || (deadline >= before + 30000 && deadline <= after + 30000),
                       "H7 touched PvP deadline remains inside explicit steady-clock bounds");
                bracketed_deadline[i] = bracketed_deadline[i] || changed;
                const auto* e = clients[i].skill_buffs.find(SkillBuffId::MageIcicleSlow);
                const bool slow_changed = e && e->expire_unix != old_slow[i];
                expect(!slow_changed ||
                           (e->expire_unix >= unix_before + 5 && e->expire_unix <= unix_after + 5),
                       "H7 newly applied66 slow retains the per-target real Unix expiry");
                bracketed_slow[i] = bracketed_slow[i] || slow_changed;
            }
            r.next_random = std::rand();
            expect(expected_draws < 0 || r.next_random == expected_next,
                   "H7 authored no-random paths preserve the next same-runtime draw");
            r.packets = drain();
            return r;
        }
    };
    void h7_observe(std::string_view name, const H7Players& p, const H7Result& r) {
        if (!h7_snapshot) {
            return;
        }
        std::ostringstream out;
        out << std::setprecision(17);
        h7_fields(out, "h7", name, p.next_iid.load(), r.next_random);
        for (const auto& a : p.areas) {
            h7_fields(out,
                      "area",
                      a.owner_id,
                      a.controller_id,
                      a.skill_index,
                      a.skill_level,
                      a.map,
                      a.x,
                      a.y,
                      a.magic,
                      a.pvp_magic,
                      a.trigger_magic_min,
                      a.trigger_magic_max,
                      a.trigger_pvp_magic_min,
                      a.trigger_pvp_magic_max,
                      a.hook_attack,
                      a.single_target_id,
                      a.single_target_is_player,
                      a.ticks_remaining,
                      a.expire_ms,
                      a.next_tick_ms);
        }
        for (const auto& m : p.marks) {
            h7_fields(
                out, "mark", m.target_id, m.target_is_player, m.owner_id, m.expire_ms, m.ready_ms);
        }
        for (const auto& h : r.hits) {
            h7_fields(out,
                      "hit",
                      h.owner ? h.owner->player_id : 0,
                      h.player_target ? h.player_target->player_id : 0,
                      h.skill_index,
                      h.skill_level,
                      h.target_id,
                      h.damage,
                      h.palsy,
                      static_cast<int>(h.flags),
                      h.target_is_player,
                      h.dead,
                      h.dead_maguni,
                      h.dead_def_index,
                      h.dead_x,
                      h.dead_y);
        }
        for (const auto& m : r.moves) {
            h7_fields(out,
                      "move",
                      m.id,
                      m.x,
                      m.y,
                      static_cast<int>(m.dx),
                      static_cast<int>(m.dy),
                      m.ended,
                      m.run);
        }
        for (std::size_t i = 0; i < p.clients.size(); ++i) {
            const auto& c = p.clients[i];
            auto state = platform::Json::parse(combat_session_state(c));
            if (p.bracketed_deadline[i]) {
                auto& v = state.object()["collection_pvp_until_ms"];
                v.set_type(platform::Json::Type::String);
                v.string() = "REAL_CLOCK_BRACKETED";
            }
            h7_fields(out,
                      "peer",
                      i,
                      static_cast<int>(c.phase),
                      c.player_id,
                      c.character_id,
                      c.map,
                      c.px,
                      c.py,
                      c.pz,
                      c.exp,
                      c.godmode,
                      c.village_revival_deadline_ms,
                      p.bracketed_deadline[i],
                      p.bracketed_slow[i]);
            out << platform::Json::stringify(state, 0) << '\n' << combat_inventory_state(c) << '\n';
            for (const auto& e : c.skill_buffs.entries()) {
                h7_fields(out,
                          "skill-buff",
                          static_cast<int>(e.id),
                          e.bstate,
                          e.skill_index,
                          e.clazz,
                          e.value1,
                          e.value2);
                if (p.bracketed_slow[i] && e.id == SkillBuffId::MageIcicleSlow) {
                    out << "REAL_UNIX_EXPIRY_BRACKETED\n";
                } else {
                    out << e.expire_unix << '\n';
                }
            }
            for (const auto& e : c.pay_buffs.entries()) {
                h7_fields(out,
                          "pay-buff",
                          e.buff_type,
                          e.bstate,
                          e.is_count,
                          e.charges,
                          e.stack_pack,
                          e.source_item_index,
                          e.expire_unix);
            }
            std::vector<std::uint32_t> known(c.known_monsters.begin(), c.known_monsters.end());
            std::sort(known.begin(), known.end());
            out << "known";
            for (auto id : known) {
                out << ',' << id;
            }
            out << '\n';
        }
        for (const auto& m : p.world.monsters()) {
            h7_fields(out,
                      "monster",
                      m.id,
                      m.def_index,
                      m.race,
                      m.x,
                      m.y,
                      m.z,
                      m.map,
                      m.hp,
                      m.max_hp,
                      m.gstate,
                      m.moving,
                      m.target_x,
                      m.target_y,
                      m.move_busy_left,
                      m.dirty_move,
                      m.dirty_stop,
                      m.slow_left,
                      m.slow_percent,
                      m.dirty_slow,
                      m.control_left,
                      m.control_break_on_damage,
                      m.aggro,
                      m.aggro_player_id,
                      m.aggro_hostility,
                      m.knee_left,
                      m.die_remove_left,
                      m.attack_swinging);
            h7_map(out, "hostility", m.hostility_by_player);
            h7_map(out, "reward", m.reward_damage_by_player);
        }
        for (const auto& g : p.world.ground_items()) {
            h7_fields(out,
                      "ground",
                      g.id,
                      g.item_index,
                      g.prefix,
                      g.random_stat_type,
                      g.random_stat_value,
                      g.count,
                      g.x,
                      g.y,
                      g.map,
                      g.owner_player_id,
                      g.owner_party_id,
                      g.party_recipient_id,
                      g.server_owned,
                      g.ownership_left,
                      g.despawn_left);
        }
        if (p.store && p.clients[0].character_id > 0) {
            auto rows = p.store->load_buff_remains(p.clients[0].character_id);
            std::sort(rows.begin(), rows.end(), [](const auto& a, const auto& b) {
                return a.buff_type < b.buff_type;
            });
            for (const auto& row : rows) {
                h7_fields(out,
                          "buff-row",
                          row.character_id,
                          row.buff_type,
                          row.remain,
                          row.is_count,
                          row.stack_pack,
                          row.source_item_index);
            }
        }
        for (std::size_t i = 0; i < r.packets.size(); ++i) {
            out << "wire " << i << '\n';
            for (const auto& bytes : r.packets[i]) {
                for (auto byte : bytes) {
                    out << std::hex << std::setw(2) << std::setfill('0')
                        << static_cast<unsigned>(byte);
                }
                out << std::dec << std::setfill(' ') << '\n';
            }
        }
        std::cout << out.str() << std::flush;
    }
    std::vector<std::uint8_t> h7_damage(int skill,
                                        int amount,
                                        std::uint32_t target,
                                        int rank = 1,
                                        int palsy = 0,
                                        int flags = protocol::kAtkHit) {
        return protocol::Skill::frame_server_damage(
            skill, 700, target, rank, amount, palsy, static_cast<std::uint8_t>(flags));
    }
    void test_h7_marks() {
        for (bool player : {false, true}) {
            for (int mode = 0; mode < 10; ++mode) {
                H7Players p;
                MageLightningMark m{player ? 701u : 4000u, player, 700, 10000, 999999};
                if (mode == 3) {
                    m.owner_id = 9999;
                } else if (mode == 4) {
                    p.clients[0].phase = SessionPhase::WaitConnect;
                } else if (mode == 5) {
                    p.clients[0].cur_hp = 0;
                } else if (mode == 6) {
                    m.target_id = 9999;
                } else if (mode == 7) {
                    if (player) {
                        p.clients[1].cur_hp = 0;
                    } else {
                        p.world.find_monster(4000)->hp = 0;
                    }
                } else if (mode == 8) {
                    if (player) {
                        p.clients[1].map = 1;
                    } else {
                        p.world.find_monster(4000)->map = 1;
                    }
                } else if (mode == 9) {
                    p.clients[0].px += 1000;
                }
                p.marks.push_back(m);
                const auto now = mode == 1 ? 10000 : mode == 2 ? 10001 : 9999;
                auto r = p.tick(now, 947, 0);
                const bool keep = mode == 0 || mode == 9;
                expect(
                    p.marks.size() == (keep ? 1u : 0u) && r.hits.empty() && r.moves.empty() &&
                        (p.marks.empty() || p.marks[0].ready_ms == 999999),
                    "H7 mark exact expiry and live identity/map gates ignore readiness and position");
                expect(std::all_of(r.packets.begin(),
                                   r.packets.end(),
                                   [](const auto& q) { return q.empty(); }),
                       "H7 mark pruning has no direct wire output");
                h7_observe("mark-" + std::to_string(player) + '-' + std::to_string(mode), p, r);
            }
        }
        {
            H7Players p;
            p.world.find_monster(4000)->hp = 0;
            p.world.monsters().push_back(
                H7Players::monster(4001, H7Players::kX + 16, H7Players::kY));
            p.marks.push_back({4000, false, 700, 20000, 0});
            p.areas.push_back(p.channel(67));
            auto r = p.tick(10000);
            expect(
                p.marks.empty() && r.hits.size() == 3 && r.hits[0].target_id == 701 &&
                    r.hits[1].target_id == 704 && r.hits[2].target_id == 4001,
                "H7 pruning the marked corpse does not cancel an already queued center-based splash");
            h7_observe("mark-prune-keeps-queued-splash", p, r);
        }
    }
    void test_h7_area_lifecycle() {
        for (int skill : {43, 65, 67}) {
            for (int mode = 0; mode < 8; ++mode) {
                H7Players p;
                auto a = skill == 43 ? p.storm() : p.channel(skill, skill == 67 ? 4000 : 0);
                if (mode == 0) {
                    a.owner_id = 9999;
                } else if (mode == 1) {
                    p.clients[0].cur_hp = 0;
                } else if (mode == 2) {
                    ++p.clients[0].px;
                } else if (mode == 3) {
                    p.clients[0].map = 1;
                } else if (mode == 4) {
                    a.ticks_remaining = 0;
                }
                p.areas.push_back(a);
                const auto now = mode == 5 ? 21000 : mode == 6 ? 21001 : mode == 7 ? 9999 : 10000;
                auto r = p.tick(now);
                const bool keep = mode == 5 || mode == 7 || (mode == 1 && skill == 43) ||
                                  (mode == 2 && skill != 65);
                expect(
                    p.areas.size() == (keep ? 1u : 0u) &&
                        (!keep || p.areas[0].next_tick_ms == (mode == 7 ? 10000u : 12000u)) &&
                        (mode != 7 || r.hits.empty()) &&
                        (skill != 43 || (p.world.find_monster(5000) != nullptr) == keep),
                    "H7 storm/hook/67 retain distinct dead/move/count and strict grace timing policies");
                h7_observe("area-life-" + std::to_string(skill) + '-' + std::to_string(mode), p, r);
            }
        }
        {
            H7Players p;
            auto a = p.storm();
            a.ticks_remaining = 1;
            p.areas.push_back(a);
            auto r = p.tick(10000, 947, 0);
            expect(
                r.hits.size() == 3 && r.hits[0].target_id == 701 && r.hits[1].target_id == 704 &&
                    r.hits[2].target_id == 4000 && r.hits[0].damage == 60 &&
                    r.hits[1].damage == 60 && r.hits[2].damage == 100 &&
                    p.clients[1].cur_hp == 940 && p.world.find_monster(4000)->hp == 900 &&
                    p.areas.size() == 1 && p.areas[0].ticks_remaining == 0 &&
                    p.areas[0].next_tick_ms == 12000 && p.world.find_monster(5000),
                "H7 literal same-level storm damage visits clients then monsters and retains its terminal area");
            expect(r.packets[2] == std::vector<std::vector<std::uint8_t>>{h7_damage(43, 60, 701),
                                                                          h7_damage(43, 60, 704),
                                                                          h7_damage(43, 100, 4000)},
                   "H7 deferred damage reaches foreign-map peer in authored append order");
            h7_observe("storm-last-pulse-retained", p, r);
            auto cleanup = p.tick(10000, 947, 0);
            expect(p.areas.empty() && !p.world.find_monster(5000) && cleanup.hits.empty() &&
                       cleanup.packets[0] ==
                           std::vector<std::vector<std::uint8_t>>{frame_remove_monster(5000)},
                   "H7 next cleanup removes controller and only its queued known-view output");
            h7_observe("storm-following-cleanup", p, cleanup);
        }
    }
    unsigned h7_percent_seed(int percent, bool success, unsigned skip = 0) {
        for (unsigned seed = 0; seed < 4096; ++seed) {
            std::srand(seed);
            for (unsigned i = 0; i < skip; ++i) {
                (void) std::rand();
            }
            if ((std::rand() % 100 + 1 <= percent) == success) {
                return seed;
            }
        }
        expect(false, "H7 finds a bounded same-runtime first-percent input");
        return 0;
    }
    unsigned h7_grade_seed(int grade) {
        for (unsigned seed = 0; seed < 4096; ++seed) {
            std::srand(seed);
            if (std::rand() % 6 + 5 == grade) {
                return seed;
            }
        }
        expect(false, "H7 finds a bounded same-runtime authored grade input");
        return 0;
    }
    void test_h7_geometry_and_single_targets() {
        {
            H7Players p;
            auto a = p.storm();
            a.x += 384;
            p.clients[1].px = a.x + 32;
            p.clients[4].px = a.x - 32;
            p.world.find_monster(4000)->x = a.x + 24;
            p.world.monsters().push_back(H7Players::monster(4001, a.x + 160, a.y));
            p.areas.push_back(a);
            auto r = p.tick(10000, 947, 0);
            expect(r.hits.size() == 3 && r.hits[0].target_id == 701 && r.hits[1].target_id == 704 &&
                       r.hits[2].target_id == 4000,
                   "H7 storm uses stored rectangle without adding the hook owner300px restriction");
            h7_observe("storm-stored-center-beyond-owner-range", p, r);
        }
        {
            H7Players p;
            auto a = p.channel(67);
            a.x += 250;
            p.clients[1].px = a.x - 32;
            p.clients[4].px = a.x + 64;
            p.world.find_monster(4000)->x = a.x - 16;
            p.world.monsters().push_back(H7Players::monster(4001, a.x + 70, a.y));
            p.areas.push_back(a);
            auto r = p.tick(10000);
            expect(r.hits.size() == 2 && r.hits[0].target_id == 701 && r.hits[1].target_id == 4000,
                   "H7 triggered67 intersects its stored-center square with owner attack range");
            h7_observe("splash-square-and-owner-range", p, r);
        }
        for (bool player : {false, true}) {
            H7Players p;
            p.areas.push_back(p.channel(67, player ? 701 : 4000, player));
            auto r = p.tick(10000);
            expect(r.hits.size() == 1 && r.hits[0].target_id == (player ? 701u : 4000u) &&
                       r.hits[0].target_is_player == player,
                   "H7 direct67 selects exactly one ID/kind without scanning other recipients");
            h7_observe(player ? "direct67-player-only" : "direct67-monster-only", p, r);
        }
        {
            H7Players p;
            p.world.find_monster(4000)->race = static_cast<int>(protocol::MonsterRace::Magic);
            p.areas.push_back(p.channel(67, 4000));
            auto r = p.tick(10000);
            expect(
                r.hits.size() == 1 && r.hits[0].damage > 0 && !r.hits[0].dead &&
                    p.world.find_monster(4000)->hp == 1000,
                "H7 reachable direct67 Magic fuse reports requested damage although ordinary attack rejects HP loss");
            h7_observe("direct67-magic-rejected-damage", p, r);
        }
        for (int mode = 0; mode < 3; ++mode) {
            H7Players p;
            auto a = p.channel(67, 4000);
            if (mode == 0) {
                a.single_target_id = 9999;
            } else if (mode == 1) {
                p.world.find_monster(4000)->map = 1;
            } else {
                p.world.find_monster(4000)->x = H7Players::kX + 301;
            }
            p.areas.push_back(a);
            auto r = p.tick(10000, 947, 0);
            expect(
                r.hits.empty() && p.areas.size() == 1 && p.areas[0].ticks_remaining == 0,
                "H7 direct67 missing/map/range rejection still consumes the terminal pulse before later cleanup");
            h7_observe("direct67-filter-" + std::to_string(mode), p, r);
        }
    }
    void test_h7_channels_and_discarded_work() {
        {
            H7Players p;
            p.clients[0].min_magic = 100;
            p.clients[0].max_magic = 101;
            p.areas.push_back(p.channel(67, 4000));
            auto r = p.tick(10000, 947, 4);
            expect(
                r.hits.size() == 1 && r.hits[0].damage >= 100 && r.hits[0].damage <= 101 &&
                    r.hits[0].flags == protocol::kAtkHit &&
                    p.world.find_monster(4000)->hp == 1000 - r.hits[0].damage,
                "H7 direct67 preserves discarded initial magic/grade plus second magic and zero-chance fatal draws");
            h7_observe("direct67-discarded-initial-roll-count", p, r);
        }
        std::vector<std::vector<std::uint8_t>> captured;
        for (int magic : {1, 1000}) {
            H7Players p;
            p.clients[0].min_magic = p.clients[0].max_magic = magic;
            p.areas.push_back(p.channel(67));
            auto r = p.tick(10000);
            if (captured.empty()) {
                captured = r.packets[2];
            } else {
                expect(
                    r.packets[2] == captured,
                    "H7 triggered67 damage uses captured magic ranges even after current owner magic changes");
            }
            h7_observe("splash-captured-range-" + std::to_string(magic), p, r);
        }
        for (int gap : {19, 20}) {
            H7Players p;
            p.clients[1].level = 1 + gap;
            p.clients[4].phase = SessionPhase::WaitConnect;
            p.world.monsters().clear();
            p.areas.push_back(p.channel(66));
            auto r = p.tick(10000, 947, gap == 20 ? 0 : -1);
            const auto* slow = p.clients[1].skill_buffs.find(SkillBuffId::MageIcicleSlow);
            expect(
                r.hits.size() == 1 && (gap == 19 ? slow != nullptr : slow == nullptr) &&
                    (gap != 20 || (r.hits[0].damage == 0 && r.hits[0].flags == 0)),
                "H7 level+20 channel gate occurs before rolls and slow assignment; level+19 remains eligible");
            h7_observe("channel-level-gap-" + std::to_string(gap), p, r);
        }
        for (bool intense : {false, true}) {
            H7Players p;
            p.clients[1].clazz = 2;
            p.clients[1].block_percent = 100;
            p.clients[4].phase = SessionPhase::WaitConnect;
            p.world.monsters().clear();
            if (intense) {
                apply_buff(p.clients[0], SkillBuffId::MageMagicIntensification, 1);
            }
            p.areas.push_back(p.channel(65));
            auto r = p.tick(10000);
            expect(
                r.hits.size() == 1 && (r.hits[0].flags & protocol::kAtkHit) == 0 &&
                    (r.hits[0].flags & protocol::kAtkBlock) != 0 &&
                    (intense ? r.hits[0].damage > 0 && p.clients[1].cur_hp < 1000
                             : r.hits[0].damage == 0 && p.clients[1].cur_hp == 1000),
                "H7 intensification can add positive player damage after a physical block without changing flags");
            h7_observe(
                intense ? "channel-block-positive-intensification" : "channel-block-zero", p, r);
        }
        for (int expected : {100, 101}) {
            for (bool broken : {false, true}) {
                H7Players p;
                p.clients[0].min_magic = p.clients[0].max_magic = 47;
                p.clients[4].phase = SessionPhase::WaitConnect;
                p.world.monsters().clear();
                if (broken) {
                    apply_buff(p.clients[1], SkillBuffId::ThiefArmorBreaker, 1);
                }
                auto a = p.channel(65);
                a.skill_level = 1;
                p.areas.push_back(a);
                auto r = p.tick(10000, h7_grade_seed(expected == 100 ? 6 : 9), 2);
                expect(
                    r.hits.size() == 1 &&
                        r.hits[0].damage == (broken && expected == 101 ? 106 : expected),
                    "H7 independent100/101 channel boundary adds armor bonus only above100 after unbroken resolution");
                h7_observe("channel-armor-" + std::to_string(expected) + '-' +
                               std::to_string(broken),
                           p,
                           r);
            }
        }
    }
    void test_h7_slow_and_storm_effects() {
        for (int mode = 0; mode < 4; ++mode) {
            H7Players p;
            p.world.monsters().clear();
            if (mode == 0) {
                p.clients[1].godmode = true;
            } else if (mode == 1) {
                p.clients[1].clazz = 2;
                p.clients[1].block_percent = 100;
            } else if (mode == 2) {
                std::int32_t remain = 0;
                std::uint64_t state = 0;
                expect(p.clients[1].skill_buffs.apply(
                           SkillBuffId::MageIcicleSlow, 1, 66, 0, -20, 7, 100, remain, state),
                       "H7 owns preexisting untimed slow replacement state");
            } else {
                p.clients[1].level = 21;
            }
            p.areas.push_back(p.channel(66));
            auto r = p.tick(10000);
            const auto* first = p.clients[1].skill_buffs.find(SkillBuffId::MageIcicleSlow);
            const auto* second = p.clients[4].skill_buffs.find(SkillBuffId::MageIcicleSlow);
            expect(
                r.hits.size() == 2 && second && second->value1 == -100 &&
                    (mode == 3 ? first == nullptr
                               : first && first->value1 == -100 && first->value2 == 0 &&
                                     first->clazz == 1 && first->skill_index == 66) &&
                    (mode > 1 || (r.hits[0].damage == 0 && p.clients[1].cur_hp == 1000)),
                "H7 eligible66 applies/replaces five-second slow even on zero/godmode/block, except level gate");
            expect(!p.clients[0].skill_buffs.find(SkillBuffId::MageIcicleSlow) &&
                       !p.clients[2].skill_buffs.find(SkillBuffId::MageIcicleSlow) &&
                       !p.clients[3].skill_buffs.find(SkillBuffId::MageIcicleSlow),
                   "H7 self/map/non-Playing recipients receive no66 slow or expiry mutation");
            h7_observe("channel66-slow-" + std::to_string(mode), p, r);
        }
        for (bool godmode : {false, true}) {
            H7Players p;
            p.clients[0].upgrade_level = 20;
            p.clients[0].weapon_min_magic = p.clients[0].weapon_max_magic = 1;
            p.clients[0].char_magic = 10;
            p.clients[1].godmode = godmode;
            if (!godmode) {
                apply_buff(p.clients[1], SkillBuffId::KnightPerfectDefense, 100);
            }
            p.clients[4].phase = SessionPhase::WaitConnect;
            p.world.monsters().clear();
            auto a = p.storm();
            p.areas.push_back(a);
            auto r = p.tick(10000, h7_percent_seed(20, true));
            expect(
                r.hits.size() == 1 && r.hits[0].palsy > 0 && r.hits[0].damage == r.hits[0].palsy &&
                    (r.hits[0].flags & protocol::kAtkExplosive) != 0 &&
                    p.clients[1].cur_hp == 1000 - r.hits[0].palsy,
                "H7 storm adds palsy after immunity/PerfectDefense reduction without normalizing that order");
            h7_observe(godmode ? "storm-palsy-after-godmode" : "storm-palsy-after-defense", p, r);
        }
        {
            H7Players p;
            p.clients[0].learned_skills[13] = 1;
            p.clients[4].phase = SessionPhase::WaitConnect;
            p.world.monsters().clear();
            p.areas.push_back(p.storm(45));
            auto r = p.tick(10000, h7_percent_seed(89, true), 1);
            expect(
                r.hits.size() == 1 && r.hits[0].damage == 60 &&
                    !p.clients[1].skill_buffs.find(SkillBuffId::MageIcicleSlow),
                "H7 player45 mastery retains its chance draw but does not add a new player slow buff");
            h7_observe("storm45-player-unused-slow-roll", p, r);
        }
        {
            H7Players p;
            p.clients[0].learned_skills[13] = 1;
            p.clients[1].phase = p.clients[4].phase = SessionPhase::WaitConnect;
            p.areas.push_back(p.storm(45));
            auto r = p.tick(10000, h7_percent_seed(89, true), 1);
            const auto* m = p.world.find_monster(4000);
            expect(
                r.hits.size() == 1 && m && m->hp == 900 && m->slow_left == 3 &&
                    m->slow_percent == -20 && !m->dirty_slow &&
                    r.packets[0] ==
                        std::vector<std::vector<std::uint8_t>>{
                            protocol::MState::frame(4000, protocol::kCmsMoveSpeed),
                            protocol::Property::frame_update_mproperty(
                                4000, protocol::kPropertyMSpeed, -20),
                            h7_damage(45, 100, 4000)},
                "H7 monster45 mastery applies real slow and known-state/speed packets before staged damage");
            h7_observe("storm45-monster-slow-before-damage", p, r);
        }
        for (int skill : {43, 45, 48}) {
            H7Players p;
            p.clients[1].res_fire = 900;
            p.clients[1].res_ice = 800;
            p.clients[1].res_lightning = 700;
            p.clients[1].res_curse = 600;
            p.clients[4].phase = SessionPhase::WaitConnect;
            p.world.monsters().clear();
            p.areas.push_back(p.storm(skill));
            auto r = p.tick(10000, 947, 0);
            const int expected = skill == 43 ? 18 : skill == 45 ? 12 : 6;
            expect(
                r.hits.size() == 1 && r.hits[0].damage == expected &&
                    p.clients[1].cur_hp == 1000 - expected,
                "H7 storm elemental resistance uses stored PvP magic and the monster-style resist divisor");
            h7_observe("storm-element-" + std::to_string(skill), p, r);
        }
    }
    void test_h7_delivery_passes() {
        for (bool maguni : {false, true}) {
            H7Players p;
            auto* m = p.world.find_monster(4000);
            m->hp = m->max_hp = 10;
            m->race = maguni ? static_cast<int>(protocol::MonsterRace::Maguni)
                             : static_cast<int>(protocol::MonsterRace::NotMaguni);
            m->moving = true;
            m->target_x += 50;
            m->target_y += 25;
            m->move_busy_left = 1;
            p.world.monsters().push_back(
                H7Players::monster(4001, H7Players::kX + 32, H7Players::kY));
            p.world.find_monster(4001)->moving = true;
            p.clients[0].known_monsters.insert(4001);
            p.clients[1].cur_hp = 10;
            p.clients[4].phase = SessionPhase::WaitConnect;
            p.areas.push_back(p.storm());
            auto second = p.areas[0];
            second.controller_id = 5001;
            auto controller = H7Players::monster(5001, H7Players::kX, H7Players::kY);
            controller.race = static_cast<int>(protocol::MonsterRace::Magic);
            p.world.monsters().push_back(controller);
            p.areas.push_back(second);
            auto r = p.tick(10000, 947, 0);
            const auto action = maguni ? protocol::Skill::frame_action_knee(4000)
                                       : protocol::Skill::frame_action_die(4000);
            expect(
                r.hits.size() == 4 && r.hits[0].target_id == 701 && r.hits[0].dead &&
                    r.hits[1].target_id == 4000 && r.hits[1].dead &&
                    r.hits[1].dead_maguni == maguni && r.hits[2].target_id == 4001 &&
                    r.hits[3].target_id == 4001 && r.moves.size() == 2 && r.moves[0].id == 4000 &&
                    r.moves[1].id == 4001 && r.moves[0].ended && r.moves[0].dx == 0 &&
                    r.moves[0].dy == 0 &&
                    r.packets[0] ==
                        std::vector<std::vector<std::uint8_t>>{
                            h7_damage(43, 60, 701),
                            h7_damage(43, 100, 4000),
                            h7_damage(43, 100, 4001),
                            h7_damage(43, 100, 4001),
                            frame_move_monster_end(4000, 0, 0, false),
                            frame_move_monster_end(4001, 0, 0, false),
                            protocol::Skill::frame_action_die(701),
                            action},
                "H7 production stages all damage then all move-ends then all death actions, never per-hit interleaving");
            expect(
                r.packets[2] == std::vector<std::vector<std::uint8_t>>{h7_damage(43, 60, 701),
                                                                       h7_damage(43, 100, 4000),
                                                                       h7_damage(43, 100, 4001),
                                                                       h7_damage(43, 100, 4001),
                                                                       frame_move_monster_end(
                                                                           4000, 0, 0, false),
                                                                       action},
                "H7 foreign-map known observer sees monster movement/death but no player death or unknown-monster move");
            expect(
                p.world.find_monster(4000)->hp == 0 && !p.world.find_monster(4000)->moving &&
                    !p.world.find_monster(4000)->dirty_stop &&
                    p.world.find_monster(4001)->hp == 800 &&
                    p.world.find_monster(4001)->target_x == p.world.find_monster(4001)->x &&
                    p.world.find_monster(4001)->target_y == p.world.find_monster(4001)->y &&
                    p.clients[1].village_revival_deadline_ms == 0,
                "H7 movement interruption resets only authored motion fields and does not add sibling death policy");
            h7_observe(
                maguni ? "delivery-maguni-three-passes" : "delivery-ordinary-three-passes", p, r);
        }
    }
    void h7_apply_drain(ClientSession& c, int charges) {
        std::int32_t remain = 0;
        std::uint64_t state = 0;
        expect(c.pay_buffs.apply(
                   static_cast<int>(protocol::BuffType::PayDrainHp), charges, 100, remain, state) &&
                   remain == charges,
               "H7 owns exact counted drain charges without a timed pay buff");
    }
    void h7_enable_store(H7Players& p) {
        p.root = test::temp_dir(("h7_mage_" + std::to_string(getpid())).c_str());
        p.store = create_data_store(StoreBackend::Sqlite);
        expect(p.store->open(p.root), "H7 opens owned SQLite counted-buff persistence");
        CharacterRecord character;
        character.name = "Owned H7 mage";
        expect(p.store->ensure_default_player("h7", "owned", character),
               "H7 creates an owned real character for buff rows");
        const auto account = p.store->find_account("h7");
        const auto characters =
            account ? p.store->list_characters(account->id) : std::vector<CharacterRecord>{};
        expect(characters.size() == 1, "H7 resolves one owned character identity");
        if (!characters.empty()) {
            p.clients[0].character_id = characters[0].id;
        }
    }
    void test_h7_drain_and_persistence() {
        for (int mode = 0; mode < 7; ++mode) {
            H7Players p;
            p.clients[0].cur_hp = mode == 4 ? 1000 : mode == 6 ? 0 : 100;
            p.clients[4].phase = SessionPhase::WaitConnect;
            p.world.find_monster(4000)->hp = 0;
            const bool has_buff = mode != 0;
            const bool success = mode != 0 && mode != 1;
            if (has_buff) {
                h7_apply_drain(p.clients[0], mode == 3 || mode == 6 ? 1 : 3);
            }
            p.areas.push_back(p.storm());
            if (mode == 6) {
                p.areas.push_back(p.channel(65));
            }
            std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db{nullptr, sqlite3_close};
            if (mode == 2 || mode == 5) {
                h7_enable_store(p);
                expect(persist_pay_buffs(p.store.get(), p.clients[0]),
                       "H7 seeds the exact pre-drain durable counted row");
                if (mode == 5) {
                    sqlite3* raw = nullptr;
                    expect(sqlite3_open((p.root + '/' + kSqliteFileName).c_str(), &raw) ==
                               SQLITE_OK,
                           "H7 opens its owned buff failure policy connection");
                    db.reset(raw);
                    expect(sqlite3_exec(db.get(),
                                        "CREATE TRIGGER h7_fail_buff BEFORE INSERT ON buff_remain "
                                        "BEGIN SELECT RAISE(FAIL,'owned H7 buff failure'); END;",
                                        nullptr,
                                        nullptr,
                                        nullptr) == SQLITE_OK,
                           "H7 injects a concrete SQLite buff-row replacement failure");
                }
            }
            auto r = p.tick(10000,
                            h7_percent_seed(10, success),
                            mode == 0   ? 0
                            : mode == 6 ? -1
                                        : 1);
            const int expected_hp = mode == 4 ? 1000 : mode == 6 ? 60 : success ? 160 : 100;
            expect(
                p.clients[0].cur_hp == expected_hp &&
                    p.clients[0].pay_buffs.has(protocol::kCbsPayDrainHp) ==
                        (has_buff && mode != 3 && mode != 6) &&
                    (mode != 6 || (r.hits.size() == 2 && r.hits[0].skill_index == 43 &&
                                   r.hits[1].skill_index == 65)),
                "H7 drain chance/count/full-HP/last-charge policies preserve dead-storm revival before a later hook");
            if (has_buff && mode != 3 && mode != 6) {
                expect(p.clients[0].pay_buffs.entries().size() == 1 &&
                           p.clients[0].pay_buffs.entries()[0].charges == (success ? 2 : 3),
                       "H7 one successful proc consumes one charge even when healing is clamped");
            }
            if (p.store) {
                const auto rows = p.store->load_buff_remains(p.clients[0].character_id);
                expect(
                    rows.size() == 1 && rows[0].remain == (mode == 5 ? 3 : 2) &&
                        rows[0].is_count == 1 && rows[0].stack_pack == 3,
                    "H7 durable replacement failure preserves old rows without rolling back live damage/heal/charge");
            }
            h7_observe("drain-policy-" + std::to_string(mode), p, r);
        }
    }
    void test_h7_special_flags_and_gates() {
        {
            H7Players p;
            p.clients[0].upgrade_level = 20;
            p.clients[0].weapon_min_magic = p.clients[0].weapon_max_magic = 1;
            p.clients[0].char_magic = 10;
            p.areas.push_back(p.channel(67, 701, true));
            auto r = p.tick(10000, h7_percent_seed(20, true, 2), 4);
            expect(
                r.hits.size() == 1 && r.hits[0].damage == 85 && r.hits[0].palsy == 0 &&
                    r.hits[0].flags == protocol::kAtkHit,
                "H7 direct lightning keeps its discarded channel/palsy draws but removes palsy and explosive output");
            h7_observe("lightning-discards-palsy-not-rolls", p, r);
        }
        {
            H7Players p;
            p.clients[1].level = 21;
            p.clients[4].phase = SessionPhase::WaitConnect;
            p.world.monsters().clear();
            apply_buff(p.clients[0], SkillBuffId::MageMagicIntensification, 1);
            p.areas.push_back(p.channel(66));
            auto r = p.tick(10000);
            expect(
                r.hits.size() == 1 && r.hits[0].damage > 0 &&
                    p.clients[1].skill_buffs.find(SkillBuffId::MageIcicleSlow),
                "H7 intensification bypasses the level+20 early return and permits the normal66 slow");
            h7_observe("channel-intensification-level-bypass", p, r);
        }
    }
    void h7_load_reward_catalog(H7Players& p) {
        p.root = test::temp_dir(("h7_mage_reward_" + std::to_string(getpid())).c_str());
        const auto write = [&](const std::string& relative, const std::string& text) {
            expect(test::write_bytes(p.root + '/' + relative, {text.begin(), text.end()}),
                   "H7 writes independently owned reward inputs");
        };
        test::mkdir_p(p.root + "/maps");
        expect(test::write_open_area(p.root + "/maps/n_000_000.akarea"),
               "H7 owns a walkable non-village area for real hostility credit");
        expect(p.world.load_maps_around(p.root + "/maps", H7Players::kX, H7Players::kY, 0),
               "H7 loads its owned hostility cell");
        write(
            "monsters/1.json",
            R"({"index":1,"name":"Owned H7 reward","stats":{"race":0,"level":1,"hp":10,"exp":3,"sight":[160, 240],"itemGroups":[[1, 1]]},"spawns":[]})");
        write("items/9000.json",
              R"({"index":9000,"name":"Owned H7 token","class":["general","etc"],"stackable":1})");
        write("groups/1.json", R"({"index":1,"entries":[{"item":9000,"chance":100}]})");
        write("itemgroups/1.json", R"({"index":1,"entries":[{"group":1,"chance":100}]})");
        expect(
            p.world.load_monsters_json_dir(p.root + "/monsters", H7Players::kX, H7Players::kY, 0),
            "H7 loads its owned same-level3EXP definition without population");
        expect(p.world.load_items_json_dir(p.root + "/items") &&
                   p.world.load_drop_tables(p.root + "/groups", p.root + "/itemgroups"),
               "H7 loads one owned ordinary utility drop through public catalog APIs");
        expect(p.world.find_monster(4000) && p.world.find_def(1) && p.world.find_def(1)->exp == 3 &&
                   p.world.find_def(1)->level == 1,
               "H7 catalog replacement preserves the owned live monster and authored EXP");
    }
    void test_h7_one_reward_after_overlapping_areas() {
        H7Players p;
        h7_load_reward_catalog(p);
        p.world.find_monster(4000)->hp = p.world.find_monster(4000)->max_hp = 10;
        p.clients[1].phase = p.clients[4].phase = SessionPhase::WaitConnect;
        auto a = p.storm();
        auto b = a;
        b.controller_id = 5001;
        auto m = H7Players::monster(5001, H7Players::kX, H7Players::kY);
        m.race = static_cast<int>(protocol::MonsterRace::Magic);
        p.world.monsters().push_back(m);
        p.areas = {a, b};
        auto r = p.tick(10000);
        expect(
            r.hits.size() == 1 && r.hits[0].dead && p.clients[0].level == 1 &&
                p.clients[0].exp == 3 && p.next_iid.load() == 9001 &&
                p.world.ground_items().size() == 1,
            "H7 first area corpse prevents a second area from duplicating literal3EXP and utility loot");
        if (!p.world.ground_items().empty()) {
            const auto& g = p.world.ground_items()[0];
            expect(
                g.id == 9001 && g.item_index == 9000 && g.count == 1 && g.owner_player_id == 700 &&
                    g.ownership_left == 120 &&
                    r.packets[0] ==
                        std::vector<std::vector<std::uint8_t>>{
                            h7_damage(43, 100, 4000),
                            protocol::Skill::frame_action_die(4000),
                            protocol::Property::frame_update_exp(3, 3),
                            frame_create_item(g)},
                "H7 real reward follows all staged effects with exact new identity and damage/action/EXP/item order");
        }
        h7_observe("overlap-one-reward-and-item", p, r);
        auto second = p.tick(10000, 947, 0);
        expect(second.hits.empty() && p.clients[0].exp == 3 && p.next_iid.load() == 9001 &&
                   p.world.ground_items().size() == 1,
               "H7 repeated phase retains consumed reward state and item identity");
        h7_observe("overlap-next-phase-no-reward", p, second);
    }
    void test_h7_mage_persistent() {
        expect(g_exp_event == nullptr,
               "H7 direct persistent fixture does not inherit an unrelated EXP event controller");
        test_h7_marks();
        test_h7_area_lifecycle();
        test_h7_geometry_and_single_targets();
        test_h7_channels_and_discarded_work();
        test_h7_slow_and_storm_effects();
        test_h7_delivery_passes();
        test_h7_drain_and_persistence();
        test_h7_special_flags_and_gates();
        test_h7_one_reward_after_overlapping_areas();
    }

    bool h8_snapshot = false;
    using H8Packets = std::array<std::vector<std::vector<std::uint8_t>>, 8>;
    struct H8Result {
        std::vector<PlayerPose> poses;
        std::vector<MonsterMoveEvent> moves;
        std::vector<MonsterAttackEvent> attacks;
        std::vector<ChestOpenEvent> chests;
        std::vector<MonsterCorpseEvent> corpses;
        std::vector<std::uint32_t> despawned_items;
        std::vector<MonsterCorpseEvent> consumed_corpses;
        std::vector<PlayerPose> viewers;
        H8Packets packets;
        int radius = 0;
        bool indexed = false;
        int next_random = 0;
    };
    template <typename... T> void h8_fields(std::ostream& out, const T&... values) {
        ((out << values << ','), ...);
        out << '\n';
    }
    template <typename Range>
    void h8_ids(std::ostream& out, std::string_view name, const Range& input) {
        std::vector<std::uint32_t> ids(input.begin(), input.end());
        std::sort(ids.begin(), ids.end());
        out << name;
        for (auto id : ids) {
            out << ',' << id;
        }
        out << '\n';
    }
    template <typename Map>
    void h8_sorted_map(std::ostream& out, std::string_view name, const Map& map) {
        std::vector<typename Map::key_type> keys;
        for (const auto& entry : map) {
            keys.push_back(entry.first);
        }
        std::sort(keys.begin(), keys.end());
        out << name;
        for (auto key : keys) {
            out << ',' << key << ':' << map.at(key);
        }
        out << '\n';
    }
    void h8_buffs(std::ostream& out, const SkillBuffSet& buffs) {
        for (const auto& entry : buffs.entries()) {
            h8_fields(out,
                      "buff",
                      static_cast<int>(entry.id),
                      entry.bstate,
                      entry.skill_index,
                      entry.clazz,
                      entry.value1,
                      entry.value2,
                      entry.expire_unix);
        }
    }
    void h8_monster(std::ostream& out, const MonsterEntity& m) {
        h8_fields(out,
                  "monster",
                  m.id,
                  m.def_index,
                  m.race,
                  std::quoted(m.name),
                  m.x,
                  m.y,
                  m.z,
                  m.map,
                  m.dir,
                  m.hp,
                  m.max_hp,
                  m.roam_speed,
                  m.chase_speed,
                  m.speed,
                  m.roam_x0,
                  m.roam_y0,
                  m.roam_x1,
                  m.roam_y1,
                  m.target_x,
                  m.target_y,
                  m.moving,
                  m.move_is_run,
                  m.move_busy_left,
                  m.idle_left,
                  m.dirty_create,
                  m.dirty_move,
                  m.dirty_stop,
                  static_cast<int>(m.ai),
                  m.aggro_player_id,
                  m.aggro,
                  m.beacon_pursuit,
                  m.aggro_hostility,
                  m.aggressive,
                  m.attack_range,
                  m.detect_range,
                  m.sight_range,
                  m.min_attack,
                  m.max_attack,
                  m.attack_cooldown,
                  m.skill_cooldown,
                  m.skill_precast_left,
                  m.pending_skill_slot,
                  m.next_skill_slot,
                  m.spawn_protection_left,
                  m.doggebi_absorb_left,
                  m.doggebi_resist_all_left,
                  m.doggebi_pending_reaction_damage,
                  m.doggebi_pending_reaction_physical,
                  m.attack_swinging,
                  m.detect_cooldown,
                  m.control_left,
                  m.control_break_on_damage,
                  m.open_player_id,
                  m.open_left,
                  m.open_remove_left,
                  m.gstate,
                  m.knee_left,
                  m.die_remove_left,
                  m.fire_flower_explode_left,
                  m.fire_flower_skill_fire_left,
                  m.fire_flower_remove_left,
                  m.fire_flower_exploded,
                  m.fire_flower_removed,
                  m.slow_left,
                  m.slow_percent,
                  m.dirty_slow);
        h8_sorted_map(out, "hostility", m.hostility_by_player);
        h8_sorted_map(out, "reward", m.reward_damage_by_player);
        h8_buffs(out, m.skill_buffs);
        out << "path";
        for (const auto& point : m.path_waypoints) {
            out << ',' << point.first << ':' << point.second;
        }
        out << '\n';
    }

    struct H8Players {
        GameWorld world;
        NpcRegistry npcs;
        map::MapCatalog catalog;
        ServerConfig config;
        HarvestPlants plants{1234};
        WorldInterestIndex interest;
        std::mutex mutex;
        std::uint32_t next_entity_id = 8000;
        std::vector<ClientSession> clients{8};
        std::array<RecordingSession*, 8> outputs{};
        std::string root;
        static constexpr int kX = 2048;
        static constexpr int kY = 2048;
        H8Players() {
            root = test::temp_dir(("h8_worldstep_" + std::to_string(getpid())).c_str());
            test::mkdir_p(root + "/maps");
            expect(test::write_open_area(root + "/maps/n_000_000.akarea") &&
                       world.load_maps_around(root + "/maps", kX, kY, 0),
                   "H8 owns every warmed map input before source-stage observation");
            config.spawn_load_radius_px = 512;
            config.max_monsters = 4;
            config.multiplayer = false;
            config.plants.types.clear();
            for (std::size_t i = 0; i < clients.size(); ++i) {
                auto& c = clients[i];
                c.phase = i == 3 ? SessionPhase::WaitConnect : SessionPhase::Playing;
                c.player_id = i == 4 ? 0 : 700 + static_cast<std::uint32_t>(i);
                c.map = i == 2 || i == 6 ? 1 : 0;
                c.px = kX + static_cast<int>(i) * 8;
                c.py = kY;
                c.cur_hp = i == 1 ? 0 : 1000;
                c.max_hp = 1000;
                c.level = 1;
                c.stats_initialized = true;
                if (i != 2 && i != 7) {
                    auto recording = std::make_unique<RecordingSession>();
                    outputs[i] = recording.get();
                    if (i == 5) {
                        recording->close();
                    }
                    c.sock = std::make_unique<HostConnection>(std::move(recording));
                }
            }
        }
        ~H8Players() {
            if (!root.empty()) {
                std::filesystem::remove_all(root);
            }
        }
        static MonsterEntity monster(std::uint32_t id, int map = 0) {
            MonsterEntity m;
            m.id = id;
            m.def_index = 1;
            m.name = "Owned H8 entity";
            m.race = static_cast<int>(protocol::MonsterRace::NotMaguni);
            m.x = m.target_x = kX + 24;
            m.y = m.target_y = kY;
            m.map = map;
            m.hp = m.max_hp = 1000;
            m.idle_left = 10000;
            return m;
        }
        void only_actor() {
            for (std::size_t i = 1; i < clients.size(); ++i) {
                clients[i].phase = SessionPhase::WaitConnect;
            }
        }
        void write(const std::string& relative, const std::string& text) {
            if (root.empty()) {
                root = test::temp_dir(("h8_worldstep_" + std::to_string(getpid())).c_str());
            }
            expect(test::write_bytes(root + '/' + relative, {text.begin(), text.end()}),
                   "H8 writes only owned fixture inputs");
        }
        void collect(std::uint64_t now, std::vector<PlayerPose>& poses) {
#if defined(OPENKAL_WORLDSTEP_BASELINE_BRIDGE)
            h8_baseline::collect(clients, now, poses);
#else
            collect_world_prey_poses(clients, now, poses);
#endif
        }
        void refill(const std::vector<PlayerPose>& viewers, int radius, std::uint64_t now) {
#if defined(OPENKAL_WORLDSTEP_BASELINE_BRIDGE)
            h8_baseline::refill(clients, world, viewers, config, radius, next_entity_id, now);
#else
            refill_monsters_for_players(
                clients, world, viewers, radius, config.max_monsters, next_entity_id, now);
#endif
        }
        void publish(std::vector<MonsterCorpseEvent>& corpses) {
#if defined(OPENKAL_WORLDSTEP_BASELINE_BRIDGE)
            h8_baseline::publish(world, clients, corpses);
#else
            publish_monster_status_and_remove_corpses(world, clients, corpses);
#endif
        }
        void sync() {
#if defined(OPENKAL_WORLDSTEP_BASELINE_BRIDGE)
            h8_baseline::sync(clients, world, npcs, interest);
#else
            sync_world_interest(clients, world, npcs, interest);
#endif
        }
        bool uses_index() const {
            return std::count_if(clients.begin(), clients.end(), [](const auto& c) {
                       return c.phase == SessionPhase::Playing && c.sock && c.player_id != 0;
                   }) > 1;
        }
        H8Packets drain() {
            H8Packets packets;
            for (std::size_t i = 0; i < clients.size(); ++i) {
                if (!clients[i].sock) {
                    continue;
                }
                for (unsigned round = 0; round < 128 && clients[i].sock->pending_bytes(); ++round) {
                    const auto before = clients[i].sock->pending_bytes();
                    expect(clients[i].sock->flush(POLLIN | POLLOUT), "H8 recorder flush succeeds");
                    expect(clients[i].sock->pending_bytes() < before,
                           "H8 complete drain makes bounded byte progress");
                }
                expect(clients[i].sock->pending_bytes() == 0, "H8 captures every queued byte");
                packets[i] = std::move(outputs[i]->packets);
                outputs[i]->packets.clear();
            }
            return packets;
        }
        void finish(H8Result& r) {
            r.next_random = std::rand();
            r.packets = drain();
        }
        H8Result step(std::uint64_t now, float elapsed) {
            std::srand(947);
            H8Result r;
            r.poses.reserve(clients.size());
            collect(now, r.poses);
            {
                std::lock_guard<std::mutex> lock(mutex);
                const int radius =
                    config.spawn_load_radius_px > 0 ? config.spawn_load_radius_px : 16384;
                const auto viewers =
                    config.multiplayer ? monster_viewers(clients) : std::vector<PlayerPose>{};
                refill(viewers, radius, now);
                plants.update(world, catalog, config.plants, next_entity_id, elapsed);
                world.tick(elapsed, r.poses, r.moves, r.attacks, r.chests, r.corpses);
                world.tick_ground_items(elapsed, r.despawned_items);
                r.consumed_corpses = r.corpses;
                publish(r.corpses);
                sync();
                r.radius = radius;
                r.viewers = viewers;
                r.indexed = uses_index();
            }
            expect(GameWorldCoveragePeer::occupancy_inactive(world),
                   "H8 world step releases tick-local borrowed occupancy state");
            finish(r);
            return r;
        }
        H8Result collect_only(std::uint64_t now) {
            std::srand(947);
            H8Result r;
            r.poses.reserve(clients.size());
            collect(now, r.poses);
            r.viewers = monster_viewers(clients);
            finish(r);
            return r;
        }
        H8Result refill_only(std::uint64_t now) {
            std::srand(947);
            H8Result r;
            {
                std::lock_guard<std::mutex> lock(mutex);
                const int radius =
                    config.spawn_load_radius_px > 0 ? config.spawn_load_radius_px : 16384;
                const auto viewers =
                    config.multiplayer ? monster_viewers(clients) : std::vector<PlayerPose>{};
                refill(viewers, radius, now);
                r.radius = radius;
                r.viewers = viewers;
            }
            finish(r);
            return r;
        }
        H8Result publish_only(std::vector<MonsterCorpseEvent> corpses, bool with_interest = false) {
            std::srand(947);
            H8Result r;
            r.corpses = std::move(corpses);
            r.consumed_corpses = r.corpses;
            {
                std::lock_guard<std::mutex> lock(mutex);
                publish(r.corpses);
                if (with_interest) {
                    sync();
                    r.indexed = uses_index();
                }
            }
            finish(r);
            return r;
        }
        H8Result sync_only() {
            std::srand(947);
            H8Result r;
            {
                std::lock_guard<std::mutex> lock(mutex);
                sync();
                r.indexed = uses_index();
            }
            finish(r);
            return r;
        }
    };
    void h8_observe(std::string_view name, const H8Players& p, const H8Result& r) {
        if (!h8_snapshot) {
            return;
        }
        std::ostringstream out;
        out << std::setprecision(17);
        h8_fields(out,
                  "h8",
                  name,
                  p.next_entity_id,
                  r.radius,
                  r.indexed,
                  r.next_random,
                  p.config.multiplayer,
                  p.config.spawn_load_radius_px,
                  p.config.max_monsters);
        for (const auto& pose : r.poses) {
            h8_fields(out,
                      "prey",
                      pose.id,
                      pose.x,
                      pose.y,
                      pose.map,
                      pose.beacon_active,
                      pose.aggro_immune);
        }
        for (const auto& pose : r.viewers) {
            h8_fields(out,
                      "refill-viewer",
                      pose.id,
                      pose.x,
                      pose.y,
                      pose.map,
                      pose.beacon_active,
                      pose.aggro_immune);
        }
        for (const auto& m : r.moves) {
            h8_fields(out,
                      "move",
                      m.id,
                      m.x,
                      m.y,
                      static_cast<int>(m.dx),
                      static_cast<int>(m.dy),
                      m.ended,
                      m.run);
        }
        for (const auto& a : r.attacks) {
            h8_fields(out,
                      "attack",
                      static_cast<int>(a.kind),
                      a.attacker_id,
                      a.target_player_id,
                      static_cast<int>(a.skill_index),
                      static_cast<int>(a.skill_level),
                      a.damage);
        }
        for (const auto& c : r.chests) {
            h8_fields(out,
                      "chest",
                      static_cast<int>(c.kind),
                      c.monster_id,
                      c.player_id,
                      c.def_index,
                      c.x,
                      c.y);
        }
        for (const auto& c : r.consumed_corpses) {
            h8_fields(out, "consumed-corpse", static_cast<int>(c.kind), c.monster_id);
        }
        for (const auto& c : r.corpses) {
            h8_fields(out, "remaining-corpse", static_cast<int>(c.kind), c.monster_id);
        }
        out << "despawned";
        for (auto id : r.despawned_items) {
            out << ',' << id;
        }
        out << '\n';
        for (std::size_t i = 0; i < p.clients.size(); ++i) {
            const auto& c = p.clients[i];
            h8_fields(out,
                      "peer",
                      i,
                      static_cast<int>(c.phase),
                      c.player_id,
                      c.map,
                      c.px,
                      c.py,
                      c.cur_hp,
                      c.last_spawn_refill_ms,
                      c.tele_pending,
                      c.beacon_active_until_ms,
                      c.aggro_immune_left,
                      c.sock ? c.sock->alive() : false);
            out << combat_session_state(c) << '\n' << combat_inventory_state(c) << '\n';
            h8_buffs(out, c.skill_buffs);
            h8_ids(out, "known-monsters", c.known_monsters);
            h8_ids(out, "known-items", c.known_items);
            h8_ids(out, "known-npcs", c.known_npcs);
            out << "scratch";
            for (auto id : c.interest_scratch) {
                out << ',' << id;
            }
            out << '\n';
        }
        for (const auto& m : p.world.monsters()) {
            h8_monster(out, m);
        }
        for (const auto& s : p.world.spawns()) {
            h8_fields(out,
                      "spawn",
                      s.def_index,
                      s.count,
                      s.area,
                      s.map_index,
                      s.cycle_sec,
                      s.next_gen_left,
                      s.x0,
                      s.y0,
                      s.x1,
                      s.y1);
        }
        for (const auto& g : p.world.ground_items()) {
            h8_fields(out,
                      "ground",
                      g.id,
                      g.item_index,
                      g.prefix,
                      g.random_stat_type,
                      g.random_stat_value,
                      g.count,
                      g.x,
                      g.y,
                      g.map,
                      g.owner_player_id,
                      g.owner_party_id,
                      g.party_recipient_id,
                      g.server_owned,
                      g.ownership_left,
                      g.despawn_left);
        }
        for (const auto& n : p.npcs.npcs()) {
            h8_fields(out,
                      "npc",
                      n.id,
                      n.index,
                      n.kind,
                      n.shape,
                      n.html,
                      n.storage_keeper,
                      n.trading_post_collector,
                      n.quest_key,
                      n.scenario_slot,
                      n.scenario_required_item,
                      n.has_scenario,
                      n.x,
                      n.y,
                      n.z,
                      n.map,
                      n.dir);
            for (const auto& t : n.teleports) {
                h8_fields(out, "npc-teleport", t.map, t.x, t.y);
            }
        }
        if (r.indexed) {
            // Sources have not mutated since the completed locked rebuild/consumer batch.
            for (int map : {0, 1, 9}) {
                out << "index-monsters," << map;
                for (const auto* m : p.interest.monsters(map)) {
                    out << ',' << m->id;
                }
                out << '\n' << "index-items," << map;
                for (const auto* g : p.interest.items(map)) {
                    out << ',' << g->id;
                }
                out << '\n' << "index-npcs," << map;
                for (const auto* n : p.interest.npcs(map)) {
                    out << ',' << n->id;
                }
                out << '\n';
            }
        }
        for (std::size_t i = 0; i < r.packets.size(); ++i) {
            out << "wire " << i << '\n';
            for (const auto& bytes : r.packets[i]) {
                for (auto byte : bytes) {
                    out << std::hex << std::setw(2) << std::setfill('0')
                        << static_cast<unsigned>(byte);
                }
                out << std::dec << std::setfill(' ') << '\n';
            }
        }
        std::cout << out.str() << std::flush;
    }
    std::vector<std::uint32_t> h8_pose_ids(const std::vector<PlayerPose>& poses) {
        std::vector<std::uint32_t> ids;
        for (const auto& pose : poses) {
            ids.push_back(pose.id);
        }
        return ids;
    }
    void test_h8_player_filters() {
        {
            H8Players p;
            auto r = p.collect_only(10000);
            expect(
                h8_pose_ids(r.poses) == std::vector<std::uint32_t>{700, 702, 705, 706, 707} &&
                    h8_pose_ids(r.viewers) == std::vector<std::uint32_t>{700, 701, 706} &&
                    p.uses_index(),
                "H8 prey, live refill viewers and socket-only index population keep distinct authored filters");
            expect(
                r.poses.size() == 5 && r.poses[1].map == 1 && r.poses[2].id == 705 &&
                    r.poses[4].id == 707,
                "H8 null/dead sockets do not suppress living prey and authored client order remains stable");
            h8_observe("player-four-distinct-filters", p, r);
            auto refill = p.refill_only(10000);
            expect(
                p.clients[0].last_spawn_refill_ms == 10000 &&
                    p.clients[1].last_spawn_refill_ms == 10000 &&
                    p.clients[2].last_spawn_refill_ms == 10000 &&
                    p.clients[3].last_spawn_refill_ms == 0 &&
                    p.clients[4].last_spawn_refill_ms == 0 &&
                    p.clients[5].last_spawn_refill_ms == 10000 &&
                    p.clients[6].last_spawn_refill_ms == 10000 &&
                    p.clients[7].last_spawn_refill_ms == 10000,
                "H8 eligible refill stamps include dead/null/dead-socket peers but exclude phase and zero ID");
            h8_observe("refill-filter-stamps-before-failure", p, refill);
        }
        for (int mode = 0; mode < 7; ++mode) {
            H8Players p;
            p.only_actor();
            auto& c = p.clients[0];
            c.beacon_active_until_ms = 10000;
            if (mode == 2) {
                c.tele_pending = true;
            } else if (mode == 3) {
                c.aggro_immune_left = .01f;
            } else if (mode == 4) {
                std::int32_t remain = 0;
                std::uint64_t state = 0;
                expect(c.skill_buffs.apply(
                           SkillBuffId::ThiefHiding, 3, 110, 0, 1, 0, 100, remain, state),
                       "H8 owns untimed Hiding state without a real expiry boundary");
            } else if (mode == 5) {
                c.aggro_immune_left = -1;
            }
            const auto now = mode == 1 ? 10000 : mode == 6 ? 10001 : 9999;
            auto r = p.collect_only(now);
            expect(
                r.poses.size() == 1 &&
                    r.poses[0].beacon_active == (mode != 1 && mode != 2 && mode != 6) &&
                    r.poses[0].aggro_immune == (mode == 3 || mode == 4),
                "H8 strict beacon/teleport deadline and positive-timer-or-Hiding immunity retain exact semantics");
            h8_observe("prey-beacon-immunity-" + std::to_string(mode), p, r);
        }
    }
    void h8_load_refill_catalog(H8Players& p, const std::string& spawns) {
        p.write(
            "monsters/1.json",
            R"({"index":1,"name":"Owned H8 refill","stats":{"race":1,"level":1,"hp":1000,"exp":0},"spawns":)" +
                spawns + "}");
        test::mkdir_p(p.root + "/maps");
        expect(test::write_open_area(p.root + "/maps/n_000_000.akarea"),
               "H8 owns one open refill map cell");
        expect(p.world.load_monsters_json_dir(p.root + "/monsters", 480, 480, 0) &&
                   p.world.load_maps_around(p.root + "/maps", 480, 480, 0),
               "H8 loads owned generator inputs through public APIs");
        p.clients[0].px = p.clients[0].py = 480;
    }
    void test_h8_refill_schedule_and_configuration() {
        for (std::uint64_t now : {1000u, 1999u, 2000u, 2001u, 999u}) {
            H8Players p;
            p.only_actor();
            p.clients[0].last_spawn_refill_ms = 1000;
            auto r = p.refill_only(now);
            const auto expected = now == 1000 || now == 1999 ? 1000 : now;
            expect(
                p.clients[0].last_spawn_refill_ms == expected && p.next_entity_id == 8000 &&
                    p.world.monsters().empty(),
                "H8 refill1000ms boundary retains unsigned future-timestamp subtraction and stamps unsuccessful calls");
            h8_observe("refill-schedule-" + std::to_string(now), p, r);
        }
        {
            H8Players p;
            p.only_actor();
            p.config.max_monsters = 1;
            h8_load_refill_catalog(
                p, R"([{"map":0,"area":1,"max":1,"cycle":1,"rect":[15, 15, 16, 16]}])");
            auto first = p.refill_only(0);
            expect(p.world.monsters().size() == 1 && p.world.monsters()[0].id == 8000 &&
                       p.clients[0].last_spawn_refill_ms == 0 && p.next_entity_id == 8001,
                   "H8 zero timestamp is a sentinel even when the first refill succeeds");
            h8_observe("refill-zero-sentinel-first", p, first);
            p.world.monsters().clear();
            auto second = p.refill_only(0);
            expect(p.world.monsters().size() == 1 && p.world.monsters()[0].id == 8001 &&
                       p.clients[0].last_spawn_refill_ms == 0 && p.next_entity_id == 8002,
                   "H8 a second zero-time refill is attempted after actual removal");
            h8_observe("refill-zero-sentinel-second", p, second);
        }
        for (int radius : {64, 0, -1}) {
            for (int maximum : {1, 0, -1}) {
                H8Players p;
                p.only_actor();
                p.config.spawn_load_radius_px = radius;
                p.config.max_monsters = maximum;
                h8_load_refill_catalog(
                    p,
                    R"([{"map":0,"area":1,"max":1,"cycle":1,"rect":[15, 15, 16, 16]}, {"map":0,"area":2,"max":1,"cycle":1,"rect":[50, 50, 51, 51]}])");
                auto r = p.refill_only(10000);
                const std::size_t count = radius > 0 || maximum > 0 ? 1 : 2;
                expect(
                    r.radius == (radius > 0 ? radius : 16384) &&
                        p.world.monsters().size() == count && p.next_entity_id == 8000 + count,
                    "H8 positive/fallback radius and nonpositive maximum retain separate existing defaults");
                h8_observe("refill-config-" + std::to_string(radius) + '-' +
                               std::to_string(maximum),
                           p,
                           r);
            }
        }
        for (bool blocked : {false, true}) {
            H8Players p;
            p.only_actor();
            h8_load_refill_catalog(
                p, R"([{"map":0,"area":1,"max":1,"cycle":2,"rect":[15, 15, 16, 16]}])");
            if (blocked) {
                auto* terrain = p.world.grid().cell(0, 0);
                for (int ty = 15; ty <= 16; ++ty) {
                    for (int tx = 15; tx <= 16; ++tx) {
                        terrain->set_blocked_mask(tx, ty, 0xffffu);
                    }
                }
            }
            auto r = p.step(10000, .25f);
            expect(
                p.clients[0].last_spawn_refill_ms == 10000 && p.world.spawns().size() == 1 &&
                    p.world.spawns()[0].next_gen_left == 1.75f &&
                    p.world.monsters().size() == (blocked ? 0u : 1u),
                "H8 stamps even placement failure and decrements newly armed gen clocks in the same world batch");
            h8_observe(blocked ? "refill-blocked-same-batch-clock"
                               : "refill-success-same-batch-clock",
                       p,
                       r);
        }
        {
            H8Players p;
            p.only_actor();
            p.clients[6].phase = SessionPhase::Playing;
            p.clients[6].px = p.clients[6].py = 480;
            p.config.multiplayer = true;
            p.config.max_monsters = 1;
            h8_load_refill_catalog(
                p,
                R"([{"map":0,"area":1,"max":1,"cycle":1,"rect":[15, 15, 16, 16]}, {"map":1,"area":2,"max":1,"cycle":1,"rect":[15, 15, 16, 16]}])");
            auto r = p.refill_only(10000);
            expect(
                p.world.monsters().size() == 2 && p.world.monsters()[0].id == 8000 &&
                    p.world.monsters()[0].map == 0 && p.world.monsters()[1].id == 8001 &&
                    p.world.monsters()[1].map == 1 &&
                    h8_pose_ids(r.viewers) == std::vector<std::uint32_t>{700, 706},
                "H8 authored client order refills distinct maps using the unchanged multiplayer neighborhood budget");
            h8_observe("refill-authored-client-map-order", p, r);
        }
    }
    void test_h8_dirty_publication() {
        for (int mode = 0; mode < 7; ++mode) {
            H8Players p;
            p.only_actor();
            auto m = H8Players::monster(4000);
            m.dirty_slow = true;
            if (mode == 0 || mode == 1) {
                m.slow_left = 5;
                m.slow_percent = mode == 0 ? -20 : -40;
                std::int32_t remain = 0;
                std::uint64_t state = 0;
                expect(m.skill_buffs.apply(
                           SkillBuffId::ArcherDarknessVision, 2, 39, 0, -30, 0, 100, remain, state),
                       "H8 owns untimed Archer dark slow for exact minimum semantics");
            } else if (mode == 2) {
                m.slow_left = 0;
                m.slow_percent = -40;
            } else if (mode == 3) {
                m.control_left = 5;
            } else if (mode == 4) {
                m.hp = 0;
            } else if (mode == 5) {
                p.clients[0].sock.reset();
            }
            p.world.monsters().push_back(m);
            if (mode != 6) {
                p.clients[0].known_monsters.insert(4000);
            }
            auto r = p.publish_only({});
            const auto* after = p.world.find_monster(4000);
            expect(
                after && !after->dirty_slow,
                "H8 dirty publication clears before output including dead/unobserved/disconnected cases");
            if (mode == 5 || mode == 6) {
                expect(r.packets[0].empty(),
                       "H8 missing output or unknown ID suppresses publication only");
            } else {
                const auto state = mode == 0 || mode == 1 ? protocol::kCmsMoveSpeed
                                   : mode == 3            ? protocol::kCmsMoveStop
                                                          : 0;
                const std::int16_t speed = mode == 0 ? -30 : mode == 1 ? -40 : 0;
                expect(
                    r.packets[0] ==
                        std::vector<std::vector<std::uint8_t>>{
                            protocol::MState::frame(4000, state),
                            protocol::Property::frame_update_mproperty(
                                4000, protocol::kPropertyMSpeed, speed)},
                    "H8 status precedes speed with ice/dark minimum, zero clearing and independent control stop");
            }
            h8_observe("dirty-publication-" + std::to_string(mode), p, r);
        }
        {
            H8Players p;
            p.only_actor();
            auto m = H8Players::monster(4000);
            m.dirty_slow = true;
            m.slow_left = 1;
            m.slow_percent = -20;
            p.world.monsters().push_back(m);
            p.clients[5].phase = SessionPhase::Playing;
            p.clients[5].known_monsters.insert(4000);
            auto r = p.publish_only({});
            expect(
                !p.world.find_monster(4000)->dirty_slow && r.packets[5].empty() &&
                    p.clients[5].known_monsters.count(4000) == 1,
                "H8 failed enqueue on a dead socket does not retain dirty status or erase observer knowledge");
            h8_observe("dirty-failed-output-clears-flag", p, r);
        }
        for (auto kind : {MonsterCorpseEvent::Kind::KneeTimeout,
                          MonsterCorpseEvent::Kind::Remove,
                          MonsterCorpseEvent::Kind::Die}) {
            H8Players p;
            p.only_actor();
            auto m = H8Players::monster(4000);
            m.hp = 0;
            m.dirty_slow = true;
            m.slow_left = 1;
            m.slow_percent = -20;
            p.world.monsters().push_back(m);
            p.clients[0].known_monsters.insert(4000);
            auto r = p.publish_only({{kind, 4000}}, true);
            auto expected = std::vector<std::vector<std::uint8_t>>{
                protocol::MState::frame(4000, protocol::kCmsMoveSpeed),
                protocol::Property::frame_update_mproperty(4000, protocol::kPropertyMSpeed, -20)};
            if (kind != MonsterCorpseEvent::Kind::Remove) {
                expected.push_back(protocol::Skill::frame_action_die(4000));
            }
            expected.push_back(frame_remove_monster(4000));
            expect(
                r.packets[0] == expected && r.corpses.empty() && r.consumed_corpses.size() == 1 &&
                    !p.world.find_monster(4000) && p.clients[0].known_monsters.empty(),
                "H8 dirty then optional death then removal clears corpses before interest and never recreates removed ID");
            h8_observe("corpse-publication-" + std::to_string(static_cast<int>(kind)), p, r);
        }
    }
    void test_h8_world_event_handoff() {
        H8Players p;
        p.only_actor();
        auto move = H8Players::monster(4000);
        move.dirty_stop = true;
        move.moving = true;
        move.control_left = 10;
        auto attack = H8Players::monster(4001);
        attack.x = attack.target_x = H8Players::kX + 8;
        attack.aggro = true;
        attack.aggro_player_id = 700;
        attack.aggro_hostility = 1;
        attack.hostility_by_player = {{700, 1}};
        attack.min_attack = attack.max_attack = 7;
        attack.ai = MonsterEntity::Ai::Attack;
        auto chest = H8Players::monster(4002);
        chest.race = static_cast<int>(protocol::MonsterRace::Item);
        chest.open_player_id = 700;
        chest.open_left = .5f;
        auto corpse = H8Players::monster(4003);
        corpse.hp = 0;
        corpse.race = static_cast<int>(protocol::MonsterRace::Maguni);
        corpse.gstate = protocol::kCgsKnee;
        corpse.knee_left = .5f;
        corpse.slow_left = 1;
        corpse.slow_percent = -20;
        corpse.dirty_slow = true;
        p.world.monsters() = {move, attack, chest, corpse};
        p.clients[0].known_monsters = {4000, 4001, 4002, 4003};
        GroundItem owned;
        owned.id = 9001;
        owned.item_index = 9000;
        owned.x = H8Players::kX;
        owned.y = H8Players::kY;
        owned.owner_player_id = 700;
        owned.owner_party_id = 0;
        owned.party_recipient_id = 0;
        owned.server_owned = false;
        owned.ownership_left = .5f;
        owned.despawn_left = 100;
        auto party = owned;
        party.id = 9003;
        party.owner_party_id = 71;
        party.party_recipient_id = 701;
        auto locked = owned;
        locked.id = 9004;
        locked.owner_player_id = 0;
        locked.server_owned = true;
        auto expired = owned;
        expired.id = 9002;
        expired.despawn_left = .5f;
        p.world.ground_items() = {owned, party, locked, expired};
        p.clients[0].known_items = {9001, 9002, 9003, 9004};
        auto r = p.step(10000, .5f);
        expect(
            r.moves.size() == 2 && r.moves[0].id == 4000 && r.moves[0].ended &&
                r.moves[1].id == 4001 && r.moves[1].ended && r.attacks.size() == 1 &&
                r.attacks[0].attacker_id == 4001 && r.attacks[0].target_player_id == 700 &&
                r.attacks[0].damage == 7 && r.attacks[0].kind == MonsterAttackEvent::Kind::Basic &&
                r.chests.size() == 1 && r.chests[0].kind == ChestOpenEvent::Kind::Completed &&
                r.chests[0].monster_id == 4002 && r.chests[0].player_id == 700 &&
                r.corpses.empty() && r.consumed_corpses.size() == 1 &&
                r.consumed_corpses[0].kind == MonsterCorpseEvent::Kind::KneeTimeout &&
                r.despawned_items == std::vector<std::uint32_t>{9002},
            "H8 keeps movement/attack/chest/despawn events usable after unlock while consuming corpse events");
        expect(
            p.clients[0].cur_hp == 1000 && p.world.find_monster(4002) &&
                p.world.find_monster(4002)->open_remove_left == 1.2f &&
                !p.world.find_monster(4003) && p.world.ground_items().size() == 3 &&
                p.world.ground_items()[0].owner_player_id == 0 &&
                p.world.ground_items()[0].owner_party_id == 0 &&
                !p.world.ground_items()[0].server_owned &&
                p.world.ground_items()[1].owner_player_id == 0 &&
                p.world.ground_items()[1].owner_party_id == 0 &&
                p.world.ground_items()[1].party_recipient_id == 701 &&
                !p.world.ground_items()[2].server_owned,
            "H8 leaves attack damage to H9, clears only authored ownership fields and retains the pending chest entity");
        expect(
            r.packets[0] ==
                    std::vector<std::vector<std::uint8_t>>{
                        protocol::MState::frame(4003, protocol::kCmsMoveSpeed),
                        protocol::Property::frame_update_mproperty(
                            4003, protocol::kPropertyMSpeed, -20),
                        protocol::Skill::frame_action_die(4003),
                        frame_remove_monster(4003),
                        frame_remove_monster(4002),
                        frame_remove_item(9002)} &&
                p.clients[0].known_items.count(9002) == 0 &&
                p.clients[0].known_monsters.count(4002) == 0,
            "H8 status/corpse output precedes immediate dead-chest interest and ground removal without duplicate publication");
        h8_observe("full-world-event-handoff", p, r);
        auto later = p.step(10500, 1.2f);
        expect(
            later.chests.size() == 1 && later.chests[0].kind == ChestOpenEvent::Kind::Remove &&
                later.chests[0].monster_id == 4002 && later.packets[0].empty() &&
                later.moves.empty() && later.attacks.empty() &&
                p.clients[0].known_monsters.count(4002) == 0,
            "H8 later chest-removal event survives although earlier interest already removed the known ID");
        h8_observe("later-chest-event-known-suppression", p, later);
    }
    GroundItem h8_item(std::uint32_t id, int map = 0) {
        GroundItem g;
        g.id = id;
        g.item_index = 9000;
        g.x = H8Players::kX;
        g.y = H8Players::kY;
        g.map = map;
        return g;
    }
    void h8_load_npcs(H8Players& p) {
        p.write(
            "npcs/1.json",
            R"({"index":61001,"name":"Owned H8 NPC zero","kind":0,"shape":1,"map":0,"xy":[2048, 2048, 0]})");
        p.write(
            "npcs/2.json",
            R"({"index":61002,"name":"Owned H8 NPC one","kind":0,"shape":2,"map":1,"xy":[2048, 2048, 0]})");
        expect(p.npcs.load_dir(p.root + "/npcs"), "H8 loads owned NPC definitions");
        p.npcs.spawn_for_maps(p.next_entity_id, 0, 1);
        expect(p.npcs.npcs().size() == 2 &&
                   std::count_if(p.npcs.npcs().begin(),
                                 p.npcs.npcs().end(),
                                 [](const auto& n) { return n.map == 0; }) == 1 &&
                   std::count_if(p.npcs.npcs().begin(),
                                 p.npcs.npcs().end(),
                                 [](const auto& n) { return n.map == 1; }) == 1,
               "H8 owns live NPCs on both map-filtered candidate paths");
    }
    void test_h8_index_thresholds() {
        for (int mode = 0; mode < 6; ++mode) {
            H8Players p;
            p.only_actor();
            if (mode == 0) {
                p.clients[0].phase = SessionPhase::WaitConnect;
            } else if (mode == 2 || mode == 5) {
                p.clients[1].phase = SessionPhase::Playing;
                p.clients[1].cur_hp = mode == 2 ? 0 : 1000;
            } else if (mode == 3) {
                p.clients[5].phase = SessionPhase::Playing;
            } else if (mode == 4) {
                p.clients[6].phase = SessionPhase::Playing;
            }
            h8_load_npcs(p);
            p.world.monsters().push_back(H8Players::monster(9090, 9));
            p.interest.rebuild(p.world, p.npcs);
            {
                std::vector<MonsterEntity> fresh;
                fresh.reserve(p.world.monsters().capacity() + 64);
                fresh.push_back(H8Players::monster(4000));
                fresh.push_back(H8Players::monster(4001, 1));
                p.world.monsters().swap(fresh);
            }
            p.world.ground_items() = {h8_item(9000), h8_item(9001, 1)};
            auto r = p.sync_only();
            const bool indexed = mode >= 2;
            expect(
                r.indexed == indexed && !p.config.multiplayer &&
                    (indexed ? p.interest.monsters(9).empty() : p.interest.monsters(9).size() == 1),
                "H8 index uses strictly more than one Playing/socket/ID peer independent of HP, socket alive and multiplayer config");
            if (mode == 0) {
                expect(r.packets[0].empty() && p.clients[0].known_monsters.empty(),
                       "H8 no eligible consumer leaves fallback knowledge untouched");
            } else {
                const auto npc = std::find_if(p.npcs.npcs().begin(),
                                              p.npcs.npcs().end(),
                                              [](const auto& n) { return n.map == 0; });
                expect(
                    npc != p.npcs.npcs().end() &&
                        r.packets[0] ==
                            std::vector<std::vector<std::uint8_t>>{
                                frame_create_monster(p.world.monsters()[0]),
                                frame_create_item(p.world.ground_items()[0]),
                                protocol::Quest::frame_create_npc(
                                    NpcRegistry::to_create_info(*npc))} &&
                        p.clients[0].known_monsters == std::unordered_set<std::uint32_t>{4000} &&
                        p.clients[0].known_items == std::unordered_set<std::uint32_t>{9000},
                    "H8 both fallback and indexed paths preserve monster/item/NPC publication order and map filters");
            }
            if (mode == 3) {
                expect(r.packets[5].empty() && p.clients[5].known_monsters.count(4000) == 1 &&
                           p.clients[5].known_items.count(9000) == 1,
                       "H8 a counted dead socket still updates known sets when enqueue fails");
            }
            h8_observe("interest-threshold-" + std::to_string(mode), p, r);
        }
    }
    void test_h8_index_rebuild_after_source_mutation() {
        H8Players p;
        p.only_actor();
        p.clients[1].phase = SessionPhase::Playing;
        h8_load_npcs(p);
        p.world.monsters().push_back(H8Players::monster(4000));
        p.world.ground_items().push_back(h8_item(9000));
        auto first = p.sync_only();
        expect(first.indexed && p.clients[0].known_monsters.count(4000) &&
                   p.clients[1].known_items.count(9000),
               "H8 initial two-consumer batch builds borrowed candidates");
        h8_observe("interest-two-before-reallocation", p, first);
        {
            std::vector<MonsterEntity> fresh;
            fresh.reserve(p.world.monsters().capacity() + 64);
            fresh.push_back(H8Players::monster(4002));
            p.world.monsters().swap(fresh);
            std::vector<GroundItem> fresh_items{h8_item(9005)};
            p.world.ground_items().swap(fresh_items);
        }
        p.clients[1].phase = SessionPhase::WaitConnect;
        auto fallback = p.sync_only();
        expect(
            !fallback.indexed &&
                p.clients[0].known_monsters == std::unordered_set<std::uint32_t>{4002} &&
                p.clients[0].known_items == std::unordered_set<std::uint32_t>{9005} &&
                fallback.packets[0] ==
                    std::vector<std::vector<std::uint8_t>>{
                        frame_create_monster(p.world.monsters()[0]),
                        frame_remove_monster(4000),
                        frame_create_item(p.world.ground_items()[0]),
                        frame_remove_item(9000)},
            "H8 one-consumer fallback ignores invalidated borrowed pointers after source storage is freed");
        h8_observe("interest-one-after-freed-source", p, fallback);
        p.world.monsters().push_back(H8Players::monster(4003));
        p.world.monsters().push_back(H8Players::monster(4004, 1));
        p.world.ground_items().push_back(h8_item(9006));
        p.clients[1].phase = SessionPhase::Playing;
        auto rebuilt = p.sync_only();
        expect(
            rebuilt.indexed &&
                p.clients[0].known_monsters == std::unordered_set<std::uint32_t>{4002, 4003} &&
                p.clients[1].known_monsters == p.clients[0].known_monsters &&
                p.clients[0].known_items == std::unordered_set<std::uint32_t>{9005, 9006} &&
                p.clients[1].known_items == p.clients[0].known_items &&
                p.interest.monsters(1).size() == 1 && p.interest.monsters(1)[0]->id == 4004 &&
                rebuilt.packets[0] ==
                    std::vector<std::vector<std::uint8_t>>{
                        frame_create_monster(p.world.monsters()[1]),
                        frame_create_item(p.world.ground_items()[1])},
            "H8 next two-consumer batch rebuilds after all source mutations before reading borrowed candidates");
        h8_observe("interest-two-after-rebuild", p, rebuilt);
    }
    void test_h8_seeded_plant_continuation() {
        H8Players p;
        p.only_actor();
        p.write(
            "monsters/65000.json",
            R"({"index":65000,"name":"Owned H8 plant","stats":{"race":4,"level":1,"hp":1,"ai":0},"spawns":[]})");
        expect(p.world.load_monsters_json_dir(p.root + "/monsters", 0, 0, 0),
               "H8 loads its own static plant definition");
        map::AreaCell outdoor;
        outdoor.assign_attrs(map::kTileCount * map::kTileCount, 0);
        outdoor.ensure_blocked_mask(false);
        outdoor.ensure_not_moveable_mask(false);
        p.world.grid().set_cell(0, 0, outdoor);
        expect(p.catalog.load_json_text(R"({"maps":[{"index":0,"kind":1,"xy":[0, 0]}]})"),
               "H8 owns an explicit eligible outdoor plant area");
        HarvestTypeConfig type;
        type.type = "owned_h8";
        type.entity_index = 65000;
        type.item_index = 23027;
        type.enabled = true;
        type.max_plants = 10;
        type.plants_per_area = 10;
        type.respawn_seconds = 600;
        p.config.plants.types = {type};
        p.world.set_harvest_plant_definitions({65000});
        auto initial = p.step(10000, 0);
        expect(p.world.monsters().size() == 8 && p.next_entity_id == 8008,
               "H8 existing explicit seed honors the first-update eight-placement budget");
        h8_observe("plants-first-eight-budget", p, initial);
        auto rest = p.step(10001, 0);
        expect(p.world.monsters().size() == 10 && p.next_entity_id == 8010,
               "H8 same persistent plant generator resumes pending area work on the next update");
        h8_observe("plants-pending-continuation", p, rest);
        if (p.world.monsters().empty()) {
            return;
        }
        const auto old_id = p.world.monsters()[0].id;
        p.world.monsters().erase(p.world.monsters().begin());
        auto vanished = p.step(11000, 600);
        expect(
            p.world.monsters().size() == 9 && p.next_entity_id == 8010,
            "H8 first vanished-node pass starts regrowth without subtracting that pass's elapsed time");
        h8_observe("plants-vanished-starts-full-delay", p, vanished);
        p.config.plants.types[0].enabled = false;
        auto disabled = p.step(12000, 1000);
        expect(p.world.monsters().size() == 9 && p.next_entity_id == 8010,
               "H8 disabled plant update preserves its waiting timer");
        h8_observe("plants-disabled-timer-retained", p, disabled);
        p.config.plants.types[0].enabled = true;
        auto before = p.step(13000, 599);
        expect(p.world.monsters().size() == 9 && p.next_entity_id == 8010,
               "H8 seeded regrowth remains pending one second before its boundary");
        h8_observe("plants-regrowth-before-boundary", p, before);
        auto due = p.step(14000, 1);
        expect(
            p.world.monsters().size() == 10 && p.next_entity_id == 8011 &&
                !p.world.find_monster(old_id) && p.world.find_monster(8010),
            "H8 exact regrowth boundary produces one new identity using retained private RNG state");
        h8_observe("plants-regrowth-due-new-identity", p, due);
        auto stable = p.step(15000, 0);
        expect(p.world.monsters().size() == 10 && p.next_entity_id == 8011,
               "H8 settled seeded plant population performs no duplicate allocation");
        h8_observe("plants-settled-future-update", p, stable);
    }
    void test_h8_world_step() {
        test_h8_player_filters();
        test_h8_refill_schedule_and_configuration();
        test_h8_dirty_publication();
        test_h8_world_event_handoff();
        test_h8_index_thresholds();
        test_h8_index_rebuild_after_source_mutation();
        test_h8_seeded_plant_continuation();
    }

    bool h9_snapshot = false;
    using H9Packets = std::array<std::vector<std::vector<std::uint8_t>>, 6>;
    struct H9Result {
        H9Packets packets;
        int next_random = 0;
    };
    template <typename... T> void h9_fields(std::ostream& out, const T&... values) {
        ((out << values << ','), ...);
        out << '\n';
    }
    template <typename Map> void h9_map(std::ostream& out, std::string_view name, const Map& map) {
        std::vector<typename Map::key_type> keys;
        for (const auto& entry : map) {
            keys.push_back(entry.first);
        }
        std::sort(keys.begin(), keys.end());
        out << name;
        for (auto key : keys) {
            out << ',' << key << ':' << map.at(key);
        }
        out << '\n';
    }
    template <typename Range>
    void h9_ids(std::ostream& out, std::string_view name, const Range& input) {
        std::vector<std::uint32_t> ids(input.begin(), input.end());
        std::sort(ids.begin(), ids.end());
        out << name;
        for (auto id : ids) {
            out << ',' << id;
        }
        out << '\n';
    }
    void h9_buffs(std::ostream& out, const SkillBuffSet& buffs) {
        for (const auto& b : buffs.entries()) {
            h9_fields(out,
                      "skill-buff",
                      static_cast<int>(b.id),
                      b.bstate,
                      b.skill_index,
                      b.clazz,
                      b.value1,
                      b.value2,
                      b.expire_unix);
        }
    }
    void h9_monster(std::ostream& out, const MonsterEntity& m) {
        h9_fields(out,
                  "monster",
                  m.id,
                  m.def_index,
                  m.race,
                  std::quoted(m.name),
                  m.x,
                  m.y,
                  m.z,
                  m.map,
                  m.dir,
                  m.hp,
                  m.max_hp,
                  m.roam_speed,
                  m.chase_speed,
                  m.speed,
                  m.roam_x0,
                  m.roam_y0,
                  m.roam_x1,
                  m.roam_y1,
                  m.target_x,
                  m.target_y,
                  m.moving,
                  m.move_is_run,
                  m.move_busy_left,
                  m.idle_left,
                  m.dirty_create,
                  m.dirty_move,
                  m.dirty_stop,
                  static_cast<int>(m.ai),
                  m.aggro_player_id,
                  m.aggro,
                  m.beacon_pursuit,
                  m.aggro_hostility,
                  m.aggressive,
                  m.attack_range,
                  m.detect_range,
                  m.sight_range,
                  m.min_attack,
                  m.max_attack,
                  m.attack_cooldown,
                  m.skill_cooldown,
                  m.skill_precast_left,
                  m.pending_skill_slot,
                  m.next_skill_slot,
                  m.spawn_protection_left,
                  m.doggebi_absorb_left,
                  m.doggebi_resist_all_left,
                  m.doggebi_pending_reaction_damage,
                  m.doggebi_pending_reaction_physical,
                  m.attack_swinging,
                  m.detect_cooldown,
                  m.control_left,
                  m.control_break_on_damage,
                  m.open_player_id,
                  m.open_left,
                  m.open_remove_left,
                  m.gstate,
                  m.knee_left,
                  m.die_remove_left,
                  m.fire_flower_explode_left,
                  m.fire_flower_skill_fire_left,
                  m.fire_flower_remove_left,
                  m.fire_flower_exploded,
                  m.fire_flower_removed,
                  m.slow_left,
                  m.slow_percent,
                  m.dirty_slow);
        h9_map(out, "hostility", m.hostility_by_player);
        h9_map(out, "reward", m.reward_damage_by_player);
        h9_buffs(out, m.skill_buffs);
        out << "path";
        for (const auto& point : m.path_waypoints) {
            out << ',' << point.first << ':' << point.second;
        }
        out << '\n';
    }

    struct H9Players {
        ScheduledStore persistence;
        GameWorld world;
        ServerConfig config;
        std::vector<ClientSession> clients{6};
        std::array<RecordingSession*, 6> outputs{};
        std::mutex mutex;
        std::atomic<std::uint32_t> next_iid{9000};
        std::unique_ptr<IDataStore> null_store;
        std::vector<MonsterMoveEvent> moves;
        std::vector<MonsterAttackEvent> attacks;
        std::vector<ChestOpenEvent> chests;
        std::vector<std::uint32_t> despawned_items;
        std::uint32_t lord_id = 0;
        static constexpr int kX = 2048;
        static constexpr int kY = 2048;
        H9Players() {
            for (std::size_t i = 0; i < clients.size(); ++i) {
                auto& c = clients[i];
                c.phase = i == 5 ? SessionPhase::WaitConnect : SessionPhase::Playing;
                c.player_id = 700 + static_cast<std::uint32_t>(i);
                c.map = i == 3 ? 1 : 0;
                c.px = i == 2 ? kX + 5000 : kX + static_cast<int>(i) * 8;
                c.py = kY;
                c.level = 1;
                c.clazz = 0;
                c.stats_initialized = true;
                c.cur_hp = c.max_hp = 1000;
                c.cur_mp = c.max_mp = 100;
                c.min_attack = c.max_attack = 100;
                c.hit = 1000;
                c.collection_world = nullptr;
                c.collection_pve = true;
                c.attack_speed_ms = 1000;
                if (i != 4) {
                    c.known_monsters = {4000, 4001, 4002};
                }
                auto recording = std::make_unique<RecordingSession>();
                outputs[i] = recording.get();
                c.sock = std::make_unique<HostConnection>(std::move(recording));
            }
            auto& c = clients[0];
            c.character_id = persistence.character_id;
            const auto saved = persistence.store->load_character(c.character_id);
            expect(saved.has_value(), "H9 uses an actual owned SQLite character identity");
            if (saved) {
                c.account_id = saved->account_id;
                c.name = saved->name;
            }
            HarvestTypeConfig plant;
            plant.type = "owned_h9";
            plant.entity_index = 65000;
            plant.item_index = 23027;
            plant.items_per_pick = 3;
            config.plants.types = {plant};
            ItemCombatDef ingredient;
            ingredient.index = 23027;
            ingredient.class_big = "general";
            ingredient.class_small = "etc";
            ingredient.stackable = true;
            GameWorldCoveragePeer::add_item(world, ingredient);
            ItemCombatDef filler;
            filler.index = 9000;
            filler.class_big = "general";
            filler.class_small = "etc";
            GameWorldCoveragePeer::add_item(world, filler);
            const auto write = [&](const std::string& relative, const std::string& text) {
                expect(test::write_bytes(persistence.root + '/' + relative,
                                         {text.begin(), text.end()}),
                       "H9 writes independently owned map and combat inputs");
            };
            test::mkdir_p(persistence.root + "/maps");
            expect(test::write_open_area(persistence.root + "/maps/n_000_000.akarea") &&
                       world.load_maps_around(persistence.root + "/maps", kX, kY, 0),
                   "H9 owns open non-village hostility geometry");
            write(
                "monsters/1.json",
                R"({"index":1,"name":"Owned H9 fallback","stats":{"race":1,"level":1,"hp":10000,"exp":3,"ai":1,"attack":[0, 0, 0],"itemGroups":[[1, 1]]},"spawns":[]})");
            write(
                "monsters/2.json",
                R"({"index":2,"name":"Owned H9 physical","stats":{"race":1,"level":1,"hp":10000,"exp":0,"ai":1,"hit":1000,"attack":[0, 60, 60],"magic":[50, 50],"int":30},"spawns":[]})");
            write(
                "monsters/3.json",
                R"({"index":3,"name":"Owned H9 Maguni","stats":{"race":0,"level":1,"hp":10000,"exp":3,"ai":1,"attack":[0, 0, 0],"itemGroups":[[1, 1]]},"spawns":[]})");
            write(
                "monsters/4.json",
                R"({"index":4,"name":"Owned H9 chest","stats":{"race":4,"level":1,"hp":1,"exp":0,"ai":0,"attack":[0, 0, 0],"itemGroups":[[1, 1]]},"spawns":[]})");
            write(
                "monsters/65000.json",
                R"({"index":65000,"name":"Owned H9 plant","stats":{"race":4,"level":1,"hp":1,"ai":0,"attack":[0, 0, 0]},"spawns":[]})");
            world.set_harvest_plant_definitions({65000});
            write("groups/1.json", R"({"index":1,"entries":[{"item":9000,"chance":100}]})");
            write("itemgroups/1.json", R"({"index":1,"entries":[{"group":1,"chance":100}]})");
            expect(
                world.load_monsters_json_dir(persistence.root + "/monsters", kX, kY, 0) &&
                    world.load_drop_tables(persistence.root + "/groups",
                                           persistence.root + "/itemgroups") &&
                    world.find_def(1) && world.find_def(1)->min_attack == 0 &&
                    world.find_def(1)->max_attack == 0,
                "H9 owns an actually loaded zero-stat fallback and positive-stat/magic alternatives");
            persistence.sql(
                "CREATE TABLE h9_audit(seq INTEGER PRIMARY KEY,kind TEXT,id INTEGER,amount INTEGER,aux INTEGER);"
                "CREATE TRIGGER h9_insert AFTER INSERT ON items BEGIN INSERT INTO h9_audit(kind,id,amount,aux) VALUES('I',NEW.iid,NEW.count,NEW.slot); END;"
                "CREATE TRIGGER h9_update AFTER UPDATE ON items BEGIN INSERT INTO h9_audit(kind,id,amount,aux) VALUES('U',NEW.iid,NEW.count,NEW.slot); END;"
                "CREATE TRIGGER h9_delete AFTER DELETE ON items BEGIN INSERT INTO h9_audit(kind,id,amount,aux) VALUES('D',OLD.iid,OLD.count,OLD.slot); END;"
                "CREATE TRIGGER h9_character AFTER UPDATE ON characters BEGIN INSERT INTO h9_audit(kind,id,amount,aux) VALUES('C',NEW.id,NEW.exp,NEW.hp); END;"
                "CREATE TRIGGER h9_buff_insert AFTER INSERT ON buff_remain BEGIN INSERT INTO h9_audit(kind,id,amount,aux) VALUES('B',NEW.buff_type,NEW.remain,NEW.is_count); END;"
                "CREATE TRIGGER h9_buff_delete AFTER DELETE ON buff_remain BEGIN INSERT INTO h9_audit(kind,id,amount,aux) VALUES('b',OLD.buff_type,OLD.remain,OLD.is_count); END;");
        }
        ClientSession& owner() {
            return clients[0];
        }
        static MonsterEntity monster(std::uint32_t id = 4000, int definition = 1) {
            MonsterEntity m;
            m.id = id;
            m.def_index = definition;
            m.race = definition == 3   ? static_cast<int>(protocol::MonsterRace::Maguni)
                     : definition == 4 ? static_cast<int>(protocol::MonsterRace::Item)
                                       : static_cast<int>(protocol::MonsterRace::NotMaguni);
            m.x = m.target_x = kX + 16;
            m.y = m.target_y = kY;
            m.hp = m.max_hp = 10000;
            return m;
        }
        void bag(int index, int count, std::uint32_t iid = 8000, int position = 0) {
            auto& slot = owner().bag[static_cast<std::size_t>(position)];
            slot.iid = iid;
            slot.index = index;
            slot.count = count;
            ItemRecord row;
            fill_item_record_from_slot(row, owner(), slot, 0);
            expect(persistence.store->insert_item(row),
                   "H9 seeds complete owned bag rows before counters");
        }
        int quantity(int index) const {
            int count = 0;
            for (const auto& slot : clients[0].bag) {
                if (slot.iid != 0 && slot.index == index) {
                    count += slot.count;
                }
            }
            return count;
        }
        std::vector<std::string> audit() const {
            std::vector<std::string> rows;
            sqlite3_stmt* statement = nullptr;
            expect(sqlite3_prepare_v2(persistence.db.get(),
                                      "SELECT kind,id,amount,aux FROM h9_audit ORDER BY seq",
                                      -1,
                                      &statement,
                                      nullptr) == SQLITE_OK,
                   "H9 reads ordered committed SQL effects before any later save");
            if (statement) {
                while (sqlite3_step(statement) == SQLITE_ROW) {
                    auto row = std::string(
                        reinterpret_cast<const char*>(sqlite3_column_text(statement, 0)));
                    for (int i = 1; i < 4; ++i) {
                        row += ',' + std::to_string(sqlite3_column_int64(statement, i));
                    }
                    rows.push_back(row);
                }
                sqlite3_finalize(statement);
            }
            return rows;
        }
        std::vector<StorageValues> item_rows() const {
            auto rows = persistence.store->load_items(persistence.character_id);
            std::sort(rows.begin(), rows.end(), [](const auto& a, const auto& b) {
                return a.iid < b.iid;
            });
            std::vector<StorageValues> result;
            for (const auto& row : rows) {
                result.push_back(storage_values(row));
            }
            return result;
        }
        void clear_audit() {
            persistence.sql("DELETE FROM h9_audit; DELETE FROM scheduled_audit;");
        }
        H9Packets drain() {
            H9Packets packets;
            for (std::size_t i = 0; i < clients.size(); ++i) {
                if (!clients[i].sock) {
                    continue;
                }
                for (unsigned round = 0; round < 128 && clients[i].sock->pending_bytes(); ++round) {
                    const auto before = clients[i].sock->pending_bytes();
                    expect(clients[i].sock->flush(POLLIN | POLLOUT), "H9 recorder flush succeeds");
                    expect(clients[i].sock->pending_bytes() < before,
                           "H9 complete drain makes bounded progress");
                }
                expect(clients[i].sock->pending_bytes() == 0, "H9 captures every queued wire byte");
                packets[i] = std::move(outputs[i]->packets);
                outputs[i]->packets.clear();
            }
            return packets;
        }
        H9Result run(unsigned seed = 947, bool no_store = false) {
            std::srand(seed);
#if defined(OPENKAL_WORLDEVENTS_BASELINE_BRIDGE)
            h9_baseline::Bridge bridge(
                clients, world, mutex, next_iid, no_store ? null_store : persistence.store, config);
            bridge.chests(chests);
            bridge.despawns(despawned_items);
            bridge.moves(moves);
            bridge.attacks(attacks, lord_id);
#else
            deliver_chest_events(chests,
                                 config.plants,
                                 world,
                                 clients,
                                 no_store ? nullptr : persistence.store.get(),
                                 next_iid,
                                 mutex);
            publish_item_despawns(despawned_items, clients);
            publish_monster_move_events(moves, clients);
            deliver_monster_attack_events(attacks,
                                          clients,
                                          world,
                                          mutex,
                                          no_store ? nullptr : persistence.store.get(),
                                          next_iid,
                                          lord_id);
#endif
            H9Result r;
            r.next_random = std::rand();
            r.packets = drain();
            return r;
        }
    };
    void h9_observe(std::string_view name, const H9Players& p, const H9Result& r) {
        if (!h9_snapshot) {
            return;
        }
        std::ostringstream out;
        out << std::setprecision(17);
        h9_fields(out, "h9", name, p.next_iid.load(), p.lord_id, r.next_random);
        for (const auto& c : p.chests) {
            h9_fields(out,
                      "chest",
                      static_cast<int>(c.kind),
                      c.monster_id,
                      c.player_id,
                      c.def_index,
                      c.x,
                      c.y);
        }
        out << "despawns";
        for (auto id : p.despawned_items) {
            out << ',' << id;
        }
        out << '\n';
        for (const auto& m : p.moves) {
            h9_fields(out,
                      "move",
                      m.id,
                      m.x,
                      m.y,
                      static_cast<int>(m.dx),
                      static_cast<int>(m.dy),
                      m.ended,
                      m.run);
        }
        for (const auto& a : p.attacks) {
            h9_fields(out,
                      "attack",
                      static_cast<int>(a.kind),
                      a.attacker_id,
                      a.target_player_id,
                      static_cast<int>(a.skill_index),
                      static_cast<int>(a.skill_level),
                      a.damage);
        }
        for (std::size_t i = 0; i < p.clients.size(); ++i) {
            const auto& c = p.clients[i];
            h9_fields(out,
                      "peer",
                      i,
                      c.player_id,
                      static_cast<int>(c.phase),
                      c.character_id,
                      c.account_id,
                      c.map,
                      c.px,
                      c.py,
                      c.opening_chest_id,
                      c.godmode,
                      c.exp,
                      c.resting,
                      c.aggro_immune_left,
                      c.next_iid,
                      c.sock ? c.sock->alive() : false);
            out << scheduled_state(c, 10000) << '\n';
            h9_buffs(out, c.skill_buffs);
            for (const auto& b : c.pay_buffs.entries()) {
                h9_fields(out,
                          "pay-buff",
                          b.buff_type,
                          b.bstate,
                          b.expire_unix,
                          b.charges,
                          b.is_count,
                          b.stack_pack,
                          b.source_item_index);
            }
            for (const auto& u : c.inventory_unlocks.entries()) {
                h9_fields(out, "unlock", u.expire_unix);
            }
            h9_map(out, "learned", c.learned_skills);
            h9_ids(out, "known-monsters", c.known_monsters);
            h9_ids(out, "known-items", c.known_items);
            out << "wire " << i << '\n';
            for (const auto& bytes : r.packets[i]) {
                for (auto byte : bytes) {
                    out << std::hex << std::setw(2) << std::setfill('0')
                        << static_cast<unsigned>(byte);
                }
                out << std::dec << std::setfill(' ') << '\n';
            }
        }
        for (const auto& m : p.world.monsters()) {
            h9_monster(out, m);
        }
        for (const auto& g : p.world.ground_items()) {
            h9_fields(out,
                      "ground",
                      g.id,
                      g.item_index,
                      g.prefix,
                      g.random_stat_type,
                      g.random_stat_value,
                      g.count,
                      g.x,
                      g.y,
                      g.map,
                      g.owner_player_id,
                      g.owner_party_id,
                      g.party_recipient_id,
                      g.server_owned,
                      g.ownership_left,
                      g.despawn_left);
        }
        auto rows = p.persistence.store->load_items(p.persistence.character_id);
        std::sort(
            rows.begin(), rows.end(), [](const auto& a, const auto& b) { return a.iid < b.iid; });
        for (const auto& row : rows) {
            out << "item-row";
            for (auto value : storage_values(row)) {
                out << ',' << value;
            }
            out << '\n';
        }
        const auto character = p.persistence.store->load_character(p.persistence.character_id);
        if (character) {
            out << "character-row," << storage_character_state(*character) << '\n';
        }
        auto buffs = p.persistence.store->load_buff_remains(p.persistence.character_id);
        std::sort(buffs.begin(), buffs.end(), [](const auto& a, const auto& b) {
            return a.buff_type < b.buff_type;
        });
        for (const auto& b : buffs) {
            h9_fields(out,
                      "buff-row",
                      b.character_id,
                      b.buff_type,
                      b.remain,
                      b.is_count,
                      b.stack_pack,
                      b.source_item_index);
        }
        for (const auto& u :
             p.persistence.store->load_inventory_unlocks(p.persistence.character_id)) {
            h9_fields(out, "unlock-row", u.character_id, u.register_index, u.remain);
        }
        for (const auto& row : p.audit()) {
            out << "audit," << row << '\n';
        }
        std::cout << out.str() << std::flush;
    }
    void h9_buff(ClientSession& c, SkillBuffId id, int value1, int value2 = 0) {
        std::int32_t remain = 0;
        std::uint64_t state = 0;
        expect(c.skill_buffs.apply(id, c.clazz, 1, 0, value1, value2, 100, remain, state),
               "H9 owns an untimed skill state without replacing a production clock");
    }
    std::size_t h9_position(const std::vector<std::vector<std::uint8_t>>& packets,
                            const std::vector<std::uint8_t>& frame) {
        const auto found = std::find(packets.begin(), packets.end(), frame);
        return static_cast<std::size_t>(std::distance(packets.begin(), found));
    }
    void h9_harvest_event(H9Players& p) {
        if (!p.world.find_monster(4000)) {
            auto plant = H9Players::monster(4000, 4);
            plant.def_index = 65000;
            plant.hp = plant.max_hp = 1;
            p.world.monsters().push_back(plant);
        }
        expect(p.world.begin_open_chest(700, 4000, 2) == 0,
               "H9 uses the real opening producer before harvest delivery");
        std::vector<PlayerPose> poses{{700, H9Players::kX, H9Players::kY, 0}};
        std::vector<MonsterCorpseEvent> corpses;
        p.world.tick(2, poses, p.moves, p.attacks, p.chests, corpses);
        expect(p.chests.size() == 1 && p.chests[0].kind == ChestOpenEvent::Kind::Completed &&
                   p.chests[0].def_index == 65000 && p.world.find_monster(4000)->hp > 0,
               "H9 a configured harvest completion stays alive until reward commitment");
        p.owner().opening_chest_id = 4000;
        p.owner().gstate |= protocol::kCgsOpenBox | protocol::kCgsHarvestPlant;
    }
    void test_h9_harvest_commit_and_failures() {
        for (int mode = 0; mode < 17; ++mode) {
            H9Players p;
            const bool merged = mode == 2 || mode == 3 || mode == 5;
            p.config.plants.types[0].items_per_pick = mode == 0 || mode == 2 ? 1 : 3;
            if (merged) {
                p.bag(23027, 5);
            }
            if (mode == 6 || mode == 16) {
                for (int i = 0; i < 72; ++i) {
                    p.bag(9000, 1, 10000 + static_cast<std::uint32_t>(i), i);
                }
            }
            if (mode == 16) {
                expect(p.owner().inventory_unlocks.apply(INT32_MAX, 100) &&
                           p.persistence.store->replace_inventory_unlocks(
                               p.owner().character_id,
                               {InventoryUnlockRecord{p.owner().character_id, 0, INT32_MAX}}),
                       "H9 owns a safely future third register before staging its full base bag");
            }
            h9_harvest_event(p);
            if (mode == 4 || mode == 14) {
                p.persistence.sql(
                    "CREATE TRIGGER h9_reject BEFORE INSERT ON items BEGIN SELECT RAISE(ABORT,'owned H9 harvest insert failure'); END;");
            } else if (mode == 5) {
                p.persistence.sql(
                    "CREATE TRIGGER h9_reject BEFORE UPDATE ON items BEGIN SELECT RAISE(ABORT,'owned H9 harvest update failure'); END;");
            } else if (mode == 7) {
                p.owner().phase = SessionPhase::WaitConnect;
            } else if (mode == 8) {
                p.owner().cur_hp = 0;
            } else if (mode == 9) {
                p.owner().map = 1;
            } else if (mode == 10) {
                p.owner().px = H9Players::kX + 1024;
            } else if (mode == 11) {
                p.world.monsters().clear();
            } else if (mode == 13) {
                p.owner().character_id = 0;
            } else if (mode == 15) {
                p.outputs[0]->close();
            }
            p.clear_audit();
            const auto before_bag = combat_inventory_state(p.owner());
            const auto before_rows = p.item_rows();
            const bool success = mode <= 3 || mode == 12 || mode == 13 || mode == 15 || mode == 16;
            auto r = p.run(947, mode == 12);
            const auto feedback = protocol::ItemMonster::frame_message(static_cast<std::uint8_t>(
                success ? protocol::ItemMonsterMsg::Opened : protocol::ItemMonsterMsg::Canceled));
            expect(
                p.world.ground_items().empty() &&
                    p.quantity(23027) ==
                        (merged ? 5 : 0) +
                            (success ? p.config.plants.types[0].items_per_pick : 0) &&
                    (success ? !p.world.find_monster(4000)
                             : mode == 11 || (p.world.find_monster(4000) &&
                                              p.world.find_monster(4000)->hp == 1 &&
                                              p.world.find_monster(4000)->open_player_id == 0 &&
                                              p.world.find_monster(4000)->open_left == 0 &&
                                              p.world.find_monster(4000)->open_remove_left == 0)),
                "H9 harvest commits one staged stack only on success, otherwise restores the surviving plant without ground loot");
            if (mode == 16) {
                expect(
                    p.owner().bag[72].index == 23027 && p.owner().bag[72].count == 3 &&
                        p.owner().inventory_unlocks.entries().size() == 1,
                    "H9 staging retains an active third register; persisted slot remains bag-kind0, not numeric bag position72");
            }
            if (!success) {
                expect(combat_inventory_state(p.owner()) == before_bag,
                       "H9 rejected or failed harvest cannot partially mutate the real bag");
            }
            expect(
                p.next_iid.load() ==
                    ((success && !merged) || mode == 4 || mode == 14 ? 9001u : 9000u),
                "H9 new-item attempts consume one allocator identity even if persistence fails; merge/rejection does not");
            if (mode != 7) {
                expect(p.owner().opening_chest_id == 0 &&
                           (p.owner().gstate &
                            (protocol::kCgsOpenBox | protocol::kCgsHarvestPlant)) == 0,
                       "H9 successful or canceled harvest clears both opener compatibility states");
                if (mode != 15) {
                    const auto state = protocol::ItemMonster::frame_gstate(700, p.owner().gstate);
                    expect(h9_position(r.packets[0], state) < h9_position(r.packets[0], feedback) &&
                               h9_position(r.packets[0], feedback) < r.packets[0].size(),
                           "H9 opener state precedes exact success/cancel feedback");
                    if (success) {
                        expect(
                            !r.packets[0].empty() && r.packets[0][0].size() >= 3 &&
                                r.packets[0][0][2] ==
                                    static_cast<std::uint8_t>(protocol::S2C::InsertItem) &&
                                r.packets[0].back() == frame_remove_monster(4000),
                            "H9 bag insertion is visible before state/feedback and final plant removal");
                    }
                }
            }
            if (mode == 15) {
                expect(
                    r.packets[0].empty() && p.owner().sock && !p.owner().sock->alive() &&
                        p.owner().known_monsters.count(4000) == 0,
                    "H9 closed transport suppresses wire but does not roll back committed harvest or retained-ID removal");
            }
            const auto rows = p.item_rows();
            const bool durable_success = success && mode != 12 && mode != 13;
            expect(
                p.audit() == (durable_success
                                  ? std::vector<std::string>{std::string(merged ? "U,8000,"
                                                                                : "I,9001,") +
                                                             std::to_string((merged ? 5 : 0) +
                                                                            p.config.plants.types[0]
                                                                                .items_per_pick) +
                                                             ",0"}
                                  : std::vector<std::string>{}) &&
                    rows.size() == before_rows.size() + (durable_success && !merged ? 1u : 0u),
                "H9 persists precisely one full inserted/updated row before output or leaves committed rows unchanged");
            if (!durable_success) {
                expect(
                    rows == before_rows,
                    "H9 failed/no-store/invalid-character paths preserve all30 stored item fields");
            }
            h9_observe("harvest-commit-policy-" + std::to_string(mode), p, r);
            if (mode == 14) {
                p.persistence.sql("DROP TRIGGER h9_reject;");
                h9_harvest_event(p);
                p.clear_audit();
                auto retry = p.run();
                expect(
                    p.quantity(23027) == 3 && p.next_iid.load() == 9002 &&
                        !p.world.find_monster(4000) &&
                        p.audit() == std::vector<std::string>{"I,9002,3,0"},
                    "H9 a failed opening can legally complete again with a new identity, without replaying its failed row");
                h9_observe("harvest-legal-reopen-after-store-failure", p, retry);
            }
        }
    }
    void test_h9_ordinary_chest_and_removal() {
        for (int mode = 0; mode < 5; ++mode) {
            H9Players p;
            auto m = H9Players::monster(4000, 4);
            m.hp = 0;
            m.open_remove_left = 1.2f;
            p.world.monsters().push_back(m);
            p.owner().opening_chest_id = 4000;
            p.owner().gstate = protocol::kCgsOpenBox;
            p.chests = {{mode < 3 ? ChestOpenEvent::Kind::Completed : ChestOpenEvent::Kind::Remove,
                         4000,
                         700,
                         4,
                         m.x,
                         m.y}};
            if (mode == 1) {
                p.owner().phase = SessionPhase::WaitConnect;
            }
            if (mode == 2 || mode == 4) {
                for (auto& c : p.clients) {
                    c.known_monsters.erase(4000);
                }
            }
            auto r = p.run();
            if (mode < 3) {
                expect(
                    p.world.ground_items().size() == 1 && p.next_iid.load() == 9001 &&
                        p.world.ground_items()[0].item_index == 9000 &&
                        p.world.ground_items()[0].owner_player_id == 700 &&
                        p.world.find_monster(4000),
                    "H9 ordinary completed chests use cached opener identity and ground-drop semantics, unlike harvest");
                if (mode == 0) {
                    const auto& g = p.world.ground_items()[0];
                    expect(
                        h9_position(r.packets[0],
                                    protocol::ItemMonster::frame_message(static_cast<std::uint8_t>(
                                        protocol::ItemMonsterMsg::Opened))) <
                                h9_position(r.packets[0], frame_create_item(g)) &&
                            h9_position(r.packets[0], frame_create_item(g)) <
                                h9_position(r.packets[0], protocol::Skill::frame_action_die(4000)),
                        "H9 ordinary opener feedback precedes ground creation, which precedes known-monster death");
                }
                if (mode == 2) {
                    expect(
                        std::find(r.packets[0].begin(),
                                  r.packets[0].end(),
                                  protocol::Skill::frame_action_die(4000)) == r.packets[0].end(),
                        "H9 H8-already-removed chest knowledge suppresses only the later death action");
                }
            } else {
                expect(
                    !p.world.find_monster(4000) && p.world.ground_items().empty() &&
                        p.next_iid.load() == 9000 && p.clients[0].known_monsters.count(4000) == 0 &&
                        r.packets[0] ==
                            (mode == 3
                                 ? std::vector<std::vector<std::uint8_t>>{frame_remove_monster(
                                       4000)}
                                 : std::vector<std::vector<std::uint8_t>>{}),
                    "H9 removal erases actual entities after optional known output, retaining already-removed suppression");
            }
            h9_observe("ordinary-chest-policy-" + std::to_string(mode), p, r);
        }
    }
    void test_h9_despawn_move_prepare_order() {
        for (int mode = 0; mode < 4; ++mode) {
            H9Players p;
            for (auto& c : p.clients) {
                c.known_items = {9001};
            }
            p.despawned_items = {9001};
            p.moves = {{4000, H9Players::kX, H9Players::kY, 4, -2, false, true},
                       {4000, H9Players::kX + 4, H9Players::kY - 2, 0, 0, true, false}};
            p.attacks = {
                {MonsterAttackEvent::Kind::SkillPrepare, 4000, mode == 1 ? 700u : 0u, 34, 1, 0}};
            if (mode == 1) {
                p.owner().known_monsters.clear();
            }
            if (mode == 2) {
                p.outputs[1]->close();
            }
            if (mode == 3) {
                p.clients[1].sock.reset();
            }
            const auto old_inputs =
                std::make_tuple(p.moves.size(), p.attacks.size(), p.despawned_items);
            auto r = p.run();
            const auto removed = frame_remove_item(9001);
            const auto on = frame_move_monster_on(4000, 4, -2, true);
            const auto end = frame_move_monster_end(4000, 0, 0, false);
            const auto prepare = protocol::Skill::frame_action_skill(4000, 34, mode == 1 ? 700 : 0);
            expect(
                r.packets[0] ==
                        (mode == 1
                             ? std::vector<std::vector<std::uint8_t>>{removed, prepare}
                             : std::vector<std::vector<std::uint8_t>>{removed, on, end, prepare}) &&
                    r.packets[2] ==
                        std::vector<std::vector<std::uint8_t>>{removed, on, end, prepare} &&
                    r.packets[3] == r.packets[2] &&
                    r.packets[4] == std::vector<std::vector<std::uint8_t>>{removed} &&
                    r.packets[5].empty(),
                "H9 despawn then authored moves then prepare preserve known-or-target audiences independently of map/distance");
            expect(
                p.clients[0].known_items.empty() && p.clients[5].known_items.count(9001) == 1 &&
                    p.owner().cur_hp == 1000 &&
                    std::make_tuple(p.moves.size(), p.attacks.size(), p.despawned_items) ==
                        old_inputs &&
                    p.moves[0].dx == 4 && p.moves[0].dy == -2 && p.moves[1].ended,
                "H9 does not consume or rewrite caller-owned lists, and non-Playing retained knowledge stays distinct");
            if (mode == 2 || mode == 3) {
                expect(
                    r.packets[1].empty() &&
                        p.clients[1].known_items.count(9001) == (mode == 3 ? 1u : 0u),
                    "H9 dead-socket enqueue failure still erases known IDs while a missing socket skips the erase");
            }
            h9_observe("despawn-move-prepare-policy-" + std::to_string(mode), p, r);
        }
        {
            H9Players p;
            h9_harvest_event(p);
            p.despawned_items = {9001};
            p.owner().known_items.insert(9001);
            p.moves = {{4001, H9Players::kX, H9Players::kY, 1, 2, false, false}};
            p.attacks = {{MonsterAttackEvent::Kind::SkillPrepare, 4001, 700, 29, 1, 0}};
            p.clear_audit();
            auto r = p.run();
            const auto removed_plant = frame_remove_monster(4000);
            const auto removed_item = frame_remove_item(9001);
            const auto move = frame_move_monster_on(4001, 1, 2, false);
            const auto prepare = protocol::Skill::frame_action_skill(4001, 29, 700);
            expect(
                h9_position(r.packets[0], removed_plant) <
                        h9_position(r.packets[0], removed_item) &&
                    h9_position(r.packets[0], removed_item) < h9_position(r.packets[0], move) &&
                    h9_position(r.packets[0], move) < h9_position(r.packets[0], prepare) &&
                    p.quantity(23027) == 3 && p.owner().cur_hp == 1000,
                "H9 one mixed delivery keeps harvest/store output before despawn, move and attack phases");
            h9_observe("mixed-four-phase-output-order", p, r);
        }
    }
    void test_h9_fallback_attack_and_target_gates() {
        for (int mode = 0; mode < 14; ++mode) {
            H9Players p;
            p.world.monsters().push_back(H9Players::monster());
            p.attacks = {{MonsterAttackEvent::Kind::Basic,
                          4000,
                          700,
                          0,
                          1,
                          mode == 1   ? -8
                          : mode == 2 ? 0
                                      : 13}};
            if (mode == 3) {
                p.world.monsters()[0].map = 1;
            }
            if (mode == 4) {
                p.world.monsters().clear();
            }
            if (mode == 5) {
                p.owner().cur_hp = 0;
            }
            if (mode == 6) {
                p.attacks[0].target_player_id = 999;
            }
            if (mode == 7) {
                p.owner().phase = SessionPhase::WaitConnect;
            }
            if (mode == 8) {
                p.owner().known_monsters.clear();
            }
            if (mode == 9) {
                p.world.monsters()[0].hp = 0;
            }
            if (mode == 10) {
                p.outputs[0]->close();
            }
            if (mode == 11 || mode == 12) {
                p.owner().resting = true;
                p.owner().pending_transform_stage = 2;
                expect(p.owner().rest_food.apply_hp(7, 4, 9000),
                       "H9 owns an active food channel before incoming damage");
            }
            if (mode == 12 || mode == 13) {
                p.owner().cur_hp = 5;
                auto chasing = H9Players::monster(4001);
                chasing.aggro = true;
                chasing.aggro_player_id = 700;
                chasing.hostility_by_player = {{700, 7}};
                chasing.reward_damage_by_player = {{700, 2}};
                chasing.moving = true;
                chasing.move_busy_left = 2;
                p.world.monsters().push_back(chasing);
            }
            if (mode == 13) {
                p.attacks.push_back({MonsterAttackEvent::Kind::Basic, 4001, 700, 0, 1, 43});
            }
            const bool positive = mode == 0 || mode >= 8;
            const int dealt = positive ? 13 : 0;
            const int hp = mode == 5 || mode >= 12 ? 0 : 1000 - dealt;
            auto r = p.run();
            expect(
                p.owner().cur_hp == hp && p.attacks.size() == (mode == 13 ? 2u : 1u) &&
                    p.next_iid.load() == 9000,
                "H9 actual zero-stat fallback respects missing/dead/map/phase gates and never applies more than the first lethal hit");
            const auto attack = protocol::Attack::frame_server(
                4000, mode == 6 ? 999 : 700, dealt, 0, dealt > 0 ? protocol::kAtkHit : 0);
            if (mode != 7 && mode != 10) {
                expect(
                    !r.packets[0].empty() && r.packets[0][0] == attack,
                    "H9 primary wire exists even for zero result or unknown monster, before any HP/rest/death effects");
            }
            expect(
                !r.packets[2].empty() && r.packets[2][0] == attack && !r.packets[3].empty() &&
                    r.packets[3][0] == attack,
                "H9 known-monster attack audience is independent of player death's spatial map/sight audience");
            if (positive && mode != 10) {
                const auto property = protocol::Property::frame_update(
                    static_cast<std::uint8_t>(protocol::PropertyType::CurHp), hp);
                expect(h9_position(r.packets[0], attack) < h9_position(r.packets[0], property) &&
                           h9_position(r.packets[0], property) < r.packets[0].size(),
                       "H9 authoritative HP follows the primary attack packet");
            }
            if (mode == 11 || mode == 12) {
                expect(
                    !p.owner().resting && !p.owner().rest_food.active() &&
                        p.owner().pending_transform_stage == 0,
                    "H9 positive damage cancels pending transform and existing rest/food without a new deadline");
            }
            if (mode >= 12) {
                const auto die = protocol::Skill::frame_action_die(700);
                const auto stopped = frame_move_monster_end(4001, 0, 0, false);
                expect(
                    h9_position(r.packets[0], die) < r.packets[0].size() &&
                        h9_position(r.packets[0], stopped) == r.packets[0].size() &&
                        std::find(r.packets[2].begin(), r.packets[2].end(), die) ==
                            r.packets[2].end() &&
                        std::find(r.packets[3].begin(), r.packets[3].end(), die) ==
                            r.packets[3].end() &&
                        h9_position(r.packets[4], die) < r.packets[4].size() &&
                        h9_position(r.packets[4], attack) == r.packets[4].size() &&
                        !p.world.find_monster(4001)->aggro &&
                        p.world.find_monster(4001)->hostility_by_player.empty() &&
                        p.world.find_monster(4001)->moving &&
                        p.world.find_monster(4001)->move_busy_left == 2,
                    "H9 lethal delivery clears aggro but preserves a committed movement segment when no pending dirty stop exists");
                if (mode == 13) {
                    expect(
                        r.packets[0].back() == protocol::Attack::frame_server(4001, 700, 0, 0, 0) &&
                            std::count(r.packets[0].begin(), r.packets[0].end(), die) == 1,
                        "H9 a second already-queued monster swing keeps zero primary animation without duplicate player death");
                }
            }
            h9_observe("fallback-attack-policy-" + std::to_string(mode), p, r);
        }
    }
    template <typename Predicate> unsigned h9_seed_for(Predicate&& wanted) {
        for (unsigned seed = 0; seed < 10000; ++seed) {
            std::srand(seed);
            if (wanted()) {
                return seed;
            }
        }
        expect(false, "H9 finds a bounded same-runtime RNG seed for an existing domain branch");
        return 947;
    }
    void test_h9_physical_and_monster_magic() {
        for (int mode = 0; mode < 9; ++mode) {
            H9Players p;
            p.owner().cur_hp = p.owner().max_hp = 1000000;
            p.world.monsters().push_back(H9Players::monster(4000, 2));
            if (mode == 1) {
                p.owner().perfect_evasion_base = 100;
            }
            if (mode == 2) {
                p.owner().wearing_shield = true;
                p.owner().block_percent = 100;
            }
            const bool magic = mode >= 3;
            const int skill = magic ? 29 + mode - 3 : 0;
            p.attacks = {
                {magic ? MonsterAttackEvent::Kind::SkillFire : MonsterAttackEvent::Kind::Basic,
                 4000,
                 700,
                 static_cast<std::uint8_t>(skill),
                 1,
                 1}};
            const auto attacker = monster_combatant(p.world.monsters()[0], p.world.find_def(2));
            const auto target = make_session_combatant(p.owner(), p.owner());
            const auto evaluate = [&] {
                return magic ? combat::resolve_monster_magic(
                                   attacker, target, skill, 1, p.world.find_def(2)->intelligence)
                             : combat::resolve_physical(attacker, target);
            };
            const unsigned seed = h9_seed_for([&] {
                const auto value = evaluate();
                return mode == 1   ? !value.hit && !value.blocked
                       : mode == 2 ? value.blocked
                                   : value.hit && value.damage > 0;
            });
            std::srand(seed);
            const auto expected = evaluate();
            const auto next = std::rand();
            auto r = p.run(seed);
            const auto packet =
                magic ? protocol::Skill::frame_server_damage(static_cast<std::uint8_t>(skill),
                                                             4000,
                                                             700,
                                                             1,
                                                             expected.damage,
                                                             0,
                                                             pack_attack_flags(expected))
                      : protocol::Attack::frame_server(
                            4000, 700, expected.damage, 0, pack_attack_flags(expected));
            expect(
                p.owner().cur_hp == std::max(0, 1000000 - expected.damage) &&
                    !r.packets[0].empty() && r.packets[0][0] == packet && r.next_random == next,
                "H9 selects the actual physical or29..34 magic domain operation with unchanged flags, HP and RNG consumption");
            expect(
                p.attacks.size() == 1 && p.attacks[0].damage == 1,
                "H9 positive-stat/magic resolution ignores producer fallback damage without rewriting its event");
            h9_observe("domain-damage-" + std::to_string(mode), p, r);
        }
        {
            H9Players p;
            p.world.monsters().push_back(H9Players::monster());
            p.attacks = {{MonsterAttackEvent::Kind::SkillFire, 4000, 700, 35, 2, 13}};
            auto r = p.run();
            expect(p.owner().cur_hp == 987 &&
                       r.packets[0][0] == protocol::Skill::frame_server_damage(
                                              35, 4000, 700, 2, 13, 0, protocol::kAtkHit),
                   "H9 unrecognized magic index retains the zero-stat fallback and skill payload");
            h9_observe("unrecognized-skill-fallback", p, r);
        }
    }
    void test_h9_mitigation_and_counted_defense() {
        for (int mode = 0; mode < 9; ++mode) {
            H9Players p;
            p.world.monsters().push_back(H9Players::monster());
            p.attacks = {{MonsterAttackEvent::Kind::Basic, 4000, 700, 0, 1, 13}};
            if (mode == 0) {
                p.owner().godmode = true;
            }
            if (mode == 1) {
                h9_buff(p.owner(), SkillBuffId::KnightPerfectDefense, 50);
            }
            if (mode == 2) {
                h9_buff(p.owner(), SkillBuffId::MageBlessing, 1);
            }
            const bool thief = mode == 3 || mode == 4;
            if (thief) {
                p.owner().clazz = combat::kPcThief;
            }
            if (mode == 4) {
                p.owner().learned_skills[26] = 1;
            }
            const bool counted = mode >= 5;
            if (counted) {
                p.owner().pay_buffs.load(
                    {BuffRemainRecord{p.owner().character_id,
                                      static_cast<int>(protocol::BuffType::PayPerfectDef),
                                      mode == 7 ? 1 : 3,
                                      1,
                                      100,
                                      0}},
                    100);
                expect(p.owner().pay_buffs.has(protocol::kCbsPayPerfectDef) &&
                           persist_pay_buffs(p.persistence.store.get(), p.owner()),
                       "H9 owns durable counted Perfect Defense without a timed row");
                if (mode == 8) {
                    p.persistence.sql(
                        "CREATE TRIGGER h9_reject_buff BEFORE INSERT ON buff_remain BEGIN SELECT RAISE(ABORT,'owned H9 counted defense failure'); END;");
                }
            }
            const unsigned seed = counted || mode == 4 ? h9_seed_for([&] {
                const int chance = combat::irand(1, 100);
                return mode == 5 ? chance > 10 : mode == 4 ? chance <= 5 : chance <= 10;
            })
                                                       : 947;
            const int expected_damage = mode == 0 || mode == 2 || mode >= 6 ? 0
                                        : mode == 1                         ? 7
                                        : mode == 4                         ? 10
                                                                            : 13;
            std::srand(seed);
            if (thief || counted) {
                (void) std::rand();
            }
            const int expected_next = std::rand();
            p.clear_audit();
            auto r = p.run(seed);
            const bool consumed = mode >= 6;
            expect(
                p.owner().cur_hp == 1000 - expected_damage && r.next_random == expected_next &&
                    r.packets[0].back() ==
                        (expected_damage > 0
                             ? protocol::Property::frame_update(
                                   static_cast<std::uint8_t>(protocol::PropertyType::CurHp),
                                   1000 - expected_damage)
                             : protocol::Attack::frame_server(
                                   4000, 700, 0, 0, consumed ? 0 : protocol::kAtkHit)),
                "H9 mitigation retains hit flags for god/self defense, clears them only on counted proc and preserves eligible Thief draws");
            if (counted) {
                const auto rows =
                    p.persistence.store->load_buff_remains(p.persistence.character_id);
                const int expected_live = mode == 5 ? 3 : mode == 7 ? 0 : 2;
                expect(
                    p.owner().pay_buffs.has(protocol::kCbsPayPerfectDef) == (expected_live > 0) &&
                        (expected_live == 0 ||
                         p.owner().pay_buffs.entries()[0].charges == expected_live) &&
                        rows.size() == (mode == 7 ? 0u : 1u) &&
                        (rows.empty() || rows[0].remain == (mode == 8 ? 3 : expected_live)),
                    "H9 counted-defense save failure preserves durable old charges while live damage prevention and one-charge consumption remain applied");
            }
            h9_observe("mitigation-counted-defense-" + std::to_string(mode), p, r);
        }
    }
    void test_h9_lord_order_and_secondary_clamp() {
        for (int mode = 0; mode < 4; ++mode) {
            H9Players p;
            p.world.monsters().push_back(H9Players::monster());
            p.lord_id = 4000;
            p.attacks = {{MonsterAttackEvent::Kind::Basic, 4000, 700, 0, 1, 13}};
            if (mode == 1) {
                p.clients[1].cur_hp = 0;
            }
            if (mode == 2) {
                p.clients[1].godmode = true;
            }
            if (mode == 3) {
                p.owner().phase = SessionPhase::WaitConnect;
            }
            auto r = p.run();
            expect(
                p.owner().cur_hp == (mode == 3 ? 1000 : 987) &&
                    p.clients[1].cur_hp == (mode == 1                ? 0
                                            : mode == 2 || mode == 3 ? 1000
                                                                     : 987) &&
                    p.clients[4].cur_hp == (mode == 3 ? 1000 : 987) &&
                    p.clients[2].cur_hp == 1000 && p.clients[3].cur_hp == 1000,
                "H9 Lord fanout follows first Playing primary and authored same-map one-tile clients, including dead targets without damaging them");
            if (mode != 3) {
                const auto secondary = protocol::Effect::frame_damaged(704, 13);
                expect(
                    h9_position(r.packets[4], secondary) == 0 &&
                        h9_position(r.packets[0], secondary) == r.packets[0].size(),
                    "H9 secondary damage is direct-to-victim, including an unknown-attacker nearby player");
            }
            if (mode == 2) {
                expect(
                    h9_position(r.packets[1], protocol::Effect::frame_damaged(701, 0)) <
                        r.packets[1].size(),
                    "H9 zero mitigated secondary still presents Damaged when hit_ok remains true");
            }
            h9_observe("lord-secondary-policy-" + std::to_string(mode), p, r);
        }
        {
            H9Players p;
            const std::string json =
                R"({"index":2,"name":"Owned H9 high magic","stats":{"race":1,"level":1,"hp":10000,"exp":0,"ai":1,"attack":[0, 0, 0],"int":100000},"spawns":[]})";
            expect(
                test::write_bytes(p.persistence.root + "/monsters/2.json",
                                  {json.begin(), json.end()}) &&
                    p.world.load_monsters_json_dir(
                        p.persistence.root + "/monsters", H9Players::kX, H9Players::kY, 0),
                "H9 loads a valid bounded authored high-magic definition for wire-width coverage");
            p.world.monsters().push_back(H9Players::monster(4000, 2));
            p.lord_id = 4000;
            for (auto& c : p.clients) {
                c.cur_hp = c.max_hp = 10000000;
            }
            p.owner().godmode = true;
            p.attacks = {{MonsterAttackEvent::Kind::SkillFire, 4000, 700, 31, 1, 0}};
            auto r = p.run();
            expect(
                p.clients[1].cur_hp < 10000000 - 65535 && p.clients[4].cur_hp < 10000000 - 65535 &&
                    h9_position(r.packets[1], protocol::Effect::frame_damaged(701, 65535)) <
                        r.packets[1].size() &&
                    h9_position(r.packets[4], protocol::Effect::frame_damaged(704, 65535)) == 0 &&
                    p.owner().cur_hp == 10000000,
                "H9 valid large secondary damage clamps only the uint16 wire effect while applying full HP damage and primary godmode");
            h9_observe("lord-secondary-uint16-clamp", p, r);
        }
    }
    combat::AttackResult h9_counter_domain(const H9Players& p) {
        const auto& c = p.clients[0];
        const auto* m = p.world.find_monster(4000);
        if (!m) {
            expect(false, "H9 counter seed selection has one owned enemy");
            return {};
        }
        const auto attacker = make_session_combatant(c, c);
        const auto defender = monster_combatant(*m, p.world.find_def(m->def_index));
        const int raw =
            protocol::thief_counter_power(combat::roll_attack(attacker), 1, combat::irand(1, 2));
        return combat::resolve_physical(
            attacker, defender, 0, 0, raw, 1, std::max(1, c.attack_speed_ms));
    }
    void test_h9_counter_order_and_world_rejection() {
        for (int mode = 0; mode < 11; ++mode) {
            H9Players p;
            p.owner().clazz = combat::kPcThief;
            p.owner().min_attack = p.owner().max_attack = 10000;
            auto m = H9Players::monster(4000, mode == 5 ? 3 : 1);
            if (mode >= 4 && mode != 6 && mode != 7 && mode != 8) {
                m.hp = m.max_hp = 1;
            }
            if (mode == 6) {
                m.spawn_protection_left = 2;
            }
            if (mode == 7) {
                m.race = static_cast<int>(protocol::MonsterRace::Item);
            }
            if (mode == 8) {
                m.moving = true;
                m.move_busy_left = 2;
            }
            p.world.monsters().push_back(m);
            if (mode != 0) {
                p.owner().learned_skills[20] = 1;
            }
            if (mode != 1) {
                h9_buff(p.owner(), SkillBuffId::ThiefCounterAttack, 1);
            }
            if (mode == 8 || mode == 10) {
                p.owner().cur_hp = 5;
            }
            if (mode == 9) {
                p.lord_id = 4000;
            }
            p.attacks = {{MonsterAttackEvent::Kind::Basic, 4000, 700, 0, 1, 13}};
            const bool counter = mode >= 3;
            const unsigned seed = mode < 2 ? 947 : h9_seed_for([&] {
                (void) combat::irand(1, 100);  // Rank0 Acrobatic still draws before Counter Attack.
                const bool proc = protocol::thief_counter_triggers(combat::irand(1, 1000));
                if (!counter) {
                    return !proc;
                }
                if (!proc) {
                    return false;
                }
                const auto result = h9_counter_domain(p);
                return result.hit && result.damage > 0;
            });
            combat::AttackResult expected;
            if (counter) {
                std::srand(seed);
                (void) combat::irand(1, 100);
                (void) combat::irand(1, 1000);
                expected = h9_counter_domain(p);
            }
            const auto before_hp = p.world.monsters()[0].hp;
            p.clear_audit();
            auto r = p.run(seed);
            const auto primary =
                protocol::Attack::frame_server(4000, 700, 13, 0, protocol::kAtkHit);
            expect(
                !r.packets[0].empty() && r.packets[0][0] == primary &&
                    p.owner().cur_hp == (mode == 8 || mode == 10 ? 0 : 987),
                "H9 incoming hit and HP subtraction precede counter publication even when the victim has just died");
            const auto reflected =
                protocol::Skill::frame_server_damage(20, 700, 4000, 1, expected.damage, 0, 0);
            if (counter) {
                const auto hp = protocol::Property::frame_update(
                    static_cast<std::uint8_t>(protocol::PropertyType::CurHp), p.owner().cur_hp);
                expect(
                    h9_position(r.packets[0], primary) < h9_position(r.packets[0], reflected) &&
                        h9_position(r.packets[0], reflected) < h9_position(r.packets[0], hp),
                    "H9 reflected positive damage is reported after primary and before victim HP independently of world acceptance");
                if (mode == 6 || mode == 7) {
                    expect(
                        p.world.find_monster(4000)->hp == before_hp &&
                            p.world.ground_items().empty() && p.owner().exp == 0,
                        "H9 spawn protection and non-damageable race can reject application while positive counter wire remains");
                } else if (mode == 3 || mode == 8) {
                    expect(p.world.find_monster(4000)->hp == before_hp - expected.damage &&
                               p.owner().exp == 0 && p.next_iid.load() == 9000,
                           "H9 positive nonlethal counter applies once without premature reward");
                    if (mode == 8) {
                        const auto die = protocol::Skill::frame_action_die(700);
                        const auto stop = frame_move_monster_end(4000, 0, 0, false);
                        expect(
                            h9_position(r.packets[0], hp) < h9_position(r.packets[0], die) &&
                                h9_position(r.packets[0], die) < h9_position(r.packets[0], stop) &&
                                h9_position(r.packets[0], stop) < r.packets[0].size() &&
                                !p.world.find_monster(4000)->moving &&
                                !p.world.find_monster(4000)->dirty_stop,
                            "H9 lethal incoming damage still counters a moving enemy; its newly pending stop is broadcast after player death");
                    }
                } else {
                    const auto action = mode == 5 ? protocol::Skill::frame_action_knee(4000)
                                                  : protocol::Skill::frame_action_die(4000);
                    expect(
                        p.world.find_monster(4000)->hp == 0 &&
                            h9_position(r.packets[0], reflected) <
                                h9_position(r.packets[0], action) &&
                            h9_position(r.packets[0], action) < h9_position(r.packets[0], hp) &&
                            p.owner().exp == (mode == 10 ? 0 : 3) &&
                            p.world.ground_items().size() == 1 && p.next_iid.load() == 9001 &&
                            p.world.find_monster(4000)->hostility_by_player.empty() &&
                            p.world.find_monster(4000)->reward_damage_by_player.empty(),
                        "H9 counter corpse/action and one3EXP-or-dead-skip reward consume hostility before final HP; ground loot remains separately owned");
                    if (mode == 5) {
                        expect(
                            (p.world.find_monster(4000)->gstate & protocol::kCgsKnee) != 0 &&
                                p.world.find_monster(4000)->knee_left > 0,
                            "H9 Maguni keeps the beheadable corpse rather than ordinary death timing");
                    }
                    if (mode == 10) {
                        const auto die = protocol::Skill::frame_action_die(700);
                        expect(
                            h9_position(r.packets[0], hp) < h9_position(r.packets[0], die) &&
                                h9_position(r.packets[0], die) < r.packets[0].size(),
                            "H9 a dead counter attacker receives no EXP resurrection and still completes player death after counter reward");
                    }
                    if (mode == 9) {
                        expect(
                            p.clients[1].cur_hp == 987 && p.clients[4].cur_hp == 987 &&
                                h9_position(r.packets[1], reflected) <
                                    h9_position(r.packets[1],
                                                protocol::Effect::frame_damaged(701, 13)) &&
                                p.world.ground_items()[0].owner_player_id == 700,
                            "H9 later Lord splash snapshots retain corpse statistics after primary counter kills it, without a second counter reward");
                    }
                }
            } else {
                expect(
                    p.world.find_monster(4000)->hp == before_hp && p.world.ground_items().empty() &&
                        p.owner().exp == 0 &&
                        h9_position(r.packets[0], reflected) == r.packets[0].size(),
                    "H9 missing learned rank/buff and failed chance cannot create counter output or mutation");
            }
            h9_observe("counter-policy-" + std::to_string(mode), p, r);
        }
    }
    void test_h9_world_events() {
        expect(g_exp_event == nullptr,
               "H9 direct owned event fixture has no unrelated global EXP controller");
        test_h9_harvest_commit_and_failures();
        test_h9_ordinary_chest_and_removal();
        test_h9_despawn_move_prepare_order();
        test_h9_fallback_attack_and_target_gates();
        test_h9_physical_and_monster_magic();
        test_h9_mitigation_and_counted_defense();
        test_h9_lord_order_and_secondary_clamp();
        test_h9_counter_order_and_world_rejection();
    }

}  // namespace

int main(int argc, char** argv) {
    if (argc > 1 && (std::string_view(argv[1]) == "--world-events-snapshot" ||
                     std::string_view(argv[1]) == "--world-events-tests")) {
        h9_snapshot = std::string_view(argv[1]) == "--world-events-snapshot";
        if (h9_snapshot) {
            log::set_level(log::Level::warn);
        }
        test_h9_world_events();
        return test::g_failures ? 1 : 0;
    }
    if (argc > 1 && (std::string_view(argv[1]) == "--world-step-snapshot" ||
                     std::string_view(argv[1]) == "--world-step-tests")) {
        h8_snapshot = std::string_view(argv[1]) == "--world-step-snapshot";
        if (h8_snapshot) {
            log::set_level(log::Level::warn);
        }
        test_h8_world_step();
        return test::g_failures ? 1 : 0;
    }
    if (argc > 1 && (std::string_view(argv[1]) == "--mage-persistent-snapshot" ||
                     std::string_view(argv[1]) == "--mage-persistent-tests")) {
        h7_snapshot = std::string_view(argv[1]) == "--mage-persistent-snapshot";
        if (h7_snapshot) {
            log::set_level(log::Level::warn);
        }
        test_h7_mage_persistent();
        return test::g_failures ? 1 : 0;
    }
    if (argc > 1 && (std::string_view(argv[1]) == "--archer-persistent-snapshot" ||
                     std::string_view(argv[1]) == "--archer-persistent-tests")) {
        h6_snapshot = std::string_view(argv[1]) == "--archer-persistent-snapshot";
        if (h6_snapshot) {
            log::set_level(log::Level::warn);
        }
        test_h6_archer_persistent();
        return test::g_failures ? 1 : 0;
    }
    if (argc > 1 && (std::string_view(argv[1]) == "--h5-snapshot" ||
                     std::string_view(argv[1]) == "--h5-tests")) {
        h5_snapshot = std::string_view(argv[1]) == "--h5-snapshot";
        if (h5_snapshot) {
            log::set_level(log::Level::warn);
        }
        test_h5_doggebi();
        return test::g_failures ? 1 : 0;
    }
    if (argc > 1 && (std::string_view(argv[1]) == "--h4-snapshot" ||
                     std::string_view(argv[1]) == "--h4-tests")) {
        h4_snapshot = std::string_view(argv[1]) == "--h4-snapshot";
        if (h4_snapshot) {
            log::set_level(log::Level::warn);
        }
        test_routing_boundaries();
        return test::g_failures ? 1 : 0;
    }
    if (argc > 1 && (std::string_view(argv[1]) == "--h3-snapshot" ||
                     std::string_view(argv[1]) == "--h3-tests")) {
        h3_snapshot = std::string_view(argv[1]) == "--h3-snapshot";
        if (h3_snapshot) {
            log::set_level(log::Level::warn);
        }
        test_shop_requests();
        return test::g_failures ? 1 : 0;
    }
    if (argc > 1 && (std::string_view(argv[1]) == "--storage-snapshot" ||
                     std::string_view(argv[1]) == "--storage-tests")) {
        h2_snapshot = std::string_view(argv[1]) == "--storage-snapshot";
        if (h2_snapshot) {
            // Keep WARN/ERROR and raw observations; omit incidental INFO temp-path messages.
            log::set_level(log::Level::warn);
        }
        test_storage_requests();
        return test::g_failures ? 1 : 0;
    }
    if (argc > 1 && (std::string_view(argv[1]) == "--h1-upkeep-snapshot" ||
                     std::string_view(argv[1]) == "--h1-upkeep-tests")) {
        h1_snapshot = std::string_view(argv[1]) == "--h1-upkeep-snapshot";
        test_h1_upkeep();
        return test::g_failures ? 1 : 0;
    }
    if (argc > 1 && (std::string_view(argv[1]) == "--scheduled-player-snapshot" ||
                     std::string_view(argv[1]) == "--scheduled-player-tests")) {
        scheduled_snapshot = std::string_view(argv[1]) == "--scheduled-player-snapshot";
        test_scheduled_player_phases();
        return test::g_failures ? 1 : 0;
    }
    if (argc > 1 && std::string_view(argv[1]) == "--combat-snapshot") {
        combat_snapshot();
        return test::g_failures ? 1 : 0;
    }
    if (argc > 1 && std::string_view(argv[1]) == "--combat-preview-purity") {
        test_preview_purity();
        return test::g_failures ? 1 : 0;
    }
    if (argc > 2 && std::string_view(argv[1]) == "--combat-work-probe") {
        expect(std::string_view(argv[2]) == "setup" || std::string_view(argv[2]) == "preview",
               "known combat work-probe mode");
        combat_work_probe(std::string_view(argv[2]) == "preview");
        return test::g_failures ? 1 : 0;
    }
    if (argc > 1 && std::string_view(argv[1]) == "--combat-benchmark") {
        benchmark_combat_preview();
        return test::g_failures ? 1 : 0;
    }
    if (argc > 1 && std::string_view(argv[1]) == "--benchmark") {
        benchmark_interest();
        return test::g_failures ? 1 : 0;
    }
    test_interest();
    test_gameplay_limits();
    test_protocol_point_decay();
    test_indexed_interest();
    test_combatant();
    test_equipment_calculator();
    test_preview_purity();
    test_talent_request_replay();
    test_scheduled_player_phases();
    test_h1_upkeep();
    test_storage_requests();
    test_shop_requests();
    test_routing_boundaries();
    test_h5_doggebi();
    test_h6_archer_persistent();
    test_h7_mage_persistent();
    test_h8_world_step();
    test_h9_world_events();
    test_receive_buffer();
    return test::g_failures ? 1 : 0;
}
