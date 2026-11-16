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
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <vector>

namespace {
    using openkal::test::expect;
    namespace engine = openkal::engine;

    struct ShadowDraw {
        engine::MeshHandle mesh;
        engine::TextureHandle texture;
        float opacity = 0.34f;
        float offset_x = 0.0f;
    };

    std::vector<std::uint8_t> render(engine::IEngine& renderer,
                                     const std::vector<ShadowDraw>& draws,
                                     int size = 64,
                                     float depth = 1.0f,
                                     const engine::ShadowReceiver& receiver = {}) {
        expect(renderer.begin_offscreen(size, size, 1.0f, 1.0f, 1.0f), "create offscreen target");
        glClearDepth(depth);
        glClear(GL_DEPTH_BUFFER_BIT);
        glClearDepth(1.0);
        renderer.begin_shadow_pass();
        for (const auto& draw : draws) {
            renderer.draw_mesh_shadow(draw.mesh,
                                      draw.texture,
                                      engine::Mat4::translation(draw.offset_x, 0.0f, 0.0f),
                                      receiver,
                                      0.0f,
                                      true,
                                      draw.opacity);
        }
        renderer.end_shadow_pass();
        float scene_depth = 0.0f;
        glReadPixels(size / 2, size / 2, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &scene_depth);
        expect(std::fabs(scene_depth - depth) < 0.00001f, "shadow pass preserves scene depth");
        std::vector<std::uint8_t> image;
        expect(renderer.end_offscreen_rgba(image), "read shadow image");
        expect(glGetError() == GL_NO_ERROR, "shadow pass has no OpenGL errors");
        return image;
    }

    int center_red(const std::vector<std::uint8_t>& image, int size = 64) {
        return image[(static_cast<std::size_t>(size / 2) * size + size / 2) * 4];
    }
}  // namespace

int main() {
    if (!glfwInit()) {
        std::fprintf(stderr, "SKIP shadow mask test: no GLFW display available\n");
        return 77;
    }
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    std::unique_ptr<engine::IEngine> renderer(engine::create_engine());
    if (!renderer->init("Shadow mask regression", 64, 64)) {
        glfwTerminate();
        return 1;
    }
    // GLEW can leave GL_INVALID_ENUM after probing a core-profile context.
    while (glGetError() != GL_NO_ERROR) {
    }

    // Look down onto X/Z with a depth range that includes the receiver and caster.
    engine::Mat4 projection;
    projection.m = {0.25f, 0, 0, 0, 0, 0, -0.0625f, 0, 0, 0.25f, 0, 0, 0, 0, 0, 1};
    renderer->set_camera(engine::Mat4::identity(), projection);
    const engine::VertexP3T2 vertices[] = {
        {-3, 2, -3, 0, 0}, {-1, 2, -3, 1, 0}, {-1, 2, -1, 1, 1}, {-3, 2, -1, 0, 1}};
    const std::uint32_t indices[] = {0, 1, 2, 0, 2, 3, 0, 1, 2, 0, 2, 3};
    const auto mesh = renderer->create_mesh(vertices, 4, indices, 6);
    const auto duplicate_mesh = renderer->create_mesh(vertices, 4, indices, 12);
    const std::uint8_t opaque[] = {255, 255, 255, 255};
    const std::uint8_t transparent[] = {255, 255, 255, 0};
    const std::uint8_t translucent[] = {255, 255, 255, 128};
    const auto texture = renderer->create_texture_rgba8(opaque, 1, 1, false);
    const auto cutout = renderer->create_texture_rgba8(transparent, 1, 1, false);
    const auto partial = renderer->create_texture_rgba8(translucent, 1, 1, false);
    const ShadowDraw draw{mesh, texture};

    const auto empty = render(*renderer, {});
    const auto single = render(*renderer, {draw});
    expect(center_red(single) >= 168 && center_red(single) <= 172,
           "single silhouette applies configured opacity once");
    expect(single == render(*renderer, {draw, draw, draw, draw}),
           "overlapping body parts do not accumulate darkness");
    expect(single == render(*renderer, {{duplicate_mesh, texture}}),
           "self-overlapping triangles do not accumulate darkness");

    const ShadowDraw weak{mesh, texture, 0.26f, 0.5f};
    const auto overlap = render(*renderer, {draw, weak});
    expect(overlap == render(*renderer, {weak, draw}), "mask is independent of draw order");
    expect(center_red(overlap) == center_red(single), "overlap keeps strongest opacity");
    expect(overlap != single, "non-overlapping silhouette extends the shared mask");
    expect(empty == render(*renderer, {{mesh, cutout}}), "transparent texels cast no shadow");
    const auto partial_image = render(*renderer, {{mesh, partial}});
    expect(center_red(partial_image) > center_red(single) && center_red(partial_image) < 255,
           "cutout alpha preserves partial coverage");
    expect(single == render(*renderer, {{mesh, partial}, draw}),
           "opaque coverage replaces weaker alpha coverage");
    expect(single == render(*renderer, {draw, {mesh, partial}}),
           "weaker alpha coverage cannot darken opaque coverage");
    expect(empty == render(*renderer, {draw}, 64, 0.1f), "scene depth occludes projected shadows");
    expect(empty == render(*renderer, {}), "each pass clears the previous mask");
    expect(center_red(render(*renderer, {draw}, 96), 96) == center_red(single),
           "mask resizes with render target");
    expect(single == render(*renderer, {draw}), "mask survives resizing back");

    const float heights[] = {0, 0, 0, 0};
    const auto height_map = renderer->create_texture_r32f(heights, 2, 2);
    engine::ShadowReceiver receiver;
    receiver.height_map = height_map;
    receiver.origin_x = -4;
    receiver.origin_z = -4;
    receiver.world_size = 8;
    expect(single == render(*renderer, {draw}, 64, 1.0f, receiver),
           "terrain receiver participates in the shared mask");

    // Cover both the default framebuffer and the bloom scene framebuffer.
    for (const float bloom : {0.0f, 0.5f}) {
        renderer->set_bloom_strength(bloom);
        renderer->begin_frame(1, 1, 1);
        renderer->begin_shadow_pass();
        renderer->draw_mesh_shadow(mesh, texture, engine::Mat4::identity(), {}, 0, true, 0.34f);
        renderer->draw_mesh_shadow(mesh, texture, engine::Mat4::identity(), {}, 0, true, 0.34f);
        renderer->end_shadow_pass();
        std::array<std::uint8_t, 4> pixel{};
        GLint viewport[4]{};
        glGetIntegerv(GL_VIEWPORT, viewport);
        glReadPixels(
            viewport[2] / 2, viewport[3] / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
        expect(std::abs(static_cast<int>(pixel[0]) - center_red(single)) <= 1,
               "window and bloom targets receive one composite");
        renderer->begin_ui_pass();
        renderer->draw_ui_rect(0, 0, 8, 8, 1, 0, 0, 0.5f);
        expect(glGetError() == GL_NO_ERROR, "subsequent scene composition and UI remain valid");
    }

    renderer->destroy_texture(height_map);
    renderer->destroy_texture(partial);
    renderer->destroy_texture(cutout);
    renderer->destroy_texture(texture);
    renderer->destroy_mesh(duplicate_mesh);
    renderer->destroy_mesh(mesh);
    renderer->shutdown();
    return openkal::test::g_failures ? 1 : 0;
}
