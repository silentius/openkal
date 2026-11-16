/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "TestPlayerNavigation.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <queue>
#include <unordered_map>

namespace {
    struct Key {
        int x = 0, y = 0, z = 0;
        bool operator==(const Key& other) const {
            return x == other.x && y == other.y && z == other.z;
        }
        bool operator!=(const Key& other) const {
            return !(*this == other);
        }
    };
    struct KeyHash {
        std::size_t operator()(const Key& value) const {
            const auto xy =
                (static_cast<std::uint64_t>(static_cast<std::uint32_t>(value.x)) << 32) |
                static_cast<std::uint32_t>(value.y);
            return std::hash<std::uint64_t>{}(xy) ^ (std::hash<int>{}(value.z) * 0x9e3779b9u);
        }
    };
    Key key(TestPlayerPosition position) {
        return {position.x, position.y, position.z};
    }
    double distance(TestPlayerPosition a, TestPlayerPosition b) {
        return std::hypot(static_cast<double>(a.x) - b.x, static_cast<double>(a.y) - b.y);
    }
    double estimate(TestPlayerPosition from, TestPlayerPosition goal, int reach) {
        const double dx = std::abs(from.x - goal.x), dy = std::abs(from.y - goal.y);
        const double octile = std::max(dx, dy) + (std::sqrt(2.0) - 1.0) * std::min(dx, dy);
        // A small weight avoids expanding every equivalent route across large open fields.
        // The full segment validator remains authoritative for every explored edge.
        // Octile distance overestimates a circular reach by at most 8.3 percent.
        return 1.1 * std::max(0.0, octile - std::max(12, reach) * 1.083);
    }
    struct Node {
        TestPlayerPosition position;
        Key parent;
        double cost = std::numeric_limits<double>::infinity();
        bool closed = false;
    };
    // Internal control flow: an unfinished node retains its validated edges for the next slice.
    struct PlanningSliceExpired {};
    struct CheckedStep {
        int x = 0, y = 0;
        std::optional<int> height;
    };
    struct Pending {
        double estimate;
        Key id;
        bool operator<(const Pending& other) const {
            return estimate > other.estimate;
        }
    };
}  // namespace

class TestPlayerRouteSearch::Impl {
    friend class TestPlayerRouteSearch;
    TestPlayerPosition m_start, m_goal;
    int m_reach, m_spacing;
    TestPlayerStep m_step;
    std::size_t m_budget;
    std::size_t m_expanded = 0;
    std::unordered_map<Key, Node, KeyHash> m_nodes;
    std::priority_queue<Pending> m_pending;
    std::vector<TestPlayerPosition> m_route;
    std::optional<Key> m_expanding;
    std::vector<CheckedStep> m_checked;
    TestPlayerRouteState m_state = TestPlayerRouteState::Searching;

  public:
    Impl(TestPlayerPosition start,
         TestPlayerPosition goal,
         int reach,
         TestPlayerStep step,
         std::size_t budget,
         int spacing)
        : m_start(start), m_goal(goal), m_reach(reach), m_spacing(spacing), m_step(std::move(step)),
          m_budget(budget) {
        m_checked.reserve(48);
        if (!m_step || reach < 0 || budget == 0 || spacing < 1 || spacing > 32 || start.x < 0 ||
            start.y < 0 || start.x >= 524288 || start.y >= 524288 || goal.x < 0 || goal.y < 0 ||
            goal.x >= 524288 || goal.y >= 524288) {
            m_state = TestPlayerRouteState::Unavailable;
        } else if (distance(start, goal) <= reach) {
            m_state = TestPlayerRouteState::Found;
        } else {
            const auto first = key(start);
            m_nodes[first] = {start, first, 0, false};
            m_pending.push({estimate(start, goal, reach), first});
        }
    }
};

TestPlayerRouteSearch::TestPlayerRouteSearch(TestPlayerPosition start,
                                             TestPlayerPosition goal,
                                             int reach,
                                             TestPlayerStep step,
                                             std::size_t budget,
                                             int spacing)
    : m_impl(std::make_unique<Impl>(start, goal, reach, std::move(step), budget, spacing)) {}
TestPlayerRouteSearch::~TestPlayerRouteSearch() = default;
const std::vector<TestPlayerPosition>& TestPlayerRouteSearch::route() const {
    return m_impl->m_route;
}

std::size_t TestPlayerRouteSearch::expanded_nodes() const {
    return m_impl->m_expanded;
}

TestPlayerRouteState TestPlayerRouteSearch::advance(std::size_t slice,
                                                    std::chrono::milliseconds time_budget) {
    auto& s = *m_impl;
    if (s.m_state != TestPlayerRouteState::Searching) {
        return s.m_state;
    }
    const auto first = key(s.m_start);
    const auto deadline = std::chrono::steady_clock::now() + time_budget;
    bool expanded = false, validated = false;
    while (slice > 0 && (s.m_expanding || (!s.m_pending.empty() && s.m_budget > 0))) {
        if (expanded && time_budget.count() > 0 && std::chrono::steady_clock::now() >= deadline) {
            break;
        }
        if (!s.m_expanding) {
            const auto id = s.m_pending.top().id;
            s.m_pending.pop();
            auto& entry = s.m_nodes.at(id);
            if (entry.closed) {
                continue;
            }
            entry.closed = true;
            --s.m_budget;
            ++s.m_expanded;
            s.m_expanding = id;
            s.m_checked.clear();
        }
        expanded = true;
        --slice;
        const auto id = *s.m_expanding;
        const auto position = s.m_nodes.at(id).position;
        const double cost = s.m_nodes.at(id).cost;
        auto& checked = s.m_checked;
        const auto step = [&](int x, int y) {
            for (const auto& previous : checked) {
                if (previous.x == x && previous.y == y) {
                    return previous.height;
                }
            }
            if (validated && time_budget.count() > 0 &&
                std::chrono::steady_clock::now() >= deadline) {
                throw PlanningSliceExpired{};
            }
            const auto height = s.m_step(position, x, y);
            validated = true;
            checked.push_back({x, y, height});
            return height;
        };
        try {
            bool arrived = distance(position, s.m_goal) <= s.m_reach;
            // Finish the exact off-grid goal with cardinal-safe edges as well.
            if (!arrived && distance(position, s.m_goal) <= s.m_spacing * 1.5) {
                const bool corner = position.x == s.m_goal.x || position.y == s.m_goal.y ||
                                    (step(s.m_goal.x, position.y) && step(position.x, s.m_goal.y));
                const auto height = corner ? step(s.m_goal.x, s.m_goal.y) : std::nullopt;
                if (height) {
                    s.m_route.push_back({s.m_goal.x, s.m_goal.y, *height});
                    arrived = true;
                }
            }
            if (arrived) {
                for (auto current = id; current != first; current = s.m_nodes.at(current).parent) {
                    s.m_route.push_back(s.m_nodes.at(current).position);
                }
                std::reverse(s.m_route.begin(), s.m_route.end());
                s.m_state = TestPlayerRouteState::Found;
                return s.m_state;
            }
            // Coarse routes cross area tiles through their centers rather than hugging fences.
            const int anchorX = s.m_spacing > 8
                                    ? position.x / s.m_spacing * s.m_spacing + s.m_spacing / 2
                                    : position.x;
            const int anchorY = s.m_spacing > 8
                                    ? position.y / s.m_spacing * s.m_spacing + s.m_spacing / 2
                                    : position.y;
            const auto visit = [&](int x, int y, bool checkCorner = true) {
                if (x < 0 || y < 0 || x >= 524288 || y >= 524288) {
                    return false;
                }
                if (checkCorner && x != position.x && y != position.y &&
                    (!step(x, position.y) || !step(position.x, y))) {
                    return false;
                }
                const auto height = step(x, y);
                if (!height) {
                    return false;
                }
                const auto next = key({x, y, *height});
                const double nextCost = cost + distance(position, {x, y, *height});
                auto& candidate = s.m_nodes[next];
                if (!candidate.closed && nextCost < candidate.cost) {
                    candidate = {{x, y, *height}, id, nextCost, false};
                    s.m_pending.push(
                        {nextCost + estimate(candidate.position, s.m_goal, s.m_reach), next});
                }
                return true;
            };
            // A fully checked longer segment reduces work across open ground. It is only
            // an additional edge; obstacles still retain all ordinary short alternatives.
            const double remaining = distance(position, s.m_goal);
            if (s.m_spacing > 8 && remaining > s.m_spacing * 4) {
                int x = s.m_goal.x, y = s.m_goal.y;
                if (remaining > 256) {
                    x = static_cast<int>(position.x + (s.m_goal.x - position.x) * 256 / remaining);
                    y = static_cast<int>(position.y + (s.m_goal.y - position.y) * 256 / remaining);
                    x = x / s.m_spacing * s.m_spacing + s.m_spacing / 2;
                    y = y / s.m_spacing * s.m_spacing + s.m_spacing / 2;
                }
                (void) visit(x, y, false);
            }
            bool obstructed = false;
            for (int dy = -s.m_spacing; dy <= s.m_spacing; dy += s.m_spacing) {
                for (int dx = -s.m_spacing; dx <= s.m_spacing; dx += s.m_spacing) {
                    const int x = anchorX + dx, y = anchorY + dy;
                    if (x == position.x && y == position.y) {
                        continue;
                    }
                    obstructed |= !visit(x, y);
                }
            }
            // Refine only where the small neighborhood is constrained. A distant wall
            // intersecting a coarse edge must not turn the whole riverbank into a fine grid.
            bool narrow = false;
            if (obstructed && s.m_spacing > 8) {
                for (int dy = -16; dy <= 16; dy += 16) {
                    for (int dx = -16; dx <= 16; dx += 16) {
                        if (dx != 0 || dy != 0) {
                            narrow |= !step(position.x + dx, position.y + dy);
                        }
                    }
                }
            }
            if (narrow) {
                for (int dy = -8; dy <= 8; dy += 8) {
                    for (int dx = -8; dx <= 8; dx += 8) {
                        if (dx != 0 || dy != 0) {
                            (void) visit(position.x + dx, position.y + dy);
                        }
                    }
                }
            }
        } catch (const PlanningSliceExpired&) {
            return TestPlayerRouteState::Searching;
        }
        s.m_expanding.reset();
    }
    if (!s.m_expanding && (s.m_pending.empty() || s.m_budget == 0)) {
        s.m_state = TestPlayerRouteState::Unavailable;
    }
    return s.m_state;
}

std::vector<TestPlayerPosition> test_player_route(TestPlayerPosition start,
                                                  TestPlayerPosition goal,
                                                  int reach,
                                                  const TestPlayerStep& step,
                                                  std::size_t budget) {
    TestPlayerRouteSearch search(start, goal, reach, step, budget);
    (void) search.advance(budget);
    return search.route();
}
