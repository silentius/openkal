/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include "openkal/server/TestControl.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QTemporaryDir>
#include <atomic>
#include <chrono>
#include <deque>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

class TestPlayerData;
class TestPlayerWorker;

/** Coordinates administration and forwards events from independent player threads to the UI. */
class TestPlayerBackend final : public QObject {
    Q_OBJECT
  public:
    /** Starts the network worker. @param parent Qt lifetime owner. */
    explicit TestPlayerBackend(QObject* parent = nullptr);

    /** Stops the worker and logs out its sessions. */
    ~TestPlayerBackend() override;

    /** Queues a GUI command. @param command Operation and arguments. */
    void submit(QJsonObject command);

  Q_SIGNALS:
    /** Delivers roster state. @param state Players and managed guilds. */
    void snapshot(QJsonObject state);

    /** Reports a completed operation. @param message User-facing result. */
    void message(QString message);

    /** Controls GUI availability. @param busy Whether a command is running. */
    void busyChanged(bool busy);

    /** Opens the normal client against a local bridge. @param endpoint Unix socket path. */
    void openClient(QString endpoint);

  private:
    struct Peer;
    void run();
    void execute(const QJsonObject& command);
    void pump();
    void refresh();
    void login(int id);
    void logout(int id);
    openkal::platform::Json request(const QJsonObject& command);
    void publish();
    QJsonObject await(std::future<QJsonObject> result);
    void waitOnline(int id);
    std::atomic<bool> m_stop{false};
    std::mutex m_mutex;
    std::deque<QJsonObject> m_commands, m_events;
    std::thread m_thread;
    openkal::server::TestControlClient m_admin;
    std::string m_host, m_token;
    std::uint16_t m_port = 30001;
    openkal::server::TlsConfig m_tls;
    std::map<int, std::unique_ptr<Peer>> m_peers;
    // Retired sessions finish on their own threads without holding up roster events.
    std::map<int, std::shared_ptr<TestPlayerWorker>> m_retired;
    QJsonArray m_players, m_guilds;
    int m_playerCount = 50, m_guildCount = 8;
    bool m_connected = false;
    bool m_publishPending = false;
    std::uint64_t m_generation = 0;
    QTemporaryDir m_directory;
    std::shared_ptr<TestPlayerData> m_agentData;
    std::chrono::steady_clock::time_point m_lastPublish{};
};
