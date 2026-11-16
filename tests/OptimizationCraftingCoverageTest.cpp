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
#include "openkal/protocol/FrameParser.h"
#include "openkal/protocol/GameExit.h"
#include "openkal/protocol/Handshake.h"
#include "openkal/protocol/Opcodes.h"
#include "openkal/protocol/PacketReader.h"
#include "openkal/protocol/PacketWriter.h"
#include "openkal/protocol/Quest.h"
#include "openkal/protocol/Teleport.h"
#include "openkal/protocol/Transform.h"
#include "openkal/server/ConsumableCrafting.h"
#include "openkal/server/ServerHost.h"
#include "openkal/server/net/Transport.h"
#include "openkal/server/store/DataStore.h"
#include "test_util.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sqlite3.h>
#include <string>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

namespace {
    using namespace openkal;
    using test::expect;
    using Bytes = std::vector<std::uint8_t>;
    constexpr std::uint16_t kStateQuest = 9610;
    constexpr const char* kGuardText = "Choose a recipe while talking to the Medicine Alchemist.";

    struct Frame {
        std::uint8_t type = 0;
        Bytes payload;
    };

    struct SavedInventory {
        std::vector<std::vector<std::string>> rows;
        std::int64_t money = 0;
        bool operator==(const SavedInventory& other) const {
            return rows == other.rows && money == other.money;
        }
    };

    bool next_frame(server::ISession& session, Bytes& rx, Frame& frame) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        do {
            if (!session.recv_into(rx)) {
                return false;
            }
            std::size_t consumed = 0;
            const auto status = protocol::FrameParser::pop(
                rx.data(), rx.size(), consumed, frame.type, frame.payload);
            if (status == protocol::FrameParseStatus::Invalid) {
                return false;
            }
            if (status == protocol::FrameParseStatus::Complete) {
                rx.erase(rx.begin(), rx.begin() + static_cast<std::ptrdiff_t>(consumed));
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        } while (std::chrono::steady_clock::now() < deadline);
        return false;
    }

    void fixture(const std::string& path, const std::string& contents) {
        std::ofstream out(path);
        out << contents;
        expect(out.good(), "write a project-authored crafting guard fixture");
    }

    class CraftingWorld {
      public:
        explicit CraftingWorld(std::string root) : m_root(std::move(root)) {
            m_config.maps_dir = m_root + "/maps";
            m_config.monsters_dir = m_root + "/monsters";
            m_config.items_dir = m_root + "/items";
            m_config.npcs_dir = m_root + "/npcs";
            m_config.quests_dir = m_root + "/quests";
            m_config.mapinfo_path = m_root + "/mapinfo.json";
            m_config.save_dir = m_root + "/save";
            m_config.unix_path = m_root + "/game.sock";
            m_config.store = server::StoreBackend::Sqlite;
            m_config.transport = server::TransportKind::Unix;
            m_config.spawn_x = m_config.spawn_y = 512;
            m_config.spawn_z = 0;
            m_config.map_radius_cells = 0;
            m_config.max_monsters = 0;
            for (const auto& dir : {m_config.maps_dir,
                                    m_config.monsters_dir,
                                    m_config.items_dir,
                                    m_config.npcs_dir,
                                    m_config.quests_dir}) {
                test::mkdir_p(dir);
            }
            expect(test::write_open_area(m_config.maps_dir + "/n_000_000.akarea"),
                   "write owned walkable crafting guard terrain");
            fixture(
                m_config.monsters_dir + "/1.json",
                R"({"index":1,"name":"Unused owned definition","stats":{"level":1,"hp":1},"spawns":[]})");
            fixture(
                m_config.mapinfo_path,
                R"({"maps":[{"index":0,"kind":1,"xy":[0, 0]}, {"index":5,"kind":1,"xy":[0, 0]}]})");
            fixture(
                m_config.npcs_dir + "/1.json",
                R"({"index":1,"name":"Owned Alchemist","kind":3,"html":990300,"xy":[512, 512, 0],"shape":7})");
            fixture(m_config.npcs_dir + "/2.json",
                    R"({"index":2,"name":"Unrelated owned NPC","html":10,"xy":[512, 512, 0]})");
            fixture(
                m_config.npcs_dir + "/3.json",
                R"({"index":3,"name":"Distant owned Alchemist","html":990300,"xy":[712, 512, 0]})");
            for (int index : {23000, 23001, 23027}) {
                fixture(m_config.items_dir + "/" + std::to_string(index) + ".json",
                        "{\"index\":" + std::to_string(index) +
                            ",\"name\":\"Owned crafting "
                            "material\",\"class\":[\"general\",\"etc\"],\"stackable\":1}");
            }
            // These ordinary, project-owned quest actions change state through the
            // same packet route as gameplay; no session internals are modified.
            fixture(
                m_config.quests_dir + "/fish.json",
                R"({"index":9610,"flag":1,"cases":[{"then":[{"op":"gstate","gstate":5,"value":1}]}]})");
            fixture(
                m_config.quests_dir + "/cook.json",
                R"({"index":9610,"flag":2,"cases":[{"then":[{"op":"gstate","gstate":6,"value":1}, {"op":"html","id":961002}]}]})");
            fixture(
                m_config.quests_dir + "/pending.json",
                R"({"index":9610,"flag":3,"cases":[{"then":[{"op":"teleport","map":0,"x":512,"y":512}]}]})");
            fixture(
                m_config.quests_dir + "/other_map.json",
                R"({"index":9610,"flag":4,"cases":[{"then":[{"op":"teleport","map":5,"x":512,"y":512}]}]})");
            fixture(
                m_config.quests_dir + "/range.json",
                R"({"index":9610,"flag":5,"cases":[{"then":[{"op":"teleport","map":0,"x":640,"y":512}]}]})");
            fixture(
                m_config.quests_dir + "/home.json",
                R"({"index":9610,"flag":6,"cases":[{"then":[{"op":"teleport","map":0,"x":512,"y":512}]}]})");
            m_store = server::create_data_store(server::StoreBackend::Sqlite);
            expect(m_store && m_store->open(m_config.save_dir), "open owned crafting guard store");
            if (!m_store) {
                return;
            }
            server::CharacterRecord hero;
            hero.name = "Owned crafter";
            hero.hp = hero.mp = 100;
            hero.money = 321;
            hero.map_x = hero.map_y = 512;
            hero.map_z = 0;
            expect(m_store->ensure_default_player("owned", "owned", hero),
                   "create owned crafting guard character");
            const auto account = m_store->find_account("owned");
            if (!account) {
                return;
            }
            const auto characters = m_store->list_characters(account->id);
            if (characters.empty()) {
                return;
            }
            m_config.character_id = characters.front().id;
            for (const auto& item : m_store->load_items(m_config.character_id)) {
                expect(m_store->delete_item(item.iid), "clear owned starter rows before seeding");
            }
            for (int ingredient = 0; ingredient < 2; ++ingredient) {
                server::ItemRecord item;
                item.character_id = m_config.character_id;
                item.iid = 100 + ingredient;
                item.index = ingredient == 0 ? 23000 : 23027;
                item.count = 8;
                item.account_bound = ingredient == 1;
                item.character_bound = ingredient == 1;
                item.prefix = 4 + ingredient;
                expect(m_store->insert_item(item), "seed complete owned ingredient instances");
            }
            m_store->close();
        }

        ~CraftingWorld() {
            stop();
            std::filesystem::remove_all(m_root);
        }

        bool start() {
            if (!m_config.character_id || !m_host.start(m_config)) {
                expect(false, "start crafting guard server");
                return false;
            }
            for (int i = 0; i < 200 && !std::filesystem::exists(m_config.unix_path); ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            m_session = server::connect_unix(m_config.unix_path);
            expect(m_session != nullptr, "connect crafting guard client");
            if (!m_session) {
                return false;
            }
            Frame frame;
            expect(m_session->send_all(protocol::Handshake::build_connect(0)),
                   "send crafting guard connect");
            do {
                if (!next_frame(*m_session, m_rx, frame)) {
                    return false;
                }
            } while (frame.type != static_cast<std::uint8_t>(protocol::S2C::Code));
            protocol::S2CCodeFields challenge;
            if (!protocol::Handshake::parse_code(
                    frame.payload.data(), frame.payload.size(), challenge)) {
                return false;
            }
            expect(m_session->send_all(
                       protocol::Handshake::build_answer(challenge.protocol_test_value)),
                   "answer crafting guard challenge");
            do {
                if (!next_frame(*m_session, m_rx, frame)) {
                    return false;
                }
            } while (frame.type != static_cast<std::uint8_t>(protocol::S2C::CreatePlayer));
            const auto frames = round_trip({});
            m_alchemist = m_other = m_distant = 0;
            for (const auto& incoming : frames) {
                if (incoming.type != static_cast<std::uint8_t>(protocol::S2C::CreateNpc)) {
                    continue;
                }
                protocol::CreateNpcInfo npc;
                expect(protocol::Quest::parse_create_npc(
                           incoming.payload.data(), incoming.payload.size(), npc),
                       "decode owned crafting NPC");
                if (npc.index == 1) {
                    m_alchemist = npc.id;
                }
                if (npc.index == 2) {
                    m_other = npc.id;
                }
                if (npc.index == 3) {
                    m_distant = npc.id;
                }
            }
            expect(m_alchemist && m_other && m_distant, "discover every owned guard-test NPC");
            return m_alchemist && m_other && m_distant;
        }

        void stop() {
            if (m_session) {
                m_session->close();
            }
            m_session.reset();
            m_host.stop();
            m_rx.clear();
        }

        std::vector<Frame> round_trip(Bytes batch) {
            const auto barrier = protocol::GameExit::frame_client_request();
            batch.insert(batch.end(), barrier.begin(), barrier.end());
            expect(m_session->send_all(batch), "send owned request and persistence barrier");
            std::vector<Frame> frames;
            Frame frame;
            bool saved = false;
            bool reached = false;
            while (next_frame(*m_session, m_rx, frame)) {
                if (frame.type == static_cast<std::uint8_t>(protocol::S2C::AnsGameExit)) {
                    reached = protocol::GameExit::parse_server_answer(
                                  frame.payload.data(), frame.payload.size(), saved) &&
                              saved;
                    break;
                }
                frames.push_back(std::move(frame));
            }
            expect(reached && m_session->alive(),
                   "server processes through the persistence barrier and stays alive");
            return frames;
        }

        SavedInventory saved_inventory() const {
            SavedInventory result;
            sqlite3* raw = nullptr;
            const int opened =
                sqlite3_open_v2((m_config.save_dir + "/" + server::kSqliteFileName).c_str(),
                                &raw,
                                SQLITE_OPEN_READONLY,
                                nullptr);
            std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db(raw, sqlite3_close);
            expect(opened == SQLITE_OK, "read durable crafting guard inventory");
            if (opened != SQLITE_OK) {
                return result;
            }
            sqlite3_stmt* raw_statement = nullptr;
            expect(sqlite3_prepare_v2(db.get(),
                                      "SELECT * FROM items WHERE character_id=? ORDER BY iid",
                                      -1,
                                      &raw_statement,
                                      nullptr) == SQLITE_OK,
                   "read every persisted item attribute for the guard assertion");
            std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> statement(raw_statement,
                                                                                 sqlite3_finalize);
            if (!statement) {
                return result;
            }
            sqlite3_bind_int(statement.get(), 1, m_config.character_id);
            int status = SQLITE_OK;
            while ((status = sqlite3_step(statement.get())) == SQLITE_ROW) {
                std::vector<std::string> row;
                for (int column = 0; column < sqlite3_column_count(statement.get()); ++column) {
                    const auto* data =
                        static_cast<const char*>(sqlite3_column_blob(statement.get(), column));
                    const auto size =
                        static_cast<std::size_t>(sqlite3_column_bytes(statement.get(), column));
                    row.push_back(std::to_string(sqlite3_column_type(statement.get(), column)) +
                                  ":" + (data ? std::string(data, size) : std::string{}));
                }
                result.rows.push_back(std::move(row));
            }
            expect(status == SQLITE_DONE, "complete the durable inventory snapshot");
            statement.reset();
            raw_statement = nullptr;
            expect(sqlite3_prepare_v2(db.get(),
                                      "SELECT money FROM characters WHERE id=?",
                                      -1,
                                      &raw_statement,
                                      nullptr) == SQLITE_OK,
                   "read persisted crafting money");
            statement.reset(raw_statement);
            if (statement) {
                sqlite3_bind_int(statement.get(), 1, m_config.character_id);
                expect(sqlite3_step(statement.get()) == SQLITE_ROW,
                       "crafting character still exists");
                result.money = sqlite3_column_int64(statement.get(), 0);
            }
            return result;
        }

        void rejected(const Bytes& request, bool guard_message, const char* reason) {
            const auto before = saved_inventory();
            const auto frames = round_trip(request);
            int guards = 0;
            int mutations = 0;
            for (const auto& frame : frames) {
                if (frame.type == static_cast<std::uint8_t>(protocol::S2C::Chatting)) {
                    std::string speaker, text;
                    expect(protocol::Chatting::parse_s2c(
                               frame.payload.data(), frame.payload.size(), speaker, text),
                           "decode actual server crafting feedback");
                    if (text == kGuardText) {
                        ++guards;
                    }
                }
                if (frame.type == static_cast<std::uint8_t>(protocol::S2C::InsertItem) ||
                    frame.type == static_cast<std::uint8_t>(protocol::S2C::TrashItem) ||
                    frame.type == static_cast<std::uint8_t>(protocol::S2C::UpdateItemNum)) {
                    ++mutations;
                }
            }
            expect(guards == (guard_message ? 1 : 0) && mutations == 0 &&
                       saved_inventory() == before,
                   reason);
        }

        Bytes recipe(std::uint8_t flag = 1) const {
            return protocol::Quest::frame_quest(
                protocol::Quest::make_key(server::kConsumableCraftingQuestIndex, flag));
        }

        void authorize() {
            const auto frames = round_trip(protocol::Quest::frame_ask_npc(m_alchemist));
            bool shown = false;
            for (const auto& frame : frames) {
                std::int32_t html = 0;
                std::uint32_t npc = 0;
                if (frame.type == static_cast<std::uint8_t>(protocol::S2C::SendHtml) &&
                    protocol::Quest::parse_send_html(
                        frame.payload.data(), frame.payload.size(), html, &npc)) {
                    shown = shown ||
                            (html == server::kConsumableCraftingDialogId && npc == m_alchemist);
                }
            }
            expect(shown,
                   "the real alchemist conversation supplies current crafting authorization");
        }

        void unrelated_conversation(bool distant) {
            (void) round_trip(protocol::Quest::frame_ask_npc(distant ? m_distant : m_other));
        }

        void state(std::uint8_t flag, std::uint32_t expected) {
            const auto frames = round_trip(
                protocol::Quest::frame_quest(protocol::Quest::make_key(kStateQuest, flag)));
            bool observed = false;
            for (const auto& frame : frames) {
                // The current gstate action broadcasts only the fishing bit.
                // Cooking's following HTML action acknowledges quest execution;
                // the rejected craft then proves that its busy bit took effect.
                if (flag == 2 && frame.type == static_cast<std::uint8_t>(protocol::S2C::SendHtml)) {
                    std::int32_t html = 0;
                    if (protocol::Quest::parse_send_html(
                            frame.payload.data(), frame.payload.size(), html)) {
                        observed = observed || html == 961002;
                    }
                }
                if (frame.type != static_cast<std::uint8_t>(protocol::S2C::GState)) {
                    continue;
                }
                protocol::PacketReader reader(frame.payload);
                std::uint32_t id = 0, value = 0;
                if (reader.read_u32(id) && reader.read_u32(value)) {
                    observed = observed || (value & expected) == expected;
                }
            }
            expect(observed, "a real quest packet confirms the requested busy state");
        }

        void end_job() {
            protocol::PacketWriter request;
            (void) round_trip(request.frame(static_cast<std::uint8_t>(protocol::C2S::QuestJobEnd)));
        }

        void teleport(std::uint8_t flag, std::uint8_t map, int x, bool acknowledge) {
            const auto frames = round_trip(
                protocol::Quest::frame_quest(protocol::Quest::make_key(kStateQuest, flag)));
            bool observed = false;
            for (const auto& frame : frames) {
                if (frame.type != static_cast<std::uint8_t>(protocol::S2C::Teleport)) {
                    continue;
                }
                std::uint8_t destination = 0, cheat = 0;
                int px = 0, py = 0, pz = 0;
                if (protocol::Teleport::parse_server(frame.payload.data(),
                                                     frame.payload.size(),
                                                     destination,
                                                     px,
                                                     py,
                                                     pz,
                                                     cheat)) {
                    observed = observed || (destination == map && px == x && py == 512);
                }
            }
            expect(observed, "a real quest starts the exact pending teleport destination");
            if (acknowledge) {
                (void) round_trip(protocol::Teleport::frame_client_ack(1, 0));
            }
        }

        void acknowledge_teleport() {
            (void) round_trip(protocol::Teleport::frame_client_ack(1, 0));
        }

        void seed_dead_offline() {
            stop();
            expect(m_store->open(m_config.save_dir), "open offline dead-crafter fixture");
            const auto record = m_store->load_character(m_config.character_id);
            expect(record.has_value(), "load existing owned crafter before offline health change");
            if (record) {
                auto dead = *record;
                dead.hp = 0;
                expect(m_store->save_character(dead),
                       "persist zero health without touching items or money");
            }
            m_store->close();
        }

        void successful_recipe() {
            const auto before = saved_inventory();
            protocol::PacketWriter extended;
            extended.write_u32(protocol::Quest::make_key(server::kConsumableCraftingQuestIndex, 1));
            extended.write_u8(0xa5);
            const auto frames =
                round_trip(extended.frame(static_cast<std::uint8_t>(protocol::C2S::Quest)));
            expect(m_store->open(m_config.save_dir),
                   "inspect the successful guard-recovery exchange");
            const auto items = m_store->load_items(m_config.character_id);
            m_store->close();
            int medicine = 0, aloe = 0, output = 0;
            bool bound_output = false;
            for (const auto& item : items) {
                if (item.index == 23000) {
                    medicine += item.count;
                }
                if (item.index == 23027) {
                    aloe += item.count;
                }
                if (item.index == 23001) {
                    output += item.count;
                    bound_output = item.account_bound && item.character_bound;
                }
            }
            bool item_delta = false;
            for (const auto& frame : frames) {
                item_delta = item_delta ||
                             frame.type == static_cast<std::uint8_t>(protocol::S2C::InsertItem);
            }
            expect(medicine == 6 && aloe == 7 && output == 1 && bound_output && item_delta &&
                       saved_inventory().money == before.money,
                   "a trailing-byte quest still crafts exactly once after clearing guards, "
                   "preserves binding and money, and emits real item confirmation");
        }

      private:
        std::string m_root;
        server::ServerConfig m_config;
        std::unique_ptr<server::IDataStore> m_store;
        server::ServerHost m_host;
        std::unique_ptr<server::ISession> m_session;
        Bytes m_rx;
        std::uint32_t m_alchemist = 0, m_other = 0, m_distant = 0;
    };
}  // namespace

int main() {
    CraftingWorld world(
        test::temp_dir(("optimization_crafting_" + std::to_string(getpid())).c_str()));
    if (!world.start()) {
        return 1;
    }
    for (int bytes = 0; bytes < 4; ++bytes) {
        protocol::PacketWriter malformed;
        malformed.payload().assign(static_cast<std::size_t>(bytes), 0);
        world.rejected(malformed.frame(static_cast<std::uint8_t>(protocol::C2S::Quest)),
                       false,
                       "truncated quest payload has no recipe feedback, item deltas or durable "
                       "inventory/money effects");
    }
    world.rejected(world.recipe(),
                   true,
                   "a valid recipe without a conversation cannot mutate inventory or money");
    world.authorize();
    world.rejected(world.recipe(255),
                   true,
                   "unknown recipe flags cannot mutate complete persisted item instances or money");
    world.unrelated_conversation(false);
    world.rejected(
        world.recipe(), true, "talking to an unrelated NPC invalidates crafting authorization");
    world.authorize();
    world.unrelated_conversation(true);
    world.rejected(world.recipe(),
                   true,
                   "an out-of-range conversation cannot retain earlier alchemist authorization");
    world.authorize();
    world.state(1, protocol::kCgsFish);
    world.rejected(
        world.recipe(),
        true,
        "fishing blocks an otherwise authorized recipe without consuming materials or money");
    world.end_job();
    world.state(2, protocol::kCgsCooking);
    world.rejected(
        world.recipe(),
        true,
        "cooking blocks an otherwise authorized recipe without consuming materials or money");
    world.end_job();
    world.teleport(3, 0, 512, false);
    world.rejected(
        world.recipe(),
        true,
        "pending teleport blocks an otherwise authorized recipe without durable effects");
    world.acknowledge_teleport();
    world.teleport(4, 5, 512, true);
    world.rejected(world.recipe(),
                   true,
                   "a completed map change invalidates the retained alchemist authorization");
    world.teleport(6, 0, 512, true);
    world.authorize();
    world.teleport(5, 0, 640, true);
    world.rejected(world.recipe(),
                   true,
                   "leaving NPC reply range invalidates an otherwise live same-map authorization");
    world.teleport(6, 0, 512, true);
    world.authorize();
    world.successful_recipe();
    world.seed_dead_offline();
    if (!world.start()) {
        return 1;
    }
    world.authorize();
    world.rejected(
        world.recipe(), true, "a dead authorized crafter cannot consume materials or money");
    return test::g_failures ? 1 : 0;
}
