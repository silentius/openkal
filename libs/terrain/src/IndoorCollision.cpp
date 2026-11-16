/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/terrain/IndoorCollision.h"

#include "openkal/map/Coords.h"
#include "openkal/map/IndoorMap.h"
#include "openkal/platform/File.h"
#include "openkal/platform/Path.h"
#include "openkal/terrain/GbModel.h"

#include <algorithm>
#include <cmath>

namespace openkal::terrain {
    namespace {
        constexpr float kHeightTolerance = 1.0f;
        constexpr float kBodyHeight = 11.5f;

        bool finite(engine::Vec3 p) {
            return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
        }

        std::string
        resolve_model(const std::string& root, const std::string& stem, std::string name) {
            name = platform::Path::normalize_separators(std::move(name));
            if (name.size() < 3 || (name.compare(name.size() - 3, 3, ".gb") != 0 &&
                                    name.compare(name.size() - 3, 3, ".GB") != 0)) {
                name += ".gb";
            }
            const auto direct = platform::join_path(root, name);
            if (platform::file_exists(direct)) {
                return direct;
            }
            return platform::join_path(platform::join_path(root, stem),
                                       name.substr(name.find_last_of('/') + 1));
        }

        bool headroom(const FloorField& floor, engine::Vec3 p) {
            float hit = kBodyHeight;
            return !floor.raycast(
                p.x, p.y + 0.1f, p.z, 0, 1, 0, kBodyHeight - 0.1f, hit, RayHitFilter::All);
        }
    }  // namespace

    bool IndoorCollision::load(const std::string& root,
                               const std::string& path,
                               int cell_x,
                               int cell_y) {
        m_loaded = false;
        m_floor = FloorField{};
        map::IndoorMap map;
        if (!map.load(path) || map.regions().empty()) {
            return false;
        }
        const auto origin = engine::Mat4::translation(
            static_cast<float>(coords::cell_origin_pixels(coords::even_cell(cell_x))),
            0,
            static_cast<float>(coords::cell_origin_pixels(coords::even_cell(cell_y))));
        auto add = [&](const std::string& name, const engine::Mat4& transform, bool region) {
            const auto model = resolve_model(root, map.stem(), name);
            GbModel cpu;
            if (!load_gb(model, cpu)) {
                return false;
            }
            if (region) {
                if (cpu.cls_tris.empty() && cpu.meshes.empty()) {
                    return false;
                }
                m_floor.add_prop(model, transform, cell_x, cell_y);
                m_floor.add_blocker(model, transform, cell_x, cell_y);
                // Collision-only regions still have to obstruct movement.
                m_floor.add_obstacle(model, transform, cell_x, cell_y);
            } else {
                m_floor.add_obstacle(model, transform, cell_x, cell_y);
            }
            return true;
        };
        for (const auto& region : map.regions()) {
            if (!add(region.name(), origin, true)) {
                return false;
            }
        }
        for (const auto& object : map.objects()) {
            const auto& v = object.matrix();
            engine::Mat4 local;
            local.m = {v[0],
                       v[1],
                       v[2],
                       0,
                       v[3],
                       v[4],
                       v[5],
                       0,
                       v[6],
                       v[7],
                       v[8],
                       0,
                       v[9],
                       v[10],
                       v[11],
                       1};
            if (!add(object.path(), engine::Mat4::multiply(origin, local), false)) {
                return false;
            }
        }
        m_loaded = !m_floor.props().empty();
        return m_loaded;
    }

    const FloorField* IndoorCollision::floor() const {
        return m_loaded ? &m_floor : nullptr;
    }

    bool IndoorCollision::resolve_standing(const FloorField& floor, engine::Vec3 p, float& height) {
        if (!finite(p) || !floor.can_occupy(p.x, p.z, p.y)) {
            return false;
        }
        height = floor.sample(p.x, p.z, p.y);
        p.y = height;
        return headroom(floor, p);
    }

    bool IndoorCollision::standing(const FloorField& floor, engine::Vec3 p) {
        float height = 0;
        return resolve_standing(floor, p, height) && std::fabs(p.y - height) <= kHeightTolerance;
    }

    bool IndoorCollision::can_move(const FloorField& floor, engine::Vec3 from, engine::Vec3 to) {
        if (!finite(from) || !finite(to)) {
            return false;
        }
        const engine::Vec3 delta{to.x - from.x, to.y - from.y, to.z - from.z};
        const float distance = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
        // Bounds work and avoids converting unbounded coordinates to an integer step count.
        if (distance > 100.0f) {
            return false;
        }
        float start_floor = 0;
        // Protocol rounding can put the feet just below the supporting floor skin.
        if (!resolve_standing(floor, from, start_floor) ||
            from.y < start_floor - kHeightTolerance ||
            !headroom(floor, {from.x, std::max(from.y, start_floor), from.z})) {
            return false;
        }
        const bool grounded = standing(floor, from) && standing(floor, to);
        if (delta.y > 0.0f && !standing(floor, to)) {
            return false;
        }
        float previous_height = start_floor;
        const int steps = std::max(1, static_cast<int>(std::ceil(distance / 2.0f)));
        for (int i = 1; i <= steps; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(steps);
            engine::Vec3 p{from.x + delta.x * t, from.y + delta.y * t, from.z + delta.z * t};
            if (grounded) {
                p.y = previous_height;
            }
            float height = 0;
            if (!resolve_standing(floor, p, height) ||
                (!grounded && (p.y < height - kHeightTolerance ||
                               !headroom(floor, {p.x, std::max(p.y, height), p.z})))) {
                return false;
            }
            previous_height = height;
        }
        if (grounded && std::fabs(previous_height - to.y) > kHeightTolerance) {
            return false;
        }
        if (distance > 1e-4f) {
            for (float offset : {grounded ? FloorField::kStepHeight + 0.3f : 0.3f, kBodyHeight}) {
                float hit = distance;
                if (floor.raycast(from.x,
                                  from.y + offset,
                                  from.z,
                                  delta.x / distance,
                                  delta.y / distance,
                                  delta.z / distance,
                                  distance,
                                  hit,
                                  RayHitFilter::All)) {
                    return false;
                }
            }
        }
        return true;
    }
}  // namespace openkal::terrain
