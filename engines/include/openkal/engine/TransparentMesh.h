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

#include "openkal/engine/engine.h"

namespace openkal::engine {
    /** Builds a world-space triangle batch in back-to-front camera order.
     * Intended for two-sided blended meshes; indices retain their winding and UVs.
     * Sorting triangle centroids does not resolve intersecting transparent surfaces.
     * @param vertices Local-space textured vertices.
     * @param indices Triangle-list indices.
     * @param model Local-to-world matrix.
     * @param view World-to-camera view matrix, without projection.
     * @param opacity Alpha multiplier, clamped to 0..1.
     * @return Ordered vertices, or an empty batch for invalid geometry/transforms.
     */
    [[nodiscard]] std::vector<SpriteBatchVertex>
    build_transparent_mesh_batch(const std::vector<VertexP3T2>& vertices,
                                 const std::vector<std::uint32_t>& indices,
                                 const Mat4& model,
                                 const Mat4& view,
                                 float opacity);
}  // namespace openkal::engine
