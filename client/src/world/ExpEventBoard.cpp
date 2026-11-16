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

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace openkal::client {
    namespace {
        constexpr float kPi = 3.14159265359f;
        constexpr int kMaterialWidth = 1024;
        constexpr int kMaterialHeight = 512;
        constexpr int kSurfaceWidth = 1024;
        constexpr int kSurfaceHeight = 640;
        using engine::Vec3;

        struct Geometry {
            std::vector<engine::VertexP3T2> vertices;
            std::vector<std::uint32_t> indices;

            void quad(Vec3 a, Vec3 b, Vec3 c, Vec3 d, int shade = 0, bool horizontal = false) {
                const auto first = static_cast<std::uint32_t>(vertices.size());
                const float u0 = (static_cast<float>(shade) + 0.02f) / 4.0f;
                const float u1 = (static_cast<float>(shade) + 0.98f) / 4.0f;
                // Keep the grain running along each beam instead of across its short edge.
                vertices.insert(vertices.end(),
                                {{a.x, a.y, a.z, u0, 1},
                                 {b.x, b.y, b.z, horizontal ? u0 : u1, horizontal ? 0.0f : 1.0f},
                                 {c.x, c.y, c.z, u1, 0},
                                 {d.x, d.y, d.z, horizontal ? u1 : u0, horizontal ? 1.0f : 0.0f}});
                indices.insert(indices.end(),
                               {first, first + 1, first + 2, first, first + 2, first + 3});
            }

            void box(float x, float y, float z, float w, float h, float depth) {
                const float l = x - w / 2, r = x + w / 2, b = y - h / 2, t = y + h / 2,
                            f = z + depth / 2, a = z - depth / 2;
                quad({l, b, f}, {r, b, f}, {r, t, f}, {l, t, f}, 0, w > h);
                quad({r, b, a}, {l, b, a}, {l, t, a}, {r, t, a}, 2, w > h);
                quad({r, b, f}, {r, b, a}, {r, t, a}, {r, t, f}, 1, depth > h);
                quad({l, b, a}, {l, b, f}, {l, t, f}, {l, t, a}, 2, depth > h);
                quad({l, t, f}, {r, t, f}, {r, t, a}, {l, t, a}, 3, w > depth);
                quad({l, b, a}, {r, b, a}, {r, b, f}, {l, b, f}, 2, w > depth);
            }

            void medallion(float x, float y, float z, float radius, float depth) {
                for (int i = 0; i < 12; ++i) {
                    const float a = static_cast<float>(i) * kPi / 6;
                    const float b = static_cast<float>(i + 1) * kPi / 6;
                    const Vec3 p{x + std::sin(a) * radius, y + std::cos(a) * radius, z};
                    const Vec3 q{x + std::sin(b) * radius, y + std::cos(b) * radius, z};
                    quad({x, y, z}, p, q, {x, y, z}, 0);
                    quad(p, {p.x, p.y, z - depth}, {q.x, q.y, z - depth}, q, 1);
                }
            }
        };

        float hash_noise(int x, int y) {
            std::uint32_t h = static_cast<std::uint32_t>(x) * 374761393u +
                              static_cast<std::uint32_t>(y) * 668265263u;
            h = (h ^ (h >> 13)) * 1274126177u;
            return static_cast<float>((h ^ (h >> 16)) & 65535u) / 65535.0f;
        }

        float noise(float x, float y) {
            const int ix = static_cast<int>(std::floor(x));
            const int iy = static_cast<int>(std::floor(y));
            const float fx = x - ix, fy = y - iy;
            const float u = fx * fx * (3 - 2 * fx), v = fy * fy * (3 - 2 * fy);
            const float a = hash_noise(ix, iy), b = hash_noise(ix + 1, iy);
            const float c = hash_noise(ix, iy + 1), d = hash_noise(ix + 1, iy + 1);
            return (a + (b - a) * u) * (1 - v) + (c + (d - c) * u) * v;
        }

        std::vector<std::uint8_t> material(int kind) {
            constexpr int tile_width = kMaterialWidth / 4;
            std::vector<std::uint8_t> pixels(kMaterialWidth * kMaterialHeight * 4);
            const std::array<std::array<float, 3>, 4> colors{
                {{{109, 68, 39}}, {{157, 119, 66}}, {{37, 76, 70}}, {{91, 96, 87}}}};
            constexpr float shades[]{1.0f, 0.78f, 0.62f, 1.16f};
            for (int y = 0; y < kMaterialHeight; ++y) {
                for (int x = 0; x < tile_width; ++x) {
                    const float u = static_cast<float>(x) / (tile_width - 1);
                    const float v = static_cast<float>(y) / (kMaterialHeight - 1);
                    const float coarse = noise(u * 7, v * 9);
                    const float fine = hash_noise(x + kind * 563, y);
                    float light = 1, patina = 0;
                    if (kind == 0) {
                        const float warp = noise(u * 4, v * 5) - 0.5f;
                        const float knot_x = (u - 0.37f) * 2.4f;
                        const float knot_y = (v - 0.66f) * 8;
                        const float knot = std::exp(-(knot_x * knot_x + knot_y * knot_y) * 3);
                        const float phase = u * 69 + warp * 9 + noise(u * 8, v * 2) * 7 + knot * 12;
                        const float ring = std::sin(phase);
                        const float pores = std::pow(std::max(0.0f, std::sin(phase * 3.1f)), 12);
                        light = 0.84f + 0.11f * ring + 0.25f * noise(u * 18, v * 3) -
                                0.10f * pores - 0.20f * knot + (fine - 0.5f) * 0.08f;
                        const float split = std::abs(u - 0.77f - 0.012f * std::sin(v * 19));
                        if (split < 0.004f && v > 0.12f && v < 0.51f) {
                            light *= 0.40f;
                        }
                    } else if (kind == 1) {
                        light = 0.84f + 0.24f * coarse + (fine - 0.5f) * 0.18f +
                                0.08f * noise(u * 160, v * 5);
                        patina = std::clamp((noise(u * 9 + 21, v * 11) - 0.51f), 0.0f, 0.24f);
                    } else if (kind == 2) {
                        const float column = u * 4;
                        const float row = v * 3;
                        const float edge_x = column - std::floor(column);
                        const float edge_y = row - std::floor(row);
                        light = 0.76f + 0.38f * coarse + (fine - 0.5f) * 0.14f;
                        if (edge_x < 0.045f || edge_y < 0.045f) {
                            light *= 0.42f;
                        } else if (edge_x < 0.09f || edge_y < 0.10f) {
                            light *= 1.30f;
                        }
                        patina = std::max(0.0f, noise(u * 12, v * 16) - 0.55f) * 0.55f;
                    } else {
                        light = 0.70f + 0.53f * coarse + (noise(u * 45, v * 47) - 0.5f) * 0.30f +
                                (fine - 0.5f) * 0.18f;
                        if (fine > 0.975f) {
                            light += 0.22f;
                        }
                    }
                    // Worn raised edges and darker recesses give the unlit material depth.
                    const float edge = std::min({u, 1 - u, v, 1 - v});
                    if (edge < 0.018f) {
                        light *= 0.61f;
                    } else if (edge < 0.035f) {
                        light *= 1.22f;
                    }
                    for (int shade = 0; shade < 4; ++shade) {
                        const std::size_t at =
                            static_cast<std::size_t>(y * kMaterialWidth + shade * tile_width + x) *
                            4;
                        constexpr float oxidation[]{37, 83, 73};
                        for (int c = 0; c < 3; ++c) {
                            const float color =
                                colors[kind][c] * light * (1 - patina) + oxidation[c] * patina;
                            pixels[at + c] = static_cast<std::uint8_t>(
                                std::clamp(color * shades[shade], 0.0f, 255.0f));
                        }
                        pixels[at + 3] = 255;
                    }
                }
            }
            return pixels;
        }

        std::vector<std::uint8_t> slate_surface() {
            std::vector<std::uint8_t> pixels(kSurfaceWidth * kSurfaceHeight * 4);
            for (int y = 0; y < kSurfaceHeight; ++y) {
                for (int x = 0; x < kSurfaceWidth; ++x) {
                    const float u = static_cast<float>(x) / kSurfaceWidth;
                    const float v = static_cast<float>(y) / kSurfaceHeight;
                    const float edge = std::min({u, 1 - u, v, 1 - v});
                    const float weather = std::max(0.0f, 1 - edge * 9);
                    const float cloudy = noise(u * 9, v * 11);
                    const float streak = noise(u * 95 + v * 5, v * 7);
                    const float grain = hash_noise(x, y) - 0.5f;
                    const float light = 0.85f + 0.16f * cloudy + 0.04f * streak + grain * 0.10f;
                    const std::size_t at = static_cast<std::size_t>(y * kSurfaceWidth + x) * 4;
                    constexpr float base[]{15, 29, 29};
                    constexpr float dust[]{20, 18, 11};
                    for (int c = 0; c < 3; ++c) {
                        pixels[at + c] = static_cast<std::uint8_t>(
                            base[c] * light + dust[c] * weather * (0.35f + cloudy * 0.65f));
                    }
                    pixels[at + 3] = 255;
                }
            }
            return pixels;
        }

        std::string count_text(std::uint64_t value) {
            std::string text = std::to_string(value);
            for (int i = static_cast<int>(text.size()) - 3; i > 0; i -= 3) {
                text.insert(static_cast<std::size_t>(i), ",");
            }
            return text;
        }

        std::string timer_text(std::uint32_t seconds) {
            std::ostringstream out;
            out << std::setfill('0');
            if (seconds >= 3600) {
                out << seconds / 3600 << ':' << std::setw(2) << (seconds / 60) % 60;
            } else {
                out << std::setw(2) << seconds / 60;
            }
            out << ':' << std::setw(2) << seconds % 60;
            return out.str();
        }
    }  // namespace

    ExpEventBoard::ExpEventBoard(engine::IEngine& engine) : m_engine(engine) {}

    ExpEventBoard::~ExpEventBoard() {
        destroy();
    }

    bool ExpEventBoard::initialize() {
        if (m_panel) {
            return true;
        }
        if (!m_body_font.load(m_engine, FreetypeFont::find_system_sans(), 32) ||
            !m_title_font.load(m_engine, FreetypeFont::find_system_sans_bold(), 72)) {
            destroy();
            return false;
        }
        std::array<Geometry, 4> geometry;
        auto& wood = geometry[0];
        auto& bronze = geometry[1];
        auto& roof = geometry[2];
        auto& stone = geometry[3];
        // The entire asset is generated here; no legacy meshes or textures are loaded.
        for (float x : {-108.0f, 108.0f}) {
            stone.box(x, 7, 0, 33, 14, 40);
            stone.box(x, 17, 0, 27, 8, 33);
            wood.box(x, 130, 0, 18, 224, 22);
            bronze.box(x, 37, 0, 19, 10, 23);
            bronze.box(x, 211, 0, 19, 9, 23);
            bronze.box(x, 240, 0, 24, 7, 27);
            for (float y : {37.0f, 211.0f}) {
                bronze.medallion(x, y, 12.5f, 3.0f, 2.0f);
                bronze.medallion(x, y, -12.5f, 3.0f, -2.0f);
            }
        }
        // Solid backing, open space beneath it, and raised framing on both faces.
        wood.box(0, 153, 0, 204, 148, 16);
        wood.box(0, 77, 0, 234, 12, 24);
        wood.box(0, 229, 0, 234, 12, 24);
        for (float z : {-11.0f, 11.0f}) {
            wood.box(-97, 153, z, 12, 144, 8);
            wood.box(97, 153, z, 12, 144, 8);
            wood.box(0, 218, z, 194, 12, 8);
            wood.box(0, 88, z, 194, 12, 8);
            bronze.box(-91, 153, z + std::copysign(4.5f, z), 2, 122, 2);
            bronze.box(91, 153, z + std::copysign(4.5f, z), 2, 122, 2);
            bronze.box(0, 213, z + std::copysign(4.5f, z), 182, 2, 2);
            bronze.box(0, 93, z + std::copysign(4.5f, z), 182, 2, 2);
        }
        // Shallow tiled canopy, with raised tips and a bronze ridge cap.
        constexpr float xs[]{-134, -114, -76, 0, 76, 114, 134};
        constexpr float ys[]{253, 247, 250, 253, 250, 247, 253};
        for (int i = 0; i < 6; ++i) {
            for (float side : {-1.0f, 1.0f}) {
                const float z = side * 32;
                roof.quad({xs[i], ys[i], z},
                          {xs[i + 1], ys[i + 1], z},
                          {xs[i + 1], ys[i + 1] + 18, 0},
                          {xs[i], ys[i] + 18, 0},
                          side > 0 ? 0 : 1);
                roof.quad({xs[i], ys[i] - 5, z},
                          {xs[i + 1], ys[i + 1] - 5, z},
                          {xs[i + 1], ys[i + 1], z},
                          {xs[i], ys[i], z},
                          2);
                bronze.quad({xs[i], ys[i] - 2, z + side},
                            {xs[i + 1], ys[i + 1] - 2, z + side},
                            {xs[i + 1], ys[i + 1], z + side},
                            {xs[i], ys[i], z + side},
                            0);
            }
            bronze.box((xs[i] + xs[i + 1]) * 0.5f,
                       (ys[i] + ys[i + 1]) * 0.5f + 18,
                       0,
                       xs[i + 1] - xs[i] + 1,
                       3,
                       7);
        }
        wood.box(0, 245, 0, 210, 8, 16);
        for (float side : {-1.0f, 1.0f}) {
            bronze.medallion(0, 237, side * 23, 17, side * 7);
            roof.medallion(0, 237, side * 24, 12, side);
            bronze.quad({0, 247, side * 25},
                        {8, 237, side * 25},
                        {0, 227, side * 25},
                        {-8, 237, side * 25});
        }
        for (std::size_t i = 0; i < geometry.size(); ++i) {
            const auto pixels = material(static_cast<int>(i));
            Part part;
            part.texture =
                m_engine.create_texture_rgba8(pixels.data(), kMaterialWidth, kMaterialHeight);
            const auto& mesh = geometry[i];
            part.mesh = m_engine.create_mesh(mesh.vertices.data(),
                                             mesh.vertices.size(),
                                             mesh.indices.data(),
                                             mesh.indices.size());
            m_parts.push_back(part);
            if (!part.mesh || !part.texture) {
                destroy();
                return false;
            }
        }
        const auto backdrop = slate_surface();
        m_backdrop = m_engine.create_texture_rgba8(backdrop.data(), kSurfaceWidth, kSurfaceHeight);
        if (!m_backdrop) {
            destroy();
            return false;
        }
        // UVs compensate only this lettering for the game's mirrored world view.
        const engine::VertexP3T2 vertices[]{
            {-90, 94, 16, 1, 1}, {90, 94, 16, 0, 1}, {90, 212, 16, 0, 0}, {-90, 212, 16, 1, 0}};
        const std::uint32_t indices[]{0, 1, 2, 0, 2, 3};
        m_panel = m_engine.create_mesh(vertices, 4, indices, 6);
        if (!m_panel) {
            destroy();
            return false;
        }
        return true;
    }

    bool ExpEventBoard::render_surface(const protocol::ExpEventStateFields& state,
                                       std::string_view item_name) {
        if (!m_engine.begin_offscreen(kSurfaceWidth, kSurfaceHeight, 0.035f, 0.071f, 0.074f)) {
            return false;
        }
        const auto rect =
            [&](float x, float y, float w, float h, float r, float g, float b, float a = 1.0f) {
                m_engine.draw_ui_rect(x, y, w, h, r, g, b, a);
            };
        const auto line = [&](float y) { rect(64, y, 896, 1, 0.65f, 0.51f, 0.29f, 0.6f); };
        const auto centered = [&](const FreetypeFont& font,
                                  float x,
                                  float y,
                                  std::string_view text,
                                  float size,
                                  float width,
                                  float r,
                                  float g,
                                  float b) {
            const float scale = std::min(size / static_cast<float>(font.pixel_size()),
                                         width / std::max(1.0f, font.text_width(text)));
            font.draw_centered(m_engine, x, y, text, r, g, b, scale);
        };
        // Cached slate stays quiet behind the text; weathering is strongest at the edges.
        m_engine.draw_ui_quad(m_backdrop, 0, 0, kSurfaceWidth, kSurfaceHeight);
        rect(22, 22, 980, 2, 0.59f, 0.46f, 0.27f);
        rect(22, 616, 980, 2, 0.59f, 0.46f, 0.27f);
        rect(22, 22, 2, 596, 0.59f, 0.46f, 0.27f);
        rect(1000, 22, 2, 596, 0.59f, 0.46f, 0.27f);
        centered(m_body_font, 512, 44, "T E M P O R A R Y   F O R T", 22, 830, 0.72f, 0.61f, 0.40f);
        centered(m_title_font, 512, 83, "EXP EVENT", 76, 870, 0.94f, 0.86f, 0.66f);
        line(182);
        const bool active = state.active_bonus_percent > 0 && state.remaining_seconds > 0;
        if (active) {
            centered(
                m_body_font, 512, 207, "THE FORT STANDS TOGETHER", 25, 850, 0.40f, 0.77f, 0.69f);
            centered(m_title_font,
                     512,
                     245,
                     "+" + std::to_string(state.active_bonus_percent) + "% EXP",
                     94,
                     860,
                     0.96f,
                     0.88f,
                     0.68f);
            centered(m_body_font, 512, 365, "EVENT IN PROGRESS", 25, 850, 0.70f, 0.78f, 0.73f);
            centered(m_title_font,
                     512,
                     404,
                     timer_text(state.remaining_seconds),
                     64,
                     860,
                     0.40f,
                     0.82f,
                     0.72f);
            centered(m_body_font, 512, 483, "TIME REMAINING", 20, 850, 0.66f, 0.72f, 0.66f);
            line(530);
            centered(m_body_font,
                     512,
                     551,
                     (state.next_bonus_percent > state.active_bonus_percent
                          ? "Next: +" + std::to_string(state.next_bonus_percent) + "% EXP | "
                          : std::string("Extend event | ")) +
                         count_text(state.contributed) + " / " + count_text(state.target) +
                         " coins",
                     25,
                     850,
                     0.79f,
                     0.72f,
                     0.57f);
        } else {
            centered(m_body_font,
                     512,
                     205,
                     "A LITTLE FROM EACH. MORE FOR EVERYONE.",
                     23,
                     850,
                     0.40f,
                     0.77f,
                     0.69f);
            centered(m_body_font, 281, 266, "DONATED", 23, 390, 0.70f, 0.74f, 0.67f);
            centered(m_body_font, 743, 266, "STILL NEEDED", 23, 390, 0.70f, 0.74f, 0.67f);
            rect(511, 272, 1, 104, 0.65f, 0.51f, 0.29f, 0.5f);
            centered(m_title_font,
                     281,
                     305,
                     count_text(state.contributed),
                     68,
                     400,
                     0.95f,
                     0.87f,
                     0.68f);
            centered(
                m_title_font,
                743,
                305,
                count_text(state.target > state.contributed ? state.target - state.contributed : 0),
                68,
                400,
                0.95f,
                0.87f,
                0.68f);
            const float progress =
                state.target == 0
                    ? 0.0f
                    : static_cast<float>(std::min(1.0,
                                                  static_cast<double>(state.contributed) /
                                                      static_cast<double>(state.target)));
            rect(74, 411, 876, 28, 0.16f, 0.23f, 0.21f);
            rect(78, 415, 868 * progress, 20, 0.27f, 0.63f, 0.54f);
            for (int i = 1; i < 20; ++i) {
                rect(78 + i * 43.4f, 415, 2, 20, 0.035f, 0.071f, 0.074f, 0.65f);
            }
            centered(m_body_font,
                     512,
                     455,
                     "GOAL  " + count_text(state.target) + "   /   REWARD  +" +
                         std::to_string(state.next_bonus_percent) + "% EXP",
                     25,
                     850,
                     0.79f,
                     0.72f,
                     0.57f);
            line(514);
            centered(m_body_font,
                     512,
                     536,
                     "Donate " + std::string(item_name),
                     27,
                     850,
                     0.82f,
                     0.86f,
                     0.78f);
            centered(m_body_font,
                     512,
                     577,
                     "Speak to the Exp Event Manager",
                     21,
                     850,
                     0.55f,
                     0.64f,
                     0.59f);
        }
        std::vector<std::uint8_t> pixels;
        if (!m_engine.end_offscreen_rgba(pixels)) {
            return false;
        }
        const auto replacement =
            m_engine.create_texture_rgba8(pixels.data(), kSurfaceWidth, kSurfaceHeight);
        if (!replacement) {
            return false;
        }
        if (m_surface) {
            m_engine.destroy_texture(m_surface);
        }
        m_surface = replacement;
        return true;
    }

    bool ExpEventBoard::update(const protocol::ExpEventStateFields& state,
                               std::string_view item_name) {
        if (!state.enabled || !initialize()) {
            return false;
        }
        const auto snapshot = protocol::ExpEventState::frame(state);
        if (snapshot == m_last_snapshot && item_name == m_last_item_name && m_surface) {
            return true;
        }
        if (!render_surface(state, item_name)) {
            return false;
        }
        m_last_snapshot = snapshot;
        m_last_item_name = item_name;
        return true;
    }

    void ExpEventBoard::draw(const engine::Mat4& model) const {
        if (!m_surface) {
            return;
        }
        for (const auto& part : m_parts) {
            m_engine.draw_mesh_textured(part.mesh, part.texture, model, false, true);
        }
        m_engine.draw_mesh_textured(m_panel, m_surface, model, false, true);
        const auto back = engine::Mat4::multiply(model, engine::Mat4::from_quaternion(0, 1, 0, 0));
        m_engine.draw_mesh_textured(m_panel, m_surface, back, false, true);
    }

    void ExpEventBoard::destroy() {
        for (const auto& part : m_parts) {
            if (part.mesh) {
                m_engine.destroy_mesh(part.mesh);
            }
            if (part.texture) {
                m_engine.destroy_texture(part.texture);
            }
        }
        m_parts.clear();
        if (m_panel) {
            m_engine.destroy_mesh(m_panel);
            m_panel = {};
        }
        if (m_surface) {
            m_engine.destroy_texture(m_surface);
            m_surface = {};
        }
        if (m_backdrop) {
            m_engine.destroy_texture(m_backdrop);
            m_backdrop = {};
        }
        m_body_font.destroy(m_engine);
        m_title_font.destroy(m_engine);
        m_last_snapshot.clear();
        m_last_item_name.clear();
    }
}  // namespace openkal::client
