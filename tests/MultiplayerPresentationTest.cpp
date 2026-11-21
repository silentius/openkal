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
#include "openkal/client/NetClient.h"
#include "openkal/client/OverheadChat.h"
#include "openkal/client/Player.h"
#include "openkal/client/RemotePlayerMotion.h"
#include "openkal/map/Coords.h"
#include "openkal/protocol/Attack.h"
#include "openkal/protocol/Chatting.h"
#include "openkal/protocol/Effect.h"
#include "openkal/protocol/Emote.h"
#include "openkal/protocol/FrameParser.h"
#include "openkal/protocol/HarvestPlant.h"
#include "openkal/protocol/ItemMonster.h"
#include "openkal/protocol/Move.h"
#include "openkal/protocol/Pvp.h"
#include "openkal/protocol/Quest.h"
#include "openkal/protocol/Skill.h"
#include "openkal/protocol/Teleport.h"
#include "openkal/server/Packets.h"
#include "openkal/server/ServerHost.h"
#include "test_util.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <memory>
#include <thread>
#include <unistd.h>
#include <utility>

namespace {
    class BufferedSession final : public openkal::server::ISession {
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
        bool send_all(const std::uint8_t* data, std::size_t size) override {
            sent.insert(sent.end(), data, data + size);
            return m_alive;
        }
        std::vector<std::uint8_t> sent;
        bool recv_into(std::vector<std::uint8_t>& buffer) override {
            buffer.insert(buffer.end(), m_pending.begin(), m_pending.end());
            m_pending.clear();
            return m_alive;
        }
        void queue(const std::vector<std::uint8_t>& frame) {
            m_pending.insert(m_pending.end(), frame.begin(), frame.end());
        }

      private:
        bool m_alive = true;
        std::vector<std::uint8_t> m_pending;
    };

    std::vector<std::uint8_t> create(std::uint32_t id, bool local, int x, int y) {
        const std::uint16_t wear[7]{};
        return openkal::protocol::CreatePlayer::frame(
            id, local ? "Alice" : "Bob", local ? 0x80 : 0, x, y, 0, 0, 0, wear, 0, 0);
    }

    void thief_chest_test() {
        using namespace openkal;
        using test::expect;
        const auto root = test::temp_dir(("thief_chest_" + std::to_string(getpid())).c_str());
        server::ServerConfig config;
        config.maps_dir = root + "/maps";
        config.monsters_dir = root + "/monsters";
        config.items_dir = root + "/items";
        config.save_dir = root + "/save";
        config.unix_path = root + "/game.sock";
        config.spawn_x = 480;
        config.spawn_y = 480;
        config.spawn_z = 0;
        config.player_clazz = 3;
        config.max_monsters = 1;
        for (const auto& dir : {config.maps_dir, config.monsters_dir, config.items_dir}) {
            test::mkdir_p(dir);
        }
        std::ofstream(config.items_dir + "/1.json")
            << R"({"index":1,"name":"Test item","class":["general","etc"]})";
        expect(test::write_open_area(config.maps_dir + "/n_000_000.akarea"),
               "write owned chest map");
        std::ofstream(config.monsters_dir + "/293.json")
            << R"({"index":293,"name":"Wooden Box","stats":{"race":4,"level":1,"hp":1},"spawns":[{"map":0,"max":1,"cycle":1400,"rect":[15, 15, 16, 16]}]})";
        server::ServerHost host;
        client::NetClient client;
        client::NetClient observer;
        const auto cleanup = [&] {
            observer.close();
            client.close();
            host.stop();
            std::filesystem::remove_all(root);
        };
        const bool started = host.start(config);
        expect(started, "start thief chest server");
        if (!started) {
            cleanup();
            return;
        }
        for (int n = 0; n < 200 && !std::filesystem::exists(config.unix_path); ++n) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        auto session = server::connect_unix(config.unix_path);
        expect(session != nullptr, "connect thief chest client");
        if (!session) {
            cleanup();
            return;
        }
        client.adopt_session(std::move(session));
        const bool handshook = client.handshake();
        expect(handshook, "handshake thief chest client");
        if (!handshook) {
            cleanup();
            return;
        }
        for (int n = 0;
             n < 100 && (!client.server_clock().ready() || client.local_player_id() == 0);
             ++n) {
            client.update(0, 32);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        const bool ready = client.server_clock().ready() && client.local_player_id() != 0;
        expect(ready, "server sends initial clock and player identity after entry");
        if (!ready) {
            cleanup();
            return;
        }
        auto observer_session = server::connect_unix(config.unix_path);
        expect(observer_session != nullptr, "connect emote observer");
        if (!observer_session) {
            cleanup();
            return;
        }
        observer.adopt_session(std::move(observer_session));
        const bool observer_handshook = observer.handshake();
        expect(observer_handshook, "handshake emote observer");
        if (!observer_handshook) {
            cleanup();
            return;
        }
        const auto wait_until = [&](const auto& condition, int milliseconds = 2000) {
            const auto deadline =
                std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
            do {
                client.update(0.01f, 32);
                observer.update(0.01f, 32);
                if (condition()) {
                    return true;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            } while (std::chrono::steady_clock::now() < deadline);
            return false;
        };
        const bool observer_ready = wait_until(
            [&] { return observer.server_clock().ready() && observer.local_player_id() != 0; });
        expect(observer_ready, "emote observer enters the world before the first broadcast");
        if (!observer_ready) {
            cleanup();
            return;
        }
        expect(wait_until([&] { return !client.monsters().empty(); }), "receive Wooden Box");
        if (client.monsters().empty()) {
            cleanup();
            return;
        }
        for (int motion = 1; motion <= protocol::kEmoteCount; ++motion) {
            if (motion > 1) {
                std::this_thread::sleep_for(std::chrono::milliseconds(260));
            }
            expect(client.send_emote(static_cast<std::uint8_t>(motion)), "send emote to server");
            protocol::PlayerEmote event;
            expect(wait_until([&] { return client.take_emote(event); }) &&
                       event.player_id == client.local_player_id() && event.motion == motion,
                   "server validates all twenty emotes and supplies the caster identity");
            protocol::PlayerEmote remote;
            expect(wait_until([&] { return observer.take_emote(remote); }) &&
                       remote.player_id == client.local_player_id() && remote.motion == motion,
                   "nearby observer receives every emote from the emitting player");
        }
        expect(client.send_rest(true), "sit before a blocked emote request");
        expect(wait_until([&] { return client.entity_resting(client.local_player_id()); }),
               "server confirms resting state");
        std::this_thread::sleep_for(std::chrono::milliseconds(260));
        expect(client.send_emote(1), "send emote while resting");
        protocol::PlayerEmote blocked;
        expect(!wait_until(
                   [&] { return client.take_emote(blocked) || observer.take_emote(blocked); }, 150),
               "resting character cannot broadcast emotes");
        expect(client.send_rest(false), "stand after blocked emote");
        expect(wait_until([&] { return !client.entity_resting(client.local_player_id()); }),
               "server confirms standing state");
        const auto box = client.monsters().begin()->second;
        client.flush_local_move(box.wx(), 0.0f, box.wz(), 32);
        const auto begin = [&] {
            expect(client.send_get_item_monster(box.id()), "request chest opening");
            // This correction used to send a vertical MoveEnd immediately after GET.
            client.sync_local_move(box.wx(), 0.1f, box.wz(), false, 0.1f, 32, true);
            std::uint8_t seconds = 0;
            return wait_until([&] { return client.take_chest_open_start(seconds); }) &&
                   seconds == 6;
        };
        expect(begin(), "Thief begins the six-second opening timer");
        expect(client.send_cancel_item_monster(box.id()), "explicit movement can cancel opening");
        expect(wait_until([&] { return client.take_chest_open_stop(); }) &&
                   client.last_chest_feedback_msg() ==
                       static_cast<std::uint8_t>(protocol::ItemMonsterMsg::Canceled),
               "server acknowledges intentional chest cancellation");
        client.flush_local_move(box.wx(), 0.1f, box.wz(), 32);
        expect(begin(), "Thief can reopen the canceled box");
        expect(wait_until(
                   [&] {
                       client.sync_local_move(
                           box.wx() + 1.0f, 0.2f, box.wz(), false, 0.01f, 32, true);
                       return client.take_chest_open_stop();
                   },
                   8000) &&
                   client.last_chest_feedback_msg() ==
                       static_cast<std::uint8_t>(protocol::ItemMonsterMsg::Opened),
               "idle height corrections cannot cancel the Thief's chest timer");
        expect(client.server_px() == box.x(), "opening preserves the movement baseline");
        client.sync_local_move(box.wx() + 1.0f, 0.2f, box.wz(), false, 0.1f, 32);
        expect(client.server_px() == coords::to_server(box.wx() + 1.0f, 32),
               "movement synchronization resumes after opening");
        cleanup();
    }
    void harvest_test() {
        using namespace openkal;
        using test::expect;
        const auto root = test::temp_dir(("harvest_network_" + std::to_string(getpid())).c_str());
        server::ServerConfig config;
        config.maps_dir = root + "/maps";
        config.monsters_dir = root + "/monsters";
        config.items_dir = root + "/items";
        config.save_dir = root + "/save";
        config.unix_path = root + "/game.sock";
        config.spawn_x = 480;
        config.spawn_y = 480;
        config.spawn_z = 0;
        config.max_monsters = 3;
        server::HarvestTypeConfig aloe;
        aloe.type = "aloe_vera";
        aloe.entity_index = protocol::kAloePlantIndex;
        aloe.item_index = protocol::kAloeItemIndex;
        aloe.items_per_pick = 3;
        config.plants.types.push_back(aloe);
        auto herb = aloe;
        herb.type = "test_herb";
        herb.entity_index = 65001;
        herb.item_index = 9991;
        herb.items_per_pick = 5;
        config.plants.types.push_back(herb);
        for (const auto& dir : {config.maps_dir, config.monsters_dir, config.items_dir}) {
            test::mkdir_p(dir);
        }
        std::ofstream(config.items_dir + "/23027.json")
            << R"({"index":23027,"name":"Aloe Vera","class":["general","etc"],"stackable":1})";
        expect(test::write_open_area(config.maps_dir + "/n_000_000.akarea"), "write gathering map");
        // A fixed fixture placement isolates interaction from the separately tested random spawner.
        std::ofstream(config.monsters_dir + "/65000.json")
            << R"({"index":65000,"name":"Aloe Vera","stats":{"race":4,"level":1,"hp":1},"spawns":[{"map":0,"max":2,"cycle":600,"rect":[15, 15, 16, 16]}]})";
        std::ofstream(config.monsters_dir + "/65001.json")
            << R"({"index":65001,"name":"Owned test herb","stats":{"race":4,"level":1,"hp":1},"spawns":[{"map":0,"max":1,"cycle":600,"rect":[15, 15, 16, 16]}]})";
        std::ofstream(config.items_dir + "/9991.json")
            << R"({"index":9991,"name":"Owned test ingredient","class":["general","etc"],"stackable":1})";
        std::ofstream(config.items_dir + "/9000.json")
            << R"({"index":9000,"name":"Bag filler","class":["general","etc"],"stackable":0})";
        server::ServerHost host;
        expect(host.start(config), "start gathering server");
        client::NetClient collector, observer;
        const auto connect = [&](client::NetClient& client) {
            for (int n = 0; n < 200 && !std::filesystem::exists(config.unix_path); ++n) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            auto session = server::connect_unix(config.unix_path);
            if (!session) {
                return false;
            }
            client.adopt_session(std::move(session));
            return client.handshake();
        };
        expect(connect(collector) && connect(observer), "two players enter gathering world");
        const auto wait = [&](const auto& condition, int milliseconds = 3000) {
            const auto deadline =
                std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
            do {
                collector.update(0.01f, 32);
                observer.update(0.01f, 32);
                if (condition()) {
                    return true;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            } while (std::chrono::steady_clock::now() < deadline);
            return false;
        };
        expect(wait([&] { return !collector.monsters().empty() && !observer.monsters().empty(); }),
               "both players see the same plant");
        if (collector.monsters().empty()) {
            host.stop();
            return;
        }
        const auto aloe_entity = std::find_if(
            collector.monsters().begin(), collector.monsters().end(), [](const auto& entry) {
                return entry.second.index() == protocol::kAloePlantIndex;
            });
        expect(aloe_entity != collector.monsters().end(), "receive the Aloe Vera type");
        if (aloe_entity == collector.monsters().end()) {
            host.stop();
            return;
        }
        const auto plant = aloe_entity->second;
        collector.flush_local_move(plant.wx(), 0, plant.wz(), 32);
        observer.flush_local_move(plant.wx(), 0, plant.wz(), 32);
        expect(collector.send_get_item_monster(plant.id()), "request gathering");
        std::uint8_t seconds = 0;
        expect(wait([&] { return collector.take_chest_open_start(seconds); }) && seconds == 2,
               "short gathering timer");
        expect(wait([&] {
                   return (observer.player_gstate(collector.local_player_id()) &
                           protocol::kCgsHarvestPlant) != 0;
               }),
               "observer receives single-cycle gathering state");
        expect(observer.send_get_item_monster(plant.id()), "competing gather request");
        expect(wait([&] {
                   return observer.last_chest_feedback_msg() ==
                          static_cast<std::uint8_t>(protocol::ItemMonsterMsg::Used);
               }),
               "plant is reserved for one collector");
        // Duplicate GET must not produce a second reward or restart the timer.
        expect(collector.send_get_item_monster(plant.id()), "duplicate gather request");
        client::PendingInsertItem reward;
        expect(wait([&] { return collector.take_insert_item(reward); }, 4000),
               "gathering inserts an inventory item directly");
        expect(reward.index == protocol::kAloeItemIndex && reward.count == 3,
               "configured three-item Aloe Vera harvest");
        expect(wait([&] {
                   return collector.monsters().find(plant.id()) == collector.monsters().end() &&
                          observer.monsters().find(plant.id()) == observer.monsters().end();
               }),
               "harvested plant disappears for both players");
        expect(collector.ground_items().empty(), "gathering creates no ground loot");
        expect(!collector.take_insert_item(reward) && !observer.take_insert_item(reward),
               "duplicate requests cannot duplicate the ingredient");
        const auto herb_entity =
            std::find_if(collector.monsters().begin(),
                         collector.monsters().end(),
                         [](const auto& entry) { return entry.second.index() == 65001; });
        expect(herb_entity != collector.monsters().end(), "receive the second plant type");
        if (herb_entity != collector.monsters().end()) {
            const auto second = herb_entity->second;
            collector.flush_local_move(second.wx(), 0, second.wz(), 32);
            expect(collector.send_get_item_monster(second.id()), "gather the second plant type");
            expect(wait([&] { return collector.take_insert_item(reward); }, 4000) &&
                       reward.index == 9991 && reward.count == 5,
                   "second type awards its own configured item and amount");
        }
        expect(observer.send_chat("/getitemid 9000 0 72"),
               "fill the second player's base inventory");
        int fillers = 0;
        expect(wait([&] {
                   while (observer.take_insert_item(reward)) {
                       if (reward.index == 9000) {
                           ++fillers;
                       }
                   }
                   return fillers == 72;
               }),
               "receive every inventory filler");
        expect(!observer.monsters().empty(), "second plant remains available");
        if (!observer.monsters().empty()) {
            const auto remaining = observer.monsters().begin()->second;
            observer.flush_local_move(remaining.wx(), 0, remaining.wz(), 32);
            (void) observer.take_chest_open_stop();
            expect(observer.send_get_item_monster(remaining.id()),
                   "attempt gathering with full inventory");
            expect(wait([&] { return observer.take_chest_open_start(seconds); }),
                   "full inventory gathering request acknowledged");
            expect(wait([&] { return observer.take_chest_open_stop(); }, 4000) &&
                       observer.last_chest_feedback_msg() ==
                           static_cast<std::uint8_t>(protocol::ItemMonsterMsg::Canceled),
                   "full inventory receives no success response");
            expect(observer.monsters().find(remaining.id()) != observer.monsters().end(),
                   "failed gathering preserves the plant");
            expect(!observer.take_insert_item(reward), "full inventory receives no ingredient");
        }
        collector.close();
        observer.close();
        host.stop();
        std::ofstream(config.monsters_dir + "/65000.json")
            << R"({"index":65000,"name":"Aloe Vera","stats":{"race":4,"level":1,"hp":1},"spawns":[]})";
        std::ofstream(config.monsters_dir + "/65001.json")
            << R"({"index":65001,"name":"Owned test herb","stats":{"race":4,"level":1,"hp":1},"spawns":[]})";
        config.mapinfo_path = root + "/mapinfo.json";
        std::ofstream(config.mapinfo_path) << R"({"maps":[{"index":0,"kind":1,"xy":[0, 0]}]})";
        config.plants.types.front().enabled = true;
        config.plants.types.front().max_plants = 2;
        config.plants.types.front().plants_per_area = 1;
        expect(host.start(config), "restart with server-controlled random plants");
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (host.monster_snapshot().empty() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        expect(host.monster_snapshot().size() == 1,
               "server area limit controls the random population");
        host.stop();
        std::string many_maps = R"({"maps":[)";
        for (int cy = 0; cy < 6; ++cy) {
            for (int cx = 0; cx < 6; ++cx) {
                if (cx != 0 || cy != 0) {
                    many_maps += ',';
                    std::filesystem::copy_file(config.maps_dir + "/n_000_000.akarea",
                                               config.maps_dir + "/n_00" + std::to_string(cx) +
                                                   "_00" + std::to_string(cy) + ".akarea");
                }
                many_maps += R"({"index":0,"kind":1,"xy":[)" + std::to_string(cx) + ", " +
                             std::to_string(cy) + "]}";
            }
        }
        many_maps += "]}";
        std::ofstream(config.mapinfo_path) << many_maps;
        config.map_radius_cells = 5;
        config.plants.types.front().max_plants = 2560;
        config.plants.types.front().plants_per_area = 80;
        config.plants.types.front().respawn_seconds = 1;
        expect(host.start(config), "start a large embedded gathering population");
        // Connect while the first population is being placed, as the game does after loading.
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        expect(connect(collector),
               "normal client handshake succeeds during large plant generation");
        expect(wait([&] { return collector.local_player_id() != 0; }),
               "player enters the world while plant generation continues");
        collector.close();
        host.stop();
        config.plants.types.front().enabled = false;
        expect(host.start(config), "restart with gathering disabled");
        expect(host.monster_snapshot().empty(), "disabled server config creates no plants");
        host.stop();
        std::filesystem::remove_all(root);
    }
}  // namespace

int main() {
    {
        using namespace openkal;
        using test::expect;
        auto session = std::make_unique<BufferedSession>();
        auto* packets = session.get();
        packets->queue(server::frame_s2c_code(1234));
        client::NetClient viewer;
        viewer.set_read_only(true);
        viewer.adopt_session(std::move(session));
        expect(viewer.handshake(), "read-only view completes the ordinary session handshake");
        expect(!viewer.observed_pose_ready(),
               "a handshake challenge alone cannot publish a zero-coordinate spectator pose");
        packets->queue(create(77, true, 1024, 1024));
        viewer.update(0, 32);
        expect(viewer.observed_pose_ready(),
               "the delayed self snapshot establishes the spectator's authoritative coordinates");
        protocol::CreateNpcInfo dialogNpc;
        dialogNpc.id = 812;
        dialogNpc.index = 1;
        dialogNpc.x = dialogNpc.y = 1024;
        packets->queue(protocol::Quest::frame_create_npc(dialogNpc));
        packets->queue(protocol::Quest::frame_send_html(100, dialogNpc.id));
        viewer.update(0, 32);
        std::int32_t html = 0;
        std::uint32_t sourceNpc = 0;
        expect(viewer.take_send_html(html, &sourceNpc) && html == 100 &&
                   sourceNpc == dialogNpc.id && viewer.last_asked_npc_id() == 0 &&
                   viewer.npcs().count(dialogNpc.id) == 1,
               "passive viewers receive dialog ownership without sending an NPC request");
        packets->sent.clear();
        expect(!viewer.send_attack_monster(900) && !viewer.send_use_item(1) &&
                   !viewer.send_puton_item(1) && !viewer.send_quest(10, 1) &&
                   !viewer.send_teleport_request(1),
               "read-only network client rejects combat, consumables, equipment, quests and "
               "teleport requests");
        viewer.sync_local_move(coords::to_client(1200), 0, coords::to_client(1300), true, 1, 32);
        expect(packets->sent.empty(),
               "read-only movement synchronization emits no gameplay packets");
        client::FloorField floor;
        terrain::HeightPatch patch;
        patch.patch_x = patch.patch_y = 0;
        patch.heights.assign(static_cast<std::size_t>(map::kHeightSamples) * map::kHeightSamples,
                             1000);
        floor.add_patch(std::move(patch));
        client::LocalPlayer observed;
        const auto present = [&] {
            const auto& motion = viewer.observed_motion();
            observed.apply_observed_pose(motion.position(), motion.yaw(), motion.moving(), floor);
        };
        present();
        expect(observed.pos().y == 1000 && viewer.observed_motion().position().y == 0 &&
                   observed.pos().x == coords::to_client(1024) && packets->sent.empty(),
               "attaching below terrain grounds only the displayed feet without changing server "
               "pose or sending input");
        const float initialX = coords::to_client(1024);
        packets->queue(protocol::Move::frame_player_on(77, 8, -8, 10));
        viewer.update(0, 32);
        int x = 0, y = 0, z = 0;
        expect(viewer.server_px() == 1032 && viewer.server_py() == 1016 &&
                   viewer.observed_motion().position().x == initialX && viewer.observed_moving() &&
                   !viewer.take_move_before(x, y, z),
               "spectator keeps the confirmed baseline separate from its interpolated pose");
        viewer.update(0.025f, 32);
        const auto& partial = viewer.observed_motion().position();
        expect(partial.x > initialX && partial.x < coords::to_client(1032) && partial.y > 0 &&
                   partial.y < coords::to_client_y(10),
               "spectator interpolates horizontal and vertical movement between packets");
        present();
        expect(observed.pos().y == 1000 && observed.pos().x == partial.x && observed.moving(),
               "observed movement keeps interpolated horizontal motion above the loaded floor");
        packets->queue(protocol::Move::frame_player_end(77, 8, 0, 0));
        viewer.update(0, 32);
        expect(viewer.observed_moving(),
               "spectator finishes the last segment before selecting idle");
        viewer.update(0.1f, 32);
        expect(viewer.observed_motion().position().x == coords::to_client(1040) &&
                   !viewer.observed_moving(),
               "spectator stops at the confirmed endpoint without overshooting");
        bool continuous = true;
        for (int frame = 0; frame < 120; ++frame) {
            if (frame % 6 == 0) {
                packets->queue(protocol::Move::frame_player_on(77, 2, 0, 0));
            }
            const float previous = viewer.observed_motion().position().x;
            viewer.update(1.0f / 120.0f, 32);
            continuous &=
                viewer.observed_motion().position().x > previous && viewer.observed_moving();
        }
        expect(continuous && packets->sent.empty(),
               "confirmed 20 Hz self movement is smooth at 120 Hz and sends no player input");
        packets->queue(protocol::Move::frame_before(77, 1024, 1024, 0, 0));
        viewer.update(0, 32);
        expect(viewer.take_move_before(x, y, z) && x == 1024 && z == 0 &&
                   viewer.observed_motion().position().x == initialX && !viewer.observed_moving(),
               "small server corrections snap immediately and cancel the old walk segment");
        packets->queue(protocol::Move::frame_player_on(77, 2, 0, 0));
        viewer.update(0.01f, 32);
        packets->queue(protocol::Quest::frame_send_html(101, dialogNpc.id));
        packets->queue(protocol::Teleport::frame_server(1, 2000, 2100, 300, 0));
        viewer.update(0, 32);
        expect(viewer.server_px() == 2000 &&
                   viewer.observed_motion().position().x == coords::to_client(2000) &&
                   viewer.observed_motion().position().y == coords::to_client_y(300) &&
                   !viewer.observed_moving(),
               "teleports reset the spectator baseline and interpolation immediately");
        expect(viewer.npcs().empty() && !viewer.take_send_html(html, &sourceNpc),
               "teleports discard stale NPCs and queued conversations from the previous sight");
        present();
        expect(observed.pos().y == 1000 && observed.pos().x == coords::to_client(2000),
               "teleport presentation applies the same standing surface without restoring stale "
               "depth");
        packets->queue(protocol::Move::frame_before(77, 2000, 2100, 10050, 0));
        viewer.update(0, 32);
        present();
        expect(observed.pos().y == 1005 && packets->sent.empty(),
               "confirmed airborne heights are preserved above terrain without spectator height "
               "corrections");
        client::FloorField missingFloor;
        observed.apply_observed_pose({16, 12, 16}, 0, false, missingFloor);
        expect(observed.pos().y == 12, "unloaded geometry preserves the confirmed height");
        client::FloorField indoor;
        const auto deck = [](float height) {
            client::FloorProp prop;
            for (const auto& point : {engine::Vec3{0, height, 0},
                                      {0, height, 32},
                                      {32, height, 0},
                                      {32, height, 0},
                                      {0, height, 32},
                                      {32, height, 32}}) {
                prop.add_vertex(point.x, point.y, point.z);
            }
            prop.pad_bounds_xz(1);
            return prop;
        };
        indoor.add_deck(deck(3));
        indoor.add_deck(deck(30));
        observed.apply_observed_pose({16, 0, 16}, 0, false, indoor);
        expect(observed.pos().y == 3,
               "indoor grounding selects the nearby standing surface and never raises the observer "
               "onto a roof");
        packets->queue(protocol::Attack::frame_server(77, 900, 0, 0, protocol::kAtkAnim));
        packets->queue(protocol::Skill::frame_action_skill(77, 3, 900));
        packets->queue(
            protocol::Skill::frame_server_damage(3, 77, 900, 1, 5, 0, protocol::kAtkHit));
        viewer.update(0, 32);
        client::PendingPlayerAttack attack;
        client::PendingSkillAction skill;
        expect(viewer.take_player_attack(attack) && attack.attacker_id == 77,
               "read-only self attacks reach the normal presentation queue");
        expect(viewer.take_skill_action(skill) && !skill.fire && skill.caster_id == 77 &&
                   viewer.take_skill_action(skill) && skill.fire && skill.target_id == 900,
               "read-only self skill preparation and fire retain server order");
    }
    using openkal::protocol::Move;
    using openkal::test::expect;
    {
        using namespace openkal;
        client::NetClient client;
        auto session = std::make_unique<BufferedSession>();
        auto* packets = session.get();
        client.adopt_session(std::move(session));
        for (int motion = 1; motion <= protocol::kEmoteCount; ++motion) {
            auto request = protocol::Emote::frame_client(static_cast<std::uint8_t>(motion));
            std::uint8_t decoded = 0;
            expect(request.size() == 4 &&
                       protocol::Emote::parse_client(
                           request.data() + 3, request.size() - 3, decoded) &&
                       decoded == motion,
                   "emote request round-trips every motion");
            expect(client.send_emote(decoded), "NetClient sends a valid emote request");
            packets->queue(protocol::Emote::frame_server(72, decoded));
        }
        client.update(0, 32);
        for (int motion = 1; motion <= protocol::kEmoteCount; ++motion) {
            protocol::PlayerEmote event;
            expect(client.take_emote(event) && event.player_id == 72 && event.motion == motion,
                   "remote emotes preserve their order, motion and authoritative caster");
        }
        const std::uint8_t invalid[] = {21, 1};
        std::uint8_t motion = 0;
        protocol::PlayerEmote event;
        expect(!protocol::Emote::parse_client(nullptr, 1, motion) &&
                   !protocol::Emote::parse_client(invalid, 1, motion) &&
                   !protocol::Emote::parse_client(invalid + 1, 0, motion) &&
                   !protocol::Emote::parse_client(invalid + 1, 2, motion) &&
                   !client.send_emote(0) && !client.send_emote(21),
               "malformed requests and unknown motions are rejected");
        auto frame = protocol::Emote::frame_server(72, 1);
        expect(!protocol::Emote::parse_server(frame.data() + 3, 4, event) &&
                   !protocol::Emote::parse_server(frame.data() + 3, 6, event) &&
                   protocol::Emote::frame_server(0, 1).empty(),
               "notifications require an exact payload and a real caster");
        packets->queue(frame);
        client.update(0, 32);
        client.close();
        expect(!client.take_emote(event), "disconnect clears pending emote presentation");
    }
    thief_chest_test();
    harvest_test();
    {
        using openkal::protocol::Quest;
        openkal::client::NetClient quests;
        auto session = std::make_unique<BufferedSession>();
        auto* packets = session.get();
        quests.adopt_session(std::move(session));
        const auto save = [&](std::uint16_t id, std::uint8_t flag, std::uint8_t clear) {
            packets->queue(Quest::frame_save_quest({id, flag, clear}));
            quests.update(0.0f, 32);
        };

        packets->queue(Quest::frame_quest_info({{11, 1, 0}, {12, 0, 0}, {13, 2, 1}}));
        quests.update(0.0f, 32);
        expect(quests.take_quest_flags_dirty(), "login snapshot refreshes the quest journal");
        expect(!quests.take_quest_state_changed() && !quests.take_quest_accepted(),
               "login snapshot does not trigger quest notification sounds");

        save(14, 1, 0);
        expect(quests.take_quest_state_changed() && quests.take_quest_accepted(),
               "server confirmation of a new active quest triggers acceptance");
        expect(!quests.take_quest_accepted(), "quest acceptance is consumed once");
        save(14, 1, 0);
        expect(!quests.take_quest_accepted(), "duplicate confirmation does not repeat acceptance");
        save(14, 2, 0);
        expect(quests.take_quest_state_changed() && !quests.take_quest_accepted(),
               "quest progress remains an update rather than an acceptance");
        save(14, 2, 1);
        expect(!quests.take_quest_accepted(), "quest completion is not acceptance");
        save(15, 1, 1);
        expect(!quests.take_quest_accepted(), "new completed entry is not acceptance");
        save(12, 1, 0);
        expect(quests.take_quest_accepted(), "starting an inactive snapshot entry is acceptance");
        save(13, 1, 0);
        expect(quests.take_quest_accepted(), "restarting a cleared quest is acceptance");

        packets->queue(Quest::frame_save_quest({16, 1, 0}));
        packets->queue(Quest::frame_save_quest({16, 2, 0}));
        quests.update(0.0f, 32);
        expect(quests.take_quest_accepted() && !quests.take_quest_accepted(),
               "same-frame progress preserves one pending acceptance notification");

        save(17, 1, 0);
        quests.close();
        expect(!quests.take_quest_accepted() && !quests.take_quest_state_changed() &&
                   quests.quest_flags().empty(),
               "closing a session clears quest state and pending notifications");
    }
    openkal::client::NetClient client;
    auto session = std::make_unique<BufferedSession>();
    auto* input = session.get();
    client.adopt_session(std::move(session));
    input->queue(create(2, false, 1000, 2000));
    input->queue(Move::frame_player_on(2, 10, -20, 0));
    client.update(0.0f, 32);
    expect(client.local_player_id() == 0, "remote movement cannot claim the local identity");
    expect(client.remote_player_positions().at(2).x == 1010 &&
               client.remote_player_positions().at(2).y == 1980 &&
               client.remote_player_positions().at(2).moving,
           "apply remote movement after spawn in one batch");
    input->queue(Move::frame_before(2, 1010, 1980, 0, 0));
    client.update(0.0f, 32);
    expect(client.local_player_id() == 0,
           "remote correction before hero creation cannot claim the local identity");
    input->queue(create(1, true, 3000, 4000));
    input->queue(Move::frame_player_on(1, 50, 50, 0));
    input->queue(Move::frame_player_on(2, -4, 7, 0));
    input->queue(Move::frame_player_end(2, 1, -2, 0));
    client.update(0.0f, 32);
    expect(client.local_player_id() == 1 && client.remote_player_positions().size() == 1,
           "self echoes leave remote baselines unchanged");
    expect(client.remote_player_positions().at(2).x == 1007 &&
               client.remote_player_positions().at(2).y == 1985 &&
               !client.remote_player_positions().at(2).moving,
           "accumulate signed deltas and stop on MoveEnd");
    input->queue(Move::frame_before(2, 800, 900, 0, 0));
    client.update(0.0f, 32);
    expect(client.remote_player_positions().at(2).x == 800 &&
               client.remote_player_positions().at(2).y == 900,
           "apply absolute remote correction");
    input->queue(Move::frame_before(1, 3000, 4000, 12340, 0));
    input->queue(Move::frame_before(2, 800, 900, 500, 0));
    input->queue(Move::frame_player_on(2, 0, 0, -25));
    client.update(0.0f, 32);
    int snap_x = 0, snap_y = 0, snap_z = 0;
    expect(client.take_move_before(snap_x, snap_y, snap_z) && snap_z == 12340,
           "local corrections preserve authoritative height");
    expect(client.remote_player_positions().at(2).z == 475,
           "remote corrections and movement preserve authoritative height");
    input->queue(openkal::server::frame_s2c_code(123));
    expect(client.handshake(), "movement test handshake");
    input->sent.clear();
    client.flush_local_move(
        openkal::coords::to_client(3000), 1200.0f, openkal::coords::to_client(4000), 32);
    int vertical_delta = 0;
    while (!input->sent.empty()) {
        std::size_t consumed = 0;
        std::uint8_t type = 0;
        std::vector<std::uint8_t> payload;
        if (openkal::protocol::FrameParser::pop(
                input->sent.data(), input->sent.size(), consumed, type, payload) !=
            openkal::protocol::FrameParseStatus::Complete) {
            expect(false, "movement packets parse");
            break;
        }
        std::int8_t dx = 0, dy = 0, dz = 0;
        expect(Move::parse_client(payload.data(), payload.size(), dx, dy, dz), "parse XYZ move");
        vertical_delta += dz;
        expect(dx == 0 && dy == 0, "vertical movement preserves horizontal baseline");
        input->sent.erase(input->sent.begin(), input->sent.begin() + consumed);
    }
    expect(vertical_delta == -340, "vertical-only movement sends every height delta");

    input->queue(openkal::protocol::CreatePlayer::frame_remove(2));
    input->queue(Move::frame_player_on(2, 50, 50, 0));
    client.update(0.0f, 32);
    expect(client.remote_player_positions().empty(), "ignore movement for removed players");
    input->queue(create(2, false, 10, 20));
    input->queue(Move::frame_player_end(2, 2, 3, 0));
    input->queue(openkal::protocol::Chatting::frame_s2c("Bob", "Hello Alice"));
    input->queue(openkal::protocol::Chatting::frame_s2c("Server", "Command reply"));
    client.update(0.0f, 32);
    expect(client.remote_player_positions().at(2).x == 12 &&
               client.remote_player_positions().at(2).y == 23,
           "re-entry seeds a fresh baseline");
    std::uint32_t removed = 0;
    expect(!client.take_remove_other_player(removed), "same-batch re-entry cancels stale removal");
    openkal::client::OverheadChat overhead;
    openkal::client::PendingChat chat;
    int chat_count = 0;
    while (client.take_chat(chat)) {
        overhead.show(chat.speaker, chat.text);
        ++chat_count;
    }
    expect(chat_count == 2, "all messages remain available for the chat window");
    expect(overhead.message("Bob") == "Hello Alice",
           "player message reaches overhead presentation");
    expect(overhead.message("Server").empty(), "server replies have no player overhead");
    overhead.update(4.0f);
    expect(!overhead.message("Bob").empty(), "message remains before timeout");
    overhead.show("Bob", "Replacement");
    overhead.update(2.0f);
    expect(overhead.message("Bob") == "Replacement",
           "new message replaces text and refreshes lifetime");
    overhead.update(3.0f);
    expect(overhead.message("Bob").empty(), "expired overhead is removed");
    overhead.show("Alice", "Own message");
    expect(overhead.message("Alice") == "Own message", "local player messages are supported");
    overhead.show("Alice", "$Guild greeting!");
    expect(overhead.message("Alice") == "Guild greeting!", "overhead hides channel markers");
    overhead.show("Alice", "@Bob Personal greeting!");
    expect(overhead.message("Alice") == "Personal greeting!",
           "overhead hides whisper routing text");
    overhead.remove("Alice");
    expect(overhead.message("Alice").empty(), "departing player loses overhead");

    openkal::client::RemotePlayerMotion motion;
    motion.reset({12, 0, 23});
    bool continuous = true;
    for (int frame = 0; frame < 120; ++frame) {
        if (frame % 6 == 0) {
            input->queue(Move::frame_player_on(2, 3, 0, 0));
        }
        client.update(1.0f / 120.0f, 32);
        const auto& position = client.remote_player_positions().at(2);
        motion.set_target({static_cast<float>(position.x), 0, static_cast<float>(position.y)},
                          position.moving);
        const float previous_x = motion.position().x;
        motion.update(1.0f / 120.0f);
        continuous = continuous && motion.position().x > previous_x && motion.moving();
    }
    expect(continuous, "received 20 Hz packets produce continuous 120 Hz remote locomotion");
    input->queue(Move::frame_player_end(2, 0, 0, 0));
    client.update(0.0f, 32);
    const auto& endpoint = client.remote_player_positions().at(2);
    motion.set_target({static_cast<float>(endpoint.x), 0, static_cast<float>(endpoint.y)},
                      endpoint.moving);
    expect(motion.moving(), "received zero-delta MoveEnd preserves the unfinished walk segment");
    motion.update(0.2f);
    expect(!motion.moving() && motion.position().x == endpoint.x,
           "received MoveEnd selects idle at the authoritative endpoint");
    // Synthetic project-owned wire fixtures; no external game data is required.
    const std::uint16_t equipment[7]{101, 102, 103, 104, 105, 106, 107};
    input->queue(
        openkal::protocol::CreatePlayer::frame(3, "Mage", 1, 900, 1000, 0, 0, 0, equipment, 4, 7));
    client.update(0.0f, 32);
    openkal::protocol::CreatePlayerFields created;
    bool found_appearance = false;
    while (client.take_create_other_player(created)) {
        if (created.id == 3) {
            found_appearance =
                created.face == 4 && created.hair == 7 &&
                std::equal(std::begin(equipment), std::end(equipment), std::begin(created.wear));
        }
    }
    expect(found_appearance, "remote creation preserves selected face, hair and all worn items");
    openkal::protocol::SkinIndices cosmetics{};
    cosmetics[0] = 25050;
    cosmetics[3] = 25003;
    input->queue(openkal::protocol::CreatePlayer::frame(
        3, "Mage", 1, 900, 1000, 0, 0, 0, equipment, 4, 7, cosmetics));
    client.update(0.0f, 32);
    expect(client.take_create_other_player(created) && created.id == 3 &&
               created.skins == cosmetics,
           "existing remote player receives updated cosmetic appearance");

    using openkal::protocol::Attack;
    input->queue(Attack::frame_server(2, 900, 0));
    input->queue(Attack::frame_server(3, 901, 10, 0, openkal::protocol::kAtkHit));
    input->queue(Attack::frame_server(1, 902, 10));
    client.update(0.0f, 32);
    openkal::client::PendingPlayerAttack attack;
    expect(client.take_player_attack(attack) && attack.attacker_id == 2 && attack.target_id == 900,
           "remote misses still produce a normal attack animation");
    expect(client.take_player_attack(attack) && attack.attacker_id == 3 && attack.target_id == 901,
           "remote attacks retain their own caster and target");
    expect(!client.take_player_attack(attack), "local attack echoes do not restart remote actors");

    input->queue(Move::frame_player_on(2, 1, 0, 0));
    input->queue(Attack::frame_server(2, 1, 0));
    input->queue(Attack::frame_server(1, 2, 8, 0, openkal::protocol::kAtkHit));
    client.update(0.0f, 32);
    expect(!client.remote_player_positions().at(2).moving,
           "a confirmed incoming PvP attack stops the attacker's approach");
    expect(client.take_player_attack(attack) && attack.attacker_id == 2 && attack.target_id == 1 &&
               attack.player_target,
           "a PvP miss still animates for the defending client");
    expect(client.take_player_attack(attack) && attack.attacker_id == 1 && attack.target_id == 2 &&
               attack.player_target && !client.take_player_attack(attack),
           "the attacking client presents its confirmed PvP swing once");
    openkal::client::PendingHitFx pvp_hit;
    expect(client.take_hit_fx(pvp_hit) && pvp_hit.attacker_id == 1 && pvp_hit.target_id == 2 &&
               !client.take_hit_fx(pvp_hit),
           "PvP presentation preserves the existing impact event");
    input->queue(Attack::frame_server(2, 1, 0));
    input->queue(Move::frame_player_on(2, 1, 0, 0));
    client.update(0.0f, 32);
    expect(client.take_player_attack(attack) && client.remote_player_positions().at(2).moving,
           "a later movement packet takes precedence over an earlier swing");

    using openkal::protocol::PvpRelation;
    using openkal::protocol::frame_pvp_status;
    input->queue(frame_pvp_status({1, PvpRelation::Ally, false, true, false, true}));
    input->queue(frame_pvp_status({2, PvpRelation::Enemy, false, true, false, true}));
    input->queue(frame_pvp_status({1}));
    input->queue(frame_pvp_status({2}));
    client.update(0.0f, 32);
    expect(!client.pvp_status(1).defeated && !client.pvp_status(2).defeated,
           "PvP status reflects the latest recovery");
    expect(client.take_duel_defeat(1) && client.take_duel_defeat(2) &&
               !client.take_duel_defeat(1) && !client.take_duel_defeat(2),
           "a duel defeat is presented once even if recovery was received in the same poll");
    input->queue(frame_pvp_status({2, PvpRelation::Enemy, false, true, false, false}));
    client.update(0.0f, 32);
    expect(client.pvp_status(2).defeated && !client.take_duel_defeat(2),
           "other PvP defeats retain their normal recovery path");

    using openkal::protocol::Skill;
    input->queue(openkal::server::frame_s2c_code(123));
    expect(client.handshake(), "complete buffered-session handshake");
    {
        openkal::client::PendingInfoMessage info;
        while (client.take_info_message(info)) {
        }
        openkal::protocol::TradeState state;
        state.phase = openkal::protocol::TradePhase::Invitation;
        state.token = 44;
        state.revision = 1;
        state.partner = "Bob";
        input->queue(openkal::protocol::frame_trade_state(state));
        client.update(0, 32);
        const auto sent = input->sent.size();
        expect(client.trade().locked() && !client.send_chat("locked") && !client.send_rest(true) &&
                   !client.send_skill_target_monster(7, 900) && !client.send_party({}) &&
                   !client.send_revival(),
               "invitation locks every non-trade outgoing channel");
        expect(input->sent.size() == sent, "blocked actions write no bytes to transport");
        state.phase = openkal::protocol::TradePhase::Open;
        input->queue(openkal::protocol::frame_trade_state(state));
        client.update(0, 32);
        expect(!client.send_drop_item(42, 1) && !client.send_chat("still locked"),
               "open trade retains packet lock");
        openkal::protocol::TradeRequest cancel;
        cancel.token = 44;
        expect(client.send_trade(cancel), "trade cancellation is allowed through the gate");
        expect(client.trade().locked(), "sending cancel alone never unlocks client");
        state = {};
        input->queue(openkal::protocol::frame_trade_state(state));
        client.update(0, 32);
        expect(!client.trade().locked() && client.send_chat("unlocked"),
               "only authoritative close unlocks normal traffic");
        expect(!client.take_info_message(info), "closing without a receipt reports no gains");
        state.phase = openkal::protocol::TradePhase::Completed;
        state.status = "Trade completed.";
        state.partner_money = 125;
        openkal::protocol::TradeItem item;
        item.iid = 91;
        item.index = 23000;
        item.count = 3;
        state.other.push_back(item);
        input->queue(openkal::protocol::frame_trade_state(state));
        // A later snapshot in the same network update must not erase the receipt.
        input->queue(openkal::protocol::frame_trade_state({}));
        client.update(0, 32);
        expect(client.take_info_message(info) && info.text == "You got 3x item #23000" &&
                   info.kind == openkal::protocol::BattleMsgKind::ItemGet && info.color &&
                   *info.color == std::array<float, 3>{0.40f, 0.95f, 0.48f},
               "committed incoming stack is reported first in green");
        expect(client.take_info_message(info) && info.text == "You got 125 geons" &&
                   info.kind == openkal::protocol::BattleMsgKind::MoneyGet && info.color &&
                   *info.color == std::array<float, 3>{1, 1, 1},
               "committed incoming Geons follow in white");
        expect(client.take_info_message(info) && info.text == "Trade completed." &&
                   !client.take_info_message(info),
               "completion follows the receipt exactly once");
        state.phase = openkal::protocol::TradePhase::Closed;
        state.status = "Trade cancelled.";
        input->queue(openkal::protocol::frame_trade_state(state));
        client.update(0, 32);
        expect(client.take_info_message(info) && info.text == "Trade cancelled." &&
                   !client.take_info_message(info),
               "cancellation never reports items or Geons as received");
        state.phase = openkal::protocol::TradePhase::Completed;
        state.status = "Trade completed.";
        state.partner_money = 0;
        state.other.front().count = 1;
        input->queue(openkal::protocol::frame_trade_state(state));
        input->queue(openkal::protocol::frame_trade_state(state));
        client.update(0, 32);
        for (int i = 0; i < 2; ++i) {
            expect(client.take_info_message(info) && info.text == "You got item #23000",
                   "single item receipt omits quantity prefix and zero Geons");
            expect(client.take_info_message(info) && info.text == "Trade completed.",
                   "consecutive trades retain their completion messages");
        }
        expect(!client.take_info_message(info), "receipt queue is drained");
        input->queue(openkal::protocol::frame_trading_post_pickup({23000, 3, 0}));
        input->queue(openkal::protocol::frame_trading_post_pickup({23000, 1, 0}));
        input->queue(openkal::protocol::frame_trading_post_pickup({0, 0, 100}));
        client.update(0, 32);
        expect(client.take_info_message(info) && info.text == "You got 3x item #23000" &&
                   info.kind == openkal::protocol::BattleMsgKind::ItemGet && info.color &&
                   *info.color == std::array<float, 3>{0.40f, 0.95f, 0.48f},
               "market pickup uses the trade's green item receipt in battle chat");
        expect(client.take_info_message(info) && info.text == "You got item #23000" &&
                   info.kind == openkal::protocol::BattleMsgKind::ItemGet,
               "single market item omits the quantity prefix");
        expect(client.take_info_message(info) && info.text == "You got 100 geons" &&
                   info.kind == openkal::protocol::BattleMsgKind::MoneyGet && info.color &&
                   *info.color == std::array<float, 3>{1, 1, 1} && !client.take_info_message(info),
               "actual market proceeds use the trade's white currency receipt");
        openkal::protocol::TradingPostState collection;
        collection.view = openkal::protocol::TradingPostView::Collection;
        collection.acknowledged = 1;
        collection.status = "Collected 3 deliveries.";
        input->queue(openkal::protocol::frame_trading_post_state(collection));
        input->queue(openkal::protocol::frame_trading_post_state(collection));
        input->queue(openkal::protocol::frame_trading_post_collection(false));
        client.update(0, 32);
        expect(!client.take_info_message(info),
               "collection refreshes and cached responses do not duplicate reward notices");
    }
    // A matching skill/target from somebody else must not consume our outstanding cast.
    expect(client.send_skill_target_monster(7, 900), "queue local skill expectation");
    input->queue(Skill::frame_server_damage(7, 3, 900, 1, 12, 0, openkal::protocol::kAtkHit));
    client.update(0.0f, 32);
    openkal::client::PendingSkillAction action;
    expect(client.take_skill_action(action) && action.caster_id == 3 && action.skill == 7 &&
               action.fire,
           "another player's identical cast is presented as remote");
    expect(!client.take_local_skill_cast(), "remote cast cannot acknowledge a local cast");
    openkal::client::PendingHitFx hit;
    expect(client.take_hit_fx(hit) && hit.attacker_id == 3 && hit.skill_index == 7,
           "hit sound selection retains the actual caster and skill");
    input->queue(Skill::frame_server_damage(7, 1, 900, 1, 12, 0, openkal::protocol::kAtkHit));
    client.update(0.0f, 32);
    expect(client.take_local_skill_cast(), "local acknowledgement survives a matching remote cast");
    while (client.take_hit_fx(hit)) {
    }

    bool all_skills = true;
    for (int clazz = 0; clazz < 4; ++clazz) {
        input->queue(openkal::protocol::CreatePlayer::frame(
            3, "Caster", static_cast<std::uint8_t>(clazz), 900, 1000, 0, 0, 0, equipment, 4, 7));
        client.update(0.0f, 32);
        for (int index = 0; index < 256; ++index) {
            input->queue(Skill::frame_action_skill(3, static_cast<std::uint8_t>(index), 900));
            client.update(0.0f, 32);
            all_skills = client.take_skill_action(action) && action.caster_id == 3 &&
                         action.skill == index && action.target_id == 900 && !action.fire &&
                         all_skills;
            input->queue(Skill::frame_server(static_cast<std::uint8_t>(index), 3, 900, 1));
            client.update(0.0f, 32);
            all_skills = client.take_skill_action(action) && action.caster_id == 3 &&
                         action.skill == index && action.target_id == 900 && action.fire &&
                         all_skills;
        }
    }
    expect(all_skills, "every skill byte for every player class reaches remote presentation");
    // Damage-bearing Mage skills must not be discarded as an Archer splash with the same id.
    input->queue(
        openkal::protocol::CreatePlayer::frame(3, "Mage", 1, 900, 1000, 0, 0, 0, equipment, 4, 7));
    client.update(0.0f, 32);
    bool all_damage_skills = true;
    for (int index = 0; index < 256; ++index) {
        input->queue(Skill::frame_server_damage(
            static_cast<std::uint8_t>(index), 3, 900, 1, 12, 0, openkal::protocol::kAtkHit));
        client.update(0.0f, 32);
        all_damage_skills =
            client.take_skill_action(action) && action.skill == index && all_damage_skills;
        (void) client.take_hit_fx(hit);
    }
    expect(all_damage_skills, "Mage damage packets cannot be mistaken for Archer-only splash");

    using openkal::protocol::Effect;
    const auto level_up = static_cast<std::uint8_t>(openkal::protocol::EffectType::LevelUp);
    input->queue(Effect::frame(3, level_up));
    input->queue(Effect::frame(1, level_up));
    input->queue(Effect::frame(9999, level_up));
    client.update(0.0f, 32);
    std::uint32_t recipient = 0;
    expect(client.take_level_up_effect(recipient) && recipient == 3,
           "remote level-up retains its recipient");
    expect(client.take_level_up_effect(recipient) && recipient == 1,
           "simultaneous local level-up is not lost");
    expect(client.take_level_up_effect(recipient) && recipient == 9999 &&
               !client.take_level_up_effect(recipient),
           "unknown recipient remains distinct from local");
    input->queue(Effect::frame(3, level_up));
    input->queue(Attack::frame_server(2, 900, 0));
    input->queue(Skill::frame_server(7, 3, 900, 1));
    client.update(0.0f, 32);
    input->close();
    client.close();
    expect(!client.take_player_attack(attack) && !client.take_skill_action(action) &&
               !client.take_level_up_effect(recipient),
           "disconnect clears pending remote presentation");
    expect(client.remote_player_positions().empty(), "disconnect clears remote positions");
    {
        // Independent observer with a nondefault origin catches server/client coordinate mixups.
        constexpr int origin_patch = 17;
        constexpr std::uint32_t monster_id = 700;
        constexpr float pi = 3.14159265359f;
        openkal::client::NetClient observer;
        auto observer_session = std::make_unique<BufferedSession>();
        auto* packets = observer_session.get();
        observer.adopt_session(std::move(observer_session));
        packets->queue(openkal::server::frame_s2c_code(456));
        expect(observer.handshake(), "complete observer handshake");
        packets->queue(create(1, true, 2000, 3000));
        packets->queue(create(2, false, 3000, 2000));
        openkal::server::MonsterEntity monster;
        monster.id = monster_id;
        monster.x = 2000;
        monster.y = 2000;
        packets->queue(openkal::server::frame_create_monster(monster));
        observer.update(0.0f, origin_patch);
        observer.sync_local_move(openkal::coords::to_client(2000, origin_patch),
                                 0.0f,
                                 openkal::coords::to_client(3000, origin_patch),
                                 false,
                                 0.0f,
                                 origin_patch);
        // Project-owned synthetic packets: normal Archer misses must participate in arrow matching.
        const std::uint16_t empty_wear[7]{};
        packets->queue(openkal::protocol::CreatePlayer::frame(
            3, "Archer", 2, 3000, 2000, 0, 0, 0, empty_wear, 0, 0));
        packets->queue(Attack::frame_server(3, monster_id, 0));
        packets->queue(Attack::frame_server(3, monster_id, 5, 0, openkal::protocol::kAtkHit));
        observer.update(0.0f, origin_patch);
        openkal::client::PendingHitFx arrow_outcome;
        expect(observer.take_hit_fx(arrow_outcome) && arrow_outcome.attacker_id == 3 &&
                   arrow_outcome.target_id == monster_id && arrow_outcome.flags == 0,
               "Archer miss is retained for projectile/result pairing");
        expect(observer.take_hit_fx(arrow_outcome) && arrow_outcome.attacker_id == 3 &&
                   (arrow_outcome.flags & openkal::protocol::kAtkHit) != 0,
               "Archer hit following a miss remains a distinct outcome");
        expect(!observer.take_hit_fx(arrow_outcome), "Archer results are consumed once");
        packets->queue(Attack::frame_server(3, monster_id, 0, 0, openkal::protocol::kAtkAnim));
        observer.update(0.0f, origin_patch);
        expect(!observer.take_hit_fx(arrow_outcome), "animation-only packet cannot confirm a hit");
        observer.set_char_identity("Local Archer", 2);
        packets->queue(Attack::frame_server(1, monster_id, 0));
        packets->queue(Attack::frame_server(1, monster_id, 5, 0, openkal::protocol::kAtkHit));
        observer.update(0.0f, origin_patch);
        expect(observer.take_hit_fx(arrow_outcome) && arrow_outcome.attacker_id == 1 &&
                   arrow_outcome.flags == 0,
               "local Archer miss also consumes its own arrow");
        expect(observer.take_hit_fx(arrow_outcome) && arrow_outcome.attacker_id == 1 &&
                   (arrow_outcome.flags & openkal::protocol::kAtkHit) != 0,
               "local Archer hit follows the miss without losing confirmation");
        const auto facing = [&](float yaw) {
            return std::fabs(std::remainder(observer.monsters().at(monster_id).yaw() - yaw,
                                            2.0f * pi)) < 0.001f;
        };
        packets->queue(Attack::frame_server(2, monster_id, 1, 0, openkal::protocol::kAtkHit));
        observer.update(0.0f, origin_patch);
        expect(facing(-pi / 2.0f), "observer faces attacked monster toward remote attacker");

        packets->queue(Move::frame_before(2, 2000, 3000, 0, 0));
        packets->queue(Attack::frame_server(2, monster_id, 0));
        observer.update(0.0f, origin_patch);
        expect(facing(pi), "remote miss uses the latest corrected attacker position");

        packets->queue(Move::frame_before(2, 1000, 2000, 0, 0));
        packets->queue(
            Skill::frame_server_damage(7, 2, monster_id, 1, 1, 0, openkal::protocol::kAtkHit));
        observer.update(0.0f, origin_patch);
        expect(facing(pi / 2.0f), "observer also faces monster toward remote skill caster");

        packets->queue(Move::frame_before(2, 2000, 1000, 0, 0));
        packets->queue(Attack::frame_server(monster_id, 2, 1));
        observer.update(0.0f, origin_patch);
        expect(facing(0.0f), "attacking monster faces its remote player target");

        packets->queue(Skill::frame_action_skill(monster_id, 7, 1));
        observer.update(0.0f, origin_patch);
        expect(facing(pi), "monster skill preparation still faces the local target");

        packets->queue(Move::frame_before(2, 2000, 2000, 0, 0));
        packets->queue(Attack::frame_server(2, monster_id, 1));
        observer.update(0.0f, origin_patch);
        expect(facing(pi), "overlapping attacker preserves the last valid facing");

        observer.sync_local_move(openkal::coords::to_client(1000, origin_patch),
                                 0.0f,
                                 openkal::coords::to_client(2000, origin_patch),
                                 false,
                                 0.0f,
                                 origin_patch);
        packets->queue(openkal::protocol::CreatePlayer::frame_remove(2));
        packets->queue(Attack::frame_server(2, monster_id, 1));
        observer.update(0.0f, origin_patch);
        expect(facing(pi), "unknown attacker does not borrow the local player's position");

        packets->queue(Attack::frame_server(monster_id, 9999, 0));
        observer.update(0.0f, origin_patch);
        expect(facing(pi), "unknown monster target preserves facing");

        packets->queue(Attack::frame_server(1, monster_id, 1));
        observer.update(0.0f, origin_patch);
        expect(facing(pi / 2.0f), "local player attacks preserve existing facing behavior");
        packets->close();
        observer.close();
    }
    {
        using namespace openkal;
        using test::expect;
        client::NetClient client;
        auto transport = std::make_unique<BufferedSession>();
        auto* packets = transport.get();
        client.adopt_session(std::move(transport));
        packets->queue(protocol::ClockSync::frame({1700000000, 3600, 123, {}}));
        client.update(0, 32);
        expect(client.server_clock().ready() && client.server_clock().online_text() == "00:02:03",
               "NetClient receives authoritative clock snapshot");
        packets->queue(protocol::ClockSync::frame({1700003600, 3600, 3723, {}}));
        client.update(0, 32);
        expect(client.server_clock().online_text() == "01:02:03",
               "NetClient applies hourly correction without resetting online duration");
        packets->close();
        client.update(0, 32);
        expect(!client.server_clock().ready(), "transport disconnect clears clock state");
    }
    return openkal::test::g_failures ? 1 : 0;
}
