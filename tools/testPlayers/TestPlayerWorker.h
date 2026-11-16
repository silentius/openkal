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

#include "TestPlayerAgent.h"
#include "openkal/server/TestControl.h"

#include <QJsonObject>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <mutex>
#include <thread>

/** Owns one gameplay session, spectator bridge and agent on a dedicated thread. */
class TestPlayerWorker final {
  public:
    /** Receives value-only state, travel, message and viewer events on the player thread. */
    using Events = std::function<void(QJsonObject)>;

    /** Starts the player loop.
     * @param character Initial server snapshot.
     * @param session Selected gameplay transport; ownership transfers to the player thread.
     * @param token Test-control token used to refresh the spectator snapshot.
     * @param events Thread-safe event sink; must outlive this worker.
     */
    TestPlayerWorker(openkal::server::CharacterRecord character,
                     std::unique_ptr<openkal::server::ISession> session,
                     std::string token,
                     Events events);

    /** Stops and joins the player before releasing its sessions. */
    ~TestPlayerWorker();

    /** Queues an operation without accessing live player state from another thread.
     * @param command Autoplay, pause, view, view_closed, equip, unequip or autonomous travel.
     * @param data Immutable catalogs for autoplay; ignored by other operations.
     * @return Completion or validated teleport request, propagating operation failures.
     */
    [[nodiscard]] std::future<QJsonObject> submit(QJsonObject command,
                                                  std::shared_ptr<TestPlayerData> data = {});

    /** Wakes the loop to stop; call on all workers before joining a population. */
    void stop();

    /** Reports when session cleanup is complete and joining cannot wait on network work.
     * @return True after the player loop has released its sessions and pending commands.
     */
    [[nodiscard]] bool finished() const;

  private:
    void run();
    bool pump();
    void publish(bool force = false);
    QJsonObject execute(const QJsonObject& command, std::shared_ptr<TestPlayerData> data);
    void handleServer(std::uint8_t type, const std::vector<std::uint8_t>& payload);
    void handleViewer(std::uint8_t type, const std::vector<std::uint8_t>& payload);
    void closeViewer();
    std::unique_ptr<openkal::server::ISession> m_server, m_viewer;
    std::unique_ptr<openkal::server::IAcceptor> m_acceptor;
    std::vector<std::uint8_t> m_rx, m_viewerRx;
    openkal::server::CharacterRecord m_character;
    TestPlayerAgent m_agent;
    std::string m_token;
    Events m_events;
    bool m_online = false, m_forwarding = false, m_viewing = false;
    std::chrono::steady_clock::time_point m_started = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point m_lastPublish{};
    QJsonObject m_lastState;
    std::atomic<bool> m_stop{false};
    std::mutex m_mutex;
    std::condition_variable m_wake;
    std::deque<std::packaged_task<QJsonObject()>> m_commands;
    std::atomic<bool> m_finished{false};
    std::thread m_thread;
};
