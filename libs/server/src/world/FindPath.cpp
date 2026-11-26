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
#include <bitset>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>

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
        constexpr auto kMaxSearchNodes = 1 + kSearchBudget * kNeighborOffsets.size();
        constexpr int kSearchDiameter = 2 * kSearchBudget + 1;

        struct SearchNode {
            int x;
            int y;
            int parentIndex;
            int travelSteps;
            int estimatedRemainingSteps;
        };

        struct FrontierEntry {
            int estimatedTotalSteps;
            int remainingSteps;
            int nodeIndex;

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

        std::array<SearchNode, kMaxSearchNodes> nodes;
        std::size_t node_count = 1;
        std::array<int, kSearchDiameter * kSearchDiameter> node_by_cell;
        std::bitset<kSearchDiameter * kSearchDiameter> seen_cells;
        // Each expansion can discover a cell at most one grid step farther from the start.
        const auto slot_for = [start_x, start_y](int x, int y) {
            const int column = (x - start_x) / kGridStep + kSearchBudget;
            const int row = (y - start_y) / kGridStep + kSearchBudget;
            return static_cast<std::size_t>(row * kSearchDiameter + column);
        };
        std::array<FrontierEntry, kMaxSearchNodes> frontier;
        auto frontier_end = frontier.begin();
        const auto enqueue = [&](FrontierEntry entry) {
            *frontier_end++ = entry;
            std::push_heap(frontier.begin(), frontier_end);
        };

        const int startEstimate = grid_distance(start_x, start_y, end_x, end_y);
        nodes[0] = {start_x, start_y, -1, 0, startEstimate};
        const auto start_slot = slot_for(start_x, start_y);
        node_by_cell[start_slot] = 0;
        seen_cells[start_slot] = true;
        enqueue({startEstimate, startEstimate, 0});

        int closestNodeIndex = 0;
        int selectedNodeIndex = -1;
        int processedNodes = 0;
        const std::int64_t allowedDistanceSq =
            std::max<std::int64_t>(std::max(0, range_sq), kDirectApproachDistanceSq);

        while (frontier_end != frontier.begin() && processedNodes < kSearchBudget) {
            std::pop_heap(frontier.begin(), frontier_end);
            const FrontierEntry next = *--frontier_end;
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
                const auto slot = slot_for(neighborX, neighborY);

                int neighborIndex = 0;
                if (!seen_cells[slot]) {
                    neighborIndex = static_cast<int>(node_count);
                    const int estimate = grid_distance(neighborX, neighborY, end_x, end_y);
                    nodes[node_count++] = {
                        neighborX, neighborY, next.nodeIndex, candidateSteps, estimate};
                    node_by_cell[slot] = neighborIndex;
                    seen_cells[slot] = true;
                } else {
                    neighborIndex = node_by_cell[slot];
                    SearchNode& neighbor = nodes[static_cast<std::size_t>(neighborIndex)];
                    if (candidateSteps >= neighbor.travelSteps) {
                        continue;
                    }
                    neighbor.parentIndex = next.nodeIndex;
                    neighbor.travelSteps = candidateSteps;
                }

                const SearchNode& neighbor = nodes[static_cast<std::size_t>(neighborIndex)];
                enqueue({neighbor.travelSteps + neighbor.estimatedRemainingSteps,
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

        std::array<std::pair<int, int>, kSearchBudget> reverse_path;
        auto reverse_end = reverse_path.begin();
        for (int index = selectedNodeIndex; index > 0;
             index = nodes[static_cast<std::size_t>(index)].parentIndex) {
            const SearchNode& node = nodes[static_cast<std::size_t>(index)];
            *reverse_end++ = {node.x, node.y};
        }

        out.found = true;
        out.waypoints.assign(std::make_reverse_iterator(reverse_end), reverse_path.rend());
        return out;
    }

}  // namespace openkal::server
