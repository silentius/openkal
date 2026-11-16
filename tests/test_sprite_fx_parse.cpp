/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/SpriteFx.h"
#include "test_util.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

    class RecordingEngine final : public openkal::engine::IEngine {
      public:
        bool init(const char*, int, int) override {
            return true;
        }

        void shutdown() override {}

        bool poll(openkal::engine::FrameInput&) override {
            return false;
        }

        void maximize_window() override {}

        bool set_window_mode(openkal::engine::WindowMode, int, int) override {
            return true;
        }

        void set_cursor_mode(openkal::engine::CursorMode) override {}

        openkal::engine::RenderCapabilities render_capabilities() const override {
            return {};
        }

        void set_bloom_strength(float) override {}

        void begin_frame(float, float, float) override {}

        void begin_ui_pass() override {}

        [[nodiscard]] openkal::engine::FrameDrawStats frame_stats() const override {
            return {};
        }

        void set_camera(const openkal::engine::Mat4&, const openkal::engine::Mat4&) override {}

        void set_polygon_offset(float, float) override {}

        void set_depth_less_equal(bool) override {}

        openkal::engine::TextureHandle
        create_texture_rgba8(const std::uint8_t*, int, int, bool, bool) override {
            return openkal::engine::TextureHandle{++m_next_texture};
        }

        openkal::engine::TextureHandle create_texture_r32f(const float*, int, int) override {
            return openkal::engine::TextureHandle{++m_next_texture};
        }

        void destroy_texture(openkal::engine::TextureHandle) override {}

        openkal::engine::MeshHandle create_mesh(const openkal::engine::VertexP3T2*,
                                                std::size_t,
                                                const std::uint32_t*,
                                                std::size_t) override {
            return {};
        }

        openkal::engine::MeshHandle create_mesh_lm(const openkal::engine::VertexP3T2T2*,
                                                   std::size_t,
                                                   const std::uint32_t*,
                                                   std::size_t) override {
            return {};
        }

        bool update_mesh(openkal::engine::MeshHandle,
                         const openkal::engine::VertexP3T2*,
                         std::size_t) override {
            return false;
        }

        void destroy_mesh(openkal::engine::MeshHandle) override {}

        void draw_mesh(openkal::engine::MeshHandle, openkal::engine::TextureHandle) override {}

        void draw_mesh_textured(openkal::engine::MeshHandle,
                                openkal::engine::TextureHandle,
                                const openkal::engine::Mat4&,
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

        void draw_mesh_shadow(openkal::engine::MeshHandle,
                              openkal::engine::TextureHandle,
                              const openkal::engine::Mat4&,
                              const openkal::engine::ShadowReceiver&,
                              float,
                              bool,
                              float) override {}

        void draw_mesh_textured_instances(openkal::engine::MeshHandle,
                                          openkal::engine::TextureHandle,
                                          const std::vector<openkal::engine::Mat4>&,
                                          bool,
                                          bool) override {}

        void draw_sprite_batch(const std::vector<openkal::engine::SpriteBatchVertex>& vertices,
                               openkal::engine::TextureHandle,
                               bool,
                               bool) override {
            ++m_sprite_batch_calls;
            m_sprite_vertices += vertices.size();
        }

        void draw_mesh_lightmapped(openkal::engine::MeshHandle,
                                   openkal::engine::TextureHandle,
                                   openkal::engine::TextureHandle,
                                   const openkal::engine::Mat4&,
                                   bool,
                                   bool,
                                   bool,
                                   float,
                                   float,
                                   float,
                                   float,
                                   float,
                                   float) override {}

        void draw_water_plane(openkal::engine::MeshHandle,
                              openkal::engine::TextureHandle,
                              const openkal::engine::Mat4&,
                              const openkal::engine::Vec3&,
                              float) override {}

        void draw_sky_face(openkal::engine::MeshHandle,
                           openkal::engine::TextureHandle,
                           const openkal::engine::Mat4&) override {}

        void draw_terrain_layer(openkal::engine::MeshHandle,
                                const openkal::engine::TerrainLayerDraw&) override {}

        void draw_mesh_lit(openkal::engine::MeshHandle,
                           const openkal::engine::Mat4&,
                           float,
                           float,
                           float) override {}

        void draw_ui_quad(openkal::engine::TextureHandle,
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

        bool begin_offscreen(int, int, float, float, float) override {
            return false;
        }

        bool end_offscreen_rgba(std::vector<std::uint8_t>&) override {
            return false;
        }

        void end_frame() override {}

        [[nodiscard]] std::size_t sprite_batch_calls() const {
            return m_sprite_batch_calls;
        }

        [[nodiscard]] std::size_t sprite_vertices() const {
            return m_sprite_vertices;
        }

      private:
        std::uint32_t m_next_texture = 0;
        std::size_t m_sprite_batch_calls = 0;
        std::size_t m_sprite_vertices = 0;
    };

    class TempDirectory final {
      public:
        TempDirectory() {
            const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
            m_path = std::filesystem::temp_directory_path() /
                     ("openkal_sprite_batch_" + std::to_string(stamp));
            std::filesystem::create_directories(m_path / "Effect/Sprite");
            std::filesystem::create_directories(m_path / "Effect/Fx");
        }

        ~TempDirectory() {
            std::error_code error;
            std::filesystem::remove_all(m_path, error);
        }

        TempDirectory(const TempDirectory&) = delete;
        TempDirectory& operator=(const TempDirectory&) = delete;

        [[nodiscard]] const std::filesystem::path& path() const {
            return m_path;
        }

      private:
        std::filesystem::path m_path;
    };

}  // namespace

int main() {
    using openkal::test::expect;
    openkal::client::SpriteFx fx;
    {
        openkal::engine::Mat4 monster = openkal::engine::Mat4::identity();
        monster.m = {0.0f,
                     0.0f,
                     1.0f,
                     0.0f,
                     0.0f,
                     1.0f,
                     0.0f,
                     0.0f,
                     -1.0f,
                     0.0f,
                     0.0f,
                     0.0f,
                     4.0f,
                     5.0f,
                     6.0f,
                     1.0f};
        openkal::engine::Mat4 mouth = openkal::engine::Mat4::identity();
        mouth.m[12] = 10.0f;
        mouth.m[13] = 20.0f;
        mouth.m[14] = 30.0f;
        const auto attach =
            openkal::client::SpriteFx::effect_bone_attach_matrix(monster, mouth, false);
        const auto emitter =
            openkal::client::SpriteFx::offset_in_matrix_axes(attach, 0.0f, -7.0f, -10.0f);
        const auto forward = openkal::engine::Mat4::transform_point(emitter, 0.0f, 0.0f, -1.0f);
        expect(emitter.m[12] == 20.0f && emitter.m[13] == 13.0f && emitter.m[14] == 30.0f,
               "monster FX offset starts at the effect bone in monster-local axes");
        expect(forward.x == 21.0f && forward.y == 13.0f && forward.z == 30.0f,
               "monster FX local -Z follows the monster facing");
    }
    expect(fx.parsed_fx_uses_effect_bone_rotation(
               "{ { model effect_ef019a useEffectBoneRotation effectBoneIndex 0 } }"),
           "useEffectBoneRotation is parsed as a flag");
    expect(!fx.parsed_fx_uses_effect_bone_rotation("{ { model effect_ef019a effectBoneIndex 0 } }"),
           "stages without useEffectBoneRotation stay false");
    expect(fx.parsed_fx_oneshot_life("{ { particle davi_ef132_01 delay 0 duration 1 } "
                                     "{ particle davi_ef132_02 delay 1 duration 1 } }") > 1.99f,
           "Arrow Rain davi_ef132_01 keeps the delayed impact particle");
    expect(fx.parsed_fx_oneshot_life("{ { particle davi_ef132 duration 0.5 } }") > 0.49f &&
               fx.parsed_fx_oneshot_life("{ { particle davi_ef132 duration 0.5 } }") < 0.51f,
           "Arrow Rain bow burst oneshot matches duration 0.5");
    expect(fx.parsed_sprite_anim_frames(
               "{ { animTexture 10 20 \"Data\\Effect\\Tex\\davi_ef132.tga\" } }") == 10,
           "Arrow Rain rain atlas is 10 frames of davi_ef132.tga");
    expect(fx.parsed_sprite_cull_distance("{ cullDistance 600 { texture flame.tga } }") > 599.9f &&
               fx.parsed_sprite_cull_distance("{ cullDistance 600 { texture flame.tga } }") <
                   600.1f,
           "D5 lamp shaders keep authored cullDistance 600");
    expect(fx.parsed_sprite_cull_distance("{ { texture flame.tga } }") > 767.9f &&
               fx.parsed_sprite_cull_distance("{ { texture flame.tga } }") < 768.1f,
           "particle shaders without cullDistance use the initial 768 default");
    expect(fx.parsed_sprite_distribution("{ { distribution cylinder ( 25 0 25 5 ) } }") ==
               openkal::client::SpriteFx::SpriteDistribution::Cylinder,
           "Arrow Rain rain uses cylinder distribution");
    expect(fx.parsed_sprite_distribution("{ { distribution sphere ( 0 0 0 8 ) } }") ==
               openkal::client::SpriteFx::SpriteDistribution::Sphere,
           "sphere stays distinct from cylinder");
    expect(fx.parsed_fx_oneshot_life(
               "{ { particle davi_ef131_04 sound SE010004 effectBoneIndex 1 duration 3 } }") >
               2.99f,
           "Arrow Explosion fuse FX lasts 3 s");
    expect(fx.parsed_sprite_anim_frames(
               "{ { animTexture 4 8 \"Data\\Effect\\Tex\\davi_ef131_01.dds\" } }") == 4,
           "Arrow Explosion fuse atlas is 4 frames of davi_ef131_01");
    expect(fx.parsed_fx_oneshot_life(
               "{ { particle davi_ef131_05 sound SE010005 effectBoneIndex -1 duration 3 } }") >
               2.99f,
           "Arrow Explosion burst FX lasts 3 s");
    expect(fx.parsed_fx_oneshot_life("{ { particle Player_EF_code_EF068a effectBoneIndex -1 "
                                     "duration 2 restart 0 } }") > 1.99f &&
               fx.parsed_fx_oneshot_life("{ { particle Player_EF_code_EF068a effectBoneIndex -1 "
                                         "duration 2 restart 0 } }") < 2.01f,
           "Poison Cloud effect_ef068a packed duration is 2 s");
    expect(fx.parsed_sprite_distribution("{ { distribution box ( 80 20 80 ) } }") ==
               openkal::client::SpriteFx::SpriteDistribution::Box,
           "Poison Cloud green stage uses a box distribution");
    expect(fx.parsed_sprite_distribution("{ { distribution cylinder ( 10 0 10 0 ) } }") ==
               openkal::client::SpriteFx::SpriteDistribution::Cylinder,
           "Arrow Explosion burst smoke uses cylinder distribution");
    expect(fx.parsed_sprite_has_initial_angle(
               "{ { initialAngle 90 orientation viewy size ( 0 7 ) aspect ( 1 40 ) } }"),
           "medicine column stage pins initialAngle");
    expect(fx.parsed_sprite_initial_angle("{ { initialAngle 90 orientation viewy } }") > 89.9f &&
               fx.parsed_sprite_initial_angle("{ { initialAngle 90 orientation viewy } }") < 90.1f,
           "medicine column initialAngle is 90 degrees");
    expect(!fx.parsed_sprite_has_initial_angle("{ { orientation viewy } }"),
           "stages without initialAngle keep a random start angle");

    TempDirectory temp;
    {
        std::ofstream script(temp.path() / "Effect/Sprite/batch.txt");
        script << "batch_test { cullDistance 600 { texture missing.tga count 4 time 1 "
                  "deadTime 0 bunching 0 cycles 0 size ( 10 10 ) "
                  "blendFunc addblend } }";
    }
    {
        std::ofstream script(temp.path() / "Effect/Fx/bof_loop.txt");
        script << "bof_loop_test { { particle batch_test delay 30 duration 3 restart 1 } }";
    }
    RecordingEngine engine;
    expect(fx.init(engine, temp.path().string()), "sprite batching fixture initializes");
    fx.attach_particle(engine, 1, "batch_test", -1, 0.0f, 0.0f, 0.0f);
    fx.attach_particle(engine, 2, "batch_test", -1, 0.0f, 0.0f, 0.0f);
    fx.follow_all(1, 0.0f, 0.0f, 0.0f);
    fx.follow_all(2, 20.0f, 0.0f, 0.0f);
    fx.update(0.5f, 0.0f, 0.0f, 50.0f);
    fx.draw(engine);
    expect(fx.last_draw_billboards() == 8, "two four-particle emitters produce eight quads");
    expect(fx.last_draw_batches() == 1 && engine.sprite_batch_calls() == 1,
           "matching texture and blend state collapse into one GPU batch");
    expect(engine.sprite_vertices() == 48, "eight quads stream as forty-eight triangle vertices");
    fx.detach(1);
    fx.detach(2);
    fx.attach_fx(engine, 3, "bof_loop_test", -1, 10.0f);
    fx.update(0.5f, 0.0f, 0.0f, 50.0f);
    fx.draw(engine);
    expect(fx.last_draw_billboards() == 0,
           "BOF delay override keeps the emitter hidden before 10 seconds");
    fx.update(9.6f, 0.0f, 0.0f, 50.0f);
    fx.draw(engine);
    expect(fx.last_draw_billboards() == 4,
           "BOF delay override starts the particle after 10 seconds");
    fx.update(3.0f, 0.0f, 0.0f, 50.0f);
    fx.draw(engine);
    expect(fx.last_draw_billboards() == 0, "BOF delay override restarts on a 13-second cycle");
    fx.destroy(engine);
    return openkal::test::g_failures == 0 ? 0 : 1;
}
