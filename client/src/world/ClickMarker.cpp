/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/ClickMarker.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace openkal::client {
    namespace {

        constexpr int kRingSegs = 48;
        constexpr float kRingOuter = 9.0f;
        constexpr float kRingInner = 6.2f;
        constexpr float kRingY = 0.45f;

        constexpr int kSphereSlices = 16;
        constexpr int kSphereStacks = 10;
        constexpr float kSphereRadius = 3.5f;
        constexpr float kSphereAlpha = 0.5f;
        constexpr float kHopHeight = 12.0f;
        constexpr float kHopRadPerSec = 8.0f;

        /** Matches the Godot sample: rotate_y(2.0 * delta). */
        constexpr float kSpinRadPerSec = 2.0f;

        void append_ring(std::vector<engine::VertexP3T2>& verts, std::vector<std::uint32_t>& idx) {
            const std::uint32_t base = static_cast<std::uint32_t>(verts.size());
            verts.reserve(verts.size() + static_cast<std::size_t>(kRingSegs * 2));
            idx.reserve(idx.size() + static_cast<std::size_t>(kRingSegs * 6));
            for (int i = 0; i < kRingSegs; ++i) {
                const float a0 =
                    (static_cast<float>(i) / static_cast<float>(kRingSegs)) * 6.2831853f;
                const float c = std::cos(a0);
                const float s = std::sin(a0);
                verts.push_back({kRingOuter * c, kRingY, kRingOuter * s, 0.0f, 0.0f});
                verts.push_back({kRingInner * c, kRingY, kRingInner * s, 1.0f, 0.0f});
            }
            for (int i = 0; i < kRingSegs; ++i) {
                const std::uint32_t a = base + static_cast<std::uint32_t>(i * 2);
                const std::uint32_t b =
                    base + static_cast<std::uint32_t>(((i + 1) % kRingSegs) * 2);
                idx.push_back(a);
                idx.push_back(a + 1);
                idx.push_back(b);
                idx.push_back(a + 1);
                idx.push_back(b + 1);
                idx.push_back(b);
            }
        }

        void append_sphere(std::vector<engine::VertexP3T2>& verts,
                           std::vector<std::uint32_t>& idx) {
            const std::uint32_t base = static_cast<std::uint32_t>(verts.size());
            const int cols = kSphereSlices + 1;
            verts.reserve(verts.size() + static_cast<std::size_t>((kSphereStacks + 1) * cols));
            for (int y = 0; y <= kSphereStacks; ++y) {
                const float v = static_cast<float>(y) / static_cast<float>(kSphereStacks);
                const float phi = v * 3.14159265f;
                const float sp = std::sin(phi);
                const float cp = std::cos(phi);
                for (int x = 0; x <= kSphereSlices; ++x) {
                    const float u = static_cast<float>(x) / static_cast<float>(kSphereSlices);
                    const float theta = u * 6.2831853f;
                    const float st = std::sin(theta);
                    const float ct = std::cos(theta);
                    verts.push_back({kSphereRadius * sp * ct,
                                     kSphereRadius * cp,
                                     kSphereRadius * sp * st,
                                     u,
                                     v});
                }
            }
            for (int y = 0; y < kSphereStacks; ++y) {
                for (int x = 0; x < kSphereSlices; ++x) {
                    const std::uint32_t i0 = base + static_cast<std::uint32_t>(y * cols + x);
                    const std::uint32_t i1 = i0 + 1;
                    const std::uint32_t i2 = i0 + static_cast<std::uint32_t>(cols);
                    const std::uint32_t i3 = i2 + 1;
                    idx.push_back(i0);
                    idx.push_back(i2);
                    idx.push_back(i1);
                    idx.push_back(i1);
                    idx.push_back(i2);
                    idx.push_back(i3);
                }
            }
        }

        void draw_tinted(engine::IEngine& engine,
                         engine::MeshHandle mesh,
                         engine::TextureHandle tex,
                         const engine::Mat4& model,
                         float r,
                         float g,
                         float b,
                         float a,
                         bool additive) {
            engine.draw_mesh_textured(mesh,
                                      tex,
                                      model,
                                      /*alpha_blend=*/!additive && a < 0.999f,
                                      /*two_sided=*/true,
                                      /*alpha_test=*/false,
                                      additive,
                                      0.0f,
                                      0.0f,
                                      r,
                                      g,
                                      b,
                                      a);
        }

    }  // namespace

    bool ClickMarker::init(engine::IEngine& engine) {
        destroy(engine);

        std::vector<engine::VertexP3T2> ring_verts;
        std::vector<std::uint32_t> ring_idx;
        append_ring(ring_verts, ring_idx);
        m_ring = engine.create_mesh(
            ring_verts.data(), ring_verts.size(), ring_idx.data(), ring_idx.size());

        std::vector<engine::VertexP3T2> sphere_verts;
        std::vector<std::uint32_t> sphere_idx;
        append_sphere(sphere_verts, sphere_idx);
        m_sphere = engine.create_mesh(
            sphere_verts.data(), sphere_verts.size(), sphere_idx.data(), sphere_idx.size());

        const std::uint8_t px[4] = {255, 255, 255, 255};
        m_texture = engine.create_texture_rgba8(px, 1, 1, true);
        m_own_texture = static_cast<bool>(m_texture);
        return static_cast<bool>(m_ring) && static_cast<bool>(m_sphere) &&
               static_cast<bool>(m_texture);
    }

    void ClickMarker::destroy(engine::IEngine& engine) {
        if (m_ring) {
            engine.destroy_mesh(m_ring);
            m_ring = {};
        }
        if (m_sphere) {
            engine.destroy_mesh(m_sphere);
            m_sphere = {};
        }
        if (m_own_texture && m_texture) {
            engine.destroy_texture(m_texture);
            m_texture = {};
            m_own_texture = false;
        }
        m_life = 0.0f;
    }

    void ClickMarker::show(float wx, float wy, float wz) {
        const float dx = wx - m_x;
        const float dz = wz - m_z;
        const bool moved = (dx * dx + dz * dz) > 0.25f;
        const bool was_hidden = m_life <= 0.0f;
        m_x = wx;
        m_y = wy;
        m_z = wz;
        m_life = 8.0f;  // Refreshed while walking; fades if stuck.
        if (was_hidden || moved) {
            m_age = 0.0f;
        }
    }

    void ClickMarker::clear() {
        m_life = 0.0f;
    }

    void ClickMarker::tick(float dt) {
        if (m_life <= 0.0f) {
            return;
        }
        m_age += dt;
        m_life = std::max(0.0f, m_life - dt);
    }

    void ClickMarker::draw(engine::IEngine& engine) const {
        if (!m_texture || m_life <= 0.0f) {
            return;
        }
        const float spin = m_age * kSpinRadPerSec;
        const float hy = spin * 0.5f;
        const auto R = engine::Mat4::from_quaternion(0.0f, std::sin(hy), 0.0f, std::cos(hy));
        const auto T = engine::Mat4::translation(m_x, m_y, m_z);

        engine.set_polygon_offset(-4.0f, -64.0f);
        if (m_ring) {
            const float pulse = 0.85f + 0.15f * std::sin(m_age * 10.0f);
            const auto S = engine::Mat4::scaling(pulse, 1.0f, pulse);
            const auto model = engine::Mat4::multiply(T, engine::Mat4::multiply(R, S));
            // Additive cycle — same trick as SelectMarker / circle.GTX on grass.
            draw_tinted(engine, m_ring, m_texture, model, 1.0f, 0.92f, 0.25f, 1.0f, true);
            const float pulse2 = 1.15f + 0.2f * std::sin(m_age * 10.0f + 1.5f);
            const auto S2 = engine::Mat4::scaling(pulse2, 1.0f, pulse2);
            const auto model2 = engine::Mat4::multiply(T, engine::Mat4::multiply(R, S2));
            draw_tinted(engine, m_ring, m_texture, model2, 1.0f, 0.55f, 0.1f, 1.0f, true);
        }

        if (m_sphere) {
            const float hop = std::fabs(std::sin(m_age * kHopRadPerSec));
            const float sy = 0.85f + 0.15f * hop;
            const float sxz = 1.12f - 0.12f * hop;
            const auto hop_t = engine::Mat4::translation(0.0f, hop * kHopHeight, 0.0f);
            const auto S = engine::Mat4::scaling(sxz, sy, sxz);
            const auto lift = engine::Mat4::translation(0.0f, kSphereRadius, 0.0f);
            const auto model = engine::Mat4::multiply(
                T,
                engine::Mat4::multiply(
                    R, engine::Mat4::multiply(hop_t, engine::Mat4::multiply(S, lift))));
            draw_tinted(
                engine, m_sphere, m_texture, model, 1.0f, 0.92f, 0.22f, kSphereAlpha, false);
        }
        engine.set_polygon_offset(0.0f, 0.0f);
    }

}  // namespace openkal::client
