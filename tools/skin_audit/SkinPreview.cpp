/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "SkinPreview.h"

#include "openkal/engine/TransparentMesh.h"
#include "openkal/terrain/GbModel.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <limits>
#include <png.h>

namespace openkal::tools {
    SkinPreview::~SkinPreview() {
        if (m_initialized) {
            clear();
            m_engine->shutdown();
        }
    }

    bool SkinPreview::initialize() {
        if (m_initialized) {
            return true;
        }
        m_engine.reset(engine::create_engine());
        m_initialized = m_engine && m_engine->init("OpenKal costume comparisons", 480, 640);
        if (!m_initialized) {
            m_error = "Cannot initialize the rendering context";
            return false;
        }
        m_engine->set_bloom_strength(0.0f);
        return true;
    }

    void SkinPreview::clear() {
        for (const auto& part : m_parts) {
            m_engine->destroy_mesh(part.mesh);
        }
        m_parts.clear();
        for (const auto& entry : m_textures) {
            m_engine->destroy_texture(entry.second);
        }
        m_textures.clear();
    }

    bool SkinPreview::load(const std::vector<std::string>& paths, const SkinPreviewPose& pose) {
        if (!m_initialized) {
            m_error = "Preview session is not initialized";
            return false;
        }
        clear();
        m_error.clear();
        std::vector<std::array<float, 16>> bone_world, skin;
        if (!pose.skeleton.empty() || !pose.motion.empty()) {
            terrain::GbModel skeleton, motion;
            if (!std::isfinite(pose.time_ms) || !terrain::load_gb(pose.skeleton, skeleton) ||
                skeleton.bones.empty() || !terrain::load_gb(pose.motion, motion) ||
                motion.anims.empty() ||
                !terrain::eval_bone_and_skin_matrices_at_time(
                    skeleton.bones, motion.anims.front(), pose.time_ms, true, bone_world, skin)) {
                m_error = "Cannot evaluate the requested skeleton/motion sample";
                return false;
            }
        }
        if (!pose.attachments.empty() && pose.attachments.size() != paths.size()) {
            m_error = "Attachment selection must match the model path count";
            return false;
        }
        std::array<float, 3> lo, hi;
        lo.fill(std::numeric_limits<float>::max());
        hi.fill(std::numeric_limits<float>::lowest());
        for (std::size_t path_index = 0; path_index < paths.size(); ++path_index) {
            const auto& path = paths[path_index];
            const int attachment = pose.attachments.empty() ? -1 : pose.attachments[path_index];
            if (attachment < -1 || attachment >= static_cast<int>(bone_world.size())) {
                m_error = "Attachment bone is unavailable";
                return false;
            }
            terrain::GbModel model;
            if (!terrain::load_gb(path, model) || model.meshes.empty()) {
                m_error = "Cannot load model: " + path;
                return false;
            }
            for (auto& mesh : model.meshes) {
                if (attachment >= 0) {
                    std::string name = mesh.name;
                    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) {
                        return static_cast<char>(std::tolower(c));
                    });
                    if (name.compare(0, 6, "effect") == 0) {
                        continue;
                    }
                    engine::Mat4 transform;
                    transform.m = bone_world[static_cast<std::size_t>(attachment)];
                    terrain::transform_mesh_normals(mesh, transform.m);
                    for (std::size_t i = 0; i + 4 < mesh.pos_uv.size(); i += 5) {
                        const auto point = engine::Mat4::transform_point(
                            transform, mesh.pos_uv[i], mesh.pos_uv[i + 1], mesh.pos_uv[i + 2]);
                        mesh.pos_uv[i] = point.x;
                        mesh.pos_uv[i + 1] = point.y;
                        mesh.pos_uv[i + 2] = point.z;
                    }
                } else if (!skin.empty()) {
                    terrain::skin_mesh(mesh, skin);
                }
                if (mesh.pos_uv.empty() || mesh.indices.empty() || mesh.material_index < 0 ||
                    mesh.material_index >= static_cast<int>(model.materials.size())) {
                    m_error = "Invalid model geometry or material: " + path;
                    return false;
                }
                const auto& name = model.materials[mesh.material_index].texture;
                const auto key = path + "|" + name;
                auto texture = m_textures.find(key);
                if (texture == m_textures.end()) {
                    terrain::ImageRgba image;
                    if (!terrain::load_gb_texture(path, name, image) || image.rgba.empty()) {
                        m_error = "Cannot load texture: " + name;
                        return false;
                    }
                    texture = m_textures
                                  .emplace(key,
                                           m_engine->create_texture_rgba8(
                                               image.rgba.data(), image.width, image.height, true))
                                  .first;
                }
                std::vector<engine::VertexP3T2> vertices;
                vertices.reserve(mesh.pos_uv.size() / 5);
                for (std::size_t i = 0; i + 4 < mesh.pos_uv.size(); i += 5) {
                    for (int axis = 0; axis < 3; ++axis) {
                        if (!std::isfinite(mesh.pos_uv[i + axis])) {
                            m_error = "Nonfinite model vertex: " + path;
                            return false;
                        }
                        lo[axis] = std::min(lo[axis], mesh.pos_uv[i + axis]);
                        hi[axis] = std::max(hi[axis], mesh.pos_uv[i + axis]);
                    }
                    vertices.push_back({mesh.pos_uv[i],
                                        mesh.pos_uv[i + 1],
                                        mesh.pos_uv[i + 2],
                                        mesh.pos_uv[i + 3],
                                        mesh.pos_uv[i + 4]});
                }
                Part part;
                part.mesh = m_engine->create_mesh(
                    vertices.data(), vertices.size(), mesh.indices.data(), mesh.indices.size());
                m_engine->set_mesh_specular(part.mesh, mesh.normals, mesh.specular_power);
                part.texture = texture->second;
                part.alpha_blend = mesh.alpha_blend;
                part.alpha_test = mesh.alpha_test;
                part.two_sided = mesh.two_sided;
                part.specular = mesh.specular_power > 0.0f;
                if (part.alpha_blend && part.two_sided) {
                    part.transparent_vertices = std::move(vertices);
                    part.transparent_indices = std::move(mesh.indices);
                }
                m_parts.push_back(std::move(part));
            }
        }
        if (m_parts.empty()) {
            m_error = "No preview meshes selected";
            return false;
        }
        m_center = {(lo[0] + hi[0]) * 0.5f, (lo[1] + hi[1]) * 0.5f, (lo[2] + hi[2]) * 0.5f};
        m_extent = {(hi[0] - lo[0]) * 0.5f, (hi[1] - lo[1]) * 0.5f, (hi[2] - lo[2]) * 0.5f};
        return true;
    }

    bool SkinPreview::capture(const std::string& path, bool back, int width, int height) {
        if (!m_initialized || m_parts.empty() || width < 128 || width > 2048 || height < 128 ||
            height > 2048) {
            m_error = "Invalid preview state or image dimensions";
            return false;
        }
        engine::FrameInput input;
        if (!m_engine->poll(input)) {
            m_error = "Preview window was closed";
            return false;
        }
        m_engine->begin_frame(0.12f, 0.13f, 0.15f);
        if (!m_engine->begin_offscreen(width, height, 0.12f, 0.13f, 0.15f)) {
            m_error = "Cannot create offscreen framebuffer";
            return false;
        }
        constexpr float kFov = 0.55850536f;
        const float aspect = static_cast<float>(width) / static_cast<float>(height);
        const float distance =
            (std::max(m_extent.y, m_extent.x / aspect) / std::tan(kFov * 0.5f) + m_extent.z) *
            1.12f;
        const engine::Vec3 eye{m_center.x, m_center.y, m_center.z + (back ? distance : -distance)};
        const auto view =
            engine::Mat4::multiply(engine::Mat4::scaling(-1.0f, 1.0f, 1.0f),
                                   engine::Mat4::look_at(eye, m_center, {0.0f, 1.0f, 0.0f}));
        m_engine->set_camera(
            view,
            engine::Mat4::perspective(kFov, aspect, 0.05f, distance + m_extent.z * 4.0f + 100.0f));
        for (int pass = 0; pass < 2; ++pass) {
            for (const auto& part : m_parts) {
                if (part.alpha_blend == (pass == 1)) {
                    if (part.alpha_blend && part.two_sided && !part.specular) {
                        const auto batch =
                            engine::build_transparent_mesh_batch(part.transparent_vertices,
                                                                 part.transparent_indices,
                                                                 engine::Mat4::identity(),
                                                                 view,
                                                                 1.0f);
                        m_engine->draw_sprite_batch(batch, part.texture, true, false);
                        continue;
                    }
                    m_engine->draw_mesh_textured(part.mesh,
                                                 part.texture,
                                                 engine::Mat4::identity(),
                                                 part.alpha_blend,
                                                 part.two_sided,
                                                 part.alpha_test && !part.alpha_blend);
                }
            }
        }
        std::vector<std::uint8_t> rgba;
        if (!m_engine->end_offscreen_rgba(rgba) ||
            rgba.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4) {
            m_error = "Cannot read rendered pixels";
            return false;
        }
        for (std::size_t i = 3; i < rgba.size(); i += 4) {
            rgba[i] = 255;
        }
        png_image image{};
        image.version = PNG_IMAGE_VERSION;
        image.width = static_cast<png_uint_32>(width);
        image.height = static_cast<png_uint_32>(height);
        image.format = PNG_FORMAT_RGBA;
        if (!png_image_write_to_file(&image, path.c_str(), 0, rgba.data(), 0, nullptr)) {
            m_error = "Cannot write PNG: " + path;
            return false;
        }
        return true;
    }

    const std::string& SkinPreview::error() const {
        return m_error;
    }
}  // namespace openkal::tools
