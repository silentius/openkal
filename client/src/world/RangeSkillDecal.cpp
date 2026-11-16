/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/RangeSkillDecal.h"

#include "openkal/client/SpriteFx.h"

#include <cmath>
#include <cstdint>
#include <vector>

namespace openkal::client {
    namespace {

        constexpr int kRingSegs = 48;
        constexpr float kRingOuter = 9.0f;
        constexpr float kRingInner = 6.2f;
        constexpr float kRingY = 0.45f;
        constexpr float kSpinRadPerSec = 2.0f;

        /** Lift so the prep FX sits where ClickMarker's sphere used to hop. */
        constexpr float kPreviewLift = 8.0f;

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

        void draw_tinted(engine::IEngine& engine,
                         engine::MeshHandle mesh,
                         engine::TextureHandle tex,
                         const engine::Mat4& model,
                         float r,
                         float g,
                         float b) {
            engine.draw_mesh_textured(mesh,
                                      tex,
                                      model,
                                      /*alpha_blend=*/false,
                                      /*two_sided=*/true,
                                      /*alpha_test=*/false,
                                      /*additive=*/true,
                                      0.0f,
                                      0.0f,
                                      r,
                                      g,
                                      b,
                                      1.0f);
        }

    }  // namespace

    bool RangeSkillDecal::init(engine::IEngine& engine) {
        destroy(engine);

        std::vector<engine::VertexP3T2> ring_verts;
        std::vector<std::uint32_t> ring_idx;
        append_ring(ring_verts, ring_idx);
        m_ring = engine.create_mesh(
            ring_verts.data(), ring_verts.size(), ring_idx.data(), ring_idx.size());

        const std::uint8_t px[4] = {255, 255, 255, 255};
        m_texture = engine.create_texture_rgba8(px, 1, 1, true);
        m_own_texture = static_cast<bool>(m_texture);
        return static_cast<bool>(m_ring) && static_cast<bool>(m_texture);
    }

    void RangeSkillDecal::destroy(engine::IEngine& engine) {
        if (m_sprite_fx) {
            m_sprite_fx->detach(kPreviewEntity);
            m_sprite_fx = nullptr;
        }
        m_fx_name.clear();
        m_active = false;
        m_age = 0.0f;
        if (m_ring) {
            engine.destroy_mesh(m_ring);
            m_ring = {};
        }
        if (m_own_texture && m_texture) {
            engine.destroy_texture(m_texture);
            m_texture = {};
            m_own_texture = false;
        }
    }

    void RangeSkillDecal::show(engine::IEngine& engine,
                               SpriteFx& sprite_fx,
                               float wx,
                               float wy,
                               float wz,
                               std::string_view fx_name) {
        m_x = wx;
        m_y = wy;
        m_z = wz;
        m_active = true;
        m_sprite_fx = &sprite_fx;
        const bool name_changed = fx_name != m_fx_name;
        if (name_changed || (!fx_name.empty() && !sprite_fx.has(kPreviewEntity))) {
            sprite_fx.detach(kPreviewEntity);
            m_fx_name.assign(fx_name.begin(), fx_name.end());
            if (!m_fx_name.empty()) {
                sprite_fx.attach_fx(engine, kPreviewEntity, m_fx_name, -1);
                if (sprite_fx.has(kPreviewEntity)) {
                    sprite_fx.loop_attached(kPreviewEntity);
                }
            }
        }
        sprite_fx.follow_all(kPreviewEntity, wx, wy + kPreviewLift, wz);
    }

    void RangeSkillDecal::clear(SpriteFx& sprite_fx) {
        sprite_fx.detach(kPreviewEntity);
        m_sprite_fx = nullptr;
        m_fx_name.clear();
        m_active = false;
        m_age = 0.0f;
    }

    void RangeSkillDecal::tick(float dt) {
        if (!m_active) {
            return;
        }
        m_age += dt;
    }

    void RangeSkillDecal::draw(engine::IEngine& engine) const {
        if (!m_active || !m_ring || !m_texture) {
            return;
        }
        const float spin = m_age * kSpinRadPerSec;
        const float hy = spin * 0.5f;
        const auto R = engine::Mat4::from_quaternion(0.0f, std::sin(hy), 0.0f, std::cos(hy));
        const auto T = engine::Mat4::translation(m_x, m_y, m_z);
        const float pulse = 0.85f + 0.15f * std::sin(m_age * 10.0f);
        const auto S = engine::Mat4::scaling(pulse, 1.0f, pulse);
        const auto model = engine::Mat4::multiply(T, engine::Mat4::multiply(R, S));
        engine.set_polygon_offset(-4.0f, -64.0f);
        draw_tinted(engine, m_ring, m_texture, model, 1.0f, 1.0f, 1.0f);
        const float pulse2 = 1.15f + 0.2f * std::sin(m_age * 10.0f + 1.5f);
        const auto S2 = engine::Mat4::scaling(pulse2, 1.0f, pulse2);
        const auto model2 = engine::Mat4::multiply(T, engine::Mat4::multiply(R, S2));
        draw_tinted(engine, m_ring, m_texture, model2, 0.85f, 0.85f, 0.85f);
        engine.set_polygon_offset(0.0f, 0.0f);
    }

}  // namespace openkal::client
