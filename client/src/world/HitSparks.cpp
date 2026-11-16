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

#include <algorithm>
#include <array>
#include <cmath>

namespace openkal::client {
    namespace {
        constexpr std::size_t kMaxSparks = 256;
    }

    bool HitSparks::init(engine::IEngine& engine) {
        destroy(engine);
        // OpenKal sprite: a small white diamond, generated without game data.
        constexpr int size = 16;
        std::array<std::uint8_t, size * size * 4> pixels{};
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                const float distance = (std::fabs(x - 7.5f) + std::fabs(y - 7.5f)) / 7.5f;
                const std::size_t offset = static_cast<std::size_t>((y * size + x) * 4);
                pixels[offset] = pixels[offset + 1] = pixels[offset + 2] = 255;
                pixels[offset + 3] = static_cast<std::uint8_t>(
                    255.0f * std::clamp((1.0f - distance) * 3.0f, 0.0f, 1.0f));
            }
        }
        m_texture = engine.create_texture_rgba8(pixels.data(), size, size, true);
        m_sparks.reserve(kMaxSparks);
        m_vertices.reserve(kMaxSparks * 6);
        return static_cast<bool>(m_texture);
    }

    void HitSparks::destroy(engine::IEngine& engine) {
        if (m_texture) {
            engine.destroy_texture(m_texture);
        }
        m_texture = {};
        m_sparks.clear();
        m_vertices.clear();
    }

    void HitSparks::spawn(const engine::Vec3& impact,
                          const engine::Vec3& origin,
                          float scale,
                          bool critical) {
        const float dx = impact.x - origin.x;
        const float dz = impact.z - origin.z;
        const float length = std::hypot(dx, dz);
        if (!m_texture || !std::isfinite(length) || length < 0.01f || !std::isfinite(scale)) {
            return;
        }
        scale = std::clamp(scale, 0.75f, 1.5f);
        const float nx = dx / length;
        const float nz = dz / length;
        const int count = critical ? 8 : 6;
        for (int i = 0; i < count && m_sparks.size() < kMaxSparks; ++i) {
            const float spread = -1.0f + 2.0f * static_cast<float>(i) / (count - 1);
            const float speed = (140.0f + 14.0f * (i % 4)) * scale;
            const float c = std::cos(spread);
            const float s = std::sin(spread);
            const engine::Vec3 velocity{(nx * c - nz * s) * speed,
                                        (63.0f + 14.0f * (i % 3)) * scale,
                                        (nx * s + nz * c) * speed};
            // Keep the initial speed, but limit each spark to a short body-relative impulse.
            const float travel = (12.0f + 2.0f * (i % 3)) * scale;
            const float life = travel / std::hypot(speed, velocity.y);
            m_sparks.push_back({impact, velocity, 0.0f, life, (1.26f + 0.18f * (i % 3)) * scale});
        }
    }

    void HitSparks::update(float dt) {
        if (!std::isfinite(dt) || dt <= 0.0f) {
            return;
        }
        for (auto& spark : m_sparks) {
            spark.age += dt;
            spark.position.x += spark.velocity.x * dt;
            spark.position.y += spark.velocity.y * dt;
            spark.position.z += spark.velocity.z * dt;
        }
        m_sparks.erase(std::remove_if(m_sparks.begin(),
                                      m_sparks.end(),
                                      [](const Spark& spark) { return spark.age >= spark.life; }),
                       m_sparks.end());
    }

    void HitSparks::draw(engine::IEngine& engine, const engine::Vec3& camera) {
        m_vertices.clear();
        for (const auto& spark : m_sparks) {
            const float dx = camera.x - spark.position.x;
            const float dy = camera.y - spark.position.y;
            const float dz = camera.z - spark.position.z;
            const float length = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (length < 0.001f) {
                continue;
            }
            const float horizontal = std::hypot(dx, dz);
            const engine::Vec3 right = horizontal > 0.001f
                                           ? engine::Vec3{dz / horizontal, 0, -dx / horizontal}
                                           : engine::Vec3{1, 0, 0};
            const engine::Vec3 up{dy / length * right.z,
                                  (dz * right.x - dx * right.z) / length,
                                  -dy / length * right.x};
            const float remaining = 1.0f - spark.age / spark.life;
            const float width = spark.size * (0.7f + 0.3f * remaining);
            const float along_right = spark.velocity.x * right.x + spark.velocity.z * right.z;
            const float along_up =
                spark.velocity.x * up.x + spark.velocity.y * up.y + spark.velocity.z * up.z;
            const float projected_speed = std::hypot(along_right, along_up);
            const float screen_x = projected_speed > 0.001f ? along_right / projected_speed : 0;
            const float screen_y = projected_speed > 0.001f ? along_up / projected_speed : 1;
            // Stretch in the projected travel direction, keeping a readable width at game zoom.
            const float streak_length = width + std::min(projected_speed * 0.06f, width * 2.0f);
            const engine::Vec3 along{right.x * screen_x + up.x * screen_y,
                                     up.y * screen_y,
                                     right.z * screen_x + up.z * screen_y};
            const engine::Vec3 across{right.x * screen_y - up.x * screen_x,
                                      -up.y * screen_x,
                                      right.z * screen_y - up.z * screen_x};
            // Keep the compact impulse bright until the final quarter of its life.
            const float alpha = 0.75f * std::min(1.0f, remaining * 4.0f);
            constexpr int corners[] = {0, 1, 2, 0, 2, 3};
            constexpr float us[] = {0, 1, 1, 0};
            constexpr float vs[] = {0, 0, 1, 1};
            for (int corner : corners) {
                const float x = (us[corner] - 0.5f) * width;
                const float y = (vs[corner] - 0.5f) * streak_length;
                m_vertices.emplace_back(spark.position.x + across.x * x + along.x * y,
                                        spark.position.y + across.y * x + along.y * y,
                                        spark.position.z + across.z * x + along.z * y,
                                        us[corner],
                                        vs[corner],
                                        1,
                                        1,
                                        1,
                                        alpha);
            }
        }
        if (!m_vertices.empty()) {
            engine.draw_sprite_batch(m_vertices, m_texture, true, true);
        }
    }
}  // namespace openkal::client
