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

#include <QJsonObject>
#include <QObject>
#include <atomic>
#include <deque>
#include <mutex>
#include <thread>

/** Runs PostgreSQL queries and local server requests on a single background thread. */
class ManagementBackend final : public QObject {
    Q_OBJECT
  public:
    /** Starts the idle worker. @param parent Optional QObject lifetime owner. */
    explicit ManagementBackend(QObject* parent = nullptr);

    /** Stops the worker after the current bounded operation completes. */
    ~ManagementBackend() override;

    /** Queues one GUI action. @param command Operation and typed arguments. */
    void submit(QJsonObject command);

  Q_SIGNALS:
    /** Publishes roster, connection health and newly received PMs. @param state Snapshot. */
    void stateChanged(QJsonObject state);

    /** Completes an explicit action. @param operation Action name. @param ok Success.
     * @param message User-facing result. @param requestId Mail draft id, if applicable. */
    void completed(QString operation, bool ok, QString message, QString requestId);

    /** Controls action availability. @param busy Whether an explicit action is running. */
    void busyChanged(bool busy);

  private:
    void run();
    std::atomic<bool> m_stop{false};
    std::mutex m_mutex;
    std::deque<QJsonObject> m_commands;
    std::thread m_thread;
};
