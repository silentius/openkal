/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/ExpEventBoard.h"
#include "openkal/engine/engine.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

namespace {
    bool capture(openkal::engine::IEngine& engine, const std::string& path) {
        std::vector<std::uint8_t> rgba;
        if (!engine.end_offscreen_rgba(rgba) || rgba.size() != 1400 * 1000 * 4) {
            return false;
        }
        std::ofstream out(path, std::ios::binary);
        out << "P6\n1400 1000\n255\n";
        for (std::size_t i = 0; i < rgba.size(); i += 4) {
            out.write(reinterpret_cast<const char*>(rgba.data() + i), 3);
        }
        return out.good();
    }
}  // namespace

int main(int argc, char** argv) {
    std::string capture_path;
    bool active = false;
    bool back = false;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--active") {
            active = true;
        } else if (arg == "--back") {
            back = true;
        } else if (arg == "--capture" && i + 1 < argc) {
            capture_path = argv[++i];
        } else {
            std::cout
                << "event_board_view [--active] [--back] [--capture output.ppm]\n"
                   "Drag to orbit, wheel to zoom, 1 collecting, 2 active, 3 empty, Esc exit.\n"
                   "The preview uses sample data; the game uses server snapshots.\n";
            return arg == "--help" ? 0 : 1;
        }
    }
    using namespace openkal;
    std::unique_ptr<engine::IEngine> renderer(engine::create_engine());
    if (!renderer || !renderer->init("OpenKal | Community noticeboard - sample data", 1400, 1000)) {
        return 1;
    }
    int result = 0;
    {
        client::ExpEventBoard board(*renderer);
        client::FreetypeFont captions;
        if (!captions.load(*renderer, client::FreetypeFont::find_system_sans(), 24)) {
            renderer->shutdown();
            return 1;
        }
        protocol::ExpEventStateFields state;
        state.enabled = true;
        state.manager_npc_index = 5037;
        state.donation_item_index = 23016;
        state.contributed = 367;
        state.target = 500;
        state.next_bonus_percent = 10;
        float yaw = back ? 3.46f : 0.31f;
        float pitch = 0.08f;
        float distance = 540;
        const auto started = std::chrono::steady_clock::now();
        const engine::VertexP3T2 ground_vertices[]{
            {-350, 0, -260, 0, 0}, {350, 0, -260, 1, 0}, {350, 0, 260, 1, 1}, {-350, 0, 260, 0, 1}};
        const std::uint32_t ground_indices[]{0, 1, 2, 0, 2, 3};
        const auto ground = renderer->create_mesh(ground_vertices, 4, ground_indices, 6);
        std::vector<std::uint8_t> ground_pixels(256 * 256 * 4);
        for (int y = 0; y < 256; ++y) {
            for (int x = 0; x < 256; ++x) {
                const float radius =
                    std::sqrt(std::pow((x - 128) / 128.0f, 2) + std::pow((y - 128) / 128.0f, 2));
                const float shade = std::max(0.0f, 1 - radius) * 22;
                const std::size_t at = static_cast<std::size_t>(y * 256 + x) * 4;
                ground_pixels[at] = static_cast<std::uint8_t>(18 + shade);
                ground_pixels[at + 1] = static_cast<std::uint8_t>(25 + shade);
                ground_pixels[at + 2] = static_cast<std::uint8_t>(28 + shade);
                ground_pixels[at + 3] = 255;
            }
        }
        const auto ground_texture = renderer->create_texture_rgba8(ground_pixels.data(), 256, 256);
        engine::FrameInput input;
        while (renderer->poll(input) && !input.quit && !input.key_escape) {
            if (input.key_digit_pressed[1]) {
                active = false;
                state.contributed = 367;
            }
            if (input.key_digit_pressed[2]) {
                active = true;
            }
            if (input.key_digit_pressed[3]) {
                active = false;
                state.contributed = 0;
            }
            if (input.mouse_left) {
                yaw += input.mouse_dx * 0.006f;
                pitch = std::clamp(pitch + input.mouse_dy * 0.004f, -0.1f, 0.8f);
            }
            distance = std::clamp(distance - input.mouse_scroll * 25, 300.0f, 1000.0f);
            const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
                                     std::chrono::steady_clock::now() - started)
                                     .count();
            state.active_bonus_percent = active ? 10 : 0;
            state.next_bonus_percent = active ? 20 : 10;
            state.remaining_seconds =
                active ? static_cast<std::uint32_t>(3600 - seconds % 3600) : 0;
            if (active) {
                state.contributed = 0;
            }
            if (!board.update(state, "Exp Event Coins")) {
                result = 1;
                break;
            }
            const int width = capture_path.empty() ? input.width : 1400;
            const int height = capture_path.empty() ? input.height : 1000;
            if (width <= 0 || height <= 0) {
                continue;
            }
            if (!capture_path.empty() &&
                !renderer->begin_offscreen(width, height, 0.07f, 0.098f, 0.11f)) {
                result = 1;
                break;
            }
            renderer->begin_frame(0.07f, 0.098f, 0.11f);
            const engine::Vec3 target{0, 138, 0};
            const engine::Vec3 eye{std::sin(yaw) * distance * std::cos(pitch),
                                   138 + std::sin(pitch) * distance,
                                   std::cos(yaw) * distance * std::cos(pitch)};
            const auto view = engine::Mat4::multiply(engine::Mat4::scaling(-1, 1, 1),
                                                     engine::Mat4::look_at(eye, target, {0, 1, 0}));
            renderer->set_camera(
                view,
                engine::Mat4::perspective(0.68f, static_cast<float>(width) / height, 1, 3000));
            renderer->draw_mesh_textured(
                ground, ground_texture, engine::Mat4::identity(), false, true);
            board.draw(engine::Mat4::identity());
            renderer->begin_ui_pass();
            captions.draw(*renderer,
                          42,
                          30,
                          "O P E N K A L   /   WORLD OBJECT STUDY",
                          0.72f,
                          0.78f,
                          0.75f,
                          0.72f);
            captions.draw(*renderer, 42, 57, "The Community Board", 0.94f, 0.86f, 0.67f, 1.15f);
            captions.draw(*renderer,
                          42,
                          static_cast<float>(height) - 58,
                          "Procedural model  /  Live FreeType surface  /  Sample event data",
                          0.61f,
                          0.69f,
                          0.67f,
                          0.72f);
            captions.draw(*renderer,
                          42,
                          static_cast<float>(height) - 33,
                          "DRAG orbit     WHEEL zoom     1 collecting     2 active     3 empty",
                          0.47f,
                          0.57f,
                          0.56f,
                          0.63f);
            if (!capture_path.empty()) {
                result = capture(*renderer, capture_path) ? 0 : 1;
                break;
            }
            renderer->end_frame();
        }
        renderer->destroy_mesh(ground);
        renderer->destroy_texture(ground_texture);
        captions.destroy(*renderer);
    }
    renderer->shutdown();
    return result;
}
