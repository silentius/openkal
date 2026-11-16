/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/server/world/FindPath.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <queue>
#include <unordered_map>
#include <vector>

namespace openkal::server {
    namespace {

        constexpr int kGridStep = 32;
        constexpr int kSearchBudget = 50;
        constexpr int kDirectApproachDistanceSq = 48 * 48;
        constexpr std::array<std::pair<int, int>, 8> kNeighborOffsets = {{{kGridStep, 0},
                                                                          {kGridStep, kGridStep},
                                                                          {0, kGridStep},
                                                                          {-kGridStep, kGridStep},
                                                                          {-kGridStep, 0},
                                                                          {-kGridStep, -kGridStep},
                                                                          {0, -kGridStep},
                                                                          {kGridStep, -kGridStep}}};

        struct SearchNode {
            int x = 0;
            int y = 0;
            int parentIndex = -1;
            int travelSteps = std::numeric_limits<int>::max();
            int estimatedRemainingSteps = 0;
        };

        struct FrontierEntry {
            int estimatedTotalSteps = 0;
            int remainingSteps = 0;
            int nodeIndex = 0;

            [[nodiscard]] bool operator<(const FrontierEntry& other) const {
                if (estimatedTotalSteps != other.estimatedTotalSteps) {
                    return estimatedTotalSteps > other.estimatedTotalSteps;
                }
                return remainingSteps > other.remainingSteps;
            }
        };

        [[nodiscard]] std::int64_t
        squared_distance(int first_x, int first_y, int second_x, int second_y) {
            const std::int64_t delta_x = static_cast<std::int64_t>(first_x) - second_x;
            const std::int64_t delta_y = static_cast<std::int64_t>(first_y) - second_y;
            return delta_x * delta_x + delta_y * delta_y;
        }

        [[nodiscard]] int grid_distance(int x, int y, int target_x, int target_y) {
            const int horizontalSteps = (std::abs(target_x - x) + kGridStep - 1) / kGridStep;
            const int verticalSteps = (std::abs(target_y - y) + kGridStep - 1) / kGridStep;
            return std::max(horizontalSteps, verticalSteps);
        }

        [[nodiscard]] std::uint64_t coordinate_key(int x, int y) {
            return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(x)) << 32u) |
                   static_cast<std::uint32_t>(y);
        }

    }  // namespace

    FindPathResult find_monster_path(int start_x,
                                     int start_y,
                                     int end_x,
                                     int end_y,
                                     int range_sq,
                                     const IsMoveTileFn& is_move_tile) {
        FindPathResult out;
        if (start_x == end_x && start_y == end_y) {
            return out;
        }
        if (!is_move_tile) {
            return out;
        }

        std::vector<SearchNode> nodes;
        nodes.reserve(kSearchBudget * kNeighborOffsets.size());
        std::unordered_map<std::uint64_t, int> nodeByCoordinate;
        nodeByCoordinate.reserve(kSearchBudget * kNeighborOffsets.size());
        std::priority_queue<FrontierEntry> frontier;

        const int startEstimate = grid_distance(start_x, start_y, end_x, end_y);
        nodes.push_back({start_x, start_y, -1, 0, startEstimate});
        nodeByCoordinate.emplace(coordinate_key(start_x, start_y), 0);
        frontier.push({startEstimate, startEstimate, 0});

        int closestNodeIndex = 0;
        int selectedNodeIndex = -1;
        int processedNodes = 0;
        const std::int64_t allowedDistanceSq =
            std::max<std::int64_t>(std::max(0, range_sq), kDirectApproachDistanceSq);

        while (!frontier.empty() && processedNodes < kSearchBudget) {
            const FrontierEntry next = frontier.top();
            frontier.pop();
            const SearchNode current = nodes[static_cast<std::size_t>(next.nodeIndex)];
            if (next.estimatedTotalSteps != current.travelSteps + current.estimatedRemainingSteps) {
                continue;
            }
            ++processedNodes;

            const SearchNode& closest = nodes[static_cast<std::size_t>(closestNodeIndex)];
            if (current.estimatedRemainingSteps < closest.estimatedRemainingSteps) {
                closestNodeIndex = next.nodeIndex;
            }

            if (squared_distance(current.x, current.y, end_x, end_y) <= allowedDistanceSq &&
                is_move_tile(current.x, current.y, end_x - current.x, end_y - current.y)) {
                selectedNodeIndex = next.nodeIndex;
                break;
            }

            for (const auto& [offsetX, offsetY] : kNeighborOffsets) {
                if (!is_move_tile(current.x, current.y, offsetX, offsetY)) {
                    continue;
                }
                const int neighborX = current.x + offsetX;
                const int neighborY = current.y + offsetY;
                const int candidateSteps = current.travelSteps + 1;
                const std::uint64_t key = coordinate_key(neighborX, neighborY);
                const auto known = nodeByCoordinate.find(key);

                int neighborIndex = 0;
                if (known == nodeByCoordinate.end()) {
                    neighborIndex = static_cast<int>(nodes.size());
                    const int estimate = grid_distance(neighborX, neighborY, end_x, end_y);
                    nodes.push_back(
                        {neighborX, neighborY, next.nodeIndex, candidateSteps, estimate});
                    nodeByCoordinate.emplace(key, neighborIndex);
                } else {
                    neighborIndex = known->second;
                    SearchNode& neighbor = nodes[static_cast<std::size_t>(neighborIndex)];
                    if (candidateSteps >= neighbor.travelSteps) {
                        continue;
                    }
                    neighbor.parentIndex = next.nodeIndex;
                    neighbor.travelSteps = candidateSteps;
                }

                const SearchNode& neighbor = nodes[static_cast<std::size_t>(neighborIndex)];
                frontier.push({neighbor.travelSteps + neighbor.estimatedRemainingSteps,
                               neighbor.estimatedRemainingSteps,
                               neighborIndex});
            }
        }

        if (selectedNodeIndex < 0 && closestNodeIndex != 0) {
            selectedNodeIndex = closestNodeIndex;
        }
        if (selectedNodeIndex <= 0) {
            return out;
        }

        std::vector<std::pair<int, int>> reversePath;
        for (int index = selectedNodeIndex; index > 0;
             index = nodes[static_cast<std::size_t>(index)].parentIndex) {
            const SearchNode& node = nodes[static_cast<std::size_t>(index)];
            reversePath.emplace_back(node.x, node.y);
        }

        out.found = true;
        out.waypoints.assign(reversePath.rbegin(), reversePath.rend());
        return out;
    }

}  // namespace openkal::server
