/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/engine/engine.h"

namespace openkal::engine {
    namespace {

        class NullEngine final : public IEngine {
          public:
            bool init(const char*, int width, int height) override {
                width_ = width;
                height_ = height;
                return true;
            }
            void shutdown() override {}
            bool poll(FrameInput& out) override {
                out = {};
                out.width = width_;
                out.height = height_;
                out.dt = 1.0f / 60.0f;
                out.quit = frames_++ > 3;
                return !out.quit;
            }
            void maximize_window() override {}
            bool set_window_mode(WindowMode, int width, int height) override {
                width_ = width;
                height_ = height;
                return true;
            }
            void set_cursor_mode(CursorMode) override {}
            RenderCapabilities render_capabilities() const override {
                return {};
            }
            void set_bloom_strength(float) override {}
            void begin_frame(float, float, float) override {}
            void begin_ui_pass() override {}
            FrameDrawStats frame_stats() const override {
                return {};
            }
            void set_camera(const Mat4&, const Mat4&) override {}
            void set_polygon_offset(float, float) override {}
            void set_depth_less_equal(bool) override {}
            TextureHandle create_texture_rgba8(const std::uint8_t*, int, int, bool, bool) override {
                return TextureHandle{++next_tex_};
            }
            TextureHandle create_texture_r32f(const float*, int, int) override {
                return TextureHandle{++next_tex_};
            }
            void destroy_texture(TextureHandle) override {}
            MeshHandle create_mesh(const VertexP3T2*,
                                   std::size_t,
                                   const std::uint32_t*,
                                   std::size_t) override {
                return MeshHandle{++next_mesh_};
            }
            MeshHandle create_mesh_lm(const VertexP3T2T2*,
                                      std::size_t,
                                      const std::uint32_t*,
                                      std::size_t) override {
                return MeshHandle{++next_mesh_};
            }
            bool update_mesh(MeshHandle, const VertexP3T2*, std::size_t) override {
                return true;
            }
            void destroy_mesh(MeshHandle) override {}
            void draw_mesh(MeshHandle, TextureHandle) override {}
            void draw_mesh_textured(MeshHandle,
                                    TextureHandle,
                                    const Mat4&,
                                    bool,
                                    bool,
                                    bool,
                                    bool,
                                    float,
                                    float,
                                    float,
                                    float,
                                    float,
                                    float,
                                    float) override {}
            void draw_mesh_shadow(MeshHandle,
                                  TextureHandle,
                                  const Mat4&,
                                  const ShadowReceiver&,
                                  float,
                                  bool,
                                  float) override {}
            void draw_mesh_textured_instances(
                MeshHandle, TextureHandle, const std::vector<Mat4>&, bool, bool) override {}
            void draw_sprite_batch(const std::vector<SpriteBatchVertex>&,
                                   TextureHandle,
                                   bool,
                                   bool) override {}
            void draw_mesh_lightmapped(MeshHandle,
                                       TextureHandle,
                                       TextureHandle,
                                       const Mat4&,
                                       bool,
                                       bool,
                                       bool,
                                       float,
                                       float,
                                       float,
                                       float,
                                       float,
                                       float) override {}
            void
            draw_water_plane(MeshHandle, TextureHandle, const Mat4&, const Vec3&, float) override {}
            void draw_sky_face(MeshHandle, TextureHandle, const Mat4&) override {}
            void draw_terrain_layer(MeshHandle, const TerrainLayerDraw&) override {}
            void draw_mesh_lit(MeshHandle, const Mat4&, float, float, float) override {}
            void draw_ui_quad(TextureHandle,
                              float,
                              float,
                              float,
                              float,
                              float,
                              float,
                              float,
                              float,
                              float,
                              float,
                              float,
                              float) override {}
            void draw_ui_rect(float, float, float, float, float, float, float, float) override {}
            void draw_ui_triangle(
                float, float, float, float, float, float, float, float, float, float) override {}
            bool begin_offscreen(int width, int height, float, float, float) override {
                off_w_ = width;
                off_h_ = height;
                return width > 0 && height > 0;
            }
            bool end_offscreen_rgba(std::vector<std::uint8_t>& rgba_out) override {
                if (off_w_ <= 0 || off_h_ <= 0) {
                    return false;
                }
                rgba_out.assign(
                    static_cast<std::size_t>(off_w_) * static_cast<std::size_t>(off_h_) * 4, 0);
                off_w_ = off_h_ = 0;
                return true;
            }
            void end_frame() override {}

          private:
            int off_w_ = 0;
            int off_h_ = 0;
            int width_ = 0;
            int height_ = 0;
            int frames_ = 0;
            std::uint32_t next_mesh_ = 0;
            std::uint32_t next_tex_ = 0;
        };

    }  // namespace

    IEngine* create_engine() {
        return new NullEngine();
    }

}  // namespace openkal::engine
