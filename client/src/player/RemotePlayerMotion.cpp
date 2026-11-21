/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/RemotePlayerMotion.h"

#include <algorithm>
#include <cmath>

namespace openkal::client {

    void RemotePlayerMotion::reset(const engine::Vec3& position) {
        m_position = position;
        m_start = position;
        m_target = position;
        m_elapsed = kInterpolationSeconds;
        m_yaw = 0.0f;
        m_target_yaw = 0.0f;
        m_network_moving = false;
    }

    void RemotePlayerMotion::set_target(const engine::Vec3& position, bool moving) {
        const bool was_moving = this->moving();
        m_network_moving = moving;
        const float dx = position.x - m_target.x;
        const float dz = position.z - m_target.z;
        if (dx == 0.0f && dz == 0.0f && position.y == m_target.y) {
            return;
        }
        if (dx != 0.0f || dz != 0.0f) {
            m_target_yaw = std::atan2(-dx, -dz);
            if (!was_moving) {
                m_yaw = m_target_yaw;
            }
        }
        m_start = m_position;
        m_target = position;
        m_elapsed = 0.0f;
        const float distance_x = m_target.x - m_position.x;
        const float distance_z = m_target.z - m_position.z;
        if (distance_x * distance_x + distance_z * distance_z > kSnapDistance * kSnapDistance) {
            m_position = m_target;
            m_start = m_target;
            m_elapsed = kInterpolationSeconds;
            m_yaw = m_target_yaw;
        }
    }

    void RemotePlayerMotion::finish_stop() {
        if (!m_network_moving) {
            m_position = m_target;
            m_start = m_target;
            m_elapsed = kInterpolationSeconds;
        }
    }

    void RemotePlayerMotion::face_target(float x, float z) {
        const float dx = x - m_position.x;
        const float dz = z - m_position.z;
        if (dx * dx + dz * dz > 0.0001f) {
            m_yaw = std::atan2(-dx, -dz);
            m_target_yaw = m_yaw;
        }
    }

    void RemotePlayerMotion::update(float dt) {
        if (!std::isfinite(dt) || dt <= 0.0f) {
            return;
        }
        m_elapsed = std::min(kInterpolationSeconds, m_elapsed + dt);
        const float blend = m_elapsed / kInterpolationSeconds;
        m_position.x = m_start.x + (m_target.x - m_start.x) * blend;
        m_position.y = m_start.y + (m_target.y - m_start.y) * blend;
        m_position.z = m_start.z + (m_target.z - m_start.z) * blend;

        constexpr float kTwoPi = 6.28318530717958647692f;
        const float turn = std::remainder(m_target_yaw - m_yaw, kTwoPi);
        const float turn_blend = 1.0f - std::exp(-dt / kInterpolationSeconds);
        m_yaw = std::remainder(m_yaw + turn * turn_blend, kTwoPi);
    }

    const engine::Vec3& RemotePlayerMotion::position() const {
        return m_position;
    }

    float RemotePlayerMotion::yaw() const {
        return m_yaw;
    }

    bool RemotePlayerMotion::moving() const {
        const bool horizontal_segment = m_start.x != m_target.x || m_start.z != m_target.z;
        return m_network_moving || (m_elapsed < kInterpolationSeconds && horizontal_segment);
    }

}  // namespace openkal::client
