/*
 * OpenKal - an open source KalOnline client and server
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "ObjectMask.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace openkal::area_editor {
    namespace {

        struct Point2 {
            float x = 0.0f;
            float y = 0.0f;
        };

        struct Segment2 {
            float x0 = 0.0f;
            float y0 = 0.0f;
            float x1 = 0.0f;
            float y1 = 0.0f;
        };

        struct Tri2 {
            Point2 a;
            Point2 b;
            Point2 c;
        };

        float cross_point(const Point2& origin, const Point2& a, const Point2& b) {
            return (a.x - origin.x) * (b.y - origin.y) - (a.y - origin.y) * (b.x - origin.x);
        }

        Point2 to_subcell(const engine::Vec3& world,
                          float patch_origin_x,
                          float patch_origin_z,
                          float patch_extent) {
            const float scale = static_cast<float>(map::kCellCount) / patch_extent;
            return {(world.x - patch_origin_x) * scale, (world.z - patch_origin_z) * scale};
        }

        engine::Vec3 lerp_y(const engine::Vec3& a, const engine::Vec3& b, float y) {
            const float t = (y - a.y) / (b.y - a.y);
            return {a.x + (b.x - a.x) * t, y, a.z + (b.z - a.z) * t};
        }

        class CoverageGrid {
          public:
            void reset(int min_x, int min_y, int max_x, int max_y) {
                m_origin_x = min_x;
                m_origin_y = min_y;
                m_width = max_x - min_x + 1;
                m_height = max_y - min_y + 1;
                if (m_width <= 0 || m_height <= 0 || m_width > map::kCellCount ||
                    m_height > map::kCellCount) {
                    m_width = 0;
                    m_height = 0;
                    m_cells.clear();
                    return;
                }
                m_cells.assign(
                    static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height), 0);
            }

            void set(int x, int y) {
                if (m_width <= 0 || x < 0 || y < 0 || x >= map::kCellCount ||
                    y >= map::kCellCount) {
                    return;
                }
                const int local_x = x - m_origin_x;
                const int local_y = y - m_origin_y;
                if (local_x < 0 || local_y < 0 || local_x >= m_width || local_y >= m_height) {
                    return;
                }
                m_cells[static_cast<std::size_t>(local_y) * static_cast<std::size_t>(m_width) +
                        static_cast<std::size_t>(local_x)] = 1;
            }

            [[nodiscard]] bool test(int x, int y) const {
                if (m_width <= 0) {
                    return false;
                }
                const int local_x = x - m_origin_x;
                const int local_y = y - m_origin_y;
                if (local_x < 0 || local_y < 0 || local_x >= m_width || local_y >= m_height) {
                    return false;
                }
                return m_cells[static_cast<std::size_t>(local_y) *
                                   static_cast<std::size_t>(m_width) +
                               static_cast<std::size_t>(local_x)] != 0;
            }

            [[nodiscard]] int width() const {
                return m_width;
            }
            [[nodiscard]] int height() const {
                return m_height;
            }
            [[nodiscard]] int origin_x() const {
                return m_origin_x;
            }
            [[nodiscard]] int origin_y() const {
                return m_origin_y;
            }

            [[nodiscard]] int count() const {
                return static_cast<int>(
                    std::count(m_cells.begin(), m_cells.end(), static_cast<std::uint8_t>(1)));
            }

          private:
            int m_origin_x = 0;
            int m_origin_y = 0;
            int m_width = 0;
            int m_height = 0;
            std::vector<std::uint8_t> m_cells;
        };

        void include_point(Point2 point, float& min_x, float& min_y, float& max_x, float& max_y) {
            min_x = std::min(min_x, point.x);
            min_y = std::min(min_y, point.y);
            max_x = std::max(max_x, point.x);
            max_y = std::max(max_y, point.y);
        }

        [[nodiscard]] bool prepare_grid(const float min_x,
                                        const float min_y,
                                        const float max_x,
                                        const float max_y,
                                        CoverageGrid& grid) {
            if (!(min_x <= max_x) || !(min_y <= max_y)) {
                return false;
            }
            if (max_x < 0.0f || max_y < 0.0f || min_x >= static_cast<float>(map::kCellCount) ||
                min_y >= static_cast<float>(map::kCellCount)) {
                return false;
            }
            const int left =
                std::clamp(static_cast<int>(std::floor(min_x)) - 1, 0, map::kCellCount - 1);
            const int top =
                std::clamp(static_cast<int>(std::floor(min_y)) - 1, 0, map::kCellCount - 1);
            const int right =
                std::clamp(static_cast<int>(std::floor(max_x)) + 1, 0, map::kCellCount - 1);
            const int bottom =
                std::clamp(static_cast<int>(std::floor(max_y)) + 1, 0, map::kCellCount - 1);
            grid.reset(left, top, right, bottom);
            return grid.width() > 0 && grid.height() > 0;
        }

        void rasterize_segment(CoverageGrid& grid, Point2 a, Point2 b) {
            const float dx = b.x - a.x;
            const float dy = b.y - a.y;
            const int steps = std::max(
                1, static_cast<int>(std::ceil(std::max(std::fabs(dx), std::fabs(dy)) * 2.0f)));
            for (int step = 0; step <= steps; ++step) {
                const float t = static_cast<float>(step) / static_cast<float>(steps);
                grid.set(static_cast<int>(std::floor(a.x + dx * t)),
                         static_cast<int>(std::floor(a.y + dy * t)));
            }
        }

        void rasterize_triangle(CoverageGrid& grid, const Tri2& triangle, bool fill_degenerate) {
            const float area = cross_point(triangle.a, triangle.b, triangle.c);
            if (std::fabs(area) < 1.0e-3f) {
                // Vertical walls project to lines. Collision uses the head-height cross-section
                // instead, so an exact boundary does not spill into the next subcell.
                if (!fill_degenerate) {
                    return;
                }
                rasterize_segment(grid, triangle.a, triangle.b);
                rasterize_segment(grid, triangle.b, triangle.c);
                rasterize_segment(grid, triangle.c, triangle.a);
                return;
            }
            const float min_x = std::min(triangle.a.x, std::min(triangle.b.x, triangle.c.x));
            const float max_x = std::max(triangle.a.x, std::max(triangle.b.x, triangle.c.x));
            const float min_y = std::min(triangle.a.y, std::min(triangle.b.y, triangle.c.y));
            const float max_y = std::max(triangle.a.y, std::max(triangle.b.y, triangle.c.y));
            const int x0 = std::max(0, static_cast<int>(std::floor(min_x)));
            const int x1 = std::min(map::kCellCount - 1, static_cast<int>(std::floor(max_x)));
            const int y0 = std::max(0, static_cast<int>(std::floor(min_y)));
            const int y1 = std::min(map::kCellCount - 1, static_cast<int>(std::floor(max_y)));
            for (int y = y0; y <= y1; ++y) {
                for (int x = x0; x <= x1; ++x) {
                    const Point2 center{static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f};
                    const float c0 = cross_point(triangle.a, triangle.b, center);
                    const float c1 = cross_point(triangle.b, triangle.c, center);
                    const float c2 = cross_point(triangle.c, triangle.a, center);
                    const bool negative = c0 < -1.0e-4f || c1 < -1.0e-4f || c2 < -1.0e-4f;
                    const bool positive = c0 > 1.0e-4f || c1 > 1.0e-4f || c2 > 1.0e-4f;
                    if (!(negative && positive)) {
                        grid.set(x, y);
                    }
                }
            }
        }

        void fill_span(CoverageGrid& grid, float x0, float x1, int cell_y) {
            if (x1 < x0) {
                std::swap(x0, x1);
            }
            const int left = static_cast<int>(std::ceil(static_cast<double>(x0) - 0.5));
            const int right = static_cast<int>(std::floor(static_cast<double>(x1) - 0.5));
            for (int x = left; x <= right; ++x) {
                grid.set(x, cell_y);
            }
        }

        // Nonzero winding of the head-height cross-section fills wall interiors that project to
        // zero area, while opposite inner loops leave courtyards empty.
        void rasterize_winding(CoverageGrid& grid, const std::vector<Segment2>& segments) {
            if (grid.height() <= 0 || segments.empty()) {
                return;
            }
            struct Hit {
                float x = 0.0f;
                int direction = 0;
            };
            for (int y = grid.origin_y(); y < grid.origin_y() + grid.height(); ++y) {
                const float scan_y = static_cast<float>(y) + 0.5f;
                std::vector<Hit> hits;
                hits.reserve(segments.size());
                for (const Segment2& segment : segments) {
                    const float y_min = std::min(segment.y0, segment.y1);
                    const float y_max = std::max(segment.y0, segment.y1);
                    if (!(scan_y >= y_min && scan_y < y_max)) {
                        continue;
                    }
                    const float t = (scan_y - segment.y0) / (segment.y1 - segment.y0);
                    hits.push_back({segment.x0 + t * (segment.x1 - segment.x0),
                                    segment.y1 > segment.y0 ? 1 : -1});
                }
                if (hits.size() < 2) {
                    continue;
                }
                std::sort(
                    hits.begin(), hits.end(), [](const Hit& a, const Hit& b) { return a.x < b.x; });
                int winding = 0;
                float last_x = hits.front().x;
                bool have_last = false;
                for (const Hit& hit : hits) {
                    if (have_last && winding != 0) {
                        fill_span(grid, last_x, hit.x, y);
                    }
                    winding += hit.direction;
                    last_x = hit.x;
                    have_last = true;
                }
            }
        }

        std::uint32_t pack_corner(int x, int y) {
            return (static_cast<std::uint32_t>(x) << 16) | static_cast<std::uint32_t>(y & 0xffff);
        }

        ObjectMaskPoint unpack_corner(std::uint32_t packed) {
            return {static_cast<float>(packed >> 16),
                    static_cast<float>(static_cast<std::int16_t>(packed & 0xffff))};
        }

        std::vector<ObjectMaskPoint> simplify_loop(const std::vector<ObjectMaskPoint>& loop) {
            if (loop.size() < 4) {
                return {};
            }
            const std::size_t count = loop.size() - 1;
            std::vector<ObjectMaskPoint> simplified;
            simplified.reserve(count);
            for (std::size_t index = 0; index < count; ++index) {
                const ObjectMaskPoint& previous = loop[(index + count - 1) % count];
                const ObjectMaskPoint& current = loop[index];
                const ObjectMaskPoint& next = loop[(index + 1) % count];
                const float cross = (current.x - previous.x) * (next.y - current.y) -
                                    (current.y - previous.y) * (next.x - current.x);
                if (cross != 0.0f) {
                    simplified.push_back(current);
                }
            }
            if (simplified.size() < 3) {
                return {};
            }
            simplified.push_back(simplified.front());
            return simplified;
        }

        std::vector<std::vector<ObjectMaskPoint>> trace_contours(const CoverageGrid& grid) {
            std::unordered_map<std::uint32_t, std::vector<std::uint32_t>> next_corner;
            const auto add_edge = [&](int x0, int y0, int x1, int y1) {
                next_corner[pack_corner(x0, y0)].push_back(pack_corner(x1, y1));
            };
            for (int local_y = 0; local_y < grid.height(); ++local_y) {
                for (int local_x = 0; local_x < grid.width(); ++local_x) {
                    const int x = grid.origin_x() + local_x;
                    const int y = grid.origin_y() + local_y;
                    if (!grid.test(x, y)) {
                        continue;
                    }
                    if (!grid.test(x, y - 1)) {
                        add_edge(x, y, x + 1, y);
                    }
                    if (!grid.test(x + 1, y)) {
                        add_edge(x + 1, y, x + 1, y + 1);
                    }
                    if (!grid.test(x, y + 1)) {
                        add_edge(x + 1, y + 1, x, y + 1);
                    }
                    if (!grid.test(x - 1, y)) {
                        add_edge(x, y + 1, x, y);
                    }
                }
            }

            std::vector<std::vector<ObjectMaskPoint>> loops;
            const int guard_limit = std::max(8, grid.width() * grid.height() * 4 + 8);
            for (auto& entry : next_corner) {
                while (!entry.second.empty()) {
                    const std::uint32_t start = entry.first;
                    std::uint32_t cursor = start;
                    std::vector<ObjectMaskPoint> loop;
                    bool closed = false;
                    for (int guard = 0; guard < guard_limit; ++guard) {
                        loop.push_back(unpack_corner(cursor));
                        const auto found = next_corner.find(cursor);
                        if (found == next_corner.end() || found->second.empty()) {
                            break;
                        }
                        const std::uint32_t destination = found->second.back();
                        found->second.pop_back();
                        cursor = destination;
                        if (cursor == start) {
                            loop.push_back(unpack_corner(start));
                            closed = true;
                            break;
                        }
                    }
                    if (closed) {
                        std::vector<ObjectMaskPoint> simplified = simplify_loop(loop);
                        if (!simplified.empty()) {
                            loops.push_back(std::move(simplified));
                        }
                    }
                }
            }
            return loops;
        }

        bool
        loops_contain(const std::vector<std::vector<ObjectMaskPoint>>& loops, float x, float y) {
            int crossings = 0;
            for (const auto& loop : loops) {
                for (std::size_t index = 0; index + 1 < loop.size(); ++index) {
                    const ObjectMaskPoint& a = loop[index];
                    const ObjectMaskPoint& b = loop[index + 1];
                    if ((a.y > y) == (b.y > y)) {
                        continue;
                    }
                    const float hit_x = a.x + (y - a.y) * (b.x - a.x) / (b.y - a.y);
                    if (hit_x > x) {
                        ++crossings;
                    }
                }
            }
            return (crossings & 1) != 0;
        }

        void append_clipped_triangle(const engine::Vec3& a,
                                     const engine::Vec3& b,
                                     const engine::Vec3& c,
                                     float head_y,
                                     float patch_origin_x,
                                     float patch_origin_z,
                                     float patch_extent,
                                     std::vector<Tri2>& triangles,
                                     std::vector<Segment2>& segments,
                                     float& min_x,
                                     float& min_y,
                                     float& max_x,
                                     float& max_y) {
            const engine::Vec3 source[3] = {a, b, c};
            std::vector<engine::Vec3> clipped;
            clipped.reserve(4);
            for (int index = 0; index < 3; ++index) {
                const engine::Vec3& current = source[index];
                const engine::Vec3& previous = source[(index + 2) % 3];
                const bool current_inside = current.y <= head_y;
                const bool previous_inside = previous.y <= head_y;
                if (current_inside) {
                    if (!previous_inside) {
                        clipped.push_back(lerp_y(previous, current, head_y));
                    }
                    clipped.push_back(current);
                } else if (previous_inside) {
                    clipped.push_back(lerp_y(previous, current, head_y));
                }
            }
            for (std::size_t index = 1; index + 1 < clipped.size(); ++index) {
                Tri2 triangle{
                    to_subcell(clipped[0], patch_origin_x, patch_origin_z, patch_extent),
                    to_subcell(clipped[index], patch_origin_x, patch_origin_z, patch_extent),
                    to_subcell(clipped[index + 1], patch_origin_x, patch_origin_z, patch_extent)};
                include_point(triangle.a, min_x, min_y, max_x, max_y);
                include_point(triangle.b, min_x, min_y, max_x, max_y);
                include_point(triangle.c, min_x, min_y, max_x, max_y);
                triangles.push_back(triangle);
            }

            Point2 hits[2];
            int hit_count = 0;
            for (int index = 0; index < 3 && hit_count < 2; ++index) {
                const engine::Vec3& start = source[index];
                const engine::Vec3& end = source[(index + 1) % 3];
                if ((start.y <= head_y) == (end.y <= head_y)) {
                    continue;
                }
                const Point2 hit = to_subcell(
                    lerp_y(start, end, head_y), patch_origin_x, patch_origin_z, patch_extent);
                hits[hit_count++] = hit;
                include_point(hit, min_x, min_y, max_x, max_y);
            }
            if (hit_count == 2) {
                segments.push_back({hits[0].x, hits[0].y, hits[1].x, hits[1].y});
            }
        }

        void collect_cells(const CoverageGrid& grid,
                           std::vector<std::uint32_t>& cells,
                           int& min_x,
                           int& min_y,
                           int& max_x,
                           int& max_y) {
            cells.clear();
            min_x = 0;
            min_y = 0;
            max_x = -1;
            max_y = -1;
            for (int local_y = 0; local_y < grid.height(); ++local_y) {
                for (int local_x = 0; local_x < grid.width(); ++local_x) {
                    const int x = grid.origin_x() + local_x;
                    const int y = grid.origin_y() + local_y;
                    if (!grid.test(x, y)) {
                        continue;
                    }
                    if (max_x < min_x) {
                        min_x = max_x = x;
                        min_y = max_y = y;
                    } else {
                        min_x = std::min(min_x, x);
                        min_y = std::min(min_y, y);
                        max_x = std::max(max_x, x);
                        max_y = std::max(max_y, y);
                    }
                    cells.push_back(static_cast<std::uint32_t>(y * map::kCellCount + x));
                }
            }
        }

    }  // namespace

    ObjectMask build_object_mask(const std::string& name,
                                 const std::vector<ObjectMaskMesh>& meshes,
                                 const engine::Mat4& model,
                                 float patch_origin_x,
                                 float patch_origin_z,
                                 float patch_extent,
                                 float base_world_y,
                                 bool foliage) {
        ObjectMask result;
        result.name = name;
        if (patch_extent <= 0.0f) {
            return result;
        }

        std::vector<Tri2> display_triangles;
        std::vector<Tri2> collision_triangles;
        std::vector<Segment2> collision_segments;
        float display_min_x = std::numeric_limits<float>::max();
        float display_min_y = std::numeric_limits<float>::max();
        float display_max_x = std::numeric_limits<float>::lowest();
        float display_max_y = std::numeric_limits<float>::lowest();
        float collision_min_x = display_min_x;
        float collision_min_y = display_min_y;
        float collision_max_x = display_max_x;
        float collision_max_y = display_max_y;
        const float head_y = base_world_y + kObjectMaskHeadHeight;

        for (const ObjectMaskMesh& mesh : meshes) {
            std::vector<engine::Vec3> world;
            world.reserve(mesh.positions.size());
            for (const engine::Vec3& position : mesh.positions) {
                world.push_back(
                    engine::Mat4::transform_point(model, position.x, position.y, position.z));
            }
            const bool skip_collision = foliage && mesh.alpha_surface;
            for (std::size_t index = 0; index + 2 < mesh.indices.size(); index += 3) {
                const std::uint32_t ia = mesh.indices[index];
                const std::uint32_t ib = mesh.indices[index + 1];
                const std::uint32_t ic = mesh.indices[index + 2];
                if (ia >= world.size() || ib >= world.size() || ic >= world.size()) {
                    continue;
                }
                Tri2 projected{to_subcell(world[ia], patch_origin_x, patch_origin_z, patch_extent),
                               to_subcell(world[ib], patch_origin_x, patch_origin_z, patch_extent),
                               to_subcell(world[ic], patch_origin_x, patch_origin_z, patch_extent)};
                include_point(
                    projected.a, display_min_x, display_min_y, display_max_x, display_max_y);
                include_point(
                    projected.b, display_min_x, display_min_y, display_max_x, display_max_y);
                include_point(
                    projected.c, display_min_x, display_min_y, display_max_x, display_max_y);
                display_triangles.push_back(projected);
                if (skip_collision) {
                    continue;
                }
                append_clipped_triangle(world[ia],
                                        world[ib],
                                        world[ic],
                                        head_y,
                                        patch_origin_x,
                                        patch_origin_z,
                                        patch_extent,
                                        collision_triangles,
                                        collision_segments,
                                        collision_min_x,
                                        collision_min_y,
                                        collision_max_x,
                                        collision_max_y);
            }
        }

        CoverageGrid display_grid;
        if (prepare_grid(
                display_min_x, display_min_y, display_max_x, display_max_y, display_grid)) {
            for (const Tri2& triangle : display_triangles) {
                rasterize_triangle(display_grid, triangle, true);
            }
            result.pick_area = static_cast<float>(display_grid.count());
            result.display_contours = trace_contours(display_grid);
        }

        CoverageGrid collision_grid;
        if (prepare_grid(collision_min_x,
                         collision_min_y,
                         collision_max_x,
                         collision_max_y,
                         collision_grid)) {
            for (const Tri2& triangle : collision_triangles) {
                rasterize_triangle(collision_grid, triangle, false);
            }
            rasterize_winding(collision_grid, collision_segments);
            collect_cells(collision_grid,
                          result.footprint_cells,
                          result.min_x,
                          result.min_y,
                          result.max_x,
                          result.max_y);
            result.collision_contours = trace_contours(collision_grid);
        }
        if (result.pick_area <= 0.0f) {
            result.pick_area = static_cast<float>(result.footprint_cells.size());
        }
        return result;
    }

    const ObjectMask*
    pick_object_mask(const std::vector<ObjectMask>& masks, int subcell_x, int subcell_y) {
        if (subcell_x < 0 || subcell_y < 0 || subcell_x >= map::kCellCount ||
            subcell_y >= map::kCellCount) {
            return nullptr;
        }
        const float x = static_cast<float>(subcell_x) + 0.5f;
        const float y = static_cast<float>(subcell_y) + 0.5f;
        const ObjectMask* best = nullptr;
        float best_area = std::numeric_limits<float>::max();
        const std::uint32_t cell =
            static_cast<std::uint32_t>(subcell_y * map::kCellCount + subcell_x);
        for (const ObjectMask& mask : masks) {
            if (mask.footprint_cells.empty() || !(mask.pick_area < best_area)) {
                continue;
            }
            const bool in_footprint =
                subcell_x >= mask.min_x && subcell_x <= mask.max_x && subcell_y >= mask.min_y &&
                subcell_y <= mask.max_y &&
                std::binary_search(mask.footprint_cells.begin(), mask.footprint_cells.end(), cell);
            if (!in_footprint &&
                (mask.display_contours.empty() || !loops_contain(mask.display_contours, x, y))) {
                continue;
            }
            best = &mask;
            best_area = mask.pick_area;
        }
        return best;
    }

    std::vector<std::uint32_t>
    clip_footprint_to_rect(const ObjectMask& mask, int min_x, int min_y, int max_x, int max_y) {
        std::vector<std::uint32_t> cells;
        if (max_x < min_x || max_y < min_y || mask.max_x < mask.min_x || mask.max_x < min_x ||
            mask.min_x > max_x || mask.max_y < min_y || mask.min_y > max_y) {
            return cells;
        }
        cells.reserve(mask.footprint_cells.size());
        for (const std::uint32_t cell : mask.footprint_cells) {
            const int x = static_cast<int>(cell % static_cast<std::uint32_t>(map::kCellCount));
            const int y = static_cast<int>(cell / static_cast<std::uint32_t>(map::kCellCount));
            if (x >= min_x && x <= max_x && y >= min_y && y <= max_y) {
                cells.push_back(cell);
            }
        }
        return cells;
    }

    std::vector<std::uint32_t> subcells_in_rect(int min_x, int min_y, int max_x, int max_y) {
        min_x = std::clamp(min_x, 0, map::kCellCount - 1);
        min_y = std::clamp(min_y, 0, map::kCellCount - 1);
        max_x = std::clamp(max_x, 0, map::kCellCount - 1);
        max_y = std::clamp(max_y, 0, map::kCellCount - 1);
        std::vector<std::uint32_t> cells;
        if (max_x < min_x || max_y < min_y) {
            return cells;
        }
        cells.reserve(static_cast<std::size_t>(max_x - min_x + 1) *
                      static_cast<std::size_t>(max_y - min_y + 1));
        for (int y = min_y; y <= max_y; ++y) {
            for (int x = min_x; x <= max_x; ++x) {
                cells.push_back(static_cast<std::uint32_t>(y * map::kCellCount + x));
            }
        }
        return cells;
    }

}  // namespace openkal::area_editor
