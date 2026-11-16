/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/SkyBox.h"

#include "openkal/platform/File.h"
#include "openkal/platform/Log.h"
#include "openkal/platform/Path.h"
#include "openkal/terrain/GtxLoader.h"

#include <cstdio>
#include <string>

namespace openkal::client {
    namespace {

        engine::MeshHandle make_face(engine::IEngine& engine,
                                     const engine::Vec3& a,
                                     const engine::Vec3& b,
                                     const engine::Vec3& c,
                                     const engine::Vec3& d) {
            // compatibility behavior stores texcoords as {float v, float u} but D3DFVF_TEX1 reads
            // {u,v}, so the values they assign to .u/.v end up swapped in the GPU. Effective UVs
            // are:
            //   a (0,0), b (1,0), c (1,1), d (0,1) — U along a→b, V along a→d.
            const engine::VertexP3T2 verts[4] = {
                {a.x, a.y, a.z, 0.0f, 0.0f},
                {b.x, b.y, b.z, 1.0f, 0.0f},
                {c.x, c.y, c.z, 1.0f, 1.0f},
                {d.x, d.y, d.z, 0.0f, 1.0f},
            };
            const std::uint32_t idx[6] = {0, 1, 2, 0, 2, 3};
            return engine.create_mesh(verts, 4, idx, 6);
        }

    }  // namespace

    bool SkyBox::load(engine::IEngine& engine, const std::string& data_root) {
        destroy(engine);
        const std::string tex_dir = platform::join_path(data_root, "OBJECTS/common/tex");
        constexpr float f = 100.0f;
        // Face order + corners match compatibility behavior initialization (front, back, left,
        // right, top, down).
        const engine::Vec3 corners[kFaceCount][4] = {
            {{-f, f, f}, {f, f, f}, {f, -f, f}, {-f, -f, f}},      // front (+Z)
            {{f, f, -f}, {-f, f, -f}, {-f, -f, -f}, {f, -f, -f}},  // back (-Z)
            {{-f, f, -f}, {-f, f, f}, {-f, -f, f}, {-f, -f, -f}},  // left (-X)
            {{f, f, f}, {f, f, -f}, {f, -f, -f}, {f, -f, f}},      // right (+X)
            {{-f, f, -f}, {f, f, -f}, {f, f, f}, {-f, f, f}},      // top (+Y)
            {{-f, -f, f}, {f, -f, f}, {f, -f, -f}, {-f, -f, -f}},  // bottom (-Y)
        };

        int loaded = 0;
        for (std::size_t i = 0; i < kFaceCount; ++i) {
            m_faces[i] =
                make_face(engine, corners[i][0], corners[i][1], corners[i][2], corners[i][3]);

            char name[64];
            std::snprintf(name, sizeof(name), "sample_sky_%d.GTX", static_cast<int>(i));
            std::string path = platform::join_path(tex_dir, name);
            if (!platform::file_exists(path)) {
                std::snprintf(name, sizeof(name), "sample_sky_%d.gtx", static_cast<int>(i));
                path = platform::join_path(tex_dir, name);
            }
            if (!platform::file_exists(path)) {
                std::snprintf(name, sizeof(name), "sample_sky_%d_.GTX", static_cast<int>(i));
                path = platform::join_path(tex_dir, name);
            }

            terrain::ImageRgba img;
            if (!terrain::load_gtx_rgba(path, img) || img.rgba.empty() || img.width <= 0 ||
                img.height <= 0) {
                log::warn("SkyBox: missing face " + path);
                continue;
            }
            m_textures[i] =
                engine.create_texture_rgba8(img.rgba.data(), img.width, img.height, false);
            ++loaded;
        }

        m_ready = loaded == static_cast<int>(kFaceCount);
        if (!m_ready) {
            log::warn("SkyBox: loaded " + std::to_string(loaded) + "/6 faces");
        } else {
            log::info("SkyBox: loaded sample_sky_0..5");
        }
        return m_ready;
    }

    void SkyBox::destroy(engine::IEngine& engine) {
        for (auto& m : m_faces) {
            if (m.id != 0) {
                engine.destroy_mesh(m);
                m = {};
            }
        }
        for (auto& t : m_textures) {
            if (t.id != 0) {
                engine.destroy_texture(t);
                t = {};
            }
        }
        m_ready = false;
    }

    void SkyBox::draw(engine::IEngine& engine) const {
        if (!m_ready) {
            return;
        }
        for (std::size_t i = 0; i < kFaceCount; ++i) {
            const auto& mesh = m_faces[i];
            const auto& tex = m_textures[i];
            if (mesh.id == 0 || tex.id == 0) {
                continue;
            }
            engine.draw_sky_face(mesh, tex, engine::Mat4::identity());
        }
    }

}  // namespace openkal::client
