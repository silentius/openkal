/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/DropItemVisual.h"

#include "openkal/platform/File.h"
#include "openkal/platform/Path.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace openkal::client {
    namespace {

        constexpr int kGlowTexSize = 64;
        constexpr float kBillboardWidth = 12.0f;
        constexpr float kBillboardHeight = 15.0f;
        constexpr float kBillboardLift = 3.4f;
        constexpr float kDiscRadius = 7.5f;
        constexpr float kDiscY = 0.08f;
        constexpr float kPulseHz = 3.2f;
        constexpr float kRimScale = 1.28f;

        [[nodiscard]] float glow_pulse(float age_sec) {
            return 0.72f + 0.28f * (0.5f + 0.5f * std::sin(age_sec * kPulseHz));
        }

        void fill_radial_glow(std::vector<std::uint8_t>& rgba) {
            rgba.assign(static_cast<std::size_t>(kGlowTexSize * kGlowTexSize * 4), 0);
            const float inv = 2.0f / static_cast<float>(kGlowTexSize);
            for (int y = 0; y < kGlowTexSize; ++y) {
                const float fy = (static_cast<float>(y) + 0.5f) * inv - 1.0f;
                for (int x = 0; x < kGlowTexSize; ++x) {
                    const float fx = (static_cast<float>(x) + 0.5f) * inv - 1.0f;
                    const float d = std::sqrt(fx * fx + fy * fy);
                    float t = 1.0f - d;
                    if (t < 0.0f) {
                        t = 0.0f;
                    }
                    // Smoothstep, then square so the rim stays soft and the core bright.
                    t = t * t * (3.0f - 2.0f * t);
                    t = t * t;
                    const auto a = static_cast<std::uint8_t>(t * 255.0f + 0.5f);
                    const std::size_t i = static_cast<std::size_t>((y * kGlowTexSize + x) * 4);
                    rgba[i + 0] = 255;
                    rgba[i + 1] = 255;
                    rgba[i + 2] = 255;
                    rgba[i + 3] = a;
                }
            }
        }

        void draw_add_glow(engine::IEngine& engine,
                           engine::MeshHandle mesh,
                           engine::TextureHandle tex,
                           const engine::Mat4& model,
                           float r,
                           float g,
                           float b,
                           float a) {
            engine.draw_mesh_textured(mesh,
                                      tex,
                                      model,
                                      /*alpha_blend=*/true,
                                      /*two_sided=*/true,
                                      /*alpha_test=*/false,
                                      /*additive=*/true,
                                      0.0f,
                                      0.0f,
                                      r,
                                      g,
                                      b,
                                      a);
        }

    }  // namespace

    int DropItemVisual::mesh_number(int item_index, int count, const ItemDb* catalog) {
        if (item_index == kDropMoneyItemIndex) {
            // Money pile sizes.
            if (count >= 5001) {
                return 10;
            }
            if (count >= 1001) {
                return 9;
            }
            if (count >= 201) {
                return 8;
            }
            return 7;
        }

        // 1 pouch, 2 sword, 3 shield, 4 wand, 5 bow, 6 stone (unused default pouch).
        int item_no = 1;
        if (catalog) {
            const ItemDef* def = catalog->find(item_index);
            if (def) {
                const std::string& small = def->class_small();
                if (small == "sword" || small == "sword2h" || small == "sword2hand" ||
                    small == "dagger") {
                    item_no = 2;
                } else if (small == "shield") {
                    item_no = 3;
                } else if (small == "wand") {
                    item_no = 4;
                } else if (small == "bow") {
                    item_no = 5;
                } else {
                    item_no = 1;
                }
            }
        }
        return item_no;
    }

    std::string DropItemVisual::gb_path(const std::string& data_root, int mesh_number) {
        if (mesh_number < 1 || mesh_number > 11) {
            return {};
        }
        char name[32];
        std::snprintf(name, sizeof(name), "dropitem%02d.gb", mesh_number);
        const std::string rel = platform::join_path(platform::join_path("Item", "DropItem"), name);
        std::string path = platform::join_path(data_root, rel);
        if (platform::file_exists(path)) {
            return path;
        }
        // Case-insensitive fallback (DropitemNN.gb on some trees).
        std::snprintf(name, sizeof(name), "Dropitem%02d.gb", mesh_number);
        path = platform::join_path(
            data_root, platform::join_path(platform::join_path("Item", "DropItem"), name));
        if (platform::file_exists(path)) {
            return path;
        }
        return platform::resolve_client_asset(data_root, rel, ".gb");
    }

    bool DropItemVisual::is_pouch(int mesh_number) {
        return mesh_number == kDropPouchMeshNumber;
    }

    bool DropItemVisual::init(engine::IEngine& engine) {
        destroy(engine);

        const engine::VertexP3T2 billboard[4] = {
            {-0.5f, -0.5f, 0.0f, 0.0f, 0.0f},
            {0.5f, -0.5f, 0.0f, 1.0f, 0.0f},
            {0.5f, 0.5f, 0.0f, 1.0f, 1.0f},
            {-0.5f, 0.5f, 0.0f, 0.0f, 1.0f},
        };
        const std::uint32_t idx[6] = {0, 1, 2, 0, 2, 3};
        m_billboard = engine.create_mesh(billboard, 4, idx, 6);

        const engine::VertexP3T2 disc[4] = {
            {-kDiscRadius, kDiscY, -kDiscRadius, 0.0f, 0.0f},
            {kDiscRadius, kDiscY, -kDiscRadius, 1.0f, 0.0f},
            {kDiscRadius, kDiscY, kDiscRadius, 1.0f, 1.0f},
            {-kDiscRadius, kDiscY, kDiscRadius, 0.0f, 1.0f},
        };
        m_disc = engine.create_mesh(disc, 4, idx, 6);

        std::vector<std::uint8_t> rgba;
        fill_radial_glow(rgba);
        m_texture =
            engine.create_texture_rgba8(rgba.data(), kGlowTexSize, kGlowTexSize, /*wrap=*/false);
        m_own_texture = static_cast<bool>(m_texture);
        return static_cast<bool>(m_billboard) && static_cast<bool>(m_disc) &&
               static_cast<bool>(m_texture);
    }

    void DropItemVisual::destroy(engine::IEngine& engine) {
        if (m_billboard) {
            engine.destroy_mesh(m_billboard);
            m_billboard = {};
        }
        if (m_disc) {
            engine.destroy_mesh(m_disc);
            m_disc = {};
        }
        if (m_own_texture && m_texture) {
            engine.destroy_texture(m_texture);
            m_texture = {};
            m_own_texture = false;
        }
    }

    void DropItemVisual::draw_glow(engine::IEngine& engine,
                                   float wx,
                                   float wy,
                                   float wz,
                                   const engine::Vec3& cam_eye,
                                   float age_sec,
                                   float r,
                                   float g,
                                   float b) const {
        if (!m_texture) {
            return;
        }
        const float pulse = glow_pulse(age_sec);
        const float halo_a = 0.55f * pulse;
        const float disc_a = 0.40f * pulse;

        if (m_disc) {
            const float s = 0.90f + 0.10f * pulse;
            const auto model = engine::Mat4::multiply(engine::Mat4::translation(wx, wy, wz),
                                                      engine::Mat4::scaling(s, 1.0f, s));
            draw_add_glow(engine, m_disc, m_texture, model, r, g, b, disc_a);
        }

        if (!m_billboard) {
            return;
        }
        float fx = cam_eye.x - wx;
        float fy = cam_eye.y - (wy + kBillboardLift);
        float fz = cam_eye.z - wz;
        float fl = std::sqrt(fx * fx + fy * fy + fz * fz);
        if (fl < 1e-4f) {
            return;
        }
        fx /= fl;
        fy /= fl;
        fz /= fl;
        float rx = fz;
        float rz = -fx;
        float rl = std::sqrt(rx * rx + rz * rz);
        if (rl < 1e-4f) {
            rx = 1.0f;
            rz = 0.0f;
            rl = 1.0f;
        }
        rx /= rl;
        rz /= rl;
        const float ux = fy * rz;
        const float uy = fz * rx - fx * rz;
        const float uz = -fy * rx;

        const float bw = kBillboardWidth * (0.94f + 0.06f * pulse);
        const float bh = kBillboardHeight * (0.94f + 0.06f * pulse);
        engine::Mat4 M = engine::Mat4::identity();
        M.m[0] = rx * bw;
        M.m[1] = 0.0f;
        M.m[2] = rz * bw;
        M.m[4] = ux * bh;
        M.m[5] = uy * bh;
        M.m[6] = uz * bh;
        M.m[8] = fx;
        M.m[9] = fy;
        M.m[10] = fz;
        M.m[12] = wx;
        M.m[13] = wy + kBillboardLift;
        M.m[14] = wz;
        draw_add_glow(engine, m_billboard, m_texture, M, r, g, b, halo_a);
    }

    void DropItemVisual::draw_mesh_glow(engine::IEngine& engine,
                                        engine::MeshHandle mesh,
                                        engine::TextureHandle texture,
                                        const engine::Mat4& model,
                                        bool two_sided,
                                        float r,
                                        float g,
                                        float b,
                                        float age_sec) {
        if (!mesh || !texture) {
            return;
        }
        const float pulse = glow_pulse(age_sec);
        const auto glow_model =
            engine::Mat4::multiply(model, engine::Mat4::scaling(kRimScale, kRimScale, kRimScale));
        engine.draw_mesh_textured(mesh,
                                  texture,
                                  glow_model,
                                  /*alpha_blend=*/true,
                                  two_sided,
                                  /*alpha_test=*/false,
                                  /*additive=*/true,
                                  0.0f,
                                  0.0f,
                                  r * 0.45f,
                                  g * 0.45f,
                                  b * 0.45f,
                                  0.42f * pulse);
    }

}  // namespace openkal::client
