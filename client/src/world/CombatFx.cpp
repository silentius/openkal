/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/CombatFx.h"

#include "openkal/client/Audio.h"
#include "openkal/client/SkillEffectTable.h"
#include "openkal/client/SpriteFx.h"
#include "openkal/platform/File.h"
#include "openkal/platform/Log.h"
#include "openkal/platform/Path.h"
#include "openkal/protocol/Attack.h"
#include "openkal/terrain/GtxLoader.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <utility>

namespace openkal::client {
    namespace {

        float frand(float lo, float hi) {
            return lo +
                   (static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX)) * (hi - lo);
        }

        void normalize3(float& x, float& y, float& z) {
            const float len = std::sqrt(x * x + y * y + z * z);
            if (len < 1e-5f) {
                x = 0.0f;
                y = 0.0f;
                z = 1.0f;
                return;
            }
            x /= len;
            y /= len;
            z /= len;
        }

        // compatibility behavior FrameMove: skip degenerate dest-pos so crashed arrows keep facing.
        void set_arrow_facing(float dx, float dy, float dz, float& yaw, float& pitch) {
            const float horiz = std::sqrt(dx * dx + dz * dz);
            if (std::fabs(dx) > 1e-5f || std::fabs(dz) > 1e-5f) {
                yaw = std::atan2(-dx, -dz);
            }
            if (std::fabs(dy) < 1e-5f && horiz < 1e-5f) {
                return;
            }
            if (horiz < 1e-5f) {
                pitch = (dy > 0.0f) ? 1.57079637f : -1.57079637f;
                return;
            }
            pitch = std::atan2(dy, horiz);
        }

        void rotate_x_d3d(float a, float& x, float& y, float& z) {
            const float c = std::cos(a);
            const float s = std::sin(a);
            const float ny = y * c - z * s;
            const float nz = y * s + z * c;
            y = ny;
            z = nz;
        }

        void rotate_y_d3d(float a, float& x, float& y, float& z) {
            const float c = std::cos(a);
            const float s = std::sin(a);
            const float nx = x * c + z * s;
            const float nz = -x * s + z * c;
            x = nx;
            z = nz;
        }

        void rotate_z_d3d(float a, float& x, float& y, float& z) {
            const float c = std::cos(a);
            const float s = std::sin(a);
            const float nx = x * c - y * s;
            const float ny = x * s + y * c;
            x = nx;
            y = ny;
        }

        [[nodiscard]] std::string ascii_lower(std::string_view s) {
            std::string out(s);
            for (char& c : out) {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            return out;
        }

        void
        rotate_axis(float ax, float ay, float az, float angle, float& vx, float& vy, float& vz) {
            const float c = std::cos(angle);
            const float s = std::sin(angle);
            const float ic = 1.0f - c;
            const float dot = ax * vx + ay * vy + az * vz;
            const float cx = ay * vz - az * vy;
            const float cy = az * vx - ax * vz;
            const float cz = ax * vy - ay * vx;
            vx = vx * c + cx * s + ax * dot * ic;
            vy = vy * c + cy * s + ay * dot * ic;
            vz = vz * c + cz * s + az * dot * ic;
        }

        [[nodiscard]] engine::Mat4 yaw_pitch_roll(float yaw, float pitch, float roll) {
            const float hy = yaw * 0.5f;
            const float hp = pitch * 0.5f;
            const float hr = roll * 0.5f;
            const auto Ry = engine::Mat4::from_quaternion(0.0f, std::sin(hy), 0.0f, std::cos(hy));
            const auto Rx = engine::Mat4::from_quaternion(std::sin(hp), 0.0f, 0.0f, std::cos(hp));
            const auto Rz = engine::Mat4::from_quaternion(0.0f, 0.0f, std::sin(hr), std::cos(hr));
            return engine::Mat4::multiply(Ry, engine::Mat4::multiply(Rx, Rz));
        }

        [[nodiscard]] engine::Mat4
        arrow_model_matrix(float x, float y, float z, float yaw, float pitch) {
            const auto R = yaw_pitch_roll(yaw, pitch, 0.0f);
            const auto S = engine::Mat4::scaling(1.0f, 1.0f, 1.0f);
            const auto T = engine::Mat4::translation(x, y, z);
            return engine::Mat4::multiply(T, engine::Mat4::multiply(R, S));
        }

        // Spine1 sits inside the torso. Pull the crash toward the shooter so the
        // stuck mesh enters from that side instead of a bone-local default.
        void
        pull_impact_toward_origin(float ox, float oz, float& tx, float& tz, float impact_scale) {
            const float dx = ox - tx;
            const float dz = oz - tz;
            const float len = std::sqrt(dx * dx + dz * dz);
            if (len < 0.05f) {
                return;
            }
            const float depth = std::clamp(impact_scale * 5.0f, 5.0f, 9.0f);
            const float step = std::min(depth, std::max(0.0f, len - 1.0f));
            tx += dx / len * step;
            tz += dz / len * step;
        }

        // Flattens a compatibility behavior mesh into the engine vertex layout (position + one UV
        // set).
        std::vector<engine::VertexP3T2> to_engine_verts(const terrain::GbMeshCpu& mesh) {
            std::vector<engine::VertexP3T2> verts;
            verts.reserve(mesh.pos_uv.size() / 5);
            for (std::size_t v = 0; v + 4 < mesh.pos_uv.size(); v += 5) {
                verts.push_back({mesh.pos_uv[v],
                                 mesh.pos_uv[v + 1],
                                 mesh.pos_uv[v + 2],
                                 mesh.pos_uv[v + 3],
                                 mesh.pos_uv[v + 4]});
            }
            return verts;
        }

        engine::TextureHandle
        load_effect_tex(engine::IEngine& engine,
                        const std::string& data_root,
                        const std::string& name,
                        std::unordered_map<std::string, engine::TextureHandle>& cache) {
            auto it = cache.find(name);
            if (it != cache.end()) {
                return it->second;
            }
            terrain::ImageRgba img;
            engine::TextureHandle tex{};
            if (terrain::load_effect_tex_rgba(data_root, name, img) && !img.rgba.empty()) {
                tex = engine.create_texture_rgba8(img.rgba.data(), img.width, img.height, true);
            }
            if (!tex) {
                std::uint8_t px[4] = {160, 220, 255, 255};
                tex = engine.create_texture_rgba8(px, 1, 1, true);
                log::warn("CombatFx: missing Effect texture " + name);
            }
            cache[name] = tex;
            return tex;
        }

        void fill_heal_donut_mesh(std::vector<engine::VertexP3T2>& verts,
                                  std::vector<std::uint32_t>& indices) {
            constexpr int kTess = 4;
            constexpr int kNumPoints = kTess * 4 + 1;
            constexpr float kRadius = 5.0f;
            constexpr float kY0 = 0.0f;
            constexpr float kY1 = 30.0f;
            constexpr float kTiling = 4.0f;
            constexpr float kPi = 3.14159265f;
            verts.clear();
            indices.clear();
            verts.reserve(static_cast<std::size_t>(kNumPoints) * 2);
            for (int i = 0; i < kNumPoints; ++i) {
                const float ang =
                    kPi * 2.0f * static_cast<float>(i) / static_cast<float>(kNumPoints - 1);
                const float cx = std::sin(ang) * kRadius;
                const float cz = std::cos(ang) * kRadius;
                const float u = static_cast<float>(i) / kTiling;
                verts.push_back({cx, kY0, cz, u, 0.0f});
                verts.push_back({cx, kY1, cz, u, 1.0f});
            }
            for (int i = 0; i + 1 < kNumPoints; ++i) {
                const auto b0 = static_cast<std::uint32_t>(i * 2);
                const auto t0 = b0 + 1;
                const auto b1 = static_cast<std::uint32_t>((i + 1) * 2);
                const auto t1 = b1 + 1;
                indices.push_back(b0);
                indices.push_back(t0);
                indices.push_back(b1);
                indices.push_back(t0);
                indices.push_back(t1);
                indices.push_back(b1);
            }
        }

        constexpr int kIceNovaTess = 6;
        constexpr int kIceNovaPoints = kIceNovaTess * 4 + 1;
        constexpr float kIceNovaOffsetY = 10.0f;
        constexpr float kIceNovaTiling = 4.0f;
        constexpr float kIceNovaOriginDrop = 1.0f;

        void
        fill_ice_nova_verts(std::vector<engine::VertexP3T2>& verts, float radius1, float radius2) {
            constexpr float kPi = 3.14159265f;
            verts.clear();
            verts.reserve(static_cast<std::size_t>(kIceNovaPoints) * 2);
            for (int i = 0; i < kIceNovaPoints; ++i) {
                const float ang =
                    kPi * 2.0f * static_cast<float>(i) / static_cast<float>(kIceNovaPoints - 1);
                const float sx = std::sin(ang);
                const float cz = std::cos(ang);
                const float u = static_cast<float>(i) / kIceNovaTiling;
                verts.push_back({sx * radius1, kIceNovaOffsetY, cz * radius1, u, 0.0f});
                verts.push_back({sx * radius2, kIceNovaOffsetY, cz * radius2, u, 1.0f});
            }
        }

        /** Skill/{stem}.gb if that file exists, otherwise Effect/{stem}.gb. */
        [[nodiscard]] std::string resolve_clip_gb(const std::string& data_root,
                                                  std::string_view key) {
            const std::string stem(key);
            const std::string skill =
                platform::resolve_client_asset(data_root, std::string("Skill/") + stem, ".gb");
            if (!skill.empty() && platform::File::exists(skill)) {
                return skill;
            }
            const std::string effect =
                platform::resolve_client_asset(data_root, std::string("Effect/") + stem, ".gb");
            if (!effect.empty() && platform::File::exists(effect)) {
                return effect;
            }
            return {};
        }

        void fill_ice_nova_indices(std::vector<std::uint32_t>& indices) {
            indices.clear();
            for (int i = 0; i + 1 < kIceNovaPoints; ++i) {
                const auto b0 = static_cast<std::uint32_t>(i * 2);
                const auto t0 = b0 + 1;
                const auto b1 = static_cast<std::uint32_t>((i + 1) * 2);
                const auto t1 = b1 + 1;
                indices.push_back(b0);
                indices.push_back(t0);
                indices.push_back(b1);
                indices.push_back(t0);
                indices.push_back(t1);
                indices.push_back(b1);
            }
        }

    }  // namespace

    bool SkillModelCache::load(engine::IEngine& engine,
                               const std::string& gb_path,
                               std::unordered_map<std::string, engine::TextureHandle>& tex_cache,
                               bool animate_materials) {
        *this = SkillModelCache{};
        terrain::GbModel cpu;
        if (!terrain::load_gb(gb_path, cpu) || cpu.meshes.empty()) {
            log::warn("CombatFx: failed to load skill GB " + gb_path);
            return false;
        }
        m_bind_meshes = cpu.meshes;
        m_bones = cpu.bones;
        if (!cpu.anims.empty()) {
            m_anim = cpu.anims[0];
        }
        // A single tiny quad is a sprite card, not a mesh — those always face the
        // camera.
        m_billboard = m_bind_meshes.size() == 1 && m_bind_meshes[0].pos_uv.size() <= 5 * 4 + 1;
        m_parts.resize(m_bind_meshes.size());
        for (std::size_t i = 0; i < m_bind_meshes.size(); ++i) {
            const auto& mesh = m_bind_meshes[i];
            auto& part = m_parts[i];
            part.set_alpha_blend(mesh.alpha_blend);
            part.set_alpha_test(mesh.alpha_test);
            part.set_two_sided(true);
            std::string tex_name;
            if (mesh.material_index >= 0 &&
                mesh.material_index < static_cast<int>(cpu.materials.size())) {
                tex_name = cpu.materials[static_cast<std::size_t>(mesh.material_index)].texture;
            }
            if (!tex_name.empty()) {
                auto it = tex_cache.find(tex_name);
                if (it != tex_cache.end()) {
                    part.set_texture(it->second);
                } else {
                    terrain::ImageRgba img;
                    if (terrain::load_gb_texture(gb_path, tex_name, img) && !img.rgba.empty()) {
                        part.set_texture(engine.create_texture_rgba8(
                            img.rgba.data(), img.width, img.height, true));
                        tex_cache[tex_name] = part.texture();
                    }
                }
            }
            if (!part.texture()) {
                std::uint8_t px[4] = {180, 220, 255, 200};
                part.set_texture(engine.create_texture_rgba8(px, 1, 1, true));
            }
        }
        if (animate_materials) {
            m_materials = std::move(cpu.materials);
            m_material_key_times = std::move(cpu.material_key_times);
        }
        m_ready = true;
        return true;
    }

    void SkillModelCache::sample_material(
        std::size_t part_index, float time_ms, float& u, float& v, float& opacity) const {
        const int index = m_bind_meshes[part_index].material_index;
        if (index >= 0 && static_cast<std::size_t>(index) < m_materials.size()) {
            (void) terrain::sample_material_uv(m_materials[static_cast<std::size_t>(index)],
                                               m_material_key_times,
                                               time_ms,
                                               u,
                                               v,
                                               opacity,
                                               false);
        }
    }

    CombatFx::Bolt::Bolt(float ax,
                         float ay,
                         float az,
                         float bx,
                         float by,
                         float bz,
                         int segments,
                         LightningKind kind)
        : m_ax(ax), m_ay(ay), m_az(az), m_bx(bx), m_by(by), m_bz(bz), m_segments(segments),
          m_kind(kind) {}

    CombatFx::HealDonut::HealDonut(float x, float y, float z, std::uint32_t follow_entity)
        : m_x(x), m_y(y), m_z(z), m_follow_entity(follow_entity) {}

    CombatFx::IceNova::IceNova(float x, float y, float z, std::uint32_t follow_entity)
        : m_x(x), m_y(y), m_z(z), m_follow_entity(follow_entity) {}

    CombatFx::StoneShieldPrep::StoneShieldPrep(float x,
                                               float y,
                                               float z,
                                               std::uint32_t follow_entity)
        : m_x(x), m_y(y), m_z(z), m_follow_entity(follow_entity) {
        m_angles[0] = 0.0f;
        m_angles[1] = 3.14159265f * 0.5f;
        m_angles[2] = 3.14159265f;
    }

    void CombatFx::StoneShieldPrep::advance(float dt) {
        m_age += dt;
        for (int i = 0; i < kRibbons; ++i) {
            m_angles[static_cast<std::size_t>(i)] += dt * 5.0f;
        }
    }

    void CombatFx::StoneShieldPrep::build_ribbon(int ribbon,
                                                 std::array<float, kPoints>& xs,
                                                 std::array<float, kPoints>& ys,
                                                 std::array<float, kPoints>& zs,
                                                 std::array<float, kPoints>& as) {
        const std::size_t ri = static_cast<std::size_t>(ribbon);
        const float fade = 1.0f - (m_age / m_life);
        for (int j = kPoints - 1; j > 0; --j) {
            const std::size_t i = static_cast<std::size_t>(j);
            m_px[ri][i] = m_px[ri][i - 1];
            m_py[ri][i] = m_py[ri][i - 1];
            m_pz[ri][i] = m_pz[ri][i - 1];
        }
        float vx = 0.0f;
        float vy = 0.0f;
        float vz = 0.0f;
        if (ribbon == 0) {
            vx = 6.0f;
            rotate_y_d3d(m_angles[0], vx, vy, vz);
            rotate_z_d3d(m_age * 2.0f, vx, vy, vz);
        } else if (ribbon == 1) {
            vy = 6.0f;
            rotate_z_d3d(m_angles[1], vx, vy, vz);
            rotate_x_d3d(m_age * 2.0f, vx, vy, vz);
        } else {
            vz = 6.0f;
            rotate_x_d3d(m_angles[2], vx, vy, vz);
            rotate_y_d3d(m_age * 2.0f, vx, vy, vz);
        }
        vy *= 1.5f;
        m_px[ri][0] = vx;
        m_py[ri][0] = vy - 3.0f;
        m_pz[ri][0] = vz;
        for (int j = 0; j < kPoints; ++j) {
            const std::size_t i = static_cast<std::size_t>(j);
            xs[i] = m_px[ri][i] + m_x;
            ys[i] = m_py[ri][i] + m_y;
            zs[i] = m_pz[ri][i] + m_z;
            as[i] = fade;
        }
    }

    CombatFx::MeditationFx::MeditationFx(float x, float y, float z, std::uint32_t follow_entity)
        : m_x(x), m_y(y), m_z(z), m_follow_entity(follow_entity) {
        m_angles[0] = 0.0f;
        m_angles[1] = 3.14159265f * 0.5f;
    }

    void CombatFx::MeditationFx::advance(float dt) {
        m_age += dt;
        for (int i = 0; i < kRibbons; ++i) {
            m_angles[static_cast<std::size_t>(i)] += dt * 8.0f;
        }
    }

    void CombatFx::MeditationFx::build_ribbon(int ribbon,
                                              std::array<float, kPoints>& xs,
                                              std::array<float, kPoints>& ys,
                                              std::array<float, kPoints>& zs,
                                              std::array<float, kPoints>& as) {
        const std::size_t ri = static_cast<std::size_t>(ribbon);
        const float fade = 1.0f - (m_age / m_life);
        for (int j = kPoints - 1; j > 0; --j) {
            const std::size_t i = static_cast<std::size_t>(j);
            m_px[ri][i] = m_px[ri][i - 1];
            m_py[ri][i] = m_py[ri][i - 1];
            m_pz[ri][i] = m_pz[ri][i - 1];
        }
        const float s = ribbon == 0 ? std::sin(m_age) : -std::sin(m_age);
        float vx = 1.0f + (1.0f - std::abs(s)) * 7.0f;
        float vy = 8.0f * s;
        float vz = 0.0f;
        rotate_y_d3d(m_angles[ri], vx, vy, vz);
        m_px[ri][0] = vx;
        m_py[ri][0] = vy - 3.0f;
        m_pz[ri][0] = vz;
        for (int j = 0; j < kPoints; ++j) {
            const std::size_t i = static_cast<std::size_t>(j);
            xs[i] = m_px[ri][i] + m_x;
            ys[i] = m_py[ri][i] + m_y;
            zs[i] = m_pz[ri][i] + m_z;
            as[i] = fade;
        }
    }

    CombatFx::Particle::Particle(float x,
                                 float y,
                                 float z,
                                 float vx,
                                 float vy,
                                 float vz,
                                 float life,
                                 float size0,
                                 float size1,
                                 float r,
                                 float g,
                                 float b,
                                 float a0,
                                 float rot,
                                 float rot_speed,
                                 engine::TextureHandle tex)
        : m_x(x), m_y(y), m_z(z), m_vx(vx), m_vy(vy), m_vz(vz), m_life(life), m_size0(size0),
          m_size1(size1), m_r(r), m_g(g), m_b(b), m_a0(a0), m_rot(rot), m_rot_speed(rot_speed),
          m_tex(tex) {}

    void CombatFx::Particle::advance(float dt) {
        m_age += dt;
        m_x += m_vx * dt;
        m_y += m_vy * dt;
        m_z += m_vz * dt;
        m_rot += m_rot_speed * dt;
        if (m_age >= m_life) {
            m_alive = false;
        }
    }

    CombatFx::Arrow::Arrow(SkillEt effect,
                           float x,
                           float y,
                           float z,
                           float tx,
                           float ty,
                           float tz,
                           float extra,
                           std::uint32_t target_entity,
                           std::uint8_t flags,
                           bool visible,
                           float impact_scale,
                           std::uint64_t feedback_token)
        : m_effect(effect), m_x(x), m_y(y), m_z(z), m_tx(tx), m_ty(ty), m_tz(tz), m_src_x(x),
          m_src_y(y), m_src_z(z), m_lin_x(x), m_lin_y(y), m_lin_z(z), m_extra(extra),
          m_target_entity(target_entity), m_feedback_token(feedback_token), m_flags(flags),
          m_visible(visible), m_impact_scale(impact_scale > 0.0f ? impact_scale : 1.0f) {
        if (visible) {
            pull_impact_toward_origin(x, z, m_tx, m_tz, m_impact_scale);
        }
        const float dx = m_tx - x;
        const float dy = m_ty - y;
        const float dz = m_tz - z;
        m_dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (m_dist < 1.0f) {
            m_dist = 1.0f;
        }
        if (effect == SkillEt::MysticArrow) {
            m_parabola_height = kArcherMysticParabolaHeight;
            m_parabola_angle = extra * 6.2831853f;
        }
        if (!visible) {
            m_speed = 200.0f;
            m_crash_timer = 0.0f;
        }
        switch (effect) {
        case SkillEt::NegativeArrow:
        case SkillEt::ChargedArrow:
            m_r = 1.0f;
            m_g = 1.0f;
            m_b = 0.0f;
            break;
        case SkillEt::MysticArrow:
            m_r = 60.0f / 255.0f;
            m_g = 1.0f;
            m_b = 120.0f / 255.0f;
            break;
        case SkillEt::PoisonArrow:
            m_r = 0.0f;
            m_g = 1.0f;
            m_b = 0.0f;
            break;
        case SkillEt::ImmolationArrow:
        case SkillEt::FireArrow:
            m_r = 1.0f;
            m_g = 0.0f;
            m_b = 0.0f;
            break;
        case SkillEt::ExplosionArrow:
            m_r = 1.0f;
            m_g = 0.5f;
            m_b = 0.0f;
            break;
        case SkillEt::ExploderArrow:
            m_r = 1.0f;
            m_g = 0.35f;
            m_b = 0.1f;
            break;
        case SkillEt::StunningArrow:
            m_r = 1.0f;
            m_g = 0.25f;
            m_b = 0.0f;
            break;
        case SkillEt::SilentShot:
            m_r = 0.0f;
            m_g = 0.0f;
            m_b = 0.0f;
            m_additive = false;
            break;
        default:
            break;
        }
        set_arrow_facing(m_tx - x, m_ty - y, m_tz - z, m_yaw, m_pitch);
        m_trail_x.fill(x);
        m_trail_y.fill(y);
        m_trail_z.fill(z);
        m_trail_a.fill(1.0f);
    }

    void CombatFx::Arrow::step(float dt) {
        m_age += dt;
        // compatibility behavior shifts the ghost_grey ribbon every frame, including after
        // crash, so the streak retracts into the impact instead of freezing in place.
        for (int i = kTrailPoints - 1; i > 0; --i) {
            const std::size_t dst = static_cast<std::size_t>(i);
            const std::size_t src = static_cast<std::size_t>(i - 1);
            m_trail_x[dst] = m_trail_x[src];
            m_trail_y[dst] = m_trail_y[src];
            m_trail_z[dst] = m_trail_z[src];
            m_trail_a[dst] = 1.0f - static_cast<float>(i) / static_cast<float>(kTrailPoints - 1);
        }
        if (m_crashed) {
            m_crash_timer -= dt;
            if (m_crash_timer <= 0.0f) {
                m_alive = false;
            }
        } else {
            const float dx = m_tx - m_lin_x;
            const float dy = m_ty - m_lin_y;
            const float dz = m_tz - m_lin_z;
            const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
            set_arrow_facing(dx, dy, dz, m_yaw, m_pitch);
            const float step_len = m_speed * dt;
            if (dist <= step_len || dist < 1.0f) {
                m_lin_x = m_tx;
                m_lin_y = m_ty;
                m_lin_z = m_tz;
                m_crashed = true;
            } else {
                m_lin_x += dx / dist * step_len;
                m_lin_y += dy / dist * step_len;
                m_lin_z += dz / dist * step_len;
            }
            m_x = m_lin_x;
            m_y = m_lin_y;
            m_z = m_lin_z;
            if (m_parabola_height > 0.0f) {
                float ox = 0.0f;
                float oy = m_parabola_height *
                           std::sin((std::sqrt((m_lin_x - m_src_x) * (m_lin_x - m_src_x) +
                                               (m_lin_y - m_src_y) * (m_lin_y - m_src_y) +
                                               (m_lin_z - m_src_z) * (m_lin_z - m_src_z)) *
                                     3.14159265f) /
                                    m_dist);
                float oz = 0.0f;
                float dirx = m_tx - m_src_x;
                float diry = m_ty - m_src_y;
                float dirz = m_tz - m_src_z;
                normalize3(dirx, diry, dirz);
                rotate_axis(dirx, diry, dirz, m_parabola_angle, ox, oy, oz);
                m_x = m_lin_x + ox;
                m_y = m_lin_y + oy;
                m_z = m_lin_z + oz;
            }
        }
        m_trail_x[0] = m_x;
        m_trail_y[0] = m_y;
        m_trail_z[0] = m_z;
        m_trail_a[0] = 1.0f;
    }

    void CombatFx::Arrow::follow_spine(const engine::Mat4& spine) {
        if (!m_crashed || m_target_entity == 0 || (m_flags & 1) == 0) {
            return;
        }
        if (!m_attached) {
            engine::Mat4 inv{};
            if (!engine::Mat4::invert(spine, inv)) {
                return;
            }
            // Face along bow origin → impact so the stuck shaft keeps the shot
            // direction, not the spine bone's facing.
            float yaw = m_yaw;
            float pitch = m_pitch;
            set_arrow_facing(m_x - m_src_x, m_y - m_src_y, m_z - m_src_z, yaw, pitch);
            const engine::Mat4 world = arrow_model_matrix(m_x, m_y, m_z, yaw, pitch);
            const engine::Mat4 extra =
                yaw_pitch_roll(0.0f, frand(-0.2f, 0.2f), frand(-3.14159265f, 3.14159265f));
            m_local = engine::Mat4::multiply(inv, engine::Mat4::multiply(world, extra));
            const float s = m_impact_scale;
            m_local.m[12] += frand(-1.5f, 1.5f) * s;
            m_local.m[13] += frand(-2.0f, 2.0f) * s;
            m_local.m[14] += frand(-1.5f, 1.5f) * s;
            m_attached = true;
        }
        m_world = engine::Mat4::multiply(spine, m_local);
        m_x = m_world.m[12];
        m_y = m_world.m[13];
        m_z = m_world.m[14];
        m_trail_x[0] = m_x;
        m_trail_y[0] = m_y;
        m_trail_z[0] = m_z;
    }

    bool CombatFx::Arrow::take_impact() {
        if (!m_crashed || m_impact_reported) {
            return false;
        }
        m_impact_reported = true;
        return true;
    }

    void CombatFx::Arrow::trail_point(int index, float& ox, float& oy, float& oz, float& oa) const {
        const std::size_t i = static_cast<std::size_t>(std::clamp(index, 0, kTrailPoints - 1));
        ox = m_trail_x[i];
        oy = m_trail_y[i];
        oz = m_trail_z[i];
        oa = m_trail_a[i];
    }

    CombatFx::Spit::Spit(float x, float y, float z, float tx, float ty, float tz)
        : m_x(x), m_y(y), m_z(z), m_tx(tx), m_ty(ty), m_tz(tz), m_para_x(x), m_para_y(y),
          m_para_z(z) {
        float dx = tx - x;
        float dy = ty - y;
        float dz = tz - z;
        m_dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (m_dist < 1.0f) {
            m_dist = 1.0f;
        }
        m_dir_x = dx;
        m_dir_y = dy;
        m_dir_z = dz;
        normalize3(m_dir_x, m_dir_y, m_dir_z);
        float rx = frand(-1.0f, 1.0f);
        float ry = frand(-1.0f, 1.0f);
        float rz = frand(-1.0f, 1.0f);
        m_orbit_x = m_dir_y * rz - m_dir_z * ry;
        m_orbit_y = m_dir_z * rx - m_dir_x * rz;
        m_orbit_z = m_dir_x * ry - m_dir_y * rx;
        normalize3(m_orbit_x, m_orbit_y, m_orbit_z);
        m_orbit_x *= 3.0f;
        m_orbit_y *= 3.0f;
        m_orbit_z *= 3.0f;
        for (int i = 0; i < kTrailPoints; ++i) {
            m_t0x[static_cast<std::size_t>(i)] = x;
            m_t0y[static_cast<std::size_t>(i)] = y;
            m_t0z[static_cast<std::size_t>(i)] = z;
            m_t0a[static_cast<std::size_t>(i)] = 1.0f;
            m_t1x[static_cast<std::size_t>(i)] = x;
            m_t1y[static_cast<std::size_t>(i)] = y;
            m_t1z[static_cast<std::size_t>(i)] = z;
            m_t1a[static_cast<std::size_t>(i)] = 1.0f;
        }
    }

    void CombatFx::Spit::step(float dt, bool home, float hx, float hy, float hz) {
        if (!m_crashed) {
            if (home) {
                m_tx = hx;
                m_ty = hy;
                m_tz = hz;
            }
            float dx = m_tx - m_x;
            float dy = m_ty - m_y;
            float dz = m_tz - m_z;
            float length = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (length < 1e-4f) {
                m_crashed = true;
            } else {
                dx /= length;
                dy /= length;
                dz /= length;
                const float step_len = m_speed * dt;
                if (step_len >= length) {
                    m_x = m_tx;
                    m_y = m_ty;
                    m_z = m_tz;
                    m_crashed = true;
                } else {
                    m_x += dx * step_len;
                    m_y += dy * step_len;
                    m_z += dz * step_len;
                }
                const float nx = m_x;
                const float ny = m_y;
                const float nz = m_z;
                m_dir_x = nx - m_para_x;
                m_dir_y = ny - m_para_y;
                m_dir_z = nz - m_para_z;
                normalize3(m_dir_x, m_dir_y, m_dir_z);
                m_para_x = nx;
                m_para_y = ny;
                m_para_z = nz;
            }
        } else {
            m_crash_timer -= dt;
            if (m_crash_timer <= 0.0f) {
                m_alive = false;
                return;
            }
        }

        // Render() copies the trail once per ~30 FPS present.
        constexpr float kTrailStep = 1.0f / 30.0f;
        m_trail_accum += dt;
        while (m_trail_accum >= kTrailStep) {
            m_trail_accum -= kTrailStep;
            for (int i = kTrailPoints - 1; i > 0; --i) {
                const std::size_t d = static_cast<std::size_t>(i);
                const std::size_t s = static_cast<std::size_t>(i - 1);
                m_t0x[d] = m_t0x[s];
                m_t0y[d] = m_t0y[s];
                m_t0z[d] = m_t0z[s];
                m_t0a[d] = 1.0f - (static_cast<float>(i) / static_cast<float>(kTrailPoints - 1));
                m_t1x[d] = m_t1x[s];
                m_t1y[d] = m_t1y[s];
                m_t1z[d] = m_t1z[s];
                m_t1a[d] = m_t0a[d];
            }
        }
        m_t0x[0] = m_x;
        m_t0y[0] = m_y;
        m_t0z[0] = m_z;
        m_t0a[0] = 1.0f;
        m_t1x[0] = m_x;
        m_t1y[0] = m_y;
        m_t1z[0] = m_z;
        m_t1a[0] = 1.0f;
        if (!m_crashed) {
            constexpr float kOrigFps = 30.0f;
            rotate_axis(
                m_dir_x, m_dir_y, m_dir_z, 0.3f * kOrigFps * dt, m_orbit_x, m_orbit_y, m_orbit_z);
            m_t1x[0] += m_orbit_x;
            m_t1y[0] += m_orbit_y;
            m_t1z[0] += m_orbit_z;
        }
    }

    void CombatFx::Spit::trail_point(
        int trail, int i, float& ox, float& oy, float& oz, float& oa) const {
        const std::size_t idx = static_cast<std::size_t>(std::clamp(i, 0, kTrailPoints - 1));
        if (trail == 0) {
            ox = m_t0x[idx];
            oy = m_t0y[idx];
            oz = m_t0z[idx];
            oa = m_t0a[idx];
            return;
        }
        ox = m_t1x[idx];
        oy = m_t1y[idx];
        oz = m_t1z[idx];
        oa = m_t1a[idx];
    }

    CombatFx::SkillAnim::SkillAnim(const SkillModelCache* model,
                                   float x,
                                   float y,
                                   float z,
                                   float yaw,
                                   float scale,
                                   float life_ms,
                                   bool billboard,
                                   bool follow,
                                   bool cast_fx)
        : m_model(model), m_x(x), m_y(y), m_z(z), m_yaw(yaw), m_scale(scale), m_follow(follow),
          m_cast_fx(cast_fx) {
        const float clip_ms = model ? model->anim().duration_ms() : 0.0f;
        m_life_ms = life_ms > 0 ? life_ms : std::max(200.0f, clip_ms);
        if (clip_ms > 0.0f && m_life_ms > 0.0f && m_life_ms < clip_ms) {
            // Compress the full clip into the shorter life window.
            m_anim_rate = clip_ms / m_life_ms;
        } else {
            m_anim_rate = 1.0f;
        }
        m_follow_billboard = billboard || (model && model->billboard());
    }

    void CombatFx::SkillAnim::create_meshes(engine::IEngine& engine) {
        if (!m_model) {
            return;
        }
        m_work_meshes = m_model->bind_meshes();
        m_live_meshes.resize(m_model->parts().size());
        for (std::size_t i = 0; i < m_model->parts().size(); ++i) {
            const auto verts = to_engine_verts(m_model->bind_meshes()[i]);
            const auto& bind = m_model->bind_meshes()[i];
            m_live_meshes[i] = engine.create_mesh(
                verts.data(), verts.size(), bind.indices.data(), bind.indices.size());
        }
    }

    void CombatFx::SkillAnim::skin(engine::IEngine& engine) {
        if (!m_model || !m_model->ready()) {
            return;
        }
        std::vector<std::array<float, 16>> skin_mats;
        const auto& clip = m_model->anim();
        const float t = clip.keyframe_count > 0 ? m_age_ms * m_anim_rate : 0.0f;
        if (clip.keyframe_count > 0 && !m_model->bones().empty()) {
            if (!terrain::eval_skin_matrices_at_time(
                    m_model->bones(), clip, t, m_cast_fx || m_loop_clip, skin_mats)) {
                return;
            }
        }
        for (std::size_t i = 0; i < m_work_meshes.size(); ++i) {
            m_work_meshes[i] = m_model->bind_meshes()[i];
            if (!skin_mats.empty()) {
                terrain::skin_mesh(m_work_meshes[i], skin_mats);
            }
            const auto verts = to_engine_verts(m_work_meshes[i]);
            if (m_live_meshes[i] && verts.size() == m_model->bind_meshes()[i].pos_uv.size() / 5) {
                engine.update_mesh(m_live_meshes[i], verts.data(), verts.size());
            }
        }
    }

    void CombatFx::SkillAnim::set_flight(float tx, float ty, float tz, float speed) {
        m_flying = true;
        m_follow_billboard = false;
        m_follow = false;
        m_anim_rate = 1.0f;
        m_tx = tx;
        m_ty = ty;
        m_tz = tz;
        m_speed = speed > 0.0f ? speed : 500.0f;
        const float dx = m_tx - m_x;
        const float dy = m_ty - m_y;
        const float dz = m_tz - m_z;
        m_yaw = std::atan2(-dx, -dz);
        const float horiz = std::sqrt(dx * dx + dz * dz);
        m_pitch = std::atan2(dy, std::max(0.001f, horiz));
    }

    void CombatFx::SkillAnim::set_mage_bolt(MageBoltKind kind,
                                            std::uint32_t target_entity,
                                            float impact_scale) {
        m_mage_bolt = kind;
        m_target_entity = target_entity;
        m_impact_scale = impact_scale > 0.0f ? impact_scale : 1.0f;
    }

    void CombatFx::SkillAnim::advance(float dt_ms) {
        m_age_ms += dt_ms;
        if (m_flying) {
            const float dt = dt_ms * 0.001f;
            const float dx = m_tx - m_x;
            const float dy = m_ty - m_y;
            const float dz = m_tz - m_z;
            const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
            m_yaw = std::atan2(-dx, -dz);
            const float horiz = std::sqrt(dx * dx + dz * dz);
            m_pitch = std::atan2(dy, std::max(0.001f, horiz));
            const float step_len = m_speed * dt;
            if (dist <= step_len || dist < 1.0f) {
                m_x = m_tx;
                m_y = m_ty;
                m_z = m_tz;
                m_alive = false;
                return;
            }
            m_x += dx / dist * step_len;
            m_y += dy / dist * step_len;
            m_z += dz / dist * step_len;
            return;
        }
        if (m_cast_fx || m_loop_clip) {
            float period_ms = m_loop_period_ms;
            if (period_ms <= 0.0f) {
                period_ms = m_model ? m_model->anim().duration_ms() : 0.0f;
            }
            if (period_ms > 0.0f && m_age_ms >= period_ms) {
                m_age_ms = std::fmod(m_age_ms, period_ms);
            }
            return;
        }
        if (m_age_ms >= m_life_ms) {
            m_alive = false;
        }
    }

    void CombatFx::SkillAnim::poll_sounds(Audio& audio) {
        if (!m_model || !m_alive) {
            return;
        }
        const float t = m_age_ms * m_anim_rate;
        m_sounds.poll(audio, m_model->anim(), t, m_cast_fx || m_loop_clip, m_x, m_y, m_z);
    }

    void CombatFx::SkillAnim::stop_sounds(Audio& audio) {
        m_sounds.stop(audio);
    }

    void CombatFx::SkillAnim::destroy_meshes(engine::IEngine& engine) {
        for (auto& m : m_live_meshes) {
            if (m) {
                engine.destroy_mesh(m);
                m = {};
            }
        }
    }

    void CombatFx::SkillAnim::draw(engine::IEngine& engine, float camera_yaw_rad) const {
        if (!m_alive || !m_model) {
            return;
        }
        engine::Mat4 M;
        if (m_have_world_matrix) {
            const auto S = engine::Mat4::scaling(m_scale, m_scale, m_scale);
            M = engine::Mat4::multiply(m_world, S);
        } else {
            const float yaw = m_follow_billboard ? camera_yaw_rad : m_yaw;
            const float hy = yaw * 0.5f;
            const auto Ry = engine::Mat4::from_quaternion(0.0f, std::sin(hy), 0.0f, std::cos(hy));
            engine::Mat4 R = Ry;
            if (m_flying) {
                const float hp = m_pitch * 0.5f;
                const auto Rx =
                    engine::Mat4::from_quaternion(std::sin(hp), 0.0f, 0.0f, std::cos(hp));
                R = engine::Mat4::multiply(Ry, Rx);
            }
            const auto S = engine::Mat4::scaling(m_scale, m_scale, m_scale);
            const auto T = engine::Mat4::translation(m_x, m_y, m_z);
            M = engine::Mat4::multiply(T, engine::Mat4::multiply(R, S));
        }
        for (std::size_t i = 0; i < m_live_meshes.size(); ++i) {
            const auto& part = m_model->parts()[i];
            if (!m_live_meshes[i] || !part.texture()) {
                continue;
            }
            float u = 0.0f;
            float v = 0.0f;
            float opacity = 1.0f;
            m_model->sample_material(i, m_age_ms * m_anim_rate, u, v, opacity);
            if (opacity <= 0.0f) {
                continue;
            }
            engine.draw_mesh_textured(m_live_meshes[i],
                                      part.texture(),
                                      M,
                                      true,
                                      true,
                                      false,
                                      true,
                                      u,
                                      v,
                                      1.0f,
                                      1.0f,
                                      1.0f,
                                      opacity);
        }
    }

    bool CombatFx::init(engine::IEngine& engine, const std::string& data_root) {
        m_ready = false;
        m_data_root = data_root;
        m_lightning_tex = load_effect_tex(engine, data_root, "Lightning.GTX", m_tex_cache);
        m_lightning_red_tex = load_effect_tex(engine, data_root, "LightningRed.GTX", m_tex_cache);
        m_lightning_spirit_tex = load_effect_tex(engine, data_root, "Lightning02.GTX", m_tex_cache);
        m_flare_tex = load_effect_tex(engine, data_root, "lig01.GTX", m_tex_cache);
        m_impact_star_tex = load_effect_tex(engine, data_root, "star4.GTX", m_tex_cache);
        m_impact_spark_tex = load_effect_tex(engine, data_root, "LightningSpark.GTX", m_tex_cache);
        m_ghost_blue_tex = load_effect_tex(engine, data_root, "ghost_blue.dds", m_tex_cache);
        m_ghost_grey_tex = load_effect_tex(engine, data_root, "ghost_grey.dds", m_tex_cache);

        const engine::VertexP3T2 quad[4] = {
            {-0.5f, -0.5f, 0, 0, 0},
            {0.5f, -0.5f, 0, 1, 0},
            {0.5f, 0.5f, 0, 1, 1},
            {-0.5f, 0.5f, 0, 0, 1},
        };
        const std::uint32_t qidx[6] = {0, 1, 2, 0, 2, 3};
        m_particle_quad = engine.create_mesh(quad, 4, qidx, 6);
        m_ribbon_mesh = engine.create_mesh(quad, 4, qidx, 6);
        m_heal_tex = load_effect_tex(engine, data_root, "healing.dds", m_tex_cache);
        {
            std::vector<engine::VertexP3T2> verts;
            std::vector<std::uint32_t> indices;
            fill_heal_donut_mesh(verts, indices);
            m_heal_donut_mesh =
                engine.create_mesh(verts.data(), verts.size(), indices.data(), indices.size());
        }
        m_ice_nova_tex = load_effect_tex(engine, data_root, "whiteres.dds", m_tex_cache);
        m_meditation_tex = load_effect_tex(engine, data_root, "GenericGlow.dds", m_tex_cache);
        {
            std::vector<engine::VertexP3T2> verts;
            std::vector<std::uint32_t> indices;
            fill_ice_nova_verts(verts, 0.0f, 0.0f);
            fill_ice_nova_indices(indices);
            m_ice_nova_mesh =
                engine.create_mesh(verts.data(), verts.size(), indices.data(), indices.size());
        }

        const std::string arrow_gb = platform::join_path(data_root, "Item/Weapon/M_arrow.gb");
        terrain::GbModel cpu;
        if (terrain::load_gb(arrow_gb, cpu) && !cpu.meshes.empty()) {
            const auto& mesh = cpu.meshes[0];
            const auto verts = to_engine_verts(mesh);
            m_arrow_mesh = engine.create_mesh(
                verts.data(), verts.size(), mesh.indices.data(), mesh.indices.size());
            std::string tex_name;
            if (mesh.material_index >= 0 &&
                mesh.material_index < static_cast<int>(cpu.materials.size())) {
                tex_name = cpu.materials[static_cast<std::size_t>(mesh.material_index)].texture;
            }
            terrain::ImageRgba aimg;
            if (terrain::load_gb_texture(arrow_gb, tex_name, aimg) && !aimg.rgba.empty()) {
                m_arrow_tex =
                    engine.create_texture_rgba8(aimg.rgba.data(), aimg.width, aimg.height, true);
            } else {
                std::uint8_t px[4] = {160, 140, 100, 255};
                m_arrow_tex = engine.create_texture_rgba8(px, 1, 1, true);
            }
        }
        if (!m_arrow_mesh) {
            // Fallback shaft so a missing M_arrow.gb still shows a flying projectile.
            const engine::VertexP3T2 shaft[8] = {
                {-0.35f, -0.35f, 0.5f, 0, 0},
                {0.35f, -0.35f, 0.5f, 1, 0},
                {0.35f, 0.35f, 0.5f, 1, 1},
                {-0.35f, 0.35f, 0.5f, 0, 1},
                {-0.25f, -0.25f, -24.0f, 0, 0},
                {0.25f, -0.25f, -24.0f, 1, 0},
                {0.25f, 0.25f, -24.0f, 1, 1},
                {-0.25f, 0.25f, -24.0f, 0, 1},
            };
            const std::uint32_t sidx[] = {0, 1, 2, 0, 2, 3, 1, 5, 6, 1, 6, 2, 5, 4, 7, 5, 7, 6,
                                          4, 0, 3, 4, 3, 7, 3, 2, 6, 3, 6, 7, 4, 5, 1, 4, 1, 0};
            m_arrow_mesh = engine.create_mesh(shaft, 8, sidx, sizeof(sidx) / sizeof(sidx[0]));
            if (!m_arrow_tex) {
                std::uint8_t px[4] = {210, 170, 90, 255};
                m_arrow_tex = engine.create_texture_rgba8(px, 1, 1, true);
            }
            log::warn("combat FX: M_arrow.gb missing — using fallback shaft");
        }

        const std::string skill_r40 =
            platform::join_path(platform::join_path(data_root, "Skill"), "Skill_R_4_0.gb");
        if (!m_skill_r4_0.load(engine, skill_r40, m_tex_cache)) {
            log::warn("combat FX: failed to load prep model " + skill_r40);
        }
        m_skill_r4_0.set_billboard(true);

        const std::string skill_r31 =
            platform::join_path(platform::join_path(data_root, "Skill"), "Skill_R_3_1.gb");
        if (!m_skill_r3_1.load(engine, skill_r31, m_tex_cache)) {
            log::warn("combat FX: failed to load ice bolt " + skill_r31);
        }
        m_skill_r3_1.set_billboard(false);

        const std::string skill_r21 =
            platform::join_path(platform::join_path(data_root, "Skill"), "Skill_R_2_1.gb");
        if (!m_skill_r2_1.load(engine, skill_r21, m_tex_cache)) {
            log::warn("combat FX: failed to load fire bolt " + skill_r21);
        }
        m_skill_r2_1.set_billboard(false);

        const std::string skill_m30 =
            platform::join_path(platform::join_path(data_root, "Skill"), "Skill_M_3_0.gb");
        if (!m_skill_m30.load(engine, skill_m30, m_tex_cache)) {
            log::warn("combat FX: failed to load CShot prep " + skill_m30);
        }
        m_skill_m30.set_billboard(false);

        const std::string level_up_gb =
            platform::join_path(platform::join_path(data_root, "Effect"), "effect_levelup.gb");
        if (!m_level_up.load(engine, level_up_gb, m_tex_cache, true)) {
            log::warn("combat FX: failed to load level-up effect " + level_up_gb);
        }

        const std::string behead_gb =
            platform::join_path(platform::join_path(data_root, "Effect"), "effect_behead.gb");
        (void) m_behead.load(engine, behead_gb, m_tex_cache);

        m_ready = true;
        return true;
    }

    void CombatFx::set_audio(Audio* audio) {
        m_audio = audio;
    }

    void CombatFx::set_sprite_fx(SpriteFx* sprite_fx) {
        m_sprite_fx = sprite_fx;
    }

    void CombatFx::play_sfx(std::string_view name, float x, float y, float z) {
        if (!m_audio) {
            return;
        }
        // Always one-shot here. Skill cast loops (`efs019_L` etc.) are owned by
        // main's skill_loop handle via Audio::play; fire-and-forget `_L` through
        // this path used to orphan forever-looping voices (Lightning Blow).
        (void) m_audio->play(Audio::parse_name(name), x, y, z, true);
    }

    void CombatFx::play_clip_loop_sfx(
        std::uint32_t entity, std::string_view name, float x, float y, float z) {
        if (!m_audio || entity == 0 || name.empty()) {
            return;
        }
        auto cue = Audio::parse_name(name);
        if (cue.empty()) {
            return;
        }
        cue.set_looping(true);
        const SoundHandle handle = m_audio->play(cue, x, y, z, false);
        if (!handle.valid()) {
            return;
        }
        m_clip_loop_sound_ids[entity].push_back(handle.id());
    }

    void CombatFx::spawn_level_up(
        engine::IEngine& engine, float x, float y, float z, bool follow, std::uint32_t entity) {
        if (!m_ready || !m_level_up.ready()) {
            return;
        }
        constexpr float kPlaybackMs = 1500.0f;
        spawn_skill_instance(
            engine, m_level_up, x, y, z, 0.0f, 1.0f, kPlaybackMs, false, follow && entity == 0);
        if (entity != 0) {
            m_skill_anims.back().set_clip_follow_entity(entity);
        }
    }

    void CombatFx::spawn_behead(engine::IEngine& engine, float x, float y, float z) {
        if (!m_ready || !m_behead.ready()) {
            return;
        }
        spawn_skill_instance(engine, m_behead, x, y, z, 0.0f, 1.0f, 1200.0f, false, false);
    }

    CombatFx::PendingClipModel::PendingClipModel(std::string name,
                                                 float x,
                                                 float y,
                                                 float z,
                                                 float yaw,
                                                 float delay_s,
                                                 float duration_s,
                                                 std::uint32_t follow_entity,
                                                 bool persist,
                                                 bool loop,
                                                 int effect_bone,
                                                 float ox,
                                                 float oy,
                                                 float oz,
                                                 bool use_effect_bone_rotation)
        : m_name(std::move(name)), m_x(x), m_y(y), m_z(z), m_yaw(yaw), m_delay(delay_s),
          m_duration(duration_s), m_follow_entity(follow_entity), m_persist(persist), m_loop(loop),
          m_effect_bone(effect_bone), m_ox(ox), m_oy(oy), m_oz(oz),
          m_use_effect_bone_rotation(use_effect_bone_rotation) {}

    const std::string& CombatFx::PendingClipModel::name() const {
        return m_name;
    }

    float CombatFx::PendingClipModel::x() const {
        return m_x;
    }

    float CombatFx::PendingClipModel::y() const {
        return m_y;
    }

    float CombatFx::PendingClipModel::z() const {
        return m_z;
    }

    float CombatFx::PendingClipModel::yaw() const {
        return m_yaw;
    }

    float CombatFx::PendingClipModel::delay() const {
        return m_delay;
    }

    void CombatFx::PendingClipModel::set_delay(float v) {
        m_delay = v;
    }

    float CombatFx::PendingClipModel::duration() const {
        return m_duration;
    }

    std::uint32_t CombatFx::PendingClipModel::follow_entity() const {
        return m_follow_entity;
    }

    bool CombatFx::PendingClipModel::persist() const {
        return m_persist;
    }

    bool CombatFx::PendingClipModel::loop() const {
        return m_loop;
    }

    int CombatFx::PendingClipModel::effect_bone() const {
        return m_effect_bone;
    }

    float CombatFx::PendingClipModel::ox() const {
        return m_ox;
    }

    float CombatFx::PendingClipModel::oy() const {
        return m_oy;
    }

    float CombatFx::PendingClipModel::oz() const {
        return m_oz;
    }

    bool CombatFx::PendingClipModel::use_effect_bone_rotation() const {
        return m_use_effect_bone_rotation;
    }

    void CombatFx::spawn_clip_model(engine::IEngine& engine,
                                    std::string_view name,
                                    float x,
                                    float y,
                                    float z,
                                    float yaw,
                                    float delay_s,
                                    float duration_s,
                                    std::uint32_t follow_entity,
                                    bool persist,
                                    bool loop,
                                    int effect_bone,
                                    float ox,
                                    float oy,
                                    float oz,
                                    bool use_effect_bone_rotation) {
        if (!m_ready || name.empty() || m_data_root.empty()) {
            return;
        }
        if (delay_s > 0.01f) {
            m_pending_clips.emplace_back(std::string(name),
                                         x,
                                         y,
                                         z,
                                         yaw,
                                         delay_s,
                                         duration_s,
                                         follow_entity,
                                         persist,
                                         loop,
                                         effect_bone,
                                         ox,
                                         oy,
                                         oz,
                                         use_effect_bone_rotation);
            return;
        }
        const std::string key = ascii_lower(name);
        auto& slot = m_clip_models[key];
        if (!slot) {
            slot = std::make_unique<SkillModelCache>();
            const std::string path = resolve_clip_gb(m_data_root, key);
            if (path.empty() || !slot->load(engine, path, m_tex_cache)) {
                log::warn("combat FX: clip model missing " + key);
            }
        }
        if (!slot->ready()) {
            return;
        }
        const float life_ms = duration_s > 0.0f ? duration_s * 1000.0f : 0.0f;
        spawn_skill_instance(engine, *slot, x, y, z, yaw, 1.0f, life_ms, false, false);
        auto& anim = m_skill_anims.back();
        anim.disable_clip_stretch();
        // FX `model` stages are world GBs (Hail ice, freeze block, twisters). The
        // tiny-quad heuristic would turn them into camera-facing cards.
        anim.set_follow_billboard(false);
        if (follow_entity != 0) {
            anim.set_clip_follow_entity(follow_entity);
            anim.set_clip_effect_bone(effect_bone);
            anim.set_clip_offset(ox, oy, oz);
            anim.set_use_effect_bone_rotation(use_effect_bone_rotation);
        }
        if (persist) {
            anim.hold_until_killed();
        }
        if (loop) {
            anim.loop_until_killed(life_ms);
        }
    }

    void CombatFx::follow_clip_entity(std::uint32_t entity, float x, float y, float z) {
        if (entity == 0) {
            return;
        }
        for (auto& anim : m_skill_anims) {
            if (anim.alive() && anim.clip_follow_entity() == entity) {
                anim.set_world_pos(x, y, z);
            }
        }
    }

    void CombatFx::follow_clip_entity(std::uint32_t entity, float x, float y, float z, float yaw) {
        if (entity == 0) {
            return;
        }
        for (auto& anim : m_skill_anims) {
            if (anim.alive() && anim.clip_follow_entity() == entity) {
                anim.set_world_pos(x, y, z);
                anim.set_yaw(yaw);
            }
        }
    }

    void CombatFx::follow_clip_bones(std::uint32_t entity,
                                     const engine::Mat4& origin,
                                     const engine::Mat4* effect_bones,
                                     int effect_bone_count) {
        if (entity == 0) {
            return;
        }
        for (auto& anim : m_skill_anims) {
            if (!anim.alive() || anim.cast_fx() || anim.clip_follow_entity() != entity) {
                continue;
            }
            engine::Mat4 mat = origin;
            const int bone = anim.clip_effect_bone();
            if (bone >= 0 && effect_bones != nullptr && bone < effect_bone_count) {
                mat = SpriteFx::effect_bone_attach_matrix(
                    origin, effect_bones[bone], anim.use_effect_bone_rotation());
            }
            mat = SpriteFx::offset_in_matrix_axes(
                mat, anim.clip_ox(), anim.clip_oy(), anim.clip_oz());
            anim.set_world_matrix(mat);
        }
        if (effect_bones != nullptr && effect_bone_count > 1) {
            follow_arrows(entity, effect_bones[1]);
        }
    }

    void CombatFx::follow_arrows(std::uint32_t entity, const engine::Mat4& spine) {
        if (entity == 0) {
            return;
        }
        for (auto& a : m_arrows) {
            if (!a.alive() || a.target_entity() != entity) {
                continue;
            }
            a.follow_spine(spine);
        }
    }

    void CombatFx::kill_clip_entity(std::uint32_t entity) {
        if (entity == 0) {
            return;
        }
        for (auto& anim : m_skill_anims) {
            if (anim.alive() && anim.clip_follow_entity() == entity) {
                anim.kill();
            }
        }
        const auto sounds = m_clip_loop_sound_ids.find(entity);
        if (sounds != m_clip_loop_sound_ids.end()) {
            if (m_audio) {
                for (const std::uint32_t id : sounds->second) {
                    m_audio->stop(SoundHandle{id});
                }
            }
            m_clip_loop_sound_ids.erase(sounds);
        }
    }

    void CombatFx::follow_attached(float x, float y, float z) {
        for (auto& s : m_skill_anims) {
            if (s.alive() && s.follows() && !s.cast_fx()) {
                s.set_world_pos(x, y, z);
            }
        }
    }

    void CombatFx::spawn_cast_fx(engine::IEngine& engine,
                                 float x,
                                 float y,
                                 float z,
                                 float yaw,
                                 std::string_view model_name,
                                 std::uint32_t entity) {
        clear_cast_fx(entity);
        if (!m_ready) {
            return;
        }
        const std::string key =
            model_name.empty() ? std::string("skill_m_3_0") : ascii_lower(model_name);
        const SkillModelCache* model = nullptr;
        if (key == "skill_m_3_0" && m_skill_m30.ready()) {
            model = &m_skill_m30;
        } else {
            auto& slot = m_clip_models[key];
            if (!slot) {
                slot = std::make_unique<SkillModelCache>();
                const std::string path = resolve_clip_gb(m_data_root, key);
                if (path.empty() || !slot->load(engine, path, m_tex_cache)) {
                    log::warn("combat FX: cast model missing " + key);
                }
            }
            if (slot && slot->ready()) {
                model = slot.get();
            }
        }
        if (!model) {
            return;
        }
        spawn_skill_instance(engine, *model, x, y, z, yaw, 1.0f, 0.0f, false, false, true);
        m_skill_anims.back().set_clip_follow_entity(entity);
    }

    void CombatFx::follow_cast_fx(float x, float y, float z, float yaw, std::uint32_t entity) {
        for (auto& s : m_skill_anims) {
            if (s.alive() && s.cast_fx() && s.clip_follow_entity() == entity) {
                s.set_world_pos(x, y, z);
                s.set_yaw(yaw);
            }
        }
    }

    void CombatFx::clear_cast_fx(std::uint32_t entity) {
        for (auto& s : m_skill_anims) {
            if (s.cast_fx() && s.clip_follow_entity() == entity) {
                s.kill();
            }
        }
    }

    void CombatFx::spawn_heal_donut(float x, float y, float z, std::uint32_t follow_entity) {
        if (!m_ready || !m_heal_donut_mesh) {
            return;
        }
        m_heal_donuts.emplace_back(x, y, z, follow_entity);
    }

    void CombatFx::follow_heal_donut(std::uint32_t entity, float x, float y, float z) {
        if (entity == 0) {
            return;
        }
        for (auto& donut : m_heal_donuts) {
            if (donut.alive() && donut.follow_entity() == entity) {
                donut.set_world_pos(x, y, z);
            }
        }
    }

    void CombatFx::spawn_ice_nova(float x, float y, float z, std::uint32_t follow_entity) {
        if (!m_ready || !m_ice_nova_mesh) {
            return;
        }
        play_sfx("efs017", x, y, z);
        m_ice_novas.emplace_back(x, y, z, follow_entity);
    }

    void CombatFx::follow_ice_nova(std::uint32_t entity, float x, float y, float z) {
        if (entity == 0) {
            return;
        }
        for (auto& nova : m_ice_novas) {
            if (nova.alive() && nova.follow_entity() == entity) {
                nova.set_world_pos(x, y, z);
            }
        }
    }

    void CombatFx::spawn_stone_shield_prep(float x, float y, float z, std::uint32_t follow_entity) {
        if (!m_ready) {
            return;
        }
        m_stone_shields.emplace_back(x, y, z, follow_entity);
    }

    void CombatFx::follow_stone_shield_prep(std::uint32_t entity, float x, float y, float z) {
        if (entity == 0) {
            return;
        }
        for (auto& prep : m_stone_shields) {
            if (prep.alive() && prep.follow_entity() == entity) {
                prep.set_world_pos(x, y, z);
            }
        }
    }

    void CombatFx::spawn_meditation(float x, float y, float z, std::uint32_t follow_entity) {
        if (!m_ready) {
            return;
        }
        m_meditations.emplace_back(x, y, z, follow_entity);
    }

    void CombatFx::follow_meditation(std::uint32_t entity, float x, float y, float z) {
        if (entity == 0) {
            return;
        }
        for (auto& fx : m_meditations) {
            if (fx.alive() && fx.follow_entity() == entity) {
                fx.set_world_pos(x, y, z);
            }
        }
    }

    bool CombatFx::take_projectile_crash(ProjectileCrash& out) {
        if (m_projectile_crashes.empty()) {
            return false;
        }
        out = m_projectile_crashes.front();
        m_projectile_crashes.erase(m_projectile_crashes.begin());
        return true;
    }

    void CombatFx::spawn_skill_instance(engine::IEngine& engine,
                                        const SkillModelCache& model,
                                        float x,
                                        float y,
                                        float z,
                                        float yaw,
                                        float scale,
                                        float life_ms,
                                        bool billboard,
                                        bool follow,
                                        bool cast_fx) {
        if (!model.ready()) {
            return;
        }
        SkillAnim anim(&model, x, y, z, yaw, scale, life_ms, billboard, follow, cast_fx);
        anim.create_meshes(engine);
        anim.skin(engine);
        m_skill_anims.push_back(std::move(anim));
    }

    void CombatFx::spawn_particle(float x,
                                  float y,
                                  float z,
                                  float vx,
                                  float vy,
                                  float vz,
                                  float life,
                                  float size0,
                                  float size1,
                                  float r,
                                  float g,
                                  float b,
                                  float a0,
                                  float rot_speed,
                                  engine::TextureHandle tex) {
        m_particles.emplace_back(
            x, y, z, vx, vy, vz, life, size0, size1, r, g, b, a0, frand(0, 6.28f), rot_speed, tex);
    }

    void CombatFx::spawn_lightning(engine::IEngine& engine,
                                   float tip_x,
                                   float tip_y,
                                   float tip_z,
                                   float bx,
                                   float by,
                                   float bz,
                                   float /*impact_scale*/,
                                   int beam_count,
                                   LightningKind kind) {
        const bool red = kind == LightningKind::Red;
        const bool spirit = kind == LightningKind::Spirit;
        play_sfx("efs021", tip_x, tip_y, tip_z);
        const float dx = bx - tip_x;
        const float dz = bz - tip_z;
        const float yaw = std::atan2(-dx, -dz);

        if (!red && !spirit) {
            const float prep_ms = m_skill_r4_0.anim().duration_ms();
            spawn_skill_instance(engine,
                                 m_skill_r4_0,
                                 tip_x,
                                 tip_y,
                                 tip_z,
                                 yaw,
                                 1.0f,
                                 prep_ms > 0 ? prep_ms : 1000.0f,
                                 true);
        }

        const float len = std::sqrt(dx * dx + (by - tip_y) * (by - tip_y) + dz * dz);
        const int segments = std::clamp(static_cast<int>(len / 18.0f), 3, 24);
        const int beams = std::max(1, beam_count);
        for (int i = 0; i < beams; ++i) {
            m_bolts.emplace_back(tip_x, tip_y, tip_z, bx, by, bz, segments, kind);
        }

        if (red && m_sprite_fx) {
            m_sprite_fx->spawn_oneshot_particle(engine, "lightningRed_flare", tip_x, tip_y, tip_z);
            m_sprite_fx->spawn_oneshot_particle(engine, "lightningRed_impact", bx, by, bz);
            return;
        }

        if (spirit && m_sprite_fx) {
            m_sprite_fx->spawn_oneshot_particle(
                engine, "Player_EF_code_EF106", tip_x, tip_y, tip_z);
            m_sprite_fx->spawn_oneshot_particle(engine, "lightning_impact", bx, by, bz);
            return;
        }

        // lightning_flare @ tip — jusulsa: count 2, size 14→8, color (0.6,1,0.8),
        // life 1.0
        for (int i = 0; i < 2; ++i) {
            spawn_particle(tip_x,
                           tip_y,
                           tip_z,
                           0,
                           0,
                           0,
                           1.0f,
                           14.0f,
                           8.0f,
                           0.6f,
                           1.0f,
                           0.8f,
                           1.0f,
                           10.0f,
                           m_flare_tex);
        }

        // lightning_impact @ spine — layer1 star: count 2, size 2→60, color
        // (0.5,0.8,1), life 0.4
        for (int i = 0; i < 2; ++i) {
            spawn_particle(bx,
                           by,
                           bz,
                           0,
                           0,
                           0,
                           0.4f,
                           2.0f,
                           60.0f,
                           0.5f,
                           0.8f,
                           1.0f,
                           1.0f,
                           10.0f,
                           m_impact_star_tex);
        }
        // layer2 sparks: count 20, size 3, speed 28 outward, life 0.35, white
        for (int i = 0; i < 20; ++i) {
            const float theta = frand(0, 6.2831853f);
            const float phi = frand(0, 3.14159265f);
            const float sp = 28.0f;
            const float vx = sp * std::sin(phi) * std::cos(theta);
            const float vy = sp * std::cos(phi);
            const float vz = sp * std::sin(phi) * std::sin(theta);
            spawn_particle(bx,
                           by,
                           bz,
                           vx,
                           vy,
                           vz,
                           0.35f,
                           3.0f,
                           0.0f,
                           1.0f,
                           1.0f,
                           1.0f,
                           1.0f,
                           0.0f,
                           m_impact_spark_tex);
        }
    }

    void CombatFx::spawn_ice_bolt(engine::IEngine& engine,
                                  float tip_x,
                                  float tip_y,
                                  float tip_z,
                                  float bx,
                                  float by,
                                  float bz,
                                  std::uint32_t target_entity,
                                  float impact_scale) {
        play_sfx("efs015", tip_x, tip_y, tip_z);
        const float dx = bx - tip_x;
        const float dz = bz - tip_z;
        const float yaw = std::atan2(-dx, -dz);
        const SkillModelCache* model = m_skill_r3_1.ready() ? &m_skill_r3_1 : nullptr;
        SkillAnim anim(model, tip_x, tip_y, tip_z, yaw, 1.0f, 0.0f, false, false);
        anim.set_flight(bx, by, bz, 500.0f);
        anim.set_mage_bolt(MageBoltKind::Ice, target_entity, impact_scale);
        if (model) {
            anim.create_meshes(engine);
            anim.skin(engine);
        }
        m_skill_anims.push_back(std::move(anim));
    }

    void CombatFx::spawn_fire_bolt(engine::IEngine& engine,
                                   float tip_x,
                                   float tip_y,
                                   float tip_z,
                                   float bx,
                                   float by,
                                   float bz,
                                   std::uint32_t target_entity,
                                   float impact_scale) {
        play_sfx("efs010", tip_x, tip_y, tip_z);
        const float dx = bx - tip_x;
        const float dz = bz - tip_z;
        const float yaw = std::atan2(-dx, -dz);
        const SkillModelCache* model = m_skill_r2_1.ready() ? &m_skill_r2_1 : nullptr;
        SkillAnim anim(model, tip_x, tip_y, tip_z, yaw, 1.0f, 0.0f, false, false);
        anim.set_flight(bx, by, bz, 500.0f);
        anim.set_mage_bolt(MageBoltKind::Fire, target_entity, impact_scale);
        if (model) {
            anim.create_meshes(engine);
            anim.skin(engine);
        }
        m_skill_anims.push_back(std::move(anim));
    }

    void CombatFx::spawn_arrow(SkillEt effect,
                               float ax,
                               float ay,
                               float az,
                               float bx,
                               float by,
                               float bz,
                               float extra,
                               std::uint32_t target_entity,
                               std::uint8_t flags,
                               float impact_scale,
                               std::uint32_t attacker_entity) {
        const auto token =
            m_arrow_feedback.launch(attacker_entity, target_entity, {ax, ay, az}, impact_scale);
        m_arrows.emplace_back(
            effect, ax, ay, az, bx, by, bz, extra, target_entity, flags, true, impact_scale, token);
    }

    void
    CombatFx::note_arrow_result(std::uint32_t attacker, std::uint32_t target, std::uint8_t flags) {
        m_arrow_feedback.confirm(attacker, target, flags);
    }

    bool CombatFx::take_arrow_feedback(ArrowHitFeedback::Impact& out) {
        return m_arrow_feedback.take(out);
    }

    void CombatFx::spawn_arrow(float ax, float ay, float az, float bx, float by, float bz) {
        spawn_arrow(SkillEt::None, ax, ay, az, bx, by, bz);
    }

    void
    CombatFx::spawn_vitality_drain(float ax, float ay, float az, float bx, float by, float bz) {
        m_arrows.emplace_back(SkillEt::VitalityDrain, ax, ay, az, bx, by, bz, 0.0f, 0, 1, false);
    }

    void
    CombatFx::spawn_devil_spirit_spit(float ax, float ay, float az, float bx, float by, float bz) {
        play_sfx("efs006", ax, ay, az);
        m_spits.emplace_back(ax, ay, az, bx, by, bz);
    }

    void CombatFx::set_homing_target(float x, float y, float z) {
        m_home_x = x;
        m_home_y = y;
        m_home_z = z;
        m_have_home = true;
    }

    void CombatFx::spawn_hit(engine::IEngine& engine,
                             float x,
                             float y,
                             float z,
                             std::uint8_t flags,
                             float scale,
                             bool arrow) {
        (void) engine;
        (void) scale;
        if ((flags & protocol::kAtkHit) == 0) {
            return;
        }
        if (arrow) {
            // Compatibility hit sound for arrows.
            play_sfx("efc007", x, y, z);
        }
        // Melee hit SFX (efc006) is keyed to (*EFFECT_MODEL effect_attack*) on the
        // swing clip so it plays after the whoosh (*SOUND efc003*).
    }

    void CombatFx::update(engine::IEngine& engine,
                          float dt,
                          float camera_yaw_rad,
                          float eye_x,
                          float eye_y,
                          float eye_z) {
        m_arrow_feedback.update(dt);
        m_cam_yaw = camera_yaw_rad;
        m_cam_x = eye_x;
        m_cam_y = eye_y;
        m_cam_z = eye_z;
        const float dt_ms = dt * 1000.0f;

        for (auto it = m_pending_clips.begin(); it != m_pending_clips.end();) {
            it->set_delay(it->delay() - dt);
            if (it->delay() <= 0.0f) {
                spawn_clip_model(engine,
                                 it->name(),
                                 it->x(),
                                 it->y(),
                                 it->z(),
                                 it->yaw(),
                                 0.0f,
                                 it->duration(),
                                 it->follow_entity(),
                                 it->persist(),
                                 it->loop(),
                                 it->effect_bone(),
                                 it->ox(),
                                 it->oy(),
                                 it->oz(),
                                 it->use_effect_bone_rotation());
                it = m_pending_clips.erase(it);
            } else {
                ++it;
            }
        }

        for (auto& bolt : m_bolts) {
            bolt.advance(dt);
        }
        m_bolts.erase(std::remove_if(m_bolts.begin(),
                                     m_bolts.end(),
                                     [](const Bolt& b) { return b.expired(); }),
                      m_bolts.end());

        for (auto& donut : m_heal_donuts) {
            donut.advance(dt);
        }
        m_heal_donuts.erase(std::remove_if(m_heal_donuts.begin(),
                                           m_heal_donuts.end(),
                                           [](const HealDonut& d) { return !d.alive(); }),
                            m_heal_donuts.end());

        for (auto& nova : m_ice_novas) {
            nova.advance(dt);
        }
        m_ice_novas.erase(std::remove_if(m_ice_novas.begin(),
                                         m_ice_novas.end(),
                                         [](const IceNova& n) { return !n.alive(); }),
                          m_ice_novas.end());

        for (auto& prep : m_stone_shields) {
            prep.advance(dt);
        }
        m_stone_shields.erase(std::remove_if(m_stone_shields.begin(),
                                             m_stone_shields.end(),
                                             [](const StoneShieldPrep& p) { return !p.alive(); }),
                              m_stone_shields.end());

        for (auto& fx : m_meditations) {
            fx.advance(dt);
        }
        m_meditations.erase(std::remove_if(m_meditations.begin(),
                                           m_meditations.end(),
                                           [](const MeditationFx& f) { return !f.alive(); }),
                            m_meditations.end());

        for (auto& p : m_particles) {
            if (!p.alive()) {
                continue;
            }
            p.advance(dt);
        }
        m_particles.erase(std::remove_if(m_particles.begin(),
                                         m_particles.end(),
                                         [](const Particle& p) { return !p.alive(); }),
                          m_particles.end());

        for (auto& a : m_arrows) {
            if (!a.alive()) {
                continue;
            }
            a.step(dt);
            if (a.take_impact()) {
                m_arrow_feedback.arrive(a.feedback_token(), {a.x(), a.y(), a.z()});
                if (a.extra() == 0.0f && a.visible()) {
                    play_sfx("efc007", a.x(), a.y(), a.z());
                    if (a.effect() == SkillEt::ExplosionArrow) {
                        play_sfx("efs009", a.x(), a.y(), a.z());
                    } else if (a.effect() != SkillEt::SilentShot) {
                        const char* hit =
                            (a.flags() & 2) != 0 ? "effect_attack02_c" : "effect_attack02";
                        spawn_clip_model(
                            engine, hit, a.x(), a.y(), a.z(), 0.0f, 0.0f, 0.0f, a.target_entity());
                    }
                }
            }
        }
        m_arrows.erase(std::remove_if(m_arrows.begin(),
                                      m_arrows.end(),
                                      [](const Arrow& a) { return !a.alive(); }),
                       m_arrows.end());

        for (auto& s : m_spits) {
            if (!s.alive()) {
                continue;
            }
            s.step(dt, m_have_home, m_home_x, m_home_y, m_home_z);
        }
        m_spits.erase(std::remove_if(
                          m_spits.begin(), m_spits.end(), [](const Spit& s) { return !s.alive(); }),
                      m_spits.end());

        for (auto& s : m_skill_anims) {
            if (!s.alive()) {
                continue;
            }
            const bool flying = s.flying();
            const auto bolt_kind = s.mage_bolt_kind();
            s.advance(dt_ms);
            if (flying && s.alive() && m_sprite_fx && bolt_kind != MageBoltKind::None) {
                const char* trail =
                    bolt_kind == MageBoltKind::Fire ? "firebolt_projectile" : "icebolt_projectile";
                m_sprite_fx->spawn_oneshot_particle(engine, trail, s.x(), s.y(), s.z());
            }
            if (flying && !s.alive() && bolt_kind != MageBoltKind::None) {
                ProjectileCrash crash;
                crash.set_kind(bolt_kind);
                crash.set_x(s.x());
                crash.set_y(s.y());
                crash.set_z(s.z());
                crash.set_target_entity(s.target_entity());
                crash.set_impact_scale(s.impact_scale());
                m_projectile_crashes.push_back(crash);
                if (m_sprite_fx) {
                    if (bolt_kind == MageBoltKind::Fire) {
                        m_sprite_fx->spawn_oneshot_particle(
                            engine, "firebolt_explosion", s.x(), s.y(), s.z());
                        const float scatter = 2.0f * s.impact_scale();
                        for (int i = 0; i < 3; ++i) {
                            m_sprite_fx->spawn_oneshot_particle(engine,
                                                                "firebolt_smallfire",
                                                                s.x(),
                                                                s.y(),
                                                                s.z(),
                                                                s.target_entity(),
                                                                1,
                                                                frand(-scatter, scatter),
                                                                frand(-scatter, scatter),
                                                                frand(-scatter, scatter));
                        }
                        play_sfx("efs009", s.x(), s.y(), s.z());
                    } else {
                        m_sprite_fx->spawn_oneshot_particle(
                            engine, "icebolt_explosion", s.x(), s.y(), s.z());
                        play_sfx("efs017", s.x(), s.y(), s.z());
                    }
                } else if (bolt_kind == MageBoltKind::Fire) {
                    play_sfx("efs009", s.x(), s.y(), s.z());
                } else {
                    play_sfx("efs017", s.x(), s.y(), s.z());
                }
            }
            if (!s.alive()) {
                if (m_audio) {
                    s.stop_sounds(*m_audio);
                }
                continue;
            }
            if (m_audio) {
                s.poll_sounds(*m_audio);
            }
            s.skin(engine);
        }
        for (auto& s : m_skill_anims) {
            if (!s.alive()) {
                s.destroy_meshes(engine);
            }
        }
        m_skill_anims.erase(std::remove_if(m_skill_anims.begin(),
                                           m_skill_anims.end(),
                                           [](const SkillAnim& s) { return !s.alive(); }),
                            m_skill_anims.end());
    }

    void CombatFx::draw_billboard(engine::IEngine& engine,
                                  engine::TextureHandle tex,
                                  float x,
                                  float y,
                                  float z,
                                  float size) const {
        if (!m_particle_quad || !tex || size <= 0.01f) {
            return;
        }
        // Camera-facing basis (view orientation).
        float fx = m_cam_x - x;
        float fy = m_cam_y - y;
        float fz = m_cam_z - z;
        float fl = std::sqrt(fx * fx + fy * fy + fz * fz);
        if (fl < 1e-4f) {
            return;
        }
        fx /= fl;
        fy /= fl;
        fz /= fl;
        // right = normalize(cross(up, forward)); up = (0,1,0)
        float rx = fz;
        float ry = 0;
        float rz = -fx;
        float rl = std::sqrt(rx * rx + rz * rz);
        if (rl < 1e-4f) {
            rx = 1;
            rz = 0;
            rl = 1;
        }
        rx /= rl;
        rz /= rl;
        // up = cross(forward, right)
        float ux = fy * rz - fz * ry;
        float uy = fz * rx - fx * rz;
        float uz = fx * ry - fy * rx;

        // Build model matrix columns: right*s, up*s, forward, translation.
        // Column-major Mat4.
        engine::Mat4 M = engine::Mat4::identity();
        const float s = size;
        M.m[0] = rx * s;
        M.m[1] = ry * s;
        M.m[2] = rz * s;
        M.m[4] = ux * s;
        M.m[5] = uy * s;
        M.m[6] = uz * s;
        M.m[8] = fx;
        M.m[9] = fy;
        M.m[10] = fz;
        M.m[12] = x;
        M.m[13] = y;
        M.m[14] = z;
        engine.draw_mesh_textured(m_particle_quad,
                                  tex,
                                  M,
                                  false,
                                  true,
                                  false,
                                  /*additive=*/true);
    }

    void CombatFx::draw_polyline(engine::IEngine& engine,
                                 engine::TextureHandle tex,
                                 const float* xs,
                                 const float* ys,
                                 const float* zs,
                                 const float* as,
                                 int n,
                                 float width,
                                 float r,
                                 float g,
                                 float b,
                                 bool additive) {
        if (!tex || n < 2) {
            return;
        }
        std::vector<engine::VertexP3T2> verts;
        std::vector<std::uint32_t> idx;
        verts.reserve(static_cast<std::size_t>(n) * 2);
        idx.reserve(static_cast<std::size_t>(n - 1) * 6);
        const float tile = std::max(1.0f, static_cast<float>(n));
        for (int i = 0; i < n; ++i) {
            const float x = xs[i];
            const float y = ys[i];
            const float z = zs[i];
            float tdx = 0.0f;
            float tdy = 0.0f;
            float tdz = 0.0f;
            if (i + 1 < n) {
                tdx = xs[i + 1] - x;
                tdy = ys[i + 1] - y;
                tdz = zs[i + 1] - z;
            } else {
                tdx = x - xs[i - 1];
                tdy = y - ys[i - 1];
                tdz = z - zs[i - 1];
            }
            float tl = std::sqrt(tdx * tdx + tdy * tdy + tdz * tdz);
            if (tl < 1e-4f) {
                tdx = 0.0f;
                tdy = 1.0f;
                tdz = 0.0f;
                tl = 1.0f;
            }
            tdx /= tl;
            tdy /= tl;
            tdz /= tl;
            float cdx = m_cam_x - x;
            float cdy = m_cam_y - y;
            float cdz = m_cam_z - z;
            float cl = std::sqrt(cdx * cdx + cdy * cdy + cdz * cdz);
            if (cl > 1e-4f) {
                cdx /= cl;
                cdy /= cl;
                cdz /= cl;
            }
            float sx = tdy * cdz - tdz * cdy;
            float sy = tdz * cdx - tdx * cdz;
            float sz = tdx * cdy - tdy * cdx;
            float sl = std::sqrt(sx * sx + sy * sy + sz * sz);
            if (sl < 1e-4f) {
                sx = -tdz;
                sy = 0.0f;
                sz = tdx;
                sl = std::sqrt(sx * sx + sz * sz);
            }
            const float half = width * std::max(0.05f, as[i]);
            if (sl > 1e-4f) {
                sx = sx / sl * half;
                sy = sy / sl * half;
                sz = sz / sl * half;
            }
            const float u = static_cast<float>(i) / tile;
            verts.push_back({x - sx, y - sy, z - sz, u, 0.0f});
            verts.push_back({x + sx, y + sy, z + sz, u, 1.0f});
            if (i + 1 < n) {
                const auto b = static_cast<std::uint32_t>(i * 2);
                idx.push_back(b);
                idx.push_back(b + 1);
                idx.push_back(b + 2);
                idx.push_back(b + 1);
                idx.push_back(b + 3);
                idx.push_back(b + 2);
            }
        }
        if (m_ribbon_mesh) {
            engine.destroy_mesh(m_ribbon_mesh);
        }
        m_ribbon_mesh = engine.create_mesh(verts.data(), verts.size(), idx.data(), idx.size());
        engine.draw_mesh_textured(m_ribbon_mesh,
                                  tex,
                                  engine::Mat4::identity(),
                                  !additive,
                                  true,
                                  false,
                                  additive,
                                  0.0f,
                                  0.0f,
                                  r,
                                  g,
                                  b,
                                  1.0f);
    }

    void CombatFx::draw(engine::IEngine& engine) {
        if (!m_ready) {
            return;
        }

        // Skill_R_4_0 prep mesh
        for (const auto& s : m_skill_anims) {
            s.draw(engine, m_cam_yaw);
        }

        if (m_heal_donut_mesh && m_heal_tex) {
            for (const auto& donut : m_heal_donuts) {
                if (!donut.alive()) {
                    continue;
                }
                float alpha = 0.0f;
                if (donut.age() * 2.0f < donut.life()) {
                    alpha = 2.0f * donut.age() / donut.life();
                } else {
                    alpha = 2.0f - 2.0f * donut.age() / donut.life();
                }
                if (alpha <= 0.01f) {
                    continue;
                }
                const auto T = engine::Mat4::translation(donut.x(), donut.y(), donut.z());
                engine.draw_mesh_textured(m_heal_donut_mesh,
                                          m_heal_tex,
                                          T,
                                          false,
                                          true,
                                          false,
                                          true,
                                          donut.age(),
                                          0.0f,
                                          alpha,
                                          alpha,
                                          alpha,
                                          1.0f);
            }
        }

        if (m_ice_nova_mesh && m_ice_nova_tex) {
            std::vector<engine::VertexP3T2> verts;
            for (const auto& nova : m_ice_novas) {
                if (!nova.alive() || nova.life() <= 0.0f) {
                    continue;
                }
                float radius1 = 0.0f;
                float radius2 = 0.0f;
                ice_nova_radii(nova.age() / nova.life(), radius1, radius2);
                fill_ice_nova_verts(verts, radius1, radius2);
                if (!engine.update_mesh(m_ice_nova_mesh, verts.data(), verts.size())) {
                    continue;
                }
                const auto T =
                    engine::Mat4::translation(nova.x(), nova.y() - kIceNovaOriginDrop, nova.z());
                engine.draw_mesh_textured(m_ice_nova_mesh,
                                          m_ice_nova_tex,
                                          T,
                                          false,
                                          true,
                                          false,
                                          true,
                                          0.0f,
                                          0.0f,
                                          120.0f / 255.0f,
                                          200.0f / 255.0f,
                                          1.0f,
                                          1.0f);
            }
        }

        if (m_flare_tex) {
            std::array<float, StoneShieldPrep::kPoints> xs{};
            std::array<float, StoneShieldPrep::kPoints> ys{};
            std::array<float, StoneShieldPrep::kPoints> zs{};
            std::array<float, StoneShieldPrep::kPoints> as{};
            for (auto& prep : m_stone_shields) {
                if (!prep.alive()) {
                    continue;
                }
                for (int ribbon = 0; ribbon < StoneShieldPrep::kRibbons; ++ribbon) {
                    prep.build_ribbon(ribbon, xs, ys, zs, as);
                    draw_polyline(engine,
                                  m_flare_tex,
                                  xs.data(),
                                  ys.data(),
                                  zs.data(),
                                  as.data(),
                                  StoneShieldPrep::kPoints,
                                  1.0f,
                                  50.0f / 255.0f,
                                  127.0f / 255.0f,
                                  1.0f,
                                  true);
                }
            }
        }

        if (m_meditation_tex) {
            std::array<float, MeditationFx::kPoints> xs{};
            std::array<float, MeditationFx::kPoints> ys{};
            std::array<float, MeditationFx::kPoints> zs{};
            std::array<float, MeditationFx::kPoints> as{};
            for (auto& fx : m_meditations) {
                if (!fx.alive()) {
                    continue;
                }
                for (int ribbon = 0; ribbon < MeditationFx::kRibbons; ++ribbon) {
                    fx.build_ribbon(ribbon, xs, ys, zs, as);
                    draw_polyline(engine,
                                  m_meditation_tex,
                                  xs.data(),
                                  ys.data(),
                                  zs.data(),
                                  as.data(),
                                  MeditationFx::kPoints,
                                  1.0f,
                                  127.0f / 255.0f,
                                  1.0f,
                                  100.0f / 255.0f,
                                  true);
                }
            }
        }

        // ET_LIGHTNING polyline — rebuild every frame with fresh jiggle + UV scroll
        // (the "animation").
        constexpr float kHalfW = 1.75f;
        constexpr float kTile = 4.0f;
        for (const auto& bolt : m_bolts) {
            const int n = bolt.segments();
            if (n < 2) {
                continue;
            }
            const float fade = std::max(0.0f, 1.0f - bolt.age() / bolt.life());
            const float scroll = -bolt.age() * 4.0f;  // DrawPolyLine(..., -runtime state*4)
            std::vector<engine::VertexP3T2> verts;
            std::vector<std::uint32_t> idx;
            verts.reserve(static_cast<std::size_t>(n) * 2);

            // Per-frame jiggle like compatibility behavior (LIGHTNING_JIGGLE_OFFSET 3.5).
            std::vector<float> jx(static_cast<std::size_t>(n), 0.0f);
            std::vector<float> jy(static_cast<std::size_t>(n), 0.0f);
            std::vector<float> jz(static_cast<std::size_t>(n), 0.0f);
            const float sdx = bolt.bx() - bolt.ax();
            const float sdy = bolt.by() - bolt.ay();
            const float sdz = bolt.bz() - bolt.az();
            float slen = std::sqrt(sdx * sdx + sdy * sdy + sdz * sdz);
            float ndx = sdx, ndy = sdy, ndz = sdz;
            if (slen > 1e-4f) {
                ndx /= slen;
                ndy /= slen;
                ndz /= slen;
            }
            for (int i = 1; i + 1 < n; ++i) {
                float vx = frand(-1, 1), vy = frand(-1, 1), vz = frand(-1, 1);
                // cross(v, dir)
                float cx = vy * ndz - vz * ndy;
                float cy = vz * ndx - vx * ndz;
                float cz = vx * ndy - vy * ndx;
                float cl = std::sqrt(cx * cx + cy * cy + cz * cz);
                if (cl > 1e-4f) {
                    cx = cx / cl * 3.5f;
                    cy = cy / cl * 3.5f;
                    cz = cz / cl * 3.5f;
                }
                jx[static_cast<std::size_t>(i)] = cx;
                jy[static_cast<std::size_t>(i)] = cy;
                jz[static_cast<std::size_t>(i)] = cz;
            }

            for (int i = 0; i < n; ++i) {
                const float t = static_cast<float>(i) / static_cast<float>(n - 1);
                const float x = bolt.ax() + sdx * t + jx[static_cast<std::size_t>(i)];
                const float y = bolt.ay() + sdy * t + jy[static_cast<std::size_t>(i)];
                const float z = bolt.az() + sdz * t + jz[static_cast<std::size_t>(i)];

                float tdx = ndx, tdy = ndy, tdz = ndz;
                if (i + 1 < n) {
                    const float t1 = static_cast<float>(i + 1) / static_cast<float>(n - 1);
                    tdx = (bolt.ax() + sdx * t1 + jx[static_cast<std::size_t>(i + 1)]) - x;
                    tdy = (bolt.ay() + sdy * t1 + jy[static_cast<std::size_t>(i + 1)]) - y;
                    tdz = (bolt.az() + sdz * t1 + jz[static_cast<std::size_t>(i + 1)]) - z;
                }
                float tl = std::sqrt(tdx * tdx + tdy * tdy + tdz * tdz);
                if (tl > 1e-4f) {
                    tdx /= tl;
                    tdy /= tl;
                    tdz /= tl;
                }

                float cdx = m_cam_x - x;
                float cdy = m_cam_y - y;
                float cdz = m_cam_z - z;
                float cl = std::sqrt(cdx * cdx + cdy * cdy + cdz * cdz);
                if (cl > 1e-4f) {
                    cdx /= cl;
                    cdy /= cl;
                    cdz /= cl;
                }

                float sx = tdy * cdz - tdz * cdy;
                float sy = tdz * cdx - tdx * cdz;
                float sz = tdx * cdy - tdy * cdx;
                float sl = std::sqrt(sx * sx + sy * sy + sz * sz);
                if (sl < 1e-4f) {
                    sx = -tdz;
                    sy = 0;
                    sz = tdx;
                    sl = std::sqrt(sx * sx + sz * sz);
                }
                if (sl > 1e-4f) {
                    sx = sx / sl * kHalfW;
                    sy = sy / sl * kHalfW;
                    sz = sz / sl * kHalfW;
                }

                const float u = static_cast<float>(i) / kTile + scroll;
                // Soften with fade by shrinking width slightly at end of life.
                const float fw = fade;
                verts.push_back({x - sx * fw, y - sy * fw, z - sz * fw, u, 0.0f});
                verts.push_back({x + sx * fw, y + sy * fw, z + sz * fw, u, 1.0f});
                if (i + 1 < n) {
                    const auto b = static_cast<std::uint32_t>(i * 2);
                    idx.push_back(b);
                    idx.push_back(b + 1);
                    idx.push_back(b + 2);
                    idx.push_back(b + 1);
                    idx.push_back(b + 3);
                    idx.push_back(b + 2);
                }
            }
            if (m_ribbon_mesh) {
                engine.destroy_mesh(m_ribbon_mesh);
            }
            m_ribbon_mesh = engine.create_mesh(verts.data(), verts.size(), idx.data(), idx.size());
            engine::TextureHandle lightning_tex = m_lightning_tex;
            if (bolt.kind() == LightningKind::Red && m_lightning_red_tex) {
                lightning_tex = m_lightning_red_tex;
            } else if (bolt.kind() == LightningKind::Spirit && m_lightning_spirit_tex) {
                lightning_tex = m_lightning_spirit_tex;
            }
            engine.draw_mesh_textured(
                m_ribbon_mesh, lightning_tex, engine::Mat4::identity(), false, true, false, true);
        }

        // Particles (flare + impact)
        for (const auto& p : m_particles) {
            if (!p.alive()) {
                continue;
            }
            const float t = std::clamp(p.age() / std::max(0.001f, p.life()), 0.0f, 1.0f);
            const float size = p.size0() + (p.size1() - p.size0()) * t;
            // Additive fade: shrink contribution by (1-t); engine has no vertex color
            // yet.
            const float vis = (1.0f - t) * p.a0();
            if (vis < 0.05f && size < 0.5f) {
                continue;
            }
            draw_billboard(engine, p.tex(), p.x(), p.y(), p.z(), size * (0.5f + 0.5f * vis));
        }

        if (m_ghost_grey_tex) {
            std::array<float, Arrow::kTrailPoints> xs{};
            std::array<float, Arrow::kTrailPoints> ys{};
            std::array<float, Arrow::kTrailPoints> zs{};
            std::array<float, Arrow::kTrailPoints> as{};
            for (const auto& arrow : m_arrows) {
                if (!arrow.visible()) {
                    continue;
                }
                for (int i = 0; i < Arrow::kTrailPoints; ++i) {
                    const std::size_t index = static_cast<std::size_t>(i);
                    arrow.trail_point(i, xs[index], ys[index], zs[index], as[index]);
                }
                if (arrow.crashed()) {
                    const float tdx = xs.back() - arrow.x();
                    const float tdy = ys.back() - arrow.y();
                    const float tdz = zs.back() - arrow.z();
                    if (tdx * tdx + tdy * tdy + tdz * tdz < 1.0f) {
                        continue;
                    }
                }
                draw_polyline(engine,
                              m_ghost_grey_tex,
                              xs.data(),
                              ys.data(),
                              zs.data(),
                              as.data(),
                              Arrow::kTrailPoints,
                              1.0f,
                              arrow.r(),
                              arrow.g(),
                              arrow.b(),
                              arrow.additive());
            }
        }

        if (m_arrow_mesh) {
            for (const auto& a : m_arrows) {
                if (!a.visible()) {
                    continue;
                }
                const engine::Mat4 M =
                    a.stuck() ? a.world_matrix()
                              : arrow_model_matrix(a.x(), a.y(), a.z(), a.yaw(), a.pitch());
                if (m_arrow_tex) {
                    engine.draw_mesh_textured(
                        m_arrow_mesh, m_arrow_tex, M, false, true, false, true);
                } else {
                    engine.draw_mesh_lit(m_arrow_mesh, M, 0.95f, 0.8f, 0.45f);
                }
            }
        }

        if (m_ghost_blue_tex) {
            std::array<float, Spit::kTrailPoints> xs{};
            std::array<float, Spit::kTrailPoints> ys{};
            std::array<float, Spit::kTrailPoints> zs{};
            std::array<float, Spit::kTrailPoints> as{};
            for (const auto& spit : m_spits) {
                if (!spit.alive()) {
                    continue;
                }
                for (int trail = 0; trail < 2; ++trail) {
                    for (int i = 0; i < Spit::kTrailPoints; ++i) {
                        const std::size_t idx = static_cast<std::size_t>(i);
                        spit.trail_point(trail, i, xs[idx], ys[idx], zs[idx], as[idx]);
                    }
                    const float width = (trail == 0) ? 4.0f : 1.0f;
                    draw_polyline(engine,
                                  m_ghost_blue_tex,
                                  xs.data(),
                                  ys.data(),
                                  zs.data(),
                                  as.data(),
                                  Spit::kTrailPoints,
                                  width);
                }
            }
        }
    }

}  // namespace openkal::client
