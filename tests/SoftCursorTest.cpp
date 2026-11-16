/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/SoftCursor.h"

#include "openkal/platform/ConfigArchive.h"
#include "test_util.h"

#include <cmath>
#include <filesystem>
#include <limits>
#include <string>

namespace {
    class CursorEngine final : public openkal::engine::IEngine {
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

        void destroy_texture(openkal::engine::TextureHandle) override {
            ++m_destroyed;
        }

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

        void draw_sprite_batch(const std::vector<openkal::engine::SpriteBatchVertex>&,
                               openkal::engine::TextureHandle,
                               bool,
                               bool) override {}

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

        void draw_ui_quad(openkal::engine::TextureHandle texture,
                          float x,
                          float y,
                          float w,
                          float h,
                          float,
                          float,
                          float,
                          float,
                          float u0,
                          float,
                          float u1,
                          float) override {
            m_quad = {texture.id, x, y, w, h, u0, u1};
            m_draws.push_back(m_quad);
        }

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

        struct Quad {
            std::uint32_t texture = 0;
            float x = 0, y = 0, w = 0, h = 0, u0 = 0, u1 = 0;
        };
        const Quad& quad() const {
            return m_quad;
        }
        const std::vector<Quad>& draws() const {
            return m_draws;
        }
        void clear_draws() {
            m_draws.clear();
        }
        std::uint32_t live_textures() const {
            return m_next_texture - m_destroyed;
        }

      private:
        std::uint32_t m_next_texture = 0;
        std::uint32_t m_destroyed = 0;
        Quad m_quad;
        std::vector<Quad> m_draws;
    };

}  // namespace

int main() {
    using openkal::client::CursorKind;
    using openkal::client::SoftCursor;
    using openkal::client::WidgetTools;
    using openkal::platform::ConfigArchive;
    using openkal::test::expect;
    CursorEngine engine;
    WidgetTools ui;
    ui.init(engine, "/missing-cursor-test-data");
    const auto baseline = engine.live_textures();
    const auto root = std::filesystem::temp_directory_path() / "openkal-cursor-package-test";
    std::string error;
    expect(ConfigArchive::mount(root.string(), OPENKAL_TEST_CONFIG_ARCHIVE, &error),
           "mount packaged cursor artwork without loose files");
    SoftCursor cursor;
    expect(cursor.load(engine, (root / "config").string()), "load cursors from package");
    cursor.draw(ui, 100, 200, 0.5f);
    const auto arrow = engine.quad();
    expect(arrow.w == 48 && arrow.h == 48 && arrow.x == 94 && arrow.y == 196,
           "arrow tip stays at the mouse hotspot and only one cell is drawn");
    expect(arrow.u0 == 0 && std::abs(arrow.u1 - 1.0f / 30.0f) < 0.0001f,
           "arrow selects the first of thirty frames");
    cursor.set(CursorKind::Default);
    cursor.draw(ui, 100, 200, 0.5f);
    expect(std::abs(engine.quad().u0 - 0.5f) < 0.0001f,
           "reselecting the same cursor preserves animation progress");
    cursor.draw(ui, 100, 200, 0);
    expect(engine.quad().u0 == 0,
           "highlight returns to the static arrow after its one-second sweep");
    cursor.draw(ui, 100, 200, 3.5f);
    cursor.draw(ui, 100, 200, 0.5f);
    expect(engine.quad().u0 == 0, "highlight stays off during the four-second pause");
    cursor.draw(ui, 100, 200, 0.5f);
    expect(engine.quad().u0 == 0, "next sweep starts at five seconds");
    cursor.draw(ui, 100, 200, 0);
    expect(std::abs(engine.quad().u0 - 0.5f) < 0.0001f,
           "highlight advances again after the five-second interval");
    cursor.set(CursorKind::Attack);
    cursor.draw(ui, 100, 200, 0.5f);
    expect(engine.quad().texture != arrow.texture && engine.quad().u0 == 0,
           "attack uses its sword texture and starts at frame zero");
    cursor.draw(ui, 100, 200, 0.5f);
    expect(std::abs(engine.quad().u0 - 0.5f) < 0.0001f, "sword has the same loop timing");
    cursor.set(CursorKind::Item, 42);
    cursor.draw(ui, 100, 200, 0.2f);
    expect(engine.quad().u0 == 0 && engine.quad().x == 76 && engine.quad().y == 176,
           "item entry starts with an open hand centered on the pickup area");
    cursor.set(CursorKind::Item, 42);
    cursor.draw(ui, 100, 200, 0.2f);
    expect(std::abs(engine.quad().u0 - 0.5f) < 0.0001f, "hand closes over 0.4 seconds");
    cursor.draw(ui, 100, 200, 0.2f);
    expect(std::abs(engine.quad().u0 - 15.0f / 16.0f) < 0.0001f,
           "hand reaches the closed pose before reopening");
    cursor.set(CursorKind::Item, 42);
    cursor.draw(ui, 100, 200, 0.2f);
    expect(engine.quad().u0 > 0.4f && engine.quad().u0 < 0.6f,
           "hand reopens smoothly while remaining over the same item");
    cursor.draw(ui, 100, 200, 0.2f);
    expect(engine.quad().u0 == 0, "hand returns to its open pose after 0.8 seconds");
    cursor.draw(ui, 100, 200, 0);
    expect(engine.quad().u0 > 0.4f && engine.quad().u0 < 0.6f,
           "hand starts the next grasp without leaving the item");
    cursor.set(CursorKind::Item, 43);
    cursor.draw(ui, 100, 200, 0);
    expect(engine.quad().u0 == 0, "another item retriggers the grasp");
    cursor.set(CursorKind::Default);
    cursor.set(CursorKind::Item, 43);
    cursor.draw(ui, 100, 200, std::numeric_limits<float>::quiet_NaN());
    cursor.draw(ui, 100, 200, -1);
    cursor.draw(ui, 100, 200, 0);
    expect(engine.quad().u0 == 0, "reentry resets the grasp and invalid frame times are ignored");
    cursor.set_index(7);
    expect(cursor.index() == 0, "missing legacy slot falls back to default");
    cursor.set_index(99);
    expect(cursor.index() == 0, "out-of-range slot falls back to default");
    engine.clear_draws();
    cursor.draw(ui, 100, 200, 0.2f, true);
    expect(engine.draws().size() == 2, "Control press draws a locator ring behind the cursor");
    const auto ring = engine.draws().front();
    expect(std::abs(ring.x + ring.w * 0.5f - 100) < 0.001f &&
               std::abs(ring.y + ring.h * 0.5f - 200) < 0.001f,
           "locator is centered on the pointer hotspot");
    engine.clear_draws();
    cursor.draw(ui, 150, 250, 0.2f, true);
    expect(engine.draws().size() == 3 && engine.draws().front().w < ring.w,
           "rings contract with staggered pulses instead of restarting while held");
    expect(std::abs(engine.draws().front().x + engine.draws().front().w * 0.5f - 150) < 0.001f,
           "locator follows a moving pointer");
    cursor.set(CursorKind::Item, 42);
    engine.clear_draws();
    cursor.draw(ui, 150, 250, 2.0f, true);
    expect(engine.draws().size() == 4, "all three pulses survive a cursor-kind change");
    engine.clear_draws();
    cursor.draw(ui, 150, 250, 0.1f, true);
    expect(engine.draws().size() == 1, "locator stops after its finite duration");
    engine.clear_draws();
    cursor.draw(ui, 150, 250, 0.1f, true);
    expect(engine.draws().size() == 1, "holding Control does not repeat the effect");
    cursor.draw(ui, 150, 250, 0.1f, false);
    engine.clear_draws();
    cursor.draw(ui, 150, 250, 0.1f, true);
    expect(engine.draws().size() == 2, "releasing and pressing Control retriggers the locator");
    cursor.destroy(engine);
    expect(engine.live_textures() == baseline,
           "destroy releases every cursor atlas and locator texture");
    expect(cursor.load(engine, (root / "config").string()), "cursor can reload after destroy");
    cursor.destroy(engine);
    ConfigArchive::unmount();
    ui.destroy();
    expect(engine.live_textures() == 0, "all test textures are released");
    return openkal::test::g_failures ? 1 : 0;
}
