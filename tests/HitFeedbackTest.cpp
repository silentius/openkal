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
#include <cmath>
#include <fstream>
#include <memory>
#include <vector>

namespace {
    using openkal::test::expect;
    namespace engine = openkal::engine;
    constexpr int kSize = 256;

    std::array<int, 3> pixel(const std::vector<std::uint8_t>& pixels, int x, int y) {
        const auto offset = static_cast<std::size_t>(y * kSize + x) * 4;
        return {pixels[offset], pixels[offset + 1], pixels[offset + 2]};
    }

    void save(const char* prefix, const char* suffix, const std::vector<std::uint8_t>& pixels) {
        if (!prefix) {
            return;
        }
        std::ofstream output(std::string(prefix) + suffix, std::ios::binary);
        output << "P6\n" << kSize << ' ' << kSize << "\n255\n";
        for (int y = 0; y < kSize; ++y) {
            for (int x = 0; x < kSize; ++x) {
                const auto offset = static_cast<std::size_t>(y * kSize + x) * 4;
                output.write(reinterpret_cast<const char*>(pixels.data() + offset), 3);
            }
        }
    }
}  // namespace

int main(int argc, char** argv) {
    if (!glfwInit()) {
        return 77;
    }
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    std::unique_ptr<engine::IEngine> renderer(engine::create_engine());
    if (!renderer->init("Hit feedback regression", kSize, kSize)) {
        glfwTerminate();
        return 1;
    }
    while (glGetError() != GL_NO_ERROR) {
    }
    const char* prefix = argc > 1 ? argv[1] : nullptr;
    const engine::VertexP3T2 quad[] = {{-0.8f, -0.8f, 0, 0, 0},
                                       {0.8f, -0.8f, 0, 1, 0},
                                       {0.8f, 0.8f, 0, 1, 1},
                                       {-0.8f, 0.8f, 0, 0, 1}};
    const std::uint32_t indices[] = {0, 1, 2, 0, 2, 3};
    const auto mesh = renderer->create_mesh(quad, 4, indices, 6);
    // Project-owned dark texture with a transparent left half.
    const std::uint8_t texels[] = {32, 64, 96, 0, 32, 64, 96, 255};
    const auto texture = renderer->create_texture_rgba8(texels, 2, 1, false);
    renderer->set_camera(engine::Mat4::identity(), engine::Mat4::identity());
    auto render_flash = [&](float flash) {
        expect(renderer->begin_offscreen(kSize, kSize, 0, 0, 0), "create flash target");
        renderer->draw_mesh_textured(mesh,
                                     texture,
                                     engine::Mat4::identity(),
                                     false,
                                     true,
                                     true,
                                     false,
                                     0,
                                     0,
                                     1,
                                     1,
                                     1,
                                     1,
                                     flash);
        std::vector<std::uint8_t> image;
        expect(renderer->end_offscreen_rgba(image), "read flash target");
        return image;
    };
    const auto normal = render_flash(0);
    const auto white = render_flash(1);
    const auto halfway = render_flash(0.5f);
    expect(pixel(normal, 208, 128) == std::array<int, 3>{32, 64, 96},
           "normal draw keeps texture color");
    expect(pixel(white, 208, 128) == std::array<int, 3>{255, 255, 255},
           "dark texels flash fully white");
    expect(pixel(white, 48, 128) == std::array<int, 3>{0, 0, 0}, "flash preserves cutout holes");
    const auto middle = pixel(halfway, 208, 128);
    expect(std::abs(middle[0] - 144) <= 1 && std::abs(middle[1] - 160) <= 1 &&
               std::abs(middle[2] - 176) <= 1,
           "flash blends smoothly toward white");
    expect(render_flash(0) == normal, "flash does not leak to subsequent mesh draws");
    save(prefix, "-white.ppm", white);

    const std::uint8_t translucent[] = {0, 0, 0, 128};
    const auto transparent = renderer->create_texture_rgba8(translucent, 1, 1, false);
    expect(renderer->begin_offscreen(kSize, kSize, 0, 0, 0), "create opacity target");
    renderer->draw_mesh_textured(mesh,
                                 transparent,
                                 engine::Mat4::identity(),
                                 true,
                                 true,
                                 false,
                                 false,
                                 0,
                                 0,
                                 1,
                                 1,
                                 1,
                                 0.5f,
                                 1);
    std::vector<std::uint8_t> image;
    expect(renderer->end_offscreen_rgba(image), "read opacity target");
    expect(std::abs(pixel(image, 128, 128)[0] - 64) <= 1,
           "flash preserves texture and corpse opacity");

    const auto identity = engine::Mat4::identity();
    const engine::Vec3 minimum{-0.8f, -0.8f, 0.0f};
    const engine::Vec3 maximum{0.8f, 0.8f, 0.0f};
    auto draw_outline = [&](float radius, float opacity) {
        const bool active =
            renderer->begin_mesh_outline(identity, minimum, maximum, radius, opacity);
        expect(active == (opacity > 0.0f), "outline pass starts only for a visible contour");
        if (active) {
            renderer->draw_mesh_outline_part(mesh, texture, true, 128.0f / 255.0f);
            renderer->end_mesh_outline();
        }
    };
    auto render_outline = [&](float radius, float opacity, bool occluded = false) {
        expect(renderer->begin_offscreen(kSize, kSize, 0, 0, 0), "create outline target");
        renderer->draw_mesh_textured(mesh, texture, identity, false, true, true);
        if (occluded) {
            // The foreground covers exactly the model. Even the expanded border must stay hidden.
            renderer->draw_mesh_textured(
                mesh, texture, engine::Mat4::translation(0, 0, -0.1f), false, true, true);
        }
        draw_outline(radius, opacity);
        std::vector<std::uint8_t> result;
        expect(renderer->end_offscreen_rgba(result), "read outline target");
        return result;
    };
    const auto start_outline = render_outline(1.0f, 0.85f);
    const auto expanded = render_outline(4.0f, 0.5f);
    const auto fading = render_outline(4.0f, 0.2f);
    const auto expired = render_outline(5.0f, 0.0f);
    expect(expired == normal, "expired contour leaves the normal image intact");
    expect(pixel(start_outline, 208, 128) == pixel(normal, 208, 128),
           "contour leaves the model interior and its texture color intact");
    expect(pixel(start_outline, 48, 128) == pixel(normal, 48, 128),
           "mask follows the cutout texture");
    expect(pixel(start_outline, 127, 128)[0] > 200 && pixel(start_outline, 230, 128)[0] > 200 &&
               pixel(start_outline, 188, 25)[0] > 200 && pixel(start_outline, 188, 230)[0] > 200,
           "a thin white line appears on all sides of the visible silhouette");
    expect(pixel(start_outline, 233, 128)[0] == 0 && pixel(expanded, 233, 128)[0] > 120 &&
               pixel(expanded, 124, 128)[0] > 120 && pixel(expanded, 188, 233)[0] > 120 &&
               pixel(expanded, 188, 22)[0] > 120 && pixel(expanded, 230, 128)[0] == 0,
           "the thin line travels outward by a few pixels in every direction");
    expect(pixel(fading, 233, 128)[0] > 40 &&
               pixel(fading, 233, 128)[0] < pixel(expanded, 233, 128)[0],
           "the expanding line fades");
    expect(render_outline(4.0f, 0.5f, true) == render_outline(4.0f, 0.0f, true),
           "fully occluded monsters leave no visible outline");
    save(prefix, "-outline-start.ppm", start_outline);
    save(prefix, "-outline-expanded.ppm", expanded);
    save(prefix, "-outline-fading.ppm", fading);
    save(prefix, "-outline-expired.ppm", expired);

    for (float bloom : {0.0f, 0.5f}) {
        renderer->set_bloom_strength(bloom);
        renderer->begin_frame(0, 0, 0);
        renderer->draw_mesh_textured(mesh, texture, identity, false, true, true);
        draw_outline(1.0f, 1.0f);
        std::array<GLint, 4> viewport{};
        glGetIntegerv(GL_VIEWPORT, viewport.data());
        const int edge_x = static_cast<int>(std::ceil(viewport[2] * 0.9f - 0.5f));
        std::array<std::uint8_t, 4> edge{};
        glReadPixels(edge_x, viewport[3] / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, edge.data());
        expect(edge[0] > 245, "outline composites to the world with or without bloom");
        renderer->end_frame();
    }

    expect(glGetError() == GL_NO_ERROR, "feedback rendering has no OpenGL errors");
    renderer->destroy_texture(transparent);
    renderer->destroy_texture(texture);
    renderer->destroy_mesh(mesh);
    renderer->shutdown();
    return openkal::test::g_failures ? 1 : 0;
}
