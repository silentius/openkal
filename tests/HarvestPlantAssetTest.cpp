/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/platform/ConfigArchive.h"
#include "openkal/platform/Path.h"
#include "openkal/terrain/GbModel.h"
#include "test_util.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <tuple>

int main() {
    using openkal::test::expect;
    namespace platform = openkal::platform;
    namespace terrain = openkal::terrain;
    const std::string root = "/openkal-owned-decoration-test";
    expect(platform::ConfigArchive::mount(root, OPENKAL_TEST_CONFIG_ARCHIVE),
           "mount packaged resources");
    for (const auto& entry : {std::tuple{"aloe", "aloeVera", 608u},
                              std::tuple{"blue_lotus", "blueLotusLeaf", 1338u},
                              std::tuple{"ginseng", "ginsengRoot", 1242u}}) {
        const auto& [stem, icon, triangles] = entry;
        expect(platform::ConfigArchive::file_exists(platform::join_path(
                   root, std::string("RuntimeData/HyperText/") + icon + ".bmp")),
               "packaged inventory icon is available without optional user data");
        terrain::GbModel model;
        const auto asset =
            platform::join_path(root, std::string("RuntimeData/models/plants/") + stem + ".gb");
        expect(terrain::load_gb(asset, model), "load owned plant from archive");
        expect(model.meshes.size() == 1 && model.materials.size() == 1, "single mesh and material");
        expect(model.bones.empty() && model.cls_tris.empty(),
               "static decoration without collision");
        if (model.meshes.empty() || model.materials.empty()) {
            return 1;
        }
        const auto& mesh = model.meshes.front();
        const auto vertex_count = mesh.pos_uv.size() / 5;
        expect(mesh.indices.size() / 3 == triangles, "complete owned plant mesh");
        float min_y = 1000, max_y = -1000;
        for (std::size_t i = 0; i < vertex_count; ++i) {
            min_y = std::min(min_y, mesh.pos_uv[i * 5 + 1]);
            max_y = std::max(max_y, mesh.pos_uv[i * 5 + 1]);
        }
        expect(min_y < 0 && max_y > 35 && max_y < 50, "buried roots and small plant height");
        for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
            const auto a = mesh.indices[i], b = mesh.indices[i + 1], c = mesh.indices[i + 2];
            expect(a < vertex_count && b < vertex_count && c < vertex_count, "valid mesh indices");
            if (a >= vertex_count || b >= vertex_count || c >= vertex_count) {
                continue;
            }
            float u[3], v[3];
            for (int axis = 0; axis < 3; ++axis) {
                u[axis] = mesh.pos_uv[b * 5 + axis] - mesh.pos_uv[a * 5 + axis];
                v[axis] = mesh.pos_uv[c * 5 + axis] - mesh.pos_uv[a * 5 + axis];
            }
            const float nx = u[1] * v[2] - u[2] * v[1];
            const float ny = u[2] * v[0] - u[0] * v[2];
            const float nz = u[0] * v[1] - u[1] * v[0];
            expect(nx * nx + ny * ny + nz * nz > 0.001f, "non-degenerate leaf triangles");
        }
        terrain::ImageRgba image;
        expect(terrain::load_gb_texture(asset, model.materials.front().texture, image),
               "resolve packaged leaf texture beside mesh");
        expect(image.width == 128 && image.height == 128, "leaf texture dimensions");
        std::set<std::uint32_t> colors;
        for (std::size_t i = 0; i + 3 < image.rgba.size(); i += 4) {
            expect(image.rgba[i + 3] == 255, "opaque leaf texture");
            colors.insert((image.rgba[i] << 16) | (image.rgba[i + 1] << 8) | image.rgba[i + 2]);
        }
        expect(colors.size() > 100, "leaf face shading and texture detail survive decoding");
    }
    platform::ConfigArchive::unmount();
    return openkal::test::g_failures == 0 ? 0 : 1;
}
