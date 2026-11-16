/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/engine/TransparentMesh.h"

#include <algorithm>
#include <cmath>

namespace openkal::engine {
    std::vector<SpriteBatchVertex>
    build_transparent_mesh_batch(const std::vector<VertexP3T2>& vertices,
                                 const std::vector<std::uint32_t>& indices,
                                 const Mat4& model,
                                 const Mat4& view,
                                 float opacity) {
        if (vertices.empty() || indices.empty() || indices.size() % 3 != 0 ||
            !std::isfinite(opacity)) {
            return {};
        }
        struct Triangle {
            std::size_t offset;
            float depth;
        };
        std::vector<SpriteBatchVertex> world;
        std::vector<float> depths;
        world.reserve(vertices.size());
        depths.reserve(vertices.size());
        for (const auto& vertex : vertices) {
            const auto point = Mat4::transform_point(model, vertex.px, vertex.py, vertex.pz);
            const auto camera = Mat4::transform_point(view, point.x, point.y, point.z);
            if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z) ||
                !std::isfinite(camera.x) || !std::isfinite(camera.y) || !std::isfinite(camera.z) ||
                !std::isfinite(vertex.u) || !std::isfinite(vertex.v)) {
                return {};
            }
            world.emplace_back(point.x,
                               point.y,
                               point.z,
                               vertex.u,
                               vertex.v,
                               1.0f,
                               1.0f,
                               1.0f,
                               std::clamp(opacity, 0.0f, 1.0f));
            depths.push_back(camera.z);
        }
        std::vector<Triangle> triangles;
        triangles.reserve(indices.size() / 3);
        for (std::size_t i = 0; i < indices.size(); i += 3) {
            if (indices[i] >= world.size() || indices[i + 1] >= world.size() ||
                indices[i + 2] >= world.size()) {
                return {};
            }
            const float depth = depths[indices[i]] / 3.0f + depths[indices[i + 1]] / 3.0f +
                                depths[indices[i + 2]] / 3.0f;
            if (!std::isfinite(depth)) {
                return {};
            }
            triangles.push_back({i, depth});
        }
        // The right-handed view places visible, farther geometry at smaller Z.
        std::stable_sort(triangles.begin(),
                         triangles.end(),
                         [](const Triangle& a, const Triangle& b) { return a.depth < b.depth; });
        std::vector<SpriteBatchVertex> result;
        result.reserve(indices.size());
        for (const auto& triangle : triangles) {
            for (std::size_t i = 0; i < 3; ++i) {
                result.push_back(world[indices[triangle.offset + i]]);
            }
        }
        return result;
    }
}  // namespace openkal::engine
