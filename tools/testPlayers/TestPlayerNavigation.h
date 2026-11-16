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

#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

/** Server-coordinate feet position used by the autonomous test players. */
struct TestPlayerPosition {
    /** Horizontal server pixel coordinates. */
    int x = 0;
    int y = 0;
    /** Server height, ten units per client vertical unit. */
    int z = 0;
};

/** Validates a complete movement segment and resolves its destination height. */
using TestPlayerStep = std::function<std::optional<int>(TestPlayerPosition, int, int)>;

/** State of a bounded incremental navigation search. */
enum class TestPlayerRouteState { Searching, Found, Unavailable };

/** Incremental weighted A* job, yielding to network and survival work between slices. */
class TestPlayerRouteSearch final {
  public:
    /** Starts a route search.
     * @param start Current pose.
     * @param goal Target pose.
     * @param reach Acceptable horizontal distance.
     * @param step Full-segment validation and destination-height resolver.
     * @param budget Total expanded-node limit.
     * @param spacing Grid spacing in server pixels; coarse grids use centered cells and refine
     * obstructed neighborhoods with eight-pixel steps.
     */
    TestPlayerRouteSearch(TestPlayerPosition start,
                          TestPlayerPosition goal,
                          int reach,
                          TestPlayerStep step,
                          std::size_t budget = 65536,
                          int spacing = 8);

    /** Releases the pending search. */
    ~TestPlayerRouteSearch();

    /** Expands a bounded slice of the search.
     * @param slice Maximum nodes to expand in this call.
     * @param time_budget Wall-time allowance, checked between segment validations; one validation
     * may overrun it. Unfinished nodes resume without repeating validated edges. Zero disables
     * the time limit for synchronous callers.
     * @return Current completion state.
     */
    TestPlayerRouteState advance(std::size_t slice = 128,
                                 std::chrono::milliseconds time_budget = std::chrono::milliseconds{
                                     0});

    /** Returns search progress across slices. @return Number of expanded nodes. */
    [[nodiscard]] std::size_t expanded_nodes() const;

    /** Returns a completed route.
     * @return Waypoints excluding the start; empty until found.
     */
    [[nodiscard]] const std::vector<TestPlayerPosition>& route() const;

  private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

/** Plans a bounded A* route over the existing eight-pixel navigation subcells.
 * The validator must check the whole segment, including body clearance. Missing
 * data must reject a step. An empty route means no route within the search budget.
 * @param start Current server position.
 * @param goal Target server position; its height is checked by the validator.
 * @param reach Acceptable horizontal distance from the target, in server pixels.
 * @param step Segment validator and height resolver.
 * @param budget Maximum expanded nodes, bounding work on the network worker.
 * @return Waypoints excluding the start, or an empty route on failure/already arrived.
 */
[[nodiscard]] std::vector<TestPlayerPosition> test_player_route(TestPlayerPosition start,
                                                                TestPlayerPosition goal,
                                                                int reach,
                                                                const TestPlayerStep& step,
                                                                std::size_t budget = 16384);
