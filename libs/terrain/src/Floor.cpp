/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/terrain/Floor.h"

#include "openkal/map/Constants.h"
#include "openkal/terrain/GbModel.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>

namespace openkal::terrain {
    namespace {

        bool is_foliage_asset_path(const std::string& path) {
            std::string lower;
            lower.reserve(path.size());
            for (unsigned char ch : path) {
                lower.push_back(static_cast<char>(std::tolower(ch)));
            }
            static const char* keys[] = {"tree",
                                         "leaf",
                                         "bush",
                                         "grass",
                                         "plant",
                                         "flower",
                                         "weed",
                                         "bilboard",
                                         "billboard"};
            for (const char* k : keys) {
                if (lower.find(k) != std::string::npos) {
                    return true;
                }
            }
            return false;
        }

    }  // namespace

    void FloorProp::add_vertex(float x, float y, float z) {
        m_tris.push_back(x);
        m_tris.push_back(y);
        m_tris.push_back(z);
        m_min_x = std::min(m_min_x, x);
        m_min_y = std::min(m_min_y, y);
        m_min_z = std::min(m_min_z, z);
        m_max_x = std::max(m_max_x, x);
        m_max_y = std::max(m_max_y, y);
        m_max_z = std::max(m_max_z, z);
    }

    void FloorProp::pad_bounds_xz(float pad) {
        m_min_x -= pad;
        m_max_x += pad;
        m_min_z -= pad;
        m_max_z += pad;
    }

    bool FloorProp::contains_xz(float world_x, float world_z) const {
        return !(world_x < m_min_x || world_x > m_max_x || world_z < m_min_z || world_z > m_max_z);
    }

    void FloorField::add_patch(terrain::HeightPatch patch) {
        m_height_field.patches.push_back(std::move(patch));
    }

    const std::vector<float>& FloorField::local_tris_for(const std::string& gb_path) {
        auto it = m_local_tris_cache.find(gb_path);
        if (it != m_local_tris_cache.end()) {
            return it->second;
        }
        std::vector<float> tris;
        if (is_foliage_asset_path(gb_path)) {
            return m_local_tris_cache.emplace(gb_path, std::move(tris)).first->second;
        }
        terrain::GbModel cpu;
        if (terrain::load_gb(gb_path, cpu)) {
            // GetFloor raycasts GB compatibility behavior, not the render mesh. Bridge
            // planks (bridge_02h*) ship collision-only GBs with mesh_count=0.
            if (!cpu.cls_tris.empty()) {
                tris = cpu.cls_tris;
            } else {
                for (const auto& m : cpu.meshes) {
                    // Leaves / cards: visible, but never stand-on or mouse-floor.
                    if (m.alpha_blend || m.alpha_test || m.two_sided) {
                        continue;
                    }
                    for (std::size_t i = 0; i + 2 < m.indices.size(); i += 3) {
                        for (int k = 0; k < 3; ++k) {
                            const std::uint32_t idx = m.indices[i + static_cast<std::size_t>(k)];
                            const std::size_t o = static_cast<std::size_t>(idx) * 5;
                            if (o + 2 >= m.pos_uv.size()) {
                                tris.insert(tris.end(), {0.0f, 0.0f, 0.0f});
                            } else {
                                tris.push_back(m.pos_uv[o]);
                                tris.push_back(m.pos_uv[o + 1]);
                                tris.push_back(m.pos_uv[o + 2]);
                            }
                        }
                    }
                }
            }
        }
        return m_local_tris_cache.emplace(gb_path, std::move(tris)).first->second;
    }

    const std::vector<float>& FloorField::blocker_tris_for(const std::string& gb_path) {
        auto it = m_blocker_tris_cache.find(gb_path);
        if (it != m_blocker_tris_cache.end()) {
            return it->second;
        }
        std::vector<float> tris;
        if (is_foliage_asset_path(gb_path)) {
            return m_blocker_tris_cache.emplace(gb_path, std::move(tris)).first->second;
        }
        terrain::GbModel cpu;
        if (terrain::load_gb(gb_path, cpu)) {
            for (const auto& m : cpu.meshes) {
                if (m.alpha_blend || m.alpha_test) {
                    continue;
                }
                for (std::size_t i = 0; i + 2 < m.indices.size(); i += 3) {
                    for (int k = 0; k < 3; ++k) {
                        const std::uint32_t idx = m.indices[i + static_cast<std::size_t>(k)];
                        const std::size_t o = static_cast<std::size_t>(idx) * 5;
                        if (o + 2 >= m.pos_uv.size()) {
                            tris.insert(tris.end(), {0.0f, 0.0f, 0.0f});
                        } else {
                            tris.push_back(m.pos_uv[o]);
                            tris.push_back(m.pos_uv[o + 1]);
                            tris.push_back(m.pos_uv[o + 2]);
                        }
                    }
                }
            }
        }
        return m_blocker_tris_cache.emplace(gb_path, std::move(tris)).first->second;
    }

    void FloorField::add_prop(const std::string& gb_path,
                              const engine::Mat4& world,
                              int patch_x,
                              int patch_y) {
        if (is_foliage_asset_path(gb_path)) {
            return;
        }
        const auto& local = local_tris_for(gb_path);
        if (local.empty()) {
            return;
        }

        FloorProp prop;
        prop.set_patch(patch_x, patch_y);
        prop.reserve_tris(local.size());
        for (std::size_t i = 0; i + 2 < local.size(); i += 3) {
            const auto p =
                engine::Mat4::transform_point(world, local[i], local[i + 1], local[i + 2]);
            prop.add_vertex(p.x, p.y, p.z);
        }
        prop.pad_bounds_xz(2.0f);
        prop.build_index();
        add_deck(std::move(prop));
    }

    void FloorField::add_blocker(const std::string& gb_path,
                                 const engine::Mat4& world,
                                 int patch_x,
                                 int patch_y) {
        if (is_foliage_asset_path(gb_path)) {
            return;
        }
        add_blocker_from_local(blocker_tris_for(gb_path), world, patch_x, patch_y);
    }

    void FloorField::add_obstacle(const std::string& gb_path,
                                  const engine::Mat4& world,
                                  int patch_x,
                                  int patch_y) {
        if (is_foliage_asset_path(gb_path)) {
            return;
        }
        add_blocker_from_local(local_tris_for(gb_path), world, patch_x, patch_y);
    }

    void FloorField::add_blocker_from_local(const std::vector<float>& local,
                                            const engine::Mat4& world,
                                            int patch_x,
                                            int patch_y) {
        if (local.empty()) {
            return;
        }

        FloorProp prop;
        prop.set_patch(patch_x, patch_y);
        prop.reserve_tris(local.size());
        for (std::size_t i = 0; i + 2 < local.size(); i += 3) {
            const auto p =
                engine::Mat4::transform_point(world, local[i], local[i + 1], local[i + 2]);
            prop.add_vertex(p.x, p.y, p.z);
        }
        prop.pad_bounds_xz(2.0f);
        prop.build_index();
        add_blocker_deck(std::move(prop));
    }

    void FloorField::add_deck(FloorProp prop) {
        if (prop.empty()) {
            return;
        }
        prop.build_index();
        m_props.push_back(std::move(prop));
        add_prop_to_grid(m_props.size() - 1);
    }

    void FloorField::add_blocker_deck(FloorProp prop) {
        if (prop.empty()) {
            return;
        }
        prop.build_index();
        m_blockers.push_back(std::move(prop));
        add_prop_to_grid(m_blockers.size() - 1, true);
    }

    void FloorField::remove_patch(int patch_x, int patch_y) {
        auto& patches = m_height_field.patches;
        patches.erase(std::remove_if(patches.begin(),
                                     patches.end(),
                                     [patch_x, patch_y](const terrain::HeightPatch& p) {
                                         return p.patch_x == patch_x && p.patch_y == patch_y;
                                     }),
                      patches.end());
        m_props.erase(std::remove_if(m_props.begin(),
                                     m_props.end(),
                                     [patch_x, patch_y](const FloorProp& p) {
                                         return p.patch_x() == patch_x && p.patch_y() == patch_y;
                                     }),
                      m_props.end());
        m_blockers.erase(std::remove_if(m_blockers.begin(),
                                        m_blockers.end(),
                                        [patch_x, patch_y](const FloorProp& p) {
                                            return p.patch_x() == patch_x && p.patch_y() == patch_y;
                                        }),
                         m_blockers.end());
        rebuild_prop_grid();
    }

    std::uint64_t FloorField::prop_grid_key(int grid_x, int grid_z) {
        return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(grid_x)) << 32) |
               static_cast<std::uint32_t>(grid_z);
    }

    void FloorField::add_prop_to_grid(std::size_t prop_index, bool blocker) {
        const auto& props = blocker ? m_blockers : m_props;
        auto& grid = blocker ? m_blocker_grid : m_prop_grid;
        if (prop_index >= props.size()) {
            return;
        }
        const FloorProp& prop = props[prop_index];
        const int min_x = static_cast<int>(std::floor(prop.min_x() / kPropGridCellSize));
        const int max_x = static_cast<int>(std::floor(prop.max_x() / kPropGridCellSize));
        const int min_z = static_cast<int>(std::floor(prop.min_z() / kPropGridCellSize));
        const int max_z = static_cast<int>(std::floor(prop.max_z() / kPropGridCellSize));
        for (int grid_z = min_z; grid_z <= max_z; ++grid_z) {
            for (int grid_x = min_x; grid_x <= max_x; ++grid_x) {
                grid[prop_grid_key(grid_x, grid_z)].push_back(prop_index);
            }
        }
    }

    void FloorField::rebuild_prop_grid() {
        m_prop_grid.clear();
        m_blocker_grid.clear();
        for (std::size_t i = 0; i < m_props.size(); ++i) {
            add_prop_to_grid(i);
        }
        for (std::size_t i = 0; i < m_blockers.size(); ++i) {
            add_prop_to_grid(i, true);
        }
    }

    const std::vector<std::size_t>* FloorField::prop_candidates(float world_x,
                                                                float world_z) const {
        const int grid_x = static_cast<int>(std::floor(world_x / kPropGridCellSize));
        const int grid_z = static_cast<int>(std::floor(world_z / kPropGridCellSize));
        const auto it = m_prop_grid.find(prop_grid_key(grid_x, grid_z));
        return it == m_prop_grid.end() ? nullptr : &it->second;
    }

    bool FloorField::ray_tri_down(float ox,
                                  float oy,
                                  float oz,
                                  float max_t,
                                  float ax,
                                  float ay,
                                  float az,
                                  float bx,
                                  float by,
                                  float bz,
                                  float cx,
                                  float cy,
                                  float cz,
                                  float& out_t,
                                  bool allow_short_risers) {
        constexpr float kEps = 1e-5f;
        const float e1x = bx - ax, e1y = by - ay, e1z = bz - az;
        const float e2x = cx - ax, e2y = cy - ay, e2z = cz - az;
        // dir=(0,-1,0); pvec = dir x e2 = (-e2.z, 0, e2.x)
        const float px = -e2z;
        const float pz = e2x;
        const float det = e1x * px + e1z * pz;
        if (det > -kEps && det < kEps) {
            return false;
        }
        const float inv_det = 1.0f / det;
        const float sx = ox - ax, sy = oy - ay, sz = oz - az;
        const float u = (sx * px + sz * pz) * inv_det;
        if (u < 0.0f || u > 1.0f) {
            return false;
        }
        const float qx = sy * e1z - sz * e1y;
        const float qy = sz * e1x - sx * e1z;
        const float qz = sx * e1y - sy * e1x;
        const float v = (-qy) * inv_det;
        if (v < 0.0f || u + v > 1.0f) {
            return false;
        }
        const float t = (e2x * qx + e2y * qy + e2z * qz) * inv_det;
        if (t <= kEps || t >= max_t) {
            return false;
        }
        const float nx = e1y * e2z - e1z * e2y;
        const float ny = e1z * e2x - e1x * e2z;
        const float nz = e1x * e2y - e1y * e2x;
        const float nlen = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (nlen < kEps || ny <= 0.0f) {
            return false;
        }
        if (ny / nlen < kSlideSlope) {
            // Short slanted risers support the feet between adjacent outdoor treads.
            constexpr float max_riser_run = 2.0f;
            const float rise = std::max({ay, by, cy}) - std::min({ay, by, cy});
            if (!allow_short_risers || rise > kStepHeight ||
                rise * ny > max_riser_run * std::hypot(nx, nz)) {
                return false;
            }
        }
        out_t = t;
        return true;
    }

    bool FloorField::ray_tri_any(float ox,
                                 float oy,
                                 float oz,
                                 float dx,
                                 float dy,
                                 float dz,
                                 float max_t,
                                 float ax,
                                 float ay,
                                 float az,
                                 float bx,
                                 float by,
                                 float bz,
                                 float cx,
                                 float cy,
                                 float cz,
                                 float& out_t) {
        constexpr float kEps = 1e-5f;
        const float e1x = bx - ax, e1y = by - ay, e1z = bz - az;
        const float e2x = cx - ax, e2y = cy - ay, e2z = cz - az;
        const float px = dy * e2z - dz * e2y;
        const float py = dz * e2x - dx * e2z;
        const float pz = dx * e2y - dy * e2x;
        const float det = e1x * px + e1y * py + e1z * pz;
        if (det > -kEps && det < kEps) {
            return false;
        }
        const float inv_det = 1.0f / det;
        const float sx = ox - ax, sy = oy - ay, sz = oz - az;
        const float u = (sx * px + sy * py + sz * pz) * inv_det;
        if (u < 0.0f || u > 1.0f) {
            return false;
        }
        const float qx = sy * e1z - sz * e1y;
        const float qy = sz * e1x - sx * e1z;
        const float qz = sx * e1y - sy * e1x;
        const float v = (dx * qx + dy * qy + dz * qz) * inv_det;
        if (v < 0.0f || u + v > 1.0f) {
            return false;
        }
        const float t = (e2x * qx + e2y * qy + e2z * qz) * inv_det;
        if (t <= kEps || t >= max_t) {
            return false;
        }
        out_t = t;
        return true;
    }

    bool FloorField::raycast(float ox,
                             float oy,
                             float oz,
                             float dx,
                             float dy,
                             float dz,
                             float max_t,
                             float& out_t,
                             RayHitFilter filter) const {
        return raycast_hulls(m_blockers, ox, oy, oz, dx, dy, dz, max_t, out_t, filter, false);
    }

    bool FloorField::raycast_walkable(float ox,
                                      float oy,
                                      float oz,
                                      float dx,
                                      float dy,
                                      float dz,
                                      float max_t,
                                      float& out_t) const {
        return raycast_hulls(
            m_props, ox, oy, oz, dx, dy, dz, max_t, out_t, RayHitFilter::All, true);
    }

    bool FloorField::raycast_hulls(const std::vector<FloorProp>& decks,
                                   float ox,
                                   float oy,
                                   float oz,
                                   float dx,
                                   float dy,
                                   float dz,
                                   float max_t,
                                   float& out_t,
                                   RayHitFilter filter,
                                   bool walkable_only,
                                   bool stop_when_blocked,
                                   float slack) const {
        out_t = max_t;
        if (max_t <= 1e-4f) {
            return false;
        }
        bool hit = false;

        auto slab = [](float o, float d, float mn, float mx, float& t_min, float& t_max) -> bool {
            if (std::fabs(d) < 1e-8f) {
                return o >= mn && o <= mx;
            }
            float t0 = (mn - o) / d;
            float t1 = (mx - o) / d;
            if (t0 > t1) {
                std::swap(t0, t1);
            }
            t_min = std::max(t_min, t0);
            t_max = std::min(t_max, t1);
            return t_min <= t_max;
        };

        const std::vector<std::size_t>* candidates = nullptr;
        std::vector<std::size_t> merged;
        const double end_x = static_cast<double>(ox) + static_cast<double>(dx) * max_t;
        const double end_z = static_cast<double>(oz) + static_cast<double>(dz) * max_t;
        const double padding = std::max(1.0,
                                        1e-5 * std::max({std::abs(static_cast<double>(ox)),
                                                         std::abs(static_cast<double>(oz)),
                                                         std::abs(end_x),
                                                         std::abs(end_z)}));
        const double min_x =
            std::floor((std::min<double>(ox, end_x) - padding) / kPropGridCellSize);
        const double max_x =
            std::floor((std::max<double>(ox, end_x) + padding) / kPropGridCellSize);
        const double min_z =
            std::floor((std::min<double>(oz, end_z) - padding) / kPropGridCellSize);
        const double max_z =
            std::floor((std::max<double>(oz, end_z) + padding) / kPropGridCellSize);
        // Keep long or unbounded rays on the linear path. Short movement rays
        // need only hulls in cells overlapped by their conservatively padded XZ bounds.
        if ((&decks == &m_props || &decks == &m_blockers) &&
            min_x >= std::numeric_limits<int>::min() && max_x <= std::numeric_limits<int>::max() &&
            min_z >= std::numeric_limits<int>::min() && max_z <= std::numeric_limits<int>::max() &&
            (max_x - min_x + 1) * (max_z - min_z + 1) <= 64) {
            const auto& grid = &decks == &m_blockers ? m_blocker_grid : m_prop_grid;
            if (min_x == max_x && min_z == max_z) {
                const auto found =
                    grid.find(prop_grid_key(static_cast<int>(min_x), static_cast<int>(min_z)));
                if (found == grid.end()) {
                    return false;
                }
                candidates = &found->second;
            } else {
                for (auto z = static_cast<std::int64_t>(min_z); z <= max_z; ++z) {
                    for (auto x = static_cast<std::int64_t>(min_x); x <= max_x; ++x) {
                        const auto found =
                            grid.find(prop_grid_key(static_cast<int>(x), static_cast<int>(z)));
                        if (found != grid.end()) {
                            merged.insert(merged.end(), found->second.begin(), found->second.end());
                        }
                    }
                }
                std::sort(merged.begin(), merged.end());
                merged.erase(std::unique(merged.begin(), merged.end()), merged.end());
                candidates = &merged;
            }
        }
        const auto count = candidates ? candidates->size() : decks.size();
        for (std::size_t index = 0; index < count; ++index) {
            const auto& prop = decks[candidates ? (*candidates)[index] : index];
            float t_min = 0.0f;
            float t_max = out_t;
            if (!slab(ox, dx, prop.min_x(), prop.max_x(), t_min, t_max) ||
                !slab(oy, dy, prop.min_y(), prop.max_y(), t_min, t_max) ||
                !slab(oz, dz, prop.min_z(), prop.max_z(), t_min, t_max)) {
                continue;
            }
            const auto& tris = prop.tris();
            auto test_tri = [&](std::size_t i) {
                if (i + 8 >= tris.size()) {
                    return;
                }
                float t = 0.0f;
                if (!ray_tri_any(ox,
                                 oy,
                                 oz,
                                 dx,
                                 dy,
                                 dz,
                                 out_t,
                                 tris[i],
                                 tris[i + 1],
                                 tris[i + 2],
                                 tris[i + 3],
                                 tris[i + 4],
                                 tris[i + 5],
                                 tris[i + 6],
                                 tris[i + 7],
                                 tris[i + 8],
                                 t)) {
                    return;
                }
                if (filter != RayHitFilter::All) {
                    const float e1x = tris[i + 3] - tris[i];
                    const float e1y = tris[i + 4] - tris[i + 1];
                    const float e1z = tris[i + 5] - tris[i + 2];
                    const float e2x = tris[i + 6] - tris[i];
                    const float e2y = tris[i + 7] - tris[i + 1];
                    const float e2z = tris[i + 8] - tris[i + 2];
                    const float nx = e1y * e2z - e1z * e2y;
                    const float ny = e1z * e2x - e1x * e2z;
                    const float nz = e1x * e2y - e1y * e2x;
                    const float nlen = std::sqrt(nx * nx + ny * ny + nz * nz);
                    if (nlen < 1e-5f) {
                        return;
                    }
                    const float ny_unit = ny / nlen;
                    if ((filter == RayHitFilter::Walls && std::fabs(ny_unit) >= 0.45f) ||
                        (filter == RayHitFilter::UpwardSlopes &&
                         (ny_unit <= 0.0f || ny_unit >= kSlideSlope)) ||
                        (filter == RayHitFilter::Ceilings && ny_unit > -0.45f)) {
                        return;
                    }
                    if (filter == RayHitFilter::StepObstacles) {
                        const float slope = std::fabs(ny_unit);
                        const bool moving_uphill = ny * (nx * dx + nz * dz) < 0.0f;
                        if (slope >= kSlideSlope || (slope >= 0.45f && !moving_uphill)) {
                            return;
                        }
                    }
                }
                if (walkable_only) {
                    const float e1x = tris[i + 3] - tris[i];
                    const float e1y = tris[i + 4] - tris[i + 1];
                    const float e1z = tris[i + 5] - tris[i + 2];
                    const float e2x = tris[i + 6] - tris[i];
                    const float e2y = tris[i + 7] - tris[i + 1];
                    const float e2z = tris[i + 8] - tris[i + 2];
                    const float nx = e1y * e2z - e1z * e2y;
                    const float ny = e1z * e2x - e1x * e2z;
                    const float nz = e1x * e2y - e1y * e2x;
                    const float nlen = std::sqrt(nx * nx + ny * ny + nz * nz);
                    if (nlen < 1e-5f || (ny / nlen) < FloorField::kSlideSlope) {
                        return;
                    }
                }
                out_t = t;
                hit = true;
            };

            if (!prop.indexed()) {
                for (std::size_t i = 0; i + 8 < tris.size(); i += 9) {
                    test_tri(i);
                    if (stop_when_blocked && hit && out_t + slack < max_t) {
                        return true;
                    }
                }
                continue;
            }

            const float x0 = ox + dx * t_min;
            const float z0 = oz + dz * t_min;
            const float x1 = ox + dx * t_max;
            const float z1 = oz + dz * t_max;
            const float b = FloorProp::kBinSize;
            int ix = static_cast<int>(std::floor(x0 / b));
            int iz = static_cast<int>(std::floor(z0 / b));
            const int ix_end = static_cast<int>(std::floor(x1 / b));
            const int iz_end = static_cast<int>(std::floor(z1 / b));
            const int step_x = (ix_end > ix) ? 1 : (ix_end < ix ? -1 : 0);
            const int step_z = (iz_end > iz) ? 1 : (iz_end < iz ? -1 : 0);
            const float seg_dx = x1 - x0;
            const float seg_dz = z1 - z0;
            float t_max_x = 1e30f;
            float t_max_z = 1e30f;
            float t_delta_x = 1e30f;
            float t_delta_z = 1e30f;
            if (step_x != 0 && std::fabs(seg_dx) > 1e-8f) {
                const float next_x =
                    (step_x > 0) ? (static_cast<float>(ix + 1) * b) : (static_cast<float>(ix) * b);
                t_delta_x = std::fabs(b / seg_dx);
                t_max_x = (next_x - x0) / seg_dx;
            }
            if (step_z != 0 && std::fabs(seg_dz) > 1e-8f) {
                const float next_z =
                    (step_z > 0) ? (static_cast<float>(iz + 1) * b) : (static_cast<float>(iz) * b);
                t_delta_z = std::fabs(b / seg_dz);
                t_max_z = (next_z - z0) / seg_dz;
            }
            const int n = std::abs(ix_end - ix) + std::abs(iz_end - iz) + 1;
            for (int step = 0; step < n; ++step) {
                const auto* bin = prop.bin_at((static_cast<float>(ix) + 0.5f) * b,
                                              (static_cast<float>(iz) + 0.5f) * b);
                if (bin != nullptr) {
                    for (const std::size_t i : *bin) {
                        test_tri(i);
                        if (stop_when_blocked && hit && out_t + slack < max_t) {
                            return true;
                        }
                    }
                }
                if (ix == ix_end && iz == iz_end) {
                    break;
                }
                if (t_max_x < t_max_z) {
                    t_max_x += t_delta_x;
                    ix += step_x;
                } else {
                    t_max_z += t_delta_z;
                    iz += step_z;
                }
            }
        }
        return hit;
    }

    bool FloorField::segment_blocked(
        float ax, float ay, float az, float bx, float by, float bz, float slack) const {
        if (!has_blockers()) {
            return false;
        }
        const float dx = bx - ax;
        const float dy = by - ay;
        const float dz = bz - az;
        const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (dist <= slack) {
            return false;
        }
        const float inv = 1.0f / dist;
        float hit = dist;
        if (!raycast_hulls(m_blockers,
                           ax,
                           ay,
                           az,
                           dx * inv,
                           dy * inv,
                           dz * inv,
                           dist,
                           hit,
                           RayHitFilter::All,
                           false,
                           true,
                           slack)) {
            return false;
        }
        return hit + slack < dist;
    }

    void FloorProp::build_index() {
        m_bins.clear();
        const auto& tris = m_tris;
        for (std::size_t i = 0; i + 8 < tris.size(); i += 9) {
            const float min_x = std::min(tris[i], std::min(tris[i + 3], tris[i + 6]));
            const float max_x = std::max(tris[i], std::max(tris[i + 3], tris[i + 6]));
            const float min_z = std::min(tris[i + 2], std::min(tris[i + 5], tris[i + 8]));
            const float max_z = std::max(tris[i + 2], std::max(tris[i + 5], tris[i + 8]));
            const int ix0 = static_cast<int>(std::floor(min_x / kBinSize));
            const int ix1 = static_cast<int>(std::floor(max_x / kBinSize));
            const int iz0 = static_cast<int>(std::floor(min_z / kBinSize));
            const int iz1 = static_cast<int>(std::floor(max_z / kBinSize));
            for (int ix = ix0; ix <= ix1; ++ix) {
                for (int iz = iz0; iz <= iz1; ++iz) {
                    const std::uint64_t key =
                        (static_cast<std::uint64_t>(static_cast<std::uint32_t>(ix)) << 32) |
                        static_cast<std::uint32_t>(iz);
                    m_bins[key].push_back(i);
                }
            }
        }
    }

    const std::vector<std::size_t>* FloorProp::bin_at(float world_x, float world_z) const {
        if (m_bins.empty()) {
            return nullptr;
        }
        const int ix = static_cast<int>(std::floor(world_x / kBinSize));
        const int iz = static_cast<int>(std::floor(world_z / kBinSize));
        const std::uint64_t key =
            (static_cast<std::uint64_t>(static_cast<std::uint32_t>(ix)) << 32) |
            static_cast<std::uint32_t>(iz);
        const auto it = m_bins.find(key);
        if (it == m_bins.end()) {
            return nullptr;
        }
        return &it->second;
    }

    float FloorField::sample_terrain(float world_x, float world_z, float fallback) const {
        return m_height_field.sample(world_x, world_z, fallback);
    }

    float FloorField::sample(float world_x, float world_z, float fallback) const {
        float y = fallback;
        static_cast<void>(query_stand(world_x, world_z, fallback, y, false));
        return y;
    }

    bool FloorField::resolve_standing(float world_x,
                                      float world_z,
                                      float current_y,
                                      float& out_y) const {
        return query_stand(world_x, world_z, current_y, out_y, false);
    }

    float FloorField::get_floor(float world_x, float world_z) const {
        if (m_height_field.covers(world_x, world_z)) {
            float y = 0.0f;
            const float ground = sample_terrain(world_x, world_z, 0.0f);
            static_cast<void>(query_stand(world_x, world_z, ground, y, true));
            return y;
        }
        float y = 0.0f;
        static_cast<void>(query_stand(world_x, world_z, 0.0f, y, true));
        return y;
    }

    bool FloorField::can_occupy(float world_x, float world_z, float current_y) const {
        float y = current_y;
        return query_stand(world_x, world_z, current_y, y, false);
    }

    bool FloorField::query_stand(
        float world_x, float world_z, float fallback, float& out_y, bool from_sky) const {
        constexpr float kMaxHeight = 1000.0f;  // 100 * METER (GetFloor)
        const bool have_terrain = m_height_field.covers(world_x, world_z);
        const float ground = m_height_field.sample(world_x, world_z, fallback);
        float origin_y = std::max(ground, fallback) + kMaxHeight;
        float stop_y = have_terrain ? ground : (origin_y - 2.0f * kMaxHeight);
        if (!have_terrain && from_sky) {
            float top = -1e30f;
            float bottom = 1e30f;
            bool have_bound = false;
            for (const auto& prop : m_props) {
                if (prop.empty()) {
                    continue;
                }
                top = std::max(top, prop.max_y());
                bottom = std::min(bottom, prop.min_y());
                have_bound = true;
            }
            if (!have_bound) {
                out_y = 0.0f;
                return false;
            }
            origin_y = top + 10.0f;
            stop_y = bottom - 10.0f;
        }
        const float max_t = origin_y - stop_y;
        if (max_t <= 1e-4f) {
            out_y = have_terrain ? ground : fallback;
            return have_terrain;
        }
        const bool near_water = have_terrain && ground < map::kWaterPosY + kWaterBridgeSlack;

        float best_land = have_terrain ? ground : fallback;
        float best_land_t = max_t;
        float best_water = ground;
        float best_water_t = max_t;
        bool have_water_bridge = false;
        bool have_stand = have_terrain;
        bool have_high = false;

        if (const auto* candidates = prop_candidates(world_x, world_z)) {
            for (const std::size_t prop_index : *candidates) {
                if (prop_index >= m_props.size()) {
                    continue;
                }
                const FloorProp& prop = m_props[prop_index];
                if (!prop.contains_xz(world_x, world_z)) {
                    continue;
                }
                const auto& tris = prop.tris();
                auto test_tri = [&](std::size_t i) {
                    if (i + 8 >= tris.size()) {
                        return;
                    }
                    float t = 0;
                    if (!ray_tri_down(world_x,
                                      origin_y,
                                      world_z,
                                      max_t,
                                      tris[i],
                                      tris[i + 1],
                                      tris[i + 2],
                                      tris[i + 3],
                                      tris[i + 4],
                                      tris[i + 5],
                                      tris[i + 6],
                                      tris[i + 7],
                                      tris[i + 8],
                                      t,
                                      have_terrain && !from_sky)) {
                        return;
                    }
                    const float hit_y = origin_y - t;
                    if (have_terrain && hit_y <= ground) {
                        return;
                    }
                    if (near_water && hit_y >= map::kWaterPosY - kWaterBridgeSlack &&
                        hit_y <= map::kWaterPosY + kMaxBridgeAboveWater) {
                        if (t < best_water_t) {
                            best_water_t = t;
                            best_water = hit_y;
                            have_water_bridge = true;
                        }
                    }
                    // MovePlayer only steps INNER_HEIGHT. Outdoor arch lintels and indoor
                    // ledge tops sit well above that and must not become the click/stand
                    // target. GetFloor (from_sky) still takes the highest deck so monsters
                    // on a terrace keep their own Y.
                    if (!from_sky && hit_y > fallback + kStepHeight) {
                        have_high = true;
                        return;
                    }
                    if (t < best_land_t) {
                        best_land_t = t;
                        best_land = hit_y;
                        have_stand = true;
                    }
                };
                if (prop.indexed()) {
                    const auto* bin = prop.bin_at(world_x, world_z);
                    if (bin == nullptr) {
                        continue;
                    }
                    for (const std::size_t i : *bin) {
                        test_tri(i);
                    }
                } else {
                    for (std::size_t i = 0; i + 8 < tris.size(); i += 9) {
                        test_tri(i);
                    }
                }
            }
        }

        if (have_water_bridge) {
            out_y = best_water;
            return true;
        }
        out_y = best_land;
        if (!have_terrain) {
            // Indoor maps have no KCM. Empty XZ must not stay occupyable at the last
            // Y (CanStand is false when GetFloor returns 0).
            return have_stand;
        }
        return have_stand || !have_high;
    }

}  // namespace openkal::terrain
