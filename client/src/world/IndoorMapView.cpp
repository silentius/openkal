/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/IndoorMapView.h"

#include "openkal/client/WorldObjectCatalog.h"
#include "openkal/map/Constants.h"
#include "openkal/map/Coords.h"
#include "openkal/map/IndoorMap.h"
#include "openkal/platform/File.h"
#include "openkal/platform/Log.h"
#include "openkal/platform/Path.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace openkal::client {
    namespace {

        [[nodiscard]] std::string slash_path(std::string path) {
            return platform::Path::normalize_separators(std::move(path));
        }

        [[nodiscard]] std::string last_name(const std::string& path) {
            const auto slash = path.find_last_of('/');
            if (slash == std::string::npos) {
                return path;
            }
            return path.substr(slash + 1);
        }

        [[nodiscard]] std::string
        resolve_gb(const std::string& map_root, const std::string& stem, const std::string& rel) {
            std::string norm = slash_path(rel);
            if (norm.empty()) {
                return {};
            }
            if (norm.size() < 3 || (norm.compare(norm.size() - 3, 3, ".gb") != 0 &&
                                    norm.compare(norm.size() - 3, 3, ".GB") != 0)) {
                norm += ".gb";
            }
            const std::string a = platform::join_path(map_root, norm);
            if (platform::file_exists(a)) {
                return a;
            }
            const std::string b =
                platform::join_path(platform::join_path(map_root, stem), last_name(norm));
            if (platform::file_exists(b)) {
                return b;
            }
            return a;
        }

        /** D3DX row-major 3x4 → column-major Mat4 (row-vector to column-vector). */
        [[nodiscard]] engine::Mat4 from_d3dx_3x4(const std::array<float, 12>& m12) {
            engine::Mat4 r{};
            r.m = {m12[0],
                   m12[1],
                   m12[2],
                   0.0f,
                   m12[3],
                   m12[4],
                   m12[5],
                   0.0f,
                   m12[6],
                   m12[7],
                   m12[8],
                   0.0f,
                   m12[9],
                   m12[10],
                   m12[11],
                   1.0f};
            return r;
        }

        void place(engine::IEngine& engine,
                   const std::string& gb_path,
                   const engine::Mat4& model,
                   float wx,
                   float wy,
                   float wz,
                   float cull_hx,
                   float cull_hy,
                   float cull_hz,
                   bool walkable,
                   bool polygon_offset,
                   std::string catalog_key,
                   const WorldObjectCatalog* object_catalog,
                   std::uint32_t* next_fx_serial,
                   std::unordered_map<std::string, WorldRenderCache::GpuModel>& model_cache,
                   std::unordered_map<std::string, engine::TextureHandle>& tex_cache,
                   FloorField& floor,
                   std::vector<WorldRenderCache::DrawnProp>& props,
                   int& ok,
                   int& missing) {
            if (walkable) {
                floor.add_prop(
                    gb_path, model, IndoorMapView::kFloorPatch, IndoorMapView::kFloorPatch);
                floor.add_blocker(
                    gb_path, model, IndoorMapView::kFloorPatch, IndoorMapView::kFloorPatch);
            } else {
                floor.add_obstacle(
                    gb_path, model, IndoorMapView::kFloorPatch, IndoorMapView::kFloorPatch);
            }
            const WorldRenderCache::GpuModel* gpu =
                WorldRenderCache::get_or_load_gb(engine, gb_path, model_cache, tex_cache);
            WorldRenderCache::DrawnProp p;
            p.gpu = gpu;
            p.patch_x = IndoorMapView::kFloorPatch;
            p.patch_y = IndoorMapView::kFloorPatch;
            p.wx = wx;
            p.wy = wy;
            p.wz = wz;
            p.cull_hx = cull_hx;
            p.cull_hy = cull_hy;
            p.cull_hz = cull_hz;
            p.model = model;
            p.polygon_offset = polygon_offset;
            p.draw = true;
            p.asset_path = gb_path;
            if (!catalog_key.empty() && object_catalog != nullptr &&
                object_catalog->find(catalog_key) != nullptr && next_fx_serial != nullptr) {
                p.catalog_key = std::move(catalog_key);
                p.fx_entity = WorldObjectCatalog::take_fx_entity(*next_fx_serial);
            }
            if (!gpu) {
                ++missing;
            } else {
                WorldRenderCache::apply_instance_bounds(p);
                ++ok;
            }
            props.push_back(p);
        }

    }  // namespace

    bool
    IndoorMapView::load(engine::IEngine& engine,
                        const std::string& map_root,
                        const std::string& map_file,
                        int origin_patch_x,
                        int origin_patch_y,
                        int cell_x,
                        int cell_y,
                        std::unordered_map<std::string, WorldRenderCache::GpuModel>& model_cache,
                        std::unordered_map<std::string, engine::TextureHandle>& tex_cache,
                        FloorField& floor,
                        const WorldObjectCatalog* object_catalog,
                        std::uint32_t* next_fx_serial) {
        unload(floor);
        map::IndoorMap cpu;
        if (!cpu.load(map_file)) {
            return false;
        }
        const float world_origin_x =
            static_cast<float>(origin_patch_x * map::kMapSize * map::kTerrainCellSize);
        const float world_origin_y =
            static_cast<float>(origin_patch_y * map::kMapSize * map::kTerrainCellSize);
        const float cell_ox =
            static_cast<float>(coords::cell_origin_pixels(coords::even_cell(cell_x)));
        const float cell_oy =
            static_cast<float>(coords::cell_origin_pixels(coords::even_cell(cell_y)));
        const float shift_x = cell_ox - world_origin_x;
        const float shift_z = cell_oy - world_origin_y;
        const auto origin_t = engine::Mat4::translation(shift_x, 0.0f, shift_z);
        int ok = 0;
        int missing = 0;

        for (const auto& region : cpu.regions()) {
            const std::string gb = resolve_gb(map_root, cpu.stem(), region.name());
            const float cx = 0.5f * (region.minimum()[0] + region.maximum()[0]) + shift_x;
            const float cy = 0.5f * (region.minimum()[1] + region.maximum()[1]);
            const float cz = 0.5f * (region.minimum()[2] + region.maximum()[2]) + shift_z;
            const float hx = 0.5f * (region.maximum()[0] - region.minimum()[0]);
            const float hy = 0.5f * (region.maximum()[1] - region.minimum()[1]);
            const float hz = 0.5f * (region.maximum()[2] - region.minimum()[2]);
            place(engine,
                  gb,
                  origin_t,
                  cx,
                  cy,
                  cz,
                  hx,
                  hy,
                  hz,
                  true,
                  false,
                  {},
                  object_catalog,
                  next_fx_serial,
                  model_cache,
                  tex_cache,
                  floor,
                  m_props,
                  ok,
                  missing);
        }
        const int region_count = static_cast<int>(cpu.regions().size());
        m_region_part_visibility.assign(static_cast<std::size_t>(region_count), {});

        int matched_region_parts = 0;
        for (int i = 0; i < region_count && i < static_cast<int>(m_props.size()); ++i) {
            const auto& region = cpu.regions()[static_cast<std::size_t>(i)];
            const auto* gpu = m_props[static_cast<std::size_t>(i)].gpu;
            if (gpu != nullptr && gpu->parts.size() == region.parts().size()) {
                ++matched_region_parts;
            }
        }

        for (const auto& obj : cpu.objects()) {
            const std::string gb = resolve_gb(map_root, cpu.stem(), obj.path());
            const auto local = from_d3dx_3x4(obj.matrix());
            const auto model = engine::Mat4::multiply(origin_t, local);
            const float wx = obj.matrix()[9] + shift_x;
            const float wy = obj.matrix()[10];
            const float wz = obj.matrix()[11] + shift_z;
            place(engine,
                  gb,
                  model,
                  wx,
                  wy,
                  wz,
                  0.0f,
                  0.0f,
                  0.0f,
                  false,
                  true,
                  WorldObjectCatalog::indoor_lookup_key(obj.path()),
                  object_catalog,
                  next_fx_serial,
                  model_cache,
                  tex_cache,
                  floor,
                  m_props,
                  ok,
                  missing);
        }

        m_leaf_objects.assign(cpu.leaves().size(), {});
        for (int i = region_count; i < static_cast<int>(m_props.size()); ++i) {
            const WorldRenderCache::DrawnProp& p = m_props[static_cast<std::size_t>(i)];
            float radius = p.cull_radius;
            if (p.cull_hx > 0.0f) {
                radius = std::sqrt(p.cull_hx * p.cull_hx + p.cull_hy * p.cull_hy +
                                   p.cull_hz * p.cull_hz);
            }
            cpu.insert_object(p.wx - shift_x, p.wy, p.wz - shift_z, radius, i, m_leaf_objects);
        }

        m_path = map_file;
        m_stem = cpu.stem();
        m_cell_x = coords::even_cell(cell_x);
        m_cell_y = coords::even_cell(cell_y);
        m_region_count = region_count;
        m_shift_x = shift_x;
        m_shift_z = shift_z;
        m_map = std::move(cpu);
        log::info("IndoorMapView: " + m_stem + " placed=" + std::to_string(ok) + " missing_gb=" +
                  std::to_string(missing) + " leaves=" + std::to_string(m_map.leaves().size()) +
                  " part_maps=" + std::to_string(matched_region_parts) + "/" +
                  std::to_string(region_count));
        return ok > 0;
    }

    void IndoorMapView::unload(FloorField& floor) {
        m_props.clear();
        m_path.clear();
        m_stem.clear();
        m_cell_x = 0;
        m_cell_y = 0;
        m_region_count = 0;
        m_shift_x = 0.0f;
        m_shift_z = 0.0f;
        m_leaf_objects.clear();
        m_region_part_visibility.clear();
        m_map.clear();
        floor.remove_patch(kFloorPatch, kFloorPatch);
    }

    void IndoorMapView::update_visibility(float eye_x,
                                          float eye_y,
                                          float eye_z,
                                          float look_x,
                                          float look_y,
                                          float look_z,
                                          const FrustumPlanes& frustum) {
        for (auto& p : m_props) {
            p.draw = true;
        }
        for (auto& visibility : m_region_part_visibility) {
            visibility.clear();
        }
        if (m_map.nodes().empty() || m_map.leaves().empty()) {
            return;
        }
        float fx = look_x - eye_x;
        float fy = look_y - eye_y;
        float fz = look_z - eye_z;
        const float len = std::sqrt(fx * fx + fy * fy + fz * fz);
        if (len > 1e-6f) {
            const float inv = 1.0f / len;
            fx *= inv;
            fy *= inv;
            fz *= inv;
        }
        // OnSetCamera: eye + (near + 0.05 * METER) along view forward.
        constexpr float kProbe = 1.0f + 0.05f * 10.0f;
        const int leaf = m_map.camera_leaf(
            eye_x + fx * kProbe - m_shift_x, eye_y + fy * kProbe, eye_z + fz * kProbe - m_shift_z);
        if (leaf < 0 || leaf >= static_cast<int>(m_map.leaves().size())) {
            return;
        }
        for (auto& p : m_props) {
            p.draw = false;
        }
        for (const auto& link : m_map.leaves()[static_cast<std::size_t>(leaf)].regions()) {
            const int ri = link.region_index();
            if (ri >= 0 && ri < m_region_count && ri < static_cast<int>(m_props.size())) {
                const std::size_t region_index = static_cast<std::size_t>(ri);
                m_props[region_index].draw = true;
                const auto* gpu = m_props[region_index].gpu;
                const auto& region = m_map.regions()[region_index];
                std::vector<std::uint8_t>& visibility = m_region_part_visibility[region_index];
                if (gpu != nullptr && gpu->parts.size() == region.parts().size() &&
                    m_map.collect_region_part_visibility(link, visibility)) {
                    for (std::size_t part_index = 0; part_index < visibility.size(); ++part_index) {
                        if (visibility[part_index] == 0) {
                            continue;
                        }
                        const auto& part = region.parts()[part_index];
                        const auto& minimum = part.minimum();
                        const auto& maximum = part.maximum();
                        const bool valid_bounds =
                            std::isfinite(minimum[0]) && std::isfinite(minimum[1]) &&
                            std::isfinite(minimum[2]) && std::isfinite(maximum[0]) &&
                            std::isfinite(maximum[1]) && std::isfinite(maximum[2]) &&
                            minimum[0] <= maximum[0] && minimum[1] <= maximum[1] &&
                            minimum[2] <= maximum[2];
                        if (valid_bounds &&
                            !WorldRenderCache::frustum_contains_aabb(frustum,
                                                                     minimum[0] + m_shift_x,
                                                                     minimum[1],
                                                                     minimum[2] + m_shift_z,
                                                                     maximum[0] + m_shift_x,
                                                                     maximum[1],
                                                                     maximum[2] + m_shift_z)) {
                            visibility[part_index] = 0;
                        }
                    }
                    const bool any_visible =
                        std::any_of(visibility.begin(), visibility.end(), [](std::uint8_t value) {
                            return value != 0;
                        });
                    if (!any_visible) {
                        visibility.clear();
                    }
                } else {
                    visibility.clear();
                }
            }
        }
        std::vector<int> vis_leaves;
        m_map.collect_pvs_leaves(m_map.leaves()[static_cast<std::size_t>(leaf)].pvs_bit_index(),
                                 vis_leaves);
        for (const int li : vis_leaves) {
            if (li < 0 || li >= static_cast<int>(m_leaf_objects.size())) {
                continue;
            }
            for (const int oi : m_leaf_objects[static_cast<std::size_t>(li)]) {
                if (oi >= 0 && oi < static_cast<int>(m_props.size())) {
                    m_props[static_cast<std::size_t>(oi)].draw = true;
                }
            }
        }
    }

    bool IndoorMapView::region_part_visible(std::size_t prop_index, std::size_t part_index) const {
        if (prop_index >= m_region_part_visibility.size()) {
            return true;
        }
        const auto& visibility = m_region_part_visibility[prop_index];
        if (visibility.empty() || part_index >= visibility.size()) {
            return true;
        }
        return visibility[part_index] != 0;
    }

}  // namespace openkal::client
