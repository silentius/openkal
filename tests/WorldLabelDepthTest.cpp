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
#include "test_util.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <array>
#include <memory>
#include <vector>

namespace {
    using openkal::test::expect;
    namespace engine = openkal::engine;

    bool green(const std::vector<std::uint8_t>& pixels, int size, int x, int y) {
        const auto offset = static_cast<std::size_t>(y * size + x) * 4;
        return pixels[offset] < 20 && pixels[offset + 1] > 200;
    }

    void verify(engine::IEngine& renderer,
                engine::MeshHandle mesh,
                engine::TextureHandle texture,
                int size,
                bool offscreen,
                float bloom) {
        renderer.set_bloom_strength(bloom);
        for (bool draw_occluder : {true, false}) {
            if (offscreen) {
                expect(renderer.begin_offscreen(size, size, 0, 0, 0), "create depth test target");
            }
            renderer.begin_frame(0, 0, 0);
            // A translated camera catches confusion between world and camera-relative depth.
            renderer.set_camera(engine::Mat4::translation(0, 0, -10), engine::Mat4::identity());
            if (draw_occluder) {
                renderer.draw_mesh_textured(
                    mesh, texture, engine::Mat4::translation(0, 0, 10), false, true, true);
            }
            renderer.begin_ui_pass();
            const engine::Vec3 behind{0, 0, 10.5f};
            renderer.set_ui_world_anchor(&behind);
            renderer.draw_ui_rect(
                0, 0, static_cast<float>(size), static_cast<float>(size), 0, 1, 0, 1);
            const engine::Vec3 in_front{0, 0, 9.5f};
            renderer.set_ui_world_anchor(&in_front);
            renderer.draw_ui_rect(size * 0.6f, size * 0.1f, size * 0.3f, size * 0.2f, 0, 1, 0, 1);
            renderer.set_ui_world_anchor(nullptr);
            renderer.draw_ui_rect(size * 0.6f, size * 0.7f, size * 0.3f, size * 0.2f, 0, 1, 0, 1);
            // Re-entering the pass must retain the pre-bloom depth source.
            renderer.begin_ui_pass();
            std::vector<std::uint8_t> pixels;
            if (offscreen) {
                expect(renderer.end_offscreen_rgba(pixels), "read depth test target");
            } else {
                pixels.resize(static_cast<std::size_t>(size * size * 4));
                glReadPixels(0, 0, size, size, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            }
            expect(green(pixels, size, size / 4, size / 2), "cutout hole preserves visible label");
            expect(green(pixels, size, size * 3 / 4, size / 2) == !draw_occluder,
                   "opaque surface hides label; next frame without surface restores it");
            expect(green(pixels, size, size * 3 / 4, size / 5),
                   "foreground label or ordinary UI stays visible");
            expect(green(pixels, size, size * 3 / 4, size * 4 / 5),
                   "ordinary UI or foreground label stays visible");
            expect(glGetError() == GL_NO_ERROR, "world label rendering has no GL errors");
        }
    }
}  // namespace

int main() {
    if (!glfwInit()) {
        std::fprintf(stderr, "SKIP world label depth test: no GLFW display available\n");
        return 77;
    }
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    std::unique_ptr<engine::IEngine> renderer(engine::create_engine());
    if (!renderer->init("World label depth regression", 64, 64)) {
        return 1;
    }
    while (glGetError() != GL_NO_ERROR) {
    }
    // Synthetic cutout: empty on the left, opaque white on the right.
    const std::array<std::uint8_t, 8> rgba{255, 255, 255, 0, 255, 255, 255, 255};
    const auto texture = renderer->create_texture_rgba8(rgba.data(), 2, 1, false, true);
    const engine::VertexP3T2 vertices[] = {
        {-1, -1, 0, 0, 0}, {1, -1, 0, 1, 0}, {1, 1, 0, 1, 1}, {-1, 1, 0, 0, 1}};
    const std::uint32_t indices[] = {0, 1, 2, 0, 2, 3};
    const auto mesh = renderer->create_mesh(vertices, 4, indices, 6);
    verify(*renderer, mesh, texture, 64, true, 0);
    verify(*renderer, mesh, texture, 96, true, 0);
    verify(*renderer, mesh, texture, 64, false, 0);
    expect(renderer->render_capabilities().bloom, "exercise bloom depth source");
    verify(*renderer, mesh, texture, 64, false, 0.5f);
    renderer->destroy_mesh(mesh);
    renderer->destroy_texture(texture);
    renderer->shutdown();
    return openkal::test::g_failures == 0 ? 0 : 1;
}
