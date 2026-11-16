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
#include <cmath>
#include <limits>
#include <memory>

namespace {
    namespace engine = openkal::engine;
    using openkal::test::expect;

    std::vector<std::uint8_t> capture(engine::IEngine& renderer,
                                      engine::MeshHandle mesh,
                                      engine::TextureHandle texture,
                                      const engine::Vec3& eye = {0, 0, 3},
                                      bool blend = false) {
        renderer.begin_frame(0, 0, 0);
        expect(renderer.begin_offscreen(64, 64, 0, 0, 0), "create material framebuffer");
        renderer.set_camera(engine::Mat4::look_at(eye, {0, 0, 0}, {0, 1, 0}),
                            engine::Mat4::perspective(0.7f, 1, 0.1f, 20));
        renderer.draw_mesh_textured(mesh, texture, engine::Mat4::identity(), blend, true);
        std::vector<std::uint8_t> image;
        expect(renderer.end_offscreen_rgba(image), "read material framebuffer");
        expect(glGetError() == GL_NO_ERROR, "material draw has no GL errors");
        return image;
    }

    int center(const std::vector<std::uint8_t>& image, int channel = 0) {
        return image[(32 * 64 + 32) * 4 + channel];
    }
}  // namespace

int main() {
    if (!glfwInit()) {
        return 77;
    }
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    std::unique_ptr<engine::IEngine> renderer(engine::create_engine());
    if (!renderer->init("Material regression", 64, 64)) {
        return 1;
    }
    while (glGetError() != GL_NO_ERROR) {
    }
    const engine::VertexP3T2 vertices[] = {
        {-1, -1, 0, 0, 0}, {1, -1, 0, 1, 0}, {1, 1, 0, 1, 1}, {-1, 1, 0, 0, 1}};
    const std::uint32_t indices[] = {0, 2, 1, 0, 3, 2};
    const auto mesh = renderer->create_mesh(vertices, 4, indices, 6);
    const auto plain = renderer->create_mesh(vertices, 4, indices, 6);
    const std::uint8_t pixels[] = {40, 60, 80, 255};
    const auto texture = renderer->create_texture_rgba8(pixels, 1, 1);
    const auto baseline = capture(*renderer, mesh, texture);
    // A normal half-way between the public Phong light direction and the eye.
    const float light_length = std::sqrt(0.35f * 0.35f + 0.8f * 0.8f + 0.5f * 0.5f);
    const std::array<float, 3> halfway{
        -0.35f / light_length, 0.8f / light_length, 1.0f - 0.5f / light_length};
    const std::vector<std::array<float, 3>> normals(4, halfway);
    renderer->set_mesh_specular(mesh, normals, 24);
    const auto lit = capture(*renderer, mesh, texture);
    expect(center(lit) > center(baseline) + 100, "stored normals produce a visible highlight");
    expect(center(lit, 3) == center(baseline, 3), "highlight preserves alpha");
    const auto side = capture(*renderer, mesh, texture, {2, 0, 3});
    expect(center(side) + 30 < center(lit), "highlight responds to the viewing direction");
    renderer->set_mesh_specular(mesh, normals, 3);
    const auto broad = capture(*renderer, mesh, texture, {1, 0, 3});
    renderer->set_mesh_specular(mesh, normals, 80);
    const auto narrow = capture(*renderer, mesh, texture, {1, 0, 3});
    expect(center(broad) > center(narrow) + 30, "stored exponent controls highlight width");
    expect(capture(*renderer, plain, texture) == baseline, "gloss does not leak into plain meshes");
    renderer->set_mesh_specular(mesh, std::vector<std::array<float, 3>>(4, {0, 0, 1}), 24);
    expect(capture(*renderer, mesh, texture) == baseline, "back-lit normals suppress highlights");
    renderer->set_mesh_specular(mesh, normals, std::numeric_limits<float>::quiet_NaN());
    expect(capture(*renderer, mesh, texture) == baseline, "invalid power disables highlights");
    renderer->set_mesh_specular(mesh, {{0, 1, 0}}, 24);
    expect(capture(*renderer, mesh, texture) == baseline,
           "normal count mismatch disables highlights");
    renderer->set_mesh_specular(mesh, normals, 24);
    const auto blended = capture(*renderer, mesh, texture, {0, 0, 3}, true);
    expect(center(blended) > center(baseline) + 100, "gloss works in the alpha pass");

    const engine::VertexP3T2 layers[] = {{-1, -1, 0, 0, 0},
                                         {1, -1, 0, 0, 0},
                                         {0, 1, 0, 0, 0},
                                         {-1, -1, -0.25f, 1, 0},
                                         {1, -1, -0.25f, 1, 0},
                                         {0, 1, -0.25f, 1, 0}};
    const std::uint32_t near_first[] = {0, 2, 1, 3, 5, 4};
    const std::uint32_t far_first[] = {3, 5, 4, 0, 2, 1};
    const std::uint8_t colors[] = {200, 0, 0, 128, 0, 200, 0, 128};
    const auto layer_texture = renderer->create_texture_rgba8(colors, 2, 1, false, true);
    const auto a = renderer->create_mesh(layers, 6, near_first, 6);
    const auto b = renderer->create_mesh(layers, 6, far_first, 6);
    // Zero normals isolate ordering while retaining the glossy mesh path.
    const std::vector<std::array<float, 3>> layer_normals(6);
    renderer->set_mesh_specular(a, layer_normals, 24);
    renderer->set_mesh_specular(b, layer_normals, 24);
    const auto ordered_a = capture(*renderer, a, layer_texture, {0, 0, 3}, true);
    const auto ordered_b = capture(*renderer, b, layer_texture, {0, 0, 3}, true);
    expect(ordered_a == ordered_b && center(ordered_a) > center(ordered_a, 1),
           "glossy transparent triangles retain back-to-front ordering and UVs");
    renderer->destroy_texture(layer_texture);
    renderer->destroy_mesh(a);
    renderer->destroy_mesh(b);
    renderer->destroy_texture(texture);
    renderer->destroy_mesh(mesh);
    renderer->destroy_mesh(plain);
    renderer->shutdown();
    return openkal::test::g_failures ? 1 : 0;
}
