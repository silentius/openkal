/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "bake.h"

#include "openkal/map/Constants.h"
#include "openkal/platform/File.h"
#include "openkal/platform/Log.h"
#include "openkal/platform/Path.h"
#include "openkal/terrain/EnvLoader.h"
#include "openkal/terrain/GbModel.h"
#include "openkal/terrain/HeightPatch.h"
#include "openkal/terrain/OplLoader.h"

#include <cctype>
#include <cstdio>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace openkal::area_editor {
    namespace {

        struct GpuSubmesh {
            engine::MeshHandle mesh;
            engine::TextureHandle texture;
            bool alpha_blend = false;
            bool alpha_test = false;
            bool two_sided = false;
        };

        struct GpuModel {
            std::vector<GpuSubmesh> parts;
            std::vector<ObjectMaskMesh> mask_meshes;
        };

        struct GpuLayer {
            engine::TextureHandle color;
            engine::TextureHandle detail;
            float repeat = 1.0f;
            bool alpha_blend = false;
        };

        struct DrawnPatch {
            engine::MeshHandle mesh;
            std::vector<GpuLayer> layers;
        };

        struct DrawnProp {
            engine::Mat4 model;
            const GpuModel* gpu = nullptr;
            bool foliage_asset = false;
        };

        void destroy_bake_resources(
            engine::IEngine& engine,
            const DrawnPatch& patch,
            const std::unordered_map<std::string, GpuModel>& models,
            const std::unordered_map<std::string, engine::TextureHandle>& detail_textures,
            const std::unordered_map<std::string, engine::TextureHandle>& prop_textures) {
            std::unordered_set<std::uint32_t> destroyed_textures;
            const auto destroy_texture = [&](engine::TextureHandle texture) {
                if (texture && destroyed_textures.insert(texture.id).second) {
                    engine.destroy_texture(texture);
                }
            };
            if (patch.mesh) {
                engine.destroy_mesh(patch.mesh);
            }
            for (const auto& layer : patch.layers) {
                destroy_texture(layer.color);
                destroy_texture(layer.detail);
            }
            for (const auto& [path, model] : models) {
                (void) path;
                for (const auto& part : model.parts) {
                    if (part.mesh) {
                        engine.destroy_mesh(part.mesh);
                    }
                    destroy_texture(part.texture);
                }
            }
            for (const auto& [key, texture] : detail_textures) {
                (void) key;
                destroy_texture(texture);
            }
            for (const auto& [key, texture] : prop_textures) {
                (void) key;
                destroy_texture(texture);
            }
        }

        [[nodiscard]] bool is_foliage_asset_path(std::string_view path) {
            std::string lower;
            lower.reserve(path.size());
            for (const unsigned char character : path) {
                lower.push_back(static_cast<char>(std::tolower(character)));
            }
            constexpr std::string_view keywords[] = {"tree", "bush", "leaf", "wood"};
            for (const std::string_view keyword : keywords) {
                if (lower.find(keyword) != std::string::npos) {
                    return true;
                }
            }
            return false;
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

        DrawnPatch
        upload_patch(engine::IEngine& engine,
                     terrain::HeightPatch& patch,
                     const std::string& tex_dir,
                     const std::vector<terrain::LayerInfo>& env_layers,
                     std::unordered_map<std::string, engine::TextureHandle>& detail_cache) {
            DrawnPatch drawn;
            std::vector<terrain::TerrainLayerGpu> layers;
            if (!patch.prepare_layers(tex_dir, env_layers, layers)) {
                return drawn;
            }

            std::vector<terrain::Vertex> verts;
            std::vector<std::uint32_t> indices;
            patch.build_mesh(verts, indices);
            std::vector<engine::VertexP3T2> gpu;
            gpu.reserve(verts.size());
            for (const auto& v : verts) {
                gpu.push_back({v.position.x, v.position.y, v.position.z, v.u, v.v});
            }
            drawn.mesh = engine.create_mesh(gpu.data(), gpu.size(), indices.data(), indices.size());

            engine::TextureHandle shared_base_color{};
            for (std::size_t i = 0; i < layers.size(); ++i) {
                GpuLayer gl;
                gl.repeat = layers[i].repeat;
                gl.alpha_blend = layers[i].alpha_blend;

                if (i == 0 && layers.size() >= 2) {
                    gl.color = engine.create_texture_rgba8(layers[i].color.rgba.data(),
                                                           layers[i].color.width,
                                                           layers[i].color.height,
                                                           false);
                    shared_base_color = gl.color;
                } else if (i == 1 && shared_base_color) {
                    gl.color = shared_base_color;
                } else {
                    gl.color = engine.create_texture_rgba8(layers[i].color.rgba.data(),
                                                           layers[i].color.width,
                                                           layers[i].color.height,
                                                           false);
                }

                std::string detail_key = "missing";
                const unsigned idx = (i < patch.layers.size()) ? patch.layers[i] : 0;
                if (idx < env_layers.size()) {
                    detail_key = env_layers[idx].texture_name;
                }
                gl.detail =
                    get_or_upload_detail(engine, detail_cache, layers[i].detail, detail_key);
                drawn.layers.push_back(gl);
            }
            return drawn;
        }

        const GpuModel*
        get_or_load_gb(engine::IEngine& engine,
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
            for (const auto& mesh : cpu.meshes) {
                GpuSubmesh part;
                part.alpha_blend = mesh.alpha_blend;
                part.alpha_test = mesh.alpha_test;
                part.two_sided = mesh.two_sided;
                std::vector<engine::VertexP3T2> verts;
                ObjectMaskMesh mask_mesh;
                mask_mesh.alpha_surface = mesh.alpha_test || mesh.alpha_blend;
                verts.reserve(mesh.pos_uv.size() / 5);
                mask_mesh.positions.reserve(mesh.pos_uv.size() / 5);
                for (std::size_t i = 0; i + 4 < mesh.pos_uv.size(); i += 5) {
                    verts.push_back({mesh.pos_uv[i],
                                     mesh.pos_uv[i + 1],
                                     mesh.pos_uv[i + 2],
                                     mesh.pos_uv[i + 3],
                                     mesh.pos_uv[i + 4]});
                    mask_mesh.positions.emplace_back(
                        mesh.pos_uv[i], mesh.pos_uv[i + 1], mesh.pos_uv[i + 2]);
                }
                mask_mesh.indices = mesh.indices;
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
                gpu.mask_meshes.push_back(std::move(mask_mesh));
            }
            auto [ins, ok] = model_cache.emplace(gb_path, std::move(gpu));
            return ok && !ins->second.parts.empty() ? &ins->second : nullptr;
        }

        std::string dirname_of(const std::string& path) {
            const auto slash = path.find_last_of("/\\");
            if (slash == std::string::npos) {
                return ".";
            }
            if (slash == 0) {
                return "/";
            }
            return path.substr(0, slash);
        }

        std::string find_existing(const std::vector<std::string>& candidates) {
            for (const auto& c : candidates) {
                if (platform::file_exists(c)) {
                    return c;
                }
            }
            return {};
        }

        void draw_patch_water(engine::IEngine& engine,
                              const std::string& data_root,
                              float origin_x,
                              float origin_z,
                              float extent,
                              const engine::Vec3& camera) {
            if (extent <= 0.0f) {
                return;
            }
            std::string water_path = data_root;
            for (const char* part : {"OBJECTS", "common", "tex", "water_001.dds"}) {
                water_path = platform::join_path(water_path, part);
            }
            terrain::ImageRgba image;
            if (!platform::file_exists(water_path) || !terrain::load_gtx_rgba(water_path, image) ||
                image.rgba.empty()) {
                image.width = 4;
                image.height = 4;
                image.rgba.assign(static_cast<std::size_t>(image.width * image.height * 4), 255);
                for (std::size_t pixel = 0; pixel < image.rgba.size(); pixel += 4) {
                    image.rgba[pixel + 0] = 36;
                    image.rgba[pixel + 1] = 112;
                    image.rgba[pixel + 2] = 168;
                }
                log::info("area_editor water texture missing, using a flat color");
            }
            const engine::TextureHandle texture =
                engine.create_texture_rgba8(image.rgba.data(), image.width, image.height, true);
            const float y = map::kWaterPosY;
            const float x1 = origin_x + extent;
            const float z1 = origin_z + extent;
            const float uv_scale = 1.0f / static_cast<float>(map::kTerrainCellSize * 4);
            const engine::VertexP3T2 vertices[] = {
                {origin_x, y, origin_z, origin_x * uv_scale, origin_z * uv_scale},
                {x1, y, origin_z, x1 * uv_scale, origin_z * uv_scale},
                {x1, y, z1, x1 * uv_scale, z1 * uv_scale},
                {origin_x, y, z1, origin_x * uv_scale, z1 * uv_scale},
            };
            const std::uint32_t indices[] = {0, 1, 2, 0, 2, 3};
            const engine::MeshHandle mesh = engine.create_mesh(vertices, 4, indices, 6);
            if (mesh && texture) {
                // Pull the plane toward the camera so a shoreline at the water height does not
                // flicker against the terrain.
                engine.set_polygon_offset(-1.0f, -2.0f);
                engine.draw_water_plane(mesh, texture, engine::Mat4::identity(), camera, 0.0f);
                engine.set_polygon_offset(0.0f, 0.0f);
            }
            if (mesh) {
                engine.destroy_mesh(mesh);
            }
            if (texture) {
                engine.destroy_texture(texture);
            }
        }

    }  // namespace

    BakePaths resolve_bake_paths(const std::string& area_path, const std::string& data_root_arg) {
        BakePaths out;
        const std::string area_dir = dirname_of(area_path);

        if (!data_root_arg.empty()) {
            out.data_root = data_root_arg;
        } else {
            // Heuristic: .../data/MAPS/foo.akarea ?? data root is parent of MAPS.
            const auto slash = area_dir.find_last_of("/\\");
            if (slash != std::string::npos) {
                const std::string leaf = area_dir.substr(slash + 1);
                std::string lower = leaf;
                for (char& c : lower) {
                    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                }
                if (lower == "maps") {
                    out.data_root = area_dir.substr(0, slash);
                }
            }
            if (out.data_root.empty()) {
                out.data_root = area_dir;
            }
        }

        const std::string maps_under_data = platform::join_path(out.data_root, "MAPS");
        if (platform::file_exists(platform::join_path(area_dir, "n.env")) ||
            platform::is_directory(platform::join_path(area_dir, "Tex"))) {
            out.maps_dir = area_dir;
        } else if (platform::is_directory(maps_under_data)) {
            out.maps_dir = maps_under_data;
        } else {
            out.maps_dir = area_dir;
        }
        return out;
    }

    BakeResult bake_patch_topdown(
        engine::IEngine& engine, const BakePaths& paths, int patch_x, int patch_y, int resolution) {
        BakeResult result;
        result.patch_x = patch_x;
        result.patch_y = patch_y;
        if (resolution <= 0) {
            result.error = "invalid bake resolution";
            return result;
        }

        char stem[64];
        std::snprintf(stem, sizeof(stem), "n_%03d_%03d", patch_x, patch_y);

        const std::string kcm_path = find_existing({
            platform::join_path(paths.maps_dir, std::string(stem) + ".kcm"),
            platform::join_path(paths.maps_dir, std::string(stem) + ".KCM"),
        });
        const std::string opl_path = find_existing({
            platform::join_path(paths.maps_dir, std::string(stem) + ".opl"),
            platform::join_path(paths.maps_dir, std::string(stem) + ".OPL"),
        });
        const std::string env_path = find_existing({
            platform::join_path(paths.maps_dir, "n.env"),
            platform::join_path(paths.maps_dir, "N.ENV"),
        });
        const std::string tex_dir = platform::join_path(paths.maps_dir, "Tex");

        if (kcm_path.empty()) {
            result.error = "missing KCM for " + std::string(stem) + " in " + paths.maps_dir;
            return result;
        }

        std::vector<terrain::LayerInfo> env_layers;
        if (!env_path.empty()) {
            (void) terrain::load_env_layers(env_path, env_layers);
        }

        terrain::HeightPatch patch;
        if (!terrain::load_kcm(kcm_path, patch)) {
            result.error = "failed to load " + kcm_path;
            return result;
        }
        patch.patch_x = patch_x;
        patch.patch_y = patch_y;

        std::unordered_map<std::string, engine::TextureHandle> detail_cache;
        std::unordered_map<std::string, GpuModel> model_cache;
        std::unordered_map<std::string, engine::TextureHandle> prop_tex_cache;
        DrawnPatch drawn = upload_patch(engine, patch, tex_dir, env_layers, detail_cache);
        if (!drawn.mesh || drawn.layers.empty()) {
            destroy_bake_resources(engine, drawn, model_cache, detail_cache, prop_tex_cache);
            result.error = "failed to upload terrain layers for " + kcm_path;
            return result;
        }

        const float extent = static_cast<float>(map::kMapSize * map::kTerrainCellSize);  // 8192
        const float x0 = static_cast<float>(patch_x - map::kPatchGrid / 2) * extent;
        const float z0 = static_cast<float>(patch_y - map::kPatchGrid / 2) * extent;

        std::vector<DrawnProp> props;
        if (!opl_path.empty()) {
            terrain::OplFile opl;
            if (terrain::load_opl(opl_path, opl)) {
                for (const auto& inst : opl.instances) {
                    if (inst.name.empty()) {
                        ++result.missing_gb;
                        continue;
                    }
                    const std::string gb_path =
                        platform::resolve_client_asset(paths.data_root, inst.name, ".gb");
                    if (gb_path.empty() || !platform::file_exists(gb_path)) {
                        ++result.missing_gb;
                        continue;
                    }
                    const GpuModel* gpu =
                        get_or_load_gb(engine, gb_path, model_cache, prop_tex_cache);
                    if (!gpu) {
                        ++result.missing_gb;
                        continue;
                    }
                    DrawnProp p;
                    p.gpu = gpu;
                    float wx = 0, wy = 0, wz = 0;
                    terrain::opl_world_translation(inst, patch_x, patch_y, wx, wy, wz);
                    const auto S = engine::Mat4::scaling(inst.sx, inst.sy, inst.sz);
                    const auto R =
                        engine::Mat4::from_quaternion(inst.qx, inst.qy, inst.qz, inst.qw);
                    const auto T = engine::Mat4::translation(wx, wy, wz);
                    p.model = engine::Mat4::multiply(engine::Mat4::multiply(T, R), S);
                    p.foliage_asset = is_foliage_asset_path(inst.name);
                    ObjectMask mask = build_object_mask(
                        inst.name, gpu->mask_meshes, p.model, x0, z0, extent, wy, p.foliage_asset);
                    if (!mask.display_contours.empty() || !mask.footprint_cells.empty()) {
                        result.object_masks.push_back(std::move(mask));
                    }
                    props.push_back(p);
                    ++result.prop_count;
                }
            }
        }

        const float cx = x0 + extent * 0.5f;
        const float cz = z0 + extent * 0.5f;
        const float eye_y = 80000.0f;

        // Match client D3D??GL X flip. Look down -Y with up = -Z so image +Y follows
        // +world Z.
        const auto view =
            engine::Mat4::multiply(engine::Mat4::scaling(-1.0f, 1.0f, 1.0f),
                                   engine::Mat4::look_at(engine::Vec3{cx, eye_y, cz},
                                                         engine::Vec3{cx, 0.0f, cz},
                                                         engine::Vec3{0.0f, 0.0f, -1.0f}));
        // After look_at: camera X ? world +X, camera Y ? world -Z.
        // With X flip: screen +X = world -X. Use ortho half-extent 4096.
        const float half = extent * 0.5f;
        const auto proj = engine::Mat4::orthographic(-half, half, -half, half, 1.0f, 200000.0f);

        if (!engine.begin_offscreen(resolution, resolution, 0.45f, 0.62f, 0.85f)) {
            destroy_bake_resources(engine, drawn, model_cache, detail_cache, prop_tex_cache);
            result.error = "begin_offscreen failed";
            return result;
        }
        engine.set_camera(view, proj);

        if (!drawn.layers.empty()) {
            engine::TerrainLayerDraw layer;
            layer.color = drawn.layers[0].color;
            layer.detail = drawn.layers[0].detail;
            layer.repeat = drawn.layers[0].repeat;
            layer.alpha_blend = false;
            engine.draw_terrain_layer(drawn.mesh, layer);
        }
        for (std::size_t i = 1; i < drawn.layers.size(); ++i) {
            engine::TerrainLayerDraw layer;
            layer.color = drawn.layers[i].color;
            layer.detail = drawn.layers[i].detail;
            layer.repeat = drawn.layers[i].repeat;
            layer.alpha_blend = true;
            engine.draw_terrain_layer(drawn.mesh, layer);
        }

        for (int pass = 0; pass < 2; ++pass) {
            const bool want_alpha = pass == 1;
            for (const auto& p : props) {
                if (!p.gpu) {
                    continue;
                }
                for (const auto& part : p.gpu->parts) {
                    // Tree foliage uses alpha-tested or blended surfaces. Leaving the opaque
                    // trunk visible makes safe-zone painting around trees practical.
                    if (p.foliage_asset && (part.alpha_test || part.alpha_blend)) {
                        continue;
                    }
                    if (part.alpha_blend != want_alpha) {
                        continue;
                    }
                    engine.draw_mesh_textured(part.mesh,
                                              part.texture,
                                              p.model,
                                              part.alpha_blend,
                                              part.two_sided,
                                              part.alpha_test && !part.alpha_blend);
                }
            }
        }

        // Depth test keeps the sea off high ground and shows the coastline.
        draw_patch_water(engine, paths.data_root, x0, z0, extent, engine::Vec3{cx, eye_y, cz});

        std::vector<std::uint8_t> rgba;
        if (!engine.end_offscreen_rgba(rgba)) {
            destroy_bake_resources(engine, drawn, model_cache, detail_cache, prop_tex_cache);
            result.error = "end_offscreen_rgba failed";
            return result;
        }

        // X-flip in the bake undoes the view flip so tile (0,0) is top-left and +X
        // goes right.
        result.image.width = resolution;
        result.image.height = resolution;
        result.image.rgba.resize(rgba.size());
        const std::size_t row = static_cast<std::size_t>(resolution) * 4;
        for (int y = 0; y < resolution; ++y) {
            for (int x = 0; x < resolution; ++x) {
                const int sx = resolution - 1 - x;
                const std::size_t src =
                    static_cast<std::size_t>(y) * row + static_cast<std::size_t>(sx) * 4;
                const std::size_t dst =
                    static_cast<std::size_t>(y) * row + static_cast<std::size_t>(x) * 4;
                result.image.rgba[dst + 0] = rgba[src + 0];
                result.image.rgba[dst + 1] = rgba[src + 1];
                result.image.rgba[dst + 2] = rgba[src + 2];
                result.image.rgba[dst + 3] = rgba[src + 3];
            }
        }

        destroy_bake_resources(engine, drawn, model_cache, detail_cache, prop_tex_cache);

        log::info("area_editor bake " + std::string(stem) +
                  ": props=" + std::to_string(result.prop_count) +
                  " missing_gb=" + std::to_string(result.missing_gb));
        return result;
    }

}  // namespace openkal::area_editor
