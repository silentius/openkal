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
#include "openkal/protocol/ClockSync.h"
#include "openkal/protocol/CreatePlayer.h"
#include "openkal/protocol/FrameParser.h"
#include "openkal/protocol/GameExit.h"
#include "openkal/protocol/Handshake.h"
#include "openkal/protocol/Move.h"
#include "openkal/protocol/Opcodes.h"
#include "openkal/server/ManagementApi.h"
#include "openkal/server/ServerHost.h"
#include "openkal/server/net/Transport.h"
#include "openkal/server/store/DataStore.h"
#include "test_util.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <future>
#include <mutex>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

namespace {
    using namespace openkal;
    using test::expect;
    using Bytes = std::vector<std::uint8_t>;
    using Clock = std::chrono::steady_clock;

    struct Peer {
        std::unique_ptr<server::ISession> session;
        Bytes rx{};
        protocol::CreatePlayerFields self{};
    };

    void append(Bytes& batch, const Bytes& frame) {
        batch.insert(batch.end(), frame.begin(), frame.end());
    }

    template <typename Handler>
    bool receive_until(Peer& peer, Handler handler, Clock::time_point deadline) {
        while (Clock::now() < deadline) {
            if (!peer.session->recv_into(peer.rx)) {
                return false;
            }
            for (;;) {
                std::size_t consumed = 0;
                std::uint8_t type = 0;
                Bytes payload;
                const auto status = protocol::FrameParser::pop(
                    peer.rx.data(), peer.rx.size(), consumed, type, payload);
                if (status == protocol::FrameParseStatus::Invalid) {
                    return false;
                }
                if (status == protocol::FrameParseStatus::Incomplete) {
                    break;
                }
                peer.rx.erase(peer.rx.begin(),
                              peer.rx.begin() + static_cast<std::ptrdiff_t>(consumed));
                if (handler(type, payload)) {
                    return true;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        return false;
    }

    bool enter(Peer& peer) {
        if (!peer.session->send_all(protocol::Handshake::build_connect(0))) {
            return false;
        }
        bool answer_sent = false, created = false;
        return receive_until(
            peer,
            [&](std::uint8_t type, const Bytes& payload) {
                if (type == static_cast<std::uint8_t>(protocol::S2C::Code)) {
                    protocol::S2CCodeFields code;
                    answer_sent =
                        protocol::Handshake::parse_code(payload.data(), payload.size(), code) &&
                        peer.session->send_all(
                            protocol::Handshake::build_answer(code.protocol_test_value));
                } else if (type == static_cast<std::uint8_t>(protocol::S2C::CreatePlayer)) {
                    protocol::CreatePlayerFields player;
                    if (protocol::CreatePlayer::parse(payload.data(), payload.size(), player) &&
                        (player.class_byte & 0x80)) {
                        peer.self = std::move(player);
                        created = true;
                    }
                } else if (type == static_cast<std::uint8_t>(protocol::S2C::ClockSync)) {
                    protocol::ClockSyncFields clock;
                    return answer_sent && created &&
                           protocol::ClockSync::parse(payload.data(), payload.size(), clock) &&
                           clock.unix_seconds > 0;
                }
                return false;
            },
            Clock::now() + std::chrono::seconds(3));
    }

    // The guard releases an unread peer only after the observation deadline. This
    // also makes the legacy blocking implementation fail without hanging teardown.
    class UnreadPeerGuard final {
      public:
        explicit UnreadPeerGuard(int fd)
            : m_worker([this, fd] {
                  std::unique_lock<std::mutex> lock(m_mutex);
                  if (!m_condition.wait_for(
                          lock, std::chrono::seconds(3), [this] { return m_done; })) {
                      m_released.store(true);
                      ::shutdown(fd, SHUT_RDWR);
                  }
              }) {}

        ~UnreadPeerGuard() {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_done = true;
            }
            m_condition.notify_one();
            m_worker.join();
        }

        bool released() const {
            return m_released.load();
        }

      private:
        std::mutex m_mutex;
        std::condition_variable m_condition;
        bool m_done = false;
        std::atomic<bool> m_released{false};
        std::thread m_worker;
    };

    void exercise_pm(const server::ServerConfig& config) {
        using J = platform::Json;
        Peer recipient{server::connect_unix(config.unix_path)};
        const bool entered = recipient.session && enter(recipient);
        expect(entered, "establish the sole owned recipient for exact PM targeting");
        if (!entered) {
            return;
        }
        auto pm = J::parse(R"({"op":"pm","body":"Owned completed support message"})");
        pm.object()["character"].set_number(config.character_id);
        auto delivered = std::async(std::launch::async, [&] {
            return server::management_request(
                config.management_socket, config.management_token, pm);
        });
        const bool received = receive_until(
            recipient,
            [&](std::uint8_t type, const Bytes& payload) {
                std::string speaker, body;
                return type == static_cast<std::uint8_t>(protocol::S2C::Chatting) &&
                       protocol::Chatting::parse_s2c(
                           payload.data(), payload.size(), speaker, body) &&
                       speaker == server::kManagementSpeaker &&
                       body == "@Owned Active Peer Owned completed support message";
            },
            Clock::now() + std::chrono::seconds(2));
        expect(received && delivered.get().at("ok").boolean(),
               "management PM acknowledges success only with its complete exact support frame on "
               "the wire");
        const auto status = [&] {
            return server::management_request(config.management_socket,
                                              config.management_token,
                                              J::parse(R"({"op":"status","after":0})"));
        };
        const auto successful_events = status();
        expect(successful_events.at("events").array().size() == 1 &&
                   successful_events.at("events").array().front().at("outgoing").boolean() &&
                   successful_events.at("events").array().front().at("body").as_string() ==
                       "Owned completed support message",
               "only the completely transmitted support PM is recorded as outgoing");

        Bytes pressure;
        for (int i = 0; i < 2048; ++i) {
            append(pressure, protocol::Chatting::frame_c2s("/time"));
        }
        UnreadPeerGuard guard(recipient.session->fd());
        expect(recipient.session->send_all(pressure),
               "fill the PM recipient with earlier valid output without reading it");
        const auto fill_deadline = Clock::now() + std::chrono::milliseconds(500);
        int queued = 0;
        while (Clock::now() < fill_deadline) {
            if (::ioctl(recipient.session->fd(), FIONREAD, &queued) == 0 && queued > 3000) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        expect(queued > 3000, "the PM recipient has accumulated real unread socket output");
        auto blocked_pm = J::parse(R"({"op":"pm","body":"Owned unsent support message"})");
        blocked_pm.object()["character"].set_number(config.character_id);
        auto blocked = std::async(std::launch::async, [&] {
            return server::management_request(
                config.management_socket, config.management_token, blocked_pm);
        });
        const auto responsive_status = status();
        expect(responsive_status.at("error").is_null() &&
                   responsive_status.at("events").array().size() == 1 &&
                   responsive_status.at("online").array().size() == 1 && !guard.released(),
               "another management peer remains responsive while the recipient cannot accept a PM");
        const bool finished =
            blocked.wait_for(std::chrono::seconds(2)) == std::future_status::ready;
        const auto failed = blocked.get();
        expect(finished &&
                   failed.at("error").as_string() ==
                       "Player disconnected before the message was sent." &&
                   !failed.at("ok").boolean() && !guard.released(),
               "a PM behind blocked output reports deadline failure without claiming delivery");
        const auto failed_events = status();
        expect(failed_events.at("events").array().size() == 1 && !guard.released(),
               "a failed queued PM does not append a successful outgoing support event");
    }

    void exercise(server::ServerHost& host, const server::ServerConfig& config) {
        Peer unread{server::connect_unix(config.unix_path)},
            active{server::connect_unix(config.unix_path)};
        expect(unread.session && active.session, "connect two real owned Unix peers");
        if (!unread.session || !active.session) {
            return;
        }
        const bool entered = enter(unread) && enter(active);
        expect(entered && unread.self.id != active.self.id,
               "both peers enter with distinct runtime identities and authoritative clocks");
        if (!entered) {
            return;
        }
        const auto before = host.monster_snapshot();
        expect(before.size() == 1 && before.front().idle_left > 0,
               "owned idle monster exposes an active world timer before backpressure");
        if (before.size() != 1 || before.front().idle_left <= 0) {
            return;
        }

        int receive_size = 4096;
        expect(
            ::setsockopt(
                unread.session->fd(), SOL_SOCKET, SO_RCVBUF, &receive_size, sizeof(receive_size)) ==
                0,
            "bound the unread peer receive window");
        constexpr int reply_count = 2048;
        Bytes pressure;
        for (int i = 0; i < reply_count; ++i) {
            append(pressure, protocol::Chatting::frame_c2s("/time"));
        }
        expect(pressure.size() < 32768, "pressure requests fit the production receive cap");

        UnreadPeerGuard guard(unread.session->fd());
        expect(unread.session->send_all(pressure), "send a bounded burst of valid slash commands");
        Bytes requests = protocol::Move::frame_client_end(1, 0, 0);
        append(requests, protocol::Chatting::frame_c2s("Owned active peer remains responsive"));
        append(requests, protocol::GameExit::frame_client_request());
        expect(active.session->send_all(requests),
               "queue active peer movement, chat and save barrier");
        int response_order = 0;
        bool ordered = true, saved = false;
        const bool barrier = receive_until(
            active,
            [&](std::uint8_t type, const Bytes& payload) {
                if (type == static_cast<std::uint8_t>(protocol::S2C::MovePlayerEnd)) {
                    std::uint32_t id = 0;
                    std::int8_t dx = 0, dy = 0, dz = 0;
                    if (protocol::Move::parse_player(
                            payload.data(), payload.size(), id, dx, dy, dz) &&
                        id == active.self.id && dx == 1 && dy == 0 && dz == 0) {
                        ordered = ordered && response_order == 0;
                        response_order = 1;
                    }
                } else if (type == static_cast<std::uint8_t>(protocol::S2C::Chatting)) {
                    std::string speaker, text;
                    if (protocol::Chatting::parse_s2c(
                            payload.data(), payload.size(), speaker, text) &&
                        text == "Owned active peer remains responsive") {
                        ordered = ordered && response_order == 1;
                        response_order = 2;
                    }
                } else if (type == static_cast<std::uint8_t>(protocol::S2C::AnsGameExit)) {
                    ordered = ordered && response_order == 2;
                    return protocol::GameExit::parse_server_answer(
                        payload.data(), payload.size(), saved);
                }
                return false;
            },
            Clock::now() + std::chrono::seconds(4));
        expect(barrier && saved && ordered && response_order == 2 && !guard.released(),
               "active peer receives movement, chat and successful save in order before watchdog "
               "release");

        int queued = 0;
        const auto one_reply = protocol::Chatting::frame_s2c("Server", "[ 00 : 00 : 00 UTC ]");
        pollfd unread_state{unread.session->fd(), POLLRDHUP, 0};
        const bool kernel_pending =
            ::ioctl(unread.session->fd(), FIONREAD, &queued) == 0 && queued > 0 &&
            static_cast<std::size_t>(queued) < one_reply.size() * reply_count;
        const bool still_open = ::poll(&unread_state, 1, 0) >= 0 &&
                                !(unread_state.revents & (POLLRDHUP | POLLHUP | POLLERR));
        expect(
            barrier && kernel_pending && still_open && !guard.released(),
            "unread peer remains open with more ordered replies than its kernel buffer accepted");

        const auto world_before = host.monster_snapshot();
        bool world_advanced = false;
        const auto world_deadline = Clock::now() + std::chrono::milliseconds(500);
        while (Clock::now() < world_deadline && !guard.released()) {
            const auto after = host.monster_snapshot();
            if (world_before.size() == 1 && after.size() == 1 &&
                after.front().id == world_before.front().id &&
                (after.front().idle_left < world_before.front().idle_left ||
                 after.front().x != world_before.front().x ||
                 after.front().y != world_before.front().y)) {
                world_advanced = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        expect(barrier && world_advanced && !guard.released(),
               "authoritative world timer advances while the slow peer remains unread");
        const auto stopping = Clock::now();
        host.stop();
        expect(Clock::now() - stopping < std::chrono::seconds(2) && !guard.released(),
               "server shutdown is bounded while a peer still has unread output");
    }
}  // namespace

int main() {
    const auto root =
        test::temp_dir(("optimization_backpressure_" + std::to_string(getpid())).c_str());
    server::ServerConfig config;
    config.maps_dir = root + "/maps";
    config.monsters_dir = root + "/monsters";
    config.items_dir = root + "/items";
    config.save_dir = root + "/save";
    config.unix_path = root + "/game.sock";
    config.management_socket = root + "/management.sock";
    config.management_token = std::string(32, 'm');
    config.store = server::StoreBackend::Sqlite;
    config.transport = server::TransportKind::Unix;
    config.spawn_x = config.spawn_y = 512;
    config.spawn_z = 0;
    config.max_monsters = 1;
    config.map_radius_cells = 0;
    for (const auto& dir : {config.maps_dir, config.monsters_dir, config.items_dir}) {
        test::mkdir_p(dir);
    }
    expect(test::write_open_area(config.maps_dir + "/n_000_000.akarea"),
           "write an owned open map for backpressure testing");
    std::ofstream(config.monsters_dir + "/1.json")
        << R"({"index":1,"name":"Owned timer witness","stats":{"level":1,"hp":100},"spawns":[{"map":0,"max":1,"cycle":1400,"rect":[16, 16, 17, 17]}]})";
    std::ofstream(config.items_dir + "/1.json")
        << R"({"index":1,"name":"Owned fixture sword","class":["weapon","sword"],"wear":1})";
    auto store = server::create_data_store(server::StoreBackend::Sqlite);
    const bool opened = store->open(config.save_dir);
    expect(opened, "open disposable backpressure character store");
    if (!opened) {
        return 1;
    }
    server::CharacterRecord hero;
    hero.name = "Owned Active Peer";
    hero.level = 1;
    hero.hp = hero.max_hp = 100;
    hero.map_x = hero.map_y = 512;
    expect(store->ensure_default_player("owned", "owned", hero),
           "seed an owned backpressure player");
    const auto account = store->find_account("owned");
    const auto characters =
        account ? store->list_characters(account->id) : std::vector<server::CharacterRecord>{};
    if (characters.empty()) {
        expect(false, "find the owned backpressure player");
        return 1;
    }
    config.character_id = characters.front().id;
    store->close();
    server::ServerHost host;
    expect(host.start(config), "start real Unix backpressure server");
    for (int i = 0; i < 200 && !std::filesystem::exists(config.unix_path); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    exercise_pm(config);
    host.stop();
    expect(host.start(config), "restart the owned world before the independent shutdown scenario");
    for (int i = 0; i < 200 && !std::filesystem::exists(config.unix_path); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    exercise(host, config);
    host.stop();
    std::filesystem::remove_all(root);
    return test::g_failures == 0 ? 0 : 1;
}
