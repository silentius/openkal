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

#include "openkal/map/Teleport.h"
#include "openkal/protocol/CreatePlayer.h"
#include "openkal/protocol/FrameDrain.h"
#include "openkal/protocol/Handshake.h"
#include "openkal/protocol/ItemWear.h"
#include "openkal/protocol/Teleport.h"

#include <cstddef>
#include <stdexcept>

namespace {
    using J = openkal::platform::Json;
    using namespace openkal;
    std::vector<std::uint8_t> frame(std::uint8_t type, const std::vector<std::uint8_t>& payload) {
        protocol::PacketWriter writer;
        writer.write_bytes(payload.data(), payload.size());
        return writer.frame(type);
    }
    template <class Handler>
    bool readFrames(server::ISession& session, std::vector<std::uint8_t>& rx, Handler handler) {
        if (!session.recv_into(rx) || rx.size() > 1048576) {
            return false;
        }
        protocol::FrameDrain frames(rx);
        std::vector<std::uint8_t> payload;
        for (;;) {
            std::uint8_t type = 0;
            const auto state = frames.pop(type, payload);
            if (state == protocol::FrameParseStatus::Incomplete) {
                return true;
            }
            if (state == protocol::FrameParseStatus::Invalid) {
                return false;
            }
            handler(type, payload);
        }
    }
}  // namespace

TestPlayerWorker::TestPlayerWorker(openkal::server::CharacterRecord character,
                                   std::unique_ptr<openkal::server::ISession> session,
                                   std::string token,
                                   Events events)
    : m_server(std::move(session)), m_character(std::move(character)), m_agent(m_character),
      m_token(std::move(token)), m_events(std::move(events)), m_thread([this] { run(); }) {}

TestPlayerWorker::~TestPlayerWorker() {
    stop();
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

bool TestPlayerWorker::finished() const {
    return m_finished.load();
}

void TestPlayerWorker::stop() {
    m_stop.store(true);
    m_wake.notify_one();
}

std::future<QJsonObject> TestPlayerWorker::submit(QJsonObject command,
                                                  std::shared_ptr<TestPlayerData> data) {
    std::packaged_task<QJsonObject()> task(
        [this, command = std::move(command), data = std::move(data)] {
            const auto result = execute(command, data);
            publish(true);
            return result;
        });
    auto result = task.get_future();
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_finished && !m_stop.load()) {
            m_commands.push_back(std::move(task));
        }
    }
    m_wake.notify_one();
    return result;
}

void TestPlayerWorker::run() {
    try {
        if (!m_server || !m_server->send_all(openkal::protocol::Handshake::build_connect(0))) {
            throw std::runtime_error("World connection failed.");
        }
        publish(true);
        while (!m_stop.load()) {
            std::deque<std::packaged_task<QJsonObject()>> commands;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                commands.swap(m_commands);
            }
            for (auto& command : commands) {
                if (m_stop.load()) {
                    break;
                }
                command();
            }
            if (m_stop.load() || !pump()) {
                break;
            }
            publish();
            std::unique_lock<std::mutex> lock(m_mutex);
            m_wake.wait_for(lock, std::chrono::milliseconds(10), [this] {
                return m_stop.load() || !m_commands.empty();
            });
        }
    } catch (const std::exception& error) {
        m_events({{"message", QString::fromUtf8(error.what())}});
    }
    closeViewer();
    m_server.reset();
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_commands.clear();
        m_finished = true;
    }
    m_events({{"disconnected", true}});
}

void TestPlayerWorker::publish(bool force) {
    const auto now = std::chrono::steady_clock::now();
    if (!force && now - m_lastPublish < std::chrono::seconds(1)) {
        return;
    }
    m_lastPublish = now;
    QJsonObject state{{"status", m_online ? (m_viewer ? "Watching" : "Online") : "Logging in"},
                      {"activity", QString::fromStdString(m_agent.status())},
                      {"level", m_agent.level()}};
    if (state != m_lastState) {
        m_lastState = state;
        m_events({{"state", state}});
    }
}

void TestPlayerWorker::closeViewer() {
    m_viewer.reset();
    m_acceptor.reset();
    m_forwarding = false;
    m_viewing = false;
}

QJsonObject TestPlayerWorker::execute(const QJsonObject& command,
                                      std::shared_ptr<TestPlayerData> data) {
    const auto op = command["op"].toString();
    if (op == "autoplay") {
        m_agent.start(std::move(data));
    } else if (op == "pause") {
        m_agent.pause();
    } else if (op == "view_closed") {
        closeViewer();
    } else if (op == "view") {
        if (!m_online) {
            throw std::runtime_error("Character could not enter the world.");
        }
        if (m_viewer || m_viewing) {
            throw std::runtime_error("A spectator window is already open or starting.");
        }
        const auto record =
            openkal::server::read_test_character(command["record"].toString().toStdString());
        if (!record) {
            throw std::runtime_error("Could not prepare local attachment.");
        }
        m_character = *record;
        const auto endpoint = command["endpoint"].toString();
        m_acceptor = openkal::server::create_unix_acceptor();
        if (!m_acceptor->listen(endpoint.toStdString())) {
            throw std::runtime_error("Could not create the local spectator bridge.");
        }
        m_viewing = true;
        m_events({{"openClient", endpoint}});
    } else if (op == "equip" || op == "unequip") {
        const auto iid = static_cast<std::uint32_t>(command["iid"].toInt());
        const auto bytes = op == "equip" ? openkal::protocol::ItemWear::frame_client_put_on(iid)
                                         : openkal::protocol::ItemWear::frame_client_put_off(iid);
        if (!m_online || !m_server->send_all(bytes)) {
            throw std::runtime_error("Equipment command could not be sent.");
        }
    } else if (op == "autonomous_city_travel" || op == "autonomous_area_travel") {
        if (!m_online || !m_agent.active()) {
            return {};
        }
        int map = 0;
        TestPlayerPosition position;
        if (op == "autonomous_city_travel") {
            const int index = command["city"].toInt(-1);
            if (index < 0 || m_agent.pending_city_travel() != static_cast<std::size_t>(index)) {
                return {};
            }
            const auto* city = openkal::map::teleport_loc(static_cast<std::size_t>(index));
            if (!city) {
                return {};
            }
            map = city->map;
            position = {city->x, city->y, city->z};
        } else {
            const auto pending = m_agent.pending_area_travel();
            if (!pending || pending->area != command["area"].toInt(-1) ||
                pending->map != command["map"].toInt(-1) ||
                pending->position.x != command["x"].toInt(-1) ||
                pending->position.y != command["y"].toInt(-1) ||
                pending->position.z != command["z"].toInt(-1)) {
                return {};
            }
            map = pending->map;
            position = pending->position;
        }
        return {{"op", "teleport"},
                {"id", command["id"]},
                {"map", map},
                {"x", position.x},
                {"y", position.y},
                {"z", position.z}};
    }
    return {};
}

void TestPlayerWorker::handleServer(std::uint8_t type, const std::vector<std::uint8_t>& payload) {
    using namespace openkal;
    m_agent.receive(type, payload);
    if (type == server::kTestResponse) {
        protocol::PacketReader reader(payload);
        std::string text;
        if (reader.read_string(text)) {
            J result;
            if (!J::parse(text, result)) {
                m_server->close();
                return;
            }
            if (!result.at("error").as_string().empty()) {
                m_events({{"message", QString::fromStdString(result.at("error").as_string())}});
                if (m_viewer) {
                    m_viewer->close();
                }
            }
        }
        return;
    }
    if (!m_online && type == static_cast<std::uint8_t>(protocol::S2C::Code)) {
        protocol::S2CCodeFields code;
        if (!protocol::Handshake::parse_code(payload.data(), payload.size(), code) ||
            !m_server->send_all(protocol::Handshake::build_answer(code.protocol_test_value))) {
            m_server->close();
        }
        return;
    }
    if (type == static_cast<std::uint8_t>(protocol::S2C::CreatePlayer)) {
        protocol::CreatePlayerFields player;
        if (protocol::CreatePlayer::parse(payload.data(), payload.size(), player) &&
            (player.class_byte & 0x80)) {
            const bool changed = !m_online;
            m_online = true;
            if (changed) {
                publish(true);
            }
        }
    }
    if (m_viewer && m_forwarding) {
        if (!m_viewer->send_all(frame(type, payload))) {
            m_viewer->close();
        }
    }
    if (type == static_cast<std::uint8_t>(protocol::S2C::Teleport)) {
        std::uint8_t map = 0, cheat = 0;
        int x = 0, y = 0, z = 0;
        if (protocol::Teleport::parse_server(payload.data(), payload.size(), map, x, y, z, cheat)) {
            (void) m_server->send_all(protocol::Teleport::frame_client_ack(1, z));
        }
    }
}

void TestPlayerWorker::handleViewer(std::uint8_t type, const std::vector<std::uint8_t>& payload) {
    using namespace openkal;
    if (type == server::kLobbyRequest) {
        protocol::PacketReader reader(payload);
        std::uint8_t operation = 255;
        if (!reader.read_u8(operation)) {
            m_viewer->close();
            return;
        }
        protocol::PacketWriter data;
        bool ok = true;
        switch (static_cast<server::LobbyOperation>(operation)) {
        case server::LobbyOperation::Capabilities:
            data.write_u8(1);
            data.write_u8(0);
            break;
        case server::LobbyOperation::Login:
            data.write_i32(m_character.account_id);
            break;
        case server::LobbyOperation::Characters:
            data.write_u8(1);
            server::write_lobby_character(data, m_character);
            break;
        case server::LobbyOperation::Items:
            data.write_u8(0);
            break;
        case server::LobbyOperation::Select:
            server::write_lobby_character(data, m_character);
            break;
        default:
            ok = false;
            break;
        }
        protocol::PacketWriter response;
        response.write_u8(operation);
        response.write_u8(ok ? 1 : 0);
        response.write_string(ok ? "" : "Operation unavailable in spectator view.");
        response.write_bytes(data.payload().data(), data.payload().size());
        (void) m_viewer->send_all(response.frame(server::kLobbyResponse));
        return;
    }
    if (type == static_cast<std::uint8_t>(protocol::C2S::Connect)) {
        m_forwarding = true;
        (void) m_server->send_all(
            server::frame_test_request(m_token, J::parse(R"({"op":"refresh"})")));
        return;
    }
    if (type == static_cast<std::uint8_t>(protocol::C2S::AnsCode) && m_forwarding) {
        // The refreshed snapshot is released only after the viewer answers its challenge.
        if (!m_server->send_all(frame(type, payload))) {
            m_viewer->close();
        }
        return;
    }
    // The view is read-only even if a modified viewer sends gameplay packets.
    // Connect above requests the existing attachment snapshot; all player actions
    // and teleport acknowledgements remain owned by this player worker.
}

bool TestPlayerWorker::pump() {
    if (!m_online && std::chrono::steady_clock::now() - m_started > std::chrono::seconds(15)) {
        m_server->close();
    }
    if (!m_server->alive() || !readFrames(*m_server, m_rx, [this](auto type, const auto& bytes) {
            handleServer(type, bytes);
        })) {
        return false;
    }
    if (m_acceptor && !m_viewer) {
        m_viewer = m_acceptor->try_accept();
        if (m_viewer) {
            m_forwarding = false;
            m_viewerRx.clear();
            publish(true);
        }
    }
    if (m_viewer && (!m_viewer->alive() ||
                     !readFrames(*m_viewer, m_viewerRx, [this](auto type, const auto& bytes) {
                         handleViewer(type, bytes);
                     }))) {
        closeViewer();
        publish(true);
    }
    if (m_online && m_server->alive()) {
        const auto now =
            static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                           std::chrono::steady_clock::now().time_since_epoch())
                                           .count());
        try {
            m_agent.tick(
                now,
                [this](const auto& packet) { return m_server->send_all(packet); },
                std::chrono::milliseconds(2),
                [this](std::size_t city) {
                    m_events({{"travel",
                               QJsonObject{{"op", "autonomous_city_travel"},
                                           {"city", static_cast<int>(city)}}}});
                    return true;
                },
                [this](const TestPlayerAreaTravel& destination) {
                    m_events({{"travel",
                               QJsonObject{{"op", "autonomous_area_travel"},
                                           {"map", destination.map},
                                           {"area", destination.area},
                                           {"x", destination.position.x},
                                           {"y", destination.position.y},
                                           {"z", destination.position.z}}}});
                    return true;
                });
        } catch (const std::exception& error) {
            m_agent.pause();
            m_events({{"message", "Autonomous play paused: " + QString::fromUtf8(error.what())}});
            publish(true);
        }
    }
    return m_server->alive();
}
