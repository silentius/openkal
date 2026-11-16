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
#include "test_util.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QThread>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <libpq-fe.h>
#include <memory>
#include <thread>
#include <unistd.h>

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    const char* dsn = std::getenv("OPENKAL_TEST_POSTGRES_DSN");
    if (!dsn) {
        return 77;
    }
    using openkal::test::expect;
    using Connection = std::unique_ptr<PGconn, decltype(&PQfinish)>;
    using Result = std::unique_ptr<PGresult, decltype(&PQclear)>;
    Connection db(PQconnectdb(dsn), PQfinish);
    if (!db || PQstatus(db.get()) != CONNECTION_OK) {
        return 1;
    }
    const std::string schema = "openkal_management_backend_" + std::to_string(getpid());
    const auto sql = [&](const std::string& statement) {
        Result r(PQexec(db.get(), statement.c_str()), PQclear);
        return r && PQresultStatus(r.get()) == PGRES_COMMAND_OK;
    };
    if (!sql(
            "CREATE SCHEMA " + schema + ";SET search_path=" + schema +
            ";CREATE TABLE characters(id BIGINT PRIMARY KEY,account_id BIGINT,name TEXT,role "
            "BIGINT DEFAULT 0,level BIGINT,deletion_state BIGINT DEFAULT 0);"
            "INSERT INTO characters(id,account_id,name,level) SELECT "
            "n,n,'Hero'||lpad(n::text,4,'0'),10 FROM generate_series(1,101)n;"
            "INSERT INTO characters(id,account_id,name,level) VALUES(102,102,'Literal%Hero',20)")) {
        return 1;
    }
    const auto socket = (std::filesystem::temp_directory_path() /
                         ("openkal-console-backend-" + std::to_string(getpid()) + ".sock"))
                            .string();
    const std::string token(32, 'b');
    const auto consoleDsn = std::string(dsn) + " options='-csearch_path=" + schema + "'";
    openkal::server::ManagementApi api;
    std::string error;
    if (!api.start(socket, token, error)) {
        return 1;
    }
    api.record_pm(1, "Hero0001", "owned support fixture", false);
    std::atomic<bool> stop{false};
    std::thread server([&] {
        while (!stop.load()) {
            api.poll([&](const auto& command) {
                if (command.at("op").as_string() == "database") {
                    auto response = openkal::platform::Json::parse("{}");
                    response.object()["database_dsn"].set_type(
                        openkal::platform::Json::Type::String);
                    response.object()["database_dsn"].string() = consoleDsn;
                    return response;
                }
                auto response =
                    api.events(static_cast<std::uint64_t>(command.at("after").number()));
                response.object()["online"] = openkal::platform::Json::parse("[1]");
                return response;
            });
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    });
    {
        ManagementBackend backend;
        QJsonObject state;
        QString operation;
        bool ok = false;
        int generation = 0, completed = 0;
        bool leakedSettings = false;
        QObject::connect(&backend, &ManagementBackend::stateChanged, &app, [&](QJsonObject value) {
            leakedSettings = leakedSettings || value.contains("database_dsn") ||
                             value.contains("dsn") || value.contains("token");
            state = value;
            ++generation;
        });
        QObject::connect(&backend,
                         &ManagementBackend::completed,
                         &app,
                         [&](QString op, bool success, QString message, QString) {
                             leakedSettings = leakedSettings ||
                                              message.contains(QString::fromStdString(consoleDsn));
                             operation = op;
                             ok = success;
                             ++completed;
                         });
        const auto wait = [&](const auto& predicate) {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(8);
            while (!predicate() && std::chrono::steady_clock::now() < deadline) {
                app.processEvents();
                QThread::msleep(2);
            }
            return predicate();
        };
        backend.submit({{"op", "connect"},
                        {"socket", QString::fromStdString(socket)},
                        {"token", QString::fromStdString(token)}});
        expect(wait([&] { return generation > 0; }) && state["connected"].toBool() &&
                   state["api"].toBool() && state["total"].toInt() == 102 &&
                   state["players"].toArray().size() == 100,
               "console obtains database settings from the authenticated API and loads its roster");
        expect(!leakedSettings, "server credentials are not emitted to GUI state or activity logs");
        bool online = false, offline = false;
        for (const auto& value : state["players"].toArray()) {
            const auto row = value.toObject();
            if (row["id"].toInt() == 1) {
                online = row["presence"].toString() == "Online";
            }
            if (row["id"].toInt() == 2) {
                offline = row["presence"].toString() == "Offline";
            }
        }
        expect(online && offline && state["events"].toArray().size() == 1,
               "console joins live presence and receives support PM events");
        auto previous = generation;
        backend.submit({{"op", "search"}, {"search", "%"}});
        expect(wait([&] { return generation > previous; }) && state["total"].toInt() == 1 &&
                   state["players"].toArray().at(0).toObject()["name"].toString() == "Literal%Hero",
               "search treats SQL wildcard characters literally");
        previous = generation;
        backend.submit({{"op", "search"}, {"search", "' OR 1=1 --"}});
        expect(wait([&] { return generation > previous; }) && state["total"].toInt() == 0,
               "player search cannot inject SQL");
        previous = generation;
        backend.submit({{"op", "search"}, {"search", ""}});
        expect(wait([&] { return generation > previous; }) && state["total"].toInt() == 102,
               "empty search restores all players");
        previous = generation;
        backend.submit({{"op", "page"}, {"page", 1}});
        expect(wait([&] { return generation > previous; }) &&
                   state["players"].toArray().size() == 2 && state["page"].toInt() == 1,
               "next roster page exposes players beyond the first 100");
        const int actions = completed;
        backend.submit({{"op", "role"}, {"character", 1}, {"expected", 0}, {"role", 3}});
        expect(wait([&] { return completed > actions; }) && operation == "role" && ok,
               "role changes execute through parameterized SQL on the worker");
        const int changed = completed;
        backend.submit({{"op", "role"}, {"character", 1}, {"expected", 0}, {"role", 1}});
        expect(wait([&] { return completed > changed; }) && !ok,
               "stale role edits cannot overwrite a concurrent role change");
        Result role(PQexec(db.get(), "SELECT role FROM characters WHERE id=1"), PQclear);
        expect(role && std::string(PQgetvalue(role.get(), 0, 0)) == "3",
               "concurrent role rejection preserves the committed admin value");
        previous = generation;
        backend.submit({{"op", "connect"},
                        {"dsn", QString::fromStdString(consoleDsn)},
                        {"socket", QString::fromStdString(socket)},
                        {"token", QString::fromStdString(std::string(32, 'x'))}});
        expect(wait([&] { return generation > previous; }) && state["connected"].toBool() &&
                   !state["api"].toBool() &&
                   state["players"].toArray().at(0).toObject()["presence"].toString() == "Unknown",
               "API failure leaves database access usable and marks presence unknown");
        previous = generation;
        const int beforeRejected = completed;
        backend.submit({{"op", "connect"},
                        {"socket", QString::fromStdString(socket)},
                        {"token", QString::fromStdString(std::string(32, 'x'))}});
        expect(wait([&] { return generation > previous && completed > beforeRejected; }) &&
                   operation == "connect" && !ok && !state["connected"].toBool(),
               "automatic database connection requires valid API authentication");
    }
    stop = true;
    server.join();
    api.close();
    expect(sql("DROP SCHEMA " + schema + " CASCADE"), "remove isolated console database fixture");
    return openkal::test::g_failures ? 1 : 0;
}
