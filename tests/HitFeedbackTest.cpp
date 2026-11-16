/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/HitSparks.h"
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

    float centroid_x(const std::vector<std::uint8_t>& pixels) {
        double total = 0;
        double weighted = 0;
        bool neutral = true;
        for (int y = 0; y < kSize; ++y) {
            for (int x = 0; x < kSize; ++x) {
                const auto c = pixel(pixels, x, y);
                neutral = neutral && c[0] == c[1] && c[1] == c[2];
                total += c[0];
                weighted += c[0] * x;
            }
        }
        expect(neutral, "spark pixels stay neutral white");
        return total > 0 ? static_cast<float>(weighted / total) : -1.0f;
    }

    void save(const char* prefix, const char* suffix, const std::vector<std::uint8_t>& pixels) {
        if (!prefix) {
            return;
        }
        std::ofstream output(std::string(prefix) + suffix, std::ios::binary);
        output << "P6\n" << kSize << ' ' << kSize << "\n255\n";
        for (int y = kSize - 1; y >= 0; --y) {
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

    openkal::client::HitSparks sparks;
    expect(sparks.init(*renderer), "initialize sparks without game data");
    auto projection = engine::Mat4::scaling(0.08f, 0.08f, 0.01f);
    renderer->set_camera(engine::Mat4::identity(), projection);
    auto render_sparks = [&](bool occluded = false) {
        expect(renderer->begin_offscreen(kSize, kSize, 0, 0, 0), "create spark target");
        if (occluded) {
            auto model = engine::Mat4::scaling(20, 20, 1);
            model.m[14] = -30;
            renderer->draw_mesh_textured(
                mesh, texture, model, false, true, false, false, 0, 0, 0, 0, 0, 1);
        }
        sparks.draw(*renderer, {0, 0, 20});
        std::vector<std::uint8_t> result;
        expect(renderer->end_offscreen_rgba(result), "read spark target");
        return result;
    };
    sparks.spawn({0, 0, 0}, {0, 0, 0}, 1, false);
    expect(sparks.particle_count() == 0,
           "coincident attacker does not emit an arbitrary direction");
    sparks.spawn({0, 0, 0}, {-30, 0, 0}, 1, false);
    expect(sparks.particle_count() == 6, "normal hit emits six sparks");
    const auto first = render_sparks();
    expect(std::fabs(centroid_x(first) - (kSize - 1) * 0.5f) < 1.0f,
           "burst starts at the supplied monster center without a surface offset");
    expect(centroid_x(render_sparks(true)) < 0, "scene geometry occludes sparks");
    sparks.update(0.06f);
    const auto moved = render_sparks();
    const float first_x = centroid_x(first);
    const float moved_x = centroid_x(moved);
    expect(first_x > 0 && moved_x > 0 && moved_x > first_x + 5,
           "rendered sparks carry the impulse away from the attacker");
    save(prefix, "-sparks.ppm", moved);
    sparks.update(0.05f);
    expect(sparks.particle_count() == 0, "compact burst expires within 110 ms");
    expect(centroid_x(render_sparks()) < 0, "expired burst leaves no rendered particles");
    for (int i = 0; i < 100; ++i) {
        sparks.spawn({0, 0, 0}, {30, 0, 0}, 1, true);
    }
    expect(sparks.particle_count() == 256, "many simultaneous hits respect global spark budget");
    renderer->set_bloom_strength(0);
    renderer->begin_frame(0, 0, 0);
    sparks.draw(*renderer, {0, 0, 20});
    renderer->end_frame();
    expect(renderer->frame_stats().mesh == 1, "all 256 sparks use one draw call");
    sparks.update(1);
    expect(sparks.particle_count() == 0, "budget recovers after particles expire");
    sparks.spawn({0, 0, 0}, {30, 0, 0}, 1, true);
    expect(sparks.particle_count() == 8, "critical hit emits eight sparks");
    sparks.update(0.04f);
    expect(centroid_x(render_sparks()) < kSize / 2, "opposite attacker mirrors burst direction");
    sparks.update(1);
    // Project-owned synthetic torso, viewed at game-like perspective distances.
    const engine::VertexP3T2 body_vertices[] = {{-6, 0, -5, 0, 0},
                                                {6, 0, -5, 1, 0},
                                                {6, 18, -5, 1, 1},
                                                {-6, 18, -5, 0, 1},
                                                {-6, 0, 5, 0, 0},
                                                {6, 0, 5, 1, 0},
                                                {6, 18, 5, 1, 1},
                                                {-6, 18, 5, 0, 1}};
    const std::uint32_t body_indices[] = {0, 1, 2, 0, 2, 3, 4, 6, 5, 4, 7, 6, 0, 4, 5, 0, 5, 1,
                                          3, 2, 6, 3, 6, 7, 0, 3, 7, 0, 7, 4, 1, 5, 6, 1, 6, 2};
    const auto body = renderer->create_mesh(body_vertices, 8, body_indices, 36);
    const std::uint8_t body_color[] = {55, 55, 55, 255};
    const auto body_texture = renderer->create_texture_rgba8(body_color, 1, 1, false);
    auto render_body = [&](const engine::Vec3& eye) {
        expect(renderer->begin_offscreen(kSize, kSize, 0.12f, 0.12f, 0.12f),
               "create perspective impulse target");
        renderer->set_camera(engine::Mat4::look_at(eye, {0, 9, 0}, {0, 1, 0}),
                             engine::Mat4::perspective(0.872665f, 1, 0.1f, 500));
        renderer->draw_mesh_textured(body, body_texture, engine::Mat4::identity(), false, true);
        sparks.draw(*renderer, eye);
        std::vector<std::uint8_t> result;
        expect(renderer->end_offscreen_rgba(result), "read perspective impulse target");
        return result;
    };
    const engine::Vec3 front_eye{0, 28, -65};
    const engine::Vec3 side_eye{-65, 28, 0};
    const auto front_body = render_body(front_eye);
    const auto side_body = render_body(side_eye);
    sparks.spawn({0, 9, 0}, {0, 9, -30}, 1, false);
    sparks.update(0.06f);
    const auto front_impulse = render_body(front_eye);
    const auto side_impulse = render_body(side_eye);
    auto visible_sparks = [&](const std::vector<std::uint8_t>& scene,
                              const std::vector<std::uint8_t>& background) {
        int count = 0;
        for (std::size_t i = 0; i < scene.size(); i += 4) {
            if (scene[i] > background[i] + 20) {
                ++count;
            }
        }
        return count;
    };
    expect(visible_sparks(front_impulse, front_body) >= 20,
           "outgoing streaks remain visible around a solid torso from the attacker's side");
    expect(visible_sparks(side_impulse, side_body) >= 20,
           "outgoing streaks remain visible around a solid torso from the side");
    save(prefix, "-impulse-front.ppm", front_impulse);
    save(prefix, "-impulse-side.ppm", side_impulse);
    renderer->destroy_texture(body_texture);
    renderer->destroy_mesh(body);
    sparks.destroy(*renderer);
    expect(sparks.particle_count() == 0, "destroy clears particles");
    expect(glGetError() == GL_NO_ERROR, "feedback rendering has no OpenGL errors");
    renderer->destroy_texture(transparent);
    renderer->destroy_texture(texture);
    renderer->destroy_mesh(mesh);
    renderer->shutdown();
    return openkal::test::g_failures ? 1 : 0;
}
