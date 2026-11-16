/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "TestPlayerBackend.h"

#include "TestPlayerWorker.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <random>
#include <stdexcept>

namespace {
    using J = openkal::platform::Json;
    using namespace openkal;
    J json(const QJsonObject& object) {
        const auto bytes = QJsonDocument(object).toJson(QJsonDocument::Compact);
        return J::parse(
            std::string_view(bytes.constData(), static_cast<std::size_t>(bytes.size())));
    }
    QJsonObject object(const J& value) {
        return QJsonDocument::fromJson(QByteArray::fromStdString(J::stringify(value, 0))).object();
    }
}  // namespace

struct TestPlayerBackend::Peer {
    std::shared_ptr<TestPlayerWorker> worker;
    QJsonObject state{{"status", "Logging in"}};
    QString endpoint;
    std::uint64_t generation = 0;
};

TestPlayerBackend::TestPlayerBackend(QObject* parent) : QObject(parent) {
    m_thread = std::thread([this] { run(); });
}

TestPlayerBackend::~TestPlayerBackend() {
    m_stop.store(true);
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void TestPlayerBackend::submit(QJsonObject command) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (command["op"].toString() == "castle_war_poll" &&
        std::any_of(m_commands.cbegin(), m_commands.cend(), [](const QJsonObject& queued) {
            return queued["op"].toString() == "castle_war_poll";
        })) {
        return;
    }
    m_commands.push_back(std::move(command));
}

void TestPlayerBackend::run() {
    auto lastKeepalive = std::chrono::steady_clock::now();
    while (!m_stop.load()) {
        if (m_connected &&
            std::chrono::steady_clock::now() - lastKeepalive > std::chrono::seconds(60)) {
            try {
                (void) request({{"op", "hello"}});
            } catch (const std::exception& error) {
                m_connected = false;
                Q_EMIT message(QString::fromUtf8(error.what()));
            }
            lastKeepalive = std::chrono::steady_clock::now();
        }
        QJsonObject command;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_commands.empty()) {
                command = m_commands.front();
                m_commands.pop_front();
            }
        }
        const auto op = command.value("op").toString();
        if (op == "castle_war_poll") {
            QJsonObject state;
            if (m_connected) {
                try {
                    state = object(request({{"op", "castle_war"}, {"action", "status"}}));
                } catch (const std::exception& error) {
                    state["error"] = QString::fromUtf8(error.what());
                }
            }
            Q_EMIT snapshot({{"castle_war", state}});
        } else if (!command.isEmpty()) {
            Q_EMIT busyChanged(true);
            try {
                execute(command);
            } catch (const std::exception& error) {
                const auto reason = QString::fromUtf8(error.what());
                if (op == "castle_war" && command.value("action").toString() == "start") {
                    Q_EMIT snapshot({{"castle_war_start_error", reason}});
                    Q_EMIT message(reason);
                } else {
                    Q_EMIT message("Operation stopped: " + reason +
                                   " Earlier successful changes were retained.");
                }
            }
            Q_EMIT busyChanged(false);
        }
        pump();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    for (auto& [id, peer] : m_peers) {
        (void) id;
        peer->worker->stop();
    }
    m_peers.clear();
    m_retired.clear();
}

J TestPlayerBackend::request(const QJsonObject& command) {
    if (!m_connected) {
        throw std::runtime_error("Connect to a test-enabled server first.");
    }
    auto result = m_admin.request(json(command), [this] { pump(); });
    if (!result.at("error").as_string().empty()) {
        throw std::runtime_error(result.at("error").as_string());
    }
    return result;
}

void TestPlayerBackend::refresh() {
    m_players = {};
    for (int start = 1; start <= m_playerCount; start += 50) {
        const auto data = object(request({{"op", "players"},
                                          {"start", start},
                                          {"count", std::min(50, m_playerCount - start + 1)}}));
        for (const auto& player : data["players"].toArray()) {
            m_players.append(player);
        }
    }
    m_guilds = object(request({{"op", "guilds"}}))["guilds"].toArray();
    publish();
}

void TestPlayerBackend::publish() {
    QJsonArray players;
    for (const auto& value : m_players) {
        auto row = value.toObject();
        const int id = row["id"].toInt();
        const auto peer = m_peers.find(id);
        row["status"] = "Offline";
        if (peer != m_peers.end()) {
            const auto& state = peer->second->state;
            for (auto field = state.begin(); field != state.end(); ++field) {
                row[field.key()] = field.value();
            }
        }
        row["guild"] = 0;
        row["rank"] = 5;
        for (const auto& guild : m_guilds) {
            const auto group = guild.toObject();
            for (const auto& member : group["members"].toArray()) {
                if (member.toObject()["id"].toInt() != id) {
                    continue;
                }
                row["guild"] = group["id"];
                row["rank"] = member.toObject()["rank"];
            }
        }
        players.append(row);
    }
    Q_EMIT snapshot({{"players", players},
                     {"guilds", m_guilds},
                     {"player_count", m_playerCount},
                     {"guild_count", m_guildCount}});
}

void TestPlayerBackend::login(int id) {
    if (m_peers.count(id)) {
        return;
    }
    // Finish the old connection before opening another session for the same character.
    while (m_retired.count(id)) {
        if (m_stop.load()) {
            throw std::runtime_error("Test player manager is stopping.");
        }
        pump();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    openkal::server::TestControlClient client;
    if (!client.connect(m_host, m_port, m_token, m_tls)) {
        throw std::runtime_error("Could not establish a trusted TLS player connection.");
    }
    const auto result = client.request(json({{"op", "login"}, {"id", id}}), [this] { pump(); });
    if (!result.at("error").as_string().empty()) {
        throw std::runtime_error(result.at("error").as_string());
    }
    auto record = openkal::server::read_test_character(result.at("record").as_string());
    if (!record) {
        throw std::runtime_error("Server returned an invalid character snapshot.");
    }
    auto peer = std::make_unique<Peer>();
    peer->generation = ++m_generation;
    peer->worker = std::make_shared<TestPlayerWorker>(
        *record,
        client.release_session(),
        m_token,
        [this, id, generation = peer->generation](QJsonObject event) {
            event["id"] = id;
            event["generation"] = static_cast<qint64>(generation);
            std::lock_guard<std::mutex> lock(m_mutex);
            m_events.push_back(std::move(event));
        });
    m_peers[id] = std::move(peer);
    publish();
}

void TestPlayerBackend::logout(int id) {
    const auto peer = m_peers.find(id);
    if (peer == m_peers.end()) {
        return;
    }
    peer->second->worker->stop();
    m_retired[id] = std::move(peer->second->worker);
    m_peers.erase(peer);
}

void TestPlayerBackend::execute(const QJsonObject& command) {
    const auto op = command["op"].toString();
    if (op == "autonomous_city_travel" || op == "autonomous_area_travel") {
        const int id = command["id"].toInt();
        const auto peer = m_peers.find(id);
        if (peer == m_peers.end() ||
            (command.contains("generation") &&
             command["generation"].toInteger() != static_cast<qint64>(peer->second->generation))) {
            return;
        }
        const auto worker = peer->second->worker;
        const auto teleport = await(worker->submit(command));
        if (teleport.isEmpty()) {
            return;
        }
        try {
            (void) request(teleport);
        } catch (...) {
            (void) worker->submit({{"op", "pause"}});
            throw;
        }
        return;
    }
    if (op == "view_closed") {
        for (auto& [id, peer] : m_peers) {
            (void) id;
            if (peer->endpoint == command["endpoint"].toString()) {
                (void) peer->worker->submit(command);
            }
        }
        publish();
        return;
    }
    if (op == "connect") {
        if (!m_peers.empty()) {
            throw std::runtime_error("Log out managed players before changing server.");
        }
        m_host = command["host"].toString().toStdString();
        m_port = static_cast<std::uint16_t>(command["port"].toInt());
        m_token = command["token"].toString().toStdString();
        if (m_token.size() < 16) {
            throw std::runtime_error(
                "Enter the server's test-control token (at least 16 characters).");
        }
        m_playerCount = std::clamp(command["players"].toInt(50), 0, 500);
        m_guildCount = std::clamp(command["guilds"].toInt(8), 0, 100);
        m_tls = openkal::server::tls_config_from_environment();
        m_tls.ca_file = command["ca"].toString().toStdString();
        m_tls.server_name = command["server_name"].toString().toStdString();
        m_connected = false;
        Q_EMIT snapshot({{"castle_war_guilds", QJsonArray{}}});
        if (!m_admin.connect(m_host, m_port, m_token, m_tls)) {
            throw std::runtime_error(
                "TLS connection failed. Check the server address and CA certificate.");
        }
        m_connected = true;
        try {
            (void) request({{"op", "hello"}});
            refresh();
        } catch (...) {
            m_connected = false;
            throw;
        }
        Q_EMIT snapshot({{"castle_war_guild_catalog_reload", true}});
        Q_EMIT message("Connected to the test server.");
        return;
    }
    if (op == "provision") {
        m_playerCount = std::clamp(command["players"].toInt(), 0, 500);
        m_guildCount = std::clamp(command["guilds"].toInt(), 0, 100);
        QJsonArray activeIds;
        for (int n = 1; n <= m_playerCount && !m_stop.load(); ++n) {
            const auto ensured = request({{"op", "ensure"}, {"ordinal", n}});
            activeIds.append(ensured.at("id").as_int());
            if (n % 10 == 0) {
                Q_EMIT message(QString("Prepared %1 / %2 players.").arg(n).arg(m_playerCount));
            }
        }
        (void) request({{"op", "guilds"}, {"count", m_guildCount}, {"active_ids", activeIds}});
        refresh();
        QJsonArray activeGuilds;
        for (const auto& guild : m_guilds) {
            if (guild.toObject()["test_ordinal"].toInt() <= m_guildCount) {
                activeGuilds.append(guild);
            }
        }
        for (const auto& value : m_players) {
            const auto row = value.toObject();
            bool active = false;
            for (const auto& guild : activeGuilds) {
                if (guild.toObject()["id"] == row["guild"]) {
                    active = true;
                }
            }
            if (active || (m_guildCount == 0 && row["guild"].toInt() == 0)) {
                continue;
            }
            const int target =
                activeGuilds.isEmpty()
                    ? 0
                    : activeGuilds[(row["ordinal"].toInt() - 1) % activeGuilds.size()]
                          .toObject()["id"]
                          .toInt();
            (void) request(
                {{"op", "membership"}, {"id", row["id"]}, {"guild", target}, {"rank", 5}});
        }
        for (auto it = m_peers.begin(); it != m_peers.end();) {
            const bool active =
                std::any_of(m_players.begin(), m_players.end(), [&](const QJsonValue& row) {
                    return row.toObject()["id"].toInt() == it->first;
                });
            const int id = it->first;
            ++it;
            if (!active) {
                logout(id);
            }
        }
        refresh();
        Q_EMIT message(
            "Population updated. Inactive characters and guilds are retained for reuse.");
        return;
    }
    if (op == "refresh") {
        refresh();
        return;
    }
    if (op == "castle_war_guilds") {
        QJsonArray guilds;
        if (m_connected) {
            try {
                qint64 after = 0;
                while (!m_stop.load()) {
                    const auto result =
                        object(request({{"op", "castle_war_guilds"}, {"after", after}}));
                    if (!result["guilds"].isArray() || !result["next"].isDouble()) {
                        throw std::runtime_error("Invalid Castle War guild catalog response.");
                    }
                    const auto page = result["guilds"].toArray();
                    for (const auto& guild : page) {
                        guilds.append(guild);
                    }
                    const auto next = result["next"].toInteger(-1);
                    if (next == 0) {
                        break;
                    }
                    if (next <= after || page.isEmpty() ||
                        next != page.last().toObject()["id"].toInteger()) {
                        throw std::runtime_error("Invalid Castle War guild catalog cursor.");
                    }
                    after = next;
                }
            } catch (...) {
                Q_EMIT snapshot({{"castle_war_guilds", QJsonArray{}}});
                throw;
            }
        }
        Q_EMIT snapshot({{"castle_war_guilds", guilds}});
        return;
    }
    if (op == "castle_war") {
        const auto action = command["action"].toString();
        const auto result = object(request(command));
        if (action == "admit" || (action == "start" && command["auto_admit"].toBool())) {
            for (const auto& id : result["participant_ids"].toArray()) {
                const auto peer = m_peers.find(id.toInt());
                if (peer != m_peers.end()) {
                    (void) peer->second->worker->submit({{"op", "pause"}});
                }
            }
        }
        Q_EMIT snapshot({{"castle_war", result}});
        const auto resultMessage = result["message"].toString();
        if (!resultMessage.isEmpty()) {
            Q_EMIT message(resultMessage);
        }
        return;
    }
    if (op == "catalog") {
        Q_EMIT snapshot({{"catalog", object(request(command))["items"]}});
        return;
    }
    if (op == "inspect") {
        const auto result = object(request(command));
        Q_EMIT snapshot({{"detail", result}, {"id", command["id"]}});
        return;
    }
    const auto ids = command["ids"].toArray();
    if (op == "autoplay" || op == "pause") {
        if (ids.isEmpty()) {
            throw std::runtime_error("Select one or more players first.");
        }
        if (op == "autoplay") {
            m_agentData = TestPlayerAgent::load_data(OPENKAL_REPO_ROOT,
                                                     command["profile"].toString().toStdString());
            for (const auto& value : ids) {
                if (m_stop.load()) {
                    break;
                }
                login(value.toInt());
            }
        }
        std::vector<std::future<QJsonObject>> completions;
        for (const auto& value : ids) {
            if (m_stop.load()) {
                break;
            }
            const int id = value.toInt();
            if (op == "autoplay") {
                waitOnline(id);
                const auto weapon = request({{"op", "ensure_weapon"}, {"id", id}});
                if (weapon.at("iid").as_int() > 0) {
                    Q_EMIT message(
                        QString("Player %1: supplied a missing starter weapon.").arg(id));
                }
                completions.push_back(m_peers.at(id)->worker->submit(command, m_agentData));
            } else if (const auto it = m_peers.find(id); it != m_peers.end()) {
                completions.push_back(it->second->worker->submit(command));
            }
        }
        for (auto& completion : completions) {
            (void) await(std::move(completion));
        }
        publish();
        Q_EMIT message(op == "autoplay" ? "Autonomous play started using normal game actions."
                                        : "Autonomous play paused.");
        return;
    }
    if (op == "view") {
        if (ids.size() != 1) {
            throw std::runtime_error("Select exactly one player to watch.");
        }
        const int id = ids.first().toInt();
        login(id);
        waitOnline(id);
        const auto worker = m_peers.at(id)->worker;
        const auto generation = m_peers.at(id)->generation;
        const auto detail = request({{"op", "inspect"}, {"id", id}});
        if (!m_directory.isValid()) {
            throw std::runtime_error("Could not prepare local attachment.");
        }
        const auto endpoint = m_directory.path() + "/player-" + QString::number(id) + "-" +
                              QString::number(static_cast<qulonglong>(generation)) + ".sock";
        if (const auto peer = m_peers.find(id); peer != m_peers.end()) {
            peer->second->endpoint = endpoint;
        }
        (void) await(
            worker->submit({{"op", "view"},
                            {"endpoint", endpoint},
                            {"record", QString::fromStdString(detail.at("record").as_string())}}));
        return;
    }
    std::mt19937 random(static_cast<std::uint32_t>(command["seed"].toInt(1)));
    const int radius = std::clamp(command["radius"].toInt(), 0, 4096);
    std::uniform_int_distribution<int> offset(-radius, radius);
    std::vector<std::pair<int, int>> positions;
    for (const auto& value : ids) {
        if (m_stop.load()) {
            break;
        }
        const int id = value.toInt();
        if (op == "login") {
            login(id);
            pump();
            continue;
        }
        if (op == "logout") {
            logout(id);
            continue;
        }
        if (op == "equip" || op == "unequip") {
            login(id);
            waitOnline(id);
            const auto iid = static_cast<std::uint32_t>(command["iid"].toInt());
            (void) await(m_peers.at(id)->worker->submit(command));
            bool confirmed = false;
            const auto confirmationDeadline =
                std::chrono::steady_clock::now() + std::chrono::seconds(3);
            while (!m_stop.load() && std::chrono::steady_clock::now() < confirmationDeadline) {
                const auto inventory = request({{"op", "inspect"}, {"id", id}});
                for (const auto& item : inventory.at("items").array()) {
                    if (item.at("iid").as_int() != static_cast<int>(iid)) {
                        continue;
                    }
                    confirmed = (item.at("slot").as_int() != 0) == (op == "equip");
                    break;
                }
                if (confirmed) {
                    break;
                }
                pump();
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            if (!confirmed) {
                throw std::runtime_error("Equipment change was rejected. Check class, level, slot "
                                         "conflicts and cooldowns.");
            }
            Q_EMIT snapshot(
                {{"detail", object(request({{"op", "inspect"}, {"id", id}}))}, {"id", id}});
            continue;
        }
        auto requestCommand = command;
        requestCommand.remove("ids");
        requestCommand["id"] = id;
        if (op == "teleport") {
            bool placed = false;
            std::string lastError;
            for (int attempt = 0; attempt < 100; ++attempt) {
                int dx = radius ? offset(random) : 0, dy = radius ? offset(random) : 0;
                if (dx * dx + dy * dy > radius * radius) {
                    continue;
                }
                const int x = command["x"].toInt() + dx, y = command["y"].toInt() + dy;
                const int spacing = command["spacing"].toInt(24);
                if (std::any_of(positions.begin(), positions.end(), [&](const auto& p) {
                        const double a = x - p.first, b = y - p.second;
                        return a * a + b * b < spacing * spacing;
                    })) {
                    continue;
                }
                requestCommand["x"] = x;
                requestCommand["y"] = y;
                try {
                    (void) request(requestCommand);
                    positions.emplace_back(x, y);
                    placed = true;
                    break;
                } catch (const std::exception& error) {
                    lastError = error.what();
                }
            }
            if (!placed) {
                throw std::runtime_error("Could not place all selected players: " + lastError +
                                         " Increase radius or reduce minimum spacing.");
            }
        } else {
            (void) request(requestCommand);
        }
    }
    refresh();
    Q_EMIT message(op == "equip" || op == "unequip" ? "Equipment change confirmed by the server."
                                                    : "Operation completed.");
}

QJsonObject TestPlayerBackend::await(std::future<QJsonObject> result) {
    while (result.wait_for(std::chrono::milliseconds(5)) != std::future_status::ready) {
        pump();
        if (m_stop.load()) {
            throw std::runtime_error("Test player manager is stopping.");
        }
    }
    pump();
    return result.get();
}

void TestPlayerBackend::waitOnline(int id) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
    while (!m_stop.load() && std::chrono::steady_clock::now() < deadline) {
        pump();
        const auto peer = m_peers.find(id);
        if (peer == m_peers.end()) {
            break;
        }
        if (peer->second->state["status"] != "Logging in") {
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    throw std::runtime_error("Character could not enter the world.");
}

void TestPlayerBackend::pump() {
    std::deque<QJsonObject> events;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        events.swap(m_events);
    }
    bool changed = false;
    for (const auto& event : events) {
        const int id = event["id"].toInt();
        const auto peer = m_peers.find(id);
        // A disconnected/replaced session must never update a later login of the same player.
        if (peer == m_peers.end() ||
            event["generation"].toInteger() != static_cast<qint64>(peer->second->generation)) {
            continue;
        }
        if (event.contains("state")) {
            peer->second->state = event["state"].toObject();
            changed = true;
        }
        if (event.contains("message")) {
            Q_EMIT message(QString("Player %1: %2").arg(id).arg(event["message"].toString()));
        }
        if (event.contains("openClient")) {
            Q_EMIT openClient(event["openClient"].toString());
        }
        if (event.contains("travel")) {
            auto command = event["travel"].toObject();
            command["id"] = id;
            command["generation"] = event["generation"];
            submit(std::move(command));
        }
        if (event["disconnected"].toBool()) {
            Q_EMIT message(QString("Player %1 disconnected.").arg(id));
            m_peers.erase(peer);
            changed = true;
        }
    }
    for (auto retired = m_retired.begin(); retired != m_retired.end();) {
        if (retired->second->finished()) {
            retired = m_retired.erase(retired);
        } else {
            ++retired;
        }
    }
    m_publishPending = m_publishPending || changed;
    const auto now = std::chrono::steady_clock::now();
    if (m_publishPending && now - m_lastPublish >= std::chrono::milliseconds(100)) {
        m_lastPublish = now;
        publish();
        m_publishPending = false;
    }
}
