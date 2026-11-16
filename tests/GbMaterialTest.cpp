/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "fixture_util.h"
#include "openkal/terrain/GbModel.h"
#include "test_util.h"

#include <cmath>
#include <cstring>
#include <limits>

namespace {
    using openkal::test::expect;
    namespace terrain = openkal::terrain;

    template <typename T>
    void store(std::vector<std::uint8_t>& bytes, std::size_t offset, T value) {
        std::memcpy(bytes.data() + offset, &value, sizeof(value));
    }

    // Extend the existing project-owned triangle with a synthetic material frame.
    terrain::GbModel fixture(const std::string& path, std::uint16_t flags, float power) {
        expect(openkal::test::write_rigid_gb_v10(path, "owned.dds"), "write owned triangle");
        auto bytes = *openkal::platform::read_file(path);
        store(bytes, 44, std::uint32_t{46});  // ten texture bytes plus a 36-byte frame
        store(bytes, 58, std::uint16_t{1});
        store(bytes, 80, flags);
        store(bytes, 86, power);
        store(bytes, 90, std::uint32_t{10});
        openkal::test::append_pod(bytes, std::uint32_t{0xff123456});
        openkal::test::append_pod(bytes, std::uint32_t{0xff654321});
        openkal::test::append_pod(bytes, std::uint32_t{0xff007b7b});
        openkal::test::append_pod(bytes, 0.65f);
        for (int i = 0; i < 5; ++i) {
            openkal::test::append_pod(bytes, 0.0f);
        }
        expect(openkal::test::write_bytes(path, bytes), "write owned material frame");
        terrain::GbModel model;
        expect(terrain::load_gb(path, model), "load owned material triangle");
        return model;
    }

    bool near(float a, float b) {
        return std::fabs(a - b) < 0.0001f;
    }
}  // namespace

int main() {
    const auto path = openkal::test::temp_dir("material") + "/owned.gb";
    const auto glossy = fixture(path, terrain::kMatSpecular, 24.0f);
    expect(glossy.materials[0].specular_power == 24.0f && glossy.meshes[0].specular_power == 24.0f,
           "retain active highlight exponent");
    const auto& frame = glossy.materials[0].frames[0];
    expect(frame.ambient == 0xff123456 && frame.diffuse == 0xff654321 &&
               frame.specular == 0xff007b7b,
           "retain packed colors without interpreting as tint");
    expect(near(frame.opacity, 0.65f) && glossy.meshes[0].alpha_blend,
           "preserve material opacity and blending");
    expect(glossy.meshes[0].normals.size() == 3 &&
               glossy.meshes[0].normals[0] == std::array<float, 3>{0, 1, 0},
           "load stored vertex normals");
    expect(fixture(path, 0, 24.0f).meshes[0].specular_power == 0.0f,
           "exponent without flag does not enable highlights");
    for (const float power :
         {-1.0f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
        expect(fixture(path, terrain::kMatSpecular, power).meshes[0].specular_power == 0.0f,
               "reject malformed highlight exponent");
    }
    expect(fixture(path, terrain::kMatSpecular, 0).meshes[0].specular_power == 1,
           "zero exponent has a finite minimum");

    expect(openkal::test::write_rigid_gb_v7(path, "owned.dds"), "write owned version 7 mesh");
    auto bytes = *openkal::platform::read_file(path);
    store(bytes, 59, terrain::kMatSpecular);
    expect(openkal::test::write_bytes(path, bytes), "write version 7 gloss flag");
    terrain::GbModel legacy;
    expect(terrain::load_gb(path, legacy) && legacy.meshes[0].specular_power == 40 &&
               legacy.materials[0].frames[0].specular == 0xffe5e5e5,
           "legacy inline material retains colors and the default exponent");

    fixture(path, terrain::kMatSpecular, 17);
    bytes = *openkal::platform::read_file(path);
    // Version 12 adds a model name and bounds to the existing owned header.
    bytes.insert(bytes.begin() + 60, 24, 0);
    bytes.insert(bytes.begin() + 8, 64, 0);
    bytes[0] = 12;
    expect(openkal::test::write_bytes(path, bytes), "write owned version 12 mesh");
    terrain::GbModel modern;
    expect(terrain::load_gb(path, modern) && modern.meshes[0].specular_power == 17 &&
               modern.meshes[0].normals[0] == std::array<float, 3>{0, 1, 0},
           "version 12 retains material exponent and normals");

    terrain::GbMeshCpu mesh;
    mesh.normals = {{{1, 1, 0}}};
    std::array<float, 16> transform{2, 0, 0, 0, 0, 1, 0, 0, 0, 0, 3, 0, 50, 70, 90, 1};
    terrain::transform_mesh_normals(mesh, transform);
    expect(near(mesh.normals[0][0], 1.0f / std::sqrt(5.0f)) &&
               near(mesh.normals[0][1], 2.0f / std::sqrt(5.0f)),
           "normal transform uses inverse transpose and ignores translation");
    terrain::GbSkinVert vertex;
    vertex.normal = {1, 1, 0};
    vertex.influence_count = 1;
    mesh.skin_verts = {vertex};
    mesh.bone_palette = {0};
    terrain::skin_mesh(mesh, {transform});
    expect(near(mesh.normals[0][0], 1.0f / std::sqrt(5.0f)) &&
               near(mesh.normals[0][1], 2.0f / std::sqrt(5.0f)),
           "skinned normals follow bone scale independently of positions");
    transform = {0, 2, 0, 0, -1, 0, 0, 0, 0, 0, 3, 0, 50, 70, 90, 1};
    terrain::skin_mesh(mesh, {transform});
    expect(near(mesh.normals[0][0], -2.0f / std::sqrt(5.0f)) &&
               near(mesh.normals[0][1], 1.0f / std::sqrt(5.0f)),
           "skinned normals follow rotated and scaled bones");
    const auto skinned = mesh;
    mesh.normals = {{{0.25f, 0.5f, 0.75f}}};
    terrain::skin_mesh(mesh, {transform}, false);
    expect(mesh.pos_uv == skinned.pos_uv &&
               mesh.normals == std::vector<std::array<float, 3>>{{{0.25f, 0.5f, 0.75f}}},
           "position-only skinning preserves geometry and leaves unused normals untouched");
    // Restore a diagonal matrix before checking singular transforms.
    transform = {2, 0, 0, 0, 0, 1, 0, 0, 0, 0, 3, 0, 50, 70, 90, 1};
    transform[0] = 0;
    terrain::skin_mesh(mesh, {transform});
    expect(mesh.normals[0] == std::array<float, 3>{}, "singular bone transform has no highlight");
    return openkal::test::g_failures ? 1 : 0;
}
