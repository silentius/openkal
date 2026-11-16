/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "TestPlayerAgent.h"

#include "fixture_util.h"
#include "openkal/client/ClientInput.h"
#include "openkal/engine/engine.h"
#include "openkal/map/Teleport.h"
#include "openkal/platform/ConfigArchive.h"
#include "openkal/platform/File.h"
#include "openkal/platform/Path.h"
#include "openkal/protocol/Attack.h"
#include "openkal/protocol/BuffState.h"
#include "openkal/protocol/Chatting.h"
#include "openkal/protocol/CreatePlayer.h"
#include "openkal/protocol/DropItem.h"
#include "openkal/protocol/FrameParser.h"
#include "openkal/protocol/Handshake.h"
#include "openkal/protocol/InsertItem.h"
#include "openkal/protocol/ItemMonster.h"
#include "openkal/protocol/ItemWear.h"
#include "openkal/protocol/MState.h"
#include "openkal/protocol/Move.h"
#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketReader.h"
#include "openkal/protocol/PacketWriter.h"
#include "openkal/protocol/Property.h"
#include "openkal/protocol/Quest.h"
#include "openkal/protocol/Shop.h"
#include "openkal/protocol/Skill.h"
#include "openkal/protocol/Teleport.h"
#include "openkal/protocol/UseItem.h"
#include "openkal/server/Combat.h"
#include "openkal/server/Packets.h"
#include "openkal/server/ServerHost.h"
#include "openkal/server/world/GameWorld.h"
#include "openkal/server/world/QuestEngine.h"
#include "test_util.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <future>
#include <memory>
#include <set>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <unistd.h>

namespace {
    using namespace openkal;
    using test::expect;
    using Position = TestPlayerPosition;
    struct Packet {
        std::uint8_t type = 0;
        std::vector<std::uint8_t> payload;
    };
    Packet unpack(const std::vector<std::uint8_t>& bytes) {
        Packet result;
        std::size_t consumed = 0;
        if (protocol::FrameParser::pop(
                bytes.data(), bytes.size(), consumed, result.type, result.payload) !=
                protocol::FrameParseStatus::Complete ||
            consumed != bytes.size()) {
            throw std::runtime_error("Invalid fixture frame");
        }
        return result;
    }
    void deliver(TestPlayerAgent& agent, const std::vector<std::uint8_t>& bytes) {
        const auto packet = unpack(bytes);
        agent.receive(packet.type, packet.payload);
    }
    void text(const std::string& path, const char* body) {
        std::filesystem::create_directories(std::filesystem::path(path).parent_path());
        std::ofstream(path) << body;
    }
    void insert(TestPlayerAgent& agent,
                std::uint32_t id,
                std::uint16_t index,
                int count = 1,
                bool worn = false,
                bool bound = false) {
        protocol::PacketWriter writer;
        writer.write_u32(id);
        writer.write_u16(index);
        writer.write_u8(0);
        writer.write_u32(protocol::pack_insert_info(true, 0, 0, worn, bound));
        writer.write_u32(static_cast<std::uint32_t>(count));
        deliver(agent, writer.frame(static_cast<std::uint8_t>(protocol::S2C::InsertItem)));
    }
    void monster(TestPlayerAgent& agent,
                 int x = 1030,
                 int y = 1024,
                 std::uint32_t id = 900,
                 int index = 1,
                 int race = 0,
                 std::uint32_t gstate = 0,
                 int hp = 100) {
        protocol::PacketWriter writer;
        writer.write_u16(static_cast<std::uint16_t>(index));
        writer.write_u32(id);
        writer.write_u32(static_cast<std::uint32_t>(x));
        writer.write_u32(static_cast<std::uint32_t>(y));
        writer.write_u16(0);
        writer.write_i32(hp);
        writer.write_i32(100);
        writer.write_u32(gstate);
        writer.write_u32(0);
        writer.write_u64(0);
        writer.write_string("");
        writer.write_u8(static_cast<std::uint8_t>(race));
        writer.write_u32(0);
        writer.write_string("Owned test monster");
        deliver(agent, writer.frame(static_cast<std::uint8_t>(protocol::S2C::CreateMonster)));
    }
    void append_u16(std::vector<std::uint8_t>& output, std::uint16_t value) {
        output.push_back(static_cast<std::uint8_t>(value));
        output.push_back(static_cast<std::uint8_t>(value >> 8u));
    }

    void append_u32(std::vector<std::uint8_t>& output, std::uint32_t value) {
        append_u16(output, static_cast<std::uint16_t>(value));
        append_u16(output, static_cast<std::uint16_t>(value >> 16u));
    }

    struct ZipEntry {
        std::string name;
        std::string contents;
        std::uint32_t offset = 0;
    };

    std::vector<std::uint8_t> make_stored_zip(std::vector<ZipEntry>& entries) {
        std::vector<std::uint8_t> output;
        for (auto& entry : entries) {
            entry.offset = static_cast<std::uint32_t>(output.size());
            append_u32(output, 0x04034b50);
            append_u16(output, 20);
            append_u16(output, 0);
            append_u16(output, 0);
            append_u16(output, 0);
            append_u16(output, 0);
            append_u32(output, 0);
            append_u32(output, static_cast<std::uint32_t>(entry.contents.size()));
            append_u32(output, static_cast<std::uint32_t>(entry.contents.size()));
            append_u16(output, static_cast<std::uint16_t>(entry.name.size()));
            append_u16(output, 0);
            output.insert(output.end(), entry.name.begin(), entry.name.end());
            output.insert(output.end(), entry.contents.begin(), entry.contents.end());
        }

        const std::uint32_t central_offset = static_cast<std::uint32_t>(output.size());
        for (const auto& entry : entries) {
            append_u32(output, 0x02014b50);
            append_u16(output, 20);
            append_u16(output, 20);
            append_u16(output, 0);
            append_u16(output, 0);
            append_u16(output, 0);
            append_u16(output, 0);
            append_u32(output, 0);
            append_u32(output, static_cast<std::uint32_t>(entry.contents.size()));
            append_u32(output, static_cast<std::uint32_t>(entry.contents.size()));
            append_u16(output, static_cast<std::uint16_t>(entry.name.size()));
            append_u16(output, 0);
            append_u16(output, 0);
            append_u16(output, 0);
            append_u16(output, 0);
            append_u32(output, 0);
            append_u32(output, entry.offset);
            output.insert(output.end(), entry.name.begin(), entry.name.end());
        }
        const std::uint32_t central_size =
            static_cast<std::uint32_t>(output.size()) - central_offset;
        append_u32(output, 0x06054b50);
        append_u16(output, 0);
        append_u16(output, 0);
        append_u16(output, static_cast<std::uint16_t>(entries.size()));
        append_u16(output, static_cast<std::uint16_t>(entries.size()));
        append_u32(output, central_size);
        append_u32(output, central_offset);
        append_u16(output, 0);
        return output;
    }

    void navigation_time_budget() {
        for (const std::size_t budget : {1, 128}) {
            bool slow = true, repeated = false;
            int calls = 0;
            std::set<std::array<int, 5>> checked;
            const TestPlayerStep step = [&](Position from, int x, int y) -> std::optional<int> {
                ++calls;
                repeated |= !checked.insert({from.x, from.y, from.z, x, y}).second;
                if (slow) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(3));
                }
                return 0;
            };
            TestPlayerRouteSearch search({16, 16, 0}, {144, 16, 0}, 0, step, budget, 32);
            auto state = search.advance(32, std::chrono::milliseconds(2));
            expect(state == TestPlayerRouteState::Searching && calls == 1 &&
                       search.expanded_nodes() == 1,
                   "a slow segment yields within a node instead of validating all neighbors");
            slow = false;
            for (int slice = 0; slice < 128 && state == TestPlayerRouteState::Searching; ++slice) {
                state = search.advance(1, std::chrono::milliseconds(2));
            }
            expect(!repeated, "resumed nodes retain their completed segment validations");
            if (budget == 1) {
                expect(state == TestPlayerRouteState::Unavailable && calls > 1 &&
                           search.expanded_nodes() == 1,
                       "the final budgeted node finishes across slices before exhaustion");
            } else {
                TestPlayerRouteSearch synchronous(
                    {16, 16, 0},
                    {144, 16, 0},
                    0,
                    [](Position, int, int) -> std::optional<int> { return 0; },
                    budget,
                    32);
                expect(synchronous.advance(128) == TestPlayerRouteState::Found &&
                           state == TestPlayerRouteState::Found &&
                           search.route().size() == synchronous.route().size() &&
                           std::equal(search.route().begin(),
                                      search.route().end(),
                                      synchronous.route().begin(),
                                      [](Position a, Position b) {
                                          return a.x == b.x && a.y == b.y && a.z == b.z;
                                      }),
                       "yielding inside a node preserves the complete synchronous route");
            }
        }
    }

    struct Fixture {
        std::string root = test::temp_dir(("agent_" + std::to_string(getpid())).c_str());
        std::shared_ptr<TestPlayerData> data;
        server::CharacterRecord character;
        Fixture() {
            // Project-owned synthetic gameplay data, covered by the project's GPL-3.0-or-later.
            std::filesystem::remove_all(root);
            for (const auto* dir : {"maps",
                                    "visuals",
                                    "items",
                                    "npcs",
                                    "tasks",
                                    "quests",
                                    "dialogs",
                                    "skills",
                                    "monsters",
                                    "shops"}) {
                std::filesystem::create_directories(root + "/" + dir);
            }
            expect(test::write_open_area(root + "/maps/n_000_000.akarea"),
                   "write owned navigation area");
            expect(test::write_kcm_v7(root + "/visuals/n_000_000.kcm", 0, 0, 0, 1, 1000),
                   "write owned height patch");
            text(
                root + "/items/1.json",
                R"({"index":1,"class":["weapon","sword"],"wear":1,"buy":10,"sell":2,"range":20,"specialty":{"attack":[2, 3],"aspeed":1000}})");
            text(
                root + "/items/2.json",
                R"({"index":2,"class":["general","etc"],"buy":10,"use":1,"stackable":1,"cooltime":[1500, 0],"specialty":{"refresh":{"stat":"hp","amount":50}}})");
            text(
                root + "/items/3.json",
                R"({"index":3,"class":["weapon","sword"],"wear":1,"sell":3,"range":20,"specialty":{"attack":[10, 12],"aspeed":1000}})");
            text(
                root + "/items/4.json",
                R"({"index":4,"class":["weapon","sword"],"wear":1,"limit":{"level":99},"specialty":{"attack":[100, 120]}})");
            text(
                root + "/items/5.json",
                R"({"index":5,"class":["weapon","wand"],"wear":1,"limit":{"class":"mage"},"specialty":{"magic":[100, 120]}})");
            text(
                root + "/items/7.json",
                R"({"index":7,"class":["general","etc"],"use":1,"stackable":1,"cooltime":[2000, 0],"specialty":{"refresh":{"stat":"hp","amount":100,"auto_hp":true}}})");
            text(
                root + "/monsters/1.json",
                R"({"index":1,"name":"Owned test monster","stats":{"race":0,"level":1,"hp":10,"exp":10},"spawns":[{"map":0,"area":1,"max":1,"cycle":60,"rect":[36, 30, 38, 34]}]})");
            text(root + "/npcs/1.json", R"({"index":1,"html":100,"xy":[1024, 1024, 10000]})");
            text(
                root + "/quests/10_0.json",
                R"({"index":10,"flag":0,"cases":[{"if":[{"op":"quest","quest":10,"flag":0},{"op":"notclear","quest":10}],"then":[{"op":"html","id":100}]}]})");
            text(
                root + "/quests/10_1.json",
                R"({"index":10,"flag":1,"cases":[{"if":[{"op":"quest","quest":10,"flag":0},{"op":"notclear","quest":10}],"then":[{"op":"save","quest":10,"flag":1}]}]})");
            text(
                root + "/dialogs/100.json",
                R"({"id":100,"links":[{"type":"quest","text":"Accept owned task","quest":[10, 1]}]})");
            text(root + "/skills/0.json",
                 R"({"class":0,"role":"knight","index":0,"type":["use"],"mp":6,"maxLevel":1})");
            text(
                root + "/skills/3.json",
                R"({"class":0,"role":"knight","index":3,"type":["use"],"mp":5,"maxLevel":1,"delay":[500, 1000],"compares":[{"id":2,"expr":24},{"id":3,"expr":500},{"id":4,"expr":1500}]})");
            text(root + "/items/8.json",
                 R"({"index":8,"class":["general","etc"],"specialty":{"buff":[12, 60, 50]}})");
            text(root + "/shops/1.json",
                 R"({"schema":"openkal.shop/v1","npcIndex":1,"items":[1, 2]})");
            text(
                root + "/loose-config/items/3.json",
                R"({"index":3,"class":["weapon","sword"],"wear":1,"specialty":{"attack":[0, 0]}})");
            data = load_data();
            character.map_x = character.map_y = 1024;
            character.map_z = 10000;
            character.hp = character.max_hp = 100;
            character.mp = character.max_mp = 50;
            character.level = 5;
        }
        std::shared_ptr<TestPlayerData> load_data(bool includeAreas = true) {
            platform::ConfigArchive::unmount();
            std::vector<ZipEntry> entries;
            for (const auto* directory :
                 {"items", "npcs", "tasks", "quests", "dialogs", "skills", "monsters", "shops"}) {
                for (const auto& file :
                     std::filesystem::recursive_directory_iterator(root + "/" + directory)) {
                    if (!file.is_regular_file()) {
                        continue;
                    }
                    const auto bytes = platform::File::read(file.path().string());
                    expect(bytes.has_value(), "read owned catalog fixture");
                    if (bytes) {
                        entries.push_back(
                            {std::string(std::string_view{directory} == "shops" ? "config/server/"
                                                                                : "config/") +
                                 std::filesystem::relative(file.path(), root).generic_string(),
                             std::string(bytes->begin(), bytes->end())});
                    }
                }
            }
            if (std::filesystem::is_regular_file(root + "/portals.json")) {
                const auto bytes = platform::File::read(root + "/portals.json");
                entries.push_back(
                    {"config/portals.json", std::string(bytes->begin(), bytes->end())});
            }
            const auto assetBase = platform::executable_directory();
            const auto relativeAssets = std::filesystem::relative(root, assetBase).generic_string();
            entries.push_back({"config/profiles/default.profile",
                               "data_root=" + relativeAssets +
                                   "\nmaps_subdir=visuals\nserver_map_root=" + relativeAssets +
                                   "/missing-maps\nconfiguration_root=" + root +
                                   "/loose-config\n"});
            if (includeAreas) {
                const auto bytes = platform::File::read(root + "/maps/n_000_000.akarea");
                entries.push_back({"RuntimeData/areas/n_000_000.akarea",
                                   std::string(bytes->begin(), bytes->end())});
            }
            std::filesystem::create_directories(root + "/package");
            expect(test::write_bytes(root + "/package/config.pak", make_stored_zip(entries)),
                   "write owned packaged gameplay fixture");
            return TestPlayerAgent::load_data(root + "/package");
        }
        ~Fixture() {
            platform::ConfigArchive::unmount();
            std::filesystem::remove_all(root);
        }
        std::unique_ptr<TestPlayerAgent> player(bool completed = true, bool armed = true) {
            auto result = std::make_unique<TestPlayerAgent>(character);
            const std::uint16_t wear[7]{};
            deliver(*result,
                    protocol::CreatePlayer::frame(
                        77, "Owned", 0x80, 1024, 1024, 10000, 0, 0, wear, 0, 0));
            protocol::CreateNpcInfo npc;
            npc.id = 800;
            npc.index = 1;
            npc.x = npc.y = 1024;
            npc.z = 10000;
            deliver(*result, protocol::Quest::frame_create_npc(npc));
            if (completed) {
                deliver(*result, protocol::Quest::frame_save_quest({10, 1, 1}));
            }
            if (armed) {
                insert(*result, 9, 1, 1, true);
            }
            result->start(data);
            return result;
        }
    };
    void parallel_players(Fixture& fixture) {
        constexpr std::size_t count = 8;
        std::promise<void> start;
        const auto ready = start.get_future().share();
        std::vector<std::future<bool>> results;
        for (std::size_t i = 0; i < count; ++i) {
            auto player = fixture.player();
            monster(*player, 1120);
            results.push_back(
                std::async(std::launch::async, [player = std::move(player), ready]() mutable {
                    ready.wait();
                    bool moved = false;
                    for (std::uint64_t now = 10000; now < 12000 && !moved; now += 50) {
                        player->tick(now, [&](const auto& bytes) {
                            const auto packet = unpack(bytes);
                            moved =
                                moved ||
                                packet.type == static_cast<std::uint8_t>(protocol::C2S::MoveOn) ||
                                packet.type == static_cast<std::uint8_t>(protocol::C2S::MoveEnd);
                            return true;
                        });
                    }
                    player->pause();
                    return moved && !player->active();
                }));
        }
        start.set_value();
        for (auto& result : results) {
            expect(result.get(),
                   "parallel agents sharing catalogs independently load navigation and move");
        }
    }

    void primary_snapshot(TestPlayerAgent& player,
                          const std::array<std::int32_t, 5>& values,
                          int points) {
        deliver(player,
                protocol::Property::frame_update_points(
                    static_cast<std::uint8_t>(protocol::PropertyType::PuPoint), points));
        for (std::size_t i = 0; i < values.size(); ++i) {
            deliver(player,
                    protocol::Property::frame_update(static_cast<std::uint8_t>(i), values[i]));
        }
    }
    std::array<std::int32_t, 5> base_primaries(int clazz) {
        const auto b = server::combat::class_base_plus_bonus(clazz);
        return {b.str, b.hth, b.intl, b.wis, b.dex};
    }
    void stat_allocation(Fixture& fixture) {
        constexpr std::array<std::array<int, 5>, 4> expected{{
            {{10, 8, 0, 0, 2}},
            {{0, 5, 5, 0, 10}},
            {{5, 5, 0, 0, 10}},
            {{5, 10, 0, 0, 5}},
        }};
        for (int clazz = 0; clazz < 4; ++clazz) {
            auto character = fixture.character;
            character.clazz = clazz;
            TestPlayerAgent player(character);
            const std::uint16_t wear[7]{};
            deliver(player,
                    protocol::CreatePlayer::frame(77,
                                                  "Owned",
                                                  static_cast<std::uint8_t>(0x80 | clazz),
                                                  1024,
                                                  1024,
                                                  10000,
                                                  0,
                                                  0,
                                                  wear,
                                                  0,
                                                  0));
            deliver(player, protocol::Quest::frame_save_quest({10, 1, 1}));
            player.start(fixture.data);
            const auto base = base_primaries(clazz);
            auto values = base;
            int points = 20, requests = 0;
            primary_snapshot(player, values, points);
            for (std::uint64_t now = 10000; now < 20000 && points > 0; now += 100) {
                std::vector<Packet> sent;
                player.tick(now, [&](const auto& bytes) {
                    sent.push_back(unpack(bytes));
                    return true;
                });
                for (const auto& packet : sent) {
                    if (packet.type != static_cast<std::uint8_t>(protocol::C2S::UpdateProperty)) {
                        continue;
                    }
                    std::uint8_t property = 255, count = 0;
                    expect(protocol::Property::parse_c2s_update(
                               packet.payload.data(), packet.payload.size(), property, count) &&
                               property < 5 && count == 1,
                           "autonomous stats use ordinary single-increment requests");
                    if (property >= 5) {
                        continue;
                    }
                    ++requests;
                    expect(server::combat::spend_primary_points(
                               clazz, property, count, values[property], points) == 1,
                           "the authoritative cost curve accepts the selected allocation");
                    primary_snapshot(player, values, points);
                    if (requests == 7) {
                        player.pause();
                        player.start(fixture.data);
                    }
                }
            }
            std::array<int, 5> increases{};
            for (std::size_t i = 0; i < increases.size(); ++i) {
                increases[i] = values[i] - base[i];
            }
            expect(points == 0 && increases == expected[clazz],
                   "each class follows the requested profile across autonomy restarts");
            points = 100;
            primary_snapshot(player, values, points);
            for (std::uint64_t now = 20000; now < 40000 && points > 0; now += 100) {
                std::vector<Packet> sent;
                player.tick(now, [&](const auto& bytes) {
                    sent.push_back(unpack(bytes));
                    return true;
                });
                for (const auto& packet : sent) {
                    if (packet.type != static_cast<std::uint8_t>(protocol::C2S::UpdateProperty)) {
                        continue;
                    }
                    std::uint8_t property = 255, count = 0;
                    if (protocol::Property::parse_c2s_update(
                            packet.payload.data(), packet.payload.size(), property, count) &&
                        property < 5) {
                        expect(server::combat::spend_primary_points(
                                   clazz, property, count, values[property], points) == 1,
                               "higher-cost allocations remain affordable");
                        primary_snapshot(player, values, points);
                    }
                }
            }
            int total = 0;
            std::array<int, 5> spent{};
            for (int i = 0; i < 5; ++i) {
                spent[i] =
                    server::combat::pending_need_points(clazz, i, base[i], values[i] - base[i]);
                total += spent[i];
            }
            bool balanced = total >= 118 && total <= 120;
            for (int i = 0; i < 5; ++i) {
                balanced &= std::abs(spent[i] * 20 - total * expected[clazz][i]) <= 60;
            }
            expect(balanced,
                   "level-up PU is distributed by spent points including increasing costs");
        }
        auto waiting = fixture.player();
        const auto base = base_primaries(0);
        int requests = 0;
        std::uint8_t property = 255;
        const auto send = [&](const auto& bytes) {
            const auto packet = unpack(bytes);
            if (packet.type == static_cast<std::uint8_t>(protocol::C2S::UpdateProperty)) {
                std::uint8_t count = 0;
                expect(protocol::Property::parse_c2s_update(
                           packet.payload.data(), packet.payload.size(), property, count),
                       "decode pending allocation");
                ++requests;
            }
            return true;
        };
        deliver(*waiting,
                protocol::Property::frame_update_points(
                    static_cast<std::uint8_t>(protocol::PropertyType::PuPoint), 20));
        waiting->tick(10000, send);
        expect(requests == 0, "PU alone cannot trigger spending before the primary snapshot");
        primary_snapshot(*waiting, base, 20);
        waiting->tick(10100, send);
        const auto first = property;
        waiting->tick(10500, send);
        deliver(*waiting,
                protocol::Property::frame_update_points(
                    static_cast<std::uint8_t>(protocol::PropertyType::PuPoint), 19));
        waiting->tick(10600, send);
        expect(requests == 1, "spending waits for both PU and the primary confirmation");
        deliver(*waiting, protocol::Property::frame_update(first, base[first] + 1));
        waiting->tick(10700, send);
        expect(requests == 2, "a complete confirmation releases the next intended allocation");
        waiting->tick(16000, send);
        expect(!waiting->active() && requests == 2 &&
                   waiting->status().find("stat-point allocation was not confirmed") !=
                       std::string::npos,
               "missing allocation replies pause without duplicate point spending");
    }
    void beheading(Fixture& fixture) {
        for (int clazz = 0; clazz < 4; ++clazz) {
            text(fixture.root + "/skills/behead" + std::to_string(clazz) + ".json",
                 ("{\"class\":" + std::to_string(clazz) +
                  ",\"index\":1,\"type\":[\"use\"],\"mp\":0,\"maxLevel\":1,\"delay\":[200, 500]}")
                     .c_str());
        }
        text(
            fixture.root + "/items/60000.json",
            R"({"index":60000,"class":["weapon","bow"],"wear":1,"range":300,"specialty":{"attack":[2, 3],"aspeed":1000}})");
        fixture.data = fixture.load_data();
        const auto player = [&](int clazz = 0, bool ranged = false) {
            auto result = fixture.player();
            const std::uint16_t wear[7]{};
            deliver(*result,
                    protocol::CreatePlayer::frame(77,
                                                  "Owned",
                                                  static_cast<std::uint8_t>(0x80 | clazz),
                                                  1024,
                                                  1024,
                                                  10000,
                                                  0,
                                                  0,
                                                  wear,
                                                  0,
                                                  0));
            insert(*result, 9, ranged ? 60000 : 1, 1, true);
            deliver(*result, protocol::Skill::frame_skill_info({{1, 1}}));
            return result;
        };
        for (int clazz = 0; clazz < 4; ++clazz) {
            auto subject = player(clazz);
            monster(*subject, 1030, 1024, 900, 1, 0, protocol::kCgsKnee, 0);
            int pre = 0, casts = 0;
            const auto send = [&](const auto& bytes) {
                const auto packet = unpack(bytes);
                pre += packet.type == static_cast<std::uint8_t>(protocol::C2S::PreSkill);
                if (packet.type == static_cast<std::uint8_t>(protocol::C2S::Skill)) {
                    std::uint8_t skill = 0, kind = 0;
                    std::uint32_t target = 0;
                    const std::uint8_t* tail = nullptr;
                    std::size_t tailSize = 0;
                    expect(
                        protocol::Skill::parse_client_header(
                            packet.payload.data(), packet.payload.size(), skill, tail, tailSize) &&
                            protocol::Skill::parse_client_target(tail, tailSize, kind, target) &&
                            skill == 1 && kind == protocol::kCkMonster && target == 900,
                        "beheading addresses the kneeling monster through the normal skill packet");
                    ++casts;
                }
                return true;
            };
            subject->tick(10000, send);
            subject->tick(10100, send);
            expect(pre == 1 && casts == 0, "beheading honors the catalog wind-up");
            subject->tick(10200, send);
            for (std::uint64_t now = 11000; now <= 20000; now += 1000) {
                subject->tick(now, send);
            }
            expect(casts == 1 && pre == 1, "each class beheads once even without a removal reply");
        }
        for (int clazz = 0; clazz < 4; ++clazz) {
            auto walking = player(clazz, true);
            monster(*walking, 1200);
            deliver(*walking, protocol::Skill::frame_action_knee(900));
            bool moved = false, cast = false;
            for (std::uint64_t now = 10000; now <= 17000 && !cast; now += 50) {
                std::vector<Packet> sent;
                walking->tick(now, [&](const auto& bytes) {
                    sent.push_back(unpack(bytes));
                    return true;
                });
                for (const auto& packet : sent) {
                    if (packet.type == static_cast<std::uint8_t>(protocol::C2S::MoveOn) ||
                        packet.type == static_cast<std::uint8_t>(protocol::C2S::MoveEnd)) {
                        std::int8_t dx = 0, dy = 0, dz = 0;
                        expect(protocol::Move::parse_client(
                                   packet.payload.data(), packet.payload.size(), dx, dy, dz),
                               "decode beheading approach");
                        moved |= dx != 0 || dy != 0;
                        deliver(*walking,
                                packet.type == static_cast<std::uint8_t>(protocol::C2S::MoveOn)
                                    ? protocol::Move::frame_player_on(77, dx, dy, dz)
                                    : protocol::Move::frame_player_end(77, dx, dy, dz));
                    }
                    cast |= packet.type == static_cast<std::uint8_t>(protocol::C2S::Skill);
                }
            }
            expect(moved && cast &&
                       std::abs(walking->position().x - 1200) <= protocol::kItemMonsterOpenRange,
                   "every class beheads within wooden-box opening range even with a ranged weapon");
        }
        std::filesystem::remove(fixture.root + "/items/60000.json");
        for (int mode = 0; mode < 4; ++mode) {
            auto subject = player();
            monster(*subject);
            deliver(*subject, protocol::Skill::frame_action_knee(900));
            if (mode == 0) {
                deliver(*subject, protocol::Skill::frame_skill_info({}));
            }
            if (mode == 1) {
                deliver(*subject, protocol::Skill::frame_action_die(900));
            }
            if (mode == 2) {
                deliver(*subject, protocol::Skill::frame_action_beheaded(900));
            }
            if (mode == 3) {
                deliver(*subject,
                        protocol::Property::frame_update_vital(
                            static_cast<std::uint8_t>(protocol::PropertyType::Hp), 20, 100));
            }
            bool attempted = false;
            for (std::uint64_t now = 10000; now <= 11000; now += 100) {
                subject->tick(now, [&](const auto& bytes) {
                    const auto type = unpack(bytes).type;
                    attempted |= type == static_cast<std::uint8_t>(protocol::C2S::PreSkill) ||
                                 type == static_cast<std::uint8_t>(protocol::C2S::Skill);
                    return true;
                });
            }
            expect(!attempted,
                   "missing skills, ordinary corpses and emergency recovery do not behead");
        }
        auto interrupted = player();
        monster(*interrupted);
        deliver(*interrupted, protocol::Skill::frame_action_knee(900));
        int casts = 0, attacks = 0;
        const auto send = [&](const auto& bytes) {
            const auto type = unpack(bytes).type;
            casts += type == static_cast<std::uint8_t>(protocol::C2S::Skill);
            attacks += type == static_cast<std::uint8_t>(protocol::C2S::Attack);
            return true;
        };
        interrupted->tick(10000, send);
        monster(*interrupted, 1031, 1024, 901);
        deliver(*interrupted, protocol::Attack::frame_server(901, 77, 10));
        interrupted->tick(10200, send);
        expect(casts == 0 && attacks == 1, "a new living attacker interrupts beheading wind-up");
        deliver(*interrupted, protocol::Attack::frame_server(77, 901, 100));
        interrupted->tick(11300, send);
        deliver(*interrupted, server::frame_remove_monster(900));
        interrupted->tick(11600, send);
        expect(casts == 0, "a disappearing corpse cancels the pending beheading request");
        for (int clazz = 0; clazz < 4; ++clazz) {
            std::filesystem::remove(fixture.root + "/skills/behead" + std::to_string(clazz) +
                                    ".json");
        }
        fixture.data = fixture.load_data();
    }
    void distributed_hunting(Fixture& fixture) {
        constexpr int population = 64, targets = 10;
        std::map<std::uint32_t, int> selections;
        bool valid = true, stable = true, retargeted = true;
        for (int i = 0; i < population; ++i) {
            auto character = fixture.character;
            character.id = i + 1;
            TestPlayerAgent player(character);
            const std::uint16_t wear[7]{};
            deliver(player,
                    protocol::CreatePlayer::frame(
                        77, "Owned", 0x80, 1024, 1024, 10000, 0, 0, wear, 0, 0));
            deliver(player, protocol::Quest::frame_save_quest({10, 1, 1}));
            insert(player, 9, 1, 1, true);
            player.start(fixture.data);
            for (int target = 0; target < targets; ++target) {
                monster(player, 1025 + target, 1024, 900 + target);
            }
            monster(player, 1024, 1024, 950, 1, 0, 0, 0);
            const auto attack = [&](std::uint64_t now) {
                std::uint32_t chosen = 0;
                player.tick(now, [&](const auto& bytes) {
                    const auto packet = unpack(bytes);
                    if (packet.type == static_cast<std::uint8_t>(protocol::C2S::Attack)) {
                        std::uint8_t kind = 0;
                        valid &= protocol::Attack::parse_client(
                                     packet.payload.data(), packet.payload.size(), kind, chosen) &&
                                 kind == protocol::kCkMonster;
                    }
                    return true;
                });
                return chosen;
            };
            const auto chosen = attack(10000);
            valid &= chosen >= 900 && chosen < 900 + targets;
            ++selections[chosen];
            monster(player, 1024, 1024, 960);
            stable &= attack(11100) == chosen;
            deliver(player, protocol::Attack::frame_server(77, chosen, 100));
            const auto next = attack(12200);
            retargeted &= next != chosen && ((next >= 900 && next < 900 + targets) || next == 960);
            deliver(player, server::frame_remove_monster(next));
            const auto replacement = attack(13300);
            retargeted &=
                replacement != next && replacement != chosen &&
                ((replacement >= 900 && replacement < 900 + targets) || replacement == 960);
        }
        expect(valid && selections.size() >= 6 && selections[900] < population / 2,
               "64 players at the same position independently spread across matching entities "
               "instead of selecting the nearest monster together");
        expect(stable, "a newly visible nearer monster does not reroll an ongoing fight");
        expect(retargeted, "dead and removed targets trigger a fresh eligible entity selection");
    }
    void level_appropriate_hunting(Fixture& fixture) {
        text(
            fixture.root + "/monsters/2.json",
            R"({"index":2,"stats":{"race":0,"level":5,"hp":100},"spawns":[{"map":0,"area":2,"max":1,"cycle":60,"rect":[55, 32, 60, 36]}]})");
        text(
            fixture.root + "/monsters/3.json",
            R"({"index":3,"stats":{"race":0,"level":10,"hp":100},"spawns":[{"map":0,"area":3,"max":1,"cycle":60,"rect":[60, 32, 65, 36]}]})");
        text(fixture.root + "/monsters/4.json",
             R"({"index":4,"stats":{"race":0,"level":20,"hp":100}})");
        fixture.data = fixture.load_data();
        const auto attack = [&](TestPlayerAgent& player, std::uint64_t now) {
            std::uint32_t target = 0;
            player.tick(now, [&](const auto& bytes) {
                const auto packet = unpack(bytes);
                if (packet.type == static_cast<std::uint8_t>(protocol::C2S::Attack)) {
                    std::uint8_t kind = 0;
                    expect(protocol::Attack::parse_client(
                               packet.payload.data(), packet.payload.size(), kind, target),
                           "decode level-aware attack");
                }
                return true;
            });
            return target;
        };
        auto subject = fixture.player();
        monster(*subject);
        monster(*subject, 1038, 1024, 901, 2);
        monster(*subject, 1025, 1024, 902, 4);
        expect(
            attack(*subject, 10000) == 901,
            "an equal-level monster beats nearer blue mobs without attacking an overlevel target");
        auto searching = fixture.player();
        monster(*searching);
        bool attackedBlue = false, moved = false;
        for (std::uint64_t now = 10000; now <= 11000 && !moved; now += 50) {
            searching->tick(now, [&](const auto& bytes) {
                const auto type = unpack(bytes).type;
                attackedBlue |= type == static_cast<std::uint8_t>(protocol::C2S::Attack);
                moved |= type == static_cast<std::uint8_t>(protocol::C2S::MoveOn);
                return true;
            });
        }
        expect(moved && !attackedBlue,
               "blue mobs yield to travel toward a known spawn in the player's level range");
        auto leveled = fixture.player();
        monster(*leveled, 1030, 1024, 901, 2);
        monster(*leveled, 1038, 1024, 902, 3);
        deliver(*leveled, protocol::Property::frame_update_level(10));
        expect(attack(*leveled, 10000) == 902,
               "a level-up immediately changes the next hunting target to the new level range");
        auto defending = fixture.player();
        monster(*defending);
        monster(*defending, 1038, 1024, 901, 2);
        deliver(*defending, protocol::Attack::frame_server(900, 77, 10));
        expect(attack(*defending, 10000) == 900,
               "an actual low-level attacker retains defense priority over level-based hunting");
        for (const auto* file : {"monsters/2.json", "monsters/3.json", "monsters/4.json"}) {
            std::filesystem::remove(fixture.root + "/" + file);
        }
        fixture.data = fixture.load_data();
    }
    void selling_in_narootuh(Fixture& fixture) {
        const auto& city = map::kTeleportLocs[0];
        text(fixture.root + "/npcs/2.json",
             ("{\"index\":2,\"map\":0,\"xy\":[" + std::to_string(city.x + 16) + ", " +
              std::to_string(city.y) + ", " + std::to_string(city.z) + "]}")
                 .c_str());
        text(fixture.root + "/shops/2.json",
             R"({"schema":"openkal.shop/v1","npcIndex":2,"items":[1, 2]})");
        fixture.data = fixture.load_data();
        auto player = fixture.player();
        for (std::uint32_t id = 100; id < 160; ++id) {
            insert(*player, id, 1);
        }
        int teleports = 0;
        bool listed = false, sold = false;
        std::vector<Packet> sent;
        const auto send = [&](const auto& bytes) {
            sent.push_back(unpack(bytes));
            return true;
        };
        const auto travel = [&](std::size_t index) {
            expect(index == 0, "inventory pressure requests Narootuh explicitly");
            ++teleports;
            return true;
        };
        player->tick(10000, send, std::chrono::milliseconds{2}, travel);
        expect(
            teleports == 1 && player->pending_city_travel() == 0 && player->position().x == 1024,
            "selling teleports before searching for a local merchant without optimistic movement");
        player->tick(11000, send, std::chrono::milliseconds{2}, travel);
        expect(teleports == 1 && sent.empty(), "selling waits for authoritative city arrival");
        deliver(*player, protocol::Teleport::frame_server(0, city.x, city.y, city.z, 0));
        deliver(*player, protocol::Move::frame_before(77, city.x, city.y, city.z, 0));
        protocol::CreateNpcInfo npc;
        npc.id = 801;
        npc.index = 2;
        npc.x = city.x + 16;
        npc.y = city.y;
        npc.z = city.z;
        deliver(*player, protocol::Quest::frame_create_npc(npc));
        player->tick(12000, send, std::chrono::milliseconds{2}, travel);
        for (const auto& packet : sent) {
            listed |= packet.type == static_cast<std::uint8_t>(protocol::C2S::ItemList);
        }
        expect(listed && teleports == 1, "confirmed Narootuh arrival opens the ordinary merchant");
        protocol::ShopItemList list;
        list.npc_id = npc.id;
        list.offers = {{1, 10}, {2, 10}};
        deliver(*player, protocol::Shop::frame_item_list_s2c(list));
        sent.clear();
        player->tick(13100, send, std::chrono::milliseconds{2}, travel);
        for (const auto& packet : sent) {
            if (packet.type == static_cast<std::uint8_t>(protocol::C2S::SellItem)) {
                std::uint32_t entity = 0, iid = 0;
                std::uint16_t quantity = 0;
                sold |= protocol::Shop::parse_sell_c2s(
                            packet.payload.data(), packet.payload.size(), entity, iid, quantity) &&
                        entity == npc.id && iid >= 100 && iid < 160 && quantity == 1;
            }
        }
        expect(sold && teleports == 1,
               "ordinary sales use the Narootuh NPC without repeated teleports");
        auto silent = fixture.player();
        for (std::uint32_t id = 100; id < 160; ++id) {
            insert(*silent, id, 1);
        }
        const auto accept = [](std::size_t) { return true; };
        silent->tick(20000, send, std::chrono::milliseconds{2}, accept);
        deliver(*silent, protocol::Teleport::frame_server(0, city.x, city.y, city.z, 0));
        deliver(*silent, protocol::Move::frame_before(77, city.x, city.y, city.z, 0));
        deliver(*silent, protocol::Quest::frame_create_npc(npc));
        sent.clear();
        silent->tick(21000, send, std::chrono::milliseconds{2}, accept);
        silent->tick(22000, send, std::chrono::milliseconds{2}, accept);
        silent->tick(27000, send, std::chrono::milliseconds{2}, accept);
        expect(!silent->active() &&
                   silent->status().find("merchant 2 did not reply") != std::string::npos &&
                   std::count_if(sent.begin(),
                                 sent.end(),
                                 [](const auto& packet) {
                                     return packet.type ==
                                            static_cast<std::uint8_t>(protocol::C2S::ItemList);
                                 }) == 1,
               "an unanswered merchant request pauses instead of repeatedly opening the shop");
        std::filesystem::remove(fixture.root + "/npcs/2.json");
        std::filesystem::remove(fixture.root + "/shops/2.json");
        fixture.data = fixture.load_data();
    }
    void loot_priorities(Fixture& fixture) {
        server::GroundItem drop;
        drop.id = 600;
        drop.item_index = 2;
        drop.x = 1028;
        drop.y = 1024;
        std::vector<Packet> sent;
        const auto tick = [&](TestPlayerAgent& player, std::uint64_t now) {
            sent.clear();
            player.tick(now, [&](const auto& bytes) {
                sent.push_back(unpack(bytes));
                return true;
            });
        };
        const auto has = [&](protocol::C2S op) {
            return std::any_of(sent.begin(), sent.end(), [&](const auto& packet) {
                return packet.type == static_cast<std::uint8_t>(op);
            });
        };
        auto questing = fixture.player(false);
        deliver(*questing, server::frame_create_item(drop));
        tick(*questing, 10000);
        expect(has(protocol::C2S::PickupItem) && !has(protocol::C2S::AskNpc),
               "nearby loot is collected before travelling to an available quest NPC");
        if (has(protocol::C2S::PickupItem)) {
            const auto& payload = sent.back().payload;
            protocol::PacketReader reader(payload.data(), payload.size());
            std::uint32_t id = 0, tileX = 0, tileY = 0;
            expect(payload.size() == 12 && reader.read_u32(id) && reader.read_u32(tileX) &&
                       reader.read_u32(tileY) && id == drop.id && tileX == 32 && tileY == 32,
                   "pickup uses the server ground-item id and ordinary tile coordinates");
        }
        deliver(*questing, server::frame_remove_item(drop.id));
        tick(*questing, 11100);
        expect(has(protocol::C2S::AskNpc),
               "quest interaction resumes after the server removes the collected drop");

        auto engaged = fixture.player();
        monster(*engaged);
        tick(*engaged, 10000);
        deliver(*engaged, server::frame_create_item(drop));
        tick(*engaged, 11100);
        expect(has(protocol::C2S::Attack) && !has(protocol::C2S::PickupItem),
               "an ongoing fight finishes before looting even when the player takes no damage");

        auto fighting = fixture.player();
        monster(*fighting);
        monster(*fighting, 1031, 1024, 901);
        tick(*fighting, 10000);
        deliver(*fighting, protocol::Attack::frame_server(900, 77, 10));
        deliver(*fighting, protocol::Attack::frame_server(901, 77, 10));
        deliver(*fighting, server::frame_create_item(drop));
        tick(*fighting, 11100);
        expect(has(protocol::C2S::Attack) && !has(protocol::C2S::PickupItem),
               "living attackers take priority over nearby loot");
        deliver(*fighting, protocol::Attack::frame_server(77, 900, 100));
        tick(*fighting, 12200);
        expect(has(protocol::C2S::Attack) && !has(protocol::C2S::PickupItem),
               "killing one attacker does not hide another attacker");
        deliver(*fighting, protocol::Attack::frame_server(901, 77, 10));
        deliver(*fighting, protocol::Attack::frame_server(77, 901, 100));
        tick(*fighting, 13300);
        expect(has(protocol::C2S::PickupItem),
               "loot is collected immediately after the last attacker dies despite recent damage");

        auto arriving = fixture.player();
        monster(*arriving);
        deliver(*arriving, protocol::Attack::frame_server(900, 77, 10));
        deliver(*arriving, server::frame_create_item(drop));
        tick(*arriving, 10000);
        expect(has(protocol::C2S::Attack) && !has(protocol::C2S::PickupItem),
               "damage received before the first controller tick still protects combat priority");

        auto healing = fixture.player();
        insert(*healing, 42, 2);
        deliver(*healing, server::frame_create_item(drop));
        deliver(*healing,
                protocol::Property::frame_update_vital(
                    static_cast<std::uint8_t>(protocol::PropertyType::Hp), 20, 100));
        tick(*healing, 10000);
        expect(has(protocol::C2S::UseItem) && !has(protocol::C2S::PickupItem),
               "emergency healing retains priority over loot");
    }
    void movement_timing(Fixture& fixture) {
        struct Travel {
            double distance = 0;
            double largestStep = 0;
            std::uint64_t shortestInterval = 100000;
            int packets = 0;
            bool packetsValid = true;
        };
        const auto travel = [&](int goalX,
                                int goalY,
                                int modifier,
                                std::uint64_t buff = 0,
                                int seconds = 60,
                                int item = 0,
                                bool freeze = false) {
            auto player = fixture.player();
            monster(*player, goalX, goalY);
            deliver(*player,
                    protocol::Property::frame_update_mproperty(
                        77, protocol::kPropertyMSpeed, static_cast<std::int16_t>(modifier)));
            if (buff) {
                deliver(*player, protocol::BuffState::frame_add(buff, seconds, 0, item));
            }
            Travel result;
            std::uint64_t previous = 0;
            for (std::uint64_t time = 10000; time <= 13000; time += 10) {
                if (freeze && time == 11000) {
                    deliver(*player, protocol::MState::frame(77, protocol::kCmsMoveStop));
                }
                std::vector<Packet> replies;
                player->tick(time, [&](const auto& bytes) {
                    replies.push_back(unpack(bytes));
                    return true;
                });
                for (const auto& packet : replies) {
                    if (packet.type != static_cast<std::uint8_t>(protocol::C2S::MoveOn) &&
                        packet.type != static_cast<std::uint8_t>(protocol::C2S::MoveEnd)) {
                        continue;
                    }
                    std::int8_t dx = 0, dy = 0, dz = 0;
                    result.packetsValid &= protocol::Move::parse_client(
                        packet.payload.data(), packet.payload.size(), dx, dy, dz);
                    const double length = std::hypot(dx, dy);
                    if (length > 0) {
                        result.distance += length;
                        result.largestStep = std::max(result.largestStep, length);
                        if (previous) {
                            result.shortestInterval =
                                std::min(result.shortestInterval, time - previous);
                        }
                        previous = time;
                        ++result.packets;
                    }
                    deliver(*player,
                            packet.type == static_cast<std::uint8_t>(protocol::C2S::MoveOn)
                                ? protocol::Move::frame_player_on(77, dx, dy, dz)
                                : protocol::Move::frame_player_end(77, dx, dy, dz));
                }
            }
            return result;
        };
        const auto cardinal = travel(1800, 1024, 0);
        const auto diagonal = travel(1800, 1800, 0);
        expect(cardinal.distance >= 114 && cardinal.distance <= 122 && diagonal.distance >= 114 &&
                   diagonal.distance <= 122 && std::abs(cardinal.distance - diagonal.distance) < 4,
               "cardinal and diagonal movement both keep the normal 40 units per second");
        expect(cardinal.packetsValid && diagonal.packetsValid && cardinal.shortestInterval >= 50 &&
                   diagonal.shortestInterval >= 50 && cardinal.largestStep <= 3.5 &&
                   diagonal.largestStep <= 3.5,
               "movement is split into small confirmed steps at no more than 20 Hz");
        const auto slowed = travel(1800, 1024, -50);
        const auto faster = travel(1800, 1024, 50);
        expect(slowed.distance >= 74 && slowed.distance <= 82 && faster.distance >= 174 &&
                   faster.distance <= 184,
               ("ordinary positive and negative movement properties use the shared speed factor: " +
                std::to_string(slowed.distance) + ", " + std::to_string(faster.distance))
                   .c_str());
        const auto haste = travel(1800, 1024, 0, protocol::kCbsSkillHaste, 60, 8);
        const auto expired = travel(1800, 1024, 0, protocol::kCbsSkillHaste, 1, 8);
        expect(haste.distance >= 174 && haste.distance <= 184 && expired.distance >= 134 &&
                   expired.distance <= 144,
               ("item haste received before the first tick applies and expires normally: " +
                std::to_string(haste.distance) + ", " + std::to_string(expired.distance))
                   .c_str());
        const auto frozen = travel(1800, 1024, 0, 0, 60, 0, true);
        expect(frozen.distance >= 34 && frozen.distance <= 42,
               "server movement-stop state stops walking without banking a later burst");
    }
    void boost(Fixture& fixture) {
        for (int clazz = 1; clazz < 4; ++clazz) {
            text(fixture.root + "/skills/run" + std::to_string(clazz) + ".json",
                 ("{\"class\":" + std::to_string(clazz) +
                  ",\"index\":0,\"type\":[\"use\"],\"mp\":6,\"maxLevel\":1}")
                     .c_str());
        }
        fixture.data = fixture.load_data();
        const auto travel = [&](bool confirm,
                                int mp,
                                bool itemHaste = false,
                                int clazz = 0,
                                int goalX = 1800,
                                bool learned = true) {
            auto player = fixture.player();
            if (clazz != 0) {
                const std::uint16_t wear[7]{};
                deliver(*player,
                        protocol::CreatePlayer::frame(77,
                                                      "Owned",
                                                      static_cast<std::uint8_t>(0x80 | clazz),
                                                      1024,
                                                      1024,
                                                      10000,
                                                      0,
                                                      0,
                                                      wear,
                                                      0,
                                                      0));
                insert(*player, 9, 1, 1, true);
            }
            monster(*player, goalX);
            if (learned) {
                deliver(*player, protocol::Skill::frame_skill_info({{0, 1}}));
            } else {
                deliver(*player, protocol::Skill::frame_skill_info({}));
            }
            deliver(*player,
                    protocol::Property::frame_update_vital(
                        static_cast<std::uint8_t>(protocol::PropertyType::Mp), mp, 100));
            if (itemHaste) {
                deliver(*player,
                        protocol::BuffState::frame_add(protocol::kCbsSkillHaste, 60, 0, 8));
            }
            int requests = 0;
            double travelled = 0;
            for (std::uint64_t now = 10000; now <= 13000; now += 10) {
                std::vector<Packet> packets;
                player->tick(now, [&](const auto& bytes) {
                    packets.push_back(unpack(bytes));
                    return true;
                });
                for (const auto& packet : packets) {
                    if (packet.type == static_cast<std::uint8_t>(protocol::C2S::Skill)) {
                        expect(packet.payload == std::vector<std::uint8_t>({0, 1}),
                               "haste uses the ordinary skill-zero enable request");
                        ++requests;
                        if (confirm) {
                            deliver(*player, protocol::Skill::frame_server(0, 77, 77, 1));
                        }
                    }
                    if (packet.type == static_cast<std::uint8_t>(protocol::C2S::MoveOn) ||
                        packet.type == static_cast<std::uint8_t>(protocol::C2S::MoveEnd)) {
                        std::int8_t dx = 0, dy = 0, dz = 0;
                        expect(protocol::Move::parse_client(
                                   packet.payload.data(), packet.payload.size(), dx, dy, dz),
                               "boosted movement remains ordinary movement");
                        travelled += std::hypot(dx, dy);
                        deliver(*player, protocol::Move::frame_player_on(77, dx, dy, dz));
                    }
                }
            }
            return std::make_pair(requests, travelled);
        };
        const auto confirmed = travel(true, 100);
        const auto rejected = travel(false, 100);
        const auto reserve = travel(true, 25);
        const auto insufficient = travel(true, 5);
        const auto item = travel(true, 100, true);
        expect(confirmed.first == 1 && confirmed.second >= 174 && confirmed.second <= 184,
               "confirmed haste uses the normal running speed without repeated MP spending");
        expect(rejected.first == 1 && rejected.second >= 114 && rejected.second <= 124,
               "an unconfirmed haste request keeps walking speed and is not spammed");
        expect(
            reserve.first == 1 && insufficient.first == 0 && item.first == 0,
            "haste uses available MP without an arbitrary reserve and preserves active item haste");
        for (int clazz = 0; clazz < 4; ++clazz) {
            expect(travel(true, 25, false, clazz, 1048).first == 1,
                   "every class activates haste for short approaches");
            expect(
                travel(true, 25, false, clazz, 1030).first == 1,
                "every class keeps haste enabled even when its current target is already in range");
        }
        for (int clazz = 1; clazz < 4; ++clazz) {
            std::filesystem::remove(fixture.root + "/skills/run" + std::to_string(clazz) + ".json");
        }
        fixture.data = fixture.load_data();
        expect(
            travel(true, 6, false, 3, 1048).first == 1 &&
                travel(true, 5, false, 3, 1048).first == 0,
            "a thief with learned run mode uses the server's six-MP fallback without a definition");
        expect(travel(true, 100, false, 3, 1048, false).first == 0,
               "a missing catalog definition does not grant unlearned run mode");
        text(fixture.root + "/skills/run3.json",
             R"({"class":3,"index":0,"type":["use"],"mp":9,"maxLevel":1})");
        fixture.data = fixture.load_data();
        expect(travel(true, 8, false, 3, 1048).first == 0 &&
                   travel(true, 9, false, 3, 1048).first == 1,
               "an explicit thief run definition retains its configured MP cost");
        std::filesystem::remove(fixture.root + "/skills/run3.json");
        fixture.data = fixture.load_data();
    }
    void portal_routing(Fixture& fixture) {
        const auto npc = platform::File::read(fixture.root + "/npcs/1.json");
        const auto monsterDefinition = platform::File::read(fixture.root + "/monsters/1.json");
        if (!npc || !monsterDefinition) {
            throw std::runtime_error("Missing owned portal fixture");
        }
        text(fixture.root + "/npcs/1.json",
             R"({"index":1,"map":3,"html":100,"xy":[1120, 1120, 10000]})");
        text(
            fixture.root + "/monsters/1.json",
            R"({"index":1,"stats":{"race":0,"level":1,"hp":10},"spawns":[{"map":3,"area":1,"max":1,"cycle":60,"rect":[36, 30, 38, 34]}]})");
        for (const bool blockedShortcut : {false, true}) {
            const std::string shortcut =
                blockedShortcut
                    ? R"({"id":"owned_blocked","fromMap":0,"fromRect":[0, 0, 64, 64],"toMap":3,"to":[1040, 1040, 10000]},)"
                    : "";
            text(
                fixture.root + "/portals.json",
                ("{\"portals\":[" + shortcut +
                 R"({"id":"owned_first","fromMap":0,"fromRect":[34, 32, 35, 33],"toMap":1,"to":[1040, 1040, 10000]},
                     {"id":"owned_other","fromMap":0,"fromRect":[32, 34, 33, 35],"toMap":2,"to":[1040, 1040, 10000]},
                     {"id":"owned_final","fromMap":1,"fromRect":[32, 32, 33, 33],"toMap":3,"to":[1040, 1040, 10000]},
                     {"id":"owned_cycle","fromMap":2,"fromRect":[32, 32, 33, 33],"toMap":0,"to":[1040, 1040, 10000]},
                     {"id":"owned_other_final","fromMap":2,"fromRect":[32, 32, 33, 33],"toMap":3,"to":[1040, 1040, 10000]}]})")
                    .c_str());
            fixture.data = fixture.load_data();
            auto player = fixture.player(false);
            const auto discard = [](const auto&) { return true; };
            std::uint64_t now = 10000;
            for (; now < 11000; now += 10) {
                player->tick(now, discard);
                if (player->status() == "Travelling through portal owned_first") {
                    break;
                }
            }
            expect(player->status() == "Travelling through portal owned_first",
                   blockedShortcut
                       ? "a rejected portal immediately refreshes reachability for hunting fallback"
                       : "multi-hop portal routes preserve authored ties and tolerate cycles");
            deliver(*player, protocol::Teleport::frame_server(1, 1040, 1040, 10000, 0));
            deliver(*player, protocol::Move::frame_before(77, 1040, 1040, 10000, 0));
            const auto arrival = now + 100;
            player->tick(arrival, discard);
            expect(player->status() == "Travelling through portal owned_final",
                   "a map change rebuilds portal routes from the new origin");
            player->tick(arrival + 3100, discard);
            expect(player->status().find("Travelling through portal") != 0,
                   "a portal on cooldown cannot remain reachable through the decision cache");
            player->tick(arrival + 31001, discard);
            expect(player->status() == "Travelling through portal owned_final",
                   "portal reachability is reevaluated when a cooldown expires");
        }
        expect(test::write_bytes(fixture.root + "/npcs/1.json", *npc) &&
                   test::write_bytes(fixture.root + "/monsters/1.json", *monsterDefinition),
               "restore owned catalogs after portal routing");
        std::filesystem::remove(fixture.root + "/portals.json");
        fixture.data = fixture.load_data();
    }

    void stalled_movement(Fixture& fixture) {
        auto player = fixture.player();
        monster(*player, 1120);
        std::vector<Packet> sent;
        const auto send = [&](const auto& bytes) {
            sent.push_back(unpack(bytes));
            return true;
        };
        std::uint64_t firstMoveAt = 10000;
        for (int slice = 0; slice < 32; ++slice) {
            firstMoveAt = 10000 + static_cast<std::uint64_t>(slice) * 50;
            player->tick(firstMoveAt, send);
            expect(player->position().x == 1024,
                   "planning and requested movement preserve the confirmed pose before an echo");
            if (!sent.empty() || player->status().find("Planning route") != 0) {
                break;
            }
        }
        const bool movementRequest =
            sent.size() == 1 &&
            (sent.front().type == static_cast<std::uint8_t>(protocol::C2S::MoveOn) ||
             sent.front().type == static_cast<std::uint8_t>(protocol::C2S::MoveEnd));
        expect(movementRequest, "ordinary movement begins with one small request");
        if (!movementRequest) {
            return;
        }
        std::int8_t dx = 0, dy = 0, dz = 0;
        expect(protocol::Move::parse_client(
                   sent.front().payload.data(), sent.front().payload.size(), dx, dy, dz),
               "decode the in-flight movement request");
        sent.clear();
        for (std::uint64_t time = firstMoveAt + 10; time <= firstMoveAt + 100; time += 10) {
            player->tick(time, send);
        }
        expect(sent.empty(), "no second movement delta is sent before the first is confirmed");
        deliver(*player, protocol::Move::frame_player_on(77, dx, dy, dz));
        player->tick(firstMoveAt + 500, send);
        std::int8_t nextX = 0, nextY = 0, nextZ = 0;
        expect(sent.size() == 1 &&
                   protocol::Move::parse_client(sent.front().payload.data(),
                                                sent.front().payload.size(),
                                                nextX,
                                                nextY,
                                                nextZ) &&
                   std::hypot(nextX, nextY) <= 3,
               "a delayed echo never releases a banked movement burst");
    }
    void dialog_lifetime(Fixture& fixture) {
        for (int cause = 0; cause < 4; ++cause) {
            auto player = fixture.player(false);
            std::vector<Packet> sent;
            const auto send = [&](const auto& bytes) {
                sent.push_back(unpack(bytes));
                return true;
            };
            player->tick(10000, send);
            expect(sent.size() == 1 &&
                       sent.front().type == static_cast<std::uint8_t>(protocol::C2S::AskNpc),
                   "NPC conversation starts inside direct-click range");
            deliver(*player, protocol::Quest::frame_send_html(100, 800));
            if (cause == 0) {
                deliver(*player, protocol::Move::frame_before(77, 1048, 1024, 10000, 0));
            } else if (cause == 1) {
                deliver(*player, protocol::Move::frame_before(77, 1049, 1024, 10000, 0));
            } else if (cause == 2) {
                deliver(*player, protocol::Quest::frame_remove_npc(800));
            } else {
                deliver(*player, protocol::Teleport::frame_server(0, 1024, 1024, 10000, 0));
            }
            sent.clear();
            // Also exercise a delayed answer after the NPC became unavailable.
            deliver(*player, protocol::Quest::frame_send_html(100, 800));
            player->tick(12000, send);
            const bool advanced = std::any_of(sent.begin(), sent.end(), [](const Packet& packet) {
                return packet.type == static_cast<std::uint8_t>(protocol::C2S::Quest);
            });
            expect(advanced == (cause == 0),
                   "only a conversation still within the 24-pixel boundary can advance quests");
        }
        auto visited = fixture.player(false);
        const auto discard = [](const auto&) { return true; };
        visited->tick(10000, discard);
        deliver(*visited, protocol::Teleport::frame_server(0, 1024, 1024, 10000, 0));
        protocol::CreateNpcInfo npc;
        npc.id = 800;
        npc.index = 1;
        npc.x = npc.y = 1024;
        npc.z = 10000;
        deliver(*visited, protocol::Quest::frame_create_npc(npc));
        bool askedAgain = false;
        visited->tick(11000, [&](const auto& bytes) {
            askedAgain |= unpack(bytes).type == static_cast<std::uint8_t>(protocol::C2S::AskNpc);
            return true;
        });
        expect(!askedAgain, "teleports preserve the cooldown of a recently visited NPC");
    }
    void unseen_npc_approach(Fixture& fixture, bool merchant = false) {
        const auto area = fixture.root + "/maps/n_000_000.akarea";
        const auto originalArea = platform::File::read(area);
        const auto npcFile = fixture.root + "/npcs/1.json";
        const auto originalNpc = platform::File::read(npcFile);
        map::AreaCell movement;
        expect(map::AreaMapCodec::load(area, movement), "read owned unseen NPC approach area");
        std::vector<map::AreaCell::SubcellMask> masks(
            static_cast<std::size_t>(map::kTileCount) * map::kTileCount, 0);
        for (int y = 31; y <= 33; ++y) {
            for (int x = 74; x <= 76; ++x) {
                masks[static_cast<std::size_t>(y) * map::kTileCount + x] = 0xffff;
            }
        }
        movement.set_not_moveable_masks(std::move(masks));
        expect(map::AreaMapCodec::save(area, movement), "block the unseen NPC's close approach");
        text(npcFile, R"({"index":1,"html":100,"xy":[2400, 1024, 10000]})");
        fixture.data = fixture.load_data();
        auto player = fixture.player(merchant);
        if (merchant) {
            for (std::uint32_t id = 100; id < 160; ++id) {
                insert(*player, id, 1);
            }
        }
        deliver(*player, protocol::Quest::frame_remove_npc(800));
        protocol::CreateNpcInfo npc;
        npc.id = 801;
        npc.index = 1;
        npc.x = 2400;
        npc.y = 1024;
        npc.z = 10000;
        bool visible = false, asked = false, discoveryFailed = false;
        for (std::uint64_t now = 10000; now <= 80000 && !asked; now += 50) {
            if (!visible &&
                std::hypot(player->position().x - npc.x, player->position().y - npc.y) <= 500) {
                deliver(*player, protocol::Quest::frame_create_npc(npc));
                visible = true;
            }
            std::vector<Packet> sent;
            player->tick(now, [&](const auto& bytes) {
                sent.push_back(unpack(bytes));
                return true;
            });
            discoveryFailed |= !visible && player->status().find("Route planning timed out;") == 0;
            for (const auto& packet : sent) {
                if (packet.type == static_cast<std::uint8_t>(protocol::C2S::MoveOn) ||
                    packet.type == static_cast<std::uint8_t>(protocol::C2S::MoveEnd)) {
                    std::int8_t dx = 0, dy = 0, dz = 0;
                    expect(protocol::Move::parse_client(
                               packet.payload.data(), packet.payload.size(), dx, dy, dz),
                           "decode unseen NPC approach movement");
                    deliver(*player,
                            packet.type == static_cast<std::uint8_t>(protocol::C2S::MoveOn)
                                ? protocol::Move::frame_player_on(77, dx, dy, dz)
                                : protocol::Move::frame_player_end(77, dx, dy, dz));
                } else if (packet.type == static_cast<std::uint8_t>(merchant
                                                                        ? protocol::C2S::ItemList
                                                                        : protocol::C2S::AskNpc)) {
                    std::uint32_t id = 0;
                    const bool parsed = merchant
                                            ? protocol::Shop::parse_item_list_c2s(
                                                  packet.payload.data(), packet.payload.size(), id)
                                            : protocol::Quest::parse_ask_npc(
                                                  packet.payload.data(), packet.payload.size(), id);
                    expect(visible && parsed && id == npc.id,
                           "discovery uses the actual visible NPC entity for conversation");
                    asked = true;
                }
            }
        }
        expect(visible && asked && !discoveryFailed &&
                   std::hypot(player->position().x - npc.x, player->position().y - npc.y) <=
                       protocol::kNpcReplyRange,
               merchant ? "an obstructed unseen merchant is reached before requesting its shop"
                        : "an obstructed unseen NPC is reached without alternating failed searches "
                          "and hunting");
        expect(test::write_bytes(area, *originalArea), "restore unseen NPC navigation fixture");
        expect(test::write_bytes(npcFile, *originalNpc), "restore unseen NPC catalog fixture");
        fixture.data = fixture.load_data();
    }
    void blocked_npc_approach(Fixture& fixture) {
        const auto area = fixture.root + "/maps/n_000_000.akarea";
        const auto originalArea = platform::File::read(area);
        const auto npcFile = fixture.root + "/npcs/1.json";
        const auto originalNpc = platform::File::read(npcFile);
        map::AreaCell movement;
        expect(map::AreaMapCodec::load(area, movement), "read owned NPC approach area");
        std::vector<map::AreaCell::SubcellMask> masks(
            static_cast<std::size_t>(map::kTileCount) * map::kTileCount, 0);
        for (int y = 31; y <= 33; ++y) {
            for (int x = 33; x <= 35; ++x) {
                masks[static_cast<std::size_t>(y) * map::kTileCount + x] = 0xffff;
            }
        }
        movement.set_not_moveable_masks(std::move(masks));
        expect(map::AreaMapCodec::save(area, movement), "block the NPC's immediate surroundings");
        text(npcFile, R"({"index":1,"html":100,"xy":[1088, 1024, 10000]})");
        fixture.data = fixture.load_data();
        auto player = fixture.player(false);
        protocol::CreateNpcInfo npc;
        npc.id = 800;
        npc.index = 1;
        npc.x = 1088;
        npc.y = 1024;
        npc.z = 10000;
        deliver(*player, protocol::Quest::frame_create_npc(npc));
        bool asked = false, advanced = false;
        int teleports = 0;
        for (std::uint64_t now = 10000; now <= 22000 && !advanced; now += 100) {
            std::vector<Packet> sent;
            player->tick(
                now,
                [&](const auto& bytes) {
                    sent.push_back(unpack(bytes));
                    return true;
                },
                std::chrono::milliseconds{2},
                [&](std::size_t) {
                    ++teleports;
                    return true;
                });
            for (const auto& packet : sent) {
                if (packet.type == static_cast<std::uint8_t>(protocol::C2S::AskNpc)) {
                    std::uint32_t id = 0;
                    expect(protocol::Quest::parse_ask_npc(
                               packet.payload.data(), packet.payload.size(), id) &&
                               id == npc.id,
                           "blocked NPC approach addresses the authoritative runtime entity");
                    asked = true;
                    deliver(*player, protocol::Quest::frame_send_html(100, npc.id));
                }
                advanced |= packet.type == static_cast<std::uint8_t>(protocol::C2S::Quest);
            }
        }
        expect(asked && advanced && teleports == 0 && player->position().x == 1024,
               "a blocked close approach talks and follows the dialog from the legal 64-pixel "
               "boundary without teleporting or crossing blocked tiles");
        int furtherActions = 0;
        const auto waitForReply = [&](std::uint64_t now) {
            player->tick(
                now,
                [&](const auto&) {
                    ++furtherActions;
                    return true;
                },
                std::chrono::milliseconds{2},
                [&](std::size_t) {
                    ++teleports;
                    return true;
                });
        };
        waitForReply(18000);
        expect(player->active() &&
                   player->status().find("waiting for NPC 1 reply") != std::string::npos &&
                   furtherActions == 0 && teleports == 0,
               "an unanswered quest request holds the conversation instead of travelling away");
        waitForReply(22000);
        expect(!player->active() &&
                   player->status().find("NPC 1 did not reply") != std::string::npos &&
                   furtherActions == 0 && teleports == 0,
               "a missing NPC reply pauses with an explicit reason instead of a teleport loop");
        for (const auto* message : {"Quest reward unavailable: item 999.",
                                    "Make room in your inventory before continuing this quest."}) {
            player = fixture.player(false);
            deliver(*player, protocol::Quest::frame_create_npc(npc));
            bool requested = false;
            for (std::uint64_t now = 10000; now <= 22000 && !requested; now += 100) {
                player->tick(now, [&](const auto& bytes) {
                    const auto type = unpack(bytes).type;
                    requested |= type == static_cast<std::uint8_t>(protocol::C2S::AskNpc) ||
                                 type == static_cast<std::uint8_t>(protocol::C2S::Quest);
                    return true;
                });
            }
            expect(requested, "start the NPC conversation before receiving a rejection");
            deliver(*player, protocol::Chatting::frame_s2c("OtherPlayer", message));
            deliver(*player, protocol::Chatting::frame_s2c("Server", "Welcome."));
            expect(player->active(), "unrelated chat does not reject a pending NPC request");
            deliver(*player, protocol::Chatting::frame_s2c("Server", message));
            expect(!player->active() && player->status().find(message) != std::string::npos,
                   "quest rejection reports the server reason without waiting for a timeout");
        }
        expect(test::write_bytes(area, *originalArea) && test::write_bytes(npcFile, *originalNpc),
               "restore owned NPC approach fixtures");
        fixture.data = fixture.load_data();
    }

    void spawn_approach(Fixture& fixture) {
        const auto file = fixture.root + "/monsters/1.json";
        const auto area = fixture.root + "/maps/n_000_000.akarea";
        const auto original = platform::File::read(file);
        text(
            file,
            R"({"index":1,"name":"Owned wide region","stats":{"race":0,"level":1,"hp":10},"spawns":[{"map":0,"area":1,"max":4,"cycle":60,"rect":[50, 20, 120, 44]}]})");
        map::AreaCell movement;
        expect(map::AreaMapCodec::load(area, movement), "read owned spawn approach area");
        std::vector<map::AreaCell::SubcellMask> masks(
            static_cast<std::size_t>(map::kTileCount) * map::kTileCount, 0);
        for (int y = 20; y <= 44; ++y) {
            for (int x = 60; x <= 120; ++x) {
                masks[static_cast<std::size_t>(y) * map::kTileCount + x] = 0xffff;
            }
        }
        masks[32 * map::kTileCount + 50] = 0xffff;
        movement.set_not_moveable_masks(std::move(masks));
        expect(map::AreaMapCodec::save(area, movement), "block the wide spawn region's center");
        fixture.data = fixture.load_data();
        auto player = fixture.player();
        bool attacked = false;
        for (std::uint64_t now = 10000; now < 35000 && !attacked; now += 50) {
            std::vector<Packet> sent;
            player->tick(now, [&](const auto& bytes) {
                sent.push_back(unpack(bytes));
                return true;
            });
            for (const auto& packet : sent) {
                if (packet.type == static_cast<std::uint8_t>(protocol::C2S::Attack)) {
                    attacked = true;
                }
                if (packet.type != static_cast<std::uint8_t>(protocol::C2S::MoveOn) &&
                    packet.type != static_cast<std::uint8_t>(protocol::C2S::MoveEnd)) {
                    continue;
                }
                std::int8_t dx = 0, dy = 0, dz = 0;
                expect(protocol::Move::parse_client(
                           packet.payload.data(), packet.payload.size(), dx, dy, dz),
                       "decode normal spawn approach movement");
                deliver(*player,
                        packet.type == static_cast<std::uint8_t>(protocol::C2S::MoveOn)
                            ? protocol::Move::frame_player_on(77, dx, dy, dz)
                            : protocol::Move::frame_player_end(77, dx, dy, dz));
            }
            if (player->position().x >= 1200) {
                monster(*player, 1800, 1024);
            }
        }
        expect(attacked && player->position().x < 1920,
               "a blocked spawn center does not prevent approaching its near edge and attacking "
               "a normally visible monster");
        movement.set_not_moveable_masks(std::vector<map::AreaCell::SubcellMask>(
            static_cast<std::size_t>(map::kTileCount) * map::kTileCount, 0xffff));
        expect(map::AreaMapCodec::save(area, movement),
               "write an entirely unavailable spawn region");
        fixture.data = fixture.load_data();
        auto unavailable = fixture.player();
        bool planned = false;
        for (std::uint64_t now = 10000; now < 30000; now += 1000) {
            unavailable->tick(now, [](const auto&) { return true; });
            planned |= unavailable->status().find("Planning route:") == 0;
        }
        expect(!planned, "entirely blocked spawn regions never start repeated path searches");
        std::vector<map::AreaCell::SubcellMask> islands(
            static_cast<std::size_t>(map::kTileCount) * map::kTileCount, 0xffff);
        islands[32 * map::kTileCount + 32] = 0;
        for (int y = 0; y < map::kTileCount; ++y) {
            for (int x = 50; x < map::kTileCount; ++x) {
                islands[static_cast<std::size_t>(y) * map::kTileCount + x] = 0;
            }
        }
        movement.set_not_moveable_masks(std::move(islands));
        expect(map::AreaMapCodec::save(area, movement),
               "separate a standable spawn region from the player's small island");
        fixture.data = fixture.load_data();
        auto isolated = fixture.player();
        std::map<std::string, int> failures;
        std::string previous;
        for (std::uint64_t now = 10000; now < 90000; now += 100) {
            isolated->tick(now, [](const auto&) { return true; });
            const auto& status = isolated->status();
            if (status.find("No route or missing navigation data (") == 0 && status != previous) {
                ++failures[status];
            }
            previous = status;
        }
        expect(failures.size() > 1 &&
                   std::all_of(failures.begin(),
                               failures.end(),
                               [](const auto& entry) { return entry.second == 1; }) &&
                   isolated->status().find("Waiting after ") == 0,
               "unreachable probes are remembered individually and cannot restart from the "
               "same island indefinitely");
        expect(test::write_bytes(file, *original) && test::write_open_area(area),
               "restore owned spawn and movement fixtures");
        fixture.data = fixture.load_data();
    }
    void area_travel(Fixture& fixture) {
        const auto file = fixture.root + "/monsters/1.json";
        const auto area = fixture.root + "/maps/n_000_000.akarea";
        const auto original = platform::File::read(file);
        text(
            file,
            R"({"index":1,"name":"Owned fallback region","stats":{"race":0,"level":1,"hp":10},"spawns":[{"map":0,"area":1,"max":4,"cycle":60,"rect":[64, 32, 68, 36]}]})");
        map::AreaCell movement;
        expect(map::AreaMapCodec::load(area, movement), "read owned fallback area");
        std::vector<map::AreaCell::SubcellMask> masks(
            static_cast<std::size_t>(map::kTileCount) * map::kTileCount, 0xffff);
        masks[32 * map::kTileCount + 32] = 0;
        for (int y = 0; y < map::kTileCount; ++y) {
            for (int x = 60; x < map::kTileCount; ++x) {
                masks[static_cast<std::size_t>(y) * map::kTileCount + x] = 0;
            }
        }
        movement.set_not_moveable_masks(std::move(masks));
        expect(map::AreaMapCodec::save(area, movement),
               "separate the player from a standable hunting region");
        fixture.data = fixture.load_data();
        std::vector<Packet> sent;
        const auto send = [&](const auto& bytes) {
            sent.push_back(unpack(bytes));
            return true;
        };
        int requests = 0;
        TestPlayerAreaTravel requested;
        const auto accept = [&](const TestPlayerAreaTravel& destination) {
            ++requests;
            requested = destination;
            return true;
        };
        const auto request = [&](TestPlayerAgent& player, const auto& dispatch) {
            std::uint64_t now = 10000;
            for (; now < 17000 && player.active() && !player.pending_area_travel(); now += 20) {
                player.tick(now, send, std::chrono::milliseconds{2}, {}, dispatch);
            }
            return now;
        };
        auto player = fixture.player();
        const auto now = request(*player, accept);
        expect(requests == 1 && player->pending_area_travel() && requested.map == 0 &&
                   requested.area == 1 && requested.position.x >= 2048 &&
                   requested.position.x <= 2176 && requested.position.y >= 1024 &&
                   requested.position.y <= 1152 && requested.position.z == 10000 &&
                   player->position().x == 1024,
               "a failed route queues one validated in-area teleport without changing the pose");
        sent.clear();
        player->tick(now + 100, send, std::chrono::milliseconds{2}, {}, accept);
        expect(sent.empty() && requests == 1 &&
                   player->status() == "Waiting for area teleport confirmation",
               "pending area travel neither retries nor emits gameplay movement");
        const auto goal = requested.position;
        deliver(*player, protocol::Move::frame_before(77, goal.x, goal.y, goal.z, 0));
        expect(player->pending_area_travel().has_value(),
               "a position update without a teleport cannot acknowledge area travel");
        deliver(*player, protocol::Teleport::frame_server(0, goal.x, goal.y, goal.z, 0));
        deliver(*player, protocol::Move::frame_before(77, goal.x + 1, goal.y, goal.z, 0));
        expect(player->pending_area_travel().has_value(),
               "a mismatched arrival pose does not acknowledge the requested destination");
        deliver(*player, protocol::Move::frame_before(77, goal.x, goal.y, goal.z, 0));
        monster(*player, goal.x + 8, goal.y);
        sent.clear();
        player->tick(now + 200, send, std::chrono::milliseconds{2}, {}, accept);
        expect(!player->pending_area_travel() && sent.empty() &&
                   player->status() == "Waiting for spawn-area sight",
               "confirmed arrival allows the normal sight snapshot to settle before decisions");
        player->tick(now + 1200, send, std::chrono::milliseconds{2}, {}, accept);
        expect(requests == 1 && std::any_of(sent.begin(),
                                            sent.end(),
                                            [](const auto& packet) {
                                                return packet.type == static_cast<std::uint8_t>(
                                                                          protocol::C2S::Attack);
                                            }),
               "confirmed fallback arrival resumes ordinary combat without another teleport");
        auto cancelled = fixture.player();
        (void) request(*cancelled, accept);
        cancelled->pause();
        expect(!cancelled->pending_area_travel(), "pause cancels a queued area fallback");
        cancelled->start(fixture.data);
        (void) request(*cancelled, accept);
        cancelled->start(fixture.data);
        expect(!cancelled->pending_area_travel(), "restart invalidates a superseded fallback");
        auto timeout = fixture.player();
        const auto timeoutAt = request(*timeout, accept);
        timeout->tick(timeoutAt + 16000, send, std::chrono::milliseconds{2}, {}, accept);
        expect(!timeout->active() && !timeout->pending_area_travel() &&
                   timeout->status() == "Paused: area teleport confirmation timed out",
               "missing area confirmation pauses instead of repeating teleports");
        auto rejected = fixture.player();
        (void) request(*rejected, [](const auto&) { return false; });
        expect(!rejected->active() && !rejected->pending_area_travel() &&
                   rejected->position().x == 1024,
               "a rejected fallback pauses without guessing its arrival position");
        auto healing = fixture.player();
        const auto healingAt = request(*healing, accept);
        insert(*healing, 42, 2);
        deliver(*healing,
                protocol::Property::frame_update_vital(
                    static_cast<std::uint8_t>(protocol::PropertyType::Hp), 20, 100));
        sent.clear();
        healing->tick(healingAt + 50, send, std::chrono::milliseconds{2}, {}, accept);
        expect(healing->pending_area_travel() &&
                   std::any_of(sent.begin(),
                               sent.end(),
                               [](const auto& packet) {
                                   return packet.type ==
                                          static_cast<std::uint8_t>(protocol::C2S::UseItem);
                               }),
               "healing keeps priority while area confirmation is pending");
        movement.set_not_moveable_masks(std::vector<map::AreaCell::SubcellMask>(
            static_cast<std::size_t>(map::kTileCount) * map::kTileCount, 0xffff));
        expect(map::AreaMapCodec::save(area, movement), "block all fallback landing points");
        fixture.data = fixture.load_data();
        auto blocked = fixture.player();
        requests = 0;
        (void) request(*blocked, accept);
        expect(requests == 0 && !blocked->pending_area_travel(),
               "a completely unstandable region cannot become a teleport destination");
        expect(test::write_bytes(file, *original) && test::write_open_area(area),
               "restore owned fallback fixtures");
        fixture.data = fixture.load_data();
    }
    void planning_timeout(Fixture& fixture) {
        auto player = fixture.player();
        // Exceed a full 32-node slice even with open-ground shortcuts on fast release builds.
        monster(*player, 7900, 7900);
        bool moved = false;
        const auto send = [&](const auto& bytes) {
            const auto packet = unpack(bytes);
            moved |= packet.type == static_cast<std::uint8_t>(protocol::C2S::MoveOn);
            return true;
        };
        player->tick(10000, send);
        expect(!moved && player->status().find("Planning route: ") == 0,
               "long planning jobs yield and expose their current progress");
        monster(*player, 7932, 7900);
        insert(*player, 9, 1, 1, true);
        player->tick(100001, send);
        expect(!moved &&
                   player->status().find("Route planning timed out; choosing another target") == 0,
               "moving targets and changed attack reach retain the planning deadline");
    }
    void standing_height(Fixture& fixture) {
        for (const int initialHeight : {9000, 11000}) {
            auto player = fixture.player();
            monster(*player, 1120);
            deliver(*player, protocol::Move::frame_before(77, 1024, 1024, initialHeight, 0));
            std::vector<Packet> sent;
            const auto send = [&](const auto& bytes) {
                sent.push_back(unpack(bytes));
                return true;
            };
            player->tick(10000, send);
            expect(sent.size() == 1 && player->position().z == initialHeight,
                   "standing height sends one request without changing the confirmed pose");
            player->tick(10010, send);
            expect(sent.size() == 1, "standing height waits for the ordinary server echo");
            bool walked = false;
            int adjustments = 0;
            for (std::uint64_t time = 10050; time < 12000 && !walked; time += 50) {
                auto pending = std::move(sent);
                sent.clear();
                for (const auto& packet : pending) {
                    std::int8_t dx = 0, dy = 0, dz = 0;
                    expect(protocol::Move::parse_client(
                               packet.payload.data(), packet.payload.size(), dx, dy, dz),
                           "decode an ordinary standing-height movement packet");
                    if (dx == 0 && dy == 0) {
                        ++adjustments;
                        expect(packet.type == static_cast<std::uint8_t>(protocol::C2S::MoveEnd) &&
                                   dz != 0 && std::abs(static_cast<int>(dz)) <= 127,
                               "standing height uses bounded idle movement deltas");
                    } else {
                        walked = true;
                        expect(player->position().z == 10000,
                               "horizontal route movement starts only from the confirmed floor");
                    }
                    deliver(*player,
                            packet.type == static_cast<std::uint8_t>(protocol::C2S::MoveOn)
                                ? protocol::Move::frame_player_on(77, dx, dy, dz)
                                : protocol::Move::frame_player_end(77, dx, dy, dz));
                }
                player->tick(time, send);
            }
            expect(walked && adjustments == 8,
                   "a stale pose above or below the floor grounds through confirmations and then "
                   "walks");
        }
        auto rejected = fixture.player();
        deliver(*rejected, protocol::Move::frame_before(77, 1024, 1024, 9000, 0));
        for (std::uint64_t time = 10000; time <= 10300; time += 100) {
            rejected->tick(time, [](const auto&) { return true; });
            deliver(*rejected, protocol::Move::frame_before(77, 1024, 1024, 9000, 0));
        }
        expect(!rejected->active() && rejected->position().z == 9000 &&
                   rejected->status() == "Paused: standing-height movement repeatedly rejected",
               "rejected height adjustments pause after bounded attempts without changing rules");
        auto threatened = fixture.player();
        insert(*threatened, 1, 2, 3);
        deliver(*threatened, protocol::Move::frame_before(77, 1024, 1024, 9000, 0));
        deliver(*threatened,
                protocol::Property::frame_update_vital(
                    static_cast<std::uint8_t>(protocol::PropertyType::Hp), 50, 100));
        std::vector<Packet> packets;
        threatened->tick(10000, [&](const auto& bytes) {
            packets.push_back(unpack(bytes));
            return true;
        });
        expect(!packets.empty() &&
                   packets.front().type == static_cast<std::uint8_t>(protocol::C2S::UseItem),
               "healing retains priority over standing-height synchronization");
    }

    void outdoor_bridge(Fixture& fixture) {
        const auto area = fixture.root + "/maps/n_000_000.akarea";
        const auto opl = fixture.root + "/visuals/n_000_000.opl";
        const auto bridge = fixture.root + "/OBJECTS/owned_bridge.gb";
        expect(test::write_blocked_area(area), "write owned blocked terrain below a bridge");
        fixture.data = fixture.load_data();
        auto navigationOnly = fixture.player();
        monster(*navigationOnly, 1120);
        bool traversed = false;
        for (std::uint64_t now = 10000; now < 11000 && !traversed; now += 10) {
            navigationOnly->tick(now, [&](const auto& bytes) {
                const auto packet = unpack(bytes);
                traversed |= packet.type == static_cast<std::uint8_t>(protocol::C2S::MoveOn);
                return true;
            });
        }
        expect(traversed, "server navigation Blocked bits do not veto ordinary player movement");
        map::AreaCell movement;
        expect(map::AreaMapCodec::load(area, movement), "read owned independent movement mask");
        movement.set_not_moveable_masks(std::vector<map::AreaCell::SubcellMask>(
            static_cast<std::size_t>(map::kTileCount) * map::kTileCount, 0xffff));
        expect(map::AreaMapCodec::save(area, movement), "set explicit player NotMoveable bits");
        std::filesystem::create_directories(fixture.root + "/OBJECTS");
        expect(test::write_cls_only_gb_v11(bridge, 200), "write owned collision-only bridge deck");
        const auto writePlacement = [&](float height) {
            return test::write_opl_v4(
                opl,
                0,
                0,
                {{"DATA\\OBJECTS\\owned_bridge", {1000.0f / 8192, 1000.0f / 8192, height}}});
        };
        expect(writePlacement(1005), "place the owned bridge in server-coordinate space");
        fixture.data = fixture.load_data();
        auto player = fixture.player();
        monster(*player, 1120);
        bool walked = false, correctHeight = true;
        for (std::uint64_t time = 10000; time < 14000 && player->position().x < 1080; time += 10) {
            std::vector<Packet> sent;
            player->tick(time, [&](const auto& bytes) {
                sent.push_back(unpack(bytes));
                return true;
            });
            for (const auto& packet : sent) {
                if (packet.type != static_cast<std::uint8_t>(protocol::C2S::MoveOn) &&
                    packet.type != static_cast<std::uint8_t>(protocol::C2S::MoveEnd)) {
                    continue;
                }
                std::int8_t dx = 0, dy = 0, dz = 0;
                expect(protocol::Move::parse_client(
                           packet.payload.data(), packet.payload.size(), dx, dy, dz),
                       "decode bridge movement");
                if (dx || dy) {
                    walked = true;
                    correctHeight &= player->position().z == 10050;
                }
                deliver(*player,
                        packet.type == static_cast<std::uint8_t>(protocol::C2S::MoveOn)
                            ? protocol::Move::frame_player_on(77, dx, dy, dz)
                            : protocol::Move::frame_player_end(77, dx, dy, dz));
            }
        }
        expect(walked && correctHeight && player->position().x >= 1080 &&
                   player->position().z == 10050,
               "ordinary confirmed movement follows a collision-only bridge above blocked terrain");
        for (const bool missing : {false, true}) {
            if (missing) {
                std::filesystem::remove(bridge);
            } else {
                expect(writePlacement(1030),
                       "place an unreachable high roof above blocked terrain");
            }
            fixture.data = fixture.load_data();
            auto blocked = fixture.player();
            monster(*blocked, 1120);
            bool moved = false;
            for (std::uint64_t time = 10000; time < 10100; time += 10) {
                blocked->tick(time, [&](const auto& bytes) {
                    const auto packet = unpack(bytes);
                    moved |= packet.type == static_cast<std::uint8_t>(protocol::C2S::MoveOn) ||
                             packet.type == static_cast<std::uint8_t>(protocol::C2S::MoveEnd);
                    return true;
                });
            }
            expect(!moved,
                   missing ? "missing collision data cannot bypass blocked terrain"
                           : "a high roof cannot substitute for a reachable bridge deck");
        }
        std::filesystem::remove(opl);
        expect(test::write_open_area(area), "restore owned open terrain");
        fixture.data = fixture.load_data();
    }

    void neighboring_bridge(Fixture& fixture) {
        const auto area = fixture.root + "/maps/n_000_000.akarea";
        const auto opl = fixture.root + "/visuals/n_001_000.opl";
        const auto model = fixture.root + "/OBJECTS/owned_neighbor_bridge.gb";
        map::AreaCell movement;
        expect(map::AreaMapCodec::load(area, movement), "read owned bridge approach area");
        movement.set_not_moveable_masks(std::vector<map::AreaCell::SubcellMask>(
            static_cast<std::size_t>(map::kTileCount) * map::kTileCount, 0xffff));
        expect(map::AreaMapCodec::save(area, movement),
               "block the river below the neighboring deck");
        std::filesystem::create_directories(fixture.root + "/OBJECTS");
        expect(test::write_cls_only_gb_v11(model, 200), "write owned neighboring bridge geometry");
        expect(test::write_opl_v4(opl,
                                  1,
                                  0,
                                  {{"DATA\\OBJECTS\\owned_neighbor_bridge",
                                    {(8100.0f - 8192) / 8192, 1000.0f / 8192, 1005}}}),
               "place a bridge extending into the current cell from its neighbor");
        fixture.data = fixture.load_data();
        for (const int initialHeight : {10050, 11000}) {
            auto player = fixture.player();
            deliver(*player, protocol::Move::frame_before(77, 8120, 1024, initialHeight, 0));
            monster(*player, 8180);
            bool walked = false, onDeck = true;
            for (std::uint64_t now = 10000; now < 14000 && player->position().x < 8160; now += 10) {
                std::vector<Packet> sent;
                player->tick(now, [&](const auto& bytes) {
                    sent.push_back(unpack(bytes));
                    return true;
                });
                for (const auto& packet : sent) {
                    if (packet.type != static_cast<std::uint8_t>(protocol::C2S::MoveOn) &&
                        packet.type != static_cast<std::uint8_t>(protocol::C2S::MoveEnd)) {
                        continue;
                    }
                    std::int8_t dx = 0, dy = 0, dz = 0;
                    expect(protocol::Move::parse_client(
                               packet.payload.data(), packet.payload.size(), dx, dy, dz),
                           "decode neighboring bridge movement");
                    if (dx || dy) {
                        walked = true;
                        onDeck &= player->position().z == 10050;
                    }
                    deliver(*player,
                            packet.type == static_cast<std::uint8_t>(protocol::C2S::MoveOn)
                                ? protocol::Move::frame_player_on(77, dx, dy, dz)
                                : protocol::Move::frame_player_end(77, dx, dy, dz));
                    onDeck &= player->position().z >= 10050;
                }
            }
            expect(walked && onDeck && player->position().x >= 8160 &&
                       player->position().z == 10050,
                   "a neighboring bridge supports both standing and initially floating players");
        }
        std::filesystem::remove(opl);
        std::filesystem::remove(model);
        expect(test::write_open_area(area), "restore owned terrain after neighboring bridge test");
        fixture.data = fixture.load_data();
    }

    void water_bridge_step(Fixture& fixture) {
        const auto opl = fixture.root + "/visuals/n_000_000.opl";
        const auto kcm = fixture.root + "/visuals/n_000_000.kcm";
        const auto model = fixture.root + "/OBJECTS/owned_step_bridge.gb";
        expect(test::write_kcm_v7(kcm, 0, 0, 0, 1, 1580), "write owned riverbed height");
        std::filesystem::create_directories(fixture.root + "/OBJECTS");
        expect(test::write_cls_only_gb_v11(model, 200), "write owned water-bridge entry");
        expect(test::write_opl_v4(
                   opl,
                   0,
                   0,
                   {{"DATA\\OBJECTS\\owned_step_bridge", {1040.0f / 8192, 1000.0f / 8192, 1591}}}),
               "place a normal water bridge above the riverbed");
        fixture.data = fixture.load_data();
        auto player = fixture.player();
        deliver(*player, protocol::Move::frame_before(77, 1024, 1024, 15800, 0));
        monster(*player, 1120);
        for (std::uint64_t now = 10000; now < 18000 && player->position().x < 1080; now += 10) {
            std::vector<Packet> packets;
            player->tick(now, [&](const auto& bytes) {
                packets.push_back(unpack(bytes));
                return true;
            });
            for (const auto& packet : packets) {
                if (packet.type != static_cast<std::uint8_t>(protocol::C2S::MoveOn) &&
                    packet.type != static_cast<std::uint8_t>(protocol::C2S::MoveEnd)) {
                    continue;
                }
                std::int8_t dx = 0, dy = 0, dz = 0;
                expect(protocol::Move::parse_client(
                           packet.payload.data(), packet.payload.size(), dx, dy, dz),
                       "decode a normal bounded bridge-entry movement");
                deliver(*player,
                        packet.type == static_cast<std::uint8_t>(protocol::C2S::MoveOn)
                            ? protocol::Move::frame_player_on(77, dx, dy, dz)
                            : protocol::Move::frame_player_end(77, dx, dy, dz));
            }
        }
        expect(player->position().x >= 1080 && player->position().z == 15910,
               "water-bridge entry uses the ordinary floor query and signed movement-height limit");
        std::filesystem::remove(opl);
        std::filesystem::remove(model);
        expect(test::write_kcm_v7(kcm, 0, 0, 0, 1, 1000), "restore owned terrain height");
        fixture.data = fixture.load_data();
    }
    void bridge_approach(Fixture& fixture) {
        const auto area = fixture.root + "/maps/n_000_000.akarea";
        const auto opl = fixture.root + "/visuals/n_000_000.opl";
        const auto model = fixture.root + "/OBJECTS/owned_detour_bridge.gb";
        map::AreaCell movement;
        expect(map::AreaMapCodec::load(area, movement), "read owned river crossing area");
        std::vector<map::AreaCell::SubcellMask> masks(
            static_cast<std::size_t>(map::kTileCount) * map::kTileCount, 0);
        for (int y = 0; y < map::kTileCount; ++y) {
            for (int x = 35; x < 38; ++x) {
                masks[static_cast<std::size_t>(y) * map::kTileCount + x] = 0xffff;
            }
        }
        movement.set_not_moveable_masks(std::move(masks));
        expect(map::AreaMapCodec::save(area, movement), "block the owned river below the deck");
        std::filesystem::create_directories(fixture.root + "/OBJECTS");
        expect(test::write_cls_only_gb_v11(model, 256), "write owned offset bridge geometry");
        expect(test::write_opl_v4(
                   opl,
                   0,
                   0,
                   {{"DATA\\OBJECTS\\owned_detour_bridge", {1088.0f / 8192, 800.0f / 8192, 1005}}}),
               "place the bridge entrance away from the direct destination bearing");
        fixture.data = fixture.load_data();
        auto player = fixture.player();
        monster(*player, 1400);
        bool crossed = false, validDeck = true;
        for (std::uint64_t now = 10000; now < 70000 && player->position().x < 1280; now += 10) {
            std::vector<Packet> sent;
            player->tick(now, [&](const auto& bytes) {
                sent.push_back(unpack(bytes));
                return true;
            });
            for (const auto& packet : sent) {
                if (packet.type != static_cast<std::uint8_t>(protocol::C2S::MoveOn) &&
                    packet.type != static_cast<std::uint8_t>(protocol::C2S::MoveEnd)) {
                    continue;
                }
                std::int8_t dx = 0, dy = 0, dz = 0;
                expect(protocol::Move::parse_client(
                           packet.payload.data(), packet.payload.size(), dx, dy, dz),
                       "decode a detour bridge movement");
                deliver(*player,
                        packet.type == static_cast<std::uint8_t>(protocol::C2S::MoveOn)
                            ? protocol::Move::frame_player_on(77, dx, dy, dz)
                            : protocol::Move::frame_player_end(77, dx, dy, dz));
                const auto pose = player->position();
                if (pose.x >= 1120 && pose.x < 1216) {
                    crossed = true;
                    validDeck &=
                        pose.z == 10050 && pose.y >= 800 && pose.x - 1088 + pose.y - 800 <= 256;
                }
            }
        }
        expect(
            crossed && validDeck && player->position().x >= 1280,
            "player detours from the blocked bank to the bridge entrance and crosses on its deck");
        std::filesystem::remove(opl);
        std::filesystem::remove(model);
        expect(test::write_open_area(area), "restore the owned open area after the bridge detour");
        fixture.data = fixture.load_data();
    }
    void shared_navigation_layers(Fixture& fixture) {
        const auto opl = fixture.root + "/visuals/n_000_000.opl";
        const auto model = fixture.root + "/OBJECTS/owned_layered_deck.gb";
        std::filesystem::create_directories(fixture.root + "/OBJECTS");
        expect(test::write_cls_only_gb_v11(model, 200), "write owned layered navigation deck");
        expect(test::write_opl_v4(
                   opl,
                   0,
                   0,
                   {{"DATA\\OBJECTS\\owned_layered_deck", {1000.0f / 8192, 1000.0f / 8192, 1030}}}),
               "place a high deck above an ordinary ground passage");
        fixture.data = fixture.load_data();
        for (const int initialHeight : {10000, 10300, 11000}) {
            const int height = initialHeight == 10000 ? 10000 : 10300;
            auto player = fixture.player();
            deliver(*player, protocol::Move::frame_before(77, 1024, 1024, initialHeight, 0));
            monster(*player, 1120);
            bool stayedOnLayer = true;
            for (std::uint64_t now = 10000; now < 14000 && player->position().x < 1080; now += 10) {
                std::vector<Packet> sent;
                player->tick(now, [&](const auto& bytes) {
                    sent.push_back(unpack(bytes));
                    return true;
                });
                for (const auto& packet : sent) {
                    if (packet.type != static_cast<std::uint8_t>(protocol::C2S::MoveOn) &&
                        packet.type != static_cast<std::uint8_t>(protocol::C2S::MoveEnd)) {
                        continue;
                    }
                    std::int8_t dx = 0, dy = 0, dz = 0;
                    expect(protocol::Move::parse_client(
                               packet.payload.data(), packet.payload.size(), dx, dy, dz),
                           "decode a layered navigation movement");
                    deliver(*player,
                            packet.type == static_cast<std::uint8_t>(protocol::C2S::MoveOn)
                                ? protocol::Move::frame_player_on(77, dx, dy, dz)
                                : protocol::Move::frame_player_end(77, dx, dy, dz));
                    stayedOnLayer &= player->position().z >= height;
                    if (dx || dy) {
                        stayedOnLayer &= player->position().z == height;
                    }
                }
            }
            expect(stayedOnLayer && player->position().x >= 1080 && player->position().z == height,
                   "players sharing navigation data settle on their own ground or bridge layer");
        }
        std::filesystem::remove(opl);
        std::filesystem::remove(model);
        fixture.data = fixture.load_data();
    }
    void thin_obstacle(Fixture& fixture) {
        const auto opl = fixture.root + "/visuals/n_000_000.opl";
        const auto model = fixture.root + "/OBJECTS/owned_thin_ledge.gb";
        std::filesystem::create_directories(fixture.root + "/OBJECTS");
        expect(test::write_cls_only_gb_v11(model, 1), "write owned one-pixel standing obstruction");
        expect(test::write_opl_v4(
                   opl,
                   0,
                   0,
                   {{"DATA\\OBJECTS\\owned_thin_ledge", {1033.0f / 8192, 1040.0f / 8192, 1600}}}),
               "place thin obstruction between coarse validation samples");
        fixture.data = fixture.load_data();
        auto player = fixture.player();
        deliver(*player, protocol::Move::frame_before(77, 1024, 1040, 10000, 0));
        monster(*player, 1120, 1040);
        bool blocked = false, clipped = false;
        for (std::uint64_t now = 10000; now < 18000 && player->position().x < 1080; now += 10) {
            std::vector<Packet> packets;
            player->tick(now, [&](const auto& bytes) {
                packets.push_back(unpack(bytes));
                return true;
            });
            blocked |= player->status().find("Refining blocked route") == 0 ||
                       player->status().find("Route blocked;") == 0;
            for (const auto& packet : packets) {
                if (packet.type != static_cast<std::uint8_t>(protocol::C2S::MoveOn) &&
                    packet.type != static_cast<std::uint8_t>(protocol::C2S::MoveEnd)) {
                    continue;
                }
                std::int8_t dx = 0, dy = 0, dz = 0;
                expect(protocol::Move::parse_client(
                           packet.payload.data(), packet.payload.size(), dx, dy, dz),
                       "decode movement around a thin standing obstruction");
                deliver(*player,
                        packet.type == static_cast<std::uint8_t>(protocol::C2S::MoveOn)
                            ? protocol::Move::frame_player_on(77, dx, dy, dz)
                            : protocol::Move::frame_player_end(77, dx, dy, dz));
                const auto pose = player->position();
                clipped |= pose.x >= 1033 && pose.x <= 1034 && pose.y == 1040;
            }
        }
        expect(player->position().x >= 1080 && !blocked && !clipped,
               ("per-pixel planning avoids an obstruction missed by four-pixel samples: x=" +
                std::to_string(player->position().x) + ", y=" +
                std::to_string(player->position().y) + ", blocked=" + std::to_string(blocked) +
                ", clipped=" + std::to_string(clipped) + ", status=" + player->status())
                   .c_str());
        std::filesystem::remove(opl);
        std::filesystem::remove(model);
        fixture.data = fixture.load_data();
    }
    std::string capture_warnings(const std::function<void()>& action) {
        std::unique_ptr<std::FILE, decltype(&std::fclose)> output(std::tmpfile(), &std::fclose);
        if (!output) {
            throw std::runtime_error("Could not capture fixture diagnostics");
        }
        const int saved = dup(STDERR_FILENO);
        if (saved < 0) {
            throw std::runtime_error("Could not save fixture stderr");
        }
        struct Restore {
            int fd;
            ~Restore() {
                std::fflush(stderr);
                (void) dup2(fd, STDERR_FILENO);
                close(fd);
            }
        } restore{saved};
        std::fflush(stderr);
        if (dup2(fileno(output.get()), STDERR_FILENO) < 0) {
            throw std::runtime_error("Could not redirect fixture stderr");
        }
        action();
        std::fflush(stderr);
        std::rewind(output.get());
        std::string result;
        char buffer[1024];
        while (const auto count = std::fread(buffer, 1, sizeof(buffer), output.get())) {
            result.append(buffer, count);
        }
        return result;
    }
    void unsupported_quest(Fixture& fixture) {
        text(fixture.root + "/npcs/2.json", R"({"index":2,"html":200,"xy":[1024, 1024, 10000]})");
        text(fixture.root + "/dialogs/200.json",
             R"({"id":200,"links":[{"type":"quest","text":"Owned transition","quest":[71, 1]}]})");
        text(
            fixture.root + "/quests/70_1.json",
            R"({"index":70,"flag":1,"cases":[{"if":[{"op":"quest","quest":70,"flag":0}],"then":[{"op":"html","raw":[]},{"op":"save","quest":70,"flag":1}]},{"if":[{"op":"quest","quest":70,"flag":1}],"then":[{"op":"save","quest":70,"flag":2}]}]})");
        text(fixture.root + "/quests/71_1.json",
             R"({"index":71,"flag":1,"cases":[{"then":[{"op":"link","quest":70,"flag":1}]}]})");
        fixture.data = fixture.load_data();
        server::QuestDb db;
        expect(db.load_dir(fixture.root + "/quests"), "load owned unsupported quest fixture");
        server::QuestEngine engine(db);
        server::PlayerQuestMap progress;
        server::QuestActionSink sink;
        std::uint32_t reported = 0;
        int reports = 0, effects = 0;
        sink.report_diagnostic = [&](std::uint32_t key, std::string_view reason) {
            reported = key;
            reports += reason == "unsupported action unsupported:html";
        };
        sink.send_html = [&](int) { ++effects; };
        sink.send_save_quest = [&](const auto&) { ++effects; };
        expect(!engine.call_process(protocol::Quest::make_key(71, 1), progress, sink) &&
                   reports == 1 && reported == protocol::Quest::make_key(70, 1) && effects == 0 &&
                   progress.empty(),
               "diagnostic collection reports the actual linked failure without applying quest "
               "effects");
        bool questSent = false, hunted = false;
        const auto send = [&](const auto& bytes) {
            const auto packet = unpack(bytes);
            questSent |= packet.type == static_cast<std::uint8_t>(protocol::C2S::AskNpc) ||
                         packet.type == static_cast<std::uint8_t>(protocol::C2S::Quest);
            hunted |= packet.type == static_cast<std::uint8_t>(protocol::C2S::Attack);
            return true;
        };
        auto player = fixture.player();
        const auto warnings = capture_warnings([&] {
            for (int client = 0; client < 3; ++client) {
                auto current = fixture.player();
                monster(*current);
                for (std::uint64_t now = 10000; now < 11000; now += 10) {
                    // Repeated inventory updates invalidate player-state previews as in live play.
                    insert(*current, 42, 2);
                    current->tick(now, send);
                }
            }
            player->tick(10000, send);
        });
        const std::string diagnostic = "quest=70 flag=1: unsupported action unsupported:html";
        const auto first = warnings.find(diagnostic);
        expect(first != std::string::npos &&
                   warnings.find(diagnostic, first + 1) == std::string::npos,
               "unsupported quest diagnostics identify the step once across repeated previews and "
               "players");
        expect(hunted && !questSent,
               "unsupported raw actions stay blocked while ordinary hunting remains available");
        protocol::CreateNpcInfo npc;
        npc.id = 801;
        npc.index = 2;
        npc.x = npc.y = 1024;
        npc.z = 10000;
        deliver(*player, protocol::Quest::frame_create_npc(npc));
        deliver(*player, protocol::Quest::frame_save_quest({70, 1, 0}));
        player->tick(12000, send);
        expect(questSent, "a server quest-state update invalidates an earlier unavailable preview");
        for (const auto* file :
             {"npcs/2.json", "dialogs/200.json", "quests/70_1.json", "quests/71_1.json"}) {
            std::filesystem::remove(fixture.root + "/" + file);
        }
        fixture.data = fixture.load_data();
    }
    void quest_priorities(Fixture& fixture) {
        text(fixture.root + "/npcs/2.json",
             R"({"index":2,"quest":[70, 1],"xy":[1120, 1024, 10000]})");
        text(fixture.root + "/quests/70_1.json",
             R"({"index":70,"flag":1,"cases":[{"then":[{"op":"html","id":200}]}]})");
        text(
            fixture.root + "/quests/70_2.json",
            R"({"index":70,"flag":2,"cases":[{"then":[{"op":"teleport","x":1200,"y":1200,"z":10000},{"op":"gstate","gstate":5,"value":1}]}]})");
        text(fixture.root + "/dialogs/200.json",
             R"({"id":200,"links":[{"type":"quest","text":"Owned service","quest":[70, 2]}]})");
        server::QuestDb services;
        expect(services.load_dir(fixture.root + "/quests"), "load valid owned service scripts");
        server::PlayerQuestMap progress;
        server::QuestActionSink sink;
        int effects = 0;
        sink.teleport = [&](int, int, int) { ++effects; };
        sink.set_gstate = [&](int, bool) { ++effects; };
        expect(server::QuestEngine(services).call_process(
                   protocol::Quest::make_key(70, 2), progress, sink) &&
                   effects == 2,
               "service script is valid ordinary gameplay before autonomy filters it");
        fixture.data = fixture.load_data();
        auto player = fixture.player();
        monster(*player);
        bool attacked = false, service = false;
        player->tick(10000, [&](const auto& bytes) {
            const auto packet = unpack(bytes);
            attacked |= packet.type == static_cast<std::uint8_t>(protocol::C2S::Attack);
            service |= packet.type == static_cast<std::uint8_t>(protocol::C2S::AskNpc) ||
                       packet.type == static_cast<std::uint8_t>(protocol::C2S::Quest) ||
                       packet.type == static_cast<std::uint8_t>(protocol::C2S::MoveOn);
            return true;
        });
        expect(attacked && !service,
               "travel and activity-toggle service links do not become autonomous quest targets");
        text(fixture.root + "/quests/70_2.json",
             R"({"index":70,"flag":2,"cases":[{"then":[{"op":"save","quest":70,"flag":2}]}]})");
        text(fixture.root + "/tasks/70.json",
             R"({"key":70,"flags":[{"key":1,"npcIndex":2,"level":1}]})");
        text(fixture.root + "/npcs/3.json",
             R"({"index":3,"quest":[71, 1],"xy":[1024, 1024, 10000]})");
        text(fixture.root + "/quests/71_1.json",
             R"({"index":71,"flag":1,"cases":[{"then":[{"op":"save","quest":71,"flag":1}]}]})");
        fixture.data = fixture.load_data();
        player = fixture.player();
        deliver(*player, protocol::Quest::frame_save_quest({70, 1, 0}));
        bool moved = false;
        for (std::uint64_t now = 10000; now < 11000 && !moved; now += 10) {
            player->tick(now, [&](const auto& bytes) {
                moved |= unpack(bytes).type == static_cast<std::uint8_t>(protocol::C2S::MoveOn);
                return true;
            });
        }
        expect(moved && player->status().find("NPC 2") != std::string::npos,
               "an accepted journal objective takes priority over a closer unrelated new quest");
        for (const auto* file : {"npcs/2.json",
                                 "npcs/3.json",
                                 "quests/70_1.json",
                                 "quests/70_2.json",
                                 "quests/71_1.json",
                                 "dialogs/200.json",
                                 "tasks/70.json"}) {
            std::filesystem::remove(fixture.root + "/" + file);
        }
        fixture.data = fixture.load_data();
    }
    void city_travel(Fixture& fixture) {
        text(fixture.root + "/dialogs/200.json",
             R"({"id":200,"links":[{"type":"quest","text":"Owned city task","quest":[20, 1]}]})");
        text(fixture.root + "/quests/20_1.json",
             R"({"index":20,"flag":1,"cases":[{"then":[{"op":"save","quest":20,"flag":1}]}]})");
        for (std::size_t index = 0; index < map::kTeleportLocCount; ++index) {
            const auto& city = map::kTeleportLocs[index];
            text(fixture.root + "/npcs/2.json",
                 ("{\"index\":2,\"html\":200,\"xy\":[" + std::to_string(city.x + 16) + ", " +
                  std::to_string(city.y) + ", " + std::to_string(city.z) + "]}")
                     .c_str());
            fixture.data = fixture.load_data();
            auto player = fixture.player();
            std::vector<Packet> sent;
            int requests = 0;
            bool correctCity = true;
            const auto travel = [&](std::size_t requested) {
                ++requests;
                correctCity &= requested == index;
                return true;
            };
            const auto tick = [&](std::uint64_t now) {
                sent.clear();
                player->tick(
                    now,
                    [&](const auto& bytes) {
                        sent.push_back(unpack(bytes));
                        return true;
                    },
                    std::chrono::milliseconds{2},
                    travel);
            };
            tick(10000);
            expect(requests == 1 && correctCity && player->pending_city_travel() == index &&
                       player->position().x == 1024 && sent.empty(),
                   "city travel requests the regular city destination without inventory or "
                   "speculative movement");
            tick(15000);
            expect(requests == 1 && sent.empty(),
                   "city travel is queued once while awaiting the server");
            deliver(*player, protocol::Teleport::frame_server(city.map, city.x, city.y, city.z, 0));
            tick(15010);
            expect(player->pending_city_travel() == index && sent.empty(),
                   "city arrival waits for the authoritative teleport acknowledgement");
            deliver(*player, protocol::Move::frame_before(77, city.x, city.y, city.z, 0));
            protocol::CreateNpcInfo npc;
            npc.id = 801;
            npc.index = 2;
            npc.x = city.x + 16;
            npc.y = city.y;
            npc.z = city.z;
            deliver(*player, protocol::Quest::frame_create_npc(npc));
            tick(15020);
            expect(!player->pending_city_travel() && requests == 1 &&
                       std::any_of(sent.begin(),
                                   sent.end(),
                                   [](const auto& packet) {
                                       return packet.type ==
                                              static_cast<std::uint8_t>(protocol::C2S::AskNpc);
                                   }),
                   "confirmed city arrival resumes the ordinary nearby NPC interaction without "
                   "another teleport");
            deliver(*player, protocol::Move::frame_before(77, 1024, 1024, 10000, 0));
            tick(30000);
            expect(requests == 2 && correctCity && player->pending_city_travel() == index,
                   "city travel can be repeated without granting or consuming scroll items");
            player->pause();
            expect(!player->pending_city_travel(), "pausing cancels a queued city travel request");
        }
        auto timeout = fixture.player();
        const auto send = [](const auto&) { return true; };
        const auto accept = [](std::size_t) { return true; };
        timeout->tick(10000, send, std::chrono::milliseconds{2}, accept);
        timeout->tick(26000, send, std::chrono::milliseconds{2}, accept);
        expect(!timeout->active() && !timeout->pending_city_travel() &&
                   timeout->status() == "Paused: city teleport confirmation timed out",
               "an unconfirmed city teleport pauses instead of looping or starting a world route");
        auto rejected = fixture.player();
        rejected->tick(
            10000, send, std::chrono::milliseconds{2}, [](std::size_t) { return false; });
        expect(!rejected->active() && !rejected->pending_city_travel(),
               "a rejected city request pauses without speculative position changes");
        auto healing = fixture.player();
        healing->tick(10000, send, std::chrono::milliseconds{2}, accept);
        insert(*healing, 42, 2);
        deliver(*healing,
                protocol::Property::frame_update_vital(
                    static_cast<std::uint8_t>(protocol::PropertyType::Hp), 20, 100));
        bool healed = false;
        healing->tick(
            10100,
            [&](const auto& bytes) {
                healed |= unpack(bytes).type == static_cast<std::uint8_t>(protocol::C2S::UseItem);
                return true;
            },
            std::chrono::milliseconds{2},
            accept);
        expect(healed && healing->pending_city_travel(),
               "health potion use keeps priority while city confirmation is pending");
        for (const auto* file : {"npcs/2.json", "dialogs/200.json", "quests/20_1.json"}) {
            std::filesystem::remove(fixture.root + "/" + file);
        }
        fixture.data = fixture.load_data();
    }

    void npc_city_fallback(Fixture& fixture) {
        const auto& city = map::kTeleportLocs[0];
        text(
            fixture.root + "/dialogs/200.json",
            R"({"id":200,"links":[{"type":"quest","text":"Owned blocked task","quest":[20, 1]}]})");
        text(fixture.root + "/quests/20_1.json",
             R"({"index":20,"flag":1,"cases":[{"then":[{"op":"save","quest":20,"flag":1}]}]})");
        for (const int offset : {16, 96}) {
            text(fixture.root + "/npcs/2.json",
                 ("{\"index\":2,\"html\":200,\"xy\":[" + std::to_string(city.x + offset) + ", " +
                  std::to_string(city.y) + ", " + std::to_string(city.z) + "]}")
                     .c_str());
            fixture.data = fixture.load_data();
            auto player = fixture.player(true, false);
            deliver(*player, protocol::Move::frame_before(77, city.x + 128, city.y, city.z, 0));
            int requests = 0;
            bool correctCity = true;
            bool asked = false;
            const auto tick = [&](std::uint64_t now) {
                player->tick(
                    now,
                    [&](const auto& bytes) {
                        asked |=
                            unpack(bytes).type == static_cast<std::uint8_t>(protocol::C2S::AskNpc);
                        return true;
                    },
                    std::chrono::milliseconds{2},
                    [&](std::size_t index) {
                        ++requests;
                        correctCity &= index == 0;
                        return true;
                    });
            };
            tick(10000);
            expect(requests == 0, "short NPC trips first try ordinary local navigation");
            std::uint64_t now = 10100;
            for (; now < 17000 && !requests; now += 100) {
                tick(now);
            }
            expect(requests == 1 && correctCity && player->pending_city_travel() == 0 &&
                       player->position().x == city.x + 128,
                   "failed local NPC navigation requests its nearest city without optimistic "
                   "movement");
            tick(now + 100);
            expect(requests == 1 && !asked,
                   "NPC city recovery waits for confirmation without duplicate requests");
            deliver(*player, protocol::Teleport::frame_server(0, city.x, city.y, city.z, 0));
            deliver(*player, protocol::Move::frame_before(77, city.x, city.y, city.z, 0));
            protocol::CreateNpcInfo npc;
            npc.id = 801;
            npc.index = 2;
            npc.x = city.x + offset;
            npc.y = city.y;
            npc.z = city.z;
            deliver(*player, protocol::Quest::frame_create_npc(npc));
            for (now += 200; now < 30000; now += 100) {
                tick(now);
            }
            expect(requests == 1 && asked == (offset == 16),
                   "confirmed recovery talks to a reachable NPC and never loops at an unreachable "
                   "NPC");
            if (offset == 96) {
                deliver(*player, protocol::Move::frame_before(77, city.x + 64, city.y, city.z, 0));
                for (; now < 38000; now += 100) {
                    tick(now);
                }
                expect(requests == 1,
                       "the same failed NPC cannot repeatedly force a city recovery after "
                       "displacement");
            }
        }
        text(fixture.root + "/shops/2.json",
             R"({"schema":"openkal.shop/v1","npcIndex":2,"items":[1, 2]})");
        fixture.data = fixture.load_data();
        auto merchant = fixture.player();
        for (std::uint32_t id = 100; id < 160; ++id) {
            insert(*merchant, id, 1);
        }
        deliver(*merchant, protocol::Move::frame_before(77, city.x + 256, city.y, city.z, 0));
        int requests = 0;
        for (std::uint64_t now = 10000; now < 17000 && !requests; now += 100) {
            merchant->tick(
                now,
                [](const auto&) { return true; },
                std::chrono::milliseconds{2},
                [&](std::size_t index) {
                    requests += index == 0;
                    return true;
                });
        }
        expect(requests == 1 && merchant->pending_city_travel() == 0,
               "an unreachable merchant also uses emergency city recovery");
        for (const auto* file :
             {"npcs/2.json", "dialogs/200.json", "quests/20_1.json", "shops/2.json"}) {
            std::filesystem::remove(fixture.root + "/" + file);
        }
        fixture.data = fixture.load_data();
    }

    void distant_quest(Fixture& fixture) {
        text(fixture.root + "/npcs/2.json", R"({"index":2,"html":200,"xy":[5000, 3000, 10000]})");
        text(
            fixture.root + "/dialogs/200.json",
            R"({"id":200,"links":[{"type":"quest","text":"Continue owned task","quest":[20, 1]}]})");
        text(
            fixture.root + "/quests/20_1.json",
            R"({"index":20,"flag":1,"cases":[{"if":[{"op":"quest","quest":20,"flag":0},{"op":"notclear","quest":20}],"then":[{"op":"save","quest":20,"flag":1}]}]})");
        fixture.data = fixture.load_data();
        auto player = fixture.player();
        bool moved = false;
        for (std::uint64_t time = 10000; time < 19000 && !moved; time += 10) {
            player->tick(time, [&](const auto& bytes) {
                const auto packet = unpack(bytes);
                moved |= packet.type == static_cast<std::uint8_t>(protocol::C2S::MoveOn);
                return true;
            });
        }
        expect(moved,
               ("a completed initial quest is followed by movement toward a distant quest NPC: " +
                player->status())
                   .c_str());
        text(fixture.root + "/npcs/2.json", R"({"index":2,"html":200,"xy":[17000, 9000, 10000]})");
        fixture.data = fixture.load_data();
        auto stalled = fixture.player();
        const auto discard = [](const auto&) { return true; };
        stalled->tick(10000, discard);
        const auto firstStatus = stalled->status();
        for (std::uint64_t time = 10010; time < 10100; time += 10) {
            deliver(*stalled, protocol::Move::frame_player_end(77, 0, 0, 0));
            deliver(*stalled, protocol::Move::frame_before(77, 1024, 1024, 10000, 0));
            stalled->tick(time, discard);
        }
        expect(firstStatus.find("Planning route: ") == 0 && stalled->status() != firstStatus,
               "redundant position confirmations preserve incremental route progress");
        bool prematureMove = false;
        for (std::uint64_t time = 10100; time < 15000; time += 10) {
            stalled->tick(time, [&](const auto& bytes) {
                const auto packet = unpack(bytes);
                if (packet.type == static_cast<std::uint8_t>(protocol::C2S::MoveOn) ||
                    packet.type == static_cast<std::uint8_t>(protocol::C2S::MoveEnd)) {
                    std::int8_t dx = 0, dy = 0, dz = 0;
                    if (protocol::Move::parse_client(
                            packet.payload.data(), packet.payload.size(), dx, dy, dz)) {
                        prematureMove |= dx != 0 || dy != 0;
                    }
                }
                return true;
            });
        }
        expect(!prematureMove && stalled->position().x == 1024 &&
                   stalled->status().find("Planning route: ") == 0,
               "an explored prefix toward an unavailable destination never strands the player in a "
               "dead end");
        stalled->tick(100001, discard);
        expect(stalled->status().find("Route planning timed out;") == 0,
               "a quest route has a bounded planning deadline");
        monster(*stalled);
        bool attacked = false, retried = false;
        for (std::uint64_t time = 102000; time < 160000; time += 1000) {
            deliver(*stalled, protocol::Quest::frame_save_quest({10, 1, 1}));
            stalled->tick(time, [&](const auto& bytes) {
                attacked |= unpack(bytes).type == static_cast<std::uint8_t>(protocol::C2S::Attack);
                return true;
            });
            retried |= stalled->status().find("Planning route: ") == 0;
        }
        expect(attacked && !retried,
               "failed quest searches yield to hunting and do not repeat at an unchanged origin");
        stalled->start(fixture.data);
        stalled->tick(170000, discard);
        expect(stalled->status().find("Planning route: ") == 0,
               "explicit restart permits retrying failed routes after runtime data changes");
        stalled->tick(141001, discard);
        deliver(*stalled, protocol::Move::frame_before(77, 1100, 1024, 10000, 0));
        stalled->tick(142000, discard);
        expect(stalled->status().find("Planning route: ") == 0,
               "a meaningful authoritative displacement permits a fresh route attempt");
        for (const auto* file : {"npcs/2.json", "dialogs/200.json", "quests/20_1.json"}) {
            std::filesystem::remove(fixture.root + "/" + file);
        }
        fixture.data = fixture.load_data();
    }
    void navigation() {
        const TestPlayerStep wall = [](Position from, int x, int y) -> std::optional<int> {
            if (x < 8 || y < 8 || x > 120 || y > 120) {
                return {};
            }
            for (int i = 0; i <= 16; ++i) {
                const int sx = from.x + (x - from.x) * i / 16;
                const int sy = from.y + (y - from.y) * i / 16;
                if (sx >= 48 && sx <= 56 && sy < 80) {
                    return {};
                }
            }
            return 0;
        };
        const auto route = test_player_route({16, 16, 0}, {96, 16, 0}, 0, wall);
        expect(!route.empty() && route.back().x == 96 && route.back().y == 16,
               "A* reaches the target around a wall rather than walking into it");
        Position previous{16, 16, 0};
        bool detour = false;
        for (const auto waypoint : route) {
            expect(wall(previous, waypoint.x, waypoint.y).has_value(),
                   "every route segment is traversable");
            detour |= waypoint.y >= 80;
            previous = waypoint;
        }
        expect(detour, "route uses the opening at the end of the fence");
        const auto corner = [](Position, int x, int y) -> std::optional<int> {
            return (x == 16 && y == 16) || (x == 24 && y == 24) ? std::optional<int>{0}
                                                                : std::nullopt;
        };
        expect(test_player_route({16, 16, 0}, {24, 24, 0}, 0, corner).empty(),
               "diagonal corner cutting is rejected, including exact-goal shortcuts");
        expect(test_player_route({16, 16, 0}, {96, 16, 0}, 0, wall, 1).empty(),
               "exhausted search budgets never produce a partial movement route");
        TestPlayerRouteSearch sliced({16, 16, 0}, {96, 16, 0}, 0, wall);
        expect(sliced.advance(1) == TestPlayerRouteState::Searching,
               "route planning yields to network and survival work");
        for (int i = 0; i < 500 && sliced.advance(1) == TestPlayerRouteState::Searching; ++i) {
        }
        expect(!sliced.route().empty(), "incremental route planning retains search progress");
        const std::vector<Position> stair = {
            {16, 16, 0}, {24, 16, 0}, {24, 24, 10}, {16, 24, 10}, {16, 16, 10}, {8, 16, 10}};
        const auto floors = [&](Position from, int x, int y) -> std::optional<int> {
            for (std::size_t i = 0; i + 1 < stair.size(); ++i) {
                if (from.x == stair[i].x && from.y == stair[i].y && from.z == stair[i].z &&
                    x == stair[i + 1].x && y == stair[i + 1].y) {
                    return stair[i + 1].z;
                }
            }
            return {};
        };
        const auto layered = test_player_route(stair.front(), stair.back(), 0, floors);
        expect(!layered.empty() && layered.back().z == 10,
               "navigation can revisit the same horizontal cell on a different floor");
    }
    void checked_navigation_edges() {
        const TestPlayerStep bounded = [](Position, int x, int y) -> std::optional<int> {
            return x >= 0 && x < 2048 && y >= 0 && y < 128 ? std::optional<int>{0} : std::nullopt;
        };
        TestPlayerRouteSearch pending({16, 16, 0}, {8016, 16, 0}, 16, bounded, 4096, 32);
        expect(pending.advance(128) == TestPlayerRouteState::Searching,
               "a long route remains pending beyond the explored region");
        expect(pending.route().empty(),
               "an explored dead-end approach cannot become a movement route before reaching the "
               "goal");
        TestPlayerRouteSearch open(
            {16, 16, 0},
            {8016, 16, 0},
            16,
            [](Position, int, int) -> std::optional<int> { return 0; },
            4096,
            32);
        expect(open.advance(128) == TestPlayerRouteState::Found && open.expanded_nodes() < 64,
               "long open routes use fully validated longer edges instead of flooding short cells");
        int calls = 0;
        std::set<std::pair<int, int>> destinations;
        TestPlayerRouteSearch route({1024, 1024, 0},
                                    {1800, 1800, 0},
                                    16,
                                    [&](Position, int x, int y) -> std::optional<int> {
                                        ++calls;
                                        destinations.emplace(x, y);
                                        return 0;
                                    });
        expect(route.advance(1) == TestPlayerRouteState::Searching && calls == 8 &&
                   destinations.size() == 8,
               "cardinal clearance checks and neighbor expansion share each validated edge");
        const TestPlayerStep riverbank = [](Position from, int x, int y) -> std::optional<int> {
            const int samples = std::max(std::abs(x - from.x), std::abs(y - from.y));
            for (int i = 0; i <= samples; ++i) {
                const int px = from.x + (x - from.x) * i / std::max(1, samples);
                const int py = from.y + (y - from.y) * i / std::max(1, samples);
                if (px < 1000 || px > 3400 || py < 1000 || py > 3000 ||
                    (px >= 1792 && px < 2048 && (py < 1536 || py > 1664))) {
                    return {};
                }
            }
            return 0;
        };
        TestPlayerRouteSearch crossing({1700, 2100, 0}, {3000, 2600, 0}, 16, riverbank, 1000, 32);
        expect(crossing.advance(1000) == TestPlayerRouteState::Found && !crossing.route().empty(),
               "riverbank detour finds the bridge within a bounded planning budget");
    }

    void narrow_navigation() {
        const auto inside = [](int x, int y) {
            return (x >= 16 && x <= 136 && y >= 6 && y <= 10) ||
                   (x >= 134 && x <= 138 && y >= 8 && y <= 40) ||
                   (x >= 136 && x <= 240 && y >= 38 && y <= 42);
        };
        const TestPlayerStep corridor = [&](Position from, int x, int y) -> std::optional<int> {
            const int samples =
                std::max(1, static_cast<int>(std::ceil(std::hypot(x - from.x, y - from.y) / 2)));
            for (int i = 0; i <= samples; ++i) {
                if (!inside(from.x + (x - from.x) * i / samples,
                            from.y + (y - from.y) * i / samples)) {
                    return {};
                }
            }
            return 0;
        };
        TestPlayerRouteSearch coarse({16, 8, 0}, {240, 40, 0}, 0, corridor, 4096, 32);
        expect(coarse.advance(4096) == TestPlayerRouteState::Found && !coarse.route().empty() &&
                   coarse.route().back().x == 240 && coarse.route().back().y == 40,
               "coarse navigation locally refines a narrow off-grid corridor without restarting");
        const auto fine = test_player_route({16, 8, 0}, {240, 40, 0}, 0, corridor);
        expect(!fine.empty() && fine.back().x == 240 && fine.back().y == 40,
               "fine navigation retains the narrow route missed by the coarse grid");
        const TestPlayerStep bridge = [](Position from, int x, int y) -> std::optional<int> {
            const int samples = std::max(std::abs(x - from.x), std::abs(y - from.y));
            for (int i = 0; i <= samples; ++i) {
                const int px = from.x + (x - from.x) * i / std::max(1, samples);
                const int py = from.y + (y - from.y) * i / std::max(1, samples);
                if (px < 960 || px > 1440 || py < 200 || py > 1100 ||
                    (px >= 1120 && px < 1216 && (py < 262 || py > 266))) {
                    return {};
                }
            }
            return 0;
        };
        TestPlayerRouteSearch approach({1024, 1024, 0}, {1400, 1024, 0}, 0, bridge, 8192, 32);
        expect(approach.advance(8192) == TestPlayerRouteState::Found && !approach.route().empty(),
               "coarse riverbank routing retains a narrow off-grid bridge entrance sixteen pixels "
               "away");
    }

    void distant_navigation() {
        const TestPlayerStep open = [](Position, int, int) -> std::optional<int> { return 0; };
        TestPlayerRouteSearch route({1024, 1024, 0}, {17024, 9024, 0}, 32, open, 8192);
        std::size_t previous = 0;
        bool boundedProgress = true;
        TestPlayerRouteState state = TestPlayerRouteState::Searching;
        for (int slice = 0; slice < 256 && state == TestPlayerRouteState::Searching; ++slice) {
            state = route.advance(32);
            boundedProgress &=
                route.expanded_nodes() > previous && route.expanded_nodes() - previous <= 32;
            previous = route.expanded_nodes();
        }
        expect(boundedProgress,
               "distant route retains measurable progress across bounded planning slices");
        expect(state == TestPlayerRouteState::Found && !route.route().empty() &&
                   route.expanded_nodes() < 8192,
               "a distant diagonal quest destination is found without exploring the whole field");
    }
    void input() {
        engine::FrameInput frame;
        frame.key_w = frame.key_digit_pressed[1] = frame.mouse_left_clicked =
            frame.mouse_right_clicked = true;
        frame.key_i_pressed = true;
        frame.quit = true;
        frame.width = 900;
        frame.mouse_x = 123;
        client::restrict_read_only_input(frame);
        expect(!frame.key_w && !frame.key_digit_pressed[1] && !frame.mouse_left_clicked &&
                   !frame.mouse_right_clicked,
               "spectator input cannot move, cast or click game actions");
        expect(frame.quit && frame.key_i_pressed && frame.width == 900 && frame.mouse_x == 123,
               "spectator input retains close, inspection and hover");
    }

    void live_player(Fixture& fixture) {
        text(
            fixture.root + "/skills/behead.json",
            R"({"class":0,"index":1,"type":["use"],"level":1,"mp":0,"maxLevel":1,"delay":[200, 500]})");
        // Owned delivery fixture: currency has no inventory item definition.
        text(fixture.root + "/items/9.json",
             R"({"index":9,"name":"Owned delivery parcel","class":["quest","etc"],"stackable":0})");
        text(
            fixture.root + "/items/23000.json",
            R"({"index":23000,"name":"Owned migrated reward","class":["general","etc"],"stackable":1})");
        text(
            fixture.root + "/quests/10_1.json",
            R"({"index":10,"flag":1,"cases":[{"if":[{"op":"quest","quest":10,"flag":0},{"op":"notclear","quest":10}],"then":[{"op":"item","out":[[9, 1]],"in":[[31, 0, 137], [47, 0, 2]]},{"op":"clear","quest":10,"flag":1}]}]})");
        text(fixture.root + "/npcs/1.json", R"({"index":1,"html":100,"xy":[1088, 1024, 10000]})");
        map::AreaCell movement;
        const auto area = fixture.root + "/maps/n_000_000.akarea";
        expect(map::AreaMapCodec::load(area, movement), "read owned live NPC approach area");
        std::vector<map::AreaCell::SubcellMask> masks(
            static_cast<std::size_t>(map::kTileCount) * map::kTileCount, 0);
        for (int y = 31; y <= 33; ++y) {
            for (int x = 33; x <= 35; ++x) {
                masks[static_cast<std::size_t>(y) * map::kTileCount + x] = 0xffff;
            }
        }
        movement.set_not_moveable_masks(std::move(masks));
        expect(map::AreaMapCodec::save(area, movement), "block close approach on the real server");
        fixture.data = fixture.load_data();
        auto initial = fixture.character;
        initial.id = 1;
        initial.name = "OwnedAgent";
        initial.level = 1;
        initial.map_z = 11000;
        initial.weapon_index = 3;
        initial.next_iid = 4;
        initial.pu_point = 20;
        auto store = server::create_data_store(server::StoreBackend::Sqlite);
        expect(store && store->open(fixture.root + "/live-save") &&
                   store->ensure_default_account("OwnedAgentAccount", "OwnedAgentPassword"),
               "prepare an ordinary account in an isolated SQLite fixture");
        if (!store) {
            return;
        }
        const auto account = store->find_account("OwnedAgentAccount");
        if (!account) {
            return;
        }
        initial.account_id = account->id;
        expect(store->save_character(initial), "save the fixture's initial character");
        server::ItemRecord sword;
        sword.iid = 1;
        sword.character_id = initial.id;
        sword.index = 3;
        sword.count = 1;
        sword.slot = 1;
        server::ItemRecord potion = sword;
        potion.iid = 2;
        potion.index = 2;
        potion.count = 5;
        potion.slot = 0;
        server::ItemRecord parcel = potion;
        parcel.iid = 3;
        parcel.index = 9;
        parcel.count = 1;
        expect(store->insert_item(sword) && store->insert_item(potion) &&
                   store->insert_item(parcel),
               "prepare ordinary starting equipment, consumables and a delivery item");
        store->close();
        server::ServerConfig config;
        config.maps_dir = fixture.root + "/maps";
        config.items_dir = fixture.root + "/items";
        config.monsters_dir = fixture.root + "/monsters";
        config.quests_dir = fixture.root + "/quests";
        config.npcs_dir = fixture.root + "/npcs";
        config.skills_dir = fixture.root + "/skills";
        config.save_dir = fixture.root + "/live-save";
        config.unix_path = fixture.root + "/game.sock";
        config.character_id = initial.id;
        config.spawn_x = config.spawn_y = 1024;
        config.spawn_z = 9000;
        config.max_monsters = 1;
        server::ServerHost host;
        expect(host.start(config), "start an ordinary server with test control disabled");
        std::unique_ptr<server::ISession> connection;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(40);
        while (!connection && std::chrono::steady_clock::now() < deadline) {
            connection = server::connect_unix(config.unix_path);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        expect(connection != nullptr, "connect the headless agent to the ordinary local server");
        if (!connection) {
            host.stop();
            return;
        }
        TestPlayerAgent agent(initial);
        agent.start(fixture.data);
        expect(connection->send_all(protocol::Handshake::build_connect(0)),
               "send the normal world handshake");
        std::vector<std::uint8_t> rx;
        bool advancedQuest = false, moved = false, grounded = false, gainedExperience = false;
        bool contextualDialog = false, beheaded = false, allocated = false;
        bool dropRequested = false, pickupSent = false, lootCollected = false;
        std::uint32_t droppedId = 0;
        while (std::chrono::steady_clock::now() < deadline &&
               !(gainedExperience && lootCollected && beheaded && allocated) &&
               connection->alive()) {
            if (!connection->recv_into(rx)) {
                break;
            }
            for (;;) {
                Packet packet;
                std::size_t consumed = 0;
                const auto parsed = protocol::FrameParser::pop(
                    rx.data(), rx.size(), consumed, packet.type, packet.payload);
                if (parsed != protocol::FrameParseStatus::Complete) {
                    break;
                }
                rx.erase(rx.begin(), rx.begin() + static_cast<std::ptrdiff_t>(consumed));
                if (packet.type == static_cast<std::uint8_t>(protocol::S2C::Code)) {
                    protocol::S2CCodeFields code;
                    if (protocol::Handshake::parse_code(
                            packet.payload.data(), packet.payload.size(), code)) {
                        (void) connection->send_all(
                            protocol::Handshake::build_answer(code.protocol_test_value));
                    }
                }
                if (packet.type == static_cast<std::uint8_t>(protocol::S2C::SaveQuest)) {
                    protocol::QuestFlagEntry entry;
                    if (protocol::Quest::parse_save_quest(
                            packet.payload.data(), packet.payload.size(), entry)) {
                        advancedQuest |= entry.quest == 10 && entry.flag == 1 && entry.clear != 0;
                    }
                    if (advancedQuest && !dropRequested) {
                        dropRequested = connection->send_all(protocol::DropItem::frame_c2s(2, 1));
                    }
                }
                if (packet.type == static_cast<std::uint8_t>(protocol::S2C::CreateItem)) {
                    protocol::PacketReader reader(packet.payload.data(), packet.payload.size());
                    std::uint16_t index = 0;
                    std::uint32_t id = 0;
                    if (reader.read_u16(index) && reader.read_u32(id) && index == 2) {
                        droppedId = id;
                    }
                }
                if (packet.type == static_cast<std::uint8_t>(protocol::S2C::RemoveItem)) {
                    protocol::PacketReader reader(packet.payload.data(), packet.payload.size());
                    std::uint32_t id = 0;
                    if (reader.read_u32(id) && droppedId && id == droppedId && pickupSent) {
                        lootCollected = true;
                    }
                }
                if (packet.type == static_cast<std::uint8_t>(protocol::S2C::SendHtml)) {
                    std::int32_t html = 0;
                    std::uint32_t npc = 0;
                    contextualDialog |=
                        protocol::Quest::parse_send_html(
                            packet.payload.data(), packet.payload.size(), html, &npc) &&
                        html == 100 && npc != 0;
                }
                moved |= packet.type == static_cast<std::uint8_t>(protocol::S2C::MovePlayerOn);
                if (packet.type == static_cast<std::uint8_t>(protocol::S2C::Action)) {
                    std::uint32_t id = 0;
                    std::uint8_t action = 0, value = 0;
                    if (protocol::Skill::parse_action(
                            packet.payload.data(), packet.payload.size(), id, action, value)) {
                        beheaded |= action == protocol::kAtBeheaded;
                    }
                }
                if (packet.type == static_cast<std::uint8_t>(protocol::S2C::UpdateProperty)) {
                    protocol::UpdatePropertyFields fields;
                    if (protocol::Property::parse_update_any(
                            packet.payload.data(), packet.payload.size(), fields)) {
                        gainedExperience |= fields.kind == protocol::UpdatePropertyKind::Exp &&
                                            fields.exp_delta > 0;
                    }
                }
                agent.receive(packet.type, packet.payload);
                grounded |= agent.position().z == 10000;
            }
            const auto now =
                static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                               std::chrono::steady_clock::now().time_since_epoch())
                                               .count());
            agent.tick(now, [&](const auto& bytes) {
                pickupSent |=
                    unpack(bytes).type == static_cast<std::uint8_t>(protocol::C2S::PickupItem);
                allocated |=
                    unpack(bytes).type == static_cast<std::uint8_t>(protocol::C2S::UpdateProperty);
                return connection->send_all(bytes);
            });
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        expect(advancedQuest,
               "ordinary server completes the agent's delivery with a currency and item reward");
        expect(contextualDialog, "ordinary server identifies the NPC in quest-dialog replies");
        expect(grounded,
               "ordinary server confirms standing-height correction without test control");
        expect(moved, "ordinary server confirms the agent's planned movement");
        if (!gainedExperience) {
            std::fprintf(stderr,
                         "Live agent stalled: %s at %d,%d,%d\n",
                         agent.status().c_str(),
                         agent.position().x,
                         agent.position().y,
                         agent.position().z);
        }
        expect(gainedExperience,
               "ordinary server awards experience for the agent's monster kill without test "
               "privileges");
        expect(dropRequested && droppedId && pickupSent && lootCollected,
               "ordinary server confirms automatic ground-item pickup without test privileges");
        expect(beheaded, "ordinary server confirms automatic beheading of a kneeling Maguni");
        connection->close();
        host.stop();
        expect(store->open(config.save_dir), "inspect delivery reward persistence");
        const auto saved = store->load_character(initial.id);
        const auto base = server::combat::class_base_plus_bonus(initial.clazz);
        expect(allocated && saved && saved->str > base.str && saved->hth > base.hth &&
                   saved->dex > base.dex && saved->pu_point < initial.pu_point,
               "ordinary server confirms and persists the requested stat distribution");
        const auto items = store->load_items(initial.id);
        expect(saved && saved->money == initial.money + 137,
               "quest currency is persisted without a corresponding inventory definition");
        expect(std::any_of(items.begin(),
                           items.end(),
                           [](const auto& item) { return item.index == 23000 && item.count == 2; }),
               "a legacy quest reward grants and persists the current medicine definition");
        expect(std::none_of(items.begin(),
                            items.end(),
                            [](const auto& item) { return item.index == 9 || item.index == 31; }),
               "delivery consumes the parcel and never creates a currency inventory row");
        store->close();
    }
}  // namespace

int main() {
    unsetenv("OPENKAL_PROFILE");
    navigation_time_budget();
    navigation();
    distant_navigation();
    narrow_navigation();
    checked_navigation_edges();
    input();
    Fixture fixture;
    stat_allocation(fixture);
    beheading(fixture);
    level_appropriate_hunting(fixture);
    distributed_hunting(fixture);
    selling_in_narootuh(fixture);
    loot_priorities(fixture);
    movement_timing(fixture);
    boost(fixture);
    portal_routing(fixture);
    quest_priorities(fixture);
    stalled_movement(fixture);
    planning_timeout(fixture);
    dialog_lifetime(fixture);
    unseen_npc_approach(fixture);
    unseen_npc_approach(fixture, true);
    blocked_npc_approach(fixture);
    spawn_approach(fixture);
    area_travel(fixture);
    standing_height(fixture);
    outdoor_bridge(fixture);
    neighboring_bridge(fixture);
    water_bridge_step(fixture);
    bridge_approach(fixture);
    shared_navigation_layers(fixture);
    city_travel(fixture);
    npc_city_fallback(fixture);
    thin_obstacle(fixture);
    std::vector<Packet> sent;
    const auto send = [&](const auto& bytes) {
        sent.push_back(unpack(bytes));
        return true;
    };
    auto tick = [&](TestPlayerAgent& player, std::uint64_t now) {
        sent.clear();
        player.tick(now, send);
    };
    {
        auto player = fixture.player(true, false);
        monster(*player);
        tick(*player, 10000);
        expect(sent.empty() && player->status() == "Waiting: no weapon equipped",
               "an unarmed character does not send attacks rejected by normal server rules");
        insert(*player, 10, 1);
        tick(*player, 11000);
        expect(sent.size() == 1 &&
                   sent.front().type == static_cast<std::uint8_t>(protocol::C2S::PutOnItem),
               "an available weapon is equipped before attempting to hunt");
    }
    {
        auto player = fixture.player();
        insert(*player, 1, 2, 3);
        monster(*player);
        deliver(*player,
                protocol::Property::frame_update_vital(
                    static_cast<std::uint8_t>(protocol::PropertyType::Hp), 20, 100));
        tick(*player, 10000);
        expect(!sent.empty() &&
                   sent.front().type == static_cast<std::uint8_t>(protocol::C2S::UseItem),
               "survival consumes a potion before combat, quests or equipment");
        tick(*player, 10100);
        expect(
            std::none_of(sent.begin(),
                         sent.end(),
                         [](const auto& p) {
                             return p.type == static_cast<std::uint8_t>(protocol::C2S::UseItem);
                         }),
            "shared potion cooldown prevents repeated consumption while awaiting server updates");
        player->pause();
        tick(*player, 20000);
        expect(std::none_of(sent.begin(),
                            sent.end(),
                            [](const auto& p) {
                                return p.type == static_cast<std::uint8_t>(protocol::C2S::Attack);
                            }),
               "pausing stops autonomous combat");
    }
    {
        auto player = fixture.player();
        insert(*player, 1, 7, 2);
        deliver(*player,
                protocol::Property::frame_update_vital(
                    static_cast<std::uint8_t>(protocol::PropertyType::Hp), 20, 100));
        tick(*player, 10000);
        std::uint32_t iid = 0;
        std::uint8_t threshold = 0;
        expect(!sent.empty() &&
                   sent.front().type == static_cast<std::uint8_t>(protocol::C2S::UseItem) &&
                   protocol::UseItem::parse_c2s(
                       sent.front().payload.data(), sent.front().payload.size(), iid, &threshold) &&
                   iid == 1 && threshold == 75,
               "Auto-HP medicine uses its normal supported threshold request");
    }
    {
        auto player = fixture.player(true, false);
        insert(*player, 10, 1, 1, true);
        insert(*player, 11, 3);
        insert(*player, 12, 4);
        insert(*player, 13, 5);
        tick(*player, 10000);
        std::uint32_t iid = 0;
        expect(sent.size() == 1 &&
                   sent.front().type == static_cast<std::uint8_t>(protocol::C2S::PutOffItem) &&
                   protocol::ItemWear::parse_client_item_id(
                       sent.front().payload.data(), sent.front().payload.size(), iid) &&
                   iid == 10,
               "upgrade replacement first unequips the current weapon using a normal request");
        deliver(*player, protocol::ItemWear::frame_server_put_off(77, 10, 1));
        tick(*player, 12000);
        expect(sent.size() == 1 &&
                   sent.front().type == static_cast<std::uint8_t>(protocol::C2S::PutOnItem) &&
                   protocol::ItemWear::parse_client_item_id(
                       sent.front().payload.data(), sent.front().payload.size(), iid) &&
                   iid == 11,
               "confirmed unequip is followed by the usable upgrade, excluding wrong-class and "
               "high-level gear");
        tick(*player, 16000);
        expect(sent.size() == 1 &&
                   sent.front().type == static_cast<std::uint8_t>(protocol::C2S::PutOnItem) &&
                   protocol::ItemWear::parse_client_item_id(
                       sent.front().payload.data(), sent.front().payload.size(), iid) &&
                   iid == 10,
               "a rejected upgrade restores the previous weapon instead of hunting unequipped");
    }
    {
        auto player = fixture.player(false);
        monster(*player);
        tick(*player, 10000);
        expect(sent.size() == 1 &&
                   sent.front().type == static_cast<std::uint8_t>(protocol::C2S::AskNpc),
               "available quests take priority over ordinary hunting");
        deliver(*player, protocol::Quest::frame_send_html(100));
        tick(*player, 12000);
        std::uint32_t key = 0;
        expect(sent.size() == 1 &&
                   sent.front().type == static_cast<std::uint8_t>(protocol::C2S::Quest) &&
                   protocol::Quest::parse_quest(
                       sent.front().payload.data(), sent.front().payload.size(), key) &&
                   key == protocol::Quest::make_key(10, 1),
               "quest links use ordinary quest packets and locally previewed prerequisites");
    }
    {
        auto player = fixture.player();
        monster(*player);
        deliver(*player, protocol::Skill::frame_skill_info({{3, 1}}));
        tick(*player, 10000);
        expect(sent.size() == 1 &&
                   sent.front().type == static_cast<std::uint8_t>(protocol::C2S::PreSkill),
               "learned combat skill starts its regular precast");
        tick(*player, 10200);
        expect(sent.empty(), "skill is not sent before the catalog wind-up expires");
        tick(*player, 10500);
        expect(sent.size() == 1 &&
                   sent.front().type == static_cast<std::uint8_t>(protocol::C2S::Skill),
               "skill is sent after the regular precast interval");
    }
    {
        text(
            fixture.root + "/portals.json",
            R"({"portals":[{"id":"owned_gate","fromMap":0,"fromRect":[32, 32, 33, 33],"toMap":1,"to":[1120, 1120, 10000]}]})");
        text(fixture.root + "/npcs/1.json",
             R"({"index":1,"map":1,"html":100,"xy":[1120, 1120, 10000]})");
        auto portalData = fixture.load_data();
        text(fixture.root + "/npcs/1.json", R"({"index":1,"html":100,"xy":[1024, 1024, 10000]})");
        std::filesystem::remove(fixture.root + "/portals.json");
        auto player = fixture.player(false);
        player->start(portalData);
        tick(*player, 10000);
        expect(!sent.empty() &&
                   (sent.front().type == static_cast<std::uint8_t>(protocol::C2S::MoveOn) ||
                    sent.front().type == static_cast<std::uint8_t>(protocol::C2S::MoveEnd)) &&
                   std::none_of(sent.begin(),
                                sent.end(),
                                [](const auto& packet) {
                                    return packet.type ==
                                           static_cast<std::uint8_t>(protocol::C2S::Teleport);
                                }),
               "a quest on another map routes to an ordinary field portal using movement packets");
    }
    {
        std::filesystem::create_directories(fixture.root + "/missing-maps");
        auto missing = fixture.load_data(false);
        auto player = fixture.player();
        player->start(missing);
        monster(*player, 1200);
        tick(*player, 10000);
        tick(*player, 10010);
        expect(sent.empty() && player->status().find("No route or missing navigation data") == 0,
               "missing area data blocks movement instead of being treated as open ground");
        const auto profile = fixture.root + "/navigation.profile";
        const auto relativeAssets =
            std::filesystem::relative(fixture.root, platform::executable_directory())
                .generic_string();
        const auto profileText = "data_root=" + relativeAssets +
                                 "\nmaps_subdir=visuals\nserver_map_root=" + relativeAssets +
                                 "/maps\n";
        text(profile, profileText.c_str());
        setenv("OPENKAL_PROFILE", (fixture.root + "/absent.profile").c_str(), 1);
        auto fromProfile = TestPlayerAgent::load_data(fixture.root + "/package", profile);
        player = fixture.player();
        player->start(fromProfile);
        monster(*player, 1200);
        for (int slice = 0; slice < 32; ++slice) {
            tick(*player, 10000 + static_cast<std::uint64_t>(slice) * 50);
            if (!sent.empty() || player->status().find("Planning route") != 0) {
                break;
            }
        }
        expect(!sent.empty(),
               ("explicit profile wins over the environment and resolves relative external map "
                "paths: " +
                player->status())
                   .c_str());
        setenv("OPENKAL_PROFILE", profile.c_str(), 1);
        expect(TestPlayerAgent::load_data(fixture.root + "/package") != nullptr,
               "empty profile selection honors OPENKAL_PROFILE");
        unsetenv("OPENKAL_PROFILE");
        bool rejected = false;
        const auto archive = fixture.root + "/package/config.pak";
        std::filesystem::rename(archive, archive + ".hidden");
        try {
            (void) TestPlayerAgent::load_data(fixture.root + "/package", profile);
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        std::filesystem::rename(archive + ".hidden", archive);
        expect(rejected,
               "missing config.pak fails instead of selecting an incomplete loose catalog");
        fixture.data = fixture.load_data();
    }
    {
        auto player = fixture.player();
        monster(*player, 1120);
        bool requested_move = false;
        for (int slice = 0; slice < 32; ++slice) {
            tick(*player, 10000 + static_cast<std::uint64_t>(slice) * 50);
            expect(player->position().x == 1024,
                   "every planning slice preserves the confirmed pose before a movement echo");
            requested_move = std::any_of(sent.begin(), sent.end(), [](const auto& packet) {
                return packet.type == static_cast<std::uint8_t>(protocol::C2S::MoveOn) ||
                       packet.type == static_cast<std::uint8_t>(protocol::C2S::MoveEnd);
            });
            if (requested_move || !sent.empty() || player->status().find("Planning route") != 0) {
                break;
            }
        }
        expect(requested_move && player->position().x == 1024,
               "requested movement does not optimistically change the confirmed pose");
        deliver(*player, protocol::Move::frame_player_on(77, 8, 0, 0));
        expect(player->position().x == 1032, "movement echo advances the confirmed pose");
        deliver(*player, protocol::Move::frame_before(77, 1024, 1024, 10000, 0));
        expect(player->position().x == 1024,
               "movement rejection restores the authoritative position");
    }
    {
        auto player = fixture.player();
        insert(*player, 10, 3, 1, true);
        for (std::uint32_t id = 100; id < 161; ++id) {
            insert(*player, id, 1);
        }
        insert(*player, 161, 1, 1, false, true);
        tick(*player, 10000);
        expect(sent.size() == 1 &&
                   sent.front().type == static_cast<std::uint8_t>(protocol::C2S::ItemList),
               "full bags trigger a normal merchant visit");
        protocol::ShopItemList list;
        list.npc_id = 800;
        deliver(*player, protocol::Shop::frame_item_list_s2c(list));
        tick(*player, 12000);
        std::uint32_t npc = 0, iid = 0;
        std::uint16_t count = 0;
        expect(sent.size() == 1 &&
                   sent.front().type == static_cast<std::uint8_t>(protocol::C2S::SellItem) &&
                   protocol::Shop::parse_sell_c2s(
                       sent.front().payload.data(), sent.front().payload.size(), npc, iid, count) &&
                   npc == 800 && iid == 160 && count == 1,
               "merchant sale keeps worn and bound equipment and sells an obsolete weapon");
    }
    parallel_players(fixture);
    unsupported_quest(fixture);
    distant_quest(fixture);
    live_player(fixture);
    return test::g_failures == 0 ? 0 : 1;
}
