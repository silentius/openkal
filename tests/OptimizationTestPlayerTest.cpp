/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

// Reuse the project's owned catalogs and protocol helpers without changing production access.
#define main existing_test_player_agent_main
#include "TestPlayerAgentTest.cpp"
#undef main

// Compile the transport implementation here so the shared parser contract can be
// characterized directly, including malformed streams that cannot enter the gameplay fixture.
#include "../tools/testPlayers/TestPlayerWorker.cpp"

#include <QCoreApplication>
#include <deque>

namespace {
    class ScriptedInput final : public server::ISession {
      public:
        std::deque<std::vector<std::uint8_t>> chunks;
        bool available = true;
        int fd() const override {
            return -1;
        }
        bool alive() const override {
            return available;
        }
        void close() override {
            available = false;
        }
        bool send_all(const std::uint8_t*, std::size_t) override {
            return available;
        }
        bool recv_into(std::vector<std::uint8_t>& output) override {
            if (!available) {
                return false;
            }
            if (!chunks.empty()) {
                const auto chunk = std::move(chunks.front());
                chunks.pop_front();
                output.insert(output.end(), chunk.begin(), chunk.end());
            }
            return true;
        }
    };

    struct FrameRecorder {
        int& calls;
        std::vector<Packet>& packets;
        void operator()(std::uint8_t type, const std::vector<std::uint8_t>& payload) const {
            ++calls;
            packets.push_back({type, payload});
        }
    };

    void worker_frame_contract() {
        const auto first = frame(201, {1, 2, 3, 4, 5});
        const auto second = frame(202, {6, 7, 8});
        for (std::size_t split = 0; split <= second.size(); ++split) {
            ScriptedInput input;
            auto prefix = first;
            prefix.insert(prefix.end(), second.begin(), second.begin() + split);
            input.chunks.push_back(prefix);
            input.chunks.emplace_back(second.begin() + split, second.end());
            std::vector<std::uint8_t> buffered;
            std::vector<Packet> received;
            int calls = 0;
            const FrameRecorder record{calls, received};
            expect(readFrames(input, buffered, record), "valid prefix and partial suffix accepted");
            expect(received.size() == (split == second.size() ? 2u : 1u),
                   "incomplete suffix is never dispatched early");
            expect(readFrames(input, buffered, record) && buffered.empty() &&
                       received.size() == 2 && received[0].type == 201 &&
                       received[0].payload == std::vector<std::uint8_t>({1, 2, 3, 4, 5}) &&
                       received[1].type == 202 &&
                       received[1].payload == std::vector<std::uint8_t>({6, 7, 8}),
                   "every header and payload split resumes exactly once in order");
        }
        ScriptedInput empty;
        std::vector<std::uint8_t> buffered;
        int calls = 0;
        std::vector<Packet> received;
        const FrameRecorder record{calls, received};
        expect(readFrames(empty, buffered, record) && calls == 0,
               "empty receive is an incomplete stream, not a disconnect");
        for (int index = 0; index < 4096; ++index) {
            buffered.insert(buffered.end(), first.begin(), first.end());
        }
        expect(readFrames(empty, buffered, record) && buffered.empty() && calls == 4096,
               "large coalesced burst dispatches each frame exactly once");
        buffered = first;
        buffered.insert(buffered.end(), {1, 0, 0});
        calls = 0;
        expect(!readFrames(empty, buffered, record) && calls == 1 && buffered.size() == 3,
               "valid prefix dispatches before a malformed length rejects the remaining stream");
        buffered = first;
        buffered.insert(buffered.end(), second.begin(), second.end());
        bool handlerThrew = false;
        try {
            (void) readFrames(empty, buffered, [](auto, const auto&) {
                throw std::runtime_error("owned handler failure");
            });
        } catch (const std::runtime_error&) {
            handlerThrew = true;
        }
        expect(handlerThrew && buffered == second,
               "handler failure consumes its dispatched frame and retains the following frame");
        buffered = first;
        buffered.insert(buffered.end(), second.begin(), second.end());
        calls = 0;
        expect(readFrames(empty,
                          buffered,
                          [&](auto, const auto&) {
                              ++calls;
                              empty.close();
                          }) &&
                   buffered.empty() && calls == 2,
               "closing the session in a handler still drains already received complete frames");
        empty.available = true;
        buffered.assign(1048577, 0);
        calls = 0;
        expect(!readFrames(empty, buffered, record) && calls == 0,
               "buffer cap rejects oversized receive before dispatch");
        buffered.assign(1048576, 0);
        expect(!readFrames(empty, buffered, record) && calls == 0,
               "exact buffer cap reaches malformed-frame validation");
        empty.available = false;
        buffered = first;
        expect(!readFrames(empty, buffered, record) && calls == 0 && buffered == first,
               "receive failure does not dispatch or consume a retained frame");
    }

    struct BridgeState {
        std::mutex mutex;
        std::deque<std::vector<std::uint8_t>> inbound;
        std::vector<std::vector<std::uint8_t>> outbound;
        bool alive = true;
        bool receiveFailure = false;
    };

    class BridgeSession final : public server::ISession {
      public:
        explicit BridgeSession(std::shared_ptr<BridgeState> state) : m_state(std::move(state)) {}
        int fd() const override {
            return -1;
        }
        bool alive() const override {
            std::lock_guard<std::mutex> lock(m_state->mutex);
            return m_state->alive;
        }
        void close() override {
            std::lock_guard<std::mutex> lock(m_state->mutex);
            m_state->alive = false;
        }
        bool send_all(const std::uint8_t* bytes, std::size_t size) override {
            std::lock_guard<std::mutex> lock(m_state->mutex);
            m_state->outbound.emplace_back(bytes, bytes + size);
            return m_state->alive;
        }
        bool recv_into(std::vector<std::uint8_t>& bytes) override {
            std::lock_guard<std::mutex> lock(m_state->mutex);
            if (!m_state->alive || m_state->receiveFailure) {
                return false;
            }
            if (!m_state->inbound.empty()) {
                const auto chunk = std::move(m_state->inbound.front());
                m_state->inbound.pop_front();
                bytes.insert(bytes.end(), chunk.begin(), chunk.end());
            }
            return true;
        }

      private:
        std::shared_ptr<BridgeState> m_state;
    };

    bool wait_until(const std::function<bool()>& condition,
                    std::chrono::milliseconds timeout = std::chrono::seconds(5)) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (condition()) {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return false;
    }

    void worker_world_and_viewer() {
        auto state = std::make_shared<BridgeState>();
        const std::uint16_t wear[7]{};
        const auto create = protocol::CreatePlayer::frame(
            77, "Fragmented", 0x80, 1024, 1024, 10000, 0, 0, wear, 0, 0);
        for (auto byte : create) {
            state->inbound.push_back({byte});
        }
        std::atomic<bool> online{false}, watching{false};
        std::atomic<int> onlineEvents{0};
        TestPlayerWorker worker(
            {}, std::make_unique<BridgeSession>(state), "fixture-token", [&](QJsonObject event) {
                if (event["state"].toObject()["status"] == "Online") {
                    online.store(true);
                    ++onlineEvents;
                }
                if (event["state"].toObject()["status"] == "Watching") {
                    watching.store(true);
                }
            });
        expect(wait_until([&] { return online.load(); }),
               "world stream assembles a one-byte-fragmented CreatePlayer exactly once");
        const std::string endpoint = "/tmp/openkal-coverage-viewer-" + std::to_string(getpid());
        server::CharacterRecord character;
        auto view = worker.submit(
            {{"op", "view"},
             {"endpoint", QString::fromStdString(endpoint)},
             {"record", QString::fromStdString(server::test_character_snapshot(character))}});
        const bool prepared = view.wait_for(std::chrono::seconds(5)) == std::future_status::ready;
        expect(prepared, "spectator listener is prepared on the worker thread");
        if (!prepared) {
            worker.stop();
            return;
        }
        try {
            (void) view.get();
        } catch (const std::exception& error) {
            expect(false, error.what());
            worker.stop();
            return;
        }
        auto viewer = server::connect_unix(endpoint);
        expect(viewer != nullptr, "connect owned local spectator transport");
        if (!viewer) {
            worker.stop();
            return;
        }
        expect(wait_until([&] { return watching.load(); }), "worker accepts its spectator session");
        const auto connect = protocol::Handshake::build_connect(0);
        expect(viewer->send_all(connect.data(), 1), "send incomplete spectator frame header");
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
        std::size_t before = 0;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            before = state->outbound.size();
        }
        expect(before == 1, "incomplete viewer connect does not request a premature snapshot");
        std::vector<std::uint8_t> tail(connect.begin() + 1, connect.end());
        const auto answer = protocol::Handshake::build_answer(123);
        const auto gameplay = protocol::Attack::frame_client(protocol::kCkMonster, 900);
        tail.insert(tail.end(), answer.begin(), answer.end());
        tail.insert(tail.end(), gameplay.begin(), gameplay.end());
        expect(viewer->send_all(tail), "send coalesced spectator connect, answer and gameplay");
        expect(wait_until([&] {
                   std::lock_guard<std::mutex> lock(state->mutex);
                   return state->outbound.size() >= 3;
               }),
               "viewer connect and answer are forwarded after reassembly");
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            expect(state->outbound.size() == 3 &&
                       unpack(state->outbound[1]).type == server::kTestRequest &&
                       state->outbound[2] == answer,
                   "viewer snapshot precedes its exact answer and read-only gameplay is ignored");
            auto forwarded = frame(201, {9, 8, 7});
            const auto next = frame(202, {6, 5});
            forwarded.insert(forwarded.end(), next.begin(), next.end());
            state->inbound.push_back(std::move(forwarded));
        }
        std::vector<std::uint8_t> response;
        expect(wait_until([&] {
                   if (!viewer->recv_into(response)) {
                       return false;
                   }
                   return response.size() ==
                          frame(201, {9, 8, 7}).size() + frame(202, {6, 5}).size();
               }),
               "coalesced world frames are forwarded to the viewer");
        auto expected = frame(201, {9, 8, 7});
        const auto next = frame(202, {6, 5});
        expected.insert(expected.end(), next.begin(), next.end());
        expect(response == expected, "world forwarding preserves frame payloads and order");
        expect(viewer->send_all(std::vector<std::uint8_t>{1, 0, 0}),
               "send malformed viewer length");
        expect(wait_until([&] {
                   std::vector<std::uint8_t> ignored;
                   return !viewer->recv_into(ignored);
               }) &&
                   !worker.finished(),
               "malformed viewer disconnect leaves the world worker alive");
        auto pause = worker.submit({{"op", "pause"}});
        expect(pause.wait_for(std::chrono::seconds(5)) == std::future_status::ready,
               "worker still handles commands after viewer rejection");
        const auto reopen = [&] {
            watching.store(false);
            auto command = worker.submit(
                {{"op", "view"},
                 {"endpoint", QString::fromStdString(endpoint)},
                 {"record", QString::fromStdString(server::test_character_snapshot(character))}});
            if (command.wait_for(std::chrono::seconds(5)) != std::future_status::ready) {
                return false;
            }
            try {
                (void) command.get();
            } catch (const std::exception&) {
                return false;
            }
            viewer = server::connect_unix(endpoint);
            return viewer && wait_until([&] { return watching.load(); });
        };
        expect(reopen(), "spectator can reconnect after malformed input");
        if (viewer) {
            const int count = onlineEvents.load();
            viewer->close();
            expect(wait_until([&] { return onlineEvents.load() > count; }) && !worker.finished(),
                   "viewer receive failure closes only its attachment");
        }
        expect(reopen(), "spectator can reconnect after peer EOF");
        if (viewer) {
            const int count = onlineEvents.load();
            expect(viewer->send_all(frame(server::kLobbyRequest, {})),
                   "send structurally valid frame with incomplete lobby payload");
            expect(wait_until([&] { return onlineEvents.load() > count; }) && !worker.finished(),
                   "a viewer closed by its handler is cleaned up on the next pump");
        }
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->inbound.push_back(std::vector<std::uint8_t>(1048577, 0));
        }
        expect(wait_until([&] { return worker.finished(); }),
               "oversized world receive terminates its worker");
        std::filesystem::remove(endpoint);
    }

    void worker_terminal_states() {
        const std::uint16_t wear[7]{};
        const auto create = protocol::CreatePlayer::frame(
            77, "Terminal", 0x80, 1024, 1024, 10000, 0, 0, wear, 0, 0);
        for (int failure = 0; failure < 3; ++failure) {
            auto state = std::make_shared<BridgeState>();
            std::atomic<bool> online{false};
            if (failure == 0) {
                state->receiveFailure = true;
            } else {
                state->inbound.push_back(create);
                if (failure == 1) {
                    state->inbound.push_back({1, 0, 0});
                }
            }
            TestPlayerWorker worker({},
                                    std::make_unique<BridgeSession>(state),
                                    "fixture-token",
                                    [&](QJsonObject event) {
                                        if (event["state"].toObject()["status"] == "Online") {
                                            online.store(true);
                                        }
                                    });
            if (failure == 2) {
                expect(wait_until([&] { return online.load(); }),
                       "terminal fixture reaches the world before transport close");
                std::lock_guard<std::mutex> lock(state->mutex);
                state->alive = false;
            }
            expect(wait_until([&] { return worker.finished(); }),
                   "world receive error, malformed frame or closed transport finishes the worker");
            auto stale = worker.submit({{"op", "pause"}});
            bool abandoned = false;
            if (stale.wait_for(std::chrono::seconds(5)) == std::future_status::ready) {
                try {
                    (void) stale.get();
                } catch (const std::future_error&) {
                    abandoned = true;
                }
            }
            expect(abandoned, "a command submitted after disconnect gets a broken-promise result");
        }
        auto silent = std::make_shared<BridgeState>();
        TestPlayerWorker login(
            {}, std::make_unique<BridgeSession>(silent), "fixture-token", [](QJsonObject) {});
        expect(wait_until([&] { return login.finished(); }, std::chrono::seconds(20)),
               "a transport that never sends CreatePlayer is closed by the login timeout");
        std::lock_guard<std::mutex> lock(silent->mutex);
        expect(!silent->alive, "login timeout closes the silent world session");
    }

    std::shared_ptr<TestPlayerData> load_with_drops(Fixture& fixture) {
        platform::ConfigArchive::unmount();
        std::vector<ZipEntry> entries;
        for (const auto* directory : {"items",
                                      "npcs",
                                      "tasks",
                                      "quests",
                                      "dialogs",
                                      "skills",
                                      "monsters",
                                      "shops",
                                      "groups",
                                      "itemgroups"}) {
            const auto path = fixture.root + "/" + directory;
            std::filesystem::create_directories(path);
            for (const auto& file : std::filesystem::recursive_directory_iterator(path)) {
                if (!file.is_regular_file()) {
                    continue;
                }
                const auto bytes = platform::File::read(file.path().string());
                expect(bytes.has_value(), "read owned selector fixture");
                if (bytes) {
                    entries.push_back(
                        {std::string(std::string_view{directory} == "shops" ? "config/server/"
                                                                            : "config/") +
                             std::filesystem::relative(file.path(), fixture.root).generic_string(),
                         std::string(bytes->begin(), bytes->end())});
                }
            }
        }
        if (std::filesystem::is_regular_file(fixture.root + "/portals.json")) {
            const auto portals = platform::File::read(fixture.root + "/portals.json");
            entries.push_back(
                {"config/portals.json", std::string(portals->begin(), portals->end())});
        }
        const auto assetBase = platform::executable_directory();
        const auto relativeAssets =
            std::filesystem::relative(fixture.root, assetBase).generic_string();
        entries.push_back({"config/profiles/default.profile",
                           "data_root=" + relativeAssets +
                               "\nmaps_subdir=visuals\nserver_map_root=" + relativeAssets +
                               "/missing-maps\n"});
        const auto bytes = platform::File::read(fixture.root + "/maps/n_000_000.akarea");
        entries.push_back(
            {"RuntimeData/areas/n_000_000.akarea", std::string(bytes->begin(), bytes->end())});
        std::filesystem::create_directories(fixture.root + "/package");
        expect(test::write_bytes(fixture.root + "/package/config.pak", make_stored_zip(entries)),
               "write owned drop selector archive");
        return TestPlayerAgent::load_data(fixture.root + "/package");
    }

    void definition(Fixture& fixture,
                    int index,
                    const std::string& drops,
                    bool spawn = true,
                    int level = 1,
                    int race = 0,
                    int map = 0) {
        const std::string spawns =
            spawn ? ",\"spawns\":[{\"map\":" + std::to_string(map) +
                        ",\"area\":1,\"max\":1,\"cycle\":60,\"rect\":[36, 30, 38, 34]}]"
                  : "";
        text(fixture.root + "/monsters/" + std::to_string(index) + ".json",
             ("{\"index\":" + std::to_string(index) +
              ",\"stats\":{\"race\":" + std::to_string(race) +
              ",\"level\":" + std::to_string(level) + ",\"hp\":100" + drops + "}" + spawns + "}")
                 .c_str());
    }

    std::uint32_t attack_once(TestPlayerAgent& player, std::uint64_t now = 10000) {
        std::uint32_t target = 0;
        player.tick(now, [&](const auto& bytes) {
            const auto packet = unpack(bytes);
            if (packet.type == static_cast<std::uint8_t>(protocol::C2S::Attack)) {
                std::uint8_t kind = 0;
                expect(protocol::Attack::parse_client(
                           packet.payload.data(), packet.payload.size(), kind, target) &&
                           kind == protocol::kCkMonster,
                       "decode observable quest hunting target");
            }
            return true;
        });
        return target;
    }

    void selector_contract(Fixture& fixture) {
        std::filesystem::remove_all(fixture.root + "/npcs");
        std::filesystem::create_directories(fixture.root + "/npcs");
        text(
            fixture.root + "/tasks/20.json",
            R"({"key":20,"flags":[{"key":2,"item":[0, 2, 1]},{"key":1,"item":[0, 0, 2]},{"key":1,"level":1,"item":[0, 2, 2]}]})");
        text(
            fixture.root + "/groups/1.json",
            R"({"index":1,"entries":[{"type":"money","amount":100,"chance":1},{"item":3,"chance":1},{"item":2,"chance":0},{"item":2,"chance":-1},{"item":2,"chance":1}]})");
        text(
            fixture.root + "/itemgroups/1.json",
            R"({"index":1,"entries":[{"group":99,"chance":1},{"group":1,"chance":0},{"group":1,"chance":-1},{"group":1,"chance":1}]})");
        definition(fixture, 1, ",\"itemGroups\":[[1, 1]]", false);
        definition(fixture, 2, ",\"itemGroups\":[[1, 1]]", true, 50);
        definition(fixture, 3, ",\"itemGroups\":[[1, 1]]", true, 1, 4);
        definition(fixture, 4, ",\"itemGroups\":[[1, 1]]", true, 1, 0, 9);
        definition(fixture,
                   5,
                   ",\"quests\":[[21, 1, 1, 1], [20, 2, 1, 1], [20, 1, 99, 1], [20, 1, 1, 1]]");
        definition(fixture, 6, ",\"itemGroups\":[[1, 1]]");
        definition(fixture, 7, "", true, 5);
        fixture.data = load_with_drops(fixture);
        const auto subject = [&](std::uint8_t flag = 1, std::uint8_t clear = 0) {
            auto player = fixture.player();
            deliver(*player, protocol::Quest::frame_save_quest({20, flag, clear}));
            // Level-five fallback would win without a valid active quest drop relation.
            monster(*player, 1030, 1024, 905, 5);
            monster(*player, 1030, 1024, 906, 6);
            monster(*player, 1030, 1024, 907, 7);
            return player;
        };
        auto player = subject();
        expect(attack_once(*player) == 905,
               "quest-specific first definition beats ordinary-drop ties and stronger fallback");
        deliver(*player, server::frame_remove_monster(905));
        insert(*player, 99, 2, 2);
        expect(attack_once(*player, 12000) == 907,
               "a pickup immediately removes quest priority on the next target selection");
        auto clearedAfterSelection = subject();
        expect(attack_once(*clearedAfterSelection) == 905,
               "the active objective initially selects its quest monster");
        deliver(*clearedAfterSelection, server::frame_remove_monster(905));
        deliver(*clearedAfterSelection, protocol::Quest::frame_save_quest({20, 1, 1}));
        expect(attack_once(*clearedAfterSelection, 12000) == 907,
               "quest clear immediately removes priority on the next target selection");
        auto completed = subject(1, 1);
        expect(attack_once(*completed) == 907,
               "cleared quests no longer request their item monster");
        auto wrongFlag = subject(3);
        expect(attack_once(*wrongFlag) == 907,
               "unmatched journal flags use ordinary level hunting");
        auto enough = subject();
        insert(*enough, 99, 2, 2);
        expect(attack_once(*enough) == 907,
               "inventory count meeting the objective immediately removes quest priority");
        auto partly = subject();
        insert(*partly, 99, 2, 1);
        expect(attack_once(*partly) == 905, "partial item count retains quest priority");
        auto unknown = fixture.player();
        deliver(*unknown, protocol::Quest::frame_save_quest({99, 1, 0}));
        monster(*unknown, 1030, 1024, 907, 7);
        expect(attack_once(*unknown) == 907,
               "journal entry without task catalog safely falls back");

        auto previousCatalog = subject();
        text(
            fixture.root + "/portals.json",
            R"({"portals":[{"id":"owned_quest","fromMap":0,"fromRect":[34, 32, 35, 33],"toMap":9,"to":[1040, 1040, 10000]}]})");
        fixture.data = load_with_drops(fixture);
        auto remote = subject();
        const auto discard = [](const auto&) { return true; };
        for (std::uint64_t now = 10000; now < 11000; now += 10) {
            remote->tick(now, discard);
            if (remote->status() == "Travelling through portal owned_quest") {
                break;
            }
        }
        expect(remote->status() == "Travelling through portal owned_quest",
               "a reachable portal enables the earlier remote-map quest definition");
        std::filesystem::remove(fixture.root + "/portals.json");

        definition(fixture, 5, ",\"itemGroups\":[[99, 1]]");
        fixture.data = load_with_drops(fixture);
        auto ordinary = subject();
        expect(attack_once(*ordinary) == 906,
               "missing outer group skips the earlier definition and finds ordinary drops");
        expect(attack_once(*previousCatalog) == 905,
               "an independent agent retains its immutable drop catalog after reload");
        previousCatalog->start(fixture.data);
        expect(attack_once(*previousCatalog, 12000) == 906,
               "restarting an agent adopts the replacement catalog and clears its old selection");
        text(fixture.root + "/tasks/20.json",
             R"({"key":20,"flags":[{"key":1,"level":6,"item":[0, 2, 2]}]})");
        fixture.data = load_with_drops(fixture);
        auto level = subject();
        expect(attack_once(*level) == 907, "objective above current player level is deferred");
        deliver(*level, server::frame_remove_monster(907));
        deliver(*level, protocol::Property::frame_update_level(6));
        expect(attack_once(*level, 12000) == 906,
               "level update immediately enables the objective without reloading catalogs");

        text(fixture.root + "/tasks/20.json",
             R"({"key":20,"flags":[{"key":1,"level":1,"item":[0, 2, 2]}]})");
        text(
            fixture.root + "/groups/1.json",
            R"({"index":1,"entries":[{"type":"money","amount":100,"chance":1},{"item":3,"chance":1},{"item":2,"chance":0},{"item":2,"chance":-1}]})");
        fixture.data = load_with_drops(fixture);
        auto noDrop = subject();
        expect(attack_once(*noDrop) == 907,
               "reloaded catalog with only money, other items and nonpositive drops has no match");
    }
}  // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    worker_frame_contract();
    worker_world_and_viewer();
    worker_terminal_states();
    Fixture fixture;
    selector_contract(fixture);
    return test::g_failures == 0 ? 0 : 1;
}
