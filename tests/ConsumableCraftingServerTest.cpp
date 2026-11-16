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
#include "openkal/map/Constants.h"
#include "openkal/platform/File.h"
#include "openkal/platform/Json.h"
#include "openkal/protocol/Chatting.h"
#include "openkal/protocol/Cooking.h"
#include "openkal/protocol/FrameParser.h"
#include "openkal/protocol/GameExit.h"
#include "openkal/protocol/Handshake.h"
#include "openkal/protocol/Transform.h"
#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketReader.h"
#include "openkal/protocol/PacketWriter.h"
#include "openkal/protocol/Quest.h"
#include "openkal/protocol/Teleport.h"
#include "openkal/server/ConsumableCrafting.h"
#include "openkal/server/ServerHost.h"
#include "openkal/server/SpeedMedicine.h"
#include "openkal/server/net/Transport.h"
#include "openkal/server/store/DataStore.h"
#include "test_util.h"

#include <chrono>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <string_view>
#include <utility>
#include <sqlite3.h>
#include <thread>
#include <unistd.h>

namespace {
    using namespace openkal;
    using test::expect;
    using ObservedFrames = std::vector<std::pair<std::uint8_t, std::vector<std::uint8_t>>>;

    bool wait_frame(server::ISession& session,
                    std::vector<std::uint8_t>& rx,
                    protocol::S2C expected,
                    std::vector<std::uint8_t>& payload,
                    ObservedFrames* observed = nullptr) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        do {
            if (!session.recv_into(rx)) {
                return false;
            }
            for (;;) {
                std::size_t consumed = 0;
                std::uint8_t type = 0;
                const auto status =
                    protocol::FrameParser::pop(rx.data(), rx.size(), consumed, type, payload);
                if (status == protocol::FrameParseStatus::Incomplete) {
                    break;
                }
                if (status == protocol::FrameParseStatus::Invalid) {
                    return false;
                }
                rx.erase(rx.begin(), rx.begin() + static_cast<std::ptrdiff_t>(consumed));
                if (observed) {
                    observed->emplace_back(type, payload);
                }
                if (type == static_cast<std::uint8_t>(expected)) {
                    return true;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        } while (std::chrono::steady_clock::now() < deadline);
        return false;
    }

    void fixture(const std::string& path, const std::string& contents) {
        std::ofstream out(path);
        out << contents;
        expect(out.good(), "write synthetic crafting fixture");
    }

    void run_case(const std::string& root,
                  bool full,
                  bool fail_save,
                  bool frees_slot = false,
                  bool character_bound = false,
                  server::ConsumableFamily family = server::ConsumableFamily::Medicine) {
        const auto& recipes = server::consumable_recipes(family);
        const int base = family == server::ConsumableFamily::MagicPotion ? 23004 : 23000;
        const int ingredient = recipes.front().ingredients.back().index;
        const int medicine_per_craft = family == server::ConsumableFamily::AutoMedicine ? 1 : 2;
        const int initial_medicine = full ? (frees_slot ? medicine_per_craft : 4) : 8;
        const int first_output = recipes.front().output_index;
        const auto request = [&](int row) { return recipes[row].flag; };
        server::ServerConfig config;
        config.maps_dir = root + "/maps";
        config.monsters_dir = root + "/monsters";
        config.items_dir = root + "/items";
        config.npcs_dir = root + "/npcs";
        config.save_dir = root + "/save";
        config.unix_path = root + "/game.sock";
        config.store = server::StoreBackend::Sqlite;
        config.transport = server::TransportKind::Unix;
        config.spawn_x = config.spawn_y = 15 * map::kTileSize;
        config.spawn_z = 0;
        config.map_radius_cells = 1;
        config.max_monsters = 0;
        for (const auto& dir :
             {config.maps_dir, config.monsters_dir, config.items_dir, config.npcs_dir}) {
            test::mkdir_p(dir);
        }
        expect(test::write_open_area(config.maps_dir + "/n_000_000.akarea"), "write crafting map");
        fixture(config.monsters_dir + "/1.json",
                R"({"index":1,"name":"Unused","stats":{"level":1,"hp":1},"spawns":[]})");
        const std::string origin = std::to_string(config.spawn_x);
        fixture(config.npcs_dir + "/1.json",
                "{\"index\":1,\"html\":192,\"xy\":[" + std::to_string(config.spawn_x - 120) + "," +
                    origin + ",0],\"dir\":[0,0],\"shape\":7}");
        fixture(config.npcs_dir + "/2.json",
                "{\"index\":2,\"html\":10,\"xy\":[" + origin + "," + origin + ",0]}");
        fixture(config.npcs_dir + "/3.json",
                "{\"index\":3,\"html\":990300,\"xy\":[" + std::to_string(config.spawn_x + 200) +
                    "," + origin + ",0]}");
        const auto alchemist_bytes = platform::read_file(
            std::string(OPENKAL_REPO_ROOT) + "/config/npcs/60001_medicine_alchemist.json");
        expect(alchemist_bytes.has_value(), "load alchemist definition");
        if (!alchemist_bytes) {
            return;
        }
        auto alchemist_definition =
            platform::Json::parse(std::string(alchemist_bytes->begin(), alchemist_bytes->end()));
        alchemist_definition.object()["xy"] =
            platform::Json::parse("[" + origin + "," + origin + ",0]");
        fixture(config.npcs_dir + "/60001.json", platform::json_stringify(alchemist_definition));
        for (int index : {23000,
                          23001,
                          23002,
                          23003,
                          23004,
                          23005,
                          23006,
                          23007,
                          23008,
                          23009,
                          23010,
                          23011,
                          23027,
                          23028,
                          23029,
                          9000}) {
            fixture(config.items_dir + "/" + std::to_string(index) + ".json",
                    "{\"index\":" + std::to_string(index) +
                        ",\"name\":\"Synthetic "
                        "material\",\"class\":[\"general\",\"etc\"],\"stackable\":1}");
        }
        auto store = server::create_data_store(server::StoreBackend::Sqlite);
        expect(store && store->open(config.save_dir), "open crafting store");
        server::CharacterRecord hero;
        hero.name = "Alchemist";
        hero.hp = hero.mp = 100;
        hero.map_x = hero.map_y = config.spawn_x;
        hero.map_z = 0;
        expect(store->ensure_default_player("crafter", "crafter", hero), "create crafting player");
        const auto account = store->find_account("crafter");
        if (!account) {
            return;
        }
        const auto characters = store->list_characters(account->id);
        if (characters.empty()) {
            return;
        }
        config.character_id = characters.front().id;
        for (const auto& item : store->load_items(config.character_id)) {
            expect(store->delete_item(item.iid), "clear crafting starter inventory");
        }
        const auto seed = [&](int iid, int index, int count, bool bound = false) {
            server::ItemRecord item;
            item.character_id = config.character_id;
            item.iid = iid;
            item.index = index;
            item.count = count;
            item.account_bound = bound;
            item.character_bound = character_bound && bound;
            expect(store->insert_item(item), "seed crafting materials");
        };
        seed(100, base, full ? initial_medicine : 1);
        seed(101, ingredient, full ? 4 : 7, true);
        if (!full) {
            seed(102, base, 7);
        }
        if (!full && family == server::ConsumableFamily::AutoMedicine) {
            for (int tier = 1; tier < 4; ++tier) {
                seed(110 + tier, base + tier, 1);
            }
        }
        if (full) {
            for (int i = 0; i < 70; ++i) {
                seed(200 + i, 9000, 1);
            }
        }
        // Persistence must roll back earlier deletions if output insertion fails.
        if (fail_save) {
            sqlite3* db = nullptr;
            expect(sqlite3_open((config.save_dir + "/" + server::kSqliteFileName).c_str(), &db) ==
                       SQLITE_OK,
                   "open crafting failure injection");
            expect(sqlite3_exec(db,
                                ("CREATE TRIGGER reject_crafted BEFORE INSERT ON items WHEN "
                                 "NEW.item_index=" +
                                 std::to_string(first_output) +
                                 " BEGIN SELECT RAISE(ABORT,'synthetic failure'); END;")
                                    .c_str(),
                                nullptr,
                                nullptr,
                                nullptr) == SQLITE_OK,
                   "inject output persistence failure");
            sqlite3_close(db);
        }
        store->close();
        server::ServerHost host;
        expect(host.start(config), "start isolated crafting server");
        for (int i = 0; i < 200 && !std::filesystem::exists(config.unix_path); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        auto session = server::connect_unix(config.unix_path);
        if (!session) {
            expect(false, "connect crafting client");
            return;
        }
        std::vector<std::uint8_t> rx, payload;
        expect(session->send_all(protocol::Handshake::build_connect(0)) &&
                   wait_frame(*session, rx, protocol::S2C::Code, payload),
               "crafting handshake");
        protocol::S2CCodeFields code;
        expect(protocol::Handshake::parse_code(payload.data(), payload.size(), code),
               "decode crafting handshake");
        expect(session->send_all(protocol::Handshake::build_answer(code.protocol_test_value)) &&
                   wait_frame(*session, rx, protocol::S2C::CreatePlayer, payload),
               "enter crafting world");
        std::uint32_t alchemist = 0, other = 0, distant = 0;
        for (int i = 0; i < 4; ++i) {
            expect(wait_frame(*session, rx, protocol::S2C::CreateNpc, payload),
                   "receive synthetic NPC spawn");
            protocol::CreateNpcInfo npc;
            if (protocol::Quest::parse_create_npc(payload.data(), payload.size(), npc)) {
                if (npc.index == 60001) {
                    alchemist = npc.id;
                    expect(npc.x == config.spawn_x && npc.y == config.spawn_y && npc.shape == 7,
                           "server respects the explicit NPC coordinates");
                }
                if (npc.index == 2) {
                    other = npc.id;
                }
                if (npc.index == 3) {
                    distant = npc.id;
                }
            }
        }
        expect(alchemist != 0 && other != 0 && distant != 0,
               "discover alchemist and unrelated NPC");
        const auto barrier = [&] {
            expect(session->send_all(protocol::GameExit::frame_client_request()) &&
                       wait_frame(*session, rx, protocol::S2C::AnsGameExit, payload),
                   "synchronize crafting request");
        };
        const auto craft = [&](std::uint8_t flag) {
            expect(session->send_all(protocol::Quest::frame_quest(
                       protocol::Quest::make_key(server::kConsumableCraftingQuestIndex, flag))),
                   "send recipe request");
            barrier();
        };
        const auto inspect = [&] {
            expect(store->open(config.save_dir), "reopen saved crafting inventory");
            auto items = store->load_items(config.character_id);
            store->close();
            return items;
        };
        const auto count = [&](int index) {
            int total = 0;
            for (const auto& item : inspect()) {
                if (item.index == index) {
                    total += item.count;
                }
            }
            return total;
        };
        craft(request(0));
        expect(count(base) == initial_medicine && count(first_output) == 0,
               "crafting without talking is rejected");
        expect(session->send_all(protocol::Quest::frame_ask_npc(alchemist)), "talk to alchemist");
        craft(255);
        expect(count(base) == initial_medicine, "unknown recipe cannot consume materials");
        expect(session->send_all(protocol::Quest::frame_ask_npc(distant)),
               "attempt out-of-range conversation");
        craft(request(0));
        expect(count(base) == initial_medicine && count(first_output) == 0,
               "distant alchemist cannot authorize a recipe");
        expect(session->send_all(protocol::Quest::frame_ask_npc(alchemist)),
               "resume nearby alchemist");
        craft(request(0));
        if (frees_slot) {
            expect(count(base) == 0 && count(ingredient) == 3 && count(first_output) == 1,
                   "a consumed input stack frees room for output in a full bag");
        } else if (full || fail_save) {
            expect(count(base) == initial_medicine && count(ingredient) == (full ? 4 : 7) &&
                       count(first_output) == 0,
                   full ? "full bag retains every ingredient"
                        : "failed save rolls back all ingredients and output");
            craft(request(0));
            expect(count(first_output) == 0,
                   "retry cannot duplicate output after a failed exchange");
        } else {
            expect(count(base) == 8 - medicine_per_craft && count(ingredient) == 6 &&
                       count(first_output) == 1,
                   "split stacks and the family ingredient produce the expected output");
            if (family == server::ConsumableFamily::AutoMedicine) {
                for (int tier = 1; tier < 4; ++tier) {
                    craft(request(tier));
                }
                for (int tier = 0; tier < 4; ++tier) {
                    expect(count(23008 + tier) == 1 && count(base + tier) == (tier == 0 ? 7 : 0),
                           "every normal Medicine size converts to matching Auto-HP Medicine");
                    for (const auto& item : inspect()) {
                        if (item.index == 23008 + tier) {
                            expect(item.account_bound && (!character_bound || item.character_bound),
                                   "every auto medicine retains the ingredient binding");
                        }
                    }
                }
                expect(count(ingredient) == 3, "each conversion consumes exactly one root");
                craft(request(3));
                expect(count(23011) == 1 && count(ingredient) == 3,
                       "missing Huge Medicine cannot consume a root or duplicate output");
            } else {
                for (int i = 0; i < 3; ++i) {
                    craft(request(0));
                }
                craft(request(1));
                craft(request(1));
                craft(request(2));
                expect(count(base) == 0 && count(first_output) == 0 && count(base + 2) == 0 &&
                           count(base + 3) == 1 && count(ingredient) == 0,
                       "all three recipes have exact ingredient and output counts");
                for (const auto& item : inspect()) {
                    if (item.index == base + 3) {
                        expect(item.account_bound && (!character_bound || item.character_bound),
                               "crafted medicine retains input account binding through every tier");
                    }
                }
                craft(request(2));
                expect(count(base + 3) == 1, "missing ingredients cannot duplicate Huge Medicine");
                expect(session->send_all(protocol::Quest::frame_ask_npc(other)),
                       "switch to unrelated NPC");
                craft(request(0));
                expect(count(base + 3) == 1 && count(first_output) == 0,
                       "another conversation clears crafting authorization");
            }
        }
        session->close();
        host.stop();
    }
    void run_routing_case(const std::string& root) {
        server::ServerConfig config;
        config.maps_dir = root + "/maps";
        config.monsters_dir = root + "/monsters";
        config.items_dir = root + "/items";
        config.npcs_dir = root + "/npcs";
        config.quests_dir = root + "/quests";
        config.fishing_path = root + "/fishing.json";
        config.server_events_path = root + "/events.json";
        config.save_dir = root + "/save";
        config.unix_path = root + "/game.sock";
        config.store = server::StoreBackend::Sqlite;
        config.transport = server::TransportKind::Unix;
        config.spawn_x = config.spawn_y = 1024;
        config.spawn_z = 0;
        config.map_radius_cells = 1;
        config.max_monsters = 0;
        for (const auto& dir : {config.maps_dir,
                                config.monsters_dir,
                                config.items_dir,
                                config.npcs_dir,
                                config.quests_dir}) {
            test::mkdir_p(dir);
        }
        expect(test::write_open_area(config.maps_dir + "/n_000_000.akarea"),
               "write owned routing map");
        fixture(
            config.monsters_dir + "/1.json",
            R"({"index":1,"name":"Unused routing monster","stats":{"level":1,"hp":1},"spawns":[]})");
        const auto npc = [&](int index, int kind, int html, std::string_view extra) {
            fixture(config.npcs_dir + "/" + std::to_string(index) + ".json",
                    "{\"index\":" + std::to_string(index) + ",\"kind\":" + std::to_string(kind) +
                        ",\"html\":" + std::to_string(html) + ",\"map\":0,\"xy\":[1024, 1024, 0]" +
                        std::string(extra) + '}');
        };
        npc(61001, 0, 7001, ",\"quest\":[6200, 1]");
        npc(61002, 3, 7002, ",\"quest\":[6201, 1]");
        npc(61003, 0, 0, ",\"quest\":[6202, 1]");
        npc(61004, 5, 7004, ",\"teleports\":[[0, 1056, 1024]]");
        npc(61008, 0, server::kSpeedMedicineDialogId, "");
        npc(61009, 0, server::kConsumableCraftingDialogId, "");
        npc(61010, 0, 7010, "");
        for (int index : {6200,
                          6201,
                          6202,
                          6204,
                          6205,
                          static_cast<int>(server::kConsumableCraftingQuestIndex)}) {
            fixture(config.quests_dir + "/" + std::to_string(index) + ".json",
                    "{\"index\":" + std::to_string(index) +
                        ",\"flag\":1,\"cases\":[{\"then\":[{\"op\":\"html\",\"id\":" +
                        std::to_string(index + 1000) + "},{\"op\":\"save\",\"quest\":" +
                        std::to_string(index) + ",\"flag\":2}]}]}");
        }
        fixture(
            config.quests_dir + "/6203.json",
            R"({"index":6203,"flag":1,"cases":[{"then":[{"op":"gstate","gstate":5,"value":1}]}]})");
        fixture(
            config.fishing_path,
            R"({"schema":"openkal.fishing/v1","baseIntervalSeconds":30,"acceleratedIntervalSeconds":15,"rewards":[{"itemIndex":23016,"chancePerThousand":1000}]})");
        fixture(
            config.server_events_path,
            R"({"events":[{"id":"exp_event","managerNpcIndex":61010,"donateQuest":{"index":6250,"flag":1},"donateHundredQuest":{"index":6251,"flag":1},"donationItem":23016,"durationSeconds":3600,"extensionSeconds":900,"requiredItems":100,"bonusStepPercent":10,"maxBonusPercent":50}]})");
        const protocol::CookingOrder order{1, 1, 1};
        const auto cost = server::speed_medicine_cost(order);
        const std::vector<std::pair<int, int>> materials = {
            {server::kDemonBloodItemIndex, cost.demon_blood},
            {server::kSeaBreamItemIndex, cost.sea_bream},
            {server::kBambooKegItemIndex, cost.bamboo_keg},
            {server::kBloodFishItemIndex, cost.blood_fish}};
        for (int index : {23016,
                          server::kDemonBloodItemIndex,
                          server::kSeaBreamItemIndex,
                          server::kBambooKegItemIndex,
                          server::kBloodFishItemIndex}) {
            fixture(
                config.items_dir + "/" + std::to_string(index) + ".json",
                "{\"index\":" + std::to_string(index) +
                    ",\"name\":\"Owned routing material\",\"class\":[\"general\",\"etc\"],\"stackable\":1}");
        }
        auto store = server::create_data_store(server::StoreBackend::Sqlite);
        expect(store && store->open(config.save_dir), "open owned routing store");
        if (!store) {
            return;
        }
        server::CharacterRecord hero;
        hero.name = "Routing witness";
        hero.hp = hero.mp = 100;
        hero.map_x = hero.map_y = 1024;
        hero.money = 10000;
        expect(store->ensure_default_player("routing", "routing", hero), "create routing identity");
        const auto account = store->find_account("routing");
        if (!account) {
            expect(false, "routing account exists");
            return;
        }
        const auto characters = store->list_characters(account->id);
        if (characters.empty()) {
            expect(false, "routing character exists");
            return;
        }
        config.character_id = characters.front().id;
        for (const auto& row : store->load_items(config.character_id)) {
            expect(store->delete_item(row.iid), "clear routing starter rows");
        }
        int iid = 100;
        const auto seed = [&](int index, int count) {
            server::ItemRecord row;
            row.iid = iid++;
            row.character_id = config.character_id;
            row.index = index;
            row.count = count;
            expect(store->insert_item(row), "seed owned routing material");
        };
        seed(23016, 101);
        for (const auto& material : materials) {
            if (material.second > 0) {
                seed(material.first, material.second);
            }
        }
        store->close();
        const auto inspect = [&] {
            expect(store->open(config.save_dir), "read routing state after saved barrier");
            auto result = store->load_items(config.character_id);
            store->close();
            return result;
        };
        const auto count = [&](int index) {
            int result = 0;
            for (const auto& row : inspect()) {
                if (row.index == index) {
                    result += row.count;
                }
            }
            return result;
        };
        const auto saved_quests = [&] {
            expect(store->open(config.save_dir), "read durable routing quest flags");
            std::map<int, std::pair<int, int>> result;
            for (const auto& row : store->load_quest_flags(config.character_id)) {
                result[row.quest] = {row.flag, row.clear};
            }
            store->close();
            return result;
        };
        const auto route_replies = [&](const ObservedFrames& frames) {
            std::vector<std::string> replies;
            for (const auto& frame : frames) {
                if (frame.first == static_cast<std::uint8_t>(protocol::S2C::SendHtml)) {
                    std::int32_t html = 0;
                    std::uint32_t source = 0;
                    expect(protocol::Quest::parse_send_html(
                               frame.second.data(), frame.second.size(), html, &source),
                           "parse routing HTML source identity");
                    replies.push_back("html:" + std::to_string(html) + ':' +
                                      std::to_string(source));
                } else if (frame.first == static_cast<std::uint8_t>(protocol::S2C::SaveQuest)) {
                    protocol::QuestFlagEntry entry;
                    expect(protocol::Quest::parse_save_quest(
                               frame.second.data(), frame.second.size(), entry),
                           "parse routing SaveQuest");
                    replies.push_back("quest:" + std::to_string(entry.quest) + ':' +
                                      std::to_string(entry.flag) + ':' +
                                      std::to_string(entry.clear));
                }
            }
            return replies;
        };
        const auto chats = [&](const ObservedFrames& frames) {
            std::vector<std::string> result;
            for (const auto& frame : frames) {
                if (frame.first == static_cast<std::uint8_t>(protocol::S2C::Chatting)) {
                    std::string speaker, text;
                    expect(protocol::Chatting::parse_s2c(
                               frame.second.data(), frame.second.size(), speaker, text),
                           "parse routing feedback");
                    result.push_back(std::move(text));
                }
            }
            return result;
        };
        const auto quest_dir = config.quests_dir;
        std::map<int, std::pair<int, int>> expected_quests;
        for (const bool with_engine : {true, false}) {
            config.quests_dir = with_engine ? quest_dir : "";
            server::ServerHost host;
            expect(host.start(config), "start actual routing Host profile");
            for (int attempt = 0; attempt < 200 && !std::filesystem::exists(config.unix_path);
                 ++attempt) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            auto session = server::connect_unix(config.unix_path);
            if (!session) {
                expect(false, "connect actual routing peer");
                return;
            }
            std::vector<std::uint8_t> rx, payload;
            expect(session->send_all(protocol::Handshake::build_connect(0)) &&
                       wait_frame(*session, rx, protocol::S2C::Code, payload),
                   "routing handshake");
            protocol::S2CCodeFields code;
            expect(protocol::Handshake::parse_code(payload.data(), payload.size(), code),
                   "decode routing handshake");
            expect(session->send_all(protocol::Handshake::build_answer(code.protocol_test_value)) &&
                       wait_frame(*session, rx, protocol::S2C::CreatePlayer, payload),
                   "enter routing world after actual restart");
            const auto send = [&](const std::vector<std::uint8_t>& frame) {
                expect(!frame.empty() && session->send_all(frame), "send ordinary routing packet");
            };
            const auto barrier = [&] {
                ObservedFrames frames;
                send(protocol::GameExit::frame_client_request());
                bool saved = false;
                expect(wait_frame(*session, rx, protocol::S2C::AnsGameExit, payload, &frames) &&
                           protocol::GameExit::parse_server_answer(
                               payload.data(), payload.size(), saved) &&
                           saved,
                       "fresh routing FIFO barrier acknowledges successful persistence");
                return frames;
            };
            const auto initial = barrier();
            std::map<int, std::uint32_t> ids;
            for (const auto& frame : initial) {
                if (frame.first == static_cast<std::uint8_t>(protocol::S2C::CreateNpc)) {
                    protocol::CreateNpcInfo value;
                    expect(protocol::Quest::parse_create_npc(
                               frame.second.data(), frame.second.size(), value),
                           "parse owned routing NPC identity");
                    ids[value.index] = value.id;
                }
            }
            expect(ids.size() == 7, "discover every routing NPC by definition identity");
            if (ids.size() != 7) {
                return;
            }
            const auto ask = [&](int index) {
                send(protocol::Quest::frame_ask_npc(ids.at(index)));
            };
            const auto request = [&](int index) {
                send(protocol::Quest::frame_quest(protocol::Quest::make_key(index, 1)));
            };
            const auto html = [&](int value, int index) {
                return "html:" + std::to_string(value) + ':' + std::to_string(ids.at(index));
            };
            if (with_engine) {
                expected_quests = saved_quests();
                expect(expected_quests == std::map<int, std::pair<int, int>>{{9001, {1, 0}}},
                       "fresh routing character has only the starter event quest 9001/1/0");
                protocol::PacketWriter trailing;
                trailing.write_u32(ids.at(61001));
                trailing.write_u8(0xaa);
                send(trailing.frame(static_cast<std::uint8_t>(protocol::C2S::AskNpc)));
                expect(
                    route_replies(barrier()) == std::vector<std::string>{html(7001, 61001)} &&
                        saved_quests() == expected_quests,
                    "ordinary HTML preserves all initial quest flags and accepts trailing Ask bytes");
                ask(61002);
                expect(route_replies(barrier()) ==
                           std::vector<std::string>{html(7201, 61002), "quest:6201:2:0"},
                       "kind3 quest HTML then SaveQuest wins over NPC HTML");
                expected_quests[6201] = {2, 0};
                expect(saved_quests() == expected_quests,
                       "kind3 persists only its authored quest flag");
                ask(61003);
                expect(route_replies(barrier()) ==
                           std::vector<std::string>{html(7202, 61003), "quest:6202:2:0"},
                       "missing NPC HTML falls back to the quest engine");
                expected_quests[6202] = {2, 0};
                expect(saved_quests() == expected_quests,
                       "fallback persists only its authored quest flag");
                protocol::PacketWriter quest;
                quest.write_u32(protocol::Quest::make_key(6204, 1));
                quest.write_u8(0xbb);
                send(quest.frame(static_cast<std::uint8_t>(protocol::C2S::Quest)));
                expect(route_replies(barrier()) ==
                           std::vector<std::string>{html(7204, 61003), "quest:6204:2:0"},
                       "trailing Quest bytes preserve the current dialog source identity");
                expected_quests[6204] = {2, 0};
                expect(saved_quests() == expected_quests,
                       "trailing Quest preserves every unrelated flag");
                request(server::kConsumableCraftingQuestIndex);
                const auto rejected_craft = barrier();
                expect(route_replies(rejected_craft).empty() && !chats(rejected_craft).empty(),
                       "recognized rejected crafting never falls into the overlapping quest step");
                ask(61004);
                expect(route_replies(barrier()) == std::vector<std::string>{html(7004, 61004)},
                       "first portal Ask arms HTML confirmation");
                protocol::PacketWriter malformed;
                malformed.write_u8(0);
                send(malformed.frame(static_cast<std::uint8_t>(protocol::C2S::AskNpc)));
                send(protocol::Quest::frame_ask_npc(0xffffffffu));
                expect(
                    route_replies(barrier()).empty(),
                    "malformed and missing intermediate Asks do not emit a second portal dialog");
                ask(61004);
                const auto teleported = barrier();
                bool destination = false;
                for (const auto& frame : teleported) {
                    if (frame.first == static_cast<std::uint8_t>(protocol::S2C::Teleport)) {
                        std::uint8_t map = 0, cheat = 0;
                        int x = 0, y = 0, z = 0;
                        expect(protocol::Teleport::parse_server(
                                   frame.second.data(), frame.second.size(), map, x, y, z, cheat),
                               "parse staged routing portal destination");
                        destination = map == 0 && x == 1056 && y == 1024 && z == 0;
                    }
                }
                expect(
                    destination && route_replies(teleported).empty(),
                    "second portal Ask after invalid selection retains confirmation and stages teleport");
                send(protocol::Teleport::frame_client_ack(1, 9876));
                (void) barrier();
                ask(61010);
                (void) barrier();
                ask(61001);
                (void) barrier();
                request(6251);
                const auto denied = barrier();
                const auto denied_chats = chats(denied);
                expect(std::find(denied_chats.begin(),
                                 denied_chats.end(),
                                 "Talk to the Exp Event Manager first.") != denied_chats.end() &&
                           count(23016) == 101,
                       "switching dialogs clears donation authorization before coin removal");
                ask(61010);
                (void) barrier();
                request(6251);
                const auto donated = barrier();
                std::size_t insert = donated.size(), event = donated.size(),
                            announcement = donated.size();
                for (std::size_t i = 0; i < donated.size(); ++i) {
                    if (donated[i].first == static_cast<std::uint8_t>(protocol::S2C::InsertItem)) {
                        insert = std::min(insert, i);
                    }
                    if (donated[i].first ==
                        static_cast<std::uint8_t>(protocol::S2C::ExpEventState)) {
                        event = std::min(event, i);
                    }
                    if (donated[i].first == static_cast<std::uint8_t>(protocol::S2C::Chatting)) {
                        std::string speaker, text;
                        if (protocol::Chatting::parse_s2c(donated[i].second.data(),
                                                          donated[i].second.size(),
                                                          speaker,
                                                          text) &&
                            text.find("EXP Event started: +10%") == 0) {
                            announcement = i;
                        }
                    }
                }
                expect(
                    insert < event && event < announcement && count(23016) == 1 &&
                        route_replies(donated).empty(),
                    "donation bag diff precedes event snapshot and announcement without quest fallback");
                ask(61008);
                (void) barrier();
                send(protocol::Cooking::frame_c2s(protocol::kCookingPotions, order));
                protocol::PacketWriter quit;
                quit.write_u8(0xff);
                send(quit.frame(static_cast<std::uint8_t>(protocol::C2S::QuestJobEnd)));
                const auto cooked = barrier();
                const auto activity = [&](const ObservedFrames& frames) {
                    std::vector<std::uint32_t> result;
                    for (const auto& frame : frames) {
                        if (frame.first == static_cast<std::uint8_t>(protocol::S2C::GState)) {
                            protocol::PacketReader reader(frame.second);
                            std::uint32_t player = 0, state = 0;
                            expect(reader.read_u32(player) && reader.read_u32(state) &&
                                       !reader.remaining(),
                                   "parse routing activity state");
                            result.push_back(state & (protocol::kCgsFish | protocol::kCgsCooking));
                        }
                    }
                    return result;
                };
                expect(activity(cooked) == std::vector<std::uint32_t>{protocol::kCgsCooking, 0},
                       "Cooking then arbitrary-payload quit dispatch preserves state order");
                request(6203);
                send(quit.frame(static_cast<std::uint8_t>(protocol::C2S::QuestJobEnd)));
                expect(activity(barrier()) == std::vector<std::uint32_t>{protocol::kCgsFish, 0},
                       "quest-driven fishing then quit uses the same real request route");
            } else {
                expect(count(23016) == 1 && saved_quests() == expected_quests,
                       "actual restart preserves donation remainder and every saved quest flag");
                ask(61002);
                expect(route_replies(barrier()) == std::vector<std::string>{html(7002, 61002)},
                       "fresh Host without quest engine falls back from kind3 to NPC HTML");
                request(6205);
                expect(route_replies(barrier()).empty(),
                       "ordinary Quest without engine completes silently before a fresh barrier");
                request(server::kConsumableCraftingQuestIndex);
                const auto craft = barrier();
                expect(route_replies(craft).empty() && !chats(craft).empty(),
                       "recognized crafting still rejects before the null-engine gate");
                ask(61010);
                (void) barrier();
                request(6250);
                const auto donation = barrier();
                expect(count(23016) == 0 && route_replies(donation).empty() &&
                           !chats(donation).empty(),
                       "donation remains reachable before the null-engine guard after restart");
            }
            expect(
                saved_quests() == expected_quests,
                "crafting, donation and activity routing preserve the complete expected quest map");
            for (const auto& material : materials) {
                expect(count(material.first) == material.second,
                       "start/quit routing never completes or charges a cooking recipe");
            }
            expect(store->open(config.save_dir), "read durable routing identity and event");
            const auto character = store->load_character(config.character_id);
            const auto event = store->load_server_event("exp_event");
            expect(character && character->money == 10000 && character->map_x == 1056 &&
                       character->map_y == 1024 && character->map_z == 0,
                   "portal ACK cannot replace authoritative position or deferred cooking money");
            expect(event && event->active_bonus_percent == 10 &&
                       event->contribution_count == (with_engine ? 0 : 1),
                   "durable event tracks activation and post-restart progress exactly");
            store->close();
            session->close();
            host.stop();
        }
    }
}  // namespace

int main() {
    const auto root =
        openkal::test::temp_dir(("consumable_crafting_" + std::to_string(getpid())).c_str());
    for (const auto family : {openkal::server::ConsumableFamily::Medicine,
                              openkal::server::ConsumableFamily::MagicPotion,
                              openkal::server::ConsumableFamily::AutoMedicine}) {
        const auto prefix = root + "/" + std::to_string(static_cast<int>(family));
        run_case(prefix + "/success", false, false, false, false, family);
        run_case(prefix + "/full", true, false, false, false, family);
        run_case(prefix + "/failed_save", false, true, false, false, family);
        run_case(prefix + "/freed_slot", true, false, true, false, family);
        run_case(prefix + "/character_bound", false, false, false, true, family);
    }
    run_routing_case(root + "/routing");
    std::filesystem::remove_all(root);
    return openkal::test::g_failures ? 1 : 0;
}
