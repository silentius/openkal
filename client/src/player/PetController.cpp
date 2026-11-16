/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/PetController.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace openkal::client {
    namespace {
        constexpr float kPi = 3.14159265358979323846f;
        constexpr float kSpawnDistance = 46.0f;
        constexpr float kFollowStartDistance = 52.0f;
        constexpr float kFollowStopDistance = 36.0f;
        constexpr float kFollowRunDistance = 92.0f;
        constexpr float kCatchUpDistance = 140.0f;
        constexpr float kSnapDistance = 320.0f;
        constexpr float kPickupSearchDistance = 64.0f;
        constexpr float kPickupArrivalDistance = 5.0f;
        constexpr float kPickupRetrySeconds = 5.0f;
        constexpr float kMinimumFollowSpeed = 68.0f;
        constexpr float kMinimumCatchUpSpeed = 96.0f;
        constexpr float kMinimumPickupSpeed = 78.0f;
        constexpr float kTurnSpeed = 9.0f;
        constexpr float kMoveFacingTolerance = 0.35f;
        constexpr float kStopTolerance = 0.25f;

        [[nodiscard]] float distance_squared(float ax, float az, float bx, float bz) {
            const float dx = bx - ax;
            const float dz = bz - az;
            return dx * dx + dz * dz;
        }

        [[nodiscard]] float wrap_angle(float value) {
            while (value > kPi) {
                value -= 2.0f * kPi;
            }
            while (value < -kPi) {
                value += 2.0f * kPi;
            }
            return value;
        }
    }  // namespace

    void PetController::reset(const PetOwnerState& owner) {
        m_x = owner.x + std::sin(owner.yaw) * kSpawnDistance;
        m_z = owner.z + std::cos(owner.yaw) * kSpawnDistance;
        m_yaw = owner.yaw;
        m_has_position = true;
        m_following = false;
        m_pickup_target_id = 0;
        m_pickup_retry_seconds.clear();
    }

    void PetController::clear() {
        m_has_position = false;
        m_following = false;
        m_pickup_target_id = 0;
        m_pickup_retry_seconds.clear();
    }

    void PetController::update_retry_cooldowns(float dt) {
        for (auto it = m_pickup_retry_seconds.begin(); it != m_pickup_retry_seconds.end();) {
            it->second -= dt;
            if (it->second <= 0.0f) {
                it = m_pickup_retry_seconds.erase(it);
            } else {
                ++it;
            }
        }
    }

    const PetPickupTarget*
    PetController::find_pickup_target(const PetOwnerState& owner,
                                      const std::vector<PetPickupTarget>& items) const {
        const PetPickupTarget* nearest = nullptr;
        float nearest_distance = std::numeric_limits<float>::max();
        const float search_distance_squared = kPickupSearchDistance * kPickupSearchDistance;
        for (const PetPickupTarget& item : items) {
            if (item.id == 0 ||
                m_pickup_retry_seconds.find(item.id) != m_pickup_retry_seconds.end()) {
                continue;
            }
            const float distance = distance_squared(owner.x, owner.z, item.x, item.z);
            if (distance > search_distance_squared) {
                continue;
            }
            if (nearest == nullptr || distance < nearest_distance ||
                (distance == nearest_distance && item.id < nearest->id)) {
                nearest = &item;
                nearest_distance = distance;
            }
        }
        return nearest;
    }

    bool PetController::move_toward(
        float dt, float target_x, float target_z, float stop_distance, float speed) {
        const float dx = target_x - m_x;
        const float dz = target_z - m_z;
        const float distance = std::sqrt(dx * dx + dz * dz);
        if (distance <= stop_distance + kStopTolerance || distance <= 1e-4f) {
            return false;
        }

        const float desired_yaw = std::atan2(-dx, -dz);
        const float yaw_delta = wrap_angle(desired_yaw - m_yaw);
        const float turn_step = kTurnSpeed * dt;
        m_yaw = wrap_angle(m_yaw + std::clamp(yaw_delta, -turn_step, turn_step));
        if (std::fabs(wrap_angle(desired_yaw - m_yaw)) > kMoveFacingTolerance) {
            return false;
        }

        const float step = std::min(speed * dt, distance - stop_distance);
        m_x += dx / distance * step;
        m_z += dz / distance * step;
        return step > 0.0f;
    }

    PetUpdateResult PetController::update(float dt,
                                          const PetOwnerState& owner,
                                          bool auto_pickup,
                                          const std::vector<PetPickupTarget>& items) {
        PetUpdateResult result;
        dt = std::clamp(dt, 0.0f, 0.1f);
        update_retry_cooldowns(dt);

        if (!m_has_position) {
            reset(owner);
        }

        if (distance_squared(m_x, m_z, owner.x, owner.z) > kSnapDistance * kSnapDistance) {
            reset(owner);
            return result;
        }

        const PetPickupTarget* pickup_target = nullptr;
        if (auto_pickup) {
            for (const PetPickupTarget& item : items) {
                if (item.id == m_pickup_target_id &&
                    distance_squared(owner.x, owner.z, item.x, item.z) <=
                        kPickupSearchDistance * kPickupSearchDistance) {
                    pickup_target = &item;
                    break;
                }
            }
            if (pickup_target == nullptr) {
                m_pickup_target_id = 0;
                pickup_target = find_pickup_target(owner, items);
                if (pickup_target != nullptr) {
                    m_pickup_target_id = pickup_target->id;
                }
            }
        } else {
            m_pickup_target_id = 0;
        }

        if (pickup_target != nullptr) {
            const float pickup_speed = std::max(kMinimumPickupSpeed, owner.run_speed * 1.3f);
            result.moving = move_toward(
                dt, pickup_target->x, pickup_target->z, kPickupArrivalDistance, pickup_speed);
            result.running = result.moving;
            constexpr float kPickupReach = kPickupArrivalDistance + kStopTolerance;
            if (distance_squared(m_x, m_z, pickup_target->x, pickup_target->z) <=
                kPickupReach * kPickupReach) {
                result.pickup_item_id = pickup_target->id;
                m_pickup_retry_seconds[pickup_target->id] = kPickupRetrySeconds;
                m_pickup_target_id = 0;
                m_following = true;
            }
            return result;
        }

        const float owner_distance_squared = distance_squared(m_x, m_z, owner.x, owner.z);
        if (!m_following && owner_distance_squared >= kFollowStartDistance * kFollowStartDistance) {
            m_following = true;
        } else if (m_following &&
                   owner_distance_squared <= (kFollowStopDistance + kStopTolerance) *
                                                 (kFollowStopDistance + kStopTolerance)) {
            m_following = false;
        }
        if (!m_following) {
            return result;
        }

        const float owner_distance = std::sqrt(owner_distance_squared);
        float follow_speed = std::max(kMinimumFollowSpeed, owner.run_speed * 1.15f);
        if (owner_distance >= kCatchUpDistance) {
            follow_speed = std::max(kMinimumCatchUpSpeed, owner.run_speed * 1.6f);
        }
        result.moving = move_toward(dt, owner.x, owner.z, kFollowStopDistance, follow_speed);
        result.running = result.moving && owner_distance >= kFollowRunDistance;
        return result;
    }

}  // namespace openkal::client
