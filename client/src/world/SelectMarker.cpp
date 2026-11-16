/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/SelectMarker.h"

#include "openkal/platform/Log.h"
#include "openkal/platform/Path.h"
#include "openkal/terrain/GtxLoader.h"

#include <cmath>
#include <cstdint>

namespace openkal::client {
    namespace {

        // Soft disc footprint under a typical field mob (~half a tile of padding).
        constexpr float kRadius = 10.0f;

    }  // namespace

    bool SelectMarker::init(engine::IEngine& engine, const std::string& data_root) {
        destroy(engine);

        const engine::VertexP3T2 verts[4] = {
            {-kRadius, 0.35f, -kRadius, 0.0f, 0.0f},
            {kRadius, 0.35f, -kRadius, 1.0f, 0.0f},
            {kRadius, 0.35f, kRadius, 1.0f, 1.0f},
            {-kRadius, 0.35f, kRadius, 0.0f, 1.0f},
        };
        const std::uint32_t idx[6] = {0, 1, 2, 0, 2, 3};
        m_quad = engine.create_mesh(verts, 4, idx, 6);
        if (!m_quad) {
            return false;
        }

        const std::string path =
            platform::join_path(platform::join_path(data_root, "Effect/Tex"), "circle.GTX");
        terrain::ImageRgba img;
        if (terrain::load_gtx_rgba(path, img) && !img.rgba.empty()) {
            m_texture = engine.create_texture_rgba8(img.rgba.data(), img.width, img.height, true);
            m_own_texture = static_cast<bool>(m_texture);
        }
        if (!m_texture) {
            // Soft white fallback so selection still reads without the asset.
            std::uint8_t px[4] = {255, 255, 255, 200};
            m_texture = engine.create_texture_rgba8(px, 1, 1, true);
            m_own_texture = static_cast<bool>(m_texture);
            log::warn("SelectMarker: missing Effect/Tex/circle.GTX");
        }
        return static_cast<bool>(m_texture);
    }

    void SelectMarker::destroy(engine::IEngine& engine) {
        if (m_quad) {
            engine.destroy_mesh(m_quad);
            m_quad = {};
        }
        if (m_own_texture && m_texture) {
            engine.destroy_texture(m_texture);
            m_texture = {};
            m_own_texture = false;
        }
        m_active = false;
        m_age = 0.0f;
    }

    void SelectMarker::show(float wx, float wy, float wz) {
        m_x = wx;
        m_y = wy;
        m_z = wz;
        m_active = true;
    }

    void SelectMarker::clear() {
        m_active = false;
    }

    void SelectMarker::tick(float dt) {
        if (!m_active) {
            return;
        }
        m_age += dt;
    }

    void SelectMarker::draw(engine::IEngine& engine) const {
        if (!m_active || !m_quad || !m_texture) {
            return;
        }
        // Gentle pulse so the disc doesn't look glued flat to the terrain.
        const float pulse = 0.92f + 0.08f * std::sin(m_age * 4.0f);
        const auto S = engine::Mat4::scaling(pulse, 1.0f, pulse);
        const auto T = engine::Mat4::translation(m_x, m_y, m_z);
        const auto model = engine::Mat4::multiply(T, S);
        engine.set_polygon_offset(-4.0f, -64.0f);
        // Kal particle stages use addblend for circle.GTX — additive keeps it bright
        // on grass.
        engine.draw_mesh_textured(m_quad,
                                  m_texture,
                                  model,
                                  /*alpha_blend=*/false,
                                  /*two_sided=*/true,
                                  /*alpha_test=*/false,
                                  /*additive=*/true);
        engine.set_polygon_offset(0.0f, 0.0f);
    }

}  // namespace openkal::client
