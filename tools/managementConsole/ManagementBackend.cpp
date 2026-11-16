/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "ManagementBackend.h"

#include "openkal/server/ManagementApi.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <algorithm>
#include <chrono>
#include <libpq-fe.h>
#include <memory>
#include <stdexcept>
#include <unordered_set>

namespace {
    using J = openkal::platform::Json;
    using Connection = std::unique_ptr<PGconn, decltype(&PQfinish)>;
    using Result = std::unique_ptr<PGresult, decltype(&PQclear)>;
    Result query(PGconn* db, const char* sql, const std::vector<std::string>& values = {}) {
        std::vector<const char*> pointers;
        for (const auto& value : values) {
            pointers.push_back(value.c_str());
        }
        Result result(PQexecParams(db,
                                   sql,
                                   static_cast<int>(pointers.size()),
                                   nullptr,
                                   pointers.data(),
                                   nullptr,
                                   nullptr,
                                   0),
                      PQclear);
        if (!result || (PQresultStatus(result.get()) != PGRES_TUPLES_OK &&
                        PQresultStatus(result.get()) != PGRES_COMMAND_OK)) {
            throw std::runtime_error(
                "Database query failed; check connectivity and apply the current server schema.");
        }
        return result;
    }
    QJsonObject api(const QString& path, const QString& token, const QJsonObject& command) {
        const auto bytes = QJsonDocument(command).toJson(QJsonDocument::Compact);
        const auto response = openkal::server::management_request(
            path.toStdString(),
            token.toStdString(),
            J::parse(std::string_view(bytes.constData(), static_cast<std::size_t>(bytes.size()))));
        return QJsonDocument::fromJson(QByteArray::fromStdString(J::stringify(response, 0)))
            .object();
    }
    std::string pattern(QString search) {
        search.replace("\\", "\\\\").replace("%", "\\%").replace("_", "\\_");
        return "%" + search.toStdString() + "%";
    }
}  // namespace
ManagementBackend::ManagementBackend(QObject* parent)
    : QObject(parent), m_thread([this] { run(); }) {}
ManagementBackend::~ManagementBackend() {
    m_stop.store(true);
    if (m_thread.joinable()) {
        m_thread.join();
    }
}
void ManagementBackend::submit(QJsonObject command) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_commands.size() < 64) {
        m_commands.push_back(std::move(command));
    }
}
void ManagementBackend::run() {
    Connection db(nullptr, PQfinish);
    QString socket, token, search, instance;
    int page = 0;
    double cursor = 0;
    bool moreEvents = false;
    auto nextRefresh = std::chrono::steady_clock::now();
    const auto refresh = [&] {
        QJsonObject state{{"connected", db && PQstatus(db.get()) == CONNECTION_OK},
                          {"api", false},
                          {"players", QJsonArray{}},
                          {"events", QJsonArray{}},
                          {"page", page},
                          {"total", 0}};
        if (!state["connected"].toBool()) {
            Q_EMIT stateChanged(state);
            return;
        }
        QJsonObject status;
        if (!socket.isEmpty() && !token.isEmpty()) {
            status = api(socket, token, {{"op", "status"}, {"after", cursor}});
        }
        if (!status.contains("error") && !instance.isEmpty() &&
            status["instance"].toString() != instance) {
            status = api(socket, token, {{"op", "status"}, {"after", 0}});
            if (!status.contains("error")) {
                status["gap"] = true;
            }
        }
        const bool apiReady = !status.isEmpty() && !status.contains("error");
        state["api"] = apiReady;
        moreEvents = apiReady && status["more"].toBool();
        if (apiReady) {
            instance = status["instance"].toString();
            cursor = status["cursor"].toDouble();
            state["events"] = status["events"];
            state["gap"] = status["gap"];
        } else if (status.contains("error")) {
            state["api_error"] = status["error"];
        }
        std::unordered_set<int> online;
        for (const auto& value : status["online"].toArray()) {
            online.insert(value.toInt());
        }
        const auto filter = pattern(search);
        const auto total = query(
            db.get(),
            "SELECT count(*) FROM characters WHERE name ILIKE $1 ESCAPE E'\\\\' OR id::text=$2",
            {filter, search.toStdString()});
        const int count = QString::fromUtf8(PQgetvalue(total.get(), 0, 0)).toInt();
        page = std::clamp(page, 0, std::max(0, (count - 1) / 100));
        state["page"] = page;
        state["total"] = count;
        const auto result =
            query(db.get(),
                  "SELECT id,account_id,name,role,level,deletion_state FROM characters "
                  "WHERE name ILIKE $1 ESCAPE E'\\\\' OR id::text=$2 ORDER BY lower(name),id LIMIT "
                  "100 OFFSET $3",
                  {filter, search.toStdString(), std::to_string(page * 100)});
        QJsonArray players;
        for (int row = 0; row < PQntuples(result.get()); ++row) {
            const auto value = [&](int column) {
                return QString::fromUtf8(PQgetvalue(result.get(), row, column));
            };
            const int id = value(0).toInt();
            players.append(QJsonObject{
                {"id", id},
                {"account", value(1).toInt()},
                {"name", value(2)},
                {"role", value(3).toInt()},
                {"level", value(4).toInt()},
                {"deletion", value(5).toInt()},
                {"presence", apiReady ? (online.count(id) ? "Online" : "Offline") : "Unknown"}});
        }
        state["players"] = players;
        Q_EMIT stateChanged(state);
    };
    while (!m_stop.load()) {
        QJsonObject command;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_commands.empty()) {
                command = m_commands.front();
                m_commands.pop_front();
            }
        }
        if (!command.isEmpty()) {
            const auto operation = command["op"].toString();
            Q_EMIT busyChanged(true);
            try {
                if (operation == "connect") {
                    db.reset();
                    socket = command["socket"].toString();
                    token = command["token"].toString();
                    cursor = 0;
                    instance.clear();
                    search.clear();
                    page = 0;
                    auto dsn = command["dsn"].toString().toStdString();
                    if (dsn.empty()) {
                        if (socket.isEmpty() || token.toUtf8().size() < 32) {
                            throw std::runtime_error(
                                "Enter the local API socket and token to use server database "
                                "settings, or supply a database override.");
                        }
                        const auto settings = api(socket, token, {{"op", "database"}});
                        if (settings.contains("error")) {
                            throw std::runtime_error(settings["error"].toString().toStdString());
                        }
                        dsn = settings["database_dsn"].toString().toStdString();
                        if (dsn.empty()) {
                            throw std::runtime_error(
                                "Server returned no PostgreSQL connection settings.");
                        }
                    }
                    const char* keys[] = {"dbname", "connect_timeout", nullptr};
                    const char* values[] = {dsn.c_str(), "3", nullptr};
                    db.reset(PQconnectdbParams(keys, values, 1));
                    if (!db || PQstatus(db.get()) != CONNECTION_OK) {
                        db.reset();
                        throw std::runtime_error("Cannot connect to PostgreSQL. Check the "
                                                 "connection string and database access.");
                    }
                    PQsetNoticeProcessor(db.get(), [](void*, const char*) {}, nullptr);
                    (void) query(db.get(), "SET statement_timeout='3s'");
                    (void) query(db.get(), "SET lock_timeout='1s'");
                } else if (operation == "disconnect") {
                    db.reset();
                    token.clear();
                } else {
                    if (!db || PQstatus(db.get()) != CONNECTION_OK) {
                        throw std::runtime_error("Connect to the database first.");
                    }
                    if (operation == "search") {
                        search = command["search"].toString();
                        page = 0;
                    } else if (operation == "page") {
                        page = std::max(0, command["page"].toInt());
                    } else if (operation == "role") {
                        const int role = command["role"].toInt(-1),
                                  old = command["expected"].toInt(-1),
                                  id = command["character"].toInt();
                        if (role < 0 || role > 3 || old < 0 || old > 3 || id <= 0) {
                            throw std::runtime_error("Invalid role update.");
                        }
                        const auto result =
                            query(db.get(),
                                  "UPDATE characters SET role=$1 WHERE id=$2 AND role=$3 AND "
                                  "deletion_state=0 RETURNING id",
                                  {std::to_string(role), std::to_string(id), std::to_string(old)});
                        if (PQntuples(result.get()) != 1) {
                            throw std::runtime_error(
                                "Role changed elsewhere or character is unavailable. Refresh and "
                                "review before applying again.");
                        }
                    } else if (operation == "mail" || operation == "pm") {
                        if (socket.isEmpty() || token.toUtf8().size() < 32) {
                            throw std::runtime_error(
                                "Configure the local API socket and token first.");
                        }
                        const auto response = api(socket, token, command);
                        if (response.contains("error")) {
                            throw std::runtime_error(response["error"].toString().toStdString());
                        }
                    } else if (operation != "refresh") {
                        throw std::runtime_error("Unknown console operation.");
                    }
                }
                if (operation != "search" && operation != "page" && operation != "refresh") {
                    Q_EMIT completed(operation,
                                     true,
                                     operation == "mail" ? "Mail persisted and queued for delivery."
                                     : operation == "pm" ? "Private message delivered."
                                     : operation == "role"       ? "Role updated."
                                     : operation == "disconnect" ? "Disconnected."
                                     : command["dsn"].toString().isEmpty()
                                         ? "Connected using server database settings."
                                         : "Connected using database override.",
                                     command["request_id"].toString());
                }
                try {
                    refresh();
                } catch (const std::exception& error) {
                    // A committed action must not be reported as failed by a later refresh.
                    Q_EMIT completed("refresh", false, QString::fromUtf8(error.what()), {});
                    db.reset();
                    Q_EMIT stateChanged(QJsonObject{{"connected", false},
                                                    {"api", false},
                                                    {"players", QJsonArray{}},
                                                    {"events", QJsonArray{}}});
                }
            } catch (const std::exception& error) {
                Q_EMIT completed(operation,
                                 false,
                                 QString::fromUtf8(error.what()),
                                 command["request_id"].toString());
                if (!db || PQstatus(db.get()) != CONNECTION_OK) {
                    db.reset();
                    Q_EMIT stateChanged(QJsonObject{{"connected", false},
                                                    {"api", false},
                                                    {"players", QJsonArray{}},
                                                    {"events", QJsonArray{}}});
                }
            }
            Q_EMIT busyChanged(false);
            nextRefresh = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(moreEvents ? 50 : 2000);
        } else if (db && std::chrono::steady_clock::now() >= nextRefresh) {
            try {
                refresh();
            } catch (...) {
                db.reset();
                Q_EMIT stateChanged(QJsonObject{{"connected", false},
                                                {"api", false},
                                                {"players", QJsonArray{}},
                                                {"events", QJsonArray{}}});
            }
            nextRefresh = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(moreEvents ? 50 : 2000);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}
