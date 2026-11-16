/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/WorldRenderCache.h"

#include "openkal/platform/File.h"
#include "openkal/platform/Log.h"
#include "openkal/platform/Path.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>

namespace openkal::client {

    using GpuLayer = WorldRenderCache::GpuLayer;
    using DrawnPatch = WorldRenderCache::DrawnPatch;
    using GpuSubmesh = WorldRenderCache::GpuSubmesh;
    using GpuModel = WorldRenderCache::GpuModel;
    using DrawnProp = WorldRenderCache::DrawnProp;

    bool WorldRenderCache::is_tree_asset_path(std::string_view path) {
        std::string lower;
        lower.reserve(path.size());
        for (const unsigned char character : path) {
            lower.push_back(static_cast<char>(std::tolower(character)));
        }
        constexpr std::array<std::string_view, 4> keywords = {"tree", "bush", "leaf", "wood"};
        return std::any_of(keywords.begin(), keywords.end(), [&](std::string_view keyword) {
            return lower.find(keyword) != std::string::npos;
        });
    }

    // View-frustum planes from column-major view-projection (includes camera
    // X-mirror).
    void WorldRenderCache::extract_frustum_planes(const engine::Mat4& vp, FrustumPlanes& out) {
        const float* m = vp.m.data();
        // left, right, bottom, top, near, far
        const float rows[6][4] = {
            {m[3] + m[0], m[7] + m[4], m[11] + m[8], m[15] + m[12]},
            {m[3] - m[0], m[7] - m[4], m[11] - m[8], m[15] - m[12]},
            {m[3] + m[1], m[7] + m[5], m[11] + m[9], m[15] + m[13]},
            {m[3] - m[1], m[7] - m[5], m[11] - m[9], m[15] - m[13]},
            {m[3] + m[2], m[7] + m[6], m[11] + m[10], m[15] + m[14]},
            {m[3] - m[2], m[7] - m[6], m[11] - m[10], m[15] - m[14]},
        };
        for (int i = 0; i < 6; ++i) {
            const float len = std::sqrt(rows[i][0] * rows[i][0] + rows[i][1] * rows[i][1] +
                                        rows[i][2] * rows[i][2]);
            const float inv = (len > 1e-8f) ? (1.0f / len) : 1.0f;
            out.p[i][0] = rows[i][0] * inv;
            out.p[i][1] = rows[i][1] * inv;
            out.p[i][2] = rows[i][2] * inv;
            out.p[i][3] = rows[i][3] * inv;
        }
    }

    bool WorldRenderCache::frustum_contains_sphere(
        const FrustumPlanes& fr, float x, float y, float z, float radius) {
        for (int i = 0; i < 6; ++i) {
            const float d = fr.p[i][0] * x + fr.p[i][1] * y + fr.p[i][2] * z + fr.p[i][3];
            if (d < -radius) {
                return false;
            }
        }
        return true;
    }

    bool WorldRenderCache::frustum_contains_aabb(const FrustumPlanes& fr,
                                                 float min_x,
                                                 float min_y,
                                                 float min_z,
                                                 float max_x,
                                                 float max_y,
                                                 float max_z) {
        for (int i = 0; i < 6; ++i) {
            const float nx = fr.p[i][0];
            const float ny = fr.p[i][1];
            const float nz = fr.p[i][2];
            const float px = (nx >= 0.0f) ? max_x : min_x;
            const float py = (ny >= 0.0f) ? max_y : min_y;
            const float pz = (nz >= 0.0f) ? max_z : min_z;
            if (nx * px + ny * py + nz * pz + fr.p[i][3] < 0.0f) {
                return false;
            }
        }
        return true;
    }

    void WorldRenderCache::apply_instance_bounds(DrawnProp& p) {
        if (!p.gpu || !p.gpu->has_bounds || p.cull_hx > 0.0f) {
            return;
        }
        const float xs[2] = {p.gpu->min_x, p.gpu->max_x};
        const float ys[2] = {p.gpu->min_y, p.gpu->max_y};
        const float zs[2] = {p.gpu->min_z, p.gpu->max_z};
        float min_x = 1e30f;
        float min_y = 1e30f;
        float min_z = 1e30f;
        float max_x = -1e30f;
        float max_y = -1e30f;
        float max_z = -1e30f;
        for (int i = 0; i < 2; ++i) {
            for (int j = 0; j < 2; ++j) {
                for (int k = 0; k < 2; ++k) {
                    const auto w = engine::Mat4::transform_point(p.model, xs[i], ys[j], zs[k]);
                    min_x = std::min(min_x, w.x);
                    min_y = std::min(min_y, w.y);
                    min_z = std::min(min_z, w.z);
                    max_x = std::max(max_x, w.x);
                    max_y = std::max(max_y, w.y);
                    max_z = std::max(max_z, w.z);
                }
            }
        }
        const float hx = 0.5f * (max_x - min_x);
        const float hy = 0.5f * (max_y - min_y);
        const float hz = 0.5f * (max_z - min_z);
        if (hx + hy + hz < 1.0f) {
            return;
        }
        p.wx = 0.5f * (min_x + max_x);
        p.wy = 0.5f * (min_y + max_y);
        p.wz = 0.5f * (min_z + max_z);
        p.cull_hx = hx;
        p.cull_hy = hy;
        p.cull_hz = hz;
    }

    float WorldRenderCache::point_segment_distance_sq(
        float px, float py, float ax, float ay, float bx, float by) {
        const float dx = bx - ax;
        const float dy = by - ay;
        const float len_sq = dx * dx + dy * dy;
        float t = 0.0f;
        if (len_sq > 1e-6f) {
            t = std::clamp(((px - ax) * dx + (py - ay) * dy) / len_sq, 0.0f, 1.0f);
        }
        const float ex = ax + dx * t - px;
        const float ey = ay + dy * t - py;
        return ex * ex + ey * ey;
    }

    std::string WorldRenderCache::patch_path(const std::string& maps_dir, int x, int y) {
        std::array<char, 64> name{};
        std::snprintf(name.data(), name.size(), "n_%03d_%03d.kcm", x, y);
        return platform::join_path(maps_dir, name.data());
    }

    engine::TextureHandle
    get_or_upload_detail(engine::IEngine& engine,
                         std::unordered_map<std::string, engine::TextureHandle>& cache,
                         const terrain::ImageRgba& detail,
                         const std::string& key) {
        auto it = cache.find(key);
        if (it != cache.end()) {
            return it->second;
        }
        engine::TextureHandle tex =
            engine.create_texture_rgba8(detail.rgba.data(), detail.width, detail.height, true);
        cache.emplace(key, tex);
        return tex;
    }

    void WorldRenderCache::sample_prop_uv(
        const GpuSubmesh& part, float time_sec, float& uv_u, float& uv_v, float& opacity) {
        uv_u = 0.0f;
        uv_v = 0.0f;
        opacity = 1.0f;
        const float time_ms = time_sec * 1000.0f;
        if (terrain::sample_material_uv(
                part.uv_material, part.uv_key_times, time_ms, uv_u, uv_v, opacity)) {
            return;
        }
        if (part.procedural_waterfall_scroll) {
            uv_v = -time_sec * 0.35f;
            return;
        }
        if (part.procedural_water_scroll) {
            uv_u = time_sec * 0.03f;
            uv_v = time_sec * 0.02f;
        }
    }

    void WorldRenderCache::draw_prop_part(engine::IEngine& engine,
                                          const GpuSubmesh& part,
                                          const engine::Mat4& model,
                                          bool alpha_blend,
                                          bool alpha_test,
                                          float uv_u,
                                          float uv_v,
                                          float opacity) {
        if (part.lightmap) {
            engine.draw_mesh_lightmapped(part.mesh,
                                         part.texture,
                                         part.lightmap,
                                         model,
                                         alpha_blend,
                                         part.two_sided,
                                         alpha_test,
                                         uv_u,
                                         uv_v,
                                         1.0f,
                                         1.0f,
                                         1.0f,
                                         opacity);
            return;
        }
        engine.draw_mesh_textured(part.mesh,
                                  part.texture,
                                  model,
                                  alpha_blend,
                                  part.two_sided,
                                  alpha_test,
                                  false,
                                  uv_u,
                                  uv_v,
                                  1.0f,
                                  1.0f,
                                  1.0f,
                                  opacity);
    }

    std::string WorldRenderCache::opl_name_to_gb_path(const std::string& data_root,
                                                      const std::string& opl_name) {
        // Client stores "DATA\OBJECTS\...", "Data\OBJECTS\...", "data\OBJECTS\...".
        // All must resolve under data_root/OBJECTS/... (never data_root/Data/...).
        return platform::resolve_client_asset(data_root, opl_name, ".gb");
    }

    const GpuModel* WorldRenderCache::get_or_load_gb(
        engine::IEngine& engine,
        const std::string& gb_path,
        std::unordered_map<std::string, GpuModel>& model_cache,
        std::unordered_map<std::string, engine::TextureHandle>& tex_cache) {
        auto it = model_cache.find(gb_path);
        if (it != model_cache.end()) {
            return it->second.parts.empty() ? nullptr : &it->second;
        }
        terrain::GbModel cpu;
        if (!terrain::load_gb(gb_path, cpu) || cpu.meshes.empty()) {
            model_cache.emplace(gb_path, GpuModel{});
            return nullptr;
        }
        GpuModel gpu;
        gpu.min_x = gpu.min_y = gpu.min_z = 1e30f;
        gpu.max_x = gpu.max_y = gpu.max_z = -1e30f;
        const bool model_is_water = cpu.option.find("Water") != std::string::npos ||
                                    cpu.option.find("water") != std::string::npos;
        for (const auto& mesh : cpu.meshes) {
            GpuSubmesh part;
            part.alpha_blend = mesh.alpha_blend;
            part.alpha_test = mesh.alpha_test;
            part.two_sided = mesh.two_sided;
            const std::size_t vert_count = mesh.pos_uv.size() / 5;
            const bool has_lm = mesh.lightmap_uv.size() >= vert_count * 2 && vert_count > 0;
            if (has_lm) {
                std::vector<engine::VertexP3T2T2> verts;
                verts.reserve(vert_count);
                for (std::size_t i = 0, vi = 0; i + 4 < mesh.pos_uv.size(); i += 5, ++vi) {
                    const float x = mesh.pos_uv[i];
                    const float y = mesh.pos_uv[i + 1];
                    const float z = mesh.pos_uv[i + 2];
                    gpu.min_x = std::min(gpu.min_x, x);
                    gpu.min_y = std::min(gpu.min_y, y);
                    gpu.min_z = std::min(gpu.min_z, z);
                    gpu.max_x = std::max(gpu.max_x, x);
                    gpu.max_y = std::max(gpu.max_y, y);
                    gpu.max_z = std::max(gpu.max_z, z);
                    const std::size_t lo = vi * 2;
                    verts.push_back({x,
                                     y,
                                     z,
                                     mesh.pos_uv[i + 3],
                                     mesh.pos_uv[i + 4],
                                     mesh.lightmap_uv[lo],
                                     mesh.lightmap_uv[lo + 1]});
                }
                part.mesh = engine.create_mesh_lm(
                    verts.data(), verts.size(), mesh.indices.data(), mesh.indices.size());
            } else {
                std::vector<engine::VertexP3T2> verts;
                verts.reserve(vert_count);
                for (std::size_t i = 0; i + 4 < mesh.pos_uv.size(); i += 5) {
                    const float x = mesh.pos_uv[i];
                    const float y = mesh.pos_uv[i + 1];
                    const float z = mesh.pos_uv[i + 2];
                    gpu.min_x = std::min(gpu.min_x, x);
                    gpu.min_y = std::min(gpu.min_y, y);
                    gpu.min_z = std::min(gpu.min_z, z);
                    gpu.max_x = std::max(gpu.max_x, x);
                    gpu.max_y = std::max(gpu.max_y, y);
                    gpu.max_z = std::max(gpu.max_z, z);
                    verts.push_back({x, y, z, mesh.pos_uv[i + 3], mesh.pos_uv[i + 4]});
                }
                part.mesh = engine.create_mesh(
                    verts.data(), verts.size(), mesh.indices.data(), mesh.indices.size());
            }

            std::string tex_name;
            if (mesh.material_index >= 0 &&
                mesh.material_index < static_cast<int>(cpu.materials.size())) {
                tex_name = cpu.materials[mesh.material_index].texture;
                part.uv_material = cpu.materials[mesh.material_index];
                part.uv_key_times = cpu.material_key_times;
            }
            if (terrain::is_waterfall_texture(tex_name)) {
                part.procedural_waterfall_scroll = part.uv_key_times.size() < 2;
            } else if (terrain::is_water_surface_texture(tex_name) || model_is_water) {
                part.procedural_water_scroll = part.uv_key_times.size() < 2;
            }
            const std::string tex_key = gb_path + "|" + tex_name;
            auto tit = tex_cache.find(tex_key);
            if (tit != tex_cache.end()) {
                part.texture = tit->second;
            } else {
                terrain::ImageRgba img;
                if (!terrain::load_gb_texture(gb_path, tex_name, img) || img.rgba.empty()) {
                    img.width = img.height = 1;
                    img.rgba = {180, 160, 140, 255};
                }
                part.texture =
                    engine.create_texture_rgba8(img.rgba.data(), img.width, img.height, true);
                tex_cache.emplace(tex_key, part.texture);
            }
            if (has_lm) {
                const std::string lm_name = mesh.lightmap;
                const std::string lm_key = gb_path + "|lm|" + lm_name;
                auto lit = tex_cache.find(lm_key);
                if (lit != tex_cache.end()) {
                    part.lightmap = lit->second;
                } else {
                    terrain::ImageRgba lm_img;
                    if (!lm_name.empty() && terrain::load_gb_texture(gb_path, lm_name, lm_img) &&
                        !lm_img.rgba.empty()) {
                        part.lightmap = engine.create_texture_rgba8(
                            lm_img.rgba.data(), lm_img.width, lm_img.height, true);
                    } else {
                        const std::uint8_t white[4] = {255, 255, 255, 255};
                        part.lightmap = engine.create_texture_rgba8(white, 1, 1, true);
                    }
                    tex_cache.emplace(lm_key, part.lightmap);
                }
            }
            gpu.parts.push_back(part);
        }
        const float extent =
            (gpu.max_x - gpu.min_x) + (gpu.max_y - gpu.min_y) + (gpu.max_z - gpu.min_z);
        gpu.has_bounds = extent > 1.0f;
        auto [ins, ok] = model_cache.emplace(gb_path, std::move(gpu));
        return ok && !ins->second.parts.empty() ? &ins->second : nullptr;
    }

    GpuModel merge_clothes_gbs(engine::IEngine& engine,
                               const std::vector<std::string>& paths,
                               std::unordered_map<std::string, engine::TextureHandle>& tex_cache,
                               const terrain::GbModel* skeleton = nullptr,
                               const terrain::GbModel* motion = nullptr) {
        GpuModel gpu;
        gpu.min_x = gpu.min_y = gpu.min_z = 1e30f;
        gpu.max_x = gpu.max_y = gpu.max_z = -1e30f;
        for (const auto& gb_path : paths) {
            terrain::GbModel cpu;
            if (!terrain::load_gb(gb_path, cpu) || cpu.meshes.empty()) {
                continue;
            }
            if (skeleton && motion) {
                if (!terrain::skin_with_skeleton_anim(cpu, *skeleton, *motion)) {
                    log::warn("WorldRenderCache: skin_with_skeleton_anim failed for " + gb_path);
                }
            }
            for (const auto& mesh : cpu.meshes) {
                GpuSubmesh part;
                part.alpha_blend = mesh.alpha_blend;
                part.alpha_test = mesh.alpha_test;
                part.two_sided = mesh.two_sided;
                std::vector<engine::VertexP3T2> verts;
                verts.reserve(mesh.pos_uv.size() / 5);
                for (std::size_t i = 0; i + 4 < mesh.pos_uv.size(); i += 5) {
                    const float x = mesh.pos_uv[i];
                    const float y = mesh.pos_uv[i + 1];
                    const float z = mesh.pos_uv[i + 2];
                    gpu.min_x = std::min(gpu.min_x, x);
                    gpu.min_y = std::min(gpu.min_y, y);
                    gpu.min_z = std::min(gpu.min_z, z);
                    gpu.max_x = std::max(gpu.max_x, x);
                    gpu.max_y = std::max(gpu.max_y, y);
                    gpu.max_z = std::max(gpu.max_z, z);
                    verts.push_back({x, y, z, mesh.pos_uv[i + 3], mesh.pos_uv[i + 4]});
                }
                part.mesh = engine.create_mesh(
                    verts.data(), verts.size(), mesh.indices.data(), mesh.indices.size());
                std::string tex_name;
                if (mesh.material_index >= 0 &&
                    mesh.material_index < static_cast<int>(cpu.materials.size())) {
                    tex_name = cpu.materials[mesh.material_index].texture;
                }
                const std::string tex_key = gb_path + "|" + tex_name;
                auto tit = tex_cache.find(tex_key);
                if (tit != tex_cache.end()) {
                    part.texture = tit->second;
                } else {
                    terrain::ImageRgba img;
                    if (!terrain::load_gb_texture(gb_path, tex_name, img) || img.rgba.empty()) {
                        img.width = img.height = 1;
                        img.rgba = {180, 160, 140, 255};
                    }
                    part.texture =
                        engine.create_texture_rgba8(img.rgba.data(), img.width, img.height, true);
                    tex_cache.emplace(tex_key, part.texture);
                }
                gpu.parts.push_back(part);
            }
        }
        const float extent =
            (gpu.max_x - gpu.min_x) + (gpu.max_y - gpu.min_y) + (gpu.max_z - gpu.min_z);
        gpu.has_bounds = extent > 1.0f && gpu.max_x > gpu.min_x;
        return gpu;
    }

    const GpuModel* WorldRenderCache::get_or_load_npc_shape(
        engine::IEngine& engine,
        const std::string& data_root,
        int shape,
        std::unordered_map<std::string, GpuModel>& model_cache,
        std::unordered_map<std::string, engine::TextureHandle>& tex_cache,
        const NpcCatalog& npc_catalog) {
        const std::string key = "npc:" + std::to_string(shape);
        auto it = model_cache.find(key);
        if (it != model_cache.end()) {
            return it->second.parts.empty() ? nullptr : &it->second;
        }
        const auto paths = NpcCatalog::clothes_gb_paths(data_root, shape);
        if (paths.empty()) {
            model_cache.emplace(key, GpuModel{});
            return nullptr;
        }
        terrain::GbModel skeleton;
        terrain::GbModel motion;
        const terrain::GbModel* skel_ptr = nullptr;
        const terrain::GbModel* mot_ptr = nullptr;
        if (const auto* rig = npc_catalog.find_rig(shape)) {
            const std::string bone_path = NpcCatalog::bone_gb(data_root, rig->bone_no());
            const std::string idle_path =
                NpcCatalog::idle_gb(data_root, rig->bone_no(), rig->idle_letter());
            if (!bone_path.empty() && !idle_path.empty() && terrain::load_gb(bone_path, skeleton) &&
                terrain::load_gb(idle_path, motion)) {
                skel_ptr = &skeleton;
                mot_ptr = &motion;
            }
        }
        GpuModel gpu = merge_clothes_gbs(engine, paths, tex_cache, skel_ptr, mot_ptr);
        if (skel_ptr && mot_ptr && !mot_ptr->anims.empty()) {
            int head = -1;
            if (const auto* rig = npc_catalog.find_rig(shape)) {
                if (const auto* bone = npc_catalog.find_bone(rig->bone_no())) {
                    head = bone->head();
                }
            }
            if (head >= 0) {
                std::vector<std::array<float, 16>> bone_world;
                std::vector<std::array<float, 16>> skin;
                if (terrain::eval_bone_and_skin_matrices_at_time(
                        skel_ptr->bones, mot_ptr->anims[0], 0.0f, true, bone_world, skin) &&
                    head < static_cast<int>(bone_world.size())) {
                    gpu.name_local_y = bone_world[static_cast<std::size_t>(head)][13];
                    gpu.has_name_anchor = true;
                }
            }
        }
        auto [ins, ok] = model_cache.emplace(key, std::move(gpu));
        return ok && !ins->second.parts.empty() ? &ins->second : nullptr;
    }

    const GpuModel* WorldRenderCache::get_or_load_player_class(
        engine::IEngine& engine,
        const std::string& data_root,
        Class clazz,
        std::unordered_map<std::string, GpuModel>& model_cache,
        std::unordered_map<std::string, engine::TextureHandle>& tex_cache) {
        const std::string key = std::string("pc:") + LocalPlayer::class_name(clazz);
        auto it = model_cache.find(key);
        if (it != model_cache.end()) {
            return it->second.parts.empty() ? nullptr : &it->second;
        }
        const auto paths = LocalPlayer::default_clothes_paths(data_root, clazz);
        if (paths.empty()) {
            model_cache.emplace(key, GpuModel{});
            return nullptr;
        }
        const char job = LocalPlayer::job_letter(clazz);
        std::array<char, 64> bone_name{};
        std::array<char, 64> idle_name{};
        std::snprintf(bone_name.data(), bone_name.size(), "T%c_bone.gb", job);
        std::snprintf(idle_name.data(), idle_name.size(), "T%c_0_A_01.gb", job);
        const std::string motion_dir = platform::join_path(data_root, "Model/Motion");
        std::string bone_path = platform::join_path(motion_dir, bone_name.data());
        if (!platform::file_exists(bone_path)) {
            std::snprintf(bone_name.data(),
                          bone_name.size(),
                          "T%c_Bone.gb",
                          static_cast<char>(std::toupper(static_cast<unsigned char>(job))));
            bone_path = platform::join_path(motion_dir, bone_name.data());
        }
        std::string idle_path = platform::join_path(motion_dir, idle_name.data());
        if (!platform::file_exists(idle_path)) {
            std::snprintf(idle_name.data(), idle_name.size(), "T%c_0_01.gb", job);
            idle_path = platform::join_path(motion_dir, idle_name.data());
        }
        terrain::GbModel skeleton;
        terrain::GbModel motion;
        const terrain::GbModel* skel_ptr = nullptr;
        const terrain::GbModel* mot_ptr = nullptr;
        if (platform::file_exists(bone_path) && platform::file_exists(idle_path) &&
            terrain::load_gb(bone_path, skeleton) && terrain::load_gb(idle_path, motion)) {
            skel_ptr = &skeleton;
            mot_ptr = &motion;
        }
        GpuModel gpu = merge_clothes_gbs(engine, paths, tex_cache, skel_ptr, mot_ptr);
        auto [ins, ok] = model_cache.emplace(key, std::move(gpu));
        return ok && !ins->second.parts.empty() ? &ins->second : nullptr;
    }

    std::optional<WorldRenderCache::PreparedPatch>
    WorldRenderCache::prepare_patch(terrain::HeightPatch patch,
                                    const std::string& tex_dir,
                                    const std::vector<terrain::LayerInfo>& env_layers) {
        PreparedPatch prepared;
        prepared.patch = std::move(patch);
        if (!prepared.patch.prepare_layers(tex_dir, env_layers, prepared.layers)) {
            return std::nullopt;
        }
        prepared.patch.build_mesh(prepared.vertices, prepared.indices);
        if (prepared.vertices.empty() || prepared.indices.empty()) {
            return std::nullopt;
        }
        prepared.detail_keys.reserve(prepared.patch.layers.size());
        for (const unsigned index : prepared.patch.layers) {
            prepared.detail_keys.push_back(
                index < env_layers.size() ? env_layers[index].texture_name : "missing");
        }
        return prepared;
    }

    DrawnPatch WorldRenderCache::upload_prepared_patch(
        engine::IEngine& engine,
        const PreparedPatch& patch,
        std::unordered_map<std::string, engine::TextureHandle>& detail_cache) {
        DrawnPatch drawn;
        drawn.patch_x = patch.patch.patch_x;
        drawn.patch_y = patch.patch.patch_y;
        drawn.deco_palette = patch.patch.deco_palette;
        drawn.deco_masks = patch.patch.deco_masks;
        drawn.heights = patch.patch.heights;
        constexpr std::size_t kHeightCount =
            static_cast<std::size_t>(map::kHeightSamples) * map::kHeightSamples;
        if (patch.patch.heights.size() == kHeightCount) {
            std::vector<float> shadow_heights;
            shadow_heights.reserve(kHeightCount);
            std::transform(patch.patch.heights.begin(),
                           patch.patch.heights.end(),
                           std::back_inserter(shadow_heights),
                           [](std::uint16_t height) { return static_cast<float>(height); });
            drawn.shadow_height_map = engine.create_texture_r32f(
                shadow_heights.data(), map::kHeightSamples, map::kHeightSamples);
        }
        std::vector<engine::VertexP3T2> gpu;
        gpu.reserve(patch.vertices.size());
        for (const auto& v : patch.vertices) {
            gpu.push_back({v.position.x, v.position.y, v.position.z, v.u, v.v});
        }
        drawn.mesh =
            engine.create_mesh(gpu.data(), gpu.size(), patch.indices.data(), patch.indices.size());

        // Layer 0 reuses layer 1's color splat (client: runtime state[0] =
        // runtime state[1]).
        engine::TextureHandle shared_base_color{};
        for (std::size_t i = 0; i < patch.layers.size(); ++i) {
            GpuLayer gl;
            gl.repeat = patch.layers[i].repeat;
            gl.alpha_blend = patch.layers[i].alpha_blend;

            if (i == 0 && patch.layers.size() >= 2) {
                // Defer until layer 1 uploads, or upload once here from layers[0].color
                // (prepare_layers already copied the same pixels into layer 0).
                gl.color = engine.create_texture_rgba8(patch.layers[i].color.rgba.data(),
                                                       patch.layers[i].color.width,
                                                       patch.layers[i].color.height,
                                                       false);
                shared_base_color = gl.color;
            } else if (i == 1 && shared_base_color) {
                // Reuse GPU texture for identical RGB+A of layer 1? Layer 1 has its own
                // alpha ? prepare_layers: layer0 color == color_for_upper (layer1
                // alphas), layer1 same. So they are identical ? reuse.
                gl.color = shared_base_color;
            } else {
                gl.color = engine.create_texture_rgba8(patch.layers[i].color.rgba.data(),
                                                       patch.layers[i].color.width,
                                                       patch.layers[i].color.height,
                                                       false);
            }

            const std::string& detail_key =
                i < patch.detail_keys.size() ? patch.detail_keys[i] : std::string{"missing"};
            gl.detail =
                get_or_upload_detail(engine, detail_cache, patch.layers[i].detail, detail_key);
            gl.owns_detail = false;
            drawn.layers.push_back(gl);
        }
        return drawn;
    }

    DrawnPatch WorldRenderCache::upload_patch(
        engine::IEngine& engine,
        terrain::HeightPatch& patch,
        const std::string& tex_dir,
        const std::vector<terrain::LayerInfo>& env_layers,
        std::unordered_map<std::string, engine::TextureHandle>& detail_cache) {
        auto prepared = prepare_patch(patch, tex_dir, env_layers);
        if (!prepared) {
            return {};
        }
        return upload_prepared_patch(engine, *prepared, detail_cache);
    }

}  // namespace openkal::client
