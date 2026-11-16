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
#include "openkal/platform/File.h"
#include "openkal/platform/Path.h"
#include "openkal/terrain/GbModel.h"
#include "test_util.h"

#include <array>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace {

    void identity16(std::array<float, 16>& m) {
        m = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    }

}  // namespace

int main() {
    using openkal::test::expect;

    const std::string root = openkal::test::temp_dir("gb");
    const std::string tex = openkal::platform::join_path(root, "stub.dds");
    const std::string water_tex = openkal::platform::join_path(root, "water_001.dds");
    const std::string gb = openkal::platform::join_path(root, "stub.gb");
    const std::string water_gb = openkal::platform::join_path(root, "water.gb");

    expect(openkal::test::write_solid_dds(tex, 16, 16, 180, 160, 140), "write stub dds");
    expect(openkal::test::write_solid_dds(water_tex, 16, 16, 100, 140, 180, 255),
           "write water dds");
    expect(openkal::test::write_rigid_gb_v10(gb, "stub.dds"), "write stub gb");
    expect(openkal::test::write_rigid_gb_v10(water_gb, "water_001.dds"), "write water gb");
    const std::string gb7 = openkal::platform::join_path(root, "stub_v7.gb");
    expect(openkal::test::write_rigid_gb_v7(gb7, "stub.dds"), "write stub gb v7");

    openkal::terrain::GbModel model;
    expect(openkal::terrain::load_gb(gb, model), "load_gb");
    expect(model.meshes.size() == 1, "has meshes");
    expect(!model.materials.empty(), "has materials");
    expect(model.materials[0].texture.find("stub") != std::string::npos, "material texture name");
    expect(model.meshes[0].indices.size() == 3, "one triangle");
    expect(model.meshes[0].pos_uv.size() >= 15, "mesh has verts");

    openkal::terrain::GbModel model7;
    expect(openkal::terrain::load_gb(gb7, model7), "load_gb v7");
    expect(model7.meshes.size() == 1, "v7 has meshes");
    expect(!model7.materials.empty() &&
               model7.materials[0].texture.find("stub") != std::string::npos,
           "v7 texture name");
    expect(model7.meshes[0].indices.size() == 3, "v7 one triangle");

    openkal::terrain::ImageRgba img;
    expect(openkal::terrain::load_gb_texture(gb, model.materials[0].texture, img), "load texture");
    expect(img.width == 16 && img.height == 16 &&
               img.rgba.size() == static_cast<std::size_t>(img.width) * img.height * 4,
           "texture rgba");

    openkal::terrain::GbModel water_model;
    expect(openkal::terrain::load_gb(water_gb, water_model), "load water gb");
    expect(water_model.materials[0].texture.find("water") != std::string::npos, "water tex name");
    openkal::terrain::ImageRgba water_img;
    expect(openkal::terrain::load_gb_texture(water_gb, water_model.materials[0].texture, water_img),
           "water tex load");
    expect(water_img.rgba.size() >= 4 && water_img.rgba[3] == 102, "water alpha forced");

    // Material UV sampling (waterfall-style) without shipping external asset pack GBs.
    {
        openkal::terrain::GbMaterial mat;
        mat.frames = {
            {1.0f, 0.0f, 0.0f},
            {1.0f, 0.0f, 0.5f},
            {1.0f, 0.0f, 1.0f},
        };
        const std::vector<std::uint16_t> times = {0, 1499, 2999};
        float u = 0, v = 0, op = 1;
        expect(openkal::terrain::sample_material_uv(mat, times, 1500.0f, u, v, op), "uv sample");
        expect(v > 0.4f && v < 0.6f, "uv mid scroll");
        expect(!openkal::terrain::is_waterfall_texture("water_002.dds"), "water_002 not waterfall");
        expect(openkal::terrain::is_waterfall_texture("waterfall_big1.dds"),
               "waterfall name detect");
        expect(openkal::terrain::is_water_surface_texture("water_001.dds"), "water surface detect");
    }

    // Tiny 2-bone skinning: mid-time pose differs from kf0; (*LOOP) wrap
    // semantics.
    {
        openkal::terrain::GbModel skel;
        skel.bones.resize(2);
        identity16(skel.bones[0].inverse);
        identity16(skel.bones[1].inverse);
        skel.bones[0].parent = 255;
        skel.bones[1].parent = 0;
        skel.bone_count = 2;

        openkal::terrain::GbModel mot;
        openkal::terrain::GbAnimClip clip;
        clip.keyframe_count = 3;
        clip.bone_count = 2;
        clip.key_times = {0, 500, 1000};
        clip.loop_start_ms = 300.0f;
        clip.samples.resize(6);
        // kf0: identity locals
        for (int b = 0; b < 2; ++b) {
            clip.samples[b] = {};
            clip.samples[b].qw = 1;
        }
        // kf1: translate bone1 +X
        clip.samples[2] = clip.samples[0];
        clip.samples[3] = clip.samples[1];
        clip.samples[3].px = 20.0f;
        // kf2: translate bone1 +X more
        clip.samples[4] = clip.samples[0];
        clip.samples[5] = clip.samples[1];
        clip.samples[5].px = 40.0f;
        clip.bone0 = {clip.samples[0], clip.samples[1]};
        mot.anims.push_back(clip);

        openkal::terrain::GbModel cloth;
        openkal::terrain::GbMeshCpu mesh;
        mesh.vertex_layout = openkal::terrain::GbVertexType::Blend1;
        mesh.bone_palette = {0, 1};
        mesh.skin_verts.resize(1);
        mesh.skin_verts[0].px = 0;
        mesh.skin_verts[0].py = 0;
        mesh.skin_verts[0].pz = 0;
        mesh.skin_verts[0].influence_count = 1;
        mesh.skin_verts[0].idx[0] = 1;  // palette index -> bone 1
        mesh.pos_uv = {0, 0, 0, 0, 0};
        mesh.indices = {0, 0, 0};
        cloth.meshes.push_back(mesh);

        expect(openkal::terrain::skin_with_skeleton_anim(cloth, skel, mot), "skin synthetic");
        expect(std::fabs(cloth.meshes[0].pos_uv[0]) < 0.01f, "kf0 at origin");

        std::vector<std::array<float, 16>> skin0, skin_mid, skin_wrap, skin_loop;
        expect(openkal::terrain::eval_skin_matrices_at_time(
                   skel.bones, mot.anims[0], 0.0f, true, skin0),
               "eval t0");
        expect(openkal::terrain::eval_skin_matrices_at_time(
                   skel.bones, mot.anims[0], 500.0f, true, skin_mid),
               "eval mid");
        openkal::terrain::GbModel c0 = cloth;
        openkal::terrain::GbModel c1 = cloth;
        // Restore bind for re-skin.
        c0.meshes[0].skin_verts = mesh.skin_verts;
        c1.meshes[0].skin_verts = mesh.skin_verts;
        openkal::terrain::skin_mesh(c0.meshes[0], skin0);
        openkal::terrain::skin_mesh(c1.meshes[0], skin_mid);
        expect(std::fabs(c1.meshes[0].pos_uv[0] - c0.meshes[0].pos_uv[0]) > 1.0f,
               "mid pose differs from start");

        expect(mot.anims[0].loop_start_ms == 300.0f, "loop start ms");
        expect(openkal::terrain::eval_skin_matrices_at_time(
                   skel.bones, mot.anims[0], mot.anims[0].duration_ms(), true, skin_wrap),
               "eval wrap");
        expect(openkal::terrain::eval_skin_matrices_at_time(
                   skel.bones, mot.anims[0], 300.0f, true, skin_loop),
               "eval loop start");
        openkal::terrain::GbModel cw = cloth;
        openkal::terrain::GbModel cl = cloth;
        cw.meshes[0].skin_verts = mesh.skin_verts;
        cl.meshes[0].skin_verts = mesh.skin_verts;
        openkal::terrain::skin_mesh(cw.meshes[0], skin_wrap);
        openkal::terrain::skin_mesh(cl.meshes[0], skin_loop);
        expect(std::fabs(cw.meshes[0].pos_uv[0] - cl.meshes[0].pos_uv[0]) < 0.1f,
               "duration wrap matches loop_start pose");
        expect(std::fabs(cw.meshes[0].pos_uv[0] - c0.meshes[0].pos_uv[0]) > 0.5f,
               "duration wrap is not t=0 intro");
    }

    {
        const std::string cls_gb = openkal::platform::join_path(root, "bridge_plank.gb");
        expect(openkal::test::write_cls_only_gb_v11(cls_gb), "write cls-only gb");
        openkal::terrain::GbModel cls_model;
        expect(openkal::terrain::load_gb(cls_gb, cls_model), "load cls-only gb");
        expect(cls_model.meshes.empty(), "cls-only has no render mesh");
        expect(cls_model.cls_tris.size() == 9, "one cls triangle");
        expect(std::fabs(cls_model.cls_tris[0]) < 0.01f &&
                   std::fabs(cls_model.cls_tris[1]) < 0.01f &&
                   std::fabs(cls_model.cls_tris[2]) < 0.01f,
               "cls v0 at origin");
        expect(std::fabs(cls_model.cls_tris[3]) < 0.01f &&
                   std::fabs(cls_model.cls_tris[4]) < 0.01f &&
                   std::fabs(cls_model.cls_tris[5] - 10.0f) < 0.01f,
               "cls v1 at +Z");
        expect(std::fabs(cls_model.cls_tris[6] - 10.0f) < 0.01f &&
                   std::fabs(cls_model.cls_tris[7]) < 0.01f &&
                   std::fabs(cls_model.cls_tris[8]) < 0.01f,
               "cls v2 at +X");
    }

    {
        const std::string lm_gb = openkal::platform::join_path(root, "d_p_lm.gb");
        expect(openkal::test::write_lm_static_gb_v10(lm_gb, "stub.dds", "lightmap.dds"),
               "write lm-static gb");
        openkal::terrain::GbModel lm_model;
        expect(openkal::terrain::load_gb(lm_gb, lm_model), "load_gb v10 vertex type 6");
        expect(lm_model.meshes.size() == 1, "lm-static has meshes");
        expect(lm_model.meshes[0].vertex_layout == openkal::terrain::GbVertexType::RigidDouble,
               "lm-static remaps to RigidDouble");
        expect(lm_model.meshes[0].indices.size() == 3, "lm-static one triangle");
        expect(lm_model.meshes[0].pos_uv.size() >= 15, "lm-static has verts");
        expect(std::fabs(lm_model.meshes[0].pos_uv[5] - 10.0f) < 0.01f, "lm-static v1 at +X");
        expect(lm_model.meshes[0].lightmap_uv.size() == 6, "lm-static second UV set");
        expect(std::fabs(lm_model.meshes[0].lightmap_uv[2] - 1.0f) < 0.01f,
               "lm-static lightmap UV of v1");
        expect(lm_model.meshes[0].lightmap == "lightmap.dds", "lm-static lightmap name after mesh");
    }

    return openkal::test::g_failures == 0 ? 0 : 1;
}
