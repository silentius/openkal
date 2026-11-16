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

#include "test_util.h"

#include <cmath>
#include <limits>

int main() {
    using namespace openkal::engine;
    using openkal::test::expect;
    const std::vector<VertexP3T2> vertices = {{-1, -1, 0, 0, 0},
                                              {1, -1, 0, 1, 0},
                                              {0, 1, 0, 0.5f, 1},
                                              {-1, -1, 2, 0, 0},
                                              {1, -1, 2, 1, 0},
                                              {0, 1, 2, 0.5f, 1}};
    const std::vector<std::uint32_t> indices = {0, 1, 2, 3, 4, 5};
    const auto model = Mat4::translation(20, 3, 0);
    const auto front = Mat4::look_at({20, 3, -10}, {20, 3, 0}, {0, 1, 0});
    const auto back = Mat4::look_at({20, 3, 10}, {20, 3, 0}, {0, 1, 0});
    const auto a = build_transparent_mesh_batch(vertices, indices, model, front, 0.333f);
    const auto b = build_transparent_mesh_batch(vertices, indices, model, back, 1.0f);
    expect(a.size() == 6 && b.size() == 6, "two triangles remain complete");
    if (a.size() == 6 && b.size() == 6) {
        expect(a[0].values()[2] == 2 && a[3].values()[2] == 0 && b[0].values()[2] == 0 &&
                   b[3].values()[2] == 2,
               "camera reversal reverses far-to-near triangle order");
        expect(a[0].values()[0] == 19 && a[0].values()[1] == 2 && a[1].values()[0] == 21 &&
                   a[2].values()[3] == 0.5f && a[2].values()[4] == 1 && a[0].values()[8] == 0.333f,
               "world transform, triangle winding, UVs and fade opacity are preserved");
    }
    const auto mirrored = build_transparent_mesh_batch(
        vertices, indices, model, Mat4::multiply(Mat4::scaling(-1, 1, 1), front), 1.0f);
    expect(mirrored.size() == 6 && mirrored[0].values()[2] == 2,
           "camera X mirror does not change camera-depth order");
    const auto rotated = build_transparent_mesh_batch(
        vertices, indices, Mat4::scaling(1, 1, -1), Mat4::identity(), 2.0f);
    expect(rotated.size() == 6 && rotated[0].values()[2] == -2 && rotated[0].values()[8] == 1,
           "model-space depth reversal is applied before sorting and opacity is clamped");
    auto coplanar = vertices;
    for (auto& v : coplanar) {
        v.pz = 0;
    }
    for (std::size_t i = 3; i < coplanar.size(); ++i) {
        coplanar[i].u += 0.25f;
    }
    const auto stable = build_transparent_mesh_batch(coplanar, indices, Mat4::identity(), front, 1);
    expect(stable.size() == 6 && stable[0].values()[3] == 0 && stable[3].values()[3] == 0.25f &&
               stable[0].values()[0] == -1 && stable[1].values()[0] == 1,
           "coplanar triangles keep source order and winding");
    expect(build_transparent_mesh_batch(vertices, {0, 1}, model, front, 1).empty() &&
               build_transparent_mesh_batch(vertices, {0, 1, 99}, model, front, 1).empty(),
           "malformed triangle lists and invalid indices fail without partial draws");
    auto invalid = vertices;
    invalid[0].u = std::numeric_limits<float>::quiet_NaN();
    auto invalid_view = front;
    invalid_view.m[0] = std::numeric_limits<float>::quiet_NaN();
    expect(build_transparent_mesh_batch(invalid, indices, model, front, 1).empty() &&
               build_transparent_mesh_batch(
                   vertices, indices, model, front, std::numeric_limits<float>::infinity())
                   .empty() &&
               build_transparent_mesh_batch(vertices, indices, model, invalid_view, 1).empty(),
           "nonfinite UVs, opacity and view transforms are rejected");
    return openkal::test::g_failures == 0 ? 0 : 1;
}
