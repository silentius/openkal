/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "TestPlayerWorker.h"

#include "openkal/protocol/CreatePlayer.h"
#include "test_util.h"

#include <QCoreApplication>
#include <QMetaObject>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <set>

namespace {
    using namespace openkal;
    using namespace std::chrono_literals;

    struct SessionState {
        std::mutex mutex;
        std::condition_variable wake;
        bool blocked = false, entered = false, wrongThread = false, destroyed = false;
        std::thread::id owner;
        int reads = 0;

        void touch() {
            const auto current = std::this_thread::get_id();
            if (owner == std::thread::id{}) {
                owner = current;
            }
            wrongThread = wrongThread || owner != current;
        }
    };

    // Project-owned synthetic transport fixture: no external server or assets are needed.
    class Session final : public server::ISession {
      public:
        explicit Session(std::shared_ptr<SessionState> state) : m_state(std::move(state)) {}
        ~Session() override {
            std::lock_guard<std::mutex> lock(m_state->mutex);
            m_state->touch();
            m_state->destroyed = true;
        }
        int fd() const override {
            return -1;
        }
        bool alive() const override {
            return m_alive;
        }
        void close() override {
            m_alive = false;
        }
        bool send_all(const std::uint8_t*, std::size_t) override {
            std::lock_guard<std::mutex> lock(m_state->mutex);
            m_state->touch();
            return m_alive;
        }
        bool recv_into(std::vector<std::uint8_t>& bytes) override {
            std::unique_lock<std::mutex> lock(m_state->mutex);
            m_state->touch();
            m_state->entered = true;
            m_state->wake.notify_all();
            m_state->wake.wait(lock, [this] { return !m_state->blocked; });
            if (++m_state->reads == 1) {
                const std::uint16_t wear[7]{};
                const auto packet = protocol::CreatePlayer::frame(
                    77, "Thread fixture", 0x80, 1024, 1024, 10000, 0, 0, wear, 0, 0);
                bytes.insert(bytes.end(), packet.begin(), packet.end());
            }
            return m_alive;
        }

      private:
        std::shared_ptr<SessionState> m_state;
        bool m_alive = true;
    };

    bool wait(const std::function<bool()>& ready) {
        const auto deadline = std::chrono::steady_clock::now() + 5s;
        while (std::chrono::steady_clock::now() < deadline) {
            QCoreApplication::processEvents();
            if (ready()) {
                return true;
            }
            std::this_thread::sleep_for(1ms);
        }
        return false;
    }
}  // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    constexpr std::size_t count = 16;
    std::vector<std::shared_ptr<SessionState>> sessions;
    std::vector<std::unique_ptr<TestPlayerWorker>> workers;
    std::vector<QJsonObject> states(count);
    std::set<std::thread::id> eventThreads;
    const auto guiThread = std::this_thread::get_id();
    bool wrongGuiThread = false;
    std::size_t disconnects = 0;
    server::CharacterRecord character;
    character.level = 5;
    for (std::size_t i = 0; i < count; ++i) {
        auto session = std::make_shared<SessionState>();
        session->blocked = i == 0;
        sessions.push_back(session);
        workers.push_back(std::make_unique<TestPlayerWorker>(
            character,
            std::make_unique<Session>(session),
            "fixture-token",
            [&, i](QJsonObject event) {
                const auto source = std::this_thread::get_id();
                QMetaObject::invokeMethod(
                    &app,
                    [&, i, source, event = std::move(event)] {
                        wrongGuiThread = wrongGuiThread || std::this_thread::get_id() != guiThread;
                        eventThreads.insert(source);
                        if (event.contains("state")) {
                            states[i] = event["state"].toObject();
                        }
                        if (event["disconnected"].toBool()) {
                            ++disconnects;
                        }
                    },
                    Qt::QueuedConnection);
            }));
    }
    {
        std::unique_lock<std::mutex> lock(sessions.front()->mutex);
        test::expect(
            sessions.front()->wake.wait_for(lock, 5s, [&] { return sessions.front()->entered; }),
            "first player is deliberately blocked in its transport");
    }
    test::expect(wait([&] {
                     for (std::size_t i = 1; i < count; ++i) {
                         if (states[i]["status"] != "Online") {
                             return false;
                         }
                     }
                     return true;
                 }),
                 "other players enter the world and deliver UI events while one player is blocked");
    test::expect(eventThreads.size() == count && !eventThreads.count(guiThread) && !wrongGuiThread,
                 "each player emits from its own thread and UI events execute on the GUI thread");

    auto blockedCommand = workers.front()->submit({{"op", "pause"}});
    std::vector<std::future<QJsonObject>> commands;
    for (std::size_t i = 1; i < count; ++i) {
        commands.push_back(workers[i]->submit({{"op", "pause"}}));
    }
    for (auto& command : commands) {
        test::expect(command.wait_for(5s) == std::future_status::ready,
                     "a stalled player does not delay commands on other players");
    }
    test::expect(blockedCommand.wait_for(0ms) == std::future_status::timeout,
                 "the stalled player's command remains local to its own queue");
    // A failed operation must not kill the player loop or strand later commands.
    auto rejected = workers[1]->submit({{"op", "view"}});
    bool failed = false;
    if (rejected.wait_for(5s) == std::future_status::ready) {
        try {
            (void) rejected.get();
        } catch (const std::exception&) {
            failed = true;
        }
    }
    test::expect(failed, "worker operation failures reach the command completion");
    auto next = workers[1]->submit({{"op", "pause"}});
    test::expect(next.wait_for(5s) == std::future_status::ready,
                 "a failed operation leaves the independent worker usable");
    auto travel = workers[1]->submit({{"op", "autonomous_city_travel"}, {"city", 0}});
    bool ignoredTravel = false;
    if (travel.wait_for(5s) == std::future_status::ready) {
        ignoredTravel = travel.get().isEmpty();
    }
    test::expect(ignoredTravel, "paused players reject stale autonomous travel requests");

    for (auto& worker : workers) {
        worker->stop();
    }
    {
        std::lock_guard<std::mutex> lock(sessions.front()->mutex);
        sessions.front()->blocked = false;
    }
    sessions.front()->wake.notify_all();
    workers.clear();
    bool abandoned = false;
    if (blockedCommand.wait_for(0ms) == std::future_status::ready) {
        try {
            (void) blockedCommand.get();
        } catch (const std::future_error&) {
            abandoned = true;
        }
    }
    test::expect(abandoned, "shutdown releases pending command waiters instead of hanging");
    test::expect(wait([&] { return disconnects == count; }),
                 "all workers deliver their final disconnect event before destruction completes");
    for (const auto& session : sessions) {
        std::lock_guard<std::mutex> lock(session->mutex);
        test::expect(session->destroyed && !session->wrongThread && session->owner != guiThread,
                     "gameplay transport and cleanup stay on the owning player thread");
    }
    return test::g_failures == 0 ? 0 : 1;
}
