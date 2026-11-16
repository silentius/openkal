/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "TileView.h"

#include "TileExporter.h"
#include "openkal/client/WidgetTools.h"
#include "openkal/engine/engine.h"
#include "openkal/map/Constants.h"
#include "openkal/platform/File.h"
#include "openkal/platform/Log.h"
#include "openkal/platform/Path.h"
#include "openkal/terrain/EnvLoader.h"
#include "openkal/terrain/GbModel.h"
#include "openkal/terrain/HeightPatch.h"
#include "openkal/terrain/OplLoader.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace openkal::tile_view {
    namespace {

        constexpr float kPi = 3.14159265f;
        constexpr float kFovRad = 60.0f * kPi / 180.0f;
        constexpr float kLookSensitivity = 0.0024f;
        constexpr float kWalkSpeed = 1800.0f;
        constexpr float kSprintSpeed = 7000.0f;
        constexpr float kVerticalSpeed = 1400.0f;

        std::string find_existing(const std::vector<std::string>& candidates) {
            for (const auto& c : candidates) {
                if (platform::file_exists(c)) {
                    return c;
                }
            }
            return {};
        }

        std::string patch_stem(int patch_x, int patch_y) {
            char stem[32];
            std::snprintf(stem, sizeof(stem), "n_%03d_%03d", patch_x, patch_y);
            return stem;
        }

        std::string
        find_maps_file(const std::string& maps_dir, int patch_x, int patch_y, const char* ext) {
            char n[32];
            char m[32];
            std::snprintf(n, sizeof(n), "n_%03d_%03d", patch_x, patch_y);
            std::snprintf(m, sizeof(m), "m_%03d_%03d", patch_x, patch_y);
            std::string ext_upper = ext;
            for (char& c : ext_upper) {
                c = static_cast<char>(static_cast<unsigned char>(c) >= 'a' &&
                                              static_cast<unsigned char>(c) <= 'z'
                                          ? c - 32
                                          : c);
            }
            return find_existing({
                platform::join_path(maps_dir, std::string(n) + ext),
                platform::join_path(maps_dir, std::string(n) + ext_upper),
                platform::join_path(maps_dir, std::string(m) + ext),
                platform::join_path(maps_dir, std::string(m) + ext_upper),
            });
        }

        struct GpuSubmesh {
            engine::MeshHandle mesh;
            engine::TextureHandle texture;
            bool alpha_blend = false;
            bool alpha_test = false;
            bool two_sided = false;
        };

        struct GpuModel {
            std::vector<GpuSubmesh> parts;
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
            std::string name;
        };

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
                verts.reserve(mesh.pos_uv.size() / 5);
                for (std::size_t i = 0; i + 4 < mesh.pos_uv.size(); i += 5) {
                    verts.push_back({mesh.pos_uv[i],
                                     mesh.pos_uv[i + 1],
                                     mesh.pos_uv[i + 2],
                                     mesh.pos_uv[i + 3],
                                     mesh.pos_uv[i + 4]});
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
            auto [ins, ok] = model_cache.emplace(gb_path, std::move(gpu));
            return ok && !ins->second.parts.empty() ? &ins->second : nullptr;
        }

    }  // namespace

    class TileView::Impl {
      public:
        Impl(
            std::string data_root, int patch_x, int patch_y, std::string export_dir, float ui_scale)
            : m_data_root(std::move(data_root)),
              m_maps_dir(platform::resolve_path_case(m_data_root, "MAPS")),
              m_export_dir(std::move(export_dir)), m_patch_x(patch_x), m_patch_y(patch_y),
              m_ui_scale(std::clamp(ui_scale, 0.5f, 8.0f)),
              m_font_scale(std::max(2.0f, 2.0f * m_ui_scale)),
              m_stem(patch_stem(patch_x, patch_y)) {}

        ~Impl() {
            m_ui.destroy();
            if (m_engine) {
                m_engine->shutdown();
            }
        }

        [[nodiscard]] int run() {
            const int win_w = static_cast<int>(1600.0f * m_ui_scale);
            const int win_h = static_cast<int>(900.0f * m_ui_scale);
            m_engine.reset(engine::create_engine());
            if (!m_engine ||
                !m_engine->init(("openkal tile_view " + m_stem).c_str(), win_w, win_h)) {
                log::error("tile_view: engine init failed (no display / GLFW?)");
                return 1;
            }
            m_engine->maximize_window();
            m_engine->set_cursor_mode(engine::CursorMode::Captured);
            m_look_captured = true;
            m_ui.init(*m_engine, m_data_root);

            if (!load_scene()) {
                log::error("tile_view: failed to load " + m_stem + " from " + m_maps_dir);
                return 1;
            }
            reset_camera();

            engine::FrameInput in;
            while (m_engine->poll(in)) {
                if (in.quit) {
                    break;
                }
                handle_input(in);
                if (m_quit) {
                    break;
                }
                const float aspect =
                    (in.height > 0) ? static_cast<float>(in.width) / static_cast<float>(in.height)
                                    : 16.0f / 9.0f;
                m_engine->begin_frame(0.45f, 0.62f, 0.85f);
                draw_world(aspect);
                draw_hud(static_cast<float>(in.width), static_cast<float>(in.height));
                m_engine->end_frame();
            }
            return 0;
        }

      private:
        bool load_scene() {
            const std::string kcm = find_maps_file(m_maps_dir, m_patch_x, m_patch_y, ".kcm");
            const std::string opl = find_maps_file(m_maps_dir, m_patch_x, m_patch_y, ".opl");
            const std::string env = find_existing({
                platform::join_path(m_maps_dir, "n.env"),
                platform::join_path(m_maps_dir, "N.ENV"),
            });
            const std::string tex_dir = platform::join_path(m_maps_dir, "Tex");
            if (kcm.empty()) {
                m_status = "missing KCM in " + m_maps_dir;
                return false;
            }

            std::vector<terrain::LayerInfo> env_layers;
            if (!env.empty()) {
                (void) terrain::load_env_layers(env, env_layers);
            }

            terrain::HeightPatch patch;
            if (!terrain::load_kcm(kcm, patch)) {
                m_status = "failed to load " + kcm;
                return false;
            }
            patch.patch_x = m_patch_x;
            patch.patch_y = m_patch_y;
            m_center_height = sample_center_height(patch);
            m_terrain = upload_patch(*m_engine, patch, tex_dir, env_layers, m_detail_cache);
            if (!m_terrain.mesh || m_terrain.layers.empty()) {
                m_status = "failed to upload terrain for " + kcm;
                return false;
            }

            if (!opl.empty()) {
                terrain::OplFile file;
                if (terrain::load_opl(opl, file)) {
                    std::map<std::string, int> unique_names;
                    for (const auto& inst : file.instances) {
                        ++m_opl_count;
                        unique_names[inst.name.empty() ? "(empty)" : inst.name] += 1;
                        if (inst.name.empty()) {
                            ++m_missing_gb;
                            continue;
                        }
                        const std::string gb_path =
                            platform::resolve_client_asset(m_data_root, inst.name, ".gb");
                        if (gb_path.empty() || !platform::file_exists(gb_path)) {
                            ++m_missing_gb;
                            log::warn("tile_view missing gb " + inst.name);
                            continue;
                        }
                        const GpuModel* gpu =
                            get_or_load_gb(*m_engine, gb_path, m_model_cache, m_prop_tex_cache);
                        if (!gpu) {
                            ++m_missing_gb;
                            log::warn("tile_view failed gb " + gb_path);
                            continue;
                        }
                        DrawnProp p;
                        p.gpu = gpu;
                        p.name = inst.name;
                        float wx = 0, wy = 0, wz = 0;
                        terrain::opl_world_translation(inst, m_patch_x, m_patch_y, wx, wy, wz);
                        const auto S = engine::Mat4::scaling(inst.sx, inst.sy, inst.sz);
                        const auto R =
                            engine::Mat4::from_quaternion(inst.qx, inst.qy, inst.qz, inst.qw);
                        const auto T = engine::Mat4::translation(wx, wy, wz);
                        p.model = engine::Mat4::multiply(engine::Mat4::multiply(T, R), S);
                        m_props.push_back(p);
                    }
                    for (const auto& [name, count] : unique_names) {
                        log::info("tile_view opl " + std::to_string(count) + "  " + name);
                    }
                }
            }

            log::info("tile_view " + m_stem + " props=" + std::to_string(m_props.size()) + "/" +
                      std::to_string(m_opl_count) +
                      " unique_gb=" + std::to_string(m_model_cache.size()) +
                      " missing=" + std::to_string(m_missing_gb));
            m_status = m_stem + "  props " + std::to_string(static_cast<int>(m_props.size())) +
                       "/" + std::to_string(m_opl_count) + "  missing " +
                       std::to_string(m_missing_gb);
            return true;
        }

        static float sample_center_height(const terrain::HeightPatch& patch) {
            const int n = map::kHeightSamples;
            if (static_cast<int>(patch.heights.size()) < n * n) {
                return 2000.0f;
            }
            const int cx = n / 2;
            const int cz = n / 2;
            return static_cast<float>(patch.heights[static_cast<std::size_t>(cz * n + cx)]);
        }

        void reset_camera() {
            const float extent = static_cast<float>(map::kMapSize * map::kTerrainCellSize);
            const float x0 = static_cast<float>(m_patch_x - map::kPatchGrid / 2) * extent;
            const float z0 = static_cast<float>(m_patch_y - map::kPatchGrid / 2) * extent;
            m_eye.x = x0 + extent * 0.5f;
            m_eye.y = m_center_height + 420.0f;
            m_eye.z = z0 + extent * 0.5f;
            m_yaw = 0.35f;
            m_pitch = -0.45f;
        }

        void handle_input(const engine::FrameInput& in) {
            if (in.key_escape_pressed) {
                if (m_look_captured) {
                    m_look_captured = false;
                    m_engine->set_cursor_mode(engine::CursorMode::Free);
                } else {
                    m_quit = true;
                }
                return;
            }
            if (in.mouse_left_clicked && !m_look_captured) {
                m_look_captured = true;
                m_engine->set_cursor_mode(engine::CursorMode::Captured);
            }
            if (in.key_r_pressed) {
                reset_camera();
            }
            if (in.key_e_pressed) {
                run_export();
            }

            if (m_look_captured) {
                m_yaw -= in.mouse_dx * kLookSensitivity;
                m_pitch = std::clamp(m_pitch - in.mouse_dy * kLookSensitivity, -1.45f, 1.45f);
            }

            const float cy = std::cos(m_yaw);
            const float sy = std::sin(m_yaw);
            const float cp = std::cos(m_pitch);
            const float sp = std::sin(m_pitch);
            const engine::Vec3 forward{sy * cp, sp, -cy * cp};
            const engine::Vec3 right{cy, 0.0f, sy};
            engine::Vec3 move{0, 0, 0};
            if (in.key_w) {
                move.x += forward.x;
                move.y += forward.y;
                move.z += forward.z;
            }
            if (in.key_s) {
                move.x -= forward.x;
                move.y -= forward.y;
                move.z -= forward.z;
            }
            if (in.key_d) {
                move.x -= right.x;
                move.z -= right.z;
            }
            if (in.key_a) {
                move.x += right.x;
                move.z += right.z;
            }
            const float len = std::sqrt(move.x * move.x + move.y * move.y + move.z * move.z);
            const float speed = (in.key_shift ? kSprintSpeed : kWalkSpeed) * std::max(in.dt, 0.0f);
            if (len > 1e-4f) {
                const float inv = speed / len;
                m_eye.x += move.x * inv;
                m_eye.y += move.y * inv;
                m_eye.z += move.z * inv;
            }
            if (in.key_space) {
                m_eye.y += kVerticalSpeed * std::max(in.dt, 0.0f) * (in.key_shift ? 2.5f : 1.0f);
            }
            if (in.key_ctrl) {
                m_eye.y -= kVerticalSpeed * std::max(in.dt, 0.0f) * (in.key_shift ? 2.5f : 1.0f);
            }
        }

        void run_export() {
            if (m_export_dir.empty()) {
                m_status = "pass --export DIR to copy this tile's files";
                log::warn(m_status);
                return;
            }
            const ExportResult out =
                TileExporter::export_tile(m_data_root, m_export_dir, m_patch_x, m_patch_y);
            m_status = out.message();
        }

        void draw_world(float aspect) {
            const float cy = std::cos(m_yaw);
            const float sy = std::sin(m_yaw);
            const float cp = std::cos(m_pitch);
            const float sp = std::sin(m_pitch);
            const engine::Vec3 forward{sy * cp, sp, -cy * cp};
            const engine::Vec3 center{
                m_eye.x + forward.x, m_eye.y + forward.y, m_eye.z + forward.z};
            const auto view = engine::Mat4::multiply(
                engine::Mat4::scaling(-1.0f, 1.0f, 1.0f),
                engine::Mat4::look_at(m_eye, center, engine::Vec3{0.0f, 1.0f, 0.0f}));
            const auto proj = engine::Mat4::perspective(kFovRad, aspect, 2.0f, 80000.0f);
            m_engine->set_camera(view, proj);

            if (!m_terrain.layers.empty()) {
                engine::TerrainLayerDraw layer;
                layer.color = m_terrain.layers[0].color;
                layer.detail = m_terrain.layers[0].detail;
                layer.repeat = m_terrain.layers[0].repeat;
                layer.alpha_blend = false;
                m_engine->draw_terrain_layer(m_terrain.mesh, layer);
            }
            for (std::size_t i = 1; i < m_terrain.layers.size(); ++i) {
                engine::TerrainLayerDraw layer;
                layer.color = m_terrain.layers[i].color;
                layer.detail = m_terrain.layers[i].detail;
                layer.repeat = m_terrain.layers[i].repeat;
                layer.alpha_blend = true;
                m_engine->draw_terrain_layer(m_terrain.mesh, layer);
            }

            for (int pass = 0; pass < 2; ++pass) {
                const bool want_alpha = pass == 1;
                for (const auto& p : m_props) {
                    if (!p.gpu) {
                        continue;
                    }
                    for (const auto& part : p.gpu->parts) {
                        if (part.alpha_blend != want_alpha) {
                            continue;
                        }
                        m_engine->draw_mesh_textured(part.mesh,
                                                     part.texture,
                                                     p.model,
                                                     part.alpha_blend,
                                                     part.two_sided,
                                                     part.alpha_test && !part.alpha_blend);
                    }
                }
            }
        }

        void draw_hud(float win_w, float win_h) {
            (void) win_w;
            const float s = m_ui_scale;
            const float pad = 14.0f * s;
            const float hud_h = 118.0f * s;
            m_engine->draw_ui_rect(0.0f, 0.0f, 720.0f * s, hud_h, 0.05f, 0.06f, 0.08f, 0.72f);
            m_ui.draw_text_glyphs(pad, pad, m_font_scale, m_status, 0.95f, 0.93f, 0.78f);

            char cam[128];
            std::snprintf(cam, sizeof(cam), "xyz %.0f %.0f %.0f", m_eye.x, m_eye.y, m_eye.z);
            m_ui.draw_text_glyphs(
                pad, pad + 22.0f * s, m_font_scale * 0.85f, cam, 0.75f, 0.78f, 0.82f);

            const char* help1 = "WASD fly  Space/Ctrl up/down  Shift sprint  mouse look";
            const char* help2 = "R reset  E export  click capture  Esc release/quit";
            m_ui.draw_text_glyphs(
                pad, pad + 44.0f * s, m_font_scale * 0.75f, help1, 0.70f, 0.72f, 0.76f);
            m_ui.draw_text_glyphs(
                pad, pad + 64.0f * s, m_font_scale * 0.75f, help2, 0.70f, 0.72f, 0.76f);
            if (!m_look_captured) {
                m_ui.draw_text_glyphs(pad,
                                      win_h - 28.0f * s,
                                      m_font_scale * 0.8f,
                                      "click to capture mouse",
                                      1.0f,
                                      0.85f,
                                      0.45f);
            }
        }

        std::string m_data_root;
        std::string m_maps_dir;
        std::string m_export_dir;
        int m_patch_x = 0;
        int m_patch_y = 0;
        float m_ui_scale = 1.0f;
        float m_font_scale = 2.0f;
        std::string m_stem;
        std::string m_status;
        std::unique_ptr<engine::IEngine> m_engine;
        client::WidgetTools m_ui;
        std::unordered_map<std::string, engine::TextureHandle> m_detail_cache;
        std::unordered_map<std::string, GpuModel> m_model_cache;
        std::unordered_map<std::string, engine::TextureHandle> m_prop_tex_cache;
        DrawnPatch m_terrain;
        std::vector<DrawnProp> m_props;
        int m_opl_count = 0;
        int m_missing_gb = 0;
        float m_center_height = 2000.0f;
        engine::Vec3 m_eye;
        float m_yaw = 0.0f;
        float m_pitch = 0.0f;
        bool m_look_captured = true;
        bool m_quit = false;
    };

    TileView::TileView(
        std::string data_root, int patch_x, int patch_y, std::string export_dir, float ui_scale)
        : m_impl(std::make_unique<Impl>(
              std::move(data_root), patch_x, patch_y, std::move(export_dir), ui_scale)) {}

    TileView::~TileView() = default;

    int TileView::run() {
        return m_impl->run();
    }

}  // namespace openkal::tile_view
