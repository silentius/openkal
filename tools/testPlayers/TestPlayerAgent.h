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

#include "TestPlayerNavigation.h"
#include "openkal/server/store/DataStore.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class TestPlayerData;

/** Validated spawn-area destination requested through the existing test manager. */
struct TestPlayerAreaTravel {
    /** Logical map index. */
    int map = 0;
    /** Authored spawn-area identifier. */
    int area = 0;
    /** Standable server-coordinate landing point with resolved height. */
    TestPlayerPosition position;
};

/** Stateful ordinary-protocol player controlled entirely by the test manager. */
class TestPlayerAgent final {
  public:
    /** Loads packaged gameplay catalogs and resolves navigation paths from the client profile.
     * @param resource_root Runtime resource directory containing config.pak.
     * @param profile_override Client profile, or empty for the normal default profile.
     * @return Immutable shared catalogs, throwing when the package or profile cannot be loaded.
     */
    [[nodiscard]] static std::shared_ptr<TestPlayerData>
    load_data(const std::string& resource_root, const std::string& profile_override = {});

    /** Initializes the observed player state.
     * @param character Initial character snapshot from the existing session login.
     */
    explicit TestPlayerAgent(const openkal::server::CharacterRecord& character);

    /** Releases the observed world and route. */
    ~TestPlayerAgent();

    /** Enables autonomy without changing the server's player state.
     * @param data Matching shared catalogs; mutable navigation caches remain private to this agent.
     */
    void start(std::shared_ptr<TestPlayerData> data);

    /** Pauses future decisions; in-flight ordinary actions may still complete. */
    void pause();

    /** Consumes a server packet, including while paused or being observed.
     * @param type Regular server opcode.
     * @param payload Packet payload without the frame header.
     */
    void receive(std::uint8_t type, const std::vector<std::uint8_t>& payload);

    /** Advances survival, stat allocation, beheading, quest, equipment and hunting decisions.
     * @param now Monotonic milliseconds.
     * @param send Sends a complete ordinary C2S frame; false means transport failure.
     * @param planning_budget Maximum navigation work per tick, clamped to one through eight
     * milliseconds.
     * @param city_travel Optional test-manager request for unlimited travel to a city-table index.
     * @param area_travel Optional test-manager fallback after a spawn route fails.
     */
    void tick(std::uint64_t now,
              const std::function<bool(const std::vector<std::uint8_t>&)>& send,
              std::chrono::milliseconds planning_budget = std::chrono::milliseconds{2},
              const std::function<bool(std::size_t)>& city_travel = {},
              const std::function<bool(const TestPlayerAreaTravel&)>& area_travel = {});

    /** Returns the requested city until its server teleport is confirmed.
     * @return City-table index, or empty when no city travel is pending.
     */
    [[nodiscard]] std::optional<std::size_t> pending_city_travel() const;

    /** Returns the fallback destination until the server confirms arrival.
     * @return Validated spawn destination, or empty when no fallback is pending.
     */
    [[nodiscard]] std::optional<TestPlayerAreaTravel> pending_area_travel() const;

    /** Returns whether autonomous decisions are enabled. @return Enabled state. */
    [[nodiscard]] bool active() const;

    /** Returns the latest confirmed server pose. @return Server-coordinate feet position. */
    [[nodiscard]] TestPlayerPosition position() const;

    /** Returns the current goal or waiting reason. @return Human-readable state. */
    [[nodiscard]] const std::string& status() const;

    /** Returns the latest observed level. @return Character level. */
    [[nodiscard]] int level() const;

  private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};
